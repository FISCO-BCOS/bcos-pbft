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
    bool existPrePrepare(PBFTMessageInterface::Ptr _prePrepareMsg)
    {
        if (!m_prePrepare)
        {
            return false;
        }
        return (_prePrepareMsg->hash() == m_prePrepare->hash()) &&
               (m_prePrepare->view() >= _prePrepareMsg->view());
    }

    bool conflictWithProcessedReq(PBFTMessageInterface::Ptr _msg)
    {
        if (!m_prePrepare)
        {
            return false;
        }
        return (_msg->hash() != m_prePrepare->hash() || _msg->view() < m_prePrepare->view());
    }

    bool conflictWithPrecommitReq(PBFTMessageInterface::Ptr _prePrepareMsg);

    virtual void addPrepareCache(PBFTMessageInterface::Ptr _prepareProposal)
    {
        addCache(m_prepareCacheList, m_prepareReqWeight, _prepareProposal);
    }

    virtual void addCommitCache(PBFTMessageInterface::Ptr _commitProposal)
    {
        addCache(m_commitCacheList, m_commitReqWeight, _commitProposal);
    }

    virtual void addPrePrepareCache(PBFTMessageInterface::Ptr _prePrepareMsg)
    {
        m_prePrepare = _prePrepareMsg;
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
        m_config->storage()->storePrecommitProposal(m_precommit->consensusProposal());
    }

    virtual PBFTMessageInterface::Ptr preCommitCache() { return m_precommit; }
    virtual PBFTMessageInterface::Ptr preCommitWithoutData() { return m_precommitWithoutData; }
    virtual void setSignatureList();
    virtual bool checkAndPreCommit();
    virtual bool checkAndCommit();

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
        std::map<bcos::crypto::HashType, std::map<IndexType, PBFTMessageInterface::Ptr>>;
    using QuorumRecoderType = std::map<bcos::crypto::HashType, uint64_t>;
    void addCache(CollectionCacheType& _cachedReq, QuorumRecoderType& _weightInfo,
        PBFTMessageInterface::Ptr _proposal);

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
    std::atomic_bool m_submitting = {false};
    std::atomic<bcos::protocol::BlockNumber> m_index;
    CollectionCacheType m_prepareCacheList;
    QuorumRecoderType m_prepareReqWeight;

    CollectionCacheType m_commitCacheList;
    QuorumRecoderType m_commitReqWeight;

    PBFTMessageInterface::Ptr m_prePrepare = nullptr;
    PBFTMessageInterface::Ptr m_precommit = nullptr;
    PBFTMessageInterface::Ptr m_precommitWithoutData = nullptr;
};
}  // namespace consensus
}  // namespace bcos
