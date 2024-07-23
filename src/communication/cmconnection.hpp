/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CMCONNECTION_HPP_
#define CMCONNECTION_HPP_

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include <Poco/Runnable.h>
#include <Poco/TaskManager.h>
#include <Poco/UUID.h>
#include <Poco/UUIDGenerator.h>

#include <aos/common/tools/error.hpp>

#include <utils/bidirectionalchannel.hpp>

#include "config/config.hpp"
#include "downloader/downloader.hpp"
#include "filechunker/filechunker.hpp"
#include "imageunpacker/imageunpacker.hpp"
#include "types.hpp"

/**
 * CM connection class.
 */
class CMConnection : public AosProtocol {
public:
    /**
     * Constructor.
     */
    CMConnection();

    /**
     * Initialize connection.
     *
     * @param cfg Configuration.
     * @param certProvider Certificate provider.
     * @param comManager Communication manager.
     * @param channel Channel.
     * @return aos::Error.
     */
    aos::Error Init(const Config& cfg, CertProviderItf* certProvider, CommunicationManagerItf& comManager,
        aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>& channel);

    /**
     * Close connection.
     */
    void Close();

private:
    class Task : public Poco::Task {
    public:
        using Callback = std::function<void()>;

        Task(Callback callback)
            : Poco::Task(generateTaskName())
            , mCallback(std::move(callback))
        {
        }

        void runTask() override { mCallback(); }

    private:
        static std::string generateTaskName()
        {
            static Poco::UUIDGenerator mUuidGenerator;

            return mUuidGenerator.create().toString();
        }

        Callback mCallback;
    };

    std::future<void> StartTaskWithWait(std::function<void()> func)
    {
        auto              promise = std::make_shared<std::promise<void>>();
        std::future<void> future  = promise->get_future();

        auto task = new Task([func, promise]() {
            func();
            promise->set_value();
        });

        mTaskManager.start(task);
        return future;
    }

    void StartTask(std::function<void()> func) { mTaskManager.start(new Task(std::move(func))); }

    void                           RunSecureChannel();
    void                           RunOpenChannel();
    void                           RunFilterMessage();
    void                           ReadSecureMsgHandler();
    void                           ReadOpenMsgHandler();
    void                           WriteSecureMsgHandler();
    void                           WriteOpenMsgHandler();
    bool                           IsPublicMessage(const std::vector<uint8_t>& message);
    aos::Error                     SendSMClockSync();
    aos::Error                     Download(const std::string& url, uint64_t requestID, const std::string& contentType);
    aos::Error                     SendFailedImageContentResponse(uint64_t requestID, const aos::Error& err);
    aos::Error                     SendImageContentInfo(const ContentInfo& contentInfo);
    aos::RetWithError<ContentInfo> GetFileContent(
        const std::string& url, uint64_t requestID, const std::string& contentType);
    aos::Error SendMessage(std::vector<uint8_t> message, std::unique_ptr<CommChannelItf>& channel);
    aos::RetWithError<std::vector<uint8_t>> ReadMessage(std::unique_ptr<CommChannelItf>& channel);

    std::unique_ptr<CommChannelItf>                                 mCMCommOpenChannel;
    std::unique_ptr<CommChannelItf>                                 mCMCommSecureChannel;
    aos::common::utils::Channel<std::vector<uint8_t>>               mSecureMsgChannel;
    aos::common::utils::Channel<std::vector<uint8_t>>               mOpenMsgChannel;
    aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>* mChannel {};

    Poco::TaskManager            mTaskManager;
    std::atomic<bool>            mShutdown {};
    std::optional<Downloader>    mDownloader;
    std::optional<ImageUnpacker> mImageUnpacker;
};

#endif /* CMCONNECTION_HPP_ */