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
#include <bcos-framework/interfaces/protocol/BlockFactory.h>
#include <bcos-framework/libutilities/ThreadPool.h>

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
    virtual void verifyProposal(bcos::crypto::PublicPtr _fromNode,
        PBFTProposalInterface::Ptr _proposal,
        std::function<void(Error::Ptr, bool)> _verifyFinishedHandler) = 0;

    virtual void asyncResetTxsFlag(PBFTProposalInterface::Ptr _proposal, bool _flag) = 0;
};

class TxsValidator : public ValidatorInterface, public std::enable_shared_from_this<TxsValidator>
{
public:
    using Ptr = std::shared_ptr<TxsValidator>;
    explicit TxsValidator(
        bcos::txpool::TxPoolInterface::Ptr _txPool, bcos::protocol::BlockFactory::Ptr _blockFactory)
      : m_txPool(_txPool),
        m_blockFactory(_blockFactory),
        m_worker(std::make_shared<ThreadPool>("validator", 1))
    {}

    ~TxsValidator() override {}

    void verifyProposal(bcos::crypto::PublicPtr _fromNode, PBFTProposalInterface::Ptr _proposal,
        std::function<void(Error::Ptr, bool)> _verifyFinishedHandler) override
    {
        m_txPool->asyncVerifyBlock(_fromNode, _proposal->data(), _verifyFinishedHandler);
    }

    void asyncResetTxsFlag(PBFTProposalInterface::Ptr _proposal, bool _flag = false) override;

protected:
    virtual void asyncResetTxsFlag(bcos::crypto::HashListPtr _txsHashList, bool _flag);

    bcos::txpool::TxPoolInterface::Ptr m_txPool;
    bcos::protocol::BlockFactory::Ptr m_blockFactory;
    ThreadPool::Ptr m_worker;
};
}  // namespace consensus
}  // namespace bcos