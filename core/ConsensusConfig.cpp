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
 * @brief Base implementation of consensus Config
 * @file ConsensusConfig.cpp
 * @author: yujiechen
 * @date 2021-04-09
 */
#include "ConsensusConfig.h"

using namespace bcos;
using namespace bcos::crypto;
using namespace bcos::consensus;
// the consensus node list
ConsensusNodeList const& ConsensusConfig::consensusNodeList() const
{
    ReadGuard l(x_consensusNodeList);
    return *m_consensusNodeList;
}

void ConsensusConfig::setConsensusNodeList(ConsensusNodeList& _consensusNodeList)
{
    std::sort(_consensusNodeList.begin(), _consensusNodeList.end());
    bool updated = false;
    // update the consensus list
    {
        UpgradableGuard l(x_consensusNodeList);
        if (_consensusNodeList == *m_consensusNodeList)
        {
            return;
        }
        UpgradeGuard ul(l);
        // consensus node list have been changed
        *m_consensusNodeList = _consensusNodeList;
        updated = true;
    }
    // update the sealersNum
    if (!updated)
    {
        return;
    }
    {
        // update the consensusNodeNum
        ReadGuard l(x_consensusNodeList);
        m_consensusNodeNum.store(m_consensusNodeList->size());
    }
    // update the nodeIndex
    auto nodeIndex = getNodeIndexByNodeID(m_keyPair->publicKey());
    if (nodeIndex != m_nodeIndex)
    {
        m_nodeIndex.store(nodeIndex);
    }
    // update quorum
    updateQuorum();
}

IndexType ConsensusConfig::getNodeIndexByNodeID(bcos::crypto::PublicPtr _nodeID)
{
    ReadGuard l(x_consensusNodeList);
    IndexType nodeIndex = NON_CONSENSUS_NODE;
    IndexType i = 0;
    for (auto _consensusNode : *m_consensusNodeList)
    {
        if (_consensusNode->nodeID()->data() == _nodeID->data())
        {
            nodeIndex = i;
            break;
        }
        i++;
    }
    return nodeIndex;
}

ConsensusNodeInterface::Ptr ConsensusConfig::getConsensusNodeByIndex(IndexType _nodeIndex)
{
    ReadGuard l(x_consensusNodeList);
    return (*m_consensusNodeList)[_nodeIndex];
}