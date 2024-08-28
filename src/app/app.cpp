/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <csignal>
#include <execinfo.h>
#include <iostream>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/trace.h>

#include <Poco/Path.h>
#include <Poco/SignalHandler.h>
#include <Poco/Util/HelpFormatter.h>
#include <systemd/sd-daemon.h>

#include <aos/common/version.hpp>
#include <utils/exception.hpp>

#include "app.hpp"
#include "logger/logmodule.hpp"
// cppcheck-suppress missingInclude
#include "communication/pipe_test.hpp"
#include "version.hpp"

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static void SegmentationHandler(int sig)
{
    static constexpr auto cBacktraceSize = 32;

    void*  array[cBacktraceSize];
    size_t size;

    LOG_ERR() << "Segmentation fault";

    size = backtrace(array, cBacktraceSize);

    backtrace_symbols_fd(array, size, STDERR_FILENO);

    raise(sig);
}

static void RegisterSegfaultSignal()
{
    struct sigaction act { };

    act.sa_handler = SegmentationHandler;
    act.sa_flags   = SA_RESETHAND;

    sigaction(SIGSEGV, &act, nullptr);
}

/***********************************************************************************************************************
 * Protected
 **********************************************************************************************************************/

void App::initialize(Application& self)
{
    if (mTestMode) {
        RunPipeTest();

        return;
    }

    if (mStopProcessing) {
        return;
    }

    RegisterSegfaultSignal();

    auto err = mLogger.Init();
    AOS_ERROR_CHECK_AND_THROW("can't initialize logger", err);

    Application::initialize(self);

    // BIO* stream     = BIO_new_fp(stdout, BIO_NOCLOSE | BIO_FP_TEXT);
    // auto categories = {OSSL_TRACE_CATEGORY_TRACE, OSSL_TRACE_CATEGORY_INIT, OSSL_TRACE_CATEGORY_TLS,
    //     /*OSSL_TRACE_CATEGORY_TLS_CIPHER,*/
    //     OSSL_TRACE_CATEGORY_ENGINE_TABLE, OSSL_TRACE_CATEGORY_ENGINE_REF_COUNT, OSSL_TRACE_CATEGORY_PKCS5V2,
    //     OSSL_TRACE_CATEGORY_PKCS12_KEYGEN, OSSL_TRACE_CATEGORY_PKCS12_DECRYPT, OSSL_TRACE_CATEGORY_X509V3_POLICY,
    //     /*OSSL_TRACE_CATEGORY_BN_CTX,*/ OSSL_TRACE_CATEGORY_CMP, OSSL_TRACE_CATEGORY_STORE,
    //     OSSL_TRACE_CATEGORY_DECODER, OSSL_TRACE_CATEGORY_ENCODER, OSSL_TRACE_CATEGORY_REF_COUNT};
    // for (auto cat : categories) {
    //     OSSL_trace_set_channel(cat, stream);
    // }

    LOG_INF() << "Initialize message-proxy: version = " << AOS_MESSAGE_PROXY_VERSION;

    // BIO* errSSL = BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT);
    // OSSL_trace_set_channel(OSSL_TRACE_CATEGORY_TLS, errSSL);
    // OSSL_trace_set_prefix(OSSL_TRACE_CATEGORY_TLS, "BEGIN TRACE[TLS]");
    // OSSL_trace_set_suffix(OSSL_TRACE_CATEGORY_TLS, "END TRACE[TLS]");

    err = mCryptoProvider.Init();
    AOS_ERROR_CHECK_AND_THROW("can't initialize crypto provider", err);

    err = mCertLoader.Init(mCryptoProvider, mPKCS11Manager);
    AOS_ERROR_CHECK_AND_THROW("can't initialize cert loader", err);

    auto retConfig = ParseConfig(mConfigFile);
    AOS_ERROR_CHECK_AND_THROW("can't parse config", retConfig.mError);

    mConfig = retConfig.mValue;

    if (!mProvisioning) {
        err = mIAMClient.Init(mConfig, mCertLoader, mCryptoProvider, mProvisioning);
        AOS_ERROR_CHECK_AND_THROW("can't initialize IAM client", err);

        err = mCMClient.Init(mConfig, mIAMClient, mCertLoader, mCryptoProvider, mProvisioning);
        AOS_ERROR_CHECK_AND_THROW("can't initialize CM client", err);
    }

    // mVChanManager.emplace(mConfig);

    if (mProvisioning) {
        // if (err = mCommunicationManager.Init(mConfig, mVChanManager.value()); !err.IsNone()) {
        //     AOS_ERROR_CHECK_AND_THROW("can't initialize communication manager", err);
        // }

        // if (err = mCMConnection.Init(mConfig, mCMClient, mCommunicationManager); !err.IsNone()) {
        //     AOS_ERROR_CHECK_AND_THROW("can't initialize CM connection", err);
        // }
    } else {
        // if (err
        //     = mCommunicationManager.Init(mConfig, mVChanManager.value(), &mIAMClient, &mCertLoader,
        //     &mCryptoProvider); !err.IsNone()) { AOS_ERROR_CHECK_AND_THROW("can't initialize communication manager",
        //     err);
        // }

        // if (err = mCMConnection.Init(mConfig, mCMClient, mCommunicationManager, &mIAMClient); !err.IsNone()) {
        //     AOS_ERROR_CHECK_AND_THROW("can't initialize CM connection", err);
        // }

        // if (err = mIAMProtectedConnection.Init(mConfig.mIAMConfig.mSecurePort, mIAMClient.GetProtectedHandler(),
        //         mCommunicationManager, &mIAMClient, mConfig.mVChan.mIAMCertStorage);
        //     !err.IsNone()) {
        //     AOS_ERROR_CHECK_AND_THROW("can't initialize IAM protected connection", err);
        // }
    }

    // if (err
    //     = mIAMPublicConnection.Init(mConfig.mIAMConfig.mOpenPort, mIAMClient.GetPublicHandler(),
    //     mCommunicationManager); !err.IsNone()) { AOS_ERROR_CHECK_AND_THROW("can't initialize IAM public connection",
    //     err);
    // }

    // Notify systemd

    auto ret = sd_notify(0, cSDNotifyReady);
    if (ret < 0) {
        AOS_ERROR_CHECK_AND_THROW("can't notify systemd", ret);
    }
}

