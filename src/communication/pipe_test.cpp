#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#define PORT      30001
#define CLIENT_IP "10.0.0.1" // Replace with the actual client IP address

struct AosProtocolHeader {
    uint32_t mPort;
    uint32_t mDataSize;
    uint8_t  mCheckSum[32];
};

struct AosProtobufHeader {
    char     mMethodName[256];
    uint32_t mDataSize;
};

void calculateSHA256(const std::string& input, uint8_t* output)
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input.c_str(), input.length());
    SHA256_Final(output, &sha256);
}

void printHex(const char* desc, const void* data, size_t size)
{
    const unsigned char* buf = static_cast<const unsigned char*>(data);
    std::cout << desc << " (" << size << " bytes):" << std::endl;
    for (size_t i = 0; i < size; i++) {
        std::cout << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(buf[i]) << " ";
        if ((i + 1) % 16 == 0)
            std::cout << std::endl;
    }
    std::cout << std::dec << std::endl;
}

bool sendMessage(int sock, const struct sockaddr_in* dest_addr, const void* message, size_t size)
{
    ssize_t sent = sendto(sock, message, size, 0, (struct sockaddr*)dest_addr, sizeof(*dest_addr));
    if (sent < 0) {
        std::cerr << "Send failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

int RunPipeTest()
{
    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return 1;
    }

    struct sockaddr_in serverAddr, clientAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    memset(&clientAddr, 0, sizeof(clientAddr));

    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return 1;
    }

    // Set up client address
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, CLIENT_IP, &clientAddr.sin_addr) <= 0) {
        std::cerr << "Invalid client address" << std::endl;
        return 1;
    }

    std::cout << "UDP Server started, sending to " << CLIENT_IP << ":" << PORT << std::endl;

    while (true) {
        // Create and send AosProtocolHeader
        AosProtocolHeader protocolHeader;
        protocolHeader.mPort     = 1234; // Example value
        protocolHeader.mDataSize = 100; // Example value

        std::string timeString = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        calculateSHA256(timeString, protocolHeader.mCheckSum);

        std::cout << "Sending AosProtocolHeader:" << std::endl;
        std::cout << "  Port: " << protocolHeader.mPort << std::endl;
        std::cout << "  Data Size: " << protocolHeader.mDataSize << std::endl;
        printHex("  Checksum", protocolHeader.mCheckSum, sizeof(protocolHeader.mCheckSum));

        if (sendMessage(serverSocket, &clientAddr, &protocolHeader, sizeof(protocolHeader))) {
            std::cout << "Sent AosProtocolHeader" << std::endl;
        } else {
            std::cout << "Failed to send AosProtocolHeader" << std::endl;
        }

        // Small delay
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Create and send AosProtobufHeader
        AosProtobufHeader protobufHeader;
        strncpy(protobufHeader.mMethodName, "ExampleMethod", sizeof(protobufHeader.mMethodName));
        protobufHeader.mMethodName[sizeof(protobufHeader.mMethodName) - 1] = '\0'; // Ensure null-termination
        protobufHeader.mDataSize                                           = 200; // Example value

        std::cout << "Sending AosProtobufHeader:" << std::endl;
        std::cout << "  Method Name: " << protobufHeader.mMethodName << std::endl;
        std::cout << "  Data Size: " << protobufHeader.mDataSize << std::endl;

        if (sendMessage(serverSocket, &clientAddr, &protobufHeader, sizeof(protobufHeader))) {
            std::cout << "Sent AosProtobufHeader" << std::endl;
        } else {
            std::cout << "Failed to send AosProtobufHeader" << std::endl;
        }

        // Delay before next iteration
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    close(serverSocket);
    return 0;
}
