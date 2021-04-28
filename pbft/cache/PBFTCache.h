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
 * @brief cache for the consensus state of the proposal
 * @file PBFTCache.h
 * @author: yujiechen
 * @date 2021-04-23
 */
#pragma once
#include "pbft/config/PBFTConfig.h"
#include "pbft/interfaces/PBFTMessageInterface.h"

namespace bcos
{
namespace consensus
{
class PBFTCache
{
public:
    using Ptr = std::shared_ptr<PBFTCache>;
    PBFTCache(PBFTConfig::Ptr _config, bcos::protocol::BlockNumber _index);
    virtual ~PBFTCache() {}
    virtual void addPrepareCache(PBFTProposalInterface::Ptr _prepareProposal)
    {
        addCache(m_prepareCacheList, m_prepareReqWeight, _prepareProposal);
    }

    virtual void addCommitCache(PBFTProposalInterface::Ptr _commitProposal)
    {
        addCache(m_commitCacheList, m_commitReqWeight, _commitProposal);
    }

    virtual void addPrePrepareCache(PBFTProposalInterface::Ptr _prePrepareProposal)
    {
        m_prePrepare = _prePrepareProposal;
    }

    bcos::protocol::BlockNumber index() const { return m_index; }

    bool collectEnoughPrepareReq()
    {
        if (!checkPrePrepareProposalStatus())
        {
            return false;
        }
        return collectEnoughQuorum(m_prePrepare->hash(), m_prepareReqWeight);
    }

    bool collectEnoughCommitReq()
    {
        if (!checkPrePrepareProposalStatus())
        {
            return false;
        }
        return collectEnoughQuorum(m_prePrepare->hash(), m_commitReqWeight);
    }
    virtual void intoPrecommit()
    {
        m_precommit->setGeneratedFrom(m_config->nodeIndex());
        m_precommit = m_prePrepare;
        m_config->storage()->storePrecommitProposal(m_precommit);
    }

    virtual PBFTProposalInterface::Ptr preCommitProposal() { return m_precommit; }
    virtual void setSignatureList();

private:
    inline bool checkPrePrepareProposalStatus()
    {
        if (m_prePrepare == nullptr)
        {
            return false;
        }
        if (m_prePrepare->view() != m_config->view())
        {
            return false;
        }
        return true;
    }

    using CollectionCacheType =
        std::map<bcos::crypto::HashType, std::map<IndexType, PBFTProposalInterface::Ptr>>;
    using QuorumRecoderType = std::map<bcos::crypto::HashType, uint64_t>;
    void addCache(CollectionCacheType& _cachedReq, QuorumRecoderType& _weightInfo,
        PBFTProposalInterface::Ptr _proposal);

    bool collectEnoughQuorum(bcos::crypto::HashType const& _hash, QuorumRecoderType& _weightInfo)
    {
        if (!_weightInfo.count(_hash))
        {
            return false;
        }
        return (_weightInfo[_hash] >= m_config->minRequiredQuorum());
    }

private:
    PBFTConfig::Ptr m_config;
    std::atomic<bcos::protocol::BlockNumber> m_index;
    CollectionCacheType m_prepareCacheList;
    QuorumRecoderType m_prepareReqWeight;

    CollectionCacheType m_commitCacheList;
    QuorumRecoderType m_commitReqWeight;

    PBFTProposalInterface::Ptr m_prePrepare = nullptr;
    PBFTProposalInterface::Ptr m_precommit = nullptr;
};
}  // namespace consensus
}  // namespace bcos
