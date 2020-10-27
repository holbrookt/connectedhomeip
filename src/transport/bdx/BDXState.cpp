/**
 *    @file
 *      
 *
 */

#include <transport/bdx/BDXState.h>

#include <transport/bdx/BDXMessages.h>

namespace chip {
namespace BDX {

CHIP_ERROR BDXState::HandleMessageReceived(uint16_t msgType, System::PacketBuffer *msgData)
{
    switch (msgType)
    {
        case kSendInit:
            HandleSendInit();
            break;
        case kSendAccept:
            HandleSendAccept();
            break;
        case kReceiveInit:
            HandleReceiveInit();
            break;
        case kReceiveAccept:
            HandleReceiveAccept();
            break;
        case kBlockQuery:
            HandleBlockQuery();
            break;
        case kBlock:
            HandleBlock();
            break;
        case kBlockEOF:
            HandleBlockEOF();
            break;
        case kBlockAck:
            HandleBlockAck();
            break;
        case kBlockAckEOF:
            HandleBlockAckEOF();
            break;
        default:
            // unknown message type
            break;
    }
}

HandleSendInit()
{

}

HandleSendAccept()
{

}

HandleReceiveInit()
{

}

HandleReceiveAccept(System::PacketBuffer *msgData)
{
    // was the last message sent a ReceiveInit?
    if (mState != kNegotiatingReceive)
    {
        
    }

    // double-check that parameters are acceptable:
    //    need to decode message first

    // acceptable? transfer control
    // acceptable? range control
    // acceptable? max size

    // if acceptable...
    if (mControlMode == kReceiverDrive)
    {
        // send block query
    }

    else if (mControlMode == kSenderDrive)
    {
        // await Block
    }

}

HandleBlockQuery()
{

}

HandleBlock()
{

}

HandleBlockEOF()
{

}

HandleBlockAck()
{

}

HandleBlockAckEOF()
{

}

} // namespace BDX
} // namespace chip

