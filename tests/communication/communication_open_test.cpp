#include <optional>

#include <gtest/gtest.h>

#include <test/utils/log.hpp>

#include <iamanager/v5/iamanager.grpc.pb.h>
#include <servicemanager/v4/servicemanager.grpc.pb.h>

#include "communication/communicationmanager.hpp"
#include "config/config.hpp"
#include "stubs/transport.hpp"

using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CommunicationOpenManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        aos::InitLogs();

        mConfig.mIAMConfig.mPort    = 8080;
        mConfig.mCMConfig.mOpenPort = 30001;
        mIAMChannelFactory.emplace();
        mIAMChannel.emplace(mIAMChannelFactory->GetChannel());

        mCMChannelFactory.emplace();
        mCmChannel.emplace(mCMChannelFactory->GetChannel());

        mPipePair.emplace();
        mPipe1.emplace();
        mPipe2.emplace();

        EXPECT_EQ(mPipePair->CreatePair(mPipe1.value(), mPipe2.value()), aos::ErrorEnum::eNone);

        mCommManager.emplace();
        mCommManagerClient.emplace(mPipe2.value());

        mIAMClientChannel = mCommManagerClient->CreateCommChannel(8080);
        mCMClientChannel  = mCommManagerClient->CreateCommChannel(30001);
    }

    void TearDown() override
    {
        mIAMChannelFactory->Close();
        mCMChannelFactory->Close();
        mPipe2->Close();
        mCommManager->Close();
    }

    std::optional<PipePair> mPipePair;
    std::optional<Pipe>     mPipe1;
    std::optional<Pipe>     mPipe2;

    std::shared_ptr<CommChannelItf> mIAMClientChannel;
    std::shared_ptr<CommChannelItf> mCMClientChannel;
    std::optional<CommManager>      mCommManagerClient;
    AosProtocol                     mProtocol;

    std::optional<CommunicationManager> mCommManager;
    Config                              mConfig;
    CertProviderItf*                    mCertProvider {};
    aos::cryptoutils::CertLoaderItf*    mCertLoader {};
    aos::crypto::x509::ProviderItf*     mCryptoProvider {};

    std::optional<aos::common::utils::BiDirectionalChannelFactory<std::vector<uint8_t>>> mIAMChannelFactory;
    std::optional<aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>>        mIAMChannel;
    std::optional<aos::common::utils::BiDirectionalChannelFactory<std::vector<uint8_t>>> mCMChannelFactory;
    std::optional<aos::common::utils::BiDirectionalChannel<std::vector<uint8_t>>>        mCmChannel;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CommunicationOpenManagerTest, TestOpenIAMChannel)
{
    auto iamReverseChannel = mIAMChannel->GetReverse();
    auto cmReverseChannel  = mCmChannel->GetReverse();

    EXPECT_EQ(mCommManager->Init(mConfig, mPipe1.value(), iamReverseChannel, cmReverseChannel), aos::ErrorEnum::eNone);

    iamanager::v5::IAMOutgoingMessages outgoingMsg;
    outgoingMsg.mutable_start_provisioning_response();

    std::vector<uint8_t> messageData(outgoingMsg.ByteSizeLong());
    EXPECT_TRUE(outgoingMsg.SerializeToArray(messageData.data(), messageData.size()));

    auto protobufHeader = mProtocol.PrepareProtobufHeader(messageData.size());
    protobufHeader.insert(protobufHeader.end(), messageData.begin(), messageData.end());
    EXPECT_EQ(mIAMClientChannel->Write(protobufHeader), aos::ErrorEnum::eNone);

    auto [receivedMsg, errReceive] = mIAMChannel->Receive();
    EXPECT_EQ(errReceive, aos::ErrorEnum::eNone);
    EXPECT_TRUE(outgoingMsg.ParseFromArray(receivedMsg.data(), receivedMsg.size()));
    EXPECT_TRUE(outgoingMsg.has_start_provisioning_response());

    servicemanager::v4::SMOutgoingMessages smOutgoingMessages;
    smOutgoingMessages.mutable_node_config_status();
    std::vector<uint8_t> messageData2(smOutgoingMessages.ByteSizeLong());
    EXPECT_TRUE(smOutgoingMessages.SerializeToArray(messageData2.data(), messageData2.size()));

    protobufHeader = mProtocol.PrepareProtobufHeader(messageData2.size());
    protobufHeader.insert(protobufHeader.end(), messageData2.begin(), messageData2.end());

    EXPECT_EQ(mCMClientChannel->Write(protobufHeader), aos::ErrorEnum::eNone);

    auto [receivedMsg2, errReceive2] = mCmChannel->Receive();
    EXPECT_EQ(errReceive2, aos::ErrorEnum::eNone);

    EXPECT_TRUE(smOutgoingMessages.ParseFromArray(receivedMsg2.data(), receivedMsg2.size()));
    EXPECT_TRUE(smOutgoingMessages.has_node_config_status());
}

