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
#include "../interfaces/PBFTMessageFactory.h"
#include "../interfaces/PBFTProposalInterface.h"
#include "bcos-framework/interfaces/txpool/TxPoolInterface.h"
#include <bcos-framework/interfaces/protocol/BlockFactory.h>
#include <bcos-framework/interfaces/protocol/TransactionSubmitResultFactory.h>
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

    virtual void asyncResetTxsFlag(bytesConstRef _data, bool _flag) = 0;
    virtual PBFTProposalInterface::Ptr generateEmptyProposal(
        PBFTMessageFactory::Ptr _factory, int64_t _index, int64_t _sealerId) = 0;

    virtual void notifyTransactionsResult(
        bcos::protocol::Block::Ptr _block, bcos::protocol::BlockHeader::Ptr _header) = 0;
};

class TxsValidator : public ValidatorInterface, public std::enable_shared_from_this<TxsValidator>
{
public:
    using Ptr = std::shared_ptr<TxsValidator>;
    explicit TxsValidator(bcos::txpool::TxPoolInterface::Ptr _txPool,
        bcos::protocol::BlockFactory::Ptr _blockFactory,
        bcos::protocol::TransactionSubmitResultFactory::Ptr _txResultFactory)
      : m_txPool(_txPool),
        m_blockFactory(_blockFactory),
        m_txResultFactory(_txResultFactory),
        m_worker(std::make_shared<ThreadPool>("validator", 1))
    {}

    ~TxsValidator() override {}

    void verifyProposal(bcos::crypto::PublicPtr _fromNode, PBFTProposalInterface::Ptr _proposal,
        std::function<void(Error::Ptr, bool)> _verifyFinishedHandler) override
    {
        m_txPool->asyncVerifyBlock(_fromNode, _proposal->data(), _verifyFinishedHandler);
    }

    void asyncResetTxsFlag(bytesConstRef _data, bool _flag = false) override;

    PBFTProposalInterface::Ptr generateEmptyProposal(
        PBFTMessageFactory::Ptr _factory, int64_t _index, int64_t _sealerId) override
    {
        auto proposal = _factory->createPBFTProposal();
        proposal->setIndex(_index);
        auto block = m_blockFactory->createBlock();
        auto blockHeader = m_blockFactory->blockHeaderFactory()->createBlockHeader();
        blockHeader->populateEmptyBlock(_index, _sealerId);
        block->setBlockHeader(blockHeader);
        auto encodedData = std::make_shared<bytes>();
        block->encode(*encodedData);
        proposal->setHash(block->blockHeader()->hash());
        proposal->setData(std::move(*encodedData));
        return proposal;
    }

    void notifyTransactionsResult(
        bcos::protocol::Block::Ptr _block, bcos::protocol::BlockHeader::Ptr _header) override
    {
        auto results = std::make_shared<bcos::protocol::TransactionSubmitResults>();
        for (size_t i = 0; i < _block->transactionsHashSize(); i++)
        {
            auto const& hash = _block->transactionHash(i);
            results->push_back(m_txResultFactory->createTxSubmitResult(_header, hash));
        }
        m_txPool->asyncNotifyBlockResult(
            _header->number(), results, [_block, _header](Error::Ptr _error) {
                if (_error == nullptr)
                {
                    PBFT_LOG(INFO) << LOG_DESC("notify block result success")
                                   << LOG_KV("number", _header->number())
                                   << LOG_KV("hash", _header->hash().abridged())
                                   << LOG_KV("txsSize", _block->transactionsHashSize());
                    return;
                }
                PBFT_LOG(INFO) << LOG_DESC("notify block result failed")
                               << LOG_KV("code", _error->errorCode())
                               << LOG_KV("msg", _error->errorMessage());
            });
    }

protected:
    virtual void asyncResetTxsFlag(bcos::crypto::HashListPtr _txsHashList, bool _flag);

    bcos::txpool::TxPoolInterface::Ptr m_txPool;
    bcos::protocol::BlockFactory::Ptr m_blockFactory;
    bcos::protocol::TransactionSubmitResultFactory::Ptr m_txResultFactory;
    ThreadPool::Ptr m_worker;
};
}  // namespace consensus
}  // namespace bcos