/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Poco/ThreadPool.h>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/util/time_util.h>
#include <servicemanager/v4/servicemanager.grpc.pb.h>

#include "logger/logmodule.hpp"

#include "cmconnection.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

CMConnection::CMConnection()
    : mTaskManager(Poco::ThreadPool::defaultPool())
{
}

aos::Error CMConnection::Init(const Config& cfg, CertProviderItf* certProvider, CommunicationManagerItf& comManager,
    aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>& channel)
{
    LOG_DBG() << "Init CMConnection";

    try {
        mChannel           = &channel;
        mCMCommOpenChannel = comManager.CreateChannel(cfg.mCMConfig.mOpenPort, nullptr);
        if (certProvider != nullptr) {
            mCMCommSecureChannel = comManager.CreateChannel(cfg.mCMConfig.mSecurePort, certProvider);
            mDownloader.emplace(cfg.mDownload.mDownloadDir);
            mImageUnpacker.emplace(cfg.mImageStoreDir);
        }
    } catch (const std::exception& e) {
        return aos::Error(aos::ErrorEnum::eFailed, e.what());
    }

    StartTask([this] { RunOpenChannel(); });
    StartTask([this] { RunSecureChannel(); });
    StartTask([this] { RunFilterMessage(); });

    return aos::ErrorEnum::eNone;
}

void CMConnection::Close()
{
    LOG_DBG() << "Close CMConnection";

    mShutdown = true;

    mOpenMsgChannel.Close();
    mCMCommOpenChannel->Close();

    if (mCMCommSecureChannel != nullptr) {
        mSecureMsgChannel.Close();
        mCMCommSecureChannel->Close();
    }

    mTaskManager.cancelAll();
    mTaskManager.joinAll();
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void CMConnection::RunSecureChannel()
{
    if (mCMCommSecureChannel == nullptr) {
        return;
    }

    LOG_DBG() << "Run CM secure channel";

    while (!mShutdown) {
        if (auto err = mCMCommSecureChannel->Connect(); !err.IsNone()) {
            LOG_ERR() << "Failed to connect error=" << err;

            continue;
        }

        LOG_DBG() << "Secure CM channel connected";

        auto readFuture  = StartTaskWithWait([this]() { ReadSecureMsgHandler(); });
        auto writeFuture = StartTaskWithWait([this]() { WriteSecureMsgHandler(); });

        readFuture.wait();
        writeFuture.wait();
    }

    LOG_DBG() << "Secure channel stopped";
}

void CMConnection::RunOpenChannel()
{
    LOG_DBG() << "Run open channel";

    while (!mShutdown) {
        if (auto err = mCMCommOpenChannel->Connect(); !err.IsNone()) {
            LOG_ERR() << "Failed to connect error=" << err;

            continue;
        }

        auto readFuture  = StartTaskWithWait([this]() { ReadOpenMsgHandler(); });
        auto writeFuture = StartTaskWithWait([this]() { WriteOpenMsgHandler(); });

        readFuture.wait();
        writeFuture.wait();
    }

    LOG_DBG() << "Open channel stopped";
}

void CMConnection::RunFilterMessage()
{
    LOG_DBG() << "Run filter message";

    while (!mShutdown) {
        auto message = mChannel->Receive();
        if (!message.mError.IsNone()) {
            LOG_ERR() << "Failed to receive message error=" << message.mError;

            return;
        }

        if (IsPublicMessage(message.mValue)) {
            LOG_DBG() << "Public message received";

            if (auto err = mOpenMsgChannel.Send(message.mValue); !err.IsNone()) {
                LOG_ERR() << "Failed to send message error=" << err;

                return;
            }

            continue;
        }

        LOG_DBG() << "Secure message received";

        if (mCMCommSecureChannel != nullptr) {
            if (auto err = mSecureMsgChannel.Send(message.mValue); !err.IsNone()) {
                LOG_ERR() << "Failed to send message error=" << err;

                return;
            }

            continue;
        }

        LOG_ERR() << "Secure channel is not initialized";
    }

    LOG_DBG() << "Filter message stopped";
}

bool CMConnection::IsPublicMessage(const std::vector<uint8_t>& message)
{
    servicemanager::v4::SMIncomingMessages incomingMessages;
    if (!incomingMessages.ParseFromArray(message.data(), message.size())) {
        return false;
    }

    return incomingMessages.has_clock_sync();
}

void CMConnection::ReadSecureMsgHandler()
{
    LOG_DBG() << "Read secure message handler";

    while (!mShutdown) {
        auto [message, err] = ReadMessage(mCMCommSecureChannel);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to read secure message error=" << err;

            return;
        }

        servicemanager::v4::SMOutgoingMessages outgoingMessages;
        if (!outgoingMessages.ParseFromArray(message.data(), message.size())) {
            LOG_ERR() << "Failed to parse message";

            continue;
        }

        if (outgoingMessages.has_image_content_request()) {
            LOG_DBG() << "Image content request received";

            StartTask([this, url = outgoingMessages.image_content_request().url(),
                          requestID   = outgoingMessages.image_content_request().request_id(),
                          contentType = outgoingMessages.image_content_request().content_type()] {
                if (auto err = Download(url, requestID, contentType); !err.IsNone()) {
                    LOG_ERR() << "Failed to download error=" << err;
                }
            });

            continue;
        }

        if (auto err = mChannel->Send(std::move(message)); !err.IsNone()) {
            LOG_ERR() << "Failed to send message error=" << err;

            return;
        }
    }
}

