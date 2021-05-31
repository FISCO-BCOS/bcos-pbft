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
 * @brief config for PBFT
 * @file PBFTConfig.h
 * @author: yujiechen
 * @date 2021-04-12
 */
#pragma once
#include "../../core/ConsensusConfig.h"
#include "../../framework/StateMachineInterface.h"
#include "../engine/PBFTTimer.h"
#include "../engine/Validator.h"
#include "../interfaces/PBFTCodecInterface.h"
#include "../interfaces/PBFTMessageFactory.h"
#include "../interfaces/PBFTStorage.h"
#include "../utilities/Common.h"
#include <bcos-framework/interfaces/crypto/CryptoSuite.h>
#include <bcos-framework/interfaces/front/FrontServiceInterface.h>
#include <bcos-framework/interfaces/sealer/SealerInterface.h>

namespace bcos
{
namespace consensus
{
const IndexType InvalidNodeIndex = -1;
class PBFTConfig : public ConsensusConfig, public std::enable_shared_from_this<PBFTConfig>
{
public:
    using Ptr = std::shared_ptr<PBFTConfig>;
    PBFTConfig(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair,
        std::shared_ptr<PBFTMessageFactory> _pbftMessageFactory,
        std::shared_ptr<PBFTCodecInterface> _codec, std::shared_ptr<ValidatorInterface> _validator,
        std::shared_ptr<bcos::front::FrontServiceInterface> _frontService,
        bcos::sealer::SealerInterface::Ptr _sealer, StateMachineInterface::Ptr _stateMachine,
        PBFTStorage::Ptr _storage)
      : ConsensusConfig(_keyPair)
    {
        m_cryptoSuite = _cryptoSuite;
        m_pbftMessageFactory = _pbftMessageFactory;
        m_codec = _codec;
        m_validator = _validator;
        m_frontService = _frontService;
        m_sealer = _sealer;
        m_stateMachine = _stateMachine;
        m_storage = _storage;
        m_storage->registerConfigResetHandler(
            [this](bcos::ledger::LedgerConfig::Ptr _ledgerConfig) { resetConfig(_ledgerConfig); });
        m_timer = std::make_shared<PBFTTimer>(consensusTimeout());
    }

    ~PBFTConfig() override {}

    virtual void resetConfig(bcos::ledger::LedgerConfig::Ptr _ledgerConfig);

    uint64_t minRequiredQuorum() const override;

    virtual ViewType view() const { return m_view; }
    virtual void setView(ViewType _view) { m_view.store(_view); }

    virtual ViewType toView() const { return m_toView; }
    virtual void setToView(ViewType _toView) { m_toView.store(_toView); }
    virtual void incToView(ViewType _increasedValue) { m_toView += _increasedValue; }

    virtual IndexType leaderIndex(bcos::protocol::BlockNumber _proposalIndex);
    virtual bool leaderAfterViewChange();
    virtual uint64_t leaderSwitchPeriod() const { return m_leaderSwitchPeriod; }
    virtual void setLeaderSwitchPeriod(uint64_t _leaderSwitchPeriod)
    {
        if (_leaderSwitchPeriod == m_leaderSwitchPeriod)
        {
            return;
        }
        m_leaderSwitchPeriod.store(_leaderSwitchPeriod);
        m_leaderSwitchPeriodUpdated = true;
    }
    bcos::crypto::CryptoSuite::Ptr cryptoSuite() { return m_cryptoSuite; }
    std::shared_ptr<PBFTMessageFactory> pbftMessageFactory() { return m_pbftMessageFactory; }
    std::shared_ptr<bcos::front::FrontServiceInterface> frontService() { return m_frontService; }
    std::shared_ptr<PBFTCodecInterface> codec() { return m_codec; }

    PBFTProposalInterface::Ptr populateCommittedProposal();
    unsigned pbftMsgDefaultVersion() const { return c_pbftMsgDefaultVersion; }
    unsigned networkTimeoutInterval() const { return c_networkTimeoutInterval; }
    std::shared_ptr<ValidatorInterface> validator() { return m_validator; }
    PBFTStorage::Ptr storage() { return m_storage; }

