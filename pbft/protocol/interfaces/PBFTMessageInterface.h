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
 * @brief interface for PBFT Message
 * @file PBFTMessageInterface.h
 * @author: yujiechen
 * @date 2021-04-13
 */
#pragma once
#include "framework/ProposalInterface.h"
namespace bcos
{
namespace consensus
{
class PBFTMessageInterface
{
public:
    using Ptr = std::shared_ptr<PBFTMessageInterface>;
    PBFTMessageInterface() = default;
    virtual ~PBFTMessageInterface() {}

    virtual void setProposals(ProposalList const& _proposals) = 0;
    virtual ProposalList const& proposals() const = 0;
};
}  // namespace consensus
}  // namespace bcos