aos::Error CMConnection::SendFailedImageContentResponse(uint64_t requestID, const aos::Error& err)
{
    LOG_ERR() << "Send failed image content response requestID=" << requestID << " error=" << err;

    servicemanager::v4::SMIncomingMessages incomingMessages;

    auto imageContentInfo = incomingMessages.mutable_image_content_info();
    auto errorInfo        = imageContentInfo->mutable_error();
    errorInfo->set_aos_code(static_cast<int>(err.Value()));
    errorInfo->set_message(err.Message());
    imageContentInfo->set_request_id(requestID);

    std::vector<uint8_t> data(incomingMessages.ByteSizeLong());

    if (!incomingMessages.SerializeToArray(data.data(), static_cast<int>(data.size()))) {
        return aos::Error(aos::ErrorEnum::eFailed, "failed to serialize message");
    }

    if (auto err = SendMessage(std::move(data), mCMCommSecureChannel); !err.IsNone()) {
        return err;
    }

    return aos::ErrorEnum::eNone;
}

aos::Error CMConnection::Download(const std::string& url, uint64_t requestID, const std::string& contentType)
{
    LOG_DBG() << "Download url=" << url.c_str() << " requestID=" << requestID << " contentType=" << contentType.c_str();

    auto [fileName, err] = mDownloader->DownloadSync(url);
    if (!err.IsNone()) {
        if (auto errSend = SendFailedImageContentResponse(requestID, err); !errSend.IsNone()) {
            return errSend;
        }

        return err;
    }

    auto [contentInfo, errContent] = GetFileContent(fileName, requestID, contentType);
    if (!errContent.IsNone()) {
        if (auto errSend = SendFailedImageContentResponse(requestID, errContent); !errSend.IsNone()) {
            return errSend;
        }

        return errContent;
    }

    if (err = SendImageContentInfo(contentInfo); !err.IsNone()) {
        if (auto errSend = SendFailedImageContentResponse(requestID, err); !errSend.IsNone()) {
            return errSend;
        }

        return err;
    }

    LOG_DBG() << "Image content sent requestID=" << requestID;

    return aos::ErrorEnum::eNone;
}

aos::Error CMConnection::SendImageContentInfo(const ContentInfo& contentInfo)
{
    servicemanager::v4::SMIncomingMessages incomingMessages;

    auto imageContentInfo = incomingMessages.mutable_image_content_info();
    imageContentInfo->set_request_id(contentInfo.mRequestID);

    for (const auto& imageFile : contentInfo.mImageFiles) {
        auto imageFileProto = imageContentInfo->add_image_files();

        LOG_DBG() << "Send image file relativePath=" << imageFile.mRelativePath.c_str();

        imageFileProto->set_relative_path(imageFile.mRelativePath);
        imageFileProto->set_sha256(imageFile.mSha256.data(), imageFile.mSha256.size());
        imageFileProto->set_size(imageFile.mSize);
    }

    std::vector<uint8_t> data(incomingMessages.ByteSizeLong());

    if (!incomingMessages.SerializeToArray(data.data(), static_cast<int>(data.size()))) {
        return aos::Error(aos::ErrorEnum::eFailed, "failed to serialize image content info");
    }

    if (auto err = SendMessage(std::move(data), mCMCommSecureChannel); !err.IsNone()) {
        return err;
    }

    for (const auto& imageContent : contentInfo.mImageContents) {
        servicemanager::v4::SMIncomingMessages incomingMessages;

        auto imageContentProto = incomingMessages.mutable_image_content();
        imageContentProto->set_request_id(imageContent.mRequestID);
        imageContentProto->set_relative_path(imageContent.mRelativePath);
        imageContentProto->set_parts_count(imageContent.mPartsCount);
        imageContentProto->set_part(imageContent.mPart);
        imageContentProto->set_data(imageContent.mData.data(), imageContent.mData.size());

        std::vector<uint8_t> data(incomingMessages.ByteSizeLong());

        if (!incomingMessages.SerializeToArray(data.data(), static_cast<int>(data.size()))) {
            return aos::Error(aos::ErrorEnum::eFailed, "failed to serialize image content");
        }

        if (auto err = SendMessage(std::move(data), mCMCommSecureChannel); !err.IsNone()) {
            return err;
        }
    }

    return aos::ErrorEnum::eNone;
}

