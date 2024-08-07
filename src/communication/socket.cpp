#include "socket.hpp"
#include "logger/logmodule.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

Socket::Socket(int port)
    : mPort(port)
    , mFd(-1)
    , mServerFd(-1)
{
    LOG_DBG() << "Socket created with Port: " << mPort;
}

aos::Error Socket::Connect()
{
    LOG_DBG() << "Starting TCP server";

    mServerFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mServerFd == -1) {
        LOG_ERR() << "Failed to create socket: " << strerror(errno);
        return aos::Error {errno};
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port        = htons(mPort);

    if (bind(mServerFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG_ERR() << "Bind failed: " << strerror(errno);
        Close();
        return aos::Error {errno};
    }

    if (listen(mServerFd, 1) < 0) {
        LOG_ERR() << "Listen failed: " << strerror(errno);
        Close();
        return aos::Error {errno};
    }

    LOG_DBG() << "TCP Server started, listening on any address, Port: " << mPort;

    // Accept a client connection
    struct sockaddr_in clientAddr;
    socklen_t          clientAddrLen = sizeof(clientAddr);

    mFd = accept(mServerFd, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (mFd < 0) {
        LOG_ERR() << "Accept failed: " << strerror(errno);
        Close();
        return aos::Error {errno};
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    LOG_DBG() << "Client connected: " << clientIP << ":" << ntohs(clientAddr.sin_port);

    return aos::ErrorEnum::eNone;
}

aos::Error Socket::Read(std::vector<uint8_t>& message)
{
    LOG_DBG() << "Read from the client, expected size=" << message.size();

    ssize_t readBytes = 0;
    while (readBytes < static_cast<ssize_t>(message.size())) {
        ssize_t len = recv(mFd, message.data() + readBytes, message.size() - readBytes, 0);
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERR() << "Failed to read from socket: " << strerror(errno);
            return aos::Error {errno};
        } else if (len == 0) {
            LOG_ERR() << "Client disconnected";
            return aos::Error {ECONNRESET};
        }

        readBytes += len;
    }

    LOG_DBG() << "Read " << readBytes << " bytes from client";
    return aos::ErrorEnum::eNone;
}

aos::Error Socket::Write(std::vector<uint8_t> message)
{
    LOG_DBG() << "Write to the client, size=" << message.size();

    ssize_t writtenBytes = 0;
    while (writtenBytes < static_cast<ssize_t>(message.size())) {
        ssize_t len = send(mFd, message.data() + writtenBytes, message.size() - writtenBytes, 0);
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERR() << "Failed to write to socket: " << strerror(errno);
            return aos::Error {errno};
        } else if (len == 0) {
            LOG_ERR() << "Client disconnected";
            return aos::Error {ECONNRESET};
        }

        writtenBytes += len;
    }

    LOG_DBG() << "Total written: " << writtenBytes << " bytes";
    return aos::ErrorEnum::eNone;
}

aos::Error Socket::Close()
{
    LOG_DBG() << "Closing the TCP connection";

    if (mFd != -1) {
        close(mFd);
        mFd = -1;
    }

    if (mServerFd != -1) {
        close(mServerFd);
        mServerFd = -1;
    }

    return aos::ErrorEnum::eNone;
}
