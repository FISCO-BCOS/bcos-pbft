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
 * @brief implementation for PBFTEngine
 * @file PBFTEngine.cpp
 * @author: yujiechen
 * @date 2021-04-20
 */
#include "PBFTEngine.h"
#include "boost/bind.hpp"
#include "interfaces/protocol/CommonError.h"
#include "pbft/cache/PBFTCacheProcessor.h"
#include <bcos-framework/interfaces/protocol/Protocol.h>

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::front;
using namespace bcos::protocol;

PBFTEngine::PBFTEngine(PBFTConfig::Ptr _config)
  : ConsensusEngine("pbft", 0), m_config(_config), m_msgQueue(std::make_shared<PBFTMsgQueue>())
{
    m_cacheProcessor = std::make_shared<PBFTCacheProcessor>(_config);
}

void PBFTEngine::start()
{
    // register the message dispatcher callback to front service
    m_config->frontService()->registerMessageDispatcher(
        ModuleID::PBFT, boost::bind(&PBFTEngine::onReceivePBFTMessage, this, _1, _2, _3, _4));
    // TODO: init the storage state
    ConsensusEngine::start();
}

void PBFTEngine::stop()
{
    ConsensusEngine::stop();
}

void PBFTEngine::onReceivePBFTMessage(bcos::Error::Ptr _error, bcos::crypto::NodeIDPtr _fromNode,
    bytesConstRef _data, std::function<void(bytesConstRef _respData)>)
{
    try
    {
        if (_error->errorCode() != CommonError::SUCCESS)
        {
            return;
        }
        // the node is not the consensusNode
        if (!m_config->isConsensusNode())
        {
            PBFT_LOG(TRACE) << LOG_DESC(
                "onReceivePBFTMessage: reject the message for the node is not the consensus "
                "node");
            return;
        }
        // decode the message and push the message into the queue
        m_msgQueue->push(m_config->codec()->decode(_data));
    }
    catch (std::exception const& _e)
    {
        PBFT_LOG(WARNING) << LOG_DESC("onReceivePBFTMessage exception")
                          << LOG_KV("fromNode", _fromNode->hex())
                          << LOG_KV("idx", m_config->nodeIndex())
                          << LOG_KV("nodeId", m_config->nodeID()->hex())
                          << LOG_KV("error", boost::diagnostic_information(_e));
    }
}

void PBFTEngine::executeWorker()
{
    // the node is not the consensusNode
    if (!m_config->isConsensusNode())
    {
        waitSignal();
        return;
    }
    // handle the PBFT message(here will wait when the msgQueue is empty)
    auto messageResult = m_msgQueue->tryPop(c_PopWaitSeconds);
    if (messageResult.first)
    {
        handleMsg(messageResult.second);
    }
    m_cacheProcessor->clearExpiredCache();
}

void PBFTEngine::handleMsg(std::shared_ptr<PBFTBaseMessageInterface> _msg)
{
    // TODO: Consider carefully whether this lock can be removed
    Guard l(m_mutex);
    switch (_msg->packetType())
    {
    case PacketType::PrePreparePacket:
    {
        auto prePrepareMsg = std::dynamic_pointer_cast<PBFTMessageInterface>(_msg);
        handlePrePrepareMsg(prePrepareMsg, m_config->needVerifyProposal());
        break;
    }
    case PacketType::PreparePacket:
    {
        auto prepareMsg = std::dynamic_pointer_cast<PBFTMessageInterface>(_msg);
        handlePrepareMsg(prepareMsg);
        break;
    }
    case PacketType::CommitPacket:
    {
        auto commitMsg = std::dynamic_pointer_cast<PBFTMessageInterface>(_msg);
        handleCommitMsg(commitMsg);
        break;
    }
    case PacketType::ViewChangePacket:
    {
        auto viewChangeMsg = std::dynamic_pointer_cast<ViewChangeMsgInterface>(_msg);
        handleViewChangeMsg(viewChangeMsg);
        break;
    }
    case PacketType::NewViewPacket:
    {
        auto newViewMsg = std::dynamic_pointer_cast<NewViewMsgInterface>(_msg);
        handleNewViewMsg(newViewMsg);
        break;
    }
    default:
    {
        PBFT_LOG(DEBUG) << LOG_DESC("handleMsg: unknown PBFT message")
                        << LOG_KV("type", std::to_string(_msg->packetType()))
                        << LOG_KV("genIdx", _msg->generatedFrom())
                        << LOG_KV("nodesef", m_config->nodeID()->hex());
        return;
    }
    }
}

