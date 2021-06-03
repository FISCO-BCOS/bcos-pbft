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
{
    // Timer is used to manage checkpoint timeout
    m_timer = std::make_shared<PBFTTimer>(m_config->checkPointTimeoutInterval());
    // register the timeout function
    m_timer->registerTimeoutHandler(boost::bind(&PBFTCache::onCheckPointTimeout, this));
}

void PBFTCache::onCheckPointTimeout()
{
    PBFT_LOG(WARNING) << LOG_DESC("onCheckPointTimeout: resend the checkpoint message package")
                      << LOG_KV("index", m_checkpointProposal->index())
                      << LOG_KV("hash", m_checkpointProposal->hash().abridged())
                      << m_config->printCurrentState();
    auto checkPointMsg = m_config->pbftMessageFactory()->populateFrom(PacketType::CheckPoint,
        m_config->pbftMsgDefaultVersion(), m_config->view(), utcTime(), m_config->nodeIndex(),
        m_checkpointProposal, m_config->cryptoSuite(), m_config->keyPair(), true);
    auto encodedData = m_config->codec()->encode(checkPointMsg);
    m_config->frontService()->asyncSendMessageByNodeIDs(
        ModuleID::PBFT, m_config->consensusNodeIDList(), ref(*encodedData));
    checkAndCommitStableCheckPoint();
    m_timer->restart();
}

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
    auto nodeInfo = m_config->getConsensusNodeByIndex(generatedFrom);
    if (!nodeInfo)
    {
        return;
    }
    if (!_weightInfo.count(proposalHash))
    {
        _weightInfo[proposalHash] = 0;
    }
    _weightInfo[proposalHash] += nodeInfo->weight();
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
    m_precommit = m_prePrepare;
    m_precommit->setGeneratedFrom(m_config->nodeIndex());
    setSignatureList(m_precommit->consensusProposal(), m_prepareCacheList);

    m_precommitWithoutData = m_precommit->populateWithoutProposal();
    auto precommitProposalWithoutData =
        m_config->pbftMessageFactory()->populateFrom(m_precommit->consensusProposal(), false);
    m_precommitWithoutData->setConsensusProposal(precommitProposalWithoutData);
    PBFT_LOG(DEBUG) << LOG_DESC("intoPrecommit") << printPBFTMsgInfo(m_precommit)
                    << m_config->printCurrentState();
}

void PBFTCache::setSignatureList(PBFTProposalInterface::Ptr _proposal, CollectionCacheType& _cache)
{
    assert(_cache.count(_proposal->hash()));
    for (auto const& it : _cache[_proposal->hash()])
    {
        _proposal->appendSignatureProof(it.first, it.second->consensusProposal()->signature());
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
    if (m_checkpointProposal)
    {
        return false;
    }
    if (m_precommit && m_precommit->view() >= m_prePrepare->view())
    {
        return false;
    }
    if (!collectEnoughPrepareReq())
    {
        return false;
    }
    // update and backup the proposal into precommit-status
    intoPrecommit();
    // generate the commitReq
    auto commitReq = m_config->pbftMessageFactory()->populateFrom(PacketType::CommitPacket,
        m_config->pbftMsgDefaultVersion(), m_config->view(), utcTime(), m_config->nodeIndex(),
        m_precommitWithoutData->consensusProposal(), m_config->cryptoSuite(), m_config->keyPair());
    // add the commitReq to local cache
    addCommitCache(commitReq);
    // broadcast the commitReq
    PBFT_LOG(DEBUG) << LOG_DESC("checkAndPreCommit: broadcast commitMsg")
                    << LOG_KV("Idx", m_config->nodeIndex())
                    << LOG_KV("hash", commitReq->hash().abridged())
                    << LOG_KV("index", commitReq->index());
    auto encodedData = m_config->codec()->encode(commitReq, m_config->pbftMsgDefaultVersion());
    m_config->frontService()->asyncSendMessageByNodeIDs(
        bcos::protocol::ModuleID::PBFT, m_config->consensusNodeIDList(), ref(*encodedData));
    // collect the commitReq and try to commit
    return checkAndCommit();
}

bool PBFTCache::checkAndCommit()
{
    if (m_submitted == true || m_checkpointProposal)
    {
        return false;
    }
    if (!collectEnoughCommitReq())
    {
        return false;
    }
    PBFT_LOG(DEBUG) << LOG_DESC("checkAndCommit")
                    << printPBFTProposal(m_precommit->consensusProposal())
                    << m_config->printCurrentState();
    m_submitted.store(true);
    return true;
}

bool PBFTCache::shouldStopTimer()
{
    if (m_index <= m_config->committedProposal()->index())
    {
        return true;
    }
    return m_submitted;
}

void PBFTCache::resetCache(ViewType _curView)
{
    m_submitted = false;
    // clear the expired prepare cache
    resetCacheAfterViewChange(m_prepareCacheList, _curView);
    // clear the expired commit cache
    resetCacheAfterViewChange(m_commitCacheList, _curView);

    // recalculate m_prepareReqWeight
    recalculateQuorum(m_prepareReqWeight, m_prepareCacheList);
    // recalculate m_commitReqWeight
    recalculateQuorum(m_commitReqWeight, m_commitCacheList);
}

void PBFTCache::setCheckPointProposal(PBFTProposalInterface::Ptr _proposal)
{
    m_timer->start();
    // expired checkpoint proposal
    if (_proposal->index() <= m_config->committedProposal()->index())
    {
        return;
    }
    if (m_checkpointProposal || _proposal->index() != index())
    {
        return;
    }
    m_checkpointProposal = _proposal;
    PBFT_LOG(DEBUG) << LOG_DESC("setCheckPointProposal") << printPBFTProposal(m_checkpointProposal)
                    << m_config->printCurrentState();
}

bool PBFTCache::collectEnoughCheckpoint()
{
    if (!m_checkpointProposal)
    {
        return false;
    }
    return collectEnoughQuorum(m_checkpointProposal->hash(), m_checkpointCacheWeight);
}

bool PBFTCache::checkAndCommitStableCheckPoint()
{
    if (m_stableCommitted || !collectEnoughCheckpoint())
    {
        return false;
    }
    setSignatureList(m_checkpointProposal, m_checkpointCacheList);
    m_stableCommitted = true;
    m_timer->stop();
    PBFT_LOG(INFO) << LOG_DESC("checkAndCommitStableCheckPoint")
                   << LOG_KV("index", m_checkpointProposal->index())
                   << LOG_KV("hash", m_checkpointProposal->hash().abridged())
                   << m_config->printCurrentState();
    if (m_config->committedProposal()->index() >= m_checkpointProposal->index())
    {
        PBFT_LOG(WARNING) << LOG_DESC("checkAndCommitStableCheckPoint: expired checkpointProposal")
                          << LOG_KV("checkPointIndex", m_checkpointProposal->index())
                          << m_config->printCurrentState();
        return false;
    }
    return true;
}