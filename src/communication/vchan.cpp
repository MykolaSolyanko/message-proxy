/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vchan.hpp"
#include "logger/logmodule.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

VChan::VChan(const std::string& path, int domain)
    : mPath(path)
    , mDomain(domain)
{
}

aos::Error VChan::Connect()
{
    LOG_DBG() << "Connect to the virtual channel";

    mVChan = libxenvchan_server_init(nullptr, mDomain, mPath.c_str(), 0, 0);
    if (mVChan == nullptr) {
        return aos::Error(aos::ErrorEnum::eFailed, "Failed to initialize the virtual channel");
    }

    mVChan->blocking = -1; // error: overflow in conversion from 'int' to 'signed char:1' changes value from '1' to '-1'

    return aos::ErrorEnum::eNone;
}

aos::Error VChan::Read(std::vector<uint8_t>& message)
{
    LOG_DBG() << "Read from the virtual channel, size=" << message.size();

    int read {};

    while (read < static_cast<int>(message.size())) {
        int len = libxenvchan_read(mVChan, message.data() + read, message.size() - read);
        if (len < 0) {
            return aos::Error(aos::ErrorEnum::eFailed, "Failed to read from the virtual channel");
        }

        read += len;
    }

    return aos::ErrorEnum::eNone;
}

aos::Error VChan::Write(std::vector<uint8_t> message)
{
    LOG_DBG() << "Write to the virtual channel, size=" << message.size();

    int written {};

    while (written < static_cast<int>(message.size())) {
        int len = libxenvchan_write(mVChan, message.data() + written, message.size() - written);
        if (len < 0) {
            return aos::Error(aos::ErrorEnum::eFailed, "Failed to write to the virtual channel");
        }

        written += len;
    }

    return aos::ErrorEnum::eNone;
}

aos::Error VChan::Close()
{
    LOG_DBG() << "Close the virtual channel";

    libxenvchan_close(mVChan);

    return aos::ErrorEnum::eNone;
}
