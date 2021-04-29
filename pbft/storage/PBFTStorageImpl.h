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
 * @brief Storage for PBFT
 * @file PBFTStorageImpl.cpp
 * @author: yujiechen
 * @date 2021-04-25
 */
#pragma once
#include "pbft/utilities/Common.h"
#include <bcos-framework/interfaces/protocol/CommonError.h>
#include <bcos-framework/interfaces/txpool/TxPoolInterface.h>
#include <bcos-framework/storage/PBFTStorage.h>
#include <bcos-framework/storage/StorageInterface.h>

namespace bcos
{
namespace consensus
{
class PBFTStorageImpl : public PBFTStorage
{
public:
    using Ptr = std::shared_ptr<PBFTStorageImpl>;
    PBFTStorageImpl(
        bcos::txpool::TxPoolInterface::Ptr _txPool, bcos::storage::StorageInterface::Ptr _storage)
      : m_txPool(_txPool), m_storage(_storage)
    {}

    ~PBFTStorageImpl() override {}

    // notify the txpool to commit precommit proposal into the blockchain
    void storePrecommitProposal(PBFTProposalInterface::Ptr _proposal) override
    {
        m_txPool->asyncStoreTxs(
            _proposal->hash(), _proposal->data(), [_proposal](Error::Ptr _error) {
                // TODO: retry or clear the cache when store preCommitProposal failed
                if (_error->errorCode() != bcos::protocol::CommonError::SUCCESS)
                {
                    PBFT_LOG(WARNING) << LOG_DESC("storePrecommitProposal failed")
                                      << LOG_KV("errorCode", std::to_string(_error->errorCode()))
                                      << LOG_KV("errorInfo", _error->errorMessage())
                                      << printPBFTProposal(_proposal);
                    return;
                }
                PBFT_LOG(INFO) << LOG_DESC("storePrecommitProposal success")
                               << printPBFTProposal(_proposal);
            });
    }

    // commit the proposal to the kvstorage
    void asyncCommitProposal(PBFTProposalInterface::Ptr) override {}

private:
    bcos::txpool::TxPoolInterface::Ptr m_txPool;
    bcos::storage::StorageInterface::Ptr m_storage;
};
}  // namespace consensus
}  // namespace bcos