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
 * @brief implementation for PBFTMessageFactory
 * @file PBFTMessageFactoryImpl.h
 * @author: yujiechen
 * @date 2021-04-20
 */
#pragma once
#include "pbft/interfaces/PBFTMessageFactory.h"
#include "pbft/protocol/PB/PBFTMessage.h"
#include "pbft/protocol/PB/PBFTNewViewMsg.h"
#include "pbft/protocol/PB/PBFTProposal.h"
#include "pbft/protocol/PB/PBFTRequest.h"
#include "pbft/protocol/PB/PBFTViewChangeMsg.h"

namespace bcos
{
namespace consensus
{
class PBFTMessageFactoryImpl : public PBFTMessageFactory
{
public:
    using Ptr = std::shared_ptr<PBFTMessageFactoryImpl>;
    PBFTMessageFactoryImpl() = default;
    ~PBFTMessageFactoryImpl() override {}

    PBFTMessageInterface::Ptr createPBFTMsg() override { return std::make_shared<PBFTMessage>(); }

    ViewChangeMsgInterface::Ptr createViewChangeMsg() override
    {
        return std::make_shared<PBFTViewChangeMsg>();
    }

    NewViewMsgInterface::Ptr createNewViewMsg() override
    {
        return std::make_shared<PBFTNewViewMsg>();
    }

    PBFTMessageInterface::Ptr createPBFTMsg(
        bcos::crypto::CryptoSuite::Ptr _cryptoSuite, bytesConstRef _data) override
    {
        return std::make_shared<PBFTMessage>(_cryptoSuite, _data);
    }

    ViewChangeMsgInterface::Ptr createViewChangeMsg(bytesConstRef _data) override
    {
        return std::make_shared<PBFTViewChangeMsg>(_data);
    }

    NewViewMsgInterface::Ptr createNewViewMsg(bytesConstRef _data) override
    {
        return std::make_shared<PBFTNewViewMsg>(_data);
    }

    PBFTProposalInterface::Ptr createPBFTProposal() override
    {
        return std::make_shared<PBFTProposal>();
    }

    PBFTProposalInterface::Ptr createPBFTProposal(bytesConstRef _data) override
    {
        return std::make_shared<PBFTProposal>(_data);
    }

    PBFTRequestInterface::Ptr createPBFTRequest() override
    {
        return std::make_shared<PBFTRequest>();
    }

    PBFTRequestInterface::Ptr createPBFTRequest(bytesConstRef _data) override
    {
        return std::make_shared<PBFTRequest>(_data);
    }
};
}  // namespace consensus
}  // namespace bcos