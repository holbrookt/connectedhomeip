/**
 *    @file
 *      Implementation for the TransferSession class.
 *      // TODO: Support Asynchronous mode. Currently, only Synchronous mode is supported.
 */

#include <protocols/bdx/BdxTransferSession.h>

#include <protocols/Protocols.h>
#include <protocols/bdx/BdxMessages.h>
#include <support/CodeUtils.h>
#include <system/SystemPacketBuffer.h>

namespace {
const uint8_t kBdxVersion = 0; ///< The version of this implementation of the BDX spec
}

/**
 * @brief
 *   Allocate a new PacketBuffer and write data from a BDX message struct.
 */
template <class BdxMsgType>
CHIP_ERROR WriteToPacketBuffer(const BdxMsgType & msgStruct, ::chip::System::PacketBufferHandle & msgBuf)
{
    CHIP_ERROR err     = CHIP_NO_ERROR;
    size_t msgDataSize = msgStruct.MessageSize();
    msgBuf             = ::chip::System::PacketBufferHandle::New(static_cast<uint16_t>(msgDataSize));
    if (msgBuf.IsNull())
    {
        return CHIP_ERROR_NO_MEMORY;
    }

    ::chip::BufBound bbuf(msgBuf->Start(), msgBuf->AvailableDataLength());
    msgStruct.WriteToBuffer(bbuf);
    msgBuf->SetDataLength(static_cast<uint16_t>(bbuf.Needed()));

    return err;
}

namespace chip {
namespace bdx {

TransferSession::TransferSession()
{
    mState         = kState_Idle;
    mPendingOutput = kOutput_None;
    mSuppportedXferOpts.SetRaw(0);
}

void TransferSession::PollOutput(OutputEvent & event, uint64_t curTimeMs)
{
    event = OutputEvent();

    if (mAwaitingResponse && (mTimeoutStartTimeMs + mTimeoutMs <= curTimeMs))
    {
        event             = OutputEvent(kOutput_TransferTimeout);
        mState            = kState_Error;
        mAwaitingResponse = false;
        return;
    }

    switch (mPendingOutput)
    {
    case kOutput_None:
        event = OutputEvent(kOutput_None);
        break;
    case kOutput_InternalError:
        event = OutputEvent::StatusReportEvent(kOutput_InternalError, mStatusReportData);
        break;
    case kOutput_StatusReceived:
        event = OutputEvent::StatusReportEvent(kOutput_StatusReceived, mStatusReportData);
        break;
    case kOutput_MsgToSend:
        event               = OutputEvent(kOutput_MsgToSend);
        event.MsgData       = std::move(mPendingMsgHandle);
        mTimeoutStartTimeMs = curTimeMs;
        break;
    case kOutput_InitReceived:
        event = OutputEvent::TransferInitEvent(mTransferRequestData, std::move(mPendingMsgHandle));
        break;
    case kOutput_AcceptReceived:
        event = OutputEvent::TransferAcceptEvent(mTransferAcceptData, std::move(mPendingMsgHandle));
        break;
    case kOutput_QueryReceived:
        event = OutputEvent(kOutput_QueryReceived);
        break;
    case kOutput_BlockReceived:
        event = OutputEvent::BlockDataEvent(mBlockEventData, std::move(mPendingMsgHandle));
        break;
    case kOutput_AckReceived:
        event = OutputEvent(kOutput_AckReceived);
        break;
    case kOutput_AckEOFReceived:
        event = OutputEvent(kOutput_AckEOFReceived);
        break;
    default:
        event = OutputEvent(kOutput_None);
        break;
    }

    mPendingOutput = kOutput_None;
}

CHIP_ERROR TransferSession::StartTransfer(TransferRole role, const TransferInitData & initData, uint32_t timeoutMs,
                                          uint64_t curTimeMs)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    MessageType msgType;
    TransferInit initMsg;

    VerifyOrExit(mState == kState_Idle, err = CHIP_ERROR_INCORRECT_STATE);

    mRole      = role;
    mTimeoutMs = timeoutMs;

