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
struct PBFTProposalCmp
{
    bool operator()(PBFTProposalInterface::Ptr _first, PBFTProposalInterface::Ptr _second)
    {
        // increase order
        return _first->index() > _second->index();
    }
};

class PBFTCacheProcessor : public std::enable_shared_from_this<PBFTCacheProcessor>
{
public:
    using Ptr = std::shared_ptr<PBFTCacheProcessor>;
    explicit PBFTCacheProcessor(PBFTConfig::Ptr _config) : m_config(_config) {}

    virtual ~PBFTCacheProcessor() {}
    virtual void initState(PBFTProposalListPtr _committedProposals);

    virtual void addPrePrepareCache(PBFTMessageInterface::Ptr _prePrepareMsg);
    virtual bool existPrePrepare(PBFTMessageInterface::Ptr _prePrepareMsg);

    virtual bool tryToFillProposal(PBFTMessageInterface::Ptr _prePrepareMsg);

    virtual bool conflictWithProcessedReq(PBFTMessageInterface::Ptr _msg);
    virtual bool conflictWithPrecommitReq(PBFTMessageInterface::Ptr _prePrepareMsg);
    virtual void addPrepareCache(PBFTMessageInterface::Ptr _prepareReq)
    {
        addCache(m_caches, _prepareReq,
            [](PBFTCache::Ptr _pbftCache, PBFTMessageInterface::Ptr _prepareReq) {
                _pbftCache->addPrepareCache(_prepareReq);
            });
    }

    virtual void addCommitReq(PBFTMessageInterface::Ptr _commitReq)
    {
        addCache(m_caches, _commitReq,
            [](PBFTCache::Ptr _pbftCache, PBFTMessageInterface::Ptr _commitReq) {
                _pbftCache->addCommitCache(_commitReq);
            });
    }

    PBFTMessageList preCommitCachesWithData()
    {
        PBFTMessageList precommitCacheList;
        for (auto const& it : m_caches)
        {
            auto precommitCache = it.second->preCommitCache();
            if (precommitCache != nullptr)
            {
                precommitCacheList.push_back(precommitCache);
            }
        }
        return precommitCacheList;
    }

    PBFTMessageList preCommitCachesWithoutData()
    {
        PBFTMessageList precommitCacheList;
        for (auto const& it : m_caches)
        {
            auto precommitCache = it.second->preCommitWithoutData();
            if (precommitCache != nullptr)
            {
                precommitCacheList.push_back(precommitCache);
            }
        }
        return precommitCacheList;
    }

    virtual void checkAndPreCommit();
    virtual void checkAndCommit();

    virtual void addViewChangeReq(ViewChangeMsgInterface::Ptr _viewChange);
    virtual NewViewMsgInterface::Ptr checkAndTryIntoNewView();

    virtual bytesPointer fetchPrecommitData(ViewChangeMsgInterface::Ptr _pbftMessage,
        bcos::protocol::BlockNumber _index, bcos::crypto::HashType const& _hash);

    virtual bool checkPrecommitMsg(PBFTMessageInterface::Ptr _precommitMsg);

    virtual void removeConsensusedCache(
        ViewType _view, bcos::protocol::BlockNumber _consensusedNumber);
    virtual void resetCacheAfterViewChange(
        ViewType _view, bcos::protocol::BlockNumber _latestCommittedProposal);
    virtual void removeInvalidViewChange(
        ViewType _view, bcos::protocol::BlockNumber _latestCommittedProposal);

    virtual void setCheckPointProposal(PBFTProposalInterface::Ptr _proposal);
    virtual void addCheckPointMsg(PBFTMessageInterface::Ptr _checkPointMsg);
    virtual void checkAndCommitStableCheckPoint();
    virtual void tryToCommitStableCheckPoint();

protected:
    virtual void updateCommitQueue(PBFTProposalInterface::Ptr _committedProposal);
    virtual void applyStateMachine(PBFTProposalInterface::Ptr _proposal);
    virtual void updateStableCheckPointQueue(PBFTProposalInterface::Ptr _stableCheckPoint);

private:
    using PBFTCachesType = std::map<bcos::protocol::BlockNumber, PBFTCache::Ptr>;
    using UpdateCacheHandler =
        std::function<void(PBFTCache::Ptr _pbftCache, PBFTMessageInterface::Ptr _pbftMessage)>;
    void addCache(PBFTCachesType& _pbftCache, PBFTMessageInterface::Ptr _pbftReq,
        UpdateCacheHandler _handler);

    PBFTMessageList generatePrePrepareMsg(
        std::map<IndexType, ViewChangeMsgInterface::Ptr> _viewChangeCache);
    void reCalculateViewChangeWeight();

private:
    PBFTConfig::Ptr m_config;
    PBFTCachesType m_caches;

    // viewchange caches
    using ViewChangeCacheType =
        std::map<ViewType, std::map<IndexType, ViewChangeMsgInterface::Ptr>>;
    ViewChangeCacheType m_viewChangeCache;
    std::map<ViewType, uint64_t> m_viewChangeWeight;
    // only needed for viewchange
    std::map<ViewType, int64_t> m_maxCommittedIndex;
    std::map<ViewType, int64_t> m_maxPrecommitIndex;

    std::priority_queue<PBFTProposalInterface::Ptr, std::vector<PBFTProposalInterface::Ptr>,
        PBFTProposalCmp>
        m_committedQueue;

    std::priority_queue<PBFTProposalInterface::Ptr, std::vector<PBFTProposalInterface::Ptr>,
        PBFTProposalCmp>
        m_stableCheckPointQueue;
};
}  // namespace consensus
}  // namespace bcos