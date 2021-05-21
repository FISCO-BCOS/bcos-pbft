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
 * @file PBFTConfig.cpp
 * @author: yujiechen
 * @date 2021-04-12
 */
#include "PBFTConfig.h"

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::protocol;
using namespace bcos::ledger;

void PBFTConfig::resetConfig(LedgerConfig::Ptr _ledgerConfig)
{
    PBFT_LOG(INFO) << LOG_DESC("resetConfig")
                   << LOG_KV("committedProp", _ledgerConfig->blockNumber())
                   << LOG_KV("propHash", _ledgerConfig->hash().abridged())
                   << LOG_KV("consensusTimeout", _ledgerConfig->consensusTimeout())
                   << LOG_KV("blockCountLimit", _ledgerConfig->blockTxCountLimit());
    // set committed proposal
    auto committedProposal = m_pbftMessageFactory->createPBFTProposal();
    committedProposal->setIndex(_ledgerConfig->blockNumber());
    committedProposal->setHash(_ledgerConfig->hash());
    setCommittedProposal(committedProposal);
    // set consensusTimeout
    setConsensusTimeout(_ledgerConfig->consensusTimeout());
    // set blockTxCountLimit
    setBlockTxCountLimit(_ledgerConfig->blockTxCountLimit());
    // set ConsensusNodeList
    auto& consensusList = _ledgerConfig->mutableConsensusNodeList();
    setConsensusNodeList(consensusList);
    // stop the timer
    m_timer->stop();
    notifySealer(_ledgerConfig->blockNumber(), _ledgerConfig->blockTxCountLimit());
}

void PBFTConfig::notifySealer(BlockNumber _committedIndex, uint64_t _maxTxsToSeal)
{
    auto leaderIdx = leaderIndex(_committedIndex);
    if (m_leaderIndex != leaderIdx || m_leaderIndex == InvalidNodeIndex)
    {
        if (m_leaderIndex == nodeIndex())
        {
            auto endProposalIndex = (leaderIdx + 1) * m_leaderSwitchPeriod - 1;
            PBFT_LOG(INFO) << LOG_DESC(
                                  "setCommittedProposal for new block submitted, notify the new "
                                  "leader to seal block")
                           << LOG_KV("preLeader", m_leaderIndex) << LOG_KV("curLeader", leaderIdx)
                           << LOG_KV("startIndex", m_progressedIndex)
                           << LOG_KV("endIndex", endProposalIndex)
                           << LOG_KV("maxTxsToSeal", _maxTxsToSeal);
            asyncNotifySealProposal(m_progressedIndex, endProposalIndex, _maxTxsToSeal);
        }

        m_leaderIndex = leaderIdx;
    }
}

void PBFTConfig::asyncNotifySealProposal(
    size_t _proposalIndex, size_t _proposalEndIndex, size_t _maxTxsToSeal)
{
    auto self = std::weak_ptr<PBFTConfig>(shared_from_this());
    m_sealer->asyncNotifySealProposal(_proposalIndex, _proposalEndIndex, _maxTxsToSeal,
        [_proposalIndex, _proposalEndIndex, _maxTxsToSeal, self](Error::Ptr _error) {
            if (_error == nullptr)
            {
                PBFT_LOG(INFO) << LOG_DESC("asyncNotifySealProposal success")
                               << LOG_KV("startIndex", _proposalIndex)
                               << LOG_KV("endIndex", _proposalEndIndex)
                               << LOG_KV("maxTxsToSeal", _maxTxsToSeal);
                return;
            }
            try
            {
                auto pbftConfig = self.lock();
                if (!pbftConfig)
                {
                    return;
                }
                // retry when send failed
                pbftConfig->asyncNotifySealProposal(
                    _proposalIndex, _proposalEndIndex, _maxTxsToSeal);
            }
            catch (std::exception const& e)
            {
                PBFT_LOG(WARNING) << LOG_DESC("asyncNotifySealProposal exception")
                                  << LOG_KV("error", boost::diagnostic_information(e));
            }
        });
}

uint64_t PBFTConfig::minRequiredQuorum() const
{
    return m_minRequiredQuorum;
}

void PBFTConfig::updateQuorum()
{
    m_totalQuorum.store(0);
    ReadGuard l(x_consensusNodeList);
    for (auto consensusNode : *m_consensusNodeList)
    {
        m_totalQuorum += consensusNode->weight();
    }
    // get m_maxFaultyQuorum
    m_maxFaultyQuorum = (m_totalQuorum - 1) / 3;
    m_minRequiredQuorum = m_totalQuorum - m_maxFaultyQuorum;
}

IndexType PBFTConfig::leaderIndex(BlockNumber _proposalIndex)
{
    return (_proposalIndex / m_leaderSwitchPeriod + m_view) % m_consensusNodeNum;
}

bool PBFTConfig::leaderAfterViewChange()
{
    auto expectedLeader =
        (m_progressedIndex / m_leaderSwitchPeriod + m_toView) % m_consensusNodeNum;
    return (m_nodeIndex == expectedLeader);
}

PBFTProposalInterface::Ptr PBFTConfig::populateCommittedProposal()
{
    ReadGuard l(x_committedProposal);
    if (!m_committedProposal)
    {
        return nullptr;
    }
    return m_pbftMessageFactory->populateFrom(
        std::dynamic_pointer_cast<PBFTProposalInterface>(m_committedProposal));
}

std::string PBFTConfig::printCurrentState()
{
    std::ostringstream stringstream;
    if (!committedProposal())
    {
        stringstream << LOG_DESC("The storage has not been intited.");
        return stringstream.str();
    }

    stringstream << LOG_KV("consNum", progressedIndex())
                 << LOG_KV("committedHash", committedProposal()->hash().abridged())
                 << LOG_KV("committedIndex", committedProposal()->index()) << LOG_KV("view", view())
                 << LOG_KV("Idx", nodeIndex()) << LOG_KV("nodeId", nodeID()->shortHex());
    return stringstream.str();
}