    // Set transfer parameters. They may be overridden later by an Accept message
    mSuppportedXferOpts.SetRaw(initData.TransferCtlFlagsRaw);
    mMaxSupportedBlockSize = initData.MaxBlockSize;
    mStartOffset           = initData.StartOffset;
    mTransferLength        = initData.Length;

    // Prepare TransferInit message
    initMsg.TransferCtlOptions.SetRaw(initData.TransferCtlFlagsRaw);
    initMsg.Version        = kBdxVersion;
    initMsg.MaxBlockSize   = mMaxSupportedBlockSize;
    initMsg.StartOffset    = mStartOffset;
    initMsg.MaxLength      = mTransferLength;
    initMsg.FileDesignator = initData.FileDesignator;
    initMsg.FileDesLength  = initData.FileDesLength;
    initMsg.Metadata       = initData.Metadata;
    initMsg.MetadataLength = initData.MetadataLength;

    err = WriteToPacketBuffer<TransferInit>(initMsg, mPendingMsgHandle);
    SuccessOrExit(err);

    msgType = (mRole == kRole_Sender) ? kBdxMsg_SendInit : kBdxMsg_ReceiveInit;
    err     = AttachBdxHeader(msgType, mPendingMsgHandle);
    SuccessOrExit(err);

    mState              = kState_AwaitingAccept;
    mAwaitingResponse   = true;
    mTimeoutStartTimeMs = curTimeMs;

    mPendingOutput = kOutput_MsgToSend;

exit:
    return err;
}

CHIP_ERROR TransferSession::WaitForTransfer(TransferRole role, BitFlags<uint8_t, TransferControlFlags> xferControlOpts,
                                            uint16_t maxBlockSize, uint32_t timeoutMs)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    VerifyOrExit(mState == kState_Idle, err = CHIP_ERROR_INCORRECT_STATE);

    // Used to determine compatibility with any future TransferInit parameters
    mRole                  = role;
    mTimeoutMs             = timeoutMs;
    mSuppportedXferOpts    = xferControlOpts;
    mMaxSupportedBlockSize = maxBlockSize;

    mState = kState_AwaitingInitMsg;

exit:
    return err;
}

CHIP_ERROR TransferSession::AcceptTransfer(const TransferAcceptData & acceptData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    System::PacketBufferHandle outMsgBuf;
    BitFlags<uint8_t, TransferControlFlags> proposedControlOpts;

    VerifyOrExit(mState == kState_NegotiateTransferParams, err = CHIP_ERROR_INCORRECT_STATE);
    VerifyOrExit(mPendingOutput == kOutput_None, err = CHIP_ERROR_INCORRECT_STATE);

    // Don't allow a Control method that wasn't supported by the initiator
    // MaxBlockSize can't be larger than the proposed value
    proposedControlOpts.SetRaw(mTransferRequestData.TransferCtlFlagsRaw);
    VerifyOrExit(proposedControlOpts.Has(acceptData.ControlMode), err = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(acceptData.MaxBlockSize <= mTransferRequestData.MaxBlockSize, err = CHIP_ERROR_INVALID_ARGUMENT);

    mTransferMaxBlockSize = acceptData.MaxBlockSize;

    if (mRole == kRole_Sender)
    {
        mStartOffset    = acceptData.StartOffset;
        mTransferLength = acceptData.Length;

        ReceiveAccept acceptMsg;
        acceptMsg.TransferCtlFlags.Set(acceptData.ControlMode);
        acceptMsg.Version        = mTransferVersion;
        acceptMsg.MaxBlockSize   = acceptData.MaxBlockSize;
        acceptMsg.StartOffset    = acceptData.StartOffset;
        acceptMsg.Length         = acceptData.Length;
        acceptMsg.Metadata       = acceptData.Metadata;
        acceptMsg.MetadataLength = acceptData.MetadataLength;

        err = WriteToPacketBuffer<ReceiveAccept>(acceptMsg, mPendingMsgHandle);
        SuccessOrExit(err);

        err = AttachBdxHeader(kBdxMsg_ReceiveAccept, mPendingMsgHandle);
        SuccessOrExit(err);
    }
    else
    {
        SendAccept acceptMsg;
        acceptMsg.TransferCtlFlags.Set(acceptData.ControlMode);
        acceptMsg.Version        = mTransferVersion;
        acceptMsg.MaxBlockSize   = acceptData.MaxBlockSize;
        acceptMsg.Metadata       = acceptData.Metadata;
        acceptMsg.MetadataLength = acceptData.MetadataLength;

        err = WriteToPacketBuffer<SendAccept>(acceptMsg, mPendingMsgHandle);
        SuccessOrExit(err);

        err = AttachBdxHeader(kBdxMsg_SendAccept, mPendingMsgHandle);
        SuccessOrExit(err);
    }

    mPendingOutput = kOutput_MsgToSend;

    mState = kState_TransferInProgress;

    if ((mRole == kRole_Receiver && mControlMode == kControl_SenderDrive) ||
        (mRole == kRole_Sender && mControlMode == kControl_ReceiverDrive))
    {
        mAwaitingResponse = true;
    }

exit:
    return err;
}

CHIP_ERROR TransferSession::PrepareBlockQuery()
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    BlockQuery queryMsg;

    VerifyOrExit(mState == kState_TransferInProgress, err = CHIP_ERROR_INCORRECT_STATE);
    VerifyOrExit(mRole == kRole_Receiver, err = CHIP_ERROR_INCORRECT_STATE);
    VerifyOrExit(mPendingOutput == kOutput_None, err = CHIP_ERROR_INCORRECT_STATE);
    VerifyOrExit(!mAwaitingResponse, err = CHIP_ERROR_INCORRECT_STATE);

    queryMsg.BlockCounter = mNextQueryNum;

    err = WriteToPacketBuffer<BlockQuery>(queryMsg, mPendingMsgHandle);
    SuccessOrExit(err);

    err = AttachBdxHeader(kBdxMsg_BlockQuery, mPendingMsgHandle);
    SuccessOrExit(err);

    mPendingOutput = kOutput_MsgToSend;

    mAwaitingResponse = true;
    mLastQueryNum     = mNextQueryNum++;

exit:
    return err;
}