TEST_F(CommunicationOpenManagerTest, TestSyncClockRequest)
{
    auto iamReverseChannel = mIAMChannel->GetReverse();
    auto cmReverseChannel  = mCmChannel->GetReverse();

    EXPECT_EQ(mCommManager->Init(mConfig, mPipe1.value(), iamReverseChannel, cmReverseChannel), aos::ErrorEnum::eNone);

    servicemanager::v4::SMOutgoingMessages outgoingMessages;
    outgoingMessages.mutable_clock_sync_request();

    std::vector<uint8_t> messageData(outgoingMessages.ByteSizeLong());
    EXPECT_TRUE(outgoingMessages.SerializeToArray(messageData.data(), messageData.size()));
    auto protobufHeader = mProtocol.PrepareProtobufHeader(messageData.size());
    protobufHeader.insert(protobufHeader.end(), messageData.begin(), messageData.end());
    EXPECT_EQ(mCMClientChannel->Write(protobufHeader), aos::ErrorEnum::eNone);

    std::vector<uint8_t> message(sizeof(AosProtobufHeader));
    EXPECT_EQ(mCMClientChannel->Read(message), aos::ErrorEnum::eNone);
    auto header = mProtocol.ParseProtobufHeader(message);
    message.clear();
    message.resize(header.mDataSize);

    EXPECT_EQ(mCMClientChannel->Read(message), aos::ErrorEnum::eNone);
    servicemanager::v4::SMIncomingMessages incomingMessages;
    EXPECT_TRUE(incomingMessages.ParseFromArray(message.data(), message.size()));
    EXPECT_TRUE(incomingMessages.has_clock_sync());

    auto currentTime = incomingMessages.clock_sync().current_time();
    auto now         = std::chrono::system_clock::now();
    auto diff        = std::chrono::duration_cast<std::chrono::seconds>(
        now - std::chrono::system_clock::from_time_t(currentTime.seconds()));
    EXPECT_LT(diff.count(), 1);
}

TEST_F(CommunicationOpenManagerTest, TestSendIAMIncomingMessages)
{
    auto iamReverseChannel = mIAMChannel->GetReverse();
    auto cmReverseChannel  = mCmChannel->GetReverse();

    EXPECT_EQ(mCommManager->Init(mConfig, mPipe1.value(), iamReverseChannel, cmReverseChannel), aos::ErrorEnum::eNone);

    iamanager::v5::IAMIncomingMessages outgoingMsg;
    outgoingMsg.mutable_start_provisioning_request();
    std::vector<uint8_t> messageData(outgoingMsg.ByteSizeLong());
    EXPECT_TRUE(outgoingMsg.SerializeToArray(messageData.data(), messageData.size()));

    EXPECT_EQ(mIAMChannel->Send(messageData), aos::ErrorEnum::eNone);

    std::vector<uint8_t> message(sizeof(AosProtobufHeader));
    EXPECT_EQ(mIAMClientChannel->Read(message), aos::ErrorEnum::eNone);
    auto header = mProtocol.ParseProtobufHeader(message);
    message.clear();
    message.resize(header.mDataSize);

    EXPECT_EQ(mIAMClientChannel->Read(message), aos::ErrorEnum::eNone);
    iamanager::v5::IAMIncomingMessages incomingMessages;
    EXPECT_TRUE(incomingMessages.ParseFromArray(message.data(), message.size()));
    EXPECT_TRUE(incomingMessages.has_start_provisioning_request());
}

