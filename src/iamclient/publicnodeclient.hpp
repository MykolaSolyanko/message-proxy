/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PUBLICNODECLIENT_HPP_
#define PUBLICNODECLIENT_HPP_

#include <atomic>
#include <condition_variable>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/channel.h>
#include <grpcpp/security/credentials.h>

#include <iamanager/v5/iamanager.grpc.pb.h>

#include <aos/common/tools/error.hpp>
#include <utils/bidirectionalchannel.hpp>

#include "communication/types.hpp"
#include "config/config.hpp"
#include "types.hpp"
#include "utils/time.hpp"

/**
 * Public node client interface.
 */
class PublicNodeClient : public HandlerItf {
public:
    ~PublicNodeClient();

    /**
     * Initialize the client.
     *
     * @param cfg Configuration.
     * @param certProvider Certificate provider.
     * @param publicServer Public server.
     * @return Error error code.
     */
    aos::Error Init(const IAMConfig& cfg, CertProviderItf& certProvider, bool publicServer);

    void OnConnected() override;
    void OnDisconnected() override;

    aos::Error                              SendMessages(std::vector<uint8_t> messages) override;
    aos::RetWithError<std::vector<uint8_t>> ReceiveMessages() override;

private:
    using StreamPtr = std::unique_ptr<
        grpc::ClientReaderWriterInterface<iamanager::v5::IAMOutgoingMessages, iamanager::v5::IAMIncomingMessages>>;
    using PublicNodeService        = iamanager::v5::IAMPublicNodesService;
    using PublicNodeServiceStubPtr = std::unique_ptr<PublicNodeService::StubInterface>;
    using HandlerFunc              = std::function<aos::Error(const iamanager::v5::IAMIncomingMessages&)>;

    static constexpr auto cReconnectInterval = std::chrono::seconds(10);

    aos::Error CreateCredentials(const std::string& certStorage, CertProviderItf& certProvider, bool publicServer);
    void       ConnectionLoop(const std::string& url) noexcept;
    void       HandleIncomingMessages();
    bool       RegisterNode(const std::string& url);
    void       InitializeHandlers();
    void       ProcessOutgoingIAMMessages();
    void       Close();

    std::vector<std::shared_ptr<grpc::ChannelCredentials>> mCredentialList;

    std::unique_ptr<grpc::ClientContext> mRegisterNodeCtx;
    StreamPtr                            mStream;
    PublicNodeServiceStubPtr             mPublicNodeServiceStub;

    std::thread             mConnectionThread;
    std::thread             mHandlerOutgoingMsgsThread;
    std::condition_variable mCV;
    bool                    mShutdown {};
    bool                    mConnected {};
    std::mutex              mMutex;
    std::string             mUrl;
    bool                    mPublicServer;

    aos::common::utils::Channel<std::vector<uint8_t>> mOutgoingMsgChannel;
    aos::common::utils::Channel<std::vector<uint8_t>> mIncomingMsgChannel;

    bool mNotifyConnected {};

    // aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>* mMsgHandler {};
};

#endif // PUBLICNODECLIENT_HPP_
