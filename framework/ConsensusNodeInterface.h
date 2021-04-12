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
 * @brief the information of the consensus node
 * @file ConsensusNodeInterface.h
 * @author: yujiechen
 * @date 2021-04-09
 */
#pragma once
#include <bcos-framework/interfaces/crypto/KeyInterface.h>
namespace bcos
{
namespace consensus
{
class ConsensusNodeInterface
{
public:
    using Ptr = std::shared_ptr<ConsensusNodeInterface>;
    ConsensusNodeInterface() = default;
    virtual ~ConsensusNodeInterface() {}

    // the nodeID of the consensus node
    virtual bcos::crypto::PublicPtr nodeID() const = 0;

    virtual uint64_t weight() const { return 100; }
};
using ConsensusNodeList = std::vector<ConsensusNodeInterface::Ptr>;
using ConsensusNodeListPtr = std::shared_ptr<ConsensusNodeList>;
}  // namespace consensus
}  // namespace bcos
