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
 * @brief implementation for ConsensusInterface
 * @file PBFT.h
 * @author: yujiechen
 * @date 2021-05-17
 */
#pragma once
#include "pbft/engine/PBFTEngine.h"
#include <bcos-framework/interfaces/consensus/ConsensusInterface.h>
#include <bcos-framework/interfaces/protocol/CommonError.h>
namespace bcos
{
namespace consensus
{
class PBFT : public ConsensusInterface
{
public:
    using Ptr = std::shared_ptr<PBFT>;
    explicit PBFT(PBFTEngine::Ptr _pbftEngine) : m_pbftEngine(_pbftEngine) {}
    virtual ~PBFT() { stop(); }

    void start() override { m_pbftEngine->start(); }
    void stop() override { m_pbftEngine->stop(); }

    void asyncSubmitProposal(bytesConstRef _proposalData,
        bcos::protocol::BlockNumber _proposalIndex, bcos::crypto::HashType const& _proposalHash,
        std::function<void(Error::Ptr)> _onProposalSubmitted) override
    {
        return m_pbftEngine->asyncSubmitProposal(
            _proposalData, _proposalIndex, _proposalHash, _onProposalSubmitted);
    }

    void asyncGetPBFTView(std::function<void(Error::Ptr, ViewType)> _onGetView) override
    {
        auto view = m_pbftEngine->pbftConfig()->view();
        _onGetView(std::make_shared<Error>(bcos::protocol::CommonError::SUCCESS, "success"), view);
    }

    void asyncNotifyConsensusMessage(bcos::Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID,
        bytesConstRef _data, std::function<void(bytesConstRef _respData)> _sendResponse,
        std::function<void(Error::Ptr _error)> _onRecv) override
    {
        m_pbftEngine->onReceivePBFTMessage(_error, _nodeID, _data, _sendResponse);
        _onRecv(std::make_shared<Error>(bcos::protocol::CommonError::SUCCESS, "success"));
    }

private:
    PBFTEngine::Ptr m_pbftEngine;
};
}  // namespace consensus
}  // namespace bcos