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

#ifndef __BDXMESSAGES_H__
#define __BDXMESSAGES_H__

/*
#include <Weave/Core/WeaveCore.h>
#include <Weave/Core/WeaveMessageLayer.h>
#include <Weave/Profiles/ProfileCommon.h>
#include <Weave/Profiles/bulk-data-transfer/Development/BDXConstants.h>
#include <Weave/Profiles/bulk-data-transfer/Development/BDXManagedNamespace.hpp>
#include <Weave/Support/NLDLLUtil.h>
*/

/**
 *   @namespace nl::Weave::Profiles::WeaveMakeManagedNamespaceIdentifier(BDX, kWeaveManagedNamespaceDesignation_Development) {
 *
 *   @brief
 *     This namespace includes all interfaces within Weave for the
 *     Weave Bulk Data Transfer (BDX) profile.
 */

#include <core/CHIPCore.h>

namespace chip {
namespace BDX {

enum MessageTypes : uint16_t
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
    kStatusReport  = 0xff, // TODO: temporary
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

} // namespace BDX
} // namespace chip

#endif // __BDXMESSAGES_H__