CHIP_ERROR TransferSession::PrepareBlock(const BlockData & inData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    DataBlock blockMsg;
    MessageType msgType;

    VerifyOrExit(mState == kState_TransferInProgress, err = CHIP_ERROR_INCORRECT_STATE);
    VerifyOrExit(mRole == kRole_Sender, err = CHIP_ERROR_INCORRECT_STATE);
    VerifyOrExit(mPendingOutput == kOutput_None, err = CHIP_ERROR_INCORRECT_STATE);
    VerifyOrExit(!mAwaitingResponse, err = CHIP_ERROR_INCORRECT_STATE);

    // Verify non-zero data is provided and is no longer than MaxBlockSize (BlockEOF may contain 0 length data)
    VerifyOrExit((inData.Data != nullptr) && (inData.Length <= mTransferMaxBlockSize), err = CHIP_ERROR_INVALID_ARGUMENT);

    blockMsg.BlockCounter = mNextBlockNum;
    blockMsg.Data         = inData.Data;
    blockMsg.DataLength   = inData.Length;

    err = WriteToPacketBuffer<DataBlock>(blockMsg, mPendingMsgHandle);
    SuccessOrExit(err);

    msgType = inData.IsEof ? kBdxMsg_BlockEOF : kBdxMsg_Block;
    err     = AttachBdxHeader(msgType, mPendingMsgHandle);
    SuccessOrExit(err);

    mPendingOutput = kOutput_MsgToSend;

    if (msgType == kBdxMsg_BlockEOF)
    {
        mState = kState_AwaitingEOFAck;
    }

    mAwaitingResponse = true;
    mLastBlockNum     = mNextBlockNum++;

exit:
    return err;
}

