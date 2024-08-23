/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_HPP_
#define APP_HPP_

#include <optional>

#include <Poco/Util/ServerApplication.h>

#include <logger/logger.hpp>
#include <utils/bidirectionalchannel.hpp>

#include <aos/common/crypto/mbedtls/cryptoprovider.hpp>
#include <aos/iam/certmodules/pkcs11/pkcs11.hpp>

#include "cmclient/cmclient.hpp"
#include "communication/cmconnection.hpp"
#include "communication/communicationmanager.hpp"
#include "communication/iamconnection.hpp"
#include "communication/vchanmanager.hpp"
#include "config/config.hpp"
#include "iamclient/iamclient.hpp"

/**
 * Aos message-proxy application.
 */
class App : public Poco::Util::ServerApplication {
protected:
    void initialize(Application& self) override;
    void uninitialize() override;
    void reinitialize(Application& self) override;
    int  main(const ArgVec& args) override;
    void defineOptions(Poco::Util::OptionSet& options) override;

private:
    static constexpr auto cSDNotifyReady     = "READY=1";
    static constexpr auto cDefaultConfigFile = "aos_message_proxy.cfg";

    void HandleHelp(const std::string& name, const std::string& value);
    void HandleVersion(const std::string& name, const std::string& value);
    void HandleProvisioning(const std::string& name, const std::string& value);
    void HandleJournal(const std::string& name, const std::string& value);
    void HandleLogLevel(const std::string& name, const std::string& value);
    void HandleConfigFile(const std::string& name, const std::string& value);
    void HandleTest(const std::string& name, const std::string& value);

    aos::common::logger::Logger mLogger;
    bool                        mStopProcessing = false;
    bool                        mProvisioning   = false;
    std::string                 mConfigFile;

    // aos::common::utils::BiDirectionalChannelFactory<std::vector<uint8_t>>         mIAMChannelFactory;
    // std::optional<aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>> mIAMChannel;
    // std::optional<aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>> mIAMReverseChannel;

    // aos::common::utils::BiDirectionalChannelFactory<std::vector<uint8_t>>         mCMChannelFactory;
    // std::optional<aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>> mCMChannel;
    // std::optional<aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>> mCMReverseChannel;

    aos::crypto::MbedTLSCryptoProvider mCryptoProvider;
    aos::cryptoutils::CertLoader       mCertLoader;
    aos::pkcs11::PKCS11Manager         mPKCS11Manager;

    Config mConfig;

    IAMClient mIAMClient;
    CMClient  mCMClient;

    std::optional<VChanManager> mVChanManager;
    CommunicationManager        mCommunicationManager;
    IAMConnection               mIAMPublicConnection;
    IAMConnection               mIAMProtectedConnection;

    CMConnection mCMConnection;
    bool         mTestMode = false;
};

#endif