CheckResult PBFTEngine::checkPBFTMsgState(PBFTBaseMessageInterface::Ptr _pbftReq) const
{
    if (_pbftReq->index() < m_config->progressedIndex())
    {
        return CheckResult::INVALID;
    }
    if (_pbftReq->index() > m_config->progressedIndex())
    {
        return CheckResult::FUTURE;
    }
    // case index equal
    if (_pbftReq->view() < m_config->view())
    {
        return CheckResult::INVALID;
    }
    if (_pbftReq->view() > m_config->view())
    {
        return CheckResult::FUTURE;
    }
    return CheckResult::VALID;
}

CheckResult PBFTEngine::checkPrePrepareMsg(std::shared_ptr<PBFTMessageInterface> _prePrepareMsg)
{
    // check the existence of the msg
    if (m_cacheProcessor->existPrePrepare(_prePrepareMsg))
    {
        return CheckResult::INVALID;
    }
    // check conflict
    if (m_cacheProcessor->conflictWithPrecommitReq(_prePrepareMsg))
    {
        return CheckResult::INVALID;
    }
    // check the state of the request
    auto checkResult = checkPBFTMsgState(_prePrepareMsg);
    if (checkResult == CheckResult::INVALID)
    {
        return checkResult;
    }
    if (checkResult == CheckResult::FUTURE)
    {
        return checkResult;
    }
    // packet can be processed in this round of consensus
    // check the proposal is generated from the leader
    if (m_config->leaderIndex(_prePrepareMsg->index()) != _prePrepareMsg->generatedFrom())
    {
        return CheckResult::INVALID;
    }
    // check the signature
    return checkSignature(_prePrepareMsg);
}

CheckResult PBFTEngine::checkSignature(PBFTBaseMessageInterface::Ptr _req)
{
    // check the signature
    auto publicKey = m_config->getConsensusNodeByIndex(_req->generatedFrom())->nodeID();
    if (!publicKey)
    {
        return CheckResult::INVALID;
    }
    if (!_req->verifySignature(m_config->cryptoSuite(), publicKey))
    {
        return CheckResult::INVALID;
    }
    return CheckResult::VALID;
}

bool PBFTEngine::handlePrePrepareMsg(
    PBFTMessageInterface::Ptr _prePrepareMsg, bool _needVerifyProposal)
{
    auto result = checkPrePrepareMsg(_prePrepareMsg);
    if (result == CheckResult::INVALID)
    {
        return false;
    }
    // add the prePrepareReq to the cache
    if (!_needVerifyProposal)
    {
        // add the pre-prepare packet into the cache
        m_cacheProcessor->addPrePrepareCache(_prePrepareMsg);

        // broadcast PrepareMsg the packet
        broadcastPrepareMsg(_prePrepareMsg);
        PBFT_LOG(DEBUG) << LOG_DESC("handlePrePrepareMsg") << printPBFTMsgInfo(_prePrepareMsg)
                        << m_config->printCurrentState();
        return true;
    }
    // verify the proposal
    auto self = std::weak_ptr<PBFTEngine>(shared_from_this());
    m_config->validator()->verifyProposals(m_config->nodeID(), _prePrepareMsg->proposals(),
        [self, _prePrepareMsg](bcos::Error::Ptr _error, bool _verifyResult) {
            try
            {
                auto pbftEngine = self.lock();
                if (!pbftEngine)
                {
                    return;
                }
                // verify exceptioned
                if (_error->errorCode() != CommonError::SUCCESS)
                {
                    PBFT_LOG(WARNING) << LOG_DESC("verify proposal exceptioned")
                                      << printPBFTMsgInfo(_prePrepareMsg)
                                      << LOG_KV("errorCode", _error->errorCode())
                                      << LOG_KV("errorMsg", _error->errorMessage());
                    return;
                }
                // verify failed
                if (!_verifyResult)
                {
                    PBFT_LOG(WARNING)
                        << LOG_DESC("verify proposal failed") << printPBFTMsgInfo(_prePrepareMsg);
                }
                // verify success
                pbftEngine->handlePrePrepareMsg(_prePrepareMsg, false);
            }
            catch (std::exception const& _e)
            {
                PBFT_LOG(WARNING) << LOG_DESC("exception when calls onVerifyFinishedHandler")
                                  << printPBFTMsgInfo(_prePrepareMsg)
                                  << LOG_KV("error", boost::diagnostic_information(_e));
            }
        });
    return true;
}

