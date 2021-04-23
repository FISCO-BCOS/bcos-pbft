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
 * @brief cache for the PBFTReq
 * @file PBFTReqCache.cpp
 * @author: yujiechen
 * @date 2021-04-21
 */
#include "PBFTReqCache.h"

using namespace bcos;
using namespace bcos::consensus;
// Note: please ensure the passed in _prePrepareMsg not be modified after addPrePrepareCache
void PBFTReqCache::addPrePrepareCache(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    WriteGuard l(x_prePrepareCache);
    m_prePrepareCache = _prePrepareMsg;
    PBFT_LOG(INFO) << LOG_DESC("addPrePrepareCache") << printPBFTMsgInfo(_prePrepareMsg);
}

bool PBFTReqCache::existPrePrepare(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    ReadGuard l(x_prePrepareCache);
    if (m_prePrepareCache == nullptr)
    {
        return false;
    }
    return (_prePrepareMsg->hash() == m_prePrepareCache->hash()) &&
           (m_prePrepareCache->view() >= _prePrepareMsg->view());
}

bool PBFTReqCache::conflictWithProcessedReq(PBFTMessageInterface::Ptr _msg)
{
    ReadGuard l(x_prePrepareCache);
    if (m_prePrepareCache == nullptr)
    {
        return false;
    }
    return (_msg->hash() != m_prePrepareCache->hash() || _msg->view() != m_prePrepareCache->view());
}

bool PBFTReqCache::conflictWithPrecommitProposal(PBFTMessageInterface::Ptr _prePrepareMsg)
{
    for (auto precommitProposal : m_precommitProposalList)
    {
        if (precommitProposal->conflictWithPrecommitProposal(_prePrepareMsg))
        {
            return true;
        }
    }
    return false;
}

void PBFTReqCache::addCache(PBFTProposalListType& _proposalCache,
    PBFTMessageInterface::Ptr _pbftReq, UpdatePBFTCacheHandler _handler)

{
    auto const& msgHash = _pbftReq->hash();
    auto const& proposals = _pbftReq->proposals();
    for (auto pbftProposal : _pbftReq->proposals())
    {
        auto index = pbftProposal->index();
        if (!_proposalCache.count(msgHash) || !(_proposalCache[msgHash].count(index)))
        {
            _proposalCache[msgHash][index] = std::make_shared<PBFTProposalCache>(m_config, index);
        }
        pbftProposal->setView(_pbftReq->view());
        pbftProposal->setGeneratedFrom(_pbftReq->generatedFrom());
        _handler(_proposalCache[msgHash][index], pbftProposal);
    }
}