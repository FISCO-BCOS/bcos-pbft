/**
 *  Copyright (C) 2021 FISCO BCOS.
 *  SPDX-License-Identifier: Apache-2.0
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * @brief interface for PBFTCodec
 * @file PBFTCodecInterface.h
 * @author: yujiechen
 * @date 2021-04-13
 */
#pragma once
#include "pbft/protocol/interfaces/PBFTBaseMessageInterface.h"
#include <bcos-framework/interfaces/crypto/KeyInterface.h>
#include <bcos-framework/libutilities/Common.h>
namespace bcos
{
namespace consensus
{
class PBFTCodecInterface
{
public:
    using Ptr = std::shared_ptr<PBFTCodecInterface>;
    PBFTCodecInterface() = default;
    virtual ~PBFTCodecInterface() {}

    virtual bytesPointer encode(
        PBFTBaseMessageInterface::Ptr _pbftMessage, int32_t _version = 0) const = 0;
    virtual PBFTBaseMessageInterface::Ptr decode(
        bcos::crypto::PublicPtr _pubKey, bytesConstRef _data) const = 0;
};
}  // namespace consensus
}  // namespace bcos