void PBFTEngine::broadcastPrepareMsg(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    auto prepareMsg = m_config->pbftMessageFactory()->populateFrom(PacketType::PreparePacket,
        m_config->pbftMsgDefaultVersion(), m_config->view(), utcTime(), m_config->nodeIndex(),
        _prePrepareMsg->proposals(), m_config->cryptoSuite(), m_config->keyPair());
    prepareMsg->setIndex(_prePrepareMsg->index());
    // add the message to local cache
    m_cacheProcessor->addPrepareCache(prepareMsg);

    auto encodedData = m_config->codec()->encode(prepareMsg, m_config->pbftMsgDefaultVersion());
    // only broadcast to the consensus nodes
    m_config->frontService()->asyncSendMessageByNodeIDs(
        ModuleID::PBFT, m_config->consensusNodeIDList(), ref(*encodedData));
    // try to precommit the message
    m_cacheProcessor->checkAndPreCommit();
}


CheckResult PBFTEngine::checkPBFTMsg(std::shared_ptr<PBFTMessageInterface> _prepareMsg)
{
    auto result = checkPBFTMsgState(_prepareMsg);
    if (result == CheckResult::INVALID)
    {
        return result;
    }
    // get the future request
    if (result == CheckResult::FUTURE)
    {
        return result;
    }
    if (_prepareMsg->generatedFrom() == m_config->nodeIndex())
    {
        PBFT_LOG(TRACE) << LOG_DESC("checkPrepareMsg: Recv own req")
                        << printPBFTMsgInfo(_prepareMsg);
        return CheckResult::INVALID;
    }
    // compare with the local pre-prepare cache
    if (m_cacheProcessor->conflictWithProcessedReq(_prepareMsg))
    {
        return CheckResult::INVALID;
    }
    return checkSignature(_prepareMsg);
}

bool PBFTEngine::handlePrepareMsg(PBFTMessageInterface::Ptr _prepareMsg)
{
    auto result = checkPBFTMsg(_prepareMsg);
    if (result == CheckResult::INVALID)
    {
        return false;
    }
    // valid prepareMessage
    m_cacheProcessor->addPrepareCache(_prepareMsg);
    if (result == CheckResult::FUTURE)
    {
        return true;
    }
    m_cacheProcessor->checkAndPreCommit();
    return true;
}

bool PBFTEngine::handleCommitMsg(PBFTMessageInterface::Ptr _commitMsg)
{
    auto result = checkPBFTMsg(_commitMsg);
    if (result == CheckResult::INVALID)
    {
        return false;
    }
    m_cacheProcessor->addCommitReq(_commitMsg);
    if (result == CheckResult::FUTURE)
    {
        return true;
    }
    m_cacheProcessor->checkAndCommit();
    return true;
}

void PBFTEngine::onTimeout()
{
    PBFT_LOG(WARNING) << LOG_DESC("onTimeout") << m_config->printCurrentState();
    // update toView
    m_config->incToView(1);
    // clear the viewchange cache
    m_cacheProcessor->removeInvalidViewChange();
    // broadcast viewchange and try to the new-view phase
    broadcastViewChangeReq();
}

