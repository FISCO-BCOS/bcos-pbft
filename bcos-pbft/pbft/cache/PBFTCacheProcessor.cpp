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
using namespace bcos::protocol;
using namespace bcos::crypto;

void PBFTCacheProcessor::initState(PBFTProposalList const& _proposals, NodeIDPtr _fromNode)
{
    for (auto proposal : _proposals)
    {
        // try to verify and load the proposal
        loadAndVerifyProposal(_fromNode, proposal);
    }
}

void PBFTCacheProcessor::loadAndVerifyProposal(
    NodeIDPtr _fromNode, PBFTProposalInterface::Ptr _proposal)
{
    // the local proposal
    if (_fromNode == nullptr)
    {
        updateCommitQueue(_proposal);
        return;
    }
    // Note: to fetch the remote proposal(the from node hits all transactions)
    auto self = std::weak_ptr<PBFTCacheProcessor>(shared_from_this());
    m_config->validator()->verifyProposal(
        _fromNode, _proposal, [self, _fromNode, _proposal](Error::Ptr _error, bool _verifyResult) {
            if (_error || !_verifyResult)
            {
                PBFT_LOG(WARNING) << LOG_DESC("loadAndVerifyProposal failed")
                                  << LOG_KV("from", _fromNode->shortHex())
                                  << LOG_KV("code", _error ? _error->errorCode() : 0)
                                  << LOG_KV("msg", _error ? _error->errorMessage() : "requestSent")
                                  << LOG_KV("verifyRet", _verifyResult);
                return;
            }
            try
            {
                auto cache = self.lock();
                if (!cache)
                {
                    return;
                }
                PBFT_LOG(INFO) << LOG_DESC("loadAndVerifyProposal success")
                               << LOG_KV("from", _fromNode->shortHex())
                               << printPBFTProposal(_proposal);
                cache->updateCommitQueue(_proposal);
            }
            catch (std::exception const& e)
            {
                PBFT_LOG(WARNING) << LOG_DESC("loadAndVerifyProposal exception")
                                  << printPBFTProposal(_proposal)
                                  << LOG_KV("error", boost::diagnostic_information(e));
            }
        });
}
// Note: please ensure the passed in _prePrepareMsg not be modified after addPrePrepareCache
void PBFTCacheProcessor::addPrePrepareCache(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    addCache(m_caches, _prePrepareMsg,
        [](PBFTCache::Ptr _pbftCache, PBFTMessageInterface::Ptr proposal) {
            _pbftCache->addPrePrepareCache(proposal);
        });
}

bool PBFTCacheProcessor::existPrePrepare(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    if (!m_caches.count(_prePrepareMsg->index()))
    {
        return false;
    }
    auto pbftCache = m_caches[_prePrepareMsg->index()];
    return pbftCache->existPrePrepare(_prePrepareMsg);
}

bool PBFTCacheProcessor::tryToFillProposal(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    if (!m_caches.count(_prePrepareMsg->index()))
    {
        return false;
    }
    auto pbftCache = m_caches[_prePrepareMsg->index()];
    auto precommit = pbftCache->preCommitCache();
    if (!precommit)
    {
        return false;
    }
    auto hit = (precommit->hash() == _prePrepareMsg->hash()) &&
               (precommit->index() == _prePrepareMsg->index());
    if (!hit)
    {
        return false;
    }
    auto proposalData = precommit->consensusProposal()->data();
    _prePrepareMsg->consensusProposal()->setData(proposalData);
    return true;
}

bool PBFTCacheProcessor::conflictWithProcessedReq(PBFTMessageInterface::Ptr _msg)
{
    if (!m_caches.count(_msg->index()))
    {
        return false;
    }
    auto pbftCache = m_caches[_msg->index()];
    return pbftCache->conflictWithProcessedReq(_msg);
}

bool PBFTCacheProcessor::conflictWithPrecommitReq(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    if (!m_caches.count(_prePrepareMsg->index()))
    {
        return false;
    }
    auto pbftCache = m_caches[_prePrepareMsg->index()];
    return pbftCache->conflictWithPrecommitReq(_prePrepareMsg);
}

void PBFTCacheProcessor::addCache(
    PBFTCachesType& _pbftCache, PBFTMessageInterface::Ptr _pbftReq, UpdateCacheHandler _handler)

{
    auto index = _pbftReq->index();
    if (!(_pbftCache.count(index)))
    {
        _pbftCache[index] = m_cacheFactory->createPBFTCache(m_config, index);
    }
    _handler(_pbftCache[index], _pbftReq);
}

