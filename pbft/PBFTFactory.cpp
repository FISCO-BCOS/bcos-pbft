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
 * @file PBFTFactory.cpp
 * @author: yujiechen
 * @date 2021-05-19
 */
#include "PBFTFactory.h"
#include "core/StateMachine.h"
#include "pbft/PBFT.h"
#include "pbft/engine/Validator.h"
#include "pbft/protocol/PB/PBFTCodec.h"
#include "pbft/protocol/PB/PBFTMessageFactoryImpl.h"
#include "pbft/storage/LedgerStorage.h"
#include "pbft/utilities/Common.h"
#include <bcos-framework/interfaces/protocol/CommonError.h>
#include <bcos-framework/libsealer/Sealer.h>

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::ledger;
using namespace bcos::protocol;
using namespace bcos::tool;

PBFTFactory::PBFTFactory(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
    bcos::crypto::KeyPairInterface::Ptr _keyPair,
    std::shared_ptr<bcos::front::FrontServiceInterface> _frontService,
    bcos::storage::StorageInterface::Ptr _storage,
    std::shared_ptr<bcos::ledger::LedgerInterface> _ledger,
    bcos::txpool::TxPoolInterface::Ptr _txpool, bcos::sealer::SealerInterface::Ptr _sealer,
    bcos::dispatcher::DispatcherInterface::Ptr _dispatcher,
    bcos::protocol::BlockFactory::Ptr _blockFactory)
{
    m_ledgerFetcher = std::make_shared<LedgerConfigFetcher>(_ledger);
    auto pbftMessageFactory = std::make_shared<PBFTMessageFactoryImpl>();
    PBFT_LOG(DEBUG) << LOG_DESC("create PBFTCodec");
    auto pbftCodec = std::make_shared<PBFTCodec>(_keyPair, _cryptoSuite, pbftMessageFactory);
    PBFT_LOG(DEBUG) << LOG_DESC("create PBFT validator");
    auto validator = std::make_shared<TxsValidator>(_txpool, _blockFactory);
    PBFT_LOG(DEBUG) << LOG_DESC("create StateMachine");
    auto stateMachine = std::make_shared<StateMachine>(_dispatcher, _blockFactory);
    PBFT_LOG(DEBUG) << LOG_DESC("create pbftStorage");
    auto pbftStorage = std::make_shared<LedgerStorage>(_ledger, _storage, _blockFactory);

    PBFT_LOG(DEBUG) << LOG_DESC("create pbftConfig");
    m_pbftConfig = std::make_shared<PBFTConfig>(_cryptoSuite, _keyPair, pbftMessageFactory,
        pbftCodec, validator, _frontService, _sealer, stateMachine, pbftStorage);
    PBFT_LOG(DEBUG) << LOG_DESC("create PBFTEngine");
    auto pbftEngine = std::make_shared<PBFTEngine>(m_pbftConfig);
    PBFT_LOG(INFO) << LOG_DESC("create PBFT");
    m_pbft = std::make_shared<PBFT>(pbftEngine);
    PBFT_LOG(INFO) << LOG_DESC("create PBFT success");
}

void PBFTFactory::init()
{
    PBFT_LOG(INFO) << LOG_DESC("fetch LedgerConfig information");
    m_ledgerFetcher->fetchBlockNumberAndHash();
    m_ledgerFetcher->fetchConsensusNodeList();
    m_ledgerFetcher->fetchConsensusTimeout();
    m_ledgerFetcher->fetchBlockTxCountLimit();
    m_ledgerFetcher->waitFetchFinished();
    PBFT_LOG(INFO) << LOG_DESC("fetch LedgerConfig information success")
                   << LOG_KV("blockNumber", m_ledgerFetcher->ledgerConfig()->blockNumber())
                   << LOG_KV("hash", m_ledgerFetcher->ledgerConfig()->hash().abridged())
                   << LOG_KV(
                          "consensusTimeout", m_ledgerFetcher->ledgerConfig()->consensusTimeout())
                   << LOG_KV(
                          "maxTxsPerBlock", m_ledgerFetcher->ledgerConfig()->blockTxCountLimit());
    m_pbftConfig->resetConfig(m_ledgerFetcher->ledgerConfig());
    // TODO: init the PBFTState
}