CHIP_ERROR TransferSession::PrepareBlockAck()
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    CounterMessage ackMsg;

    VerifyOrExit(mRole == kRole_Receiver, err = CHIP_ERROR_INCORRECT_STATE);
    VerifyOrExit((mState == kState_TransferInProgress) || (mState == kState_ReceivedEOF), err = CHIP_ERROR_INCORRECT_STATE);
    VerifyOrExit(mPendingOutput == kOutput_None, err = CHIP_ERROR_INCORRECT_STATE);

    ackMsg.BlockCounter = mLastBlockNum;

    if (mState == kState_TransferInProgress)
    {
        err = WriteToPacketBuffer<BlockAck>(ackMsg, mPendingMsgHandle);
        SuccessOrExit(err);

        err = AttachBdxHeader(kBdxMsg_BlockAck, mPendingMsgHandle);
        SuccessOrExit(err);

        if (mControlMode == kControl_SenderDrive)
        {
            // In Sender Drive, a BlockAck is implied to also be a query for the next Block, so expect to receive a Block
            // message.
            mLastQueryNum     = ackMsg.BlockCounter + 1;
            mAwaitingResponse = true;
        }
    }
    else if (mState == kState_ReceivedEOF)
    {
        err = WriteToPacketBuffer<BlockAckEOF>(ackMsg, mPendingMsgHandle);
        SuccessOrExit(err);

        err = AttachBdxHeader(kBdxMsg_BlockAckEOF, mPendingMsgHandle);
        SuccessOrExit(err);

        mState            = kState_TransferDone;
        mAwaitingResponse = false;
    }

    mPendingOutput = kOutput_MsgToSend;

exit:
    return err;
}

CHIP_ERROR TransferSession::AbortTransfer(StatusCode reason)
{
    // TODO: prepare a StatusReport
    mState = kState_Error;

    return CHIP_ERROR_NOT_IMPLEMENTED;
}

void TransferSession::Reset()
{
    mPendingOutput = kOutput_None;
    mState         = kState_Idle;
    mSuppportedXferOpts.SetRaw(0);
    mTransferVersion       = 0;
    mMaxSupportedBlockSize = 0;
    mStartOffset           = 0;
    mTransferLength        = 0;
    mTransferMaxBlockSize  = 0;

    mNumBytesProcessed = 0;
    mLastBlockNum      = 0;
    mNextBlockNum      = 0;
    mLastQueryNum      = 0;
    mNextQueryNum      = 0;

    mTimeoutMs          = 0;
    mTimeoutStartTimeMs = 0;
    mAwaitingResponse   = 0;
}

CHIP_ERROR TransferSession::HandleMessageReceived(System::PacketBufferHandle msg, uint64_t curTimeMs)
{
    CHIP_ERROR err      = CHIP_NO_ERROR;
    uint16_t headerSize = 0;
    PayloadHeader payloadHeader;

    VerifyOrExit(!msg.IsNull(), err = CHIP_ERROR_INVALID_ARGUMENT);

    err = payloadHeader.Decode(msg->Start(), msg->DataLength(), &headerSize);
    SuccessOrExit(err);

    msg->ConsumeHead(headerSize);

    // TODO: call HandleStatusReport if message is StatusReport
    if (payloadHeader.GetProtocolID() == Protocols::kProtocol_BDX)
    {
        err = HandleBdxMessage(payloadHeader, std::move(msg));
        SuccessOrExit(err);

        mTimeoutStartTimeMs = curTimeMs;
    }
    else
    {
        err = CHIP_ERROR_INVALID_MESSAGE_TYPE;
    }

exit:
    return err;
}

