#include <openssl/sha.h>

#include "communication/utils.hpp"
#include "communicationmanager.hpp"
#include "logger/logmodule.hpp"
#include "openchannel.hpp"
#include "securechannel.hpp"

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

constexpr std::chrono::milliseconds cConnectionTimeout = std::chrono::seconds(10);
constexpr std::chrono::seconds      cReconnectTimeout  = std::chrono::seconds(5);

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static void CalculateChecksum(const std::vector<uint8_t>& data, uint8_t* checksum)
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(checksum, &sha256);
}

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error CommunicationManager::Init(const Config& cfg, TransportItf& transport,
    /*     aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>&                  iamChannel,
        [[maybe_unused]] aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>& cmChannel, */
    CertProviderItf* certProvider, aos::cryptoutils::CertLoaderItf* certLoader,
    aos::crypto::x509::ProviderItf* cryptoProvider)
{
    LOG_DBG() << "Init CommunicationManager";

    mTransport      = &transport;
    mCertProvider   = certProvider;
    mCertLoader     = certLoader;
    mCryptoProvider = cryptoProvider;
    mCfg            = &cfg;

    // if (auto err = mIAMConnection.Init(cfg.mIAMConfig.mPort, certProvider, *this, iamChannel);
    //     err != aos::ErrorEnum::eNone) {
    //     return err;
    // }

    // if (auto err = mCMConnection.Init(cfg, certProvider, *this, cmChannel); !err.IsNone()) {
    //     return err;
    // }

    mThread = std::thread(&CommunicationManager::Run, this);

    return aos::ErrorEnum::eNone;
}

std::unique_ptr<CommChannelItf> CommunicationManager::CreateChannel(
    int port, CertProviderItf* certProvider, const std::string& certStorage)
{
    std::unique_ptr<CommunicationChannel> chan = std::make_unique<CommunicationChannel>(port, this);

    if (certProvider == nullptr) {
        LOG_DBG() << "Create open channel";

        auto openchannel = std::make_unique<OpenChannel>(chan.get(), port);

        mChannels[port] = std::move(chan);

        return openchannel;
    }

    LOG_DBG() << "Create secure channel for port=" << port << " certStorage=" << certStorage.c_str();

    auto securechannel = std::make_unique<SecureChannel>(
        *mCfg, *chan, *certProvider, *mCertLoader, *mCryptoProvider, port, certStorage);

    mChannels[port] = std::move(chan);

    return securechannel;
}

aos::Error CommunicationManager::Connect()
{
    std::lock_guard<std::mutex> lock(mMutex);

    LOG_DBG() << "Connect communication manager";

    if (mIsConnected) {
        return aos::ErrorEnum::eNone;
    }

    LOG_DBG() << "Connect CommunicationManager";

    auto err = mTransport->Connect();
    if (!err.IsNone()) {
        return err;
    }

    mIsConnected = true;
    mCondVar.notify_all();

    return aos::ErrorEnum::eNone;
}

aos::Error CommunicationManager::Read([[maybe_unused]] std::vector<uint8_t>& message)
{
    return aos::ErrorEnum::eNotSupported;
}

aos::Error CommunicationManager::Write(std::vector<uint8_t> message)
{
    std::unique_lock<std::mutex> lock(mMutex);
    mCondVar.wait_for(lock, cConnectionTimeout, [this]() { return mIsConnected; });

    if (!mIsConnected) {
        return aos::ErrorEnum::eTimeout;
    }

    return mTransport->Write(std::move(message));
}

aos::Error CommunicationManager::Close()
{
    LOG_DBG() << "Close CommunicationManager";

    if (mShutdown) {
        return aos::ErrorEnum::eNone;
    }

    LOG_DBG() << "Close CommunicationManager";

    mShutdown = true;

    aos::Error err;

    {
        std::lock_guard<std::mutex> lock(mMutex);

        err = mTransport->Close();
        mCondVar.notify_all();
    }

    // mIAMConnection.Close();
    // mCMConnection.Close();

    if (mThread.joinable()) {
        mThread.join();
    }

    return err;
}

void CommunicationManager::Run()
{
    LOG_DBG() << "Run CommunicationManager";

    while (!mShutdown) {
        if (auto err = Connect(); !err.IsNone()) {
            LOG_ERR() << "Failed to connect communication manager error=" << err;

            {
                std::unique_lock<std::mutex> lock(mMutex);
                mCondVar.wait_for(lock, cReconnectTimeout, [this]() { return mIsConnected || mShutdown; });
            }

            continue;
        }

        if (auto err = ReadHandler(); !err.IsNone()) {
            std::lock_guard<std::mutex> lock(mMutex);

            LOG_ERR() << "Failed to read error=" << err;
            mIsConnected = false;

            continue;
        }
    }
}

aos::Error CommunicationManager::ReadHandler()
{
    LOG_DBG() << "Read handler CommunicationManager";

    while (!mShutdown) {
        // Read header
        std::vector<uint8_t> headerBuffer(sizeof(AosProtocolHeader));
        auto                 err = mTransport->Read(headerBuffer);
        if (!err.IsNone()) {
            return err;
        }

        LOG_DBG() << "Received header";

        AosProtocolHeader header;
        std::memcpy(&header, headerBuffer.data(), sizeof(AosProtocolHeader));

        int port = header.mPort;

        // Read body
        std::vector<uint8_t> message(header.mDataSize);
        err = mTransport->Read(message);
        if (!err.IsNone()) {
            return err;
        }

        LOG_DBG() << "Received message port=" << port << " size=" << message.size();

        std::array<uint8_t, SHA256_DIGEST_LENGTH> checksum;
        CalculateChecksum(message, checksum.data());

        if (std::memcmp(checksum.data(), header.mCheckSum, SHA256_DIGEST_LENGTH) != 0) {
            LOG_ERR() << "Checksum mismatch";

            continue;
        }

        if (mChannels.find(port) == mChannels.end()) {
            LOG_ERR() << "Channel not found port=" << port;

            continue;
        }

        LOG_DBG() << "Send message to channel port=" << port;

        if (err = mChannels[port]->Receive(std::move(message)); !err.IsNone()) {
            return err;
        }
    }

    return aos::ErrorEnum::eNone;
}
