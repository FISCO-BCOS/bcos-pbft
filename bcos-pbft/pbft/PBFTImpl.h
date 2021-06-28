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
 * @file PBFTImpl.h
 * @author: yujiechen
 * @date 2021-05-17
 */
#pragma once
#include "engine/BlockValidator.h"
#include "engine/PBFTEngine.h"
#include <bcos-framework/interfaces/consensus/ConsensusInterface.h>
#include <bcos-framework/libtool/LedgerConfigFetcher.h>
namespace bcos
{
namespace consensus
{
class PBFTImpl : public ConsensusInterface
{
public:
    using Ptr = std::shared_ptr<PBFTImpl>;
    explicit PBFTImpl(PBFTEngine::Ptr _pbftEngine) : m_pbftEngine(_pbftEngine)
    {
        m_blockValidator = std::make_shared<BlockValidator>(m_pbftEngine->pbftConfig());
    }
    virtual ~PBFTImpl() { stop(); }

    void start() override
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

    void stop() override
    {
        if (!m_running)
        {
            PBFT_LOG(WARNING) << LOG_DESC("The PBFT module has already been stopped!");
            return;
        }
        m_pbftEngine->stop();
        m_running = false;
        PBFT_LOG(INFO) << LOG_DESC("Stop the PBFT module.");
    }

    void asyncSubmitProposal(bytesConstRef _proposalData,
        bcos::protocol::BlockNumber _proposalIndex, bcos::crypto::HashType const& _proposalHash,
        std::function<void(Error::Ptr)> _onProposalSubmitted) override
    {
        if (!m_running)
        {
            if (_onProposalSubmitted)
            {
                _onProposalSubmitted(std::make_shared<Error>(
                    -1, "The PBFT module has not been initialized finished!"));
            }
            return;
        }
        return m_pbftEngine->asyncSubmitProposal(
            _proposalData, _proposalIndex, _proposalHash, _onProposalSubmitted);
    }

    void asyncGetPBFTView(std::function<void(Error::Ptr, ViewType)> _onGetView) override
    {
        if (!m_running)
        {
            if (_onGetView)
            {
                _onGetView(std::make_shared<Error>(
                               -1, "The PBFT module has not been initialized finished!"),
                    0);
            }
            return;
        }
        auto view = m_pbftEngine->pbftConfig()->view();
        if (!_onGetView)
        {
            return;
        }
        _onGetView(nullptr, view);
    }

    void asyncNotifyConsensusMessage(bcos::Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID,
        bytesConstRef _data, std::function<void(bytesConstRef _respData)> _sendResponse,
        std::function<void(Error::Ptr _error)> _onRecv) override
    {
        if (!m_running)
        {
            if (_onRecv)
            {
                _onRecv(std::make_shared<Error>(
                    -1, "The PBFT module has not been initialized finished!"));
            }
            return;
        }
        m_pbftEngine->onReceivePBFTMessage(_error, _nodeID, _data, _sendResponse);
        if (!_onRecv)
        {
            return;
        }
        _onRecv(nullptr);
    }

    // the sync module calls this interface to check block
    void asyncCheckBlock(bcos::protocol::Block::Ptr _block,
        std::function<void(Error::Ptr, bool)> _onVerifyFinish) override
    {
        if (!m_running)
        {
            if (_onVerifyFinish)
            {
                _onVerifyFinish(std::make_shared<Error>(
                                    -1, "The PBFT module has not been initialized finished!"),
                    false);
            }
            return;
        }
        m_blockValidator->asyncCheckBlock(_block, _onVerifyFinish);
    }

    // the sync module calls this interface to notify new block
    void asyncNotifyNewBlock(bcos::ledger::LedgerConfig::Ptr _ledgerConfig,
        std::function<void(Error::Ptr)> _onRecv) override
    {
        if (!m_running)
        {
            if (_onRecv)
            {
                _onRecv(std::make_shared<Error>(
                    -1, "The PBFT module has not been initialized finished!"));
            }
            return;
        }

        m_pbftEngine->asyncNotifyNewBlock(_ledgerConfig, _onRecv);
    }

    void notifyHighestSyncingNumber(bcos::protocol::BlockNumber _blockNumber) override
    {
        m_pbftEngine->pbftConfig()->setSyncingHighestNumber(_blockNumber);
    }

    void asyncNoteUnSealedTxsSize(
        size_t _unsealedTxsSize, std::function<void(Error::Ptr)> _onRecvResponse) override
    {
        if (!m_running)
        {
            if (_onRecvResponse)
            {
                _onRecvResponse(std::make_shared<Error>(
                    -1, "The PBFT module has not been initialized finished!"));
            }
            return;
        }
        m_pbftEngine->pbftConfig()->setUnSealedTxsSize(_unsealedTxsSize);
        if (_onRecvResponse)
        {
            _onRecvResponse(nullptr);
        }
    }

    virtual void init(bcos::sync::BlockSyncInterface::Ptr _blockSync)
    {
        auto config = m_pbftEngine->pbftConfig();
        config->setBlockSync(_blockSync);
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
            PBFT_LOG(INFO) << LOG_DESC("init PBFT state")
                           << LOG_KV("stateProposals", stateProposals->size());
            auto consensusedProposalIndex = config->storage()->maxCommittedProposalIndex();
            config->setProgressedIndex(consensusedProposalIndex + 1);
            m_pbftEngine->initState(*stateProposals);
        }
        config->timer()->start();
        PBFT_LOG(INFO) << LOG_DESC("init PBFT success");
    }

    void setLedgerFetcher(bcos::tool::LedgerConfigFetcher::Ptr _ledgerFetcher)
    {
        m_ledgerFetcher = _ledgerFetcher;
    }
    PBFTEngine::Ptr pbftEngine() { return m_pbftEngine; }

protected:
    PBFTEngine::Ptr m_pbftEngine;
    BlockValidator::Ptr m_blockValidator;
    bcos::tool::LedgerConfigFetcher::Ptr m_ledgerFetcher;

    std::atomic_bool m_running = {false};
};
}  // namespace consensus
}  // namespace bcos