TEST_F(CommunicationOpenManagerTest, TestSendClockSync)
{
    auto iamReverseChannel = mIAMChannel->GetReverse();
    auto cmReverseChannel  = mCmChannel->GetReverse();

    EXPECT_EQ(mCommManager->Init(mConfig, mPipe1.value(), iamReverseChannel, cmReverseChannel), aos::ErrorEnum::eNone);

    servicemanager::v4::SMIncomingMessages outgoingMsg;
    outgoingMsg.mutable_clock_sync();
    std::vector<uint8_t> messageData(outgoingMsg.ByteSizeLong());
    EXPECT_TRUE(outgoingMsg.SerializeToArray(messageData.data(), messageData.size()));

    EXPECT_EQ(mCmChannel->Send(messageData), aos::ErrorEnum::eNone);

    std::vector<uint8_t> message(sizeof(AosProtobufHeader));
    EXPECT_EQ(mCMClientChannel->Read(message), aos::ErrorEnum::eNone);
    auto header = mProtocol.ParseProtobufHeader(message);
    message.clear();
    message.resize(header.mDataSize);

    EXPECT_EQ(mCMClientChannel->Read(message), aos::ErrorEnum::eNone);
    servicemanager::v4::SMIncomingMessages incomingMessages;
    EXPECT_TRUE(incomingMessages.ParseFromArray(message.data(), message.size()));
    EXPECT_TRUE(incomingMessages.has_clock_sync());
}

TEST_F(CommunicationOpenManagerTest, TestIAMFlow)
{
    auto iamReverseChannel = mIAMChannel->GetReverse();
    auto cmReverseChannel  = mCmChannel->GetReverse();

    EXPECT_EQ(mCommManager->Init(mConfig, mPipe1.value(), iamReverseChannel, cmReverseChannel), aos::ErrorEnum::eNone);

    iamanager::v5::IAMIncomingMessages outgoingMsg;
    outgoingMsg.mutable_start_provisioning_request();
    std::vector<uint8_t> messageData(outgoingMsg.ByteSizeLong());
    EXPECT_TRUE(outgoingMsg.SerializeToArray(messageData.data(), messageData.size()));

    EXPECT_EQ(mIAMChannel->Send(messageData), aos::ErrorEnum::eNone);

    std::vector<uint8_t> message(sizeof(AosProtobufHeader));
    EXPECT_EQ(mIAMClientChannel->Read(message), aos::ErrorEnum::eNone);
    auto header = mProtocol.ParseProtobufHeader(message);
    message.clear();
    message.resize(header.mDataSize);

    EXPECT_EQ(mIAMClientChannel->Read(message), aos::ErrorEnum::eNone);
    iamanager::v5::IAMIncomingMessages incomingMessages;
    EXPECT_TRUE(incomingMessages.ParseFromArray(message.data(), message.size()));
    EXPECT_TRUE(incomingMessages.has_start_provisioning_request());

    iamanager::v5::IAMOutgoingMessages outgoingMsg2;
    outgoingMsg2.mutable_start_provisioning_response();
    std::vector<uint8_t> messageData2(outgoingMsg2.ByteSizeLong());
    EXPECT_TRUE(outgoingMsg2.SerializeToArray(messageData2.data(), messageData2.size()));

    auto protobufHeader = mProtocol.PrepareProtobufHeader(messageData2.size());
    protobufHeader.insert(protobufHeader.end(), messageData2.begin(), messageData2.end());
    EXPECT_EQ(mIAMClientChannel->Write(protobufHeader), aos::ErrorEnum::eNone);

    auto [receivedMsg, errReceive] = mIAMChannel->Receive();
    EXPECT_EQ(errReceive, aos::ErrorEnum::eNone);
    EXPECT_TRUE(outgoingMsg2.ParseFromArray(receivedMsg.data(), receivedMsg.size()));
    EXPECT_TRUE(outgoingMsg2.has_start_provisioning_response());
}
