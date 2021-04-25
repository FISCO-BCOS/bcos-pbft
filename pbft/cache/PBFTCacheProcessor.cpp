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
 * @brief cache processor for the PBFTReq
 * @file PBFTCacheProcessor.cpp
 * @author: yujiechen
 * @date 2021-04-21
 */
#include "PBFTCacheProcessor.h"
#include <bcos-framework/interfaces/protocol/Protocol.h>

using namespace bcos;
using namespace bcos::consensus;
// Note: please ensure the passed in _prePrepareMsg not be modified after addPrePrepareCache
void PBFTCacheProcessor::addPrePrepareCache(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    WriteGuard l(x_prePrepareCache);
    m_prePrepareCache = _prePrepareMsg;
    addCache(m_caches, _prePrepareMsg,
        [](PBFTCache::Ptr _pbftCache, PBFTProposalInterface::Ptr proposal) {
            _pbftCache->addPrePrepareCache(proposal);
        });
    m_blockToCommit = _prePrepareMsg->index();
    PBFT_LOG(INFO) << LOG_DESC("addPrePrepareCache") << printPBFTMsgInfo(_prePrepareMsg);
}

bool PBFTCacheProcessor::existPrePrepare(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    ReadGuard l(x_prePrepareCache);
    if (m_prePrepareCache == nullptr)
    {
        return false;
    }
    return (_prePrepareMsg->hash() == m_prePrepareCache->hash()) &&
           (m_prePrepareCache->view() >= _prePrepareMsg->view());
}

bool PBFTCacheProcessor::conflictWithProcessedReq(PBFTMessageInterface::Ptr _msg)
{
    ReadGuard l(x_prePrepareCache);
    if (m_prePrepareCache == nullptr)
    {
        return false;
    }
    return (_msg->hash() != m_prePrepareCache->hash() || _msg->view() != m_prePrepareCache->view());
}

bool PBFTCacheProcessor::conflictWithPrecommitReq(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    if (!m_preCommitReq)
    {
        return false;
    }
    if (m_preCommitReq->index() < m_config->progressedIndex())
    {
        return false;
    }
    if (_prePrepareMsg->index() == m_preCommitReq->index() &&
        _prePrepareMsg->hash() != m_preCommitReq->hash())
    {
        PBFT_LOG(INFO) << LOG_DESC(
                              "the received pre-prepare msg is conflict with the preparedCache")
                       << printPBFTMsgInfo(_prePrepareMsg);
        return true;
    }
    return true;
}

void PBFTCacheProcessor::addCache(
    PBFTCachesType& _proposalCache, PBFTMessageInterface::Ptr _pbftReq, UpdateCacheHandler _handler)

{
    auto const& msgHash = _pbftReq->hash();
    auto const& proposals = _pbftReq->proposals();
    for (auto proposal : proposals)
    {
        auto index = proposal->index();
        if (!_proposalCache.count(msgHash) || !(_proposalCache[msgHash].count(index)))
        {
            _proposalCache[msgHash][index] = std::make_shared<PBFTCache>(m_config, index);
        }
        proposal->setView(_pbftReq->view());
        proposal->setGeneratedFrom(_pbftReq->generatedFrom());
        _handler(_proposalCache[msgHash][index], proposal);
    }
}

void PBFTCacheProcessor::checkAndPreCommit()
{
    if (!m_prePrepareCache)
    {
        return;
    }
    auto hash = m_prePrepareCache->hash();
    if (!m_caches.count(hash))
    {
        return;
    }
    auto const& reqList = m_caches[hash];
    PBFTProposalList precommitProposals;
    for (auto const& it : reqList)
    {
        if (it.second->collectEnoughPrepareReq())
        {
            // update and backup the proposal into precommit-status
            it.second->intoPrecommit();
            precommitProposals.push_back(it.second->preCommitProposal());
            m_preCommitReq = m_prePrepareCache;
            auto precommitWithoutData =
                m_config->pbftMessageFactory()->populateFrom(it.second->preCommitProposal(), false);
            m_preCommitProposals->push_back(precommitWithoutData);
        }
    }
    // generate the commitReq
    auto commitReq = m_config->pbftMessageFactory()->populateFrom(PacketType::CommitPacket,
        m_config->pbftMsgDefaultVersion(), m_config->view(), utcTime(), m_config->nodeIndex(),
        precommitProposals, m_config->cryptoSuite(), m_config->keyPair());
    // add the commitReq to local cache
    addCommitReq(commitReq);
    // broadcast the commitReq
    auto encodedData = m_config->codec()->encode(commitReq, m_config->pbftMsgDefaultVersion());
    m_config->frontService()->asyncSendMessageByNodeIDs(
        bcos::protocol::ModuleID::PBFT, m_config->consensusNodeIDList(), ref(*encodedData));
    // collect the commitReq and try to commit
    checkAndCommit();
}

void PBFTCacheProcessor::checkAndCommit()
{
    if (!m_prePrepareCache)
    {
        return;
    }
    auto hash = m_prePrepareCache->hash();
    if (!m_caches.count(hash))
    {
        return;
    }
    auto const& reqList = m_caches[hash];
    PBFTProposalList commitProposals;
    for (auto const& it : reqList)
    {
        if (it.second->collectEnoughCommitReq())
        {
            // commit the proposal to the queue fistly
            m_commitQueue.push(it.second->preCommitProposal());
            it.second->setSignatureList();
        }
    }
    submitProposalsInOrder();
}

void PBFTCacheProcessor::submitProposalsInOrder()
{
    auto topProposal = m_commitQueue.top();
    if (!topProposal)
    {
        return;
    }
    if (topProposal->index() > m_blockToCommit)
    {
        return;
    }
    if (topProposal->index() < m_blockToCommit)
    {
        PBFT_LOG(FATAL) << LOG_DESC(
                               "The index of the proposal to commit is smaller than expected block")
                        << printPBFTProposal(topProposal);
        return;
    }
    // submit blocks to the storage module in order
    while (topProposal != nullptr && topProposal->index() == m_blockToCommit)
    {
        // commit the proposal
        m_config->storage()->asyncCommitProposal(topProposal);
        m_commitQueue.pop();
        m_blockToCommit += 1;
        topProposal = m_commitQueue.top();
    }
}

void PBFTCacheProcessor::addViewChangeReq(ViewChangeMsgInterface::Ptr _viewChange)
{
    auto reqView = _viewChange->view();
    auto fromIdx = _viewChange->generatedFrom();
    if (!m_viewChangeCache.count(reqView) && !m_viewChangeCache[reqView].count(fromIdx))
    {
        m_viewChangeCache[reqView][fromIdx] = _viewChange;
    }
}
// TODO:
void PBFTCacheProcessor::checkAndTryToNewView() {}