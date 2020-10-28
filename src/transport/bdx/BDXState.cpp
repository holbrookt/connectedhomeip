/**
 *    @file
 *
 *
 */

#include <transport/bdx/BDXState.h>

#include <transport/bdx/BDXMessages.h>

#include <core/CHIPError.h>

namespace chip {
namespace BDX {

BDXState::BDXState(TransferParams transferParams, SendMessageCallback sendMessageCallback)
{
    mSupportedOptions = transferParams;
    SendMessage       = sendMessageCallback;
}

CHIP_ERROR BDXState::HandleMessageReceived(uint16_t msgType, System::PacketBuffer * msgData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    if (msgData == NULL)
    {
        err = CHIP_ERROR_INVALID_ARGUMENT;
        goto done;
    }

    switch (msgType)
    {
    /*        case kSendInit:
                HandleSendInit();
                break;
            case kSendAccept:
                HandleSendAccept();
                break;
    */
    case kReceiveInit:
        HandleReceiveInit(msgData);
        break;
        /*        case kReceiveAccept:
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
        */
        break;
    default:
        // unknown message type
        break;
    }

done:
    return;
}
/*
void BDXState::HandleSendInit()
{

}

void BDXState::HandleSendAccept()
{

}
*/
void BDXState::HandleReceiveInit(System::PacketBuffer * msgData)
{
    // must be configured as a sender
    if (mSupportedOptions.Role != kSender)
    {
        // TODO: Send StatusReport message with appropriate error
        SendMessage(kStatusReport, NULL);
        goto done;
    }

    // must not be in the middle of a transfer
    if (mState != kIdle)
    {
        // TODO: send ReceiveReject message
        SendMessage(kStatusReport, NULL);
        goto done;
    }

    if (!AreParametersAcceptable())
    {
        // TODO: send ReceiveReject message
        SendMessage(kStatusReport, NULL);
        goto done;
    }

    // TODO: send ReceiveAccept message

    if (mControlMode == kReceiverDrive)
    {
        // TODO: wait for query
    }
    else if (mControlMode == kSenderDrive)
    {
        // TODO: send block
    }

done:
    return;
}
/*
BDXState::HandleReceiveAccept(System::PacketBuffer *msgData)
{
    // was the last message sent a ReceiveInit?
    if (mState != kNegotiatingReceive)
    {
        //TODO
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

BDXState::HandleBlockQuery()
{

}

BDXState::HandleBlock()
{

}

BDXState::HandleBlockEOF()
{

}

BDXState::HandleBlockAck()
{

}

BDXState::HandleBlockAckEOF()
{
    // if role != Receiver
    if (mTransferRole != kReceiver)
    {
        EndTransfer(CHIP_ERROR_INCORRECT_STATE);
    }

    // if state != ending transfer
    if (mState != kFinalizeTransfer)
    {
        // TODO: send StatusReport and end transfer
        EndTransfer(CHIP_ERROR_INCORRECT_STATE);
    }

    // TODO: does counter correspond to BlockEOF counter?
    if (msgData[BlockCounter] !=  mBlockCounter)
    {
        // TODO: send StatusReport and end transfer
    }

    // end transfer and clean up
    EndTransfer(CHIP_NO_ERROR);
}
*/
void BDXState::EndTransfer(CHIP_ERROR error)
{
    // TODO
    // kControlMode = kNotSpecified;
    // mState = kStateIdle;
}

bool BDXState::AreParametersAcceptable()
{
    // TODO
    return true;
}

} // namespace BDX
} // namespace chip
