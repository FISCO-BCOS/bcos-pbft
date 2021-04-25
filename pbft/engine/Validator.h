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
 * @brief Validator for the consensus module
 * @file Validator.h
 * @author: yujiechen
 * @date 2021-04-21
 */
#pragma once
#include "bcos-framework/interfaces/txpool/TxPoolInterface.h"
#include "pbft/interfaces/PBFTProposalInterface.h"
#include <bcos-framework/interfaces/protocol/CommonError.h>

namespace bcos
{
namespace consensus
{
class ValidatorInterface
{
public:
    using Ptr = std::shared_ptr<ValidatorInterface>;
    ValidatorInterface() = default;
    virtual ~ValidatorInterface() {}
    virtual void verifyProposals(bcos::crypto::PublicPtr _fromNode,
        PBFTProposalList const& _proposals,
        std::function<void(Error::Ptr, bool)> _verifyFinishedHandler) = 0;
};

class TxsValidator : public ValidatorInterface
{
public:
    using Ptr = std::shared_ptr<TxsValidator>;
    explicit TxsValidator(bcos::txpool::TxPoolInterface::Ptr _txPool) : m_txPool(_txPool) {}
    ~TxsValidator() override {}

    void verifyProposals(bcos::crypto::PublicPtr _fromNode, PBFTProposalList const& _proposals,
        std::function<void(Error::Ptr, bool)> _verifyFinishedHandler) override
    {
        std::vector<bytesConstRef> proposalsData;
        for (auto proposal : _proposals)
        {
            proposalsData.push_back(proposal->data());
        }
        m_txPool->asyncVerifyBlocks(_fromNode, proposalsData, _verifyFinishedHandler);
    }

protected:
    bcos::txpool::TxPoolInterface::Ptr m_txPool;
};
}  // namespace consensus
}  // namespace bcos