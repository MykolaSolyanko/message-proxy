#ifndef COMMUNICATION_TYPES_HPP
#define COMMUNICATION_TYPES_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include <aosprotocol.h>

#include <aos/common/tools/error.hpp>
#include <openssl/sha.h>

#include "iamclient/types.hpp"

// 64kb
static constexpr size_t cMaxMessageSize = 64 * 1024;

class AosProtocol {
public:
    std::vector<uint8_t> PrepareHeader(uint32_t port, const std::vector<uint8_t>& data)
    {
        AosProtocolHeader header {};

        header.mPort     = port;
        header.mDataSize = static_cast<uint32_t>(data.size());

        SHA256(data.data(), data.size(), header.mCheckSum);

        std::vector<uint8_t> headerVector(cHeaderSize);
        std::memcpy(headerVector.data(), &header, cHeaderSize);

        return headerVector;
    }

    std::vector<uint8_t> PrepareProtobufHeader(uint32_t dataSize)
    {
        AosProtobufHeader header {};

        header.mDataSize = dataSize;

        std::vector<uint8_t> headerVector(cProtobufHeaderSize);
        std::memcpy(headerVector.data(), &header, cProtobufHeaderSize);

        return headerVector;
    }

    AosProtobufHeader ParseProtobufHeader(const std::vector<uint8_t>& header)
    {
        AosProtobufHeader headerStruct {};

        std::memcpy(&headerStruct, header.data(), cProtobufHeaderSize);

        return headerStruct;
    }

protected:
    static constexpr size_t cProtobufHeaderSize = sizeof(AosProtobufHeader);

private:
    static constexpr size_t cHeaderSize = sizeof(AosProtocolHeader);
};

class CommChannelItf {
public:
    ~CommChannelItf() = default;

    virtual aos::Error Connect() = 0;

    /**
     * Read message.
     *
     * @param message Message.
     * @return aos::Error.
     */
    virtual aos::Error Read(std::vector<uint8_t>& message) = 0;

    /**
     * Write message.
     *
     * @param message Message.
     * @return aos::Error.
     */
    virtual aos::Error Write(std::vector<uint8_t> message) = 0;

    /**
     * Close channel.
     *
     * @return aos::Error.
     */
    virtual aos::Error Close() = 0;
};

class TransportItf {
public:
    ~TransportItf() = default;

    virtual aos::Error Connect() = 0;

    /**
     * Read message.
     *
     * @param message Message.
     * @return aos::Error.
     */
    virtual aos::Error Read(std::vector<uint8_t>& message) = 0;

    /**
     * Write message.
     *
     * @return aos::Error.
     */
    virtual aos::Error Write(std::vector<uint8_t> message) = 0;

    /**
     * Close channel.
     *
     * @return aos::Error.
     */
    virtual aos::Error Close() = 0;
};

class CommunicationManagerItf : public CommChannelItf {
public:
    virtual std::unique_ptr<CommChannelItf> CreateChannel(int port, CertProviderItf* certProvider) = 0;
};

#endif // COMMUNICATION_TYPES_HPP
