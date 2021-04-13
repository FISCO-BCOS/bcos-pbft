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
#include "core/ConsensusNode.h"

using namespace bcos;
using namespace bcos::consensus;

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

IndexType PBFTConfig::leaderIndex(bcos::protocol::BlockNumber _proposalIndex)
{
    return (_proposalIndex / m_leaderSwitchPeriod + m_view) % m_consensusNodeNum;
}