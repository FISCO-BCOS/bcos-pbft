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
#include "Common.h"
#include <bcos-pbft/core/ConsensusConfig.h>

namespace bcos
{
namespace consensus
{
class PBFTConfig : public ConsensusConfig
{
public:
    explicit PBFTConfig(bcos::crypto::KeyPairInterface::Ptr _keyPair) : ConsensusConfig(_keyPair) {}
    ~PBFTConfig() override {}

    uint64_t minRequiredQuorum() const override;

    virtual ViewType view() const { return m_view; }
    virtual void setView(ViewType _view) { m_view.store(_view); }

    virtual ViewType toView() const { return m_toView; }
    virtual void setToView(ViewType _toView) { m_toView.store(_toView); }

    virtual IndexType leaderIndex(bcos::protocol::BlockNumber _proposalIndex);

    virtual uint64_t leaderSwitchPeriod() const { return m_leaderSwitchPeriod; }
    virtual void setLeaderSwitchPeriod(uint64_t _leaderSwitchPeriod)
    {
        m_leaderSwitchPeriod.store(_leaderSwitchPeriod);
    }

protected:
    void updateQuorum() override;

private:
    std::atomic<uint64_t> m_maxFaultyQuorum;
    std::atomic<uint64_t> m_totalQuorum;
    std::atomic<uint64_t> m_minRequiredQuorum;
    std::atomic<ViewType> m_view = {0};
    std::atomic<ViewType> m_toView = {0};

    std::atomic<uint64_t> m_leaderSwitchPeriod = {1};
};
}  // namespace consensus
}  // namespace bcos
