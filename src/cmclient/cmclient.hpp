/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CMCLIENT_HPP_
#define CMCLIENT_HPP_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>

#include <grpcpp/security/credentials.h>
#include <servicemanager/v4/servicemanager.grpc.pb.h>

#include <aos/common/cryptoutils.hpp>
#include <aos/common/tools/error.hpp>
#include <utils/bidirectionalchannel.hpp>
#include <utils/channel.hpp>

#include "communication/types.hpp"
#include "config/config.hpp"
#include "iamclient/types.hpp"
#include "types.hpp"

using SMService        = servicemanager::v4::SMService;
using SMServiceStubPtr = std::unique_ptr<SMService::StubInterface>;

/**
 * CMClient class.
 */
class CMClient : public HandlerItf {
public:
    /**
     *  Destructor.
     */
    ~CMClient();

    /**
     *  Constructor.
     */
    CMClient() = default;

    /**
     *  Initialize CMClient.
     *
     * @param config configuration.
     * @param certProvider certificate provider.
     * @param certLoader certificate loader.
     * @param cryptoProvider crypto provider.
     * @param channel channel.
     * @param insecureConnection insecure connection.
     * @return aos::Error.
     */
    aos::Error Init(const Config& config, CertProviderItf& certProvider, aos::cryptoutils::CertLoaderItf& certLoader,
        aos::crypto::x509::ProviderItf& cryptoProvider, bool insecureConnection = false);

    void OnConnected() override;
    void OnDisconnected() override;

    aos::Error                              SendMessages(std::vector<uint8_t> messages) override;
    aos::RetWithError<std::vector<uint8_t>> ReceiveMessages() override;

private:
    constexpr static auto cReconnectTimeout = std::chrono::seconds(10);
    constexpr static auto cCMConnectTimeout = std::chrono::seconds(30);

    using StreamPtr = std::unique_ptr<grpc::ClientReaderWriterInterface<::servicemanager::v4::SMOutgoingMessages,
        servicemanager::v4::SMIncomingMessages>>;

    void                                                         RunCM(std::string url);
    SMServiceStubPtr                                             CreateSMStub(const std::string& url);
    void                                                         RegisterSM(const std::string& url);
    void                                                         ProcessIncomingSMMessage();
    void                                                         ProcessOutgoingSMMessages();
    aos::RetWithError<std::shared_ptr<grpc::ChannelCredentials>> CreateCredentials(
        const std::string& certStorage, bool insecureConnection);
    void Close();

    std::thread mCMThread;
    std::thread mHandlerOutgoingMsgsThread;

    std::atomic<bool> mShutdown {false};
    bool              mCMConnected {false};

    std::mutex              mMutex;
    std::condition_variable mCV;

    std::shared_ptr<grpc::ChannelCredentials> mCredentials;
    SMServiceStubPtr                          mSMStub;
    StreamPtr                                 mStream;
    std::unique_ptr<grpc::ClientContext>      mCtx;
    std::string                               mUrl;

    CertProviderItf*                                  mCertProvider {};
    aos::cryptoutils::CertLoaderItf*                  mCertLoader {};
    aos::crypto::x509::ProviderItf*                   mCryptoProvider {};
    aos::common::utils::Channel<std::vector<uint8_t>> mOutgoingMsgChannel;
    aos::common::utils::Channel<std::vector<uint8_t>> mIncomingMsgChannel;
    bool                                              mNotifyConnected {};
};

#endif