void PBFTCacheProcessor::checkAndPreCommit()
{
    for (auto const& it : m_caches)
    {
        auto ret = it.second->checkAndPreCommit();
        if (!ret)
        {
            continue;
        }
        updateCommitQueue(it.second->preCommitCache()->consensusProposal());
    }
}

void PBFTCacheProcessor::checkAndCommit()
{
    for (auto const& it : m_caches)
    {
        auto ret = it.second->checkAndCommit();
        if (!ret)
        {
            continue;
        }
        updateCommitQueue(it.second->preCommitCache()->consensusProposal());
        // refresh the timer when commit success
        m_config->timer()->restart();
        m_config->resetToView();
    }
    resetTimer();
}

void PBFTCacheProcessor::resetTimer()
{
    for (auto const& it : m_caches)
    {
        if (!it.second->shouldStopTimer())
        {
            // start the timer when there has proposals in consensus
            if (!m_config->timer()->running())
            {
                m_config->timer()->start();
            }
            return;
        }
    }
    // reset the timer when has no proposals in consensus
    m_config->resetTimer();
}

void PBFTCacheProcessor::updateCommitQueue(PBFTProposalInterface::Ptr _committedProposal)
{
    assert(_committedProposal);
    m_committedQueue.push(_committedProposal);
    m_committedProposalList.insert(_committedProposal->index());
    PBFT_LOG(INFO) << LOG_DESC("######## CommitProposal") << printPBFTProposal(_committedProposal)
                   << m_config->printCurrentState();
    if (m_committedProposalNotifier)
    {
        m_committedProposalNotifier(
            _committedProposal->index(), [_committedProposal](Error::Ptr _error) {
                if (!_error)
                {
                    return;
                }
                PBFT_LOG(WARNING)
                    << LOG_DESC("notify the committed proposal index to the sync module failed")
                    << LOG_KV("index", _committedProposal->index())
                    << LOG_KV("hash", _committedProposal->hash().abridged());
            });
    }
    tryToApplyCommitQueue();
}

ProposalInterface::ConstPtr PBFTCacheProcessor::getAppliedCheckPointProposal(
    bcos::protocol::BlockNumber _index)
{
    if (_index == m_config->committedProposal()->index())
    {
        return m_config->committedProposal();
    }

    if (!(m_caches.count(_index)))
    {
        return nullptr;
    }
    return (m_caches[_index])->checkPointProposal();
}

void PBFTCacheProcessor::tryToApplyCommitQueue()
{
    while (!m_committedQueue.empty() &&
           m_committedQueue.top()->index() < m_config->expectedCheckPoint())
    {
        PBFT_LOG(DEBUG) << LOG_DESC("updateCommitQueue: remove invalid proposal")
                        << LOG_KV("index", m_committedQueue.top()->index())
                        << LOG_KV("expectedIndex", m_config->expectedCheckPoint())
                        << m_config->printCurrentState();
        m_committedQueue.pop();
    }
    // try to execute the proposal
    if (!m_committedQueue.empty() &&
        m_committedQueue.top()->index() == m_config->expectedCheckPoint())
    {
        auto proposal = m_committedQueue.top();
        auto lastAppliedProposal = getAppliedCheckPointProposal(m_config->expectedCheckPoint() - 1);
        if (!lastAppliedProposal)
        {
            PBFT_LOG(WARNING) << LOG_DESC("The last proposal has not been applied")
                              << m_config->printCurrentState();
            return;
        }
        // commit the proposal
        m_committedQueue.pop();
        applyStateMachine(lastAppliedProposal, proposal);
    }
}

