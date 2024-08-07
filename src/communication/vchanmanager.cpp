/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vchanmanager.hpp"
#ifdef VCHAN
#include "vchan.hpp"
#else
#include "socket.hpp"
#endif

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

#ifdef VCHAN
struct VChanImpl {
    VChanImpl(const Config& cfg)
        : mVChanReader(cfg.mVChan.mXSRXPath, cfg.mVChan.mDomain)
        , mVChanWriter(cfg.mVChan.mXSTXPath, cfg.mVChan.mDomain)
    {
    }

    VChan mVChanReader;
    VChan mVChanWriter;
};
#else
struct VChanImpl {
    VChanImpl([[maybe_unused]] const Config& cfg)
        : mSocket(30001)
    {
    }

    Socket mSocket;
};
#endif

VChanManager::VChanManager(const Config& cfg)
    : mImpl(new VChanImpl(cfg))
{
}

VChanManager::~VChanManager()
{
    delete mImpl;
}

aos::Error VChanManager::Connect()
{
    // if (auto error = mImpl->mVChanReader.Connect(); !error.IsNone()) {
    //     return error;
    // }

    // return mImpl->mVChanWriter.Connect();

    // if (auto error = mImpl->mSocket.Connect(); !error.IsNone()) {
    //     return error;
    // }

    return mImpl->mSocket.Connect();
}

aos::Error VChanManager::Read(std::vector<uint8_t>& message)
{
    return mImpl->mSocket.Read(message);
}

aos::Error VChanManager::Write(std::vector<uint8_t> message)
{
    return mImpl->mSocket.Write(std::move(message));
}

aos::Error VChanManager::Close()
{
    return mImpl->mSocket.Close();
}
