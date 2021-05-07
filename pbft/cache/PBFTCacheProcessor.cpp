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
    addCache(m_caches, _prePrepareMsg,
        [](PBFTCache::Ptr _pbftCache, PBFTMessageInterface::Ptr proposal) {
            _pbftCache->addPrePrepareCache(proposal);
        });
    PBFT_LOG(INFO) << LOG_DESC("addPrePrepareCache") << printPBFTMsgInfo(_prePrepareMsg);
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
    auto const& proposalData = precommit->consensusProposal()->data();
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
        _pbftCache[index] = std::make_shared<PBFTCache>(m_config, index);
    }
    _handler(_pbftCache[index], _pbftReq);
}

void PBFTCacheProcessor::checkAndPreCommit()
{
    for (auto const& it : m_caches)
    {
        it.second->checkAndPreCommit();
    }
}

void PBFTCacheProcessor::checkAndCommit()
{
    for (auto const& it : m_caches)
    {
        it.second->checkAndCommit();
    }
}

void PBFTCacheProcessor::addViewChangeReq(ViewChangeMsgInterface::Ptr _viewChange)
{
    auto reqView = _viewChange->view();
    auto fromIdx = _viewChange->generatedFrom();
    if (!m_viewChangeCache.count(reqView) && !m_viewChangeCache[reqView].count(fromIdx))
    {
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
            if (!m_maxPrecommitIndex.count(reqView) ||
                m_maxPrecommitIndex[reqView] < precommitIndex)
            {
                m_maxPrecommitIndex[reqView] = precommitIndex;
            }
        }
        PBFT_LOG(DEBUG) << LOG_DESC("addViewChangeReq") << printPBFTMsgInfo(_viewChange)
                        << LOG_KV("weight", m_viewChangeWeight[reqView])
                        << LOG_KV("maxCommittedIndex", m_maxCommittedIndex[reqView])
                        << LOG_KV("maxPrecommitIndex", m_maxPrecommitIndex[reqView]);
    }
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
    std::map<bcos::protocol::BlockNumber, PBFTMessageInterface::Ptr> preparedProposals;
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
    for (auto i = maxCommittedIndex; i < maxPrecommitIndex; i++)
    {
        PBFTProposalInterface::Ptr prePrepareProposal = nullptr;
        auto generatedFrom = m_config->nodeIndex();
        if (preparedProposals.count(i))
        {
            prePrepareProposal = preparedProposals[i]->consensusProposal();
            generatedFrom = preparedProposals[i]->generatedFrom();
        }
        else
        {
            // empty prePrepare
            prePrepareProposal = m_config->pbftMessageFactory()->populateEmptyProposal(
                i, m_config->cryptoSuite()->hashImpl()->emptyHash());
        }
        auto prePrepareMsg =
            m_config->pbftMessageFactory()->populateFrom(PacketType::PrePreparePacket,
                m_config->pbftMsgDefaultVersion(), m_config->toView(), utcTime(), generatedFrom,
                prePrepareProposal, m_config->cryptoSuite(), m_config->keyPair(), false);

        prePrepareMsgList.push_back(prePrepareMsg);
        PBFT_LOG(DEBUG) << LOG_DESC("generatePrePrepareMsg") << printPBFTMsgInfo(prePrepareMsg);
    }
    return prePrepareMsgList;
}

NewViewMsgInterface::Ptr PBFTCacheProcessor::checkAndTryIntoNewView()
{
    if (!m_config->leaderAfterViewChange())
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
        viewChangeList.push_back(m_config->pbftMessageFactory()->populateFrom(it.second));
    }
    // create newView message
    auto newViewMsg = m_config->pbftMessageFactory()->createNewViewMsg();
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
        bcos::protocol::ModuleID::PBFT, m_config->consensusNodeIDList(), ref(*encodedData));
    PBFT_LOG(DEBUG) << LOG_DESC("The next leader broadcast NewView request")
                    << printPBFTMsgInfo(newViewMsg);
    return newViewMsg;
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

bytesPointer PBFTCacheProcessor::fetchPrecommitData(ViewChangeMsgInterface::Ptr _pbftMessage,
    bcos::protocol::BlockNumber _index, bcos::crypto::HashType const& _hash)
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
    _pbftMessage->setPreparedProposals(precommitMessage);
    return m_config->codec()->encode(_pbftMessage);
}

void PBFTCacheProcessor::removeConsensusedCache(bcos::protocol::BlockNumber _consensusedNumber)
{
    for (auto pcache = m_caches.begin(); pcache != m_caches.end();)
    {
        if (pcache->first <= _consensusedNumber)
        {
            pcache = m_caches.erase(pcache);
            continue;
        }
        pcache++;
    }
}