// Return CHIP_ERROR only if there was a problem decoding the message. Otherwise, use SetTransferError().
CHIP_ERROR TransferSession::HandleBdxMessage(PayloadHeader & header, System::PacketBufferHandle msg)
{
    CHIP_ERROR err      = CHIP_NO_ERROR;
    MessageType msgType = static_cast<MessageType>(header.GetMessageType());

    VerifyOrExit(!msg.IsNull(), err = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(mPendingOutput == kOutput_None, err = CHIP_ERROR_INCORRECT_STATE);

    switch (msgType)
    {
    case kBdxMsg_SendInit:
    case kBdxMsg_ReceiveInit:
        HandleTransferInit(msgType, std::move(msg));
        break;
    case kBdxMsg_SendAccept:
        HandleSendAccept(std::move(msg));
        break;
    case kBdxMsg_ReceiveAccept:
        HandleReceiveAccept(std::move(msg));
        break;
    case kBdxMsg_BlockQuery:
        HandleBlockQuery(std::move(msg));
        break;
    case kBdxMsg_Block:
        HandleBlock(std::move(msg));
        break;
    case kBdxMsg_BlockEOF:
        HandleBlockEOF(std::move(msg));
        break;
    case kBdxMsg_BlockAck:
        HandleBlockAck(std::move(msg));
        break;
    case kBdxMsg_BlockAckEOF:
        HandleBlockAckEOF(std::move(msg));
        break;
    default:
        err = CHIP_ERROR_INVALID_MESSAGE_TYPE;
        break;
    }

exit:
    return err;
}

CHIP_ERROR TransferSession::HandleStatusReportMessage(PayloadHeader & header, System::PacketBufferHandle msg)
{
    mPendingOutput = kOutput_StatusReceived;
    mState         = kState_Error;

    // TODO: parse status report message

    mAwaitingResponse = false;

    return CHIP_ERROR_NOT_IMPLEMENTED;
}

void TransferSession::HandleTransferInit(MessageType msgType, System::PacketBufferHandle msgData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    TransferInit transferInit;

    VerifyOrExit(mState == kState_AwaitingInitMsg, SetTransferError(kStatus_ServerBadState));

    if (mRole == kRole_Sender)
    {
        VerifyOrExit(msgType == kBdxMsg_ReceiveInit, SetTransferError(kStatus_ServerBadState));
    }
    else
    {
        VerifyOrExit(msgType == kBdxMsg_SendInit, SetTransferError(kStatus_ServerBadState));
    }

    err = transferInit.Parse(msgData.Retain());
    VerifyOrExit(err == CHIP_NO_ERROR, SetTransferError(kStatus_BadMessageContents));

    ResolveTransferControlOptions(transferInit.TransferCtlOptions);
    mTransferVersion      = ::chip::min(kBdxVersion, transferInit.Version);
    mTransferMaxBlockSize = ::chip::min(mMaxSupportedBlockSize, transferInit.MaxBlockSize);

    // Accept for now, they may be changed or rejected by the peer if this is a ReceiveInit
    mStartOffset    = transferInit.StartOffset;
    mTransferLength = transferInit.MaxLength;

    // Store the Request data to share with the caller for verification
    mTransferRequestData.TransferCtlFlagsRaw = transferInit.TransferCtlOptions.Raw(),
    mTransferRequestData.MaxBlockSize        = transferInit.MaxBlockSize;
    mTransferRequestData.StartOffset         = transferInit.StartOffset;
    mTransferRequestData.Length              = transferInit.MaxLength;
    mTransferRequestData.FileDesignator      = transferInit.FileDesignator;
    mTransferRequestData.FileDesLength       = transferInit.FileDesLength;
    mTransferRequestData.Metadata            = transferInit.Metadata;
    mTransferRequestData.MetadataLength      = transferInit.MetadataLength;

    mPendingMsgHandle = std::move(msgData);
    mPendingOutput    = kOutput_InitReceived;

    mState = kState_NegotiateTransferParams;

exit:
    return;
}

void TransferSession::HandleReceiveAccept(System::PacketBufferHandle msgData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    ReceiveAccept rcvAcceptMsg;

    VerifyOrExit(mRole == kRole_Receiver, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mState == kState_AwaitingAccept, SetTransferError(kStatus_ServerBadState));

    err = rcvAcceptMsg.Parse(msgData.Retain());
    VerifyOrExit(err == CHIP_NO_ERROR, SetTransferError(kStatus_BadMessageContents));

    // Verify that Accept parameters are compatible with the original proposed parameters
    err = VerifyProposedMode(rcvAcceptMsg.TransferCtlFlags);
    SuccessOrExit(err);

    mTransferMaxBlockSize = rcvAcceptMsg.MaxBlockSize;
    mStartOffset          = rcvAcceptMsg.StartOffset;
    mTransferLength       = rcvAcceptMsg.Length;

    // Note: if VerifyProposedMode() returned with no error, then mControlMode must match the proposed mode in the ReceiveAccept
    // message
    mTransferAcceptData.ControlMode    = mControlMode;
    mTransferAcceptData.MaxBlockSize   = rcvAcceptMsg.MaxBlockSize;
    mTransferAcceptData.StartOffset    = rcvAcceptMsg.StartOffset;
    mTransferAcceptData.Length         = rcvAcceptMsg.Length;
    mTransferAcceptData.Metadata       = rcvAcceptMsg.Metadata;
    mTransferAcceptData.MetadataLength = rcvAcceptMsg.MetadataLength;

    mPendingMsgHandle = std::move(msgData);
    mPendingOutput    = kOutput_AcceptReceived;

    mAwaitingResponse = (mControlMode == kControl_SenderDrive);
    mState            = kState_TransferInProgress;

exit:
    return;
}

void TransferSession::HandleSendAccept(System::PacketBufferHandle msgData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    SendAccept sendAcceptMsg;

    VerifyOrExit(mRole == kRole_Sender, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mState == kState_AwaitingAccept, SetTransferError(kStatus_ServerBadState));

    err = sendAcceptMsg.Parse(msgData.Retain());
    VerifyOrExit(err == CHIP_NO_ERROR, SetTransferError(kStatus_BadMessageContents));

    // Verify that Accept parameters are compatible with the original proposed parameters
    err = VerifyProposedMode(sendAcceptMsg.TransferCtlFlags);
    SuccessOrExit(err);

    // Note: if VerifyProposedMode() returned with no error, then mControlMode must match the proposed mode in the SendAccept
    // message
    mTransferMaxBlockSize = sendAcceptMsg.MaxBlockSize;

    mTransferAcceptData.ControlMode    = mControlMode;
    mTransferAcceptData.MaxBlockSize   = sendAcceptMsg.MaxBlockSize;
    mTransferAcceptData.StartOffset    = mStartOffset;    // Not included in SendAccept msg, so use member
    mTransferAcceptData.Length         = mTransferLength; // Not included in SendAccept msg, so use member
    mTransferAcceptData.Metadata       = sendAcceptMsg.Metadata;
    mTransferAcceptData.MetadataLength = sendAcceptMsg.MetadataLength;

    mPendingMsgHandle = std::move(msgData);
    mPendingOutput    = kOutput_AcceptReceived;

    mAwaitingResponse = (mControlMode == kControl_ReceiverDrive);
    mState            = kState_TransferInProgress;

exit:
    return;
}

void TransferSession::HandleBlockQuery(System::PacketBufferHandle msgData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    BlockQuery query;

    VerifyOrExit(mRole == kRole_Sender, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mState == kState_TransferInProgress, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mAwaitingResponse, SetTransferError(kStatus_ServerBadState));

    err = query.Parse(std::move(msgData));
    VerifyOrExit(err == CHIP_NO_ERROR, SetTransferError(kStatus_BadMessageContents));

    VerifyOrExit(query.BlockCounter == mNextBlockNum, SetTransferError(kStatus_BadBlockCounter));

    mPendingOutput = kOutput_QueryReceived;

    mAwaitingResponse = false;
    mLastQueryNum     = query.BlockCounter;

exit:
    return;
}

void TransferSession::HandleBlock(System::PacketBufferHandle msgData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    Block blockMsg;

    VerifyOrExit(mRole == kRole_Receiver, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mState == kState_TransferInProgress, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mAwaitingResponse, SetTransferError(kStatus_ServerBadState));

    err = blockMsg.Parse(msgData.Retain());
    VerifyOrExit(err == CHIP_NO_ERROR, SetTransferError(kStatus_BadMessageContents));

    VerifyOrExit(blockMsg.BlockCounter == mLastQueryNum, SetTransferError(kStatus_BadBlockCounter));
    VerifyOrExit((blockMsg.DataLength > 0) && (blockMsg.DataLength <= mTransferMaxBlockSize),
                 SetTransferError(kStatus_BadMessageContents));

    if (IsTransferLengthDefinite())
    {
        VerifyOrExit(mNumBytesProcessed + blockMsg.DataLength <= mTransferLength, SetTransferError(kStatus_LengthMismatch));
    }

    mBlockEventData.Data   = blockMsg.Data;
    mBlockEventData.Length = blockMsg.DataLength;
    mBlockEventData.IsEof  = false;

    mPendingMsgHandle = std::move(msgData);
    mPendingOutput    = kOutput_BlockReceived;

    mNumBytesProcessed += blockMsg.DataLength;
    mLastBlockNum = blockMsg.BlockCounter;

    mAwaitingResponse = false;

exit:
    return;
}

void TransferSession::HandleBlockEOF(System::PacketBufferHandle msgData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    BlockEOF blockEOFMsg;

    VerifyOrExit(mRole == kRole_Receiver, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mState == kState_TransferInProgress, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mAwaitingResponse, SetTransferError(kStatus_ServerBadState));

    err = blockEOFMsg.Parse(msgData.Retain());
    VerifyOrExit(err == CHIP_NO_ERROR, SetTransferError(kStatus_BadMessageContents));

    VerifyOrExit(blockEOFMsg.BlockCounter == mLastQueryNum, SetTransferError(kStatus_BadBlockCounter));
    VerifyOrExit(blockEOFMsg.DataLength <= mTransferMaxBlockSize, SetTransferError(kStatus_BadMessageContents));

    mBlockEventData.Data   = blockEOFMsg.Data;
    mBlockEventData.Length = blockEOFMsg.DataLength;
    mBlockEventData.IsEof  = true;

    mPendingMsgHandle = std::move(msgData);
    mPendingOutput    = kOutput_BlockReceived;

    mNumBytesProcessed += blockEOFMsg.DataLength;
    mLastBlockNum = blockEOFMsg.BlockCounter;

    mAwaitingResponse = false;
    mState            = kState_ReceivedEOF;

exit:
    return;
}

void TransferSession::HandleBlockAck(System::PacketBufferHandle msgData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    BlockAck ackMsg;

    VerifyOrExit(mRole == kRole_Sender, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mState == kState_TransferInProgress, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mAwaitingResponse, SetTransferError(kStatus_ServerBadState));

    err = ackMsg.Parse(std::move(msgData));
    VerifyOrExit(err == CHIP_NO_ERROR, SetTransferError(kStatus_BadMessageContents));
    VerifyOrExit(ackMsg.BlockCounter == mLastBlockNum, SetTransferError(kStatus_BadBlockCounter));

    mPendingOutput = kOutput_AckReceived;

    // In Receiver Drive, the Receiver can send a BlockAck to indicate receipt of the message and reset the timeout.
    // In this case, the Sender should wait to receive a BlockQuery next.
    mAwaitingResponse = (mControlMode == kControl_ReceiverDrive);

exit:
    return;
}

void TransferSession::HandleBlockAckEOF(System::PacketBufferHandle msgData)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    BlockAckEOF ackMsg;

    VerifyOrExit(mRole == kRole_Sender, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mState == kState_AwaitingEOFAck, SetTransferError(kStatus_ServerBadState));
    VerifyOrExit(mAwaitingResponse, SetTransferError(kStatus_ServerBadState));

    err = ackMsg.Parse(std::move(msgData));
    VerifyOrExit(err == CHIP_NO_ERROR, SetTransferError(kStatus_BadMessageContents));
    VerifyOrExit(ackMsg.BlockCounter == mLastBlockNum, SetTransferError(kStatus_BadBlockCounter));

    mPendingOutput = kOutput_AckEOFReceived;

    mAwaitingResponse = false;

    mState = kState_TransferDone;

exit:
    return;
}

