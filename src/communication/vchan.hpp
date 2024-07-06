/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VCHAN_HPP_
#define VCHAN_HPP_

#include "types.hpp"
#include <libxenvchan.h>

/**
 * @brief Virtual Channel class
 */
class VChan : public TransportItf {
public:
    VChan(const std::string& path, int domain);

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
    std::string         mPath;
    int                 mDomain;
    struct libxenvchan* mVChan {};
}
#endif // VCHAN_HPP_
