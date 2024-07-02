/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <utils/grpchelper.hpp>

#include "cmclient.hpp"
#include "logger/logmodule.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error CMClient::Init(const Config& config, CertProviderItf& certProvider,
    aos::cryptoutils::CertLoaderItf& certLoader, aos::crypto::x509::ProviderItf& cryptoProvider,
    aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>& channel, bool insecureConnection)
{
    LOG_INF() << "Initializing CM client";

    mCertProvider   = &certProvider;
    mCertLoader     = &certLoader;
    mCryptoProvider = &cryptoProvider;
    mMsgHandler     = &channel;

    auto [credentials, err] = CreateCredentials(config.mCertStorage, insecureConnection);
    if (!err.IsNone()) {
        return err;
    }

    mCredentials = credentials;

    mCMThread = std::thread(&CMClient::RunCM, this, config.mCMConfig.mCMServerURL);

    mHandlerOutgoingMsgsThread = std::thread(&CMClient::ProcessOutgoingSMMessages, this);

    return aos::ErrorEnum::eNone;
}

CMClient::~CMClient()
{
    LOG_INF() << "Shutting down CM client";

    mShutdown = true;

    {
        std::lock_guard<std::mutex> lock(mMutex);

        if (mCtx) {
            mCtx->TryCancel();
        }

        mCV.notify_all();
    }

    if (mCMThread.joinable()) {
        mCMThread.join();
    }

    if (mHandlerOutgoingMsgsThread.joinable()) {
        mHandlerOutgoingMsgsThread.join();
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

aos::RetWithError<std::shared_ptr<grpc::ChannelCredentials>> CMClient::CreateCredentials(
    const std::string& certStorage, bool insecureConnection)
{
    if (insecureConnection) {
        return {grpc::InsecureChannelCredentials(), aos::ErrorEnum::eNone};
    }

    aos::iam::certhandler::CertInfo certInfo;

    return mCertProvider->GetMTLSConfig(certStorage);
}

SMServiceStubPtr CMClient::CreateSMStub(const std::string& url)
{
    auto channel = grpc::CreateCustomChannel(url, mCredentials, grpc::ChannelArguments());
    if (!channel) {
        throw std::runtime_error("Failed to create channel");
    }

    return SMService::NewStub(channel);
}

void CMClient::RegisterSM(const std::string& url)
{
    LOG_DBG() << "Registering SM service: url=" << url.c_str();

    std::lock_guard<std::mutex> lock(mMutex);

    mSMStub = CreateSMStub(url);

    mCtx = std::make_unique<grpc::ClientContext>();
    mCtx->set_deadline(std::chrono::system_clock::now() + cCMConnectTimeout);

    if (mStream = mSMStub->RegisterSM(mCtx.get()); !mStream) {
        throw std::runtime_error("Failed to register service to SM");
    }

    mCMConnected = true;
    mCV.notify_one();
}

void CMClient::RunCM(std::string url)
{
    LOG_DBG() << "CM client thread started";

    while (!mShutdown) {
        LOG_DBG() << "Connecting to CM...";

        try {
            RegisterSM(url);
            ProcessIncomingSMMessage();

        } catch (const std::exception& e) {
            LOG_ERR() << "Failed to connect to CM: error=" << e.what();
        }

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCMConnected = false;

            mCV.wait_for(lock, cReconnectTimeout, [&] { return mShutdown.load(); });
        }
    }

    LOG_DBG() << "CM client thread stopped";
}

void CMClient::ProcessIncomingSMMessage()
{
    LOG_DBG() << "Processing SM message";

    servicemanager::v4::SMIncomingMessages incomingMsg;

    while (mStream->Read(&incomingMsg)) {
        std::vector<uint8_t> data(incomingMsg.ByteSizeLong());

        if (!incomingMsg.SerializeToArray(data.data(), static_cast<int>(data.size()))) {
            LOG_ERR() << "Failed to serialize message";

            continue;
        }

        LOG_DBG() << "Sending message to handler";

        if (auto err = mMsgHandler->Send(std::move(data)); !err.IsNone()) {
            LOG_ERR() << "Failed to send message: error=" << err;

            return;
        }
    }
}

void CMClient::ProcessOutgoingSMMessages()
{
    LOG_DBG() << "Processing outgoing SM messages";

    while (!mShutdown) {
        auto [msg, err] = mMsgHandler->Receive();
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to receive message: error=" << err;

            return;
        }

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCV.wait(lock, [this] { return mCMConnected || mShutdown.load(); });

            if (mShutdown) {
                return;
            }
        }

        servicemanager::v4::SMOutgoingMessages outgoingMsg;
        if (!outgoingMsg.ParseFromArray(msg.data(), static_cast<int>(msg.size()))) {
            LOG_ERR() << "Failed to parse outgoing message";

            continue;
        }

        LOG_DBG() << "Sending message to CM";

        if (!mStream->Write(outgoingMsg)) {
            LOG_ERR() << "Failed to send message";

            continue;
        }
    }

    LOG_DBG() << "Outgoing SM messages thread stopped";
}
