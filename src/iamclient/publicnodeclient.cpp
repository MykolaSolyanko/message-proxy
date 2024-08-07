/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <utils/grpchelper.hpp>

#include "logger/logmodule.hpp"
#include "publicnodeclient.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error PublicNodeClient::Init(const IAMConfig& cfg, CertProviderItf& certProvider,
    aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>& channel, bool provisioningMode)
{
    LOG_INF() << "Initializing PublicNodeClient: provisioningMode=" << provisioningMode;

    mMsgHandler = &channel;

    if (auto err = CreateCredentials(cfg.mCertStorage, certProvider, provisioningMode); !err.IsNone()) {
        return err;
    }

    std::string url;
    if (provisioningMode) {
        url = cfg.mIAMPublicServerURL;
    } else {
        url = cfg.mIAMProtectedServerURL;
    }

    mConnectionThread = std::thread(&PublicNodeClient::ConnectionLoop, this, url);

    mHandlerOutgoingMsgsThread = std::thread(&PublicNodeClient::ProcessOutgoingIAMMessages, this);

    return aos::ErrorEnum::eNone;
}

PublicNodeClient::~PublicNodeClient()
{
    LOG_INF() << "Destroying PublicNodeClient";

    {
        std::unique_lock lock {mMutex};

        mShutdown = true;

        if (mRegisterNodeCtx) {
            mRegisterNodeCtx->TryCancel();
        }

        mCV.notify_all();
    }

    if (mConnectionThread.joinable()) {
        mConnectionThread.join();
    }

    if (mHandlerOutgoingMsgsThread.joinable()) {
        mHandlerOutgoingMsgsThread.join();
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

aos::Error PublicNodeClient::CreateCredentials(
    const std::string& certStorage, CertProviderItf& certProvider, bool provisioningMode)
{
    if (provisioningMode) {
        mCredentialList.push_back(grpc::InsecureChannelCredentials());

        if (auto TLSCredentials = certProvider.GetTLSCredentials(); TLSCredentials) {
            mCredentialList.push_back(certProvider.GetTLSCredentials());
        }

        return aos::ErrorEnum::eNone;
    }

    auto res = certProvider.GetMTLSConfig(certStorage);
    if (!res.mError.IsNone()) {
        return AOS_ERROR_WRAP(res.mError);
    }

    mCredentialList.push_back(res.mValue);

    return aos::ErrorEnum::eNone;
}

void PublicNodeClient::ConnectionLoop(const std::string& url) noexcept
{
    LOG_DBG() << "PublicNodeClient connection loop started";

    while (true) {
        try {
            if (RegisterNode(url)) {
                HandleIncomingMessages();
            }
        } catch (const std::exception& e) {
            LOG_ERR() << "PublicNodeClient connection error: " << e.what();
        }

        std::unique_lock lock {mMutex};

        mCV.wait_for(lock, cReconnectInterval, [this]() { return mShutdown; });
        if (mShutdown) {
            break;
        }
    }

    LOG_DBG() << "PublicNodeClient connection loop stopped";
}

bool PublicNodeClient::RegisterNode(const std::string& url)
{
    LOG_DBG() << "Registering node: url=" << url.c_str();

    for (const auto& credentials : mCredentialList) {
        std::unique_lock lock {mMutex};

        if (mShutdown) {
            return false;
        }

        auto channel = grpc::CreateCustomChannel(url, credentials, grpc::ChannelArguments());
        if (!channel) {
            LOG_ERR() << "Failed to create channel";

            continue;
        }

        mPublicNodeServiceStub = PublicNodeService::NewStub(channel);
        if (!mPublicNodeServiceStub) {
            LOG_ERR() << "Failed to create stub";

            continue;
        }

        mRegisterNodeCtx = std::make_unique<grpc::ClientContext>();
        mStream          = mPublicNodeServiceStub->RegisterNode(mRegisterNodeCtx.get());
        if (!mStream) {
            LOG_ERR() << "Failed to create stream";

            continue;
        }

        mConnected = true;
        mCV.notify_all();

        LOG_DBG() << "Connection established";

        return true;
    }

    LOG_ERR() << "Failed to register node";

    return false;
}

void PublicNodeClient::HandleIncomingMessages()
{
    iamanager::v5::IAMIncomingMessages incomingMsg;

    LOG_DBG() << "Handling incoming messages";

    while (mStream->Read(&incomingMsg)) {
        std::vector<uint8_t> message(incomingMsg.ByteSizeLong());

        LOG_DBG() << "Received message: msg=" << incomingMsg.DebugString().c_str();

        if (!incomingMsg.SerializeToArray(message.data(), message.size())) {
            LOG_ERR() << "Failed to serialize message";

            continue;
        }

        if (auto err = mMsgHandler->Send(std::move(message)); !err.IsNone()) {
            LOG_ERR() << "Failed to send message: error=" << err.Message();

            return;
        }
    }
}

void PublicNodeClient::ProcessOutgoingIAMMessages()
{
    LOG_DBG() << "Processing outgoing IAM messages";

    while (!mShutdown) {
        auto [msg, err] = mMsgHandler->Receive();
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to receive message: error=" << err;

            return;
        }

        LOG_DBG() << "Received message from IAM";

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCV.wait(lock, [this] { return mConnected || mShutdown; });

            if (mShutdown) {
                return;
            }
        }

        iamanager::v5::IAMOutgoingMessages outgoingMsg;
        if (!outgoingMsg.ParseFromArray(msg.data(), static_cast<int>(msg.size()))) {
            LOG_ERR() << "Failed to parse outgoing message";

            continue;
        }

        LOG_DBG() << "Sending message to IAM: msg=" << outgoingMsg.DebugString().c_str();

        if (!mStream->Write(outgoingMsg)) {
            LOG_ERR() << "Failed to send message";

            continue;
        }
    }
}