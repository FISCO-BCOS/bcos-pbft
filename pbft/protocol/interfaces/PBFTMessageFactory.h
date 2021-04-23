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
 * @brief factory for PBFTMessage
 * @file PBFTMessageFactory.h
 * @author: yujiechen
 * @date 2021-04-20
 */
#pragma once
#include "pbft/protocol/interfaces/NewViewMsgInterface.h"
#include "pbft/protocol/interfaces/PBFTMessageInterface.h"
#include "pbft/protocol/interfaces/PBFTProposalInterface.h"
#include "pbft/protocol/interfaces/ViewChangeMsgInterface.h"
#include <bcos-framework/interfaces/crypto/CryptoSuite.h>
namespace bcos
{
namespace consensus
{
class PBFTMessageFactory
{
public:
    using Ptr = std::shared_ptr<PBFTMessageFactory>;
    PBFTMessageFactory() = default;
    virtual ~PBFTMessageFactory() {}

    virtual PBFTMessageInterface::Ptr createPBFTMsg() = 0;
    virtual PBFTMessageInterface::Ptr createPBFTMsg(
        bcos::crypto::CryptoSuite::Ptr _cryptoSuite, bytesConstRef _data) = 0;

    virtual ViewChangeMsgInterface::Ptr createViewChangeMsg() = 0;
    virtual ViewChangeMsgInterface::Ptr createViewChangeMsg(bytesConstRef _data) = 0;

    virtual NewViewMsgInterface::Ptr createNewViewMsg() = 0;
    virtual NewViewMsgInterface::Ptr createNewViewMsg(bytesConstRef _data) = 0;
    virtual PBFTProposalInterface::Ptr createPBFTProposal() = 0;
};
}  // namespace consensus
}  // namespace bcos
