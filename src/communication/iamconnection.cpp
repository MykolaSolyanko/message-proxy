/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "iamconnection.hpp"
#include "logger/logmodule.hpp"

aos::Error IAMConnection::Init(int port, CertProviderItf* certProvider, CommunicationManagerItf& comManager,
    aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>& channel)
{
    LOG_DBG() << "Init IAM connection";

    mChannel = &channel;

    try {

        mIAMCommChannel = comManager.CreateChannel(port, certProvider);
    } catch (std::exception& e) {
        return aos::Error(aos::ErrorEnum::eFailed, e.what());
    }

    mConnectThread = std::thread(&IAMConnection::Run, this);

    return aos::ErrorEnum::eNone;
}

void IAMConnection::Close()
{
    LOG_DBG() << "Close IAM connection";

    mIAMCommChannel->Close();
    mShutdown = true;

    if (mConnectThread.joinable()) {
        mConnectThread.join();
    }
}

void IAMConnection::Run()
{
    LOG_DBG() << "Run IAM connection";

    while (!mShutdown) {
        if (auto err = mIAMCommChannel->Connect(); !err.IsNone()) {
            LOG_ERR() << "Failed to connect to IAM error=" << err;

            std::unique_lock<std::mutex> lock(mMutex);
            mCondVar.wait_for(lock, cConnectionTimeout, [this]() { return mShutdown; });

            continue;
        }

        auto readThread  = std::thread(&IAMConnection::ReadHandler, this);
        auto writeThread = std::thread(&IAMConnection::WriteHandler, this);

        readThread.join();
        writeThread.join();
    }

    LOG_DBG() << "Run IAM connection finished";
}

void IAMConnection::ReadHandler()
{
    LOG_DBG() << "Read handler IAM connection";

    while (!mShutdown) {
        LOG_DBG() << "Waiting for message from IAM";

        std::vector<uint8_t> message(cProtobufHeaderSize);
        if (auto err = mIAMCommChannel->Read(message); !err.IsNone()) {
            LOG_ERR() << "Failed to read from IAM error=" << err;

            return;
        }

        auto protobufHeader = ParseProtobufHeader(message);
        message.clear();
        message.resize(protobufHeader.mDataSize);

        if (auto err = mIAMCommChannel->Read(message); !err.IsNone()) {
            LOG_ERR() << "Failed to read from IAM error=" << err;

            return;
        }

        if (auto err = mChannel->Send(message); !err.IsNone()) {
            LOG_ERR() << "Failed to send message to IAM error=" << err;

            return;
        }
    }

    LOG_DBG() << "Read handler IAM connection finished";
}

void IAMConnection::WriteHandler()
{
    LOG_DBG() << "Write handler IAM connection";

    while (!mShutdown) {
        auto message = mChannel->Receive();
        if (!message.mError.IsNone()) {
            LOG_ERR() << "Failed to receive message from IAM error=" << message.mError;

            return;
        }

        LOG_DBG() << "Received message from IAM: size=" << message.mValue.size();

        auto header = PrepareProtobufHeader(message.mValue.size());
        header.insert(header.end(), message.mValue.begin(), message.mValue.end());

        LOG_DBG() << "Send message to IAM: size=" << header.size();

        if (auto err = mIAMCommChannel->Write(std::move(header)); !err.IsNone()) {
            LOG_ERR() << "Failed to write to IAM error=" << err;

            return;
        }
    }
}