// execute the proposal and broadcast checkpoint message
void PBFTCacheProcessor::applyStateMachine(
    ProposalInterface::ConstPtr _lastAppliedProposal, PBFTProposalInterface::Ptr _proposal)
{
    PBFT_LOG(DEBUG) << LOG_DESC("applyStateMachine") << LOG_KV("index", _proposal->index())
                    << LOG_KV("hash", _proposal->hash().abridged())
                    << m_config->printCurrentState();
    auto executedProposal = m_config->pbftMessageFactory()->createPBFTProposal();
    auto self = std::weak_ptr<PBFTCacheProcessor>(shared_from_this());
    m_config->stateMachine()->asyncApply(m_config->consensusNodeList(), _lastAppliedProposal,
        _proposal, executedProposal, [self, _proposal, executedProposal](bool _ret) {
            try
            {
                auto cache = self.lock();
                if (!cache)
                {
                    return;
                }
                auto config = cache->m_config;
                if (!_ret)
                {
                    PBFT_LOG(WARNING)
                        << LOG_DESC("_proposal execute failed") << printPBFTProposal(_proposal)
                        << config->printCurrentState();
                    auto expectedCheckPoint =
                        std::max(_proposal->index(), config->progressedIndex());
                    config->setExpectedCheckPoint(expectedCheckPoint);
                    return;
                }
                // commit the proposal when execute success
                config->storage()->asyncCommitProposal(_proposal);
                if (cache->m_proposalAppliedHandler)
                {
                    cache->m_proposalAppliedHandler(executedProposal);
                }
                PBFT_LOG(DEBUG) << LOG_DESC(
                                       "applyStateMachine success and broadcast checkpoint message")
                                << LOG_KV("index", executedProposal->index())
                                << LOG_KV("beforeExec", _proposal->hash().abridged())
                                << LOG_KV("afterExec", executedProposal->hash().abridged())
                                << config->printCurrentState();
            }
            catch (std::exception const& e)
            {
                PBFT_LOG(WARNING) << LOG_DESC("applyStateMachine failed")
                                  << LOG_KV("error", boost::diagnostic_information(e));
            }
        });
}

void PBFTCacheProcessor::setCheckPointProposal(PBFTProposalInterface::Ptr _proposal)
{
    auto index = _proposal->index();
    if (!(m_caches.count(index)))
    {
        m_caches[index] = m_cacheFactory->createPBFTCache(m_config, index);
    }
    (m_caches[index])->setCheckPointProposal(_proposal);
}

void PBFTCacheProcessor::addCheckPointMsg(PBFTMessageInterface::Ptr _checkPointMsg)
{
    addCache(m_caches, _checkPointMsg,
        [](PBFTCache::Ptr _pbftCache, PBFTMessageInterface::Ptr _checkPointMsg) {
            _pbftCache->addCheckPointMsg(_checkPointMsg);
        });
}

void PBFTCacheProcessor::addViewChangeReq(ViewChangeMsgInterface::Ptr _viewChange)
{
    auto reqView = _viewChange->view();
    auto fromIdx = _viewChange->generatedFrom();
    if (m_viewChangeCache.count(reqView) && m_viewChangeCache[reqView].count(fromIdx))
    {
        return;
    }

    auto nodeInfo = m_config->getConsensusNodeByIndex(fromIdx);
    if (!nodeInfo)
    {
        return;
    }
    m_viewChangeCache[reqView][fromIdx] = _viewChange;
    if (!m_viewChangeWeight.count(reqView))
    {
        m_viewChangeWeight[reqView] = 0;
    }
    m_viewChangeWeight[reqView] += nodeInfo->weight();
    auto committedIndex = _viewChange->committedProposal()->index();
    if (!m_maxCommittedIndex.count(reqView) || m_maxCommittedIndex[reqView] < committedIndex)
    {
        m_maxCommittedIndex[reqView] = committedIndex;
    }
    // get the max precommitIndex
    for (auto precommit : _viewChange->preparedProposals())
    {
        auto precommitIndex = precommit->index();
        if (!m_maxPrecommitIndex.count(reqView) || m_maxPrecommitIndex[reqView] < precommitIndex)
        {
            m_maxPrecommitIndex[reqView] = precommitIndex;
        }
    }
    PBFT_LOG(DEBUG) << LOG_DESC("addViewChangeReq") << printPBFTMsgInfo(_viewChange)
                    << LOG_KV("weight", m_viewChangeWeight[reqView])
                    << LOG_KV("maxCommittedIndex", m_maxCommittedIndex[reqView])
                    << LOG_KV("maxPrecommitIndex", m_maxPrecommitIndex[reqView])
                    << m_config->printCurrentState();
}

