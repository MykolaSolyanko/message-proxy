/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CM_TYPES_HPP_
#define CM_TYPES_HPP_

#include <cstdint>
#include <vector>

#include <aos/common/tools/error.hpp>

/**
 * Message handler interface.
 */
class MessageHandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~MessageHandlerItf() = default;

    /**
     * Receive SM message.
     *
     * @return aos::RetWithError<std::vector<uint8_t>>.
     */
    virtual aos::RetWithError<std::vector<uint8_t>> ReceiveSMMessage() = 0;

    /**
     * Send SM message.
     *
     * @param data Data.
     * @return aos::Error.
     */
    virtual aos::Error SendSMMessage(const std::vector<uint8_t>& data) = 0;
};

#endif // CM_TYPES_HPP_