CHIP_ERROR TransferSession::AttachBdxHeader(MessageType msgType, System::PacketBufferHandle & msgBuf)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    PayloadHeader payloadHeader;

    payloadHeader.SetMessageType(Protocols::kProtocol_BDX, static_cast<uint8_t>(msgType));

    uint16_t headerSize              = payloadHeader.EncodeSizeBytes();
    uint16_t actualEncodedHeaderSize = 0;

    VerifyOrExit(msgBuf->EnsureReservedSize(headerSize), err = CHIP_ERROR_NO_MEMORY);

    msgBuf->SetStart(msgBuf->Start() - headerSize);
    err = payloadHeader.Encode(msgBuf->Start(), msgBuf->DataLength(), &actualEncodedHeaderSize);
    SuccessOrExit(err);
    VerifyOrExit(headerSize == actualEncodedHeaderSize, err = CHIP_ERROR_INTERNAL);

exit:
    return err;
}

void TransferSession::ResolveTransferControlOptions(const BitFlags<uint8_t, TransferControlFlags> & proposed)
{
    // Must specify at least one synchronous option
    if (!proposed.Has(kControl_SenderDrive) && !proposed.Has(kControl_ReceiverDrive))
    {
        SetTransferError(kStatus_TransferMethodNotSupported);
        return;
    }

    // Ensure there are options supported by both nodes. Async gets priority.
    // If there is only one common option, choose that one. Otherwise the application must pick.
    BitFlags<uint8_t, TransferControlFlags> commonOpts;
    commonOpts.SetRaw(proposed.Raw() & mSuppportedXferOpts.Raw());
    if (commonOpts.Raw() == 0)
    {
        SetTransferError(kStatus_TransferMethodNotSupported);
    }
    else if (commonOpts.HasOnly(kControl_Async))
    {
        mControlMode = kControl_Async;
    }
    else if (commonOpts.HasOnly(kControl_ReceiverDrive))
    {
        mControlMode = kControl_ReceiverDrive;
    }
    else if (commonOpts.HasOnly(kControl_SenderDrive))
    {
        mControlMode = kControl_SenderDrive;
    }
}