PBFTMessageList PBFTCacheProcessor::generatePrePrepareMsg(
    std::map<IndexType, ViewChangeMsgInterface::Ptr> _viewChangeCache)
{
    auto toView = m_config->toView();
    auto maxCommittedIndex = m_config->committedProposal()->index();
    if (m_maxCommittedIndex.count(toView))
    {
        maxCommittedIndex = m_maxCommittedIndex[toView];
    }
    auto maxPrecommitIndex = m_config->progressedIndex();
    if (m_maxPrecommitIndex.count(toView))
    {
        maxPrecommitIndex = m_maxPrecommitIndex[toView];
    }
    std::map<BlockNumber, PBFTMessageInterface::Ptr> preparedProposals;
    for (auto it : _viewChangeCache)
    {
        auto viewChangeReq = it.second;
        for (auto proposal : viewChangeReq->preparedProposals())
        {
            // invalid precommit proposal
            if (proposal->index() < maxCommittedIndex)
            {
                continue;
            }
            // repeated precommit proposal
            if (preparedProposals.count(proposal->index()))
            {
                auto precommitProposal = preparedProposals[proposal->index()];
                assert(precommitProposal->index() == proposal->index() &&
                       precommitProposal->hash() == proposal->hash());
                continue;
            }
            // new precommit proposla
            preparedProposals[proposal->index()] = proposal;
            proposal->setGeneratedFrom(viewChangeReq->generatedFrom());
        }
    }
    // generate prepareMsg from maxCommittedIndex to  maxPrecommitIndex
    PBFTMessageList prePrepareMsgList;
    for (auto i = (maxCommittedIndex + 1); i <= maxPrecommitIndex; i++)
    {
        PBFTProposalInterface::Ptr prePrepareProposal = nullptr;
        auto generatedFrom = m_config->nodeIndex();
        bool empty = false;
        if (preparedProposals.count(i))
        {
            prePrepareProposal = preparedProposals[i]->consensusProposal();
            generatedFrom = preparedProposals[i]->generatedFrom();
        }
        else
        {
            // empty prePrepare
            prePrepareProposal = m_config->validator()->generateEmptyProposal(
                m_config->pbftMessageFactory(), i, m_config->nodeIndex());
            empty = true;
        }
        auto prePrepareMsg = m_config->pbftMessageFactory()->populateFrom(
            PacketType::PrePreparePacket, prePrepareProposal, m_config->pbftMsgDefaultVersion(),
            m_config->toView(), utcTime(), generatedFrom);
        prePrepareMsgList.push_back(prePrepareMsg);
        PBFT_LOG(DEBUG) << LOG_DESC("generatePrePrepareMsg") << printPBFTMsgInfo(prePrepareMsg)
                        << LOG_KV("dataSize", prePrepareMsg->consensusProposal()->data().size())
                        << LOG_KV("emptyProposal", empty);
    }
    return prePrepareMsgList;
}

NewViewMsgInterface::Ptr PBFTCacheProcessor::checkAndTryIntoNewView()
{
    if (m_newViewGenerated || !m_config->leaderAfterViewChange())
    {
        return nullptr;
    }
    auto toView = m_config->toView();
    if (!m_viewChangeWeight.count(toView))
    {
        return nullptr;
    }
    if (m_viewChangeWeight[toView] < m_config->minRequiredQuorum())
    {
        return nullptr;
    }
    // the next leader collect enough viewChange requests
    // set the viewchanges(without prePreparedProposals)
    auto viewChangeCache = m_viewChangeCache[toView];
    ViewChangeMsgList viewChangeList;
    for (auto const& it : viewChangeCache)
    {
        viewChangeList.push_back(it.second);
    }
    // create newView message
    auto newViewMsg = m_config->pbftMessageFactory()->createNewViewMsg();
    newViewMsg->setHash(m_config->committedProposal()->hash());
    newViewMsg->setIndex(m_config->committedProposal()->index());
    newViewMsg->setPacketType(PacketType::NewViewPacket);
    newViewMsg->setVersion(m_config->pbftMsgDefaultVersion());
    newViewMsg->setView(toView);
    newViewMsg->setTimestamp(utcTime());
    newViewMsg->setGeneratedFrom(m_config->nodeIndex());
    // set viewchangeList
    newViewMsg->setViewChangeMsgList(viewChangeList);
    // set generated pre-prepare list
    auto generatedPrePrepareList = generatePrePrepareMsg(viewChangeCache);
    newViewMsg->setPrePrepareList(generatedPrePrepareList);
    // encode and broadcast the viewchangeReq
    auto encodedData = m_config->codec()->encode(newViewMsg);
    m_config->frontService()->asyncSendMessageByNodeIDs(
        ModuleID::PBFT, m_config->consensusNodeIDList(), ref(*encodedData));
    m_newViewGenerated = true;
    PBFT_LOG(DEBUG) << LOG_DESC("The next leader broadcast NewView request")
                    << printPBFTMsgInfo(newViewMsg) << LOG_KV("Idx", m_config->nodeIndex());
    return newViewMsg;
}

