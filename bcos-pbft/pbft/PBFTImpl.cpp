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
 * @file PBFTImpl.cpp
 * @author: yujiechen
 * @date 2021-05-17
 */
#include "PBFTImpl.h"

using namespace bcos;
using namespace bcos::consensus;

void PBFTImpl::start()
{
    if (m_running)
    {
        PBFT_LOG(WARNING) << LOG_DESC("The PBFT module has already been started!");
        return;
    }
    m_pbftEngine->start();
    m_running = true;
    PBFT_LOG(INFO) << LOG_DESC("Start the PBFT module.");
}

void PBFTImpl::stop()
{
    if (!m_running)
    {
        PBFT_LOG(WARNING) << LOG_DESC("The PBFT module has already been stopped!");
        return;
    }
    m_blockValidator->stop();
    m_pbftEngine->stop();
    m_running = false;
    PBFT_LOG(INFO) << LOG_DESC("Stop the PBFT module.");
}

void PBFTImpl::asyncSubmitProposal(bytesConstRef _proposalData,
    bcos::protocol::BlockNumber _proposalIndex, bcos::crypto::HashType const& _proposalHash,
    std::function<void(Error::Ptr)> _onProposalSubmitted)
{
    return m_pbftEngine->asyncSubmitProposal(
        _proposalData, _proposalIndex, _proposalHash, _onProposalSubmitted);
}

void PBFTImpl::asyncGetPBFTView(std::function<void(Error::Ptr, ViewType)> _onGetView)
{
    auto view = m_pbftEngine->pbftConfig()->view();
    if (!_onGetView)
    {
        return;
    }
    _onGetView(nullptr, view);
}

void PBFTImpl::asyncNotifyConsensusMessage(bcos::Error::Ptr _error, std::string const& _id,
    bcos::crypto::NodeIDPtr _nodeID, bytesConstRef _data,
    std::function<void(Error::Ptr _error)> _onRecv)
{
    m_pbftEngine->onReceivePBFTMessage(_error, _id, _nodeID, _data);
    if (!_onRecv)
    {
        return;
    }
    _onRecv(nullptr);
}

// the sync module calls this interface to check block
void PBFTImpl::asyncCheckBlock(
    bcos::protocol::Block::Ptr _block, std::function<void(Error::Ptr, bool)> _onVerifyFinish)
{
    m_blockValidator->asyncCheckBlock(_block, _onVerifyFinish);
}

// the sync module calls this interface to notify new block
void PBFTImpl::asyncNotifyNewBlock(
    bcos::ledger::LedgerConfig::Ptr _ledgerConfig, std::function<void(Error::Ptr)> _onRecv)
{
    m_pbftEngine->asyncNotifyNewBlock(_ledgerConfig, _onRecv);
}

void PBFTImpl::notifyHighestSyncingNumber(bcos::protocol::BlockNumber _blockNumber)
{
    m_pbftEngine->pbftConfig()->setSyncingHighestNumber(_blockNumber);
}

void PBFTImpl::asyncNoteUnSealedTxsSize(
    size_t _unsealedTxsSize, std::function<void(Error::Ptr)> _onRecvResponse)
{
    m_pbftEngine->pbftConfig()->setUnSealedTxsSize(_unsealedTxsSize);
    if (_onRecvResponse)
    {
        _onRecvResponse(nullptr);
    }
}

void PBFTImpl::init()
{
    auto config = m_pbftEngine->pbftConfig();
    config->validator()->init();
    PBFT_LOG(INFO) << LOG_DESC("fetch LedgerConfig information");

    m_ledgerFetcher->fetchBlockNumberAndHash();
    m_ledgerFetcher->fetchConsensusNodeList();
    m_ledgerFetcher->fetchConsensusTimeout();
    m_ledgerFetcher->fetchBlockTxCountLimit();
    m_ledgerFetcher->fetchConsensusLeaderPeriod();
    m_ledgerFetcher->waitFetchFinished();
    auto ledgerConfig = m_ledgerFetcher->ledgerConfig();
    PBFT_LOG(INFO) << LOG_DESC("fetch LedgerConfig information success")
                   << LOG_KV("blockNumber", ledgerConfig->blockNumber())
                   << LOG_KV("hash", ledgerConfig->hash().abridged())
                   << LOG_KV("consensusTimeout", ledgerConfig->consensusTimeout())
                   << LOG_KV("maxTxsPerBlock", ledgerConfig->blockTxCountLimit())
                   << LOG_KV("consensusNodeList", ledgerConfig->consensusNodeList().size());
    config->resetConfig(ledgerConfig);

    PBFT_LOG(INFO) << LOG_DESC("fetch PBFT state");
    auto stateProposals = config->storage()->loadState(ledgerConfig->blockNumber());
    if (stateProposals && stateProposals->size() > 0)
    {
        m_pbftEngine->initState(*stateProposals, config->keyPair()->publicKey());
        auto lowWaterMarkIndex = stateProposals->size() - 1;
        auto lowWaterMark = ((*stateProposals)[lowWaterMarkIndex])->index();
        config->setLowWaterMark(lowWaterMark);
        PBFT_LOG(INFO) << LOG_DESC("init PBFT state")
                       << LOG_KV("stateProposals", stateProposals->size())
                       << LOG_KV("lowWaterMark", lowWaterMark)
                       << LOG_KV("highWaterMark", config->highWaterMark());
    }
    config->timer()->start();
    PBFT_LOG(INFO) << LOG_DESC("init PBFT success");
}