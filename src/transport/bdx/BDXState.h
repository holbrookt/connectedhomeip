#ifndef BDX_STATE_H
#define BDX_STATE_H

#include <inttypes.h>

#include <core/CHIPError.h>
#include <system/SystemPacketBuffer.h>
#include <transport/bdx/BDXMessages.h>

namespace chip {
namespace BDX {

enum TransferStates : uint8_t
{
    kIdle               = 0,
    kNegotiateReceive   = 1,
    kNegotiateSend      = 2,
    kTransferInProgress = 3,
    kFinalizeTransfer   = 4,
    kError              = 4,
};

enum ControlMode : uint8_t
{
    kReceiverDrive = 0,
    kSenderDrive   = 1,
    kAsync         = 2,
};

enum TransferRole : uint8_t
{
    kReceiver     = 0,
    kSender       = 1,
    kNotSpecified = 2,
};

struct TransferParams
{
    TransferRole Role;
    // bool SupportsAsync;
    // uint8_t SupportedVersions;
    bool SupportsSenderDrive; // TODO maybe better off as a bitfield
    bool SupportsReceiverDrive;
    // uint16_t MaxBlockSize;
    // bool DefLen;
    // uint64_t TransferLength;
};

typedef CHIP_ERROR (*SendMessageCallback)(uint16_t msgType, System::PacketBuffer * msgData);

class DLL_EXPORT BDXState
{
public:
    BDXState(TransferParams transferParams, SendMessageCallback sendMessageCallback); // unique_ptr ?
    CHIP_ERROR HandleMessageReceived(uint16_t msgId, System::PacketBuffer * msgData);
    void HandleReceiveInit(System::PacketBuffer * msgData);
    void HandleMessageError(CHIP_ERROR error);

    // TODO: define callback for sending messages
private:
    bool AreParametersAcceptable();
    void EndTransfer(CHIP_ERROR error);

    TransferParams mSupportedOptions;
    TransferStates mState;

    SendMessageCallback SendMessage;

    // per-transfer variables
    ControlMode mControlMode;
    uint16_t mMaxBlockSize;
};

} // namespace BDX
} // namespace chip

#endif /* BDX_STATE_H */
