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
 * @file PBFTCache.cpp
 * @author: yujiechen
 * @date 2021-04-23
 */
#include "PBFTCache.h"

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::protocol;
using namespace bcos::crypto;

PBFTCache::PBFTCache(PBFTConfig::Ptr _config, BlockNumber _index)
  : m_config(_config), m_index(_index)
{}

void PBFTCache::addCache(CollectionCacheType& _cachedReq, QuorumRecoderType& _weightInfo,
    PBFTMessageInterface::Ptr _pbftCache)

{
    if (_pbftCache->index() != m_index)
    {
        return;
    }
    auto const& proposalHash = _pbftCache->hash();
    auto generatedFrom = _pbftCache->generatedFrom();
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
    _cachedReq[proposalHash][generatedFrom] = _pbftCache;
}

void PBFTCache::setSignatureList()
{
    for (auto const& it : m_commitCacheList[m_precommit->hash()])
    {
        m_precommit->consensusProposal()->appendSignatureProof(
            it.first, it.second->consensusProposal()->signature());
    }
}

bool PBFTCache::conflictWithPrecommitReq(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    if (!m_precommit)
    {
        return false;
    }
    if (m_precommit->index() < m_config->progressedIndex())
    {
        return false;
    }
    if (_prePrepareMsg->index() == m_precommit->index() &&
        _prePrepareMsg->hash() != m_precommit->hash())
    {
        PBFT_LOG(INFO) << LOG_DESC(
                              "the received pre-prepare msg is conflict with the preparedCache")
                       << printPBFTMsgInfo(_prePrepareMsg);
        return true;
    }
    return false;
}

PBFTMessageInterface::Ptr PBFTCache::checkAndPreCommit()
{
    if (!collectEnoughPrepareReq())
    {
        return nullptr;
    }
    // update and backup the proposal into precommit-status
    intoPrecommit();
    m_precommit = m_prePrepare;
    m_precommit->setGeneratedFrom(m_config->nodeIndex());

    m_precommitWithoutData = m_config->pbftMessageFactory()->populateFrom(m_precommit);
    auto precommitProposalWithoutData =
        m_config->pbftMessageFactory()->populateFrom(m_precommit->consensusProposal(), false);
    m_precommitWithoutData->setConsensusProposal(precommitProposalWithoutData);

    // generate the commitReq
    auto commitReq = m_config->pbftMessageFactory()->populateFrom(PacketType::CommitPacket,
        m_config->pbftMsgDefaultVersion(), m_config->view(), utcTime(), m_config->nodeIndex(),
        m_precommitWithoutData->consensusProposal(), m_config->cryptoSuite(), m_config->keyPair());
    // add the commitReq to local cache
    addCommitCache(commitReq);
    // broadcast the commitReq
    auto encodedData = m_config->codec()->encode(commitReq, m_config->pbftMsgDefaultVersion());
    m_config->frontService()->asyncSendMessageByNodeIDs(
        bcos::protocol::ModuleID::PBFT, m_config->consensusNodeIDList(), ref(*encodedData));
    // collect the commitReq and try to commit
    return checkAndCommit();
}

PBFTMessageInterface::Ptr PBFTCache::checkAndCommit()
{
    if (!collectEnoughCommitReq())
    {
        return nullptr;
    }
    setSignatureList();
    return m_precommit;
}