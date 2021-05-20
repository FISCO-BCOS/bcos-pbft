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
 * @brief factory to create the PBFTEngine
 * @file PBFTFactory.h
 * @author: yujiechen
 * @date 2021-05-19
 */
#pragma once
#include "PBFT.h"
#include "pbft/config/PBFTConfig.h"
#include <bcos-framework/interfaces/dispatcher/DispatcherInterface.h>
#include <bcos-framework/interfaces/ledger/LedgerInterface.h>
#include <bcos-framework/interfaces/storage/StorageInterface.h>
#include <bcos-framework/libtool/LedgerConfigFetcher.h>

namespace bcos
{
namespace consensus
{
class PBFTFactory : public std::enable_shared_from_this<PBFTFactory>
{
public:
    PBFTFactory(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair,
        std::shared_ptr<bcos::front::FrontServiceInterface> _frontService,
        bcos::storage::StorageInterface::Ptr _storage,
        std::shared_ptr<bcos::ledger::LedgerInterface> _ledger,
        bcos::txpool::TxPoolInterface::Ptr _txpool, bcos::sealer::SealerInterface::Ptr _sealer,
        bcos::dispatcher::DispatcherInterface::Ptr _dispatcher,
        bcos::protocol::BlockFactory::Ptr _blockFactory);

    virtual ~PBFTFactory() {}
    virtual void init();

    ConsensusInterface::Ptr consensus() { return m_pbft; }
    PBFTConfig::Ptr pbftConfig() { return m_pbftConfig; }

private:
    ConsensusInterface::Ptr m_pbft;
    PBFTConfig::Ptr m_pbftConfig;
    bcos::tool::LedgerConfigFetcher::Ptr m_ledgerFetcher;
};
}  // namespace consensus
}  // namespace bcos