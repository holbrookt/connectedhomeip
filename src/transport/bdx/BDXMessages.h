/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *
 *
 */

#pragma once

#include <protocols/common/CHIPMessage.h>
#include <support/CodeUtils.h>

namespace chip {
namespace BDX {

enum BDXMsgType : uint16_t
{
    kSendInit      = 0x01,
    kSendAccept    = 0x02,
    kReceiveInit   = 0x04,
    kReceiveAccept = 0x05,
    kBlockQuery    = 0x10,
    kBlock         = 0x11,
    kBlockEOF      = 0x12,
    kBlockAck      = 0x13,
    kBlockAckEOF   = 0x14,
};

enum BDXError : uint32_t
{
    kOverflow                   = 0x0011,
    kLengthTooLarge             = 0x0012,
    kLengthTooShort             = 0x0013,
    kLengthMismatch             = 0x0014,
    kLengthRequired             = 0x0015,
    kBadMessageContents         = 0x0016,
    kBadBlockCounter            = 0x0017,
    kTransferFailedUnknownError = 0x001F,
    kServerBadState             = 0x0020,
    kFailureToSend              = 0x0021,
    kTransferMethodNotSupported = 0x0050,
    kFileDesignatorUnknown      = 0x0051,
    kStartOffsetNotSupported    = 0x0052,
    kVersionNotSupported        = 0x0053,
    kUnknown                    = 0x005F,
};

/**
 * @class SendInit
 *
 * @brief
 *   The SendInit message is used to start an exchange when the sender
 *   is the initiator.
 */
class SendInit
{
public:
    SendInit();

    uint8_t test;
    /*
        WEAVE_ERROR init(uint8_t aVersion,
                         bool aSenderDrive,
                         bool aReceiverDrive,
                         bool aAsynchMode,
                         uint16_t aMaxBlockSize,
                         uint64_t aStartOffset,
                         uint64_t aLength,
                         ReferencedString &aFileDesignator,
                         ReferencedTLVData *aMetaData);

        WEAVE_ERROR init(uint8_t aVersion,
                         bool aSenderDrive,
                         bool aReceiverDrive,
                         bool aAsynchMode,
                         uint16_t aMaxBlockSize,
                         uint32_t aStartOffset,
                         uint32_t aLength,
                         ReferencedString &aFileDesignator,
                         ReferencedTLVData *aMetaData);
    */
};

/**
 * @class ReceiveInit
 *
 * @brief
 *   The ReceiveInit message is used to start an exchange when the receiver
 *   is the initiator.
 */
class DLL_EXPORT ReceiveInit
{
public:
    ReceiveInit(void);

    CHIP_ERROR init(uint8_t aVersion, bool aSenderDrive, bool aReceiverDrive, bool aAsynchMode, uint16_t aMaxBlockSize,
                    uint64_t aStartOffset, uint64_t aLength, ReferencedString & aFileDesignator, ReferencedTLVData * aMetaData);

    CHIP_ERROR init(uint8_t aVersion, bool aSenderDrive, bool aReceiverDrive, bool aAsynchMode, uint16_t aMaxBlockSize,
                    uint32_t aStartOffset, uint32_t aLength, ReferencedString & aFileDesignator, ReferencedTLVData * aMetaData);

    /**
     * @brief
     *
     *   MetaDataTLVWriteCallback provides a means by which a client
     *   can supply a SendInit with any metadata they want.  The
     *   client is free to supply pre-encoded TLV (faster), encode
     *   on-the-fly (uses less memory), lazy-encode (a little faster on
     *   startup), etc. as they see fit.
     *
     *   In all cases, it is assumed that the data produced by the
     *   callback is constant for a given SendInit, i.e. does not
     *   change no matter when it is called.  This is because the
     *   callback is also used to compute the length of any such
     *   written-out TLV, which could be requested at any time.
     *
     * @param[in]    aBuffer           The destination buffer, into which some TLV can be written
     * @param[in]    aBufferLength     The length (in bytes) of the destination buffer
     * @param[inout] aNumBytesWritten  The the number of bytes written to the destination buffer
     * @param[in]    aAppState         User-provided app state
     *
     * @retval       #CHIP_ERROR      Any error encountered.
     */
    typedef CHIP_ERROR (*MetaDataTLVWriteCallback)(uint8_t * aBuffer, uint16_t aBufferLength, uint16_t & aNumBytesWritten,
                                                   void * aAppState);

    CHIP_ERROR init(uint8_t aVersion, bool aSenderDrive, bool aReceiverDrive, bool aAsynchMode, uint16_t aMaxBlockSize,
                    uint64_t aStartOffset, uint64_t aLength, ReferencedString & aFileDesignator,
                    MetaDataTLVWriteCallback aMetaDataWriteCallback, void * aMetaDataAppState);

    CHIP_ERROR init(uint8_t aVersion, bool aSenderDrive, bool aReceiverDrive, bool aAsynchMode, uint16_t aMaxBlockSize,
                    uint32_t aStartOffset, uint32_t aLength, ReferencedString & aFileDesignator,
                    MetaDataTLVWriteCallback aMetaDataWriteCallback, void * aMetaDataAppState);

    CHIP_ERROR pack(System::PacketBuffer * aBuffer);
    uint16_t packedLength(void);
    static CHIP_ERROR parse(System::PacketBuffer * aBuffer, SendInit & aRequest);

private:
    uint16_t GetWrittenMetaDataCallbackLength(void);

public:
    bool operator==(const SendInit &) const;

    uint8_t mVersion; /**< Version of the BDX protocol we decided on. */
    // Transfer mode options
    bool mSenderDriveSupported;      /**< True if we can support sender drive. */
    bool mReceiverDriveSupported;    /**< True if we can support receiver drive. */
    bool mAsynchronousModeSupported; /**< True if we can support async mode. */
    // Range control options
    bool mDefiniteLength;     /**< True if the length field is present. */
    bool mStartOffsetPresent; /**< True if the start offset field is present. */
    bool mWideRange;          /**< True if offset and length are 64 bits. */
    // Block size and offset
    uint16_t mMaxBlockSize; /**< Proposed max block size to use in transfer. */
    uint64_t mStartOffset;  /**< Proposed start offset of data. */
    uint64_t mLength;       /**< Proposed length of data in transfer, 0 for indefinite. */
    // File designator
    ReferencedString mFileDesignator; /**< String containing pre-negotiated information. */
    // Additional metadata
    ReferencedTLVData mMetaData;                     /**< Optional TLV Metadata. */
    MetaDataTLVWriteCallback mMetaDataWriteCallback; /**< Optional function to write out TLV Metadata. */
    void * mMetaDataAppState;                        /**< Optional app state for TLV Metadata. */
};

} // namespace BDX
} // namespace chip