CHIP_ERROR TransferSession::VerifyProposedMode(const BitFlags<uint8_t, TransferControlFlags> & proposed)
{
    TransferControlFlags mode;

    // Must specify only one mode in Accept messages
    if (proposed.HasOnly(kControl_Async))
    {
        mode = kControl_Async;
    }
    else if (proposed.HasOnly(kControl_ReceiverDrive))
    {
        mode = kControl_ReceiverDrive;
    }
    else if (proposed.HasOnly(kControl_SenderDrive))
    {
        mode = kControl_SenderDrive;
    }
    else
    {
        SetTransferError(kStatus_BadMessageContents);
        return CHIP_ERROR_INTERNAL;
    }

    // Verify the proposed mode is supported by this instance
    if (mSuppportedXferOpts.Has(mode))
    {
        mControlMode = mode;
    }
    else
    {
        SetTransferError(kStatus_TransferMethodNotSupported);
        return CHIP_ERROR_INTERNAL;
    }

    return CHIP_NO_ERROR;
}

void TransferSession::SetTransferError(StatusCode code)
{
    mStatusReportData.error = code;
    mPendingOutput          = kOutput_InternalError;
    mState                  = kState_Error;
}

bool TransferSession::IsTransferLengthDefinite()
{
    return (mTransferLength > 0);
}