aos::RetWithError<ContentInfo> CMConnection::GetFileContent(
    const std::string& url, uint64_t requestID, const std::string& contentType)
{
    auto [unpackedDir, err] = mImageUnpacker->Unpack(url, contentType);
    if (!err.IsNone()) {
        return {ContentInfo(), err};
    }

    LOG_DBG() << "Unpacked image unpackedDir=" << unpackedDir.c_str() << " requestID=" << requestID;

    return ChunkFiles(unpackedDir, requestID);
}

void CMConnection::ReadOpenMsgHandler()
{
    LOG_DBG() << "Read open message handler";

    while (!mShutdown) {
        auto [message, err] = ReadMessage(mCMCommOpenChannel);
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to read open message error=" << err;

            return;
        }

        servicemanager::v4::SMOutgoingMessages outgoingMessages;
        if (!outgoingMessages.ParseFromArray(message.data(), message.size())) {
            LOG_ERR() << "Failed to parse open message";

            continue;
        }

        if (outgoingMessages.has_clock_sync_request()) {
            if (auto err = SendSMClockSync(); !err.IsNone()) {
                LOG_ERR() << "Failed to send clock sync error=" << err;

                continue;
            }
        }

        if (auto err = mChannel->Send(message); !err.IsNone()) {
            LOG_ERR() << "Failed to send message error=" << err;

            return;
        }
    }
}

aos::Error CMConnection::SendSMClockSync()
{
    LOG_DBG() << "Send clock sync";

    servicemanager::v4::SMIncomingMessages incomingMessages;

    auto clockSync                     = incomingMessages.mutable_clock_sync();
    *clockSync->mutable_current_time() = google::protobuf::util::TimeUtil::GetCurrentTime();

    std::vector<uint8_t> data(incomingMessages.ByteSizeLong());

    if (!incomingMessages.SerializeToArray(data.data(), static_cast<int>(data.size()))) {
        return aos::Error(aos::ErrorEnum::eFailed, "failed to serialize message");
    }

    if (auto err = SendMessage(std::move(data), mCMCommOpenChannel); !err.IsNone()) {
        return err;
    }

    return aos::ErrorEnum::eNone;
}

void CMConnection::WriteSecureMsgHandler()
{
    LOG_DBG() << "Write secure message handler";

    while (!mShutdown) {
        auto message = mSecureMsgChannel.Receive();
        if (!message.mError.IsNone()) {
            LOG_ERR() << "Failed to receive secure message error=" << message.mError;

            return;
        }

        if (auto err = SendMessage(std::move(message.mValue), mCMCommSecureChannel); !err.IsNone()) {
            LOG_ERR() << "Failed to write secure message error=" << err;

            return;
        }
    }
}

void CMConnection::WriteOpenMsgHandler()
{
    LOG_DBG() << "Write open message handler";

    while (!mShutdown) {
        auto message = mOpenMsgChannel.Receive();
        if (!message.mError.IsNone()) {
            LOG_ERR() << "Failed to receive open message error=" << message.mError;

            return;
        }

        if (auto err = SendMessage(std::move(message.mValue), mCMCommOpenChannel); !err.IsNone()) {
            LOG_ERR() << "Failed to write open message error=" << err;

            return;
        }
    }
}

aos::Error CMConnection::SendMessage(std::vector<uint8_t> message, std::unique_ptr<CommChannelItf>& channel)
{
    auto protobufHeader = PrepareProtobufHeader(message.size());
    protobufHeader.insert(protobufHeader.end(), message.begin(), message.end());

    return channel->Write(std::move(protobufHeader));
}

aos::RetWithError<std::vector<uint8_t>> CMConnection::ReadMessage(std::unique_ptr<CommChannelItf>& channel)
{
    std::vector<uint8_t> message(cProtobufHeaderSize);
    if (auto err = channel->Read(message); !err.IsNone()) {
        return {std::vector<uint8_t>(), err};
    }

    auto protobufHeader = ParseProtobufHeader(message);
    message.clear();
    message.resize(protobufHeader.mDataSize);

    if (auto err = channel->Read(message); !err.IsNone()) {
        return {std::vector<uint8_t>(), err};
    }

    return {message, aos::ErrorEnum::eNone};
}