    std::string printCurrentState();
    int64_t highWaterMark() { return m_progressedIndex + m_warterMarkLimit; }

    PBFTTimer::Ptr timer() { return m_timer; }

    void setConsensusTimeout(uint64_t _consensusTimeout) override
    {
        ConsensusConfig::setConsensusTimeout(_consensusTimeout);
        m_timer->reset(_consensusTimeout);
    }

    void setCommittedProposal(ProposalInterface::Ptr _committedProposal) override
    {
        ConsensusConfig::setCommittedProposal(_committedProposal);
        if (_committedProposal->index() + 1 > m_expectedCheckPoint)
        {
            PBFT_LOG(DEBUG) << LOG_DESC("PBFTConfig: resetExpectedCheckPoint")
                            << LOG_KV("expectedCheckPoint", m_expectedCheckPoint);
            m_expectedCheckPoint = _committedProposal->index() + 1;
        }
    }

    int64_t expectedCheckPoint() { return m_expectedCheckPoint; }
    void setExpectedCheckPoint(bcos::protocol::BlockNumber _expectedCheckPoint)
    {
        m_expectedCheckPoint = std::max(committedProposal()->index() + 1, _expectedCheckPoint);
        PBFT_LOG(DEBUG) << LOG_DESC("PBFTConfig: setExpectedCheckPoint")
                        << LOG_KV("expectedCheckPoint", m_expectedCheckPoint);
    }

    StateMachineInterface::Ptr stateMachine() { return m_stateMachine; }

    bcos::sealer::SealerInterface::Ptr sealer() { return m_sealer; }

    int64_t warterMarkLimit() const { return m_warterMarkLimit; }
    void setWarterMarkLimit(int64_t _warterMarkLimit) { m_warterMarkLimit = _warterMarkLimit; }

protected:
    void updateQuorum() override;
    virtual void notifySealer(bcos::protocol::BlockNumber _committedIndex, uint64_t _maxTxsToSeal);
    virtual void asyncNotifySealProposal(
        size_t _proposalIndex, size_t _proposalEndIndex, size_t _maxTxsToSeal);

private:
    bcos::crypto::CryptoSuite::Ptr m_cryptoSuite;
    // Factory for creating PBFT message package
    std::shared_ptr<PBFTMessageFactory> m_pbftMessageFactory;
    // Codec for serialization/deserialization of PBFT message packets
    std::shared_ptr<PBFTCodecInterface> m_codec;
    // Proposal validator
    std::shared_ptr<ValidatorInterface> m_validator;
    // FrontService, used to send/receive P2P message packages
    std::shared_ptr<bcos::front::FrontServiceInterface> m_frontService;
    bcos::sealer::SealerInterface::Ptr m_sealer;
    StateMachineInterface::Ptr m_stateMachine;
    PBFTStorage::Ptr m_storage;
    // Timer
    PBFTTimer::Ptr m_timer;
    std::atomic_bool m_leaderSwitchPeriodUpdated = {false};

    std::atomic<uint64_t> m_maxFaultyQuorum = {0};
    std::atomic<uint64_t> m_totalQuorum = {0};
    std::atomic<uint64_t> m_minRequiredQuorum = {0};
    std::atomic<ViewType> m_view = {0};
    std::atomic<ViewType> m_toView = {0};
    // invalid leaderIndex
    std::atomic<IndexType> m_leaderIndex = InvalidNodeIndex;

    std::atomic<bcos::protocol::BlockNumber> m_expectedCheckPoint = {0};

    int64_t m_warterMarkLimit = 10;
    std::atomic<uint64_t> m_leaderSwitchPeriod = {1};
    const unsigned c_pbftMsgDefaultVersion = 0;
    const unsigned c_networkTimeoutInterval = 1000;
};
}  // namespace consensus
}  // namespace bcos
