/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vchanmanager.hpp"
#include "vchan.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

struct VChanImpl {
    VChanImpl(const Config& cfg)
        : mVChanReader(cfg.mVChan.mXSRXPath, cfg.mVChan.mDomain)
        , mVChanWriter(cfg.mVChan.mXSTXPath, cfg.mVChan.mDomain)
    {
    }

    VChan mVChanReader;
    VChan mVChanWriter;
};

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
    aos::Error error = mImpl->mVChanReader.Connect();
    if (error) {
        return error;
    }

    return mImpl->mVChanWriter.Connect();
}

aos::Error VChanManager::Read(std::vector<uint8_t>& message)
{
    return mImpl->mVChanReader.Read(message);
}

aos::Error VChanManager::Write(std::vector<uint8_t> message)
{
    return mImpl->mVChanWriter.Write(std::move(message));
}

aos::Error VChanManager::Close()
{
    aos::Error error = mImpl->mVChanReader.Close();
    if (error) {
        return error;
    }

    return mImpl->mVChanWriter.Close();
}
