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

bool PBFTCache::existPrePrepare(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    if (!m_prePrepare)
    {
        return false;
    }
    return (_prePrepareMsg->hash() == m_prePrepare->hash()) &&
           (m_prePrepare->view() >= _prePrepareMsg->view());
}

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

bool PBFTCache::conflictWithProcessedReq(PBFTMessageInterface::Ptr _msg)
{
    if (!m_prePrepare)
    {
        return false;
    }
    return (_msg->hash() != m_prePrepare->hash() || _msg->view() < m_prePrepare->view());
}

bool PBFTCache::checkPrePrepareProposalStatus()
{
    if (m_prePrepare == nullptr)
    {
        return false;
    }
    if (m_prePrepare->view() != m_config->view())
    {
        return false;
    }
    return true;
}

bool PBFTCache::collectEnoughQuorum(
    bcos::crypto::HashType const& _hash, QuorumRecoderType& _weightInfo)
{
    if (!_weightInfo.count(_hash))
    {
        return false;
    }
    return (_weightInfo[_hash] >= m_config->minRequiredQuorum());
}

bool PBFTCache::collectEnoughPrepareReq()
{
    if (!checkPrePrepareProposalStatus())
    {
        return false;
    }
    return collectEnoughQuorum(m_prePrepare->hash(), m_prepareReqWeight);
}

bool PBFTCache::collectEnoughCommitReq()
{
    if (!checkPrePrepareProposalStatus())
    {
        return false;
    }
    return collectEnoughQuorum(m_prePrepare->hash(), m_commitReqWeight);
}

void PBFTCache::intoPrecommit()
{
    m_precommit->setGeneratedFrom(m_config->nodeIndex());
    m_precommit = m_prePrepare;
    m_config->storage()->storePrecommitProposal(m_precommit->consensusProposal());
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

bool PBFTCache::checkAndPreCommit()
{
    // already precommitted
    if (m_precommit != nullptr)
    {
        return false;
    }
    if (!collectEnoughPrepareReq())
    {
        return false;
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

bool PBFTCache::checkAndCommit()
{
    if (m_submitting == true)
    {
        return false;
    }
    if (!collectEnoughCommitReq())
    {
        return false;
    }
    setSignatureList();
    // commit the proposal
    m_config->storage()->asyncCommitProposal(m_precommit->consensusProposal());
    PBFT_LOG(DEBUG) << LOG_DESC("checkAndCommit: commitProposal")
                    << printPBFTProposal(m_precommit->consensusProposal());
    m_submitting.store(true);
    return true;
}


void PBFTCache::resetCache(ViewType _curView)
{
    m_submitting = false;
    // reset pre-prepare
    if (m_prePrepare->view() < _curView)
    {
        m_prePrepare = nullptr;
    }
    // reset precommit
    if (m_precommit->view() < _curView)
    {
        m_precommit = nullptr;
        m_precommitWithoutData = nullptr;
    }
    // clear the expired prepare cache
    resetCacheAfterViewChange(m_prepareCacheList, _curView);
    // clear the expired commit cache
    resetCacheAfterViewChange(m_commitCacheList, _curView);

    // recalculate m_prepareReqWeight
    recalculateQuorum(m_prepareReqWeight, m_prepareCacheList);
    // recalculate m_commitReqWeight
    recalculateQuorum(m_commitReqWeight, m_commitCacheList);
}