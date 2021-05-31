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
 * @brief fixture for the PBFT
 * @file PBFTFixture.h
 * @author: yujiechen
 * @date 2021-05-28
 */
#pragma once
#include "pbft/PBFTFactory.h"
#include "pbft/PBFTImpl.h"
#include "test/unittests/faker/FakeDispatcher.h"
#include "test/unittests/faker/FakeStorage.h"
#include "test/unittests/faker/FakeTxPool.h"
#include <bcos-framework/interfaces/consensus/ConsensusNode.h>
#include <bcos-framework/libprotocol/protobuf/PBBlockFactory.h>
#include <bcos-framework/libprotocol/protobuf/PBBlockHeaderFactory.h>
#include <bcos-framework/libprotocol/protobuf/PBTransactionFactory.h>
#include <bcos-framework/libprotocol/protobuf/PBTransactionReceiptFactory.h>
#include <bcos-framework/testutils/faker/FakeFrontService.h>
#include <bcos-framework/testutils/faker/FakeLedger.h>
#include <bcos-framework/testutils/faker/FakeSealer.h>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

using namespace bcos::crypto;
using namespace bcos::front;
using namespace bcos::storage;
using namespace bcos::ledger;
using namespace bcos::txpool;
using namespace bcos::sealer;
using namespace bcos::protocol;
using namespace bcos::dispatcher;
using namespace bcos::consensus;

namespace bcos
{
namespace test
{
class PBFTFixture
{
public:
    PBFTFixture(CryptoSuite::Ptr _cryptoSuite, KeyPairInterface::Ptr _keyPair,
        size_t _consensusTimeout = 3, size_t _txCountLimit = 1000)
      : m_cryptoSuite(_cryptoSuite), m_keyPair(_keyPair), m_nodeId(_keyPair->publicKey())
    {
        auto blockHeaderFactory = std::make_shared<PBBlockHeaderFactory>(_cryptoSuite);
        auto txFactory = std::make_shared<PBTransactionFactory>(_cryptoSuite);
        auto receiptFactory = std::make_shared<PBTransactionReceiptFactory>(_cryptoSuite);
        // create block factory
        m_blockFactory =
            std::make_shared<PBBlockFactory>(blockHeaderFactory, txFactory, receiptFactory);

        // create fakeFrontService
        m_frontService = std::make_shared<FakeFrontService>(_keyPair->publicKey());

        // create fakeStorage
        m_storage = std::make_shared<FakeStorage>();

        // create fakeLedger
        m_ledger = std::make_shared<FakeLedger>(m_blockFactory, 20, 10, 10);
        m_ledger->setSystemConfig(SYSTEM_KEY_CONSENSUS_TIMEOUT, std::to_string(_consensusTimeout));
        m_ledger->setSystemConfig(SYSTEM_KEY_TX_COUNT_LIMIT, std::to_string(_txCountLimit));

        // create fakeTxPool
        m_txpool = std::make_shared<FakeTxPool>();

        // create FakeSealer
        m_sealer = std::make_shared<FakeSealer>();

        // create FakeDispatcher
        m_dispatcher = std::make_shared<FakeDispatcher>();

        m_pbftFactory = std::make_shared<PBFTFactory>(_cryptoSuite, _keyPair, m_frontService,
            m_storage, m_ledger, m_txpool, m_sealer, m_dispatcher, m_blockFactory);
        m_pbft = std::dynamic_pointer_cast<PBFTImpl>(m_pbftFactory->consensus());
        m_pbftEngine = m_pbftFactory->pbftEngine();
    }
    virtual ~PBFTFixture() {}

    void init() { m_pbftFactory->init(); }

    void appendConsensusNode(ConsensusNode::Ptr _node)
    {
        m_ledger->ledgerConfig()->mutableConsensusNodeList().push_back(_node);
        pbftConfig()->setConsensusNodeList(m_ledger->ledgerConfig()->mutableConsensusNodeList());
    }

    void appendConsensusNode(PublicPtr _nodeId)
    {
        auto node = std::make_shared<ConsensusNode>(_nodeId, 1);
        appendConsensusNode(node);
    }

    void clearConsensusNodeList() { m_ledger->ledgerConfig()->mutableConsensusNodeList().clear(); }

    FakeFrontService::Ptr frontService() { return m_frontService; }
    FakeStorage::Ptr storage() { return m_storage; }
    FakeLedger::Ptr ledger() { return m_ledger; }
    FakeTxPool::Ptr txpool() { return m_txpool; }
    FakeSealer::Ptr sealer() { return m_sealer; }
    FakeDispatcher::Ptr dispatcher() { return m_dispatcher; }

    PBFTFactory::Ptr pbftFactory() { return m_pbftFactory; }
    PBFTImpl::Ptr pbft() { return m_pbft; }
    PBFTConfig::Ptr pbftConfig() { return m_pbftFactory->pbftConfig(); }
    PublicPtr nodeID() { return m_nodeId; }

    PBFTEngine::Ptr pbftEngine() { return m_pbftEngine; }

private:
    CryptoSuite::Ptr m_cryptoSuite;
    KeyPairInterface::Ptr m_keyPair;
    PublicPtr m_nodeId;
    BlockFactory::Ptr m_blockFactory;

    FakeFrontService::Ptr m_frontService;
    FakeStorage::Ptr m_storage;
    FakeLedger::Ptr m_ledger;
    FakeTxPool::Ptr m_txpool;
    FakeSealer::Ptr m_sealer;
    FakeDispatcher::Ptr m_dispatcher;

    PBFTFactory::Ptr m_pbftFactory;
    PBFTEngine::Ptr m_pbftEngine;
    PBFTImpl::Ptr m_pbft;
};
}  // namespace test
}  // namespace bcos