void PBFTEngine::broadcastViewChangeReq()
{
    // broadcast the viewChangeReq
    auto committedProposal = m_config->populateCommittedProposal();
    if (committedProposal == nullptr)
    {
        PBFT_LOG(WARNING) << LOG_DESC(
            "broadcastViewChangeReq failed for the latest storage state has not been loaded.");
    }
    auto viewChangeReq = m_config->pbftMessageFactory()->createViewChangeMsg();
    viewChangeReq->setPacketType(PacketType::ViewChangePacket);
    viewChangeReq->setVersion(m_config->pbftMsgDefaultVersion());
    viewChangeReq->setView(m_config->view());
    viewChangeReq->setTimestamp(utcTime());
    viewChangeReq->setGeneratedFrom(m_config->nodeIndex());
    // set the committed proposal
    viewChangeReq->setCommittedProposal(committedProposal);
    // set prepared proposals
    viewChangeReq->setPreparedProposals(m_cacheProcessor->preCommitProposals());
    // encode and broadcast the viewchangeReq
    auto encodedData = m_config->codec()->encode(viewChangeReq);
    // only broadcast to the consensus nodes
    m_config->frontService()->asyncSendMessageByNodeIDs(
        ModuleID::PBFT, m_config->consensusNodeIDList(), ref(*encodedData));
    // collect the viewchangeReq
    m_cacheProcessor->addViewChangeReq(viewChangeReq);
    m_cacheProcessor->checkAndTryToNewView();
}

bool PBFTEngine::handleViewChangeMsg(ViewChangeMsgInterface::Ptr _viewChangeMsg)
{
    // check the proposal index
    if (_viewChangeMsg->committedProposal()->index() < m_config->committedProposal()->index())
    {
        PBFT_LOG(DEBUG) << LOG_DESC("InvalidViewChangeReq: invalid index")
                        << printPBFTMsgInfo(_viewChangeMsg) << m_config->printCurrentState();
        return false;
    }
    // TODO: catchup view
    // check the view
    if (_viewChangeMsg->view() <= m_config->view())
    {
        PBFT_LOG(DEBUG) << LOG_DESC("InvalidViewChangeReq: invalid view")
                        << printPBFTMsgInfo(_viewChangeMsg) << m_config->printCurrentState();
        return false;
    }
    // check the committed proposal hash
    if (_viewChangeMsg->committedProposal()->index() == m_config->committedProposal()->index() &&
        _viewChangeMsg->committedProposal()->hash() != m_config->committedProposal()->hash())
    {
        PBFT_LOG(DEBUG) << LOG_DESC("InvalidViewChangeReq: conflict with local committedProposal")
                        << LOG_DESC("received proposal")
                        << printPBFTProposal(_viewChangeMsg->committedProposal())
                        << LOG_DESC("local committedProposal")
                        << printPBFTProposal(m_config->committedProposal());
        return false;
    }
    // check the precommmitted proposals
    for (auto proposal : _viewChangeMsg->preparedProposals())
    {
        if (!checkPrecommitProposal(proposal))
        {
            PBFT_LOG(DEBUG) << LOG_DESC("InvalidViewChangeReq for invalid proposal")
                            << printPBFTProposal(proposal) << m_config->printCurrentState();
            return false;
        }
    }
    checkSignature(_viewChangeMsg);
    m_cacheProcessor->addViewChangeReq(_viewChangeMsg);
    m_cacheProcessor->checkAndTryToNewView();
    return true;
}

bool PBFTEngine::checkPrecommitProposal(std::shared_ptr<PBFTProposalInterface> _precommitProposal)
{
    // check the index of the proposal
    if (_precommitProposal->index() < m_config->progressedIndex())
    {
        return false;
    }
    // check the signature
    uint64_t weight = 0;
    auto proofSize = _precommitProposal->signatureProofSize();
    for (size_t i = 0; i < proofSize; i++)
    {
        auto proof = _precommitProposal->signatureProof(i);
        // check the proof
        auto nodeInfo = m_config->getConsensusNodeByIndex(proof.first);
        if (!nodeInfo)
        {
            return false;
        }
        // verify the signature
        auto ret = m_config->cryptoSuite()->signatureImpl()->verify(
            nodeInfo->nodeID(), _precommitProposal->hash(), proof.second);
        if (!ret)
        {
            return false;
        }
        weight += nodeInfo->weight();
    }
    // check the quorum
    return (weight >= m_config->minRequiredQuorum());
}

bool PBFTEngine::handleNewViewMsg(NewViewMsgInterface::Ptr)
{
    return true;
}