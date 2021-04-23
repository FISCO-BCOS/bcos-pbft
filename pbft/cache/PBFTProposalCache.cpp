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
 * @brief cache for the consensus state of the proposal
 * @file PBFTProposalCache.cpp
 * @author: yujiechen
 * @date 2021-04-23
 */
#include "PBFTProposalCache.h"

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::protocol;
using namespace bcos::crypto;

PBFTProposalCache::PBFTProposalCache(PBFTConfig::Ptr _config, BlockNumber _index)
  : m_config(_config), m_index(_index)
{}

bool PBFTProposalCache::conflictWithPrecommitProposal(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    // empty or older prepared cache
    if (!m_precommitProposal)
    {
        return false;
    }
    if (m_precommitProposal->index() < m_config->progressedIndex())
    {
        return false;
    }
    if (_prePrepareMsg->index() == m_precommitProposal->index() &&
        _prePrepareMsg->hash() != m_precommitProposal->hash())
    {
        PBFT_LOG(INFO) << LOG_DESC(
                              "the received pre-prepare msg is conflict with the preparedCache")
                       << printPBFTMsgInfo(_prePrepareMsg);
        return true;
    }
    return false;
}

void PBFTProposalCache::addCache(CollectionCacheType& _cachedReq, QuorumRecoderType& _weightInfo,
    PBFTProposalInterface::Ptr _proposal)

{
    if (_proposal->index() != m_index)
    {
        return;
    }
    auto const& proposalHash = _proposal->hash();
    auto generatedFrom = _proposal->generatedFrom();
    if (_cachedReq.count(proposalHash) && _cachedReq[proposalHash].count(generatedFrom))
    {
        return;
    }
    auto nodeInfo = m_config->getConsensusNodeByIndex(generatedFrom);
    if (!nodeInfo)
    {
        return;
    }
    if (!_weightInfo.count(proposalHash))
    {
        _weightInfo[proposalHash] = 0;
    }
    else
    {
        _weightInfo[proposalHash] += nodeInfo->weight();
    }
    _cachedReq[proposalHash][generatedFrom] = _proposal;
}
