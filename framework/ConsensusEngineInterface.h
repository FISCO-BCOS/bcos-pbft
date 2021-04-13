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
 * @brief interface for Consensus Engine
 * @file ConsensusEngineInterface.h
 * @author: yujiechen
 * @date 2021-04-09
 */
#pragma once
#include <bcos-pbft/framework/ConsensusConfigInterface.h>
#include <bcos-pbft/framework/ConsensusStateInterface.h>
#include <bcos-pbft/framework/TimerInterface.h>
namespace bcos
{
namespace consensus
{
class ConsensusEngineInterface
{
public:
    using Ptr = std::shared_ptr<ConsensusEngineInterface>;
    ConsensusEngineInterface() = default;
    virtual ~ConsensusEngineInterface() {}

    // start the consensus engine
    virtual void start() = 0;
    // stop the consensus engine
    virtual void stop() = 0;

    // the consensus config
    virtual ConsensusConfigInterface::Ptr consensusConfig() const = 0;
    // the consensus Timer
    virtual TimerInterface::Ptr timer() = 0;
};
}  // namespace consensus
}  // namespace bcos