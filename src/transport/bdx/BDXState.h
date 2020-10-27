#ifndef BDX_STATE_H
#define BDX_STATE_H

namespace chip {
namespace BDX {

enum TransferStates : uint8_t
{
    kIdle = 0,
    kNegotiateReceive = 1,
    kNegotiateSend = 2,
    kTransferInProgress = 3,
    kError = 4,
}

enum ControlMode : uint8_t
{
    kReceiverDrive = 0,
    kSenderDrive = 1,
    kAsync = 2,
}

struct TransferParams
{
    bool SupportsAsync;
    //uint8_t SupportedVersions;
    bool SupportsSenderDrive;
    bool SupportsReceiverDrive;
    uint16_t MaxBlockSize;
    bool DefLen;
    uint64_t TransferLength;
}

class DLL_EXPORT BDXState
{
public:
    BDXState(TransferParams transferParams);  // unique_ptr ?
    CHIP_ERROR HandleMessageReceived(uint8_t msgId, System::PacketBuffer *msgData);
    void HandleMessageError(CHIP_ERROR error);

private:
    TransferParams mSupportedOptions;
    TransferStates mState;

    // per-transfer variables
    ControlMode mControlMode;
    uint16_t mMaxBlockSize;
}

}  // namespace BDX
}  // namespace chip

#endif  /* BDX_STATE_H */
