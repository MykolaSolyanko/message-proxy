/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "logger/logmodule.hpp"
#include "pipe.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Pipe::Pipe(const std::string& path)
    : mPath(path)
    , mFd(-1) // Initialize mFd to an invalid value
{
}

aos::Error Pipe::Connect()
{
    LOG_DBG() << "Connect to the pipe";

    mFd = open(mPath.c_str(), O_RDWR);
    if (mFd == -1) {
        return aos::Error {errno};
    }

    return aos::ErrorEnum::eNone;
}

aos::Error Pipe::Read(std::vector<uint8_t>& message)
{
    LOG_DBG() << "Read from the pipe, size=" << message.size();

    ssize_t readBytes = 0;
    while (readBytes < static_cast<ssize_t>(message.size())) {
        ssize_t len = read(mFd, message.data() + readBytes, message.size() - readBytes);
        if (len < 0) {
            return aos::Error {errno};
        }

        readBytes += len;
    }

    return aos::ErrorEnum::eNone;
}

aos::Error Pipe::Write(std::vector<uint8_t> message)
{
    LOG_DBG() << "Write to the pipe, size=" << message.size();

    ssize_t writtenBytes = 0;
    while (writtenBytes < static_cast<ssize_t>(message.size())) {
        ssize_t len = write(mFd, message.data() + writtenBytes, message.size() - writtenBytes);
        if (len < 0) {
            return aos::Error {errno};
        }

        writtenBytes += len;
    }

    return aos::ErrorEnum::eNone;
}

aos::Error Pipe::Close()
{
    LOG_DBG() << "Close the pipe";

    if (close(mFd) == -1) {
        return aos::Error {errno};
    }

    return aos::ErrorEnum::eNone;
}
