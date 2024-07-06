/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IAMCONNECTION_HPP_
#define IAMCONNECTION_HPP_

#include <memory>
#include <mutex>
#include <thread>

#include <aos/common/tools/error.hpp>

#include <utils/bidirectionalchannel.hpp>

#include "iamclient/types.hpp"
#include "types.hpp"

/**
 * IAM connection class.
 */
class IAMConnection : public AosProtocol {
public:
    /**
     * Initialize connection.
     *
     * @param port Port.
     * @param certProvider Certificate provider.
     * @param comManager Communication manager.
     * @param channel Channel.
     * @return aos::Error.
     */
    aos::Error Init(int port, CertProviderItf* certProvider, CommunicationManagerItf& comManager,
        aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>& channel);

    /**
     * Close connection.
     *
     */
    void Close();

private:
    void Run();
    void ReadHandler();
    void WriteHandler();

    aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>* mChannel {};
    bool                                                            mShutdown {};
    std::thread                                                     mConnectThread;
    std::unique_ptr<CommChannelItf>                                 mIAMCommChannel;
};

#endif /* IAMCONNECTION_HPP_ */
