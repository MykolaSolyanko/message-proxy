
/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <sstream>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <utils/cryptohelper.hpp>
#include <utils/pkcs11helper.hpp>

#include "logger/logmodule.hpp"
#include "securechannel.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

SecureChannel::SecureChannel(const Config& cfg, CommChannelItf& channel, CertProviderItf& certProvider,
    aos::cryptoutils::CertLoaderItf& certLoader, aos::crypto::x509::ProviderItf& cryptoProvider, int port)
    : mChannel(&channel)
    , mCertProvider(&certProvider)
    , mCertLoader(&certLoader)
    , mCryptoProvider(&cryptoProvider)
    , mCfg(&cfg)
    , mPort(port)
{
    InitOpenssl();
    const SSL_METHOD* method = TLS_server_method();
    mCtx                     = CreateSSLContext(method);

    ENGINE* eng = ENGINE_by_id("pkcs11");
    if (!eng) {
        throw std::runtime_error("Failed to load PKCS11 engine");
    }

    if (!ENGINE_init(eng)) {
        throw std::runtime_error("Failed to initialize PKCS11 engine");
    }

    ConfigureSSLContext(mCtx, eng);
}

SecureChannel::~SecureChannel()
{
    SSL_free(mSsl);
    SSL_CTX_free(mCtx);
    CleanupOpenssl();
}

aos::Error SecureChannel::Connect()
{
    if (auto err = mChannel->Connect(); !err.IsNone()) {
        return err;
    }

    if (mSsl != nullptr) {
        SSL_free(mSsl);
    }

    mSsl               = SSL_new(mCtx);
    BIO_METHOD* method = CreateCustomBIOMethod();
    BIO*        rbio   = BIO_new(method);
    BIO*        wbio   = BIO_new(method);
    BIO_set_data(rbio, this);
    BIO_set_data(wbio, this);
    SSL_set_bio(mSsl, rbio, wbio);

    if (SSL_accept(mSsl) <= 0) {
        LOG_ERR() << "Failed to accept SSL connection";

        return aos::Error(aos::ErrorEnum::eRuntime, GetOpensslErrorString().c_str());
    }

    LOG_DBG() << "SSL connection accepted";

    return aos::ErrorEnum::eNone;
}

aos::Error SecureChannel::Read(std::vector<uint8_t>& message)
{
    if (message.empty()) {
        return aos::Error(aos::ErrorEnum::eRuntime, "message buffer is empty");
    }

    int bytes_read = SSL_read(mSsl, message.data(), message.size());
    if (bytes_read <= 0) {
        return aos::Error(aos::ErrorEnum::eRuntime, GetOpensslErrorString().c_str());
    }

    return aos::ErrorEnum::eNone;
}

aos::Error SecureChannel::Write(std::vector<uint8_t> message)
{
    if (int bytes_written = SSL_write(mSsl, message.data(), message.size()); bytes_written <= 0) {
        return aos::Error(aos::ErrorEnum::eRuntime, GetOpensslErrorString().c_str());
    }

    return aos::ErrorEnum::eNone;
}

aos::Error SecureChannel::Close()
{
    LOG_DBG() << "Close SecureChannel";

    auto err = mChannel->Close();

    if (mSsl != nullptr) {
        SSL_shutdown(mSsl);
    }

    return err;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void SecureChannel::InitOpenssl()
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void SecureChannel::CleanupOpenssl()
{
    EVP_cleanup();
}

std::string SecureChannel::GetOpensslErrorString()
{
    std::ostringstream oss;
    unsigned long      errCode;

    while ((errCode = ERR_get_error()) != 0) {
        char buf[256];

        ERR_error_string_n(errCode, buf, sizeof(buf));
        oss << buf << std::endl;
    }

    return oss.str();
}

SSL_CTX* SecureChannel::CreateSSLContext(const SSL_METHOD* method)
{
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        throw std::runtime_error(GetOpensslErrorString());
    }

    return ctx;
}

