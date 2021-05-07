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
 * @brief  Storage for the ledger
 * @file LedgerStorage.h
 * @author: yujiechen
 * @date 2021-04-26
 */
#pragma once
#include "pbft/interfaces/PBFTStorage.h"
#include <bcos-framework/interfaces/ledger/LedgerInterface.h>
#include <bcos-framework/interfaces/storage/StorageInterface.h>

namespace bcos
{
namespace consensus
{
class LedgerStorage : public PBFTStorage
{
public:
    using Ptr = std::shared_ptr<LedgerStorage>;
    LedgerStorage(std::shared_ptr<bcos::ledger::LedgerInterface> _ledger,
        bcos::storage::StorageInterface::Ptr _storage)
      : m_ledger(_ledger), m_storage(_storage)
    {}

    // commit the precommit proposal into the kvstorage
    void storePrecommitProposal(PBFTProposalInterface::Ptr) override {}
    // commit the executed-block into the blockchain
    void asyncCommitProposal(PBFTProposalInterface::Ptr _proposal) override;

    void registerConfigResetHandler(
        std::function<void(bcos::ledger::LedgerConfig::Ptr)> _resetConfigHandler) override
    {
        m_resetConfigHandler = _resetConfigHandler;
    }

    void registerFinalizeHandler(
        std::function<void(bcos::ledger::LedgerConfig::Ptr)> _finalizeHandler) override
    {
        m_finalizeHandler = _finalizeHandler;
    }

private:
    std::shared_ptr<bcos::ledger::LedgerInterface> m_ledger;
    bcos::storage::StorageInterface::Ptr m_storage;
    std::function<void(bcos::ledger::LedgerConfig::Ptr)> m_resetConfigHandler;
    std::function<void(bcos::ledger::LedgerConfig::Ptr)> m_finalizeHandler;
};
}  // namespace consensus
}  // namespace bcos