TransferSession::OutputEvent TransferSession::OutputEvent::TransferInitEvent(TransferInitData data, System::PacketBufferHandle msg)
{
    OutputEvent event(kOutput_InitReceived);
    event.MsgData          = std::move(msg);
    event.transferInitData = data;
    return event;
}

/**
 * @brief
 *   Convenience method for constructing an OutputEvent with TransferAcceptData that does not contain Metadata
 */
TransferSession::OutputEvent TransferSession::OutputEvent::TransferAcceptEvent(TransferAcceptData data)
{
    OutputEvent event(kOutput_AcceptReceived);
    event.transferAcceptData = data;
    return event;
}
/**
 * @brief
 *   Convenience method for constructing an OutputEvent with TransferAcceptData that contains Metadata
 */
TransferSession::OutputEvent TransferSession::OutputEvent::TransferAcceptEvent(TransferAcceptData data,
                                                                               System::PacketBufferHandle msg)
{
    OutputEvent event = TransferAcceptEvent(data);
    event.MsgData     = std::move(msg);
    return event;
}

TransferSession::OutputEvent TransferSession::OutputEvent::BlockDataEvent(BlockData data, System::PacketBufferHandle msg)
{
    OutputEvent event(kOutput_BlockReceived);
    event.MsgData   = std::move(msg);
    event.blockdata = data;
    return event;
}

/**
 * @brief
 *   Convenience method for constructing an event with kOutput_InternalError or kOutputStatusReceived
 */
TransferSession::OutputEvent TransferSession::OutputEvent::StatusReportEvent(OutputEventType type, StatusReportData data)
{
    OutputEvent event(type);
    event.statusData = data;
    return event;
}

} // namespace bdx
} // namespace chip