void App::uninitialize()
{
    if (mTestMode) {
        return;
    }

    LOG_INF() << "Uninitialize message-proxy";

    mVChanManager->Close();
    mCommunicationManager.Close();
    mIAMPublicConnection.Close();

    if (!mProvisioning) {
        mIAMProtectedConnection.Close();
    }

    mCMConnection.Close();

    Application::uninitialize();
}

void App::reinitialize(Application& self)
{
    LOG_INF() << "Reinitialize message-proxy";

    Application::reinitialize(self);
}

int App::main(const ArgVec& args)
{
    (void)args;

    if (mStopProcessing) {
        return Application::EXIT_OK;
    }

    waitForTerminationRequest();

    return Application::EXIT_OK;
}

void App::defineOptions(Poco::Util::OptionSet& options)
{
    Application::defineOptions(options);

    options.addOption(Poco::Util::Option("help", "h", "displays help information")
                          .callback(Poco::Util::OptionCallback<App>(this, &App::HandleHelp)));
    options.addOption(Poco::Util::Option("version", "", "displays version information")
                          .callback(Poco::Util::OptionCallback<App>(this, &App::HandleVersion)));
    options.addOption(Poco::Util::Option("provisioning", "p", "enables provisioning mode")
                          .callback(Poco::Util::OptionCallback<App>(this, &App::HandleProvisioning)));
    options.addOption(Poco::Util::Option("journal", "j", "redirects logs to systemd journal")
                          .callback(Poco::Util::OptionCallback<App>(this, &App::HandleJournal)));
    options.addOption(Poco::Util::Option("verbose", "v", "sets current log level")
                          .argument("${level}")
                          .callback(Poco::Util::OptionCallback<App>(this, &App::HandleLogLevel)));
    options.addOption(Poco::Util::Option("config", "c", "path to config file")
                          .argument("${file}")
                          .callback(Poco::Util::OptionCallback<App>(this, &App::HandleConfigFile)));
    options.addOption(Poco::Util::Option("test", "t", "run pipe test")
                          .callback(Poco::Util::OptionCallback<App>(this, &App::HandleTest)));
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void App::HandleHelp(const std::string& name, const std::string& value)
{
    (void)name;
    (void)value;

    mStopProcessing = true;

    Poco::Util::HelpFormatter helpFormatter(options());

    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("[OPTIONS]");
    helpFormatter.setHeader("Aos IAM manager service.");
    helpFormatter.format(std::cout);

    stopOptionsProcessing();
}

void App::HandleVersion(const std::string& name, const std::string& value)
{
    (void)name;
    (void)value;

    mStopProcessing = true;

    std::cout << "Aos IA manager version:   " << AOS_MESSAGE_PROXY_VERSION << std::endl;
    std::cout << "Aos core library version: " << AOS_CORE_VERSION << std::endl;

    stopOptionsProcessing();
}

void App::HandleProvisioning(const std::string& name, const std::string& value)
{
    (void)name;
    (void)value;

    mProvisioning = true;
}

void App::HandleJournal(const std::string& name, const std::string& value)
{
    (void)name;
    (void)value;

    mLogger.SetBackend(aos::common::logger::Logger::Backend::eJournald);
}

void App::HandleLogLevel(const std::string& name, const std::string& value)
{
    (void)name;

    aos::LogLevel level;

    auto err = level.FromString(aos::String(value.c_str()));
    if (!err.IsNone()) {
        throw Poco::Exception("unsupported log level", value);
    }

    mLogger.SetLogLevel(level);
}

void App::HandleConfigFile(const std::string& name, const std::string& value)
{
    (void)name;

    mConfigFile = value;
}

void App::HandleTest(const std::string& name, const std::string& value)
{
    (void)name;
    (void)value;

    // mVChanManager->Test();
    // RunPipeTest();
    mTestMode = true;

    // stopOptionsProcessing();
}