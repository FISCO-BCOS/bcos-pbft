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
#include "core/ConsensusConfig.h"
#include "pbft/engine/Validator.h"
#include "pbft/interfaces/PBFTCodecInterface.h"
#include "pbft/interfaces/PBFTMessageFactory.h"
#include "pbft/interfaces/PBFTStorage.h"
#include "pbft/utilities/Common.h"
#include <bcos-framework/interfaces/crypto/CryptoSuite.h>
#include <bcos-framework/interfaces/front/FrontServiceInterface.h>

namespace bcos
{
namespace consensus
{
class PBFTConfig : public ConsensusConfig
{
public:
    using Ptr = std::shared_ptr<PBFTConfig>;
    PBFTConfig(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair,
        std::shared_ptr<PBFTMessageFactory> _pbftMessageFactory,
        std::shared_ptr<PBFTCodecInterface> _codec, std::shared_ptr<ValidatorInterface> _validator,
        std::shared_ptr<bcos::front::FrontServiceInterface> _frontService,
        PBFTStorage::Ptr _storage, bool _needVerifyProposal)
      : ConsensusConfig(_keyPair)
    {
        m_cryptoSuite = _cryptoSuite;
        m_pbftMessageFactory = _pbftMessageFactory;
        m_codec = _codec;
        m_validator = _validator;
        m_frontService = _frontService;
        m_storage = _storage;
        m_needVerifyProposal = _needVerifyProposal;
    }

    ~PBFTConfig() override {}

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
        m_leaderSwitchPeriod.store(_leaderSwitchPeriod);
    }
    bcos::crypto::CryptoSuite::Ptr cryptoSuite() { return m_cryptoSuite; }
    std::shared_ptr<PBFTMessageFactory> pbftMessageFactory() { return m_pbftMessageFactory; }
    std::shared_ptr<bcos::front::FrontServiceInterface> frontService() { return m_frontService; }
    std::shared_ptr<PBFTCodecInterface> codec() { return m_codec; }

    PBFTProposalInterface::Ptr populateCommittedProposal();
    unsigned pbftMsgDefaultVersion() const { return c_pbftMsgDefaultVersion; }
    std::shared_ptr<ValidatorInterface> validator() { return m_validator; }
    PBFTStorage::Ptr storage() { return m_storage; }
    bool needVerifyProposal() const { return m_needVerifyProposal; }

    std::string printCurrentState();

protected:
    void updateQuorum() override;

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
    PBFTStorage::Ptr m_storage;
    // Flag, used to indicate whether Proposal needs to be verified
    bool m_needVerifyProposal;

    std::atomic<uint64_t> m_maxFaultyQuorum;
    std::atomic<uint64_t> m_totalQuorum;
    std::atomic<uint64_t> m_minRequiredQuorum;
    std::atomic<ViewType> m_view = {0};
    std::atomic<ViewType> m_toView = {0};

    std::atomic<uint64_t> m_leaderSwitchPeriod = {1};
    const unsigned c_pbftMsgDefaultVersion = 0;
};
}  // namespace consensus
}  // namespace bcos
