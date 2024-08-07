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
#include "pipe.hpp"
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
    VChanImpl(const Config& cfg)
        : mVChanReader(cfg.mVChan.mXSRXPath)
        , mVChanWriter(cfg.mVChan.mXSTXPath)
    {
    }

    Pipe mVChanReader;
    Pipe mVChanWriter;
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
    if (auto error = mImpl->mVChanReader.Connect(); !error.IsNone()) {
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
    if (auto error = mImpl->mVChanReader.Close(); !error.IsNone()) {
        return error;
    }

    return mImpl->mVChanWriter.Close();
}
