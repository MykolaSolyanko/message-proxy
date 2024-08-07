/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "clientsocket.hpp"

#include "logger/logmodule.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

SocketClient::SocketClient(const std::string& ipAddress, int port)
    : mIPAddress(ipAddress)
    , mPort(port)
    , mFd(-1)
{
    LOG_DBG() << "SocketClient created with IP: " << mIPAddress.c_str() << ", Port: " << mPort;
}

aos::Error SocketClient::Connect()
{
    LOG_DBG() << "Connecting to server";

    mFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mFd == -1) {
        LOG_ERR() << "Failed to create socket: " << strerror(errno);
        return aos::Error {errno};
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(mPort);

    if (inet_pton(AF_INET, mIPAddress.c_str(), &serverAddr.sin_addr) <= 0) {
        LOG_ERR() << "Invalid address/ Address not supported";
        return aos::Error {errno};
    }

    if (connect(mFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG_ERR() << "Connection failed: " << strerror(errno);
        return aos::Error {errno};
    }

    LOG_DBG() << "Connected to server at " << mIPAddress.c_str() << ":" << mPort;

    return aos::ErrorEnum::eNone;
}

aos::Error SocketClient::Read(std::vector<uint8_t>& message)
{
    LOG_DBG() << "Read from the server, expected size=" << message.size();

    ssize_t readBytes = 0;
    while (readBytes < static_cast<ssize_t>(message.size())) {
        ssize_t len = recv(mFd, message.data() + readBytes, message.size() - readBytes, 0);
        if (len < 0) {
            if (errno == EINTR) {
                // System call was interrupted, try again
                continue;
            }
            LOG_ERR() << "Failed to read from socket: " << strerror(errno);
            return aos::Error {errno};
        } else if (len == 0) {
            LOG_ERR() << "Server disconnected";
            return aos::Error {ECONNRESET};
        }

        readBytes += len;
    }

    LOG_DBG() << "Read " << readBytes << " bytes from server";

    return aos::ErrorEnum::eNone;
}

aos::Error SocketClient::Write(std::vector<uint8_t> message)
{
    LOG_DBG() << "Write to the server, size=" << message.size();

    ssize_t writtenBytes = 0;
    while (writtenBytes < static_cast<ssize_t>(message.size())) {
        ssize_t len = send(mFd, message.data() + writtenBytes, message.size() - writtenBytes, 0);
        if (len < 0) {
            if (errno == EINTR) {
                // System call was interrupted, try again
                continue;
            }
            LOG_ERR() << "Failed to write to socket: " << strerror(errno);
            return aos::Error {errno};
        } else if (len == 0) {
            LOG_ERR() << "Server disconnected";
            return aos::Error {ECONNRESET};
        }

        LOG_DBG() << "Written " << len << " bytes";
        writtenBytes += len;
    }

    LOG_DBG() << "Total written: " << writtenBytes << " bytes";

    return aos::ErrorEnum::eNone;
}

aos::Error SocketClient::Close()
{
    LOG_DBG() << "Closing the TCP connection";

    if (mFd != -1) {
        if (close(mFd) == -1) {
            LOG_ERR() << "Failed to close socket: " << strerror(errno);
            return aos::Error {errno};
        }
        mFd = -1;
    }

    return aos::ErrorEnum::eNone;
}