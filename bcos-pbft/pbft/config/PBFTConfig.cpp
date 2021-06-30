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
                   << LOG_KV("committedIndex", _ledgerConfig->blockNumber())
                   << LOG_KV("propHash", _ledgerConfig->hash().abridged())
                   << LOG_KV("consensusTimeout", _ledgerConfig->consensusTimeout())
                   << LOG_KV("blockCountLimit", _ledgerConfig->blockTxCountLimit())
                   << LOG_KV("leaderPeriod", _ledgerConfig->leaderSwitchPeriod());
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
    // set leader_period
    setLeaderSwitchPeriod(_ledgerConfig->leaderSwitchPeriod());
    // reset the timer
    resetTimer();

    if (_ledgerConfig->sealerId() == -1)
    {
        PBFT_LOG(DEBUG) << LOG_DESC("^^^^^^^^Report") << printCurrentState();
    }
    else
    {
        PBFT_LOG(DEBUG) << LOG_DESC("^^^^^^^^Report") << LOG_KV("sealer", _ledgerConfig->sealerId())
                        << printCurrentState();
    }
    // notify the latest block number to the sealer
    if (m_stateNotifier)
    {
        m_stateNotifier(_ledgerConfig->blockNumber());
    }

    if (m_syncingHighestNumber > _ledgerConfig->blockNumber())
    {
        m_syncingState = true;
        return;
    }
    // try to reach a new view after syncing completed
    if (m_syncingState)
    {
        m_syncingState = false;
        m_timer->start();
    }
    notifySealer(m_expectedCheckPoint);
    if (!m_newBlockNotifier)
    {
        return;
    }
    m_newBlockNotifier(_ledgerConfig, [_ledgerConfig](Error::Ptr _error) {
        if (_error)
        {
            PBFT_LOG(WARNING) << LOG_DESC("asyncNotifyNewBlock to sync module failed")
                              << LOG_KV("number", _ledgerConfig->blockNumber())
                              << LOG_KV("hash", _ledgerConfig->hash().abridged())
                              << LOG_KV("code", _error->errorCode())
                              << LOG_KV("msg", _error->errorMessage());
        }
    });
}

void PBFTConfig::notifySealer(BlockNumber _progressedIndex, bool _enforce)
{
    auto currentLeader = leaderIndex(_progressedIndex);
    if (currentLeader != nodeIndex())
    {
        return;
    }
    if (_enforce)
    {
        asyncNotifySealProposal(_progressedIndex, _progressedIndex, blockTxCountLimit());
        m_sealEndIndex = std::max(_progressedIndex, m_sealEndIndex.load());
        m_sealStartIndex = std::min(_progressedIndex, m_sealStartIndex.load());
        return;
    }
    int64_t endProposalIndex =
        (_progressedIndex / m_leaderSwitchPeriod + 1) * m_leaderSwitchPeriod - 1;
    endProposalIndex = std::min(endProposalIndex, highWaterMark());
    if (m_sealEndIndex >= endProposalIndex)
    {
        return;
    }
    auto startSealIndex = std::max(m_sealStartIndex.load(), _progressedIndex);
    if (startSealIndex > endProposalIndex)
    {
        return;
    }
    asyncNotifySealProposal(startSealIndex, endProposalIndex, blockTxCountLimit());

    m_sealStartIndex = startSealIndex;
    m_sealEndIndex = endProposalIndex;


    PBFT_LOG(INFO) << LOG_DESC("notifySealer: notify the new leader to seal block")
                   << LOG_KV("idx", nodeIndex()) << LOG_KV("startIndex", startSealIndex)
                   << LOG_KV("endIndex", endProposalIndex)
                   << LOG_KV("maxTxsToSeal", blockTxCountLimit());
}

void PBFTConfig::asyncNotifySealProposal(
    size_t _proposalIndex, size_t _proposalEndIndex, size_t _maxTxsToSeal, size_t _retryTime)
{
    if (!m_sealProposalNotifier)
    {
        return;
    }
    if (_retryTime > 3)
    {
        return;
    }
    auto self = std::weak_ptr<PBFTConfig>(shared_from_this());
    m_sealProposalNotifier(_proposalIndex, _proposalEndIndex, _maxTxsToSeal,
        [_proposalIndex, _proposalEndIndex, _maxTxsToSeal, self, _retryTime](Error::Ptr _error) {
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
                // retry after 1 seconds
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                // retry when send failed
                pbftConfig->asyncNotifySealProposal(
                    _proposalIndex, _proposalEndIndex, _maxTxsToSeal, _retryTime + 1);
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
    auto expectedLeader = leaderIndexInNewViewPeriod(m_toView);
    return (m_nodeIndex == expectedLeader);
}

IndexType PBFTConfig::leaderIndexInNewViewPeriod(ViewType _view)
{
    return (m_progressedIndex / m_leaderSwitchPeriod + _view) % m_consensusNodeNum;
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
        stringstream << LOG_DESC("The storage has not been init.");
        return stringstream.str();
    }
    stringstream << LOG_KV("committedIndex", committedProposal()->index())
                 << LOG_KV("consNum", progressedIndex())
                 << LOG_KV("committedHash", committedProposal()->hash().abridged())
                 << LOG_KV("view", view()) << LOG_KV("toView", toView())
                 << LOG_KV("changeCycle", m_timer->changeCycle())
                 << LOG_KV("expectedCheckPoint", m_expectedCheckPoint) << LOG_KV("Idx", nodeIndex())
                 << LOG_KV("nodeId", nodeID()->shortHex());
    return stringstream.str();
}