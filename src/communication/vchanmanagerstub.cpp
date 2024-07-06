/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vchanmanager.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

VChanManager::VChanManager([[maybe_unused]] const Config& cfg)
{
}

VChanManager::~VChanManager()
{
}

aos::Error VChanManager::Connect()
{
    return aos::ErrorEnum::eNone;
}

aos::Error VChanManager::Read([[maybe_unused]] std::vector<uint8_t>& message)
{
    return aos::ErrorEnum::eNone;
}

aos::Error VChanManager::Write([[maybe_unused]] std::vector<uint8_t> message)
{
    return aos::ErrorEnum::eNone;
}

aos::Error VChanManager::Close()
{
    return aos::ErrorEnum::eNone;
}
