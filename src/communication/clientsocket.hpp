/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CLIENTSOCKET_HPP_
#define CLIENTSOCKET_HPP_

#include "types.hpp"

/**
 * @brief Virtual Channel class
 */
class SocketClient : public TransportItf {
public:
    SocketClient(const std::string& ipAddress, int port);

    /**
     * @brief Connect to the virtual channel
     */
    aos::Error Connect() override;

    /**
     * @brief Read message from the virtual channel
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Read(std::vector<uint8_t>& message) override;

    /**
     * @brief Write message to the virtual channel
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Write(std::vector<uint8_t> message) override;

    /**
     * @brief Close the virtual channel
     *
     * @return aos::Error
     */
    aos::Error Close() override;

private:
    std::string mIPAddress;
    int         mPort;
    int         mFd;
};

#endif // CLIENTSOCKET_HPP_
