/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VCHANMANAGER_HPP_
#define VCHANMANAGER_HPP_

#include "types.hpp"
#include <config/config.hpp>

struct VChanImpl;

/**
 * @brief Virtual Channel Manager class
 */
class VChanManager : public TransportItf {
public:
    VChanManager(const Config& cfg);

    ~VChanManager();
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
    VChanImpl* mImpl {};
};

#endif // VCHANMANAGER_HPP_
