/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#include <string>

#include <aos/common/tools/error.hpp>
#include <utils/time.hpp>

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/*
 * Downloader configuration.
 */
struct Download {
    std::string                  mDownloadDir;
    int                          mMaxConcurrentDownloads;
    aos::common::utils::Duration mRetryDelay;
    aos::common::utils::Duration mMaxRetryDelay;
};

/*
 * VChan configuration.
 */
struct VChanConfig {
    int         mDomain;
    std::string mXSRXPath;
    std::string mXSTXPath;
    std::string mCertStorage;
};

/*
 * IAM configuration.
 */
struct IAMConfig {
    std::string mIAMPublicServerURL;
    std::string mIAMProtectedServerURL;
    std::string mCertStorage;
    int         mPort;
};

/*
 * CM configuration.
 */
struct CMConfig {
    std::string mCMServerURL;
    int         mOpenPort;
    int         mSecurePort;
};

/*
 * Configuration.
 */
struct Config {
    std::string mWorkingDir;
    VChanConfig mVChan;
    CMConfig    mCMConfig;
    std::string mCertStorage;
    std::string mCACert;
    std::string mImageStoreDir;
    Download    mDownload;
    IAMConfig   mIAMConfig;
};

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*
 * Parse configuration from the file.
 *
 * @param filename Configuration file name.
 * @return RetWithError<Config> Configuration.
 */
aos::RetWithError<Config> ParseConfig(const std::string& filename);

#endif
