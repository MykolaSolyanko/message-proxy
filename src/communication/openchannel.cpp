#include "openchannel.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

OpenChannel::OpenChannel(CommChannelItf& channel, int port)
    : mChannel(&channel)
    , mPort(port)
{
}

aos::Error OpenChannel::Connect()
{
    return mChannel->Connect();
}

aos::Error OpenChannel::Read(std::vector<uint8_t>& message)
{
    return mChannel->Read(message);
}

aos::Error OpenChannel::Write(std::vector<uint8_t> message)
{
    return mChannel->Write(std::move(message));
}

aos::Error OpenChannel::Close()
{
    return mChannel->Close();
}
