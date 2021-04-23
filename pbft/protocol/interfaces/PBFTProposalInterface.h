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
 * @brief interface for PBFTProposal
 * @file PBFTProposalInterfaces.h
 * @author: yujiechen
 * @date 2021-04-15
 */
#pragma once
#include "framework/ProposalInterface.h"
#include "pbft/utilities/Common.h"

namespace bcos
{
namespace consensus
{
class PBFTProposalInterface : virtual public ProposalInterface
{
public:
    using Ptr = std::shared_ptr<PBFTProposalInterface>;
    PBFTProposalInterface() = default;
    ~PBFTProposalInterface() override {}

    virtual size_t signatureProofSize() const = 0;
    virtual std::pair<int64_t, bytesConstRef> signatureProof(size_t _index) const = 0;
    virtual void appendSignatureProof(int64_t _nodeIdx, bytes const& _signatureData) = 0;

    // In order to support the consensus of a single proposal dimension
    virtual void setView(ViewType _view) = 0;
    virtual ViewType view() const = 0;

    virtual void setGeneratedFrom(IndexType _from) = 0;
    virtual IndexType generatedFrom() const = 0;
};
using PBFTProposalList = std::vector<PBFTProposalInterface::Ptr>;
using PBFTProposalListPtr = std::shared_ptr<PBFTProposalList>;
}  // namespace consensus
}  // namespace bcos