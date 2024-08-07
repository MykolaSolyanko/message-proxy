/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>

#include <utils/json.hpp>

#include "config.hpp"
#include "logger/logmodule.hpp"

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static aos::common::utils::Duration GetDuration(
    const aos::common::utils::CaseInsensitiveObjectWrapper& object, const std::string& key)
{
    auto value = object.GetValue<std::string>(key);

    if (value.empty()) {
        return {};
    }

    auto ret = aos::common::utils::ParseDuration(value);

    if (!ret.mError.IsNone()) {
        throw std::runtime_error("Failed to parse " + key);
    }

    return ret.mValue;
}

static Download ParseDownloader(const aos::common::utils::CaseInsensitiveObjectWrapper& object)
{
    return Download {
        object.GetValue<std::string>("DownloadDir"),
        object.GetValue<int>("MaxConcurrentDownloads"),
        GetDuration(object, "RetryDelay"),
        GetDuration(object, "MaxRetryDelay"),
    };
}

static VChanConfig ParseVChanConfig(const aos::common::utils::CaseInsensitiveObjectWrapper& object)
{
    return VChanConfig {
        object.GetValue<int>("Domain"),
        object.GetValue<std::string>("XSRXPath"),
        object.GetValue<std::string>("XSTXPath"),
        object.GetValue<std::string>("CertStorage"),
    };
}

static IAMConfig ParseIAMConfig(const aos::common::utils::CaseInsensitiveObjectWrapper& object)
{
    return IAMConfig {
        object.GetValue<std::string>("IAMPublicServerURL"),
        object.GetValue<std::string>("IAMProtectedServerURL"),
        object.GetValue<std::string>("CertStorage"),
        object.GetValue<int>("Port"),
    };
}

static CMConfig ParseCMConfig(const aos::common::utils::CaseInsensitiveObjectWrapper& object)
{
    return CMConfig {
        object.GetValue<std::string>("CMServerURL"),
        object.GetValue<int>("OpenPort"),
        object.GetValue<int>("SecurePort"),
    };
}

/***********************************************************************************************************************
 * Public functions
 **********************************************************************************************************************/

aos::RetWithError<Config> ParseConfig(const std::string& filename)
{
    LOG_DBG() << "Parsing config file: filename=" << filename.c_str();

    std::ifstream file(filename);

    if (!file.is_open()) {
        return {Config {}, aos::Error(aos::ErrorEnum::eFailed, "Failed to open file")};
    }

    auto result = aos::common::utils::ParseJson(file);
    if (!result.mError.IsNone()) {
        return {Config {}, result.mError};
    }

    Config config {};

    try {
        aos::common::utils::CaseInsensitiveObjectWrapper object(result.mValue.extract<Poco::JSON::Object::Ptr>());

        LOG_DBG() << "Parsing config";

        config.mWorkingDir = object.GetValue<std::string>("WorkingDir");
        LOG_DBG() << "WorkingDir: " << config.mWorkingDir.c_str();
        config.mVChan = ParseVChanConfig(object.GetObject("VChan"));
        LOG_DBG() << "VChan parsed successfully";
        config.mCMConfig = ParseCMConfig(object.GetObject("CMConfig"));
        LOG_DBG() << "CMConfig parsed successfully";
        config.mCertStorage = object.GetValue<std::string>("CertStorage");
        LOG_DBG() << "CertStorage: " << config.mCertStorage.c_str();
        config.mCACert = object.GetValue<std::string>("CACert");
        LOG_DBG() << "CACert: " << config.mCACert.c_str();
        config.mImageStoreDir = object.GetValue<std::string>("ImageStoreDir");
        LOG_DBG() << "ImageStoreDir: " << config.mImageStoreDir.c_str();

        config.mDownload = ParseDownloader(object.GetObject("Downloader"));
        LOG_DBG() << "Download parsed successfully";
        config.mIAMConfig = ParseIAMConfig(object.GetObject("IAMConfig"));
        LOG_DBG() << "IAMConfig parsed successfully";

        LOG_DBG() << "Config parsed successfully";

    } catch (const std::exception& e) {
        return {config, aos::Error(aos::ErrorEnum::eFailed, e.what())};
    }

    return config;
}