aos::Error SecureChannel::ConfigureSSLContext(SSL_CTX* ctx, ENGINE* eng)
{
    LOG_DBG() << "Configuring SSL context";

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);

    aos::iam::certhandler::CertInfo certInfo;

    auto err = mCertProvider->GetCertificate(mCfg->mVChan.mCertStorage, certInfo);
    if (!err.IsNone()) {
        return err;
    }

    auto [certificate, errLoad]
        = aos::common::utils::LoadPEMCertificates(certInfo.mCertURL, *mCertLoader, *mCryptoProvider);
    if (!errLoad.IsNone()) {
        return errLoad;
    }

    auto [keyURI, errURI] = aos::common::utils::CreatePKCS11URL(certInfo.mKeyURL);
    if (!errURI.IsNone()) {
        return errURI;
    }

    EVP_PKEY* pkey = ENGINE_load_private_key(eng, keyURI.c_str(), nullptr, nullptr);
    if (!pkey) {
        return aos::Error(aos::ErrorEnum::eRuntime, GetOpensslErrorString().c_str());
    }

    if (SSL_CTX_use_PrivateKey(ctx, pkey) <= 0) {
        return aos::Error(aos::ErrorEnum::eRuntime, GetOpensslErrorString().c_str());
    }

    // Split the certificate chain into individual certificates
    BIO* bio = BIO_new_mem_buf(certificate.c_str(), -1);
    if (!bio) {
        return aos::Error(aos::ErrorEnum::eRuntime, "failed to create BIO");
    }
    std::unique_ptr<BIO, decltype(&BIO_free)> bioPtr(bio, BIO_free);

    X509* cert = PEM_read_bio_X509(bio, nullptr, 0, nullptr);
    if (!cert) {
        return aos::Error(aos::ErrorEnum::eRuntime, GetOpensslErrorString().c_str());
    }
    std::unique_ptr<X509, decltype(&X509_free)> certPtr(cert, X509_free);

    if (SSL_CTX_use_certificate(ctx, cert) <= 0) {
        return aos::Error(aos::ErrorEnum::eRuntime, GetOpensslErrorString().c_str());
    }

    // Load the intermediate certificates
    STACK_OF(X509)* chain  = sk_X509_new_null();
    X509* intermediateCert = nullptr;
    while ((intermediateCert = PEM_read_bio_X509(bio, nullptr, 0, nullptr)) != nullptr) {
        sk_X509_push(chain, intermediateCert);
    }

    if (SSL_CTX_set1_chain(ctx, chain) <= 0) {
        sk_X509_pop_free(chain, X509_free);
        return aos::Error(aos::ErrorEnum::eRuntime, GetOpensslErrorString().c_str());
    }

    sk_X509_pop_free(chain, X509_free);

    if (SSL_CTX_load_verify_locations(ctx, mCfg->mCACert.c_str(), nullptr) <= 0) {
        return aos::Error(aos::ErrorEnum::eRuntime, GetOpensslErrorString().c_str());
    }

    LOG_DBG() << "SSL context configured";

    return aos::ErrorEnum::eNone;
}

int SecureChannel::CustomBIOWrite(BIO* bio, const char* buf, int len)
{
    LOG_DBG() << "SecureChannel::CustomBIOWrite: request " << len << " bytes";

    SecureChannel*       channel = static_cast<SecureChannel*>(BIO_get_data(bio));
    std::vector<uint8_t> data(buf, buf + len);
    auto                 err = channel->mChannel->Write(std::move(data));

    return err.IsNone() ? len : -1;
}

int SecureChannel::CustomBIORead(BIO* bio, char* buf, int len)
{
    LOG_DBG() << "SecureChannel::CustomBIORead: request " << len << " bytes";

    SecureChannel*       channel = static_cast<SecureChannel*>(BIO_get_data(bio));
    std::vector<uint8_t> data(len);
    auto                 err = channel->mChannel->Read(data);
    if (!err.IsNone()) {
        return -1;
    }

    std::memcpy(buf, data.data(), data.size());

    return data.size();
}

long SecureChannel::CustomBIOCtrl(
    [[maybe_unused]] BIO* bio, int cmd, [[maybe_unused]] long num, [[maybe_unused]] void* ptr)
{
    switch (cmd) {
    case BIO_CTRL_FLUSH:
        return 1;
    default:
        return 0;
    }
}

BIO_METHOD* SecureChannel::CreateCustomBIOMethod()
{
    BIO_METHOD* method = BIO_meth_new(BIO_TYPE_SOURCE_SINK, "Custom BIO");

    BIO_meth_set_write(method, CustomBIOWrite);
    BIO_meth_set_read(method, CustomBIORead);
    BIO_meth_set_ctrl(method, CustomBIOCtrl);

    return method;
}