ViewType PBFTCacheProcessor::tryToTriggerFastViewChange()
{
    uint64_t greaterViewWeight = 0;
    ViewType viewToReach = 0;
    for (auto const& it : m_viewChangeCache)
    {
        if (it.first <= m_config->toView())
        {
            continue;
        }
        auto view = it.first;
        if (viewToReach == 0)
        {
            viewToReach = view;
        }
        if (viewToReach > view)
        {
            viewToReach = view;
        }
        // check the quorum
        auto viewChangeCache = it.second;
        for (auto const& cache : viewChangeCache)
        {
            auto fromIdx = cache.first;
            auto nodeInfo = m_config->getConsensusNodeByIndex(fromIdx);
            if (!nodeInfo)
            {
                continue;
            }
            greaterViewWeight += nodeInfo->weight();
        }
    }
    if (greaterViewWeight < (m_config->maxFaultyQuorum() + 1))
    {
        return 0;
    }
    if (m_config->toView() >= viewToReach)
    {
        return 0;
    }
    m_config->setToView(viewToReach);
    PBFT_LOG(INFO) << LOG_DESC("tryToTriggerFastViewChange") << m_config->printCurrentState();
    return viewToReach;
}

bool PBFTCacheProcessor::checkPrecommitMsg(PBFTMessageInterface::Ptr _precommitMsg)
{
    // check the view
    if (_precommitMsg->view() > m_config->view())
    {
        return false;
    }
    if (!_precommitMsg->consensusProposal())
    {
        return false;
    }
    return checkPrecommitWeight(_precommitMsg);
}

bool PBFTCacheProcessor::checkPrecommitWeight(PBFTMessageInterface::Ptr _precommitMsg)
{
    auto precommitProposal = _precommitMsg->consensusProposal();
    // check the signature
    uint64_t weight = 0;
    auto proofSize = precommitProposal->signatureProofSize();
    for (size_t i = 0; i < proofSize; i++)
    {
        auto proof = precommitProposal->signatureProof(i);
        // check the proof
        auto nodeInfo = m_config->getConsensusNodeByIndex(proof.first);
        if (!nodeInfo)
        {
            return false;
        }
        // verify the signature
        auto ret = m_config->cryptoSuite()->signatureImpl()->verify(
            nodeInfo->nodeID(), precommitProposal->hash(), proof.second);
        if (!ret)
        {
            return false;
        }
        weight += nodeInfo->weight();
    }
    // check the quorum
    return (weight >= m_config->minRequiredQuorum());
}

ViewChangeMsgInterface::Ptr PBFTCacheProcessor::fetchPrecommitData(
    BlockNumber _index, bcos::crypto::HashType const& _hash)
{
    if (!m_caches.count(_index))
    {
        return nullptr;
    }
    auto cache = m_caches[_index];
    if (cache->preCommitCache() == nullptr || cache->preCommitCache()->hash() != _hash)
    {
        return nullptr;
    }

    PBFTMessageList precommitMessage;
    precommitMessage.push_back(cache->preCommitCache());

    auto pbftMessage = m_config->pbftMessageFactory()->createViewChangeMsg();
    pbftMessage->setPreparedProposals(precommitMessage);
    return pbftMessage;
}

void PBFTCacheProcessor::removeConsensusedCache(ViewType _view, BlockNumber _consensusedNumber)
{
    for (auto pcache = m_caches.begin(); pcache != m_caches.end();)
    {
        if (pcache->first <= _consensusedNumber)
        {
            pcache = m_caches.erase(pcache);
            continue;
        }
        if (pcache->second->stableCommitted())
        {
            pcache = m_caches.erase(pcache);
            continue;
        }
        pcache++;
    }
    removeInvalidViewChange(_view, _consensusedNumber);
    m_maxPrecommitIndex.clear();
    m_maxCommittedIndex.clear();
    m_newViewGenerated = false;
}


void PBFTCacheProcessor::resetCacheAfterViewChange(
    ViewType _view, BlockNumber _latestCommittedProposal)
{
    for (auto const& it : m_caches)
    {
        it.second->resetCache(_view);
    }
    m_maxPrecommitIndex.clear();
    m_maxCommittedIndex.clear();
    m_newViewGenerated = false;
    removeInvalidViewChange(_view, _latestCommittedProposal);
}

void PBFTCacheProcessor::removeInvalidViewChange(
    ViewType _view, BlockNumber _latestCommittedProposal)
{
    for (auto it = m_viewChangeCache.begin(); it != m_viewChangeCache.end();)
    {
        auto view = it->first;
        if (view <= _view)
        {
            it = m_viewChangeCache.erase(it);
            m_viewChangeWeight.erase(view);
            continue;
        }
        auto viewChangeCache = it->second;
        for (auto pcache = viewChangeCache.begin(); pcache != viewChangeCache.end();)
        {
            auto index = pcache->second->index();
            if (index < _latestCommittedProposal)
            {
                pcache = viewChangeCache.erase(pcache);
                continue;
            }
            pcache++;
        }
        it++;
    }
    // recalculate m_viewChangeWeight
    reCalculateViewChangeWeight();
}

void PBFTCacheProcessor::reCalculateViewChangeWeight()
{
    for (auto const& it : m_viewChangeCache)
    {
        auto view = it.first;
        m_viewChangeWeight[view] = 0;
        auto const& viewChangeCache = it.second;
        for (auto const& cache : viewChangeCache)
        {
            auto generatedFrom = cache.second->generatedFrom();
            auto nodeInfo = m_config->getConsensusNodeByIndex(generatedFrom);
            if (!nodeInfo)
            {
                continue;
            }
            m_viewChangeWeight[view] += nodeInfo->weight();
        }
    }
}

void PBFTCacheProcessor::checkAndCommitStableCheckPoint()
{
    std::vector<PBFTCache::Ptr> stabledCacheList;
    for (auto const& it : m_caches)
    {
        auto ret = it.second->checkAndCommitStableCheckPoint();
        if (!ret)
        {
            continue;
        }
        stabledCacheList.emplace_back(it.second);
    }
    // Note: since updateStableCheckPointQueue may update m_caches after commitBlock
    // must call it after iterator m_caches
    for (auto cache : stabledCacheList)
    {
        updateStableCheckPointQueue(cache->checkPointProposal());
    }
}

void PBFTCacheProcessor::updateStableCheckPointQueue(PBFTProposalInterface::Ptr _stableCheckPoint)
{
    assert(_stableCheckPoint);
    m_stableCheckPointQueue.push(_stableCheckPoint);
    PBFT_LOG(DEBUG) << LOG_DESC("updateStableCheckPointQueue: insert new checkpoint proposal")
                    << LOG_KV("index", _stableCheckPoint->index())
                    << LOG_KV("hash", _stableCheckPoint->hash().abridged())
                    << m_config->printCurrentState();
    tryToCommitStableCheckPoint();
}

void PBFTCacheProcessor::tryToCommitStableCheckPoint()
{
    // remove the invalid checkpoint
    while (!m_stableCheckPointQueue.empty() &&
           m_stableCheckPointQueue.top()->index() <= m_config->committedProposal()->index())
    {
        PBFT_LOG(DEBUG) << LOG_DESC("updateStableCheckPointQueue: remove invalid checkpoint")
                        << LOG_KV("index", m_stableCheckPointQueue.top()->index())
                        << LOG_KV("committedIndex", m_config->committedProposal()->index());
        m_committedProposalList.erase(m_stableCheckPointQueue.top()->index());
        m_stableCheckPointQueue.pop();
    }
    // submit stable-checkpoint to ledger in ordeer
    if (!m_stableCheckPointQueue.empty() &&
        m_stableCheckPointQueue.top()->index() == m_config->committedProposal()->index() + 1)
    {
        PBFT_LOG(DEBUG) << LOG_DESC("updateStableCheckPointQueue: commit stable checkpoint")
                        << LOG_KV("index", m_stableCheckPointQueue.top()->index())
                        << LOG_KV("committedIndex", m_config->committedProposal()->index());
        auto stableCheckPoint = m_stableCheckPointQueue.top();
        m_committedProposalList.erase(stableCheckPoint->index());
        m_stableCheckPointQueue.pop();
        m_config->storage()->asyncCommmitStableCheckPoint(stableCheckPoint);
    }
}

bool PBFTCacheProcessor::shouldRequestCheckPoint(bcos::protocol::BlockNumber _checkPointIndex)
{
    // expired checkpoint
    if (_checkPointIndex <= m_config->committedProposal()->index())
    {
        return false;
    }
    // hit in the local committedProposalList or already been requested
    if (m_committedProposalList.count(_checkPointIndex))
    {
        return false;
    }
    // precompiled in the local cache
    if (m_caches.count(_checkPointIndex) && m_caches[_checkPointIndex]->precommitted())
    {
        return false;
    }
    // in case of request again
    m_committedProposalList.insert(_checkPointIndex);
    return true;
}