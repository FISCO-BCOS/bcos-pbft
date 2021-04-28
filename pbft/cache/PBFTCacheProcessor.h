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
 * @brief cache processor for the PBFTReq
 * @file PBFTCacheProcessor.h
 * @author: yujiechen
 * @date 2021-04-21
 */
#pragma once
#include "pbft/cache/PBFTCache.h"
#include "pbft/config/PBFTConfig.h"
#include "pbft/interfaces/PBFTMessageFactory.h"
#include "pbft/interfaces/PBFTMessageInterface.h"
#include "pbft/interfaces/ViewChangeMsgInterface.h"
#include <queue>
namespace bcos
{
namespace consensus
{
struct ProposalQueueCmp
{
    bool operator()(ProposalInterface::Ptr _a, PBFTProposalInterface::Ptr _b)
    {
        // increase order
        return _a->index() > _b->index();
    }
};
class PBFTCacheProcessor
{
public:
    using Ptr = std::shared_ptr<PBFTCacheProcessor>;
    explicit PBFTCacheProcessor(PBFTConfig::Ptr _config)
      : m_config(_config), m_preCommitProposals(std::make_shared<PBFTProposalList>())
    {}

    virtual ~PBFTCacheProcessor() {}

    virtual void addPrePrepareCache(PBFTMessageInterface::Ptr _prePrepareMsg);
    virtual bool existPrePrepare(PBFTMessageInterface::Ptr _prePrepareMsg);
    virtual bool conflictWithProcessedReq(PBFTMessageInterface::Ptr _msg);
    virtual bool conflictWithPrecommitReq(PBFTMessageInterface::Ptr _prePrepareMsg);

    virtual void addPrepareCache(PBFTMessageInterface::Ptr _prepareReq)
    {
        addCache(m_caches, _prepareReq,
            [](PBFTCache::Ptr _pbftCache, PBFTProposalInterface::Ptr _pbftProposal) {
                _pbftCache->addPrepareCache(_pbftProposal);
            });
    }

    virtual void addCommitReq(PBFTMessageInterface::Ptr _commitReq)
    {
        addCache(m_caches, _commitReq,
            [](PBFTCache::Ptr _pbftCache, PBFTProposalInterface::Ptr _pbftProposal) {
                _pbftCache->addCommitCache(_pbftProposal);
            });
    }

    PBFTProposalList const& preCommitProposals() { return *m_preCommitProposals; }
    virtual void checkAndPreCommit();
    virtual void checkAndCommit();

    virtual void addViewChangeReq(ViewChangeMsgInterface::Ptr _viewChange);
    virtual NewViewMsgInterface::Ptr checkAndTryIntoNewView();

    // TODO: clear the expired cache periodically
    virtual void clearExpiredCache() {}
    // TODO: remove invalid viewchange
    virtual void removeInvalidViewChange() {}
    virtual bool checkPrecommitProposal(std::shared_ptr<PBFTProposalInterface> _precommitProposal);
    virtual bytesPointer precommitProposalData(PBFTMessageInterface::Ptr _pbftMessage,
        bcos::protocol::BlockNumber _index, bcos::crypto::HashType const& _hash);

    // TODO: fill the missed proposal
    virtual void fillMissedProposal(PBFTProposalInterface::Ptr _proposal);

private:
    using PBFTCachesType =
        std::map<bcos::crypto::HashType, std::map<bcos::protocol::BlockNumber, PBFTCache::Ptr>>;

    using UpdateCacheHandler = std::function<void(
        PBFTCache::Ptr _proposalCache, PBFTProposalInterface::Ptr _pbftProposal)>;
    void addCache(PBFTCachesType& _proposalCache, PBFTMessageInterface::Ptr _pbftReq,
        UpdateCacheHandler _handler);
    virtual void submitProposalsInOrder();

private:
    PBFTConfig::Ptr m_config;
    PBFTMessageInterface::Ptr m_prePrepareCache = nullptr;
    mutable SharedMutex x_prePrepareCache;
    PBFTCachesType m_caches;

    PBFTProposalListPtr m_preCommitProposals;
    PBFTMessageInterface::Ptr m_preCommitReq;

    // viewchange caches
    using ViewChangeCacheType =
        std::map<ViewType, std::map<IndexType, ViewChangeMsgInterface::Ptr>>;
    ViewChangeCacheType m_viewChangeCache;
    std::map<ViewType, uint64_t> m_viewChangeWeight;
    std::map<ViewType, int64_t> m_maxCommittedIndex;

    NewViewMsgInterface::Ptr m_newViewCache;

    // cached proposals that have collected enough signatures
    std::priority_queue<PBFTProposalInterface::Ptr, PBFTProposalList, ProposalQueueCmp>
        m_commitQueue;
    std::atomic<bcos::protocol::BlockNumber> m_blockToCommit = {0};
};
}  // namespace consensus
}  // namespace bcos