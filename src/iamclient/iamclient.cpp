/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "iamclient.hpp"
#include "logger/logmodule.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error IAMClient::Init(const Config& cfg, aos::cryptoutils::CertLoaderItf& certLoader,
    aos::crypto::x509::ProviderItf&                                 cryptoProvider,
    aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>& channel, bool provisioningMode,
    MTLSCredentialsFunc mtlsCredentialsFunc)
{
    LOG_INF() << "Initializing IAM client";

    mPublicServiceHandler.emplace();
    mPublicNodeClient.emplace();

    if (auto err = mPublicServiceHandler->Init(
            cfg, certLoader, cryptoProvider, provisioningMode, std::move(mtlsCredentialsFunc));
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return mPublicNodeClient->Init(cfg.mIAMConfig, *this, channel, provisioningMode);
}

aos::RetWithError<std::shared_ptr<grpc::ChannelCredentials>> IAMClient::GetMTLSConfig(const std::string& certStorage)
{
    LOG_DBG() << "Getting MTLS config: certStorage=" << certStorage.c_str();

    return mPublicServiceHandler->GetMTLSConfig(certStorage);
}

std::shared_ptr<grpc::ChannelCredentials> IAMClient::GetTLSCredentials()
{
    LOG_DBG() << "Getting TLS config";

    return mPublicServiceHandler->GetTLSCredentials();
}

aos::Error IAMClient::GetCertificate(const std::string& certType, aos::iam::certhandler::CertInfo& certInfo)
{
    LOG_DBG() << "Getting certificate: certType=" << certType.c_str();

    return mPublicServiceHandler->GetCertificate(certType, certInfo);
}
