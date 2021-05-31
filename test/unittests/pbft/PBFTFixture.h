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
class FakeCacheProcessor : public PBFTCacheProcessor
{
public:
    using Ptr = std::shared_ptr<FakeCacheProcessor>;
    explicit FakeCacheProcessor(PBFTConfig::Ptr _config) : PBFTCacheProcessor(_config) {}

    ~FakeCacheProcessor() override {}
};

class FakePBFTEngine : public PBFTEngine
{
public:
    using Ptr = std::shared_ptr<FakePBFTEngine>;
    explicit FakePBFTEngine(PBFTConfig::Ptr _config) : PBFTEngine(_config)
    {
        m_cacheProcessor = std::make_shared<FakeCacheProcessor>(_config);
        m_logSync = std::make_shared<PBFTLogSync>(_config, m_cacheProcessor);
    }
    ~FakePBFTEngine() override {}

    void onReceivePBFTMessage(bcos::Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID,
        bytesConstRef _data, std::function<void(bytesConstRef _respData)> _sendResponse) override
    {
        PBFTEngine::onReceivePBFTMessage(_error, _nodeID, _data, _sendResponse);
    }

    // PBFT main processing function
    void executeWorker() override
    {
        while (!msgQueue()->empty())
        {
            PBFTEngine::executeWorker();
        }
    }

    void onRecvProposal(bytesConstRef _proposalData, bcos::protocol::BlockNumber _proposalIndex,
        bcos::crypto::HashType const& _proposalHash) override
    {
        PBFTEngine::onRecvProposal(_proposalData, _proposalIndex, _proposalHash);
    }

    bool handlePrePrepareMsg(std::shared_ptr<PBFTMessageInterface> _prePrepareMsg,
        bool _needVerifyProposal, bool _generatedFromNewView = false,
        bool _needCheckSignature = true) override
    {
        return PBFTEngine::handlePrePrepareMsg(
            _prePrepareMsg, _needVerifyProposal, _generatedFromNewView, _needCheckSignature);
    }

    PBFTMsgQueuePtr msgQueue() { return m_msgQueue; }
};

class FakePBFTFactory : public PBFTFactory
{
public:
    using Ptr = std::shared_ptr<PBFTFactory>;
    FakePBFTFactory(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair,
        std::shared_ptr<bcos::front::FrontServiceInterface> _frontService,
        bcos::storage::StorageInterface::Ptr _storage,
        std::shared_ptr<bcos::ledger::LedgerInterface> _ledger,
        bcos::txpool::TxPoolInterface::Ptr _txpool, bcos::sealer::SealerInterface::Ptr _sealer,
        bcos::dispatcher::DispatcherInterface::Ptr _dispatcher,
        bcos::protocol::BlockFactory::Ptr _blockFactory)
      : PBFTFactory(_cryptoSuite, _keyPair, _frontService, _storage, _ledger, _txpool, _sealer,
            _dispatcher, _blockFactory)
    {
        PBFT_LOG(DEBUG) << LOG_DESC("create PBFTEngine");
        m_pbftEngine = std::make_shared<FakePBFTEngine>(m_pbftConfig);

        PBFT_LOG(INFO) << LOG_DESC("create PBFT");
        m_pbft = std::make_shared<PBFTImpl>(m_pbftEngine);
        PBFT_LOG(INFO) << LOG_DESC("create PBFT success");
    }
};

class PBFTFixture
{
public:
    using Ptr = std::shared_ptr<PBFTFixture>;
    PBFTFixture(CryptoSuite::Ptr _cryptoSuite, KeyPairInterface::Ptr _keyPair,
        FakeLedger::Ptr _ledger = nullptr, size_t _consensusTimeout = 3,
        size_t _txCountLimit = 1000)
      : m_cryptoSuite(_cryptoSuite), m_keyPair(_keyPair), m_nodeId(_keyPair->publicKey())
    {
        // create block factory
        m_blockFactory = createBlockFactory(_cryptoSuite);

        // create fakeFrontService
        m_frontService = std::make_shared<FakeFrontService>(_keyPair->publicKey());

        // create fakeStorage
        m_storage = std::make_shared<FakeStorage>();

        // create fakeLedger
        if (_ledger == nullptr)
        {
            m_ledger = std::make_shared<FakeLedger>(m_blockFactory, 20, 10, 10);
            m_ledger->setSystemConfig(
                SYSTEM_KEY_CONSENSUS_TIMEOUT, std::to_string(_consensusTimeout));
            m_ledger->setSystemConfig(SYSTEM_KEY_TX_COUNT_LIMIT, std::to_string(_txCountLimit));
        }
        else
        {
            m_ledger = _ledger;
        }
        // create fakeTxPool
        m_txpool = std::make_shared<FakeTxPool>();

        // create FakeSealer
        m_sealer = std::make_shared<FakeSealer>();

        // create FakeDispatcher
        m_dispatcher = std::make_shared<FakeDispatcher>();

        m_pbftFactory = std::make_shared<FakePBFTFactory>(_cryptoSuite, _keyPair, m_frontService,
            m_storage, m_ledger, m_txpool, m_sealer, m_dispatcher, m_blockFactory);
        m_pbft = std::dynamic_pointer_cast<PBFTImpl>(m_pbftFactory->consensus());
        m_pbftEngine = std::dynamic_pointer_cast<FakePBFTEngine>(m_pbftFactory->pbftEngine());
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

    void updateSwitchPerid() {}

    void clearConsensusNodeList() { m_ledger->ledgerConfig()->mutableConsensusNodeList().clear(); }

    FakeFrontService::Ptr frontService() { return m_frontService; }
    FakeStorage::Ptr storage() { return m_storage; }
    FakeLedger::Ptr ledger() { return m_ledger; }
    FakeTxPool::Ptr txpool() { return m_txpool; }
    FakeSealer::Ptr sealer() { return m_sealer; }
    FakeDispatcher::Ptr dispatcher() { return m_dispatcher; }

    FakePBFTFactory::Ptr pbftFactory() { return m_pbftFactory; }
    PBFTImpl::Ptr pbft() { return m_pbft; }
    PBFTConfig::Ptr pbftConfig() { return m_pbftFactory->pbftConfig(); }
    PublicPtr nodeID() { return m_nodeId; }

    FakePBFTEngine::Ptr pbftEngine() { return m_pbftEngine; }
    KeyPairInterface::Ptr keyPair() { return m_keyPair; }

    void setFrontService(FakeFrontService::Ptr _fakeFrontService)
    {
        m_frontService = _fakeFrontService;
    }

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

    FakePBFTFactory::Ptr m_pbftFactory;
    FakePBFTEngine::Ptr m_pbftEngine;
    PBFTImpl::Ptr m_pbft;
};
using PBFTFixtureList = std::vector<PBFTFixture::Ptr>;

inline PBFTFixture::Ptr createPBFTFixture(CryptoSuite::Ptr _cryptoSuite,
    FakeLedger::Ptr _ledger = nullptr, size_t _consensusTimeout = 3, size_t _txCountLimit = 1000)
{
    auto keyPair = _cryptoSuite->signatureImpl()->generateKeyPair();
    return std::make_shared<PBFTFixture>(
        _cryptoSuite, keyPair, _ledger, _consensusTimeout, _txCountLimit);
}

inline std::map<IndexType, PBFTFixture::Ptr> createFakers(CryptoSuite::Ptr _cryptoSuite,
    size_t _consensusNodeSize, size_t _currentBlockNumber, size_t _consensusTimeout = 3,
    size_t _txCountLimit = 1000)
{
    PBFTFixtureList fakerList;
    // create block factory
    auto blockFactory = createBlockFactory(_cryptoSuite);

    auto ledger = std::make_shared<FakeLedger>(blockFactory, _currentBlockNumber + 1, 10, 0);
    for (size_t i = 0; i < _consensusNodeSize; i++)
    {
        // ensure all the block are consistent
        auto fakedLedger = std::make_shared<FakeLedger>(
            blockFactory, _currentBlockNumber + 1, 10, 0, ledger->sealerList());
        fakedLedger->setSystemConfig(
            SYSTEM_KEY_CONSENSUS_TIMEOUT, std::to_string(_consensusTimeout));
        fakedLedger->setSystemConfig(SYSTEM_KEY_TX_COUNT_LIMIT, std::to_string(_txCountLimit));

        auto peerFaker =
            createPBFTFixture(_cryptoSuite, fakedLedger, _consensusTimeout, _txCountLimit);
        fakerList.push_back(peerFaker);
    }

    for (size_t i = 0; i < _consensusNodeSize; i++)
    {
        auto faker = fakerList[i];
        for (size_t j = 0; j < _consensusNodeSize; j++)
        {
            faker->appendConsensusNode(fakerList[j]->keyPair()->publicKey());
        }
    }
    // init the fakers
    auto fakeGateWay = std::make_shared<FakeGateWay>();
    for (size_t i = 0; i < _consensusNodeSize; i++)
    {
        auto faker = fakerList[i];
        faker->init();
        fakeGateWay->addConsensusInterface(faker->keyPair()->publicKey(), faker->pbft());
    }
    std::map<IndexType, PBFTFixture::Ptr> indexToFakerMap;
    for (size_t i = 0; i < _consensusNodeSize; i++)
    {
        auto faker = fakerList[i];
        indexToFakerMap[faker->pbftConfig()->nodeIndex()] = faker;
        faker->frontService()->setGateWay(fakeGateWay);
    }
    return indexToFakerMap;
}

inline Block::Ptr fakeBlock(CryptoSuite::Ptr _cryptoSuite, PBFTFixture::Ptr _faker,
    size_t _proposalIndex, size_t _txsHashSize)
{
    auto ledgerConfig = _faker->ledger()->ledgerConfig();
    auto parent = (_faker->ledger()->ledgerData())[ledgerConfig->blockNumber()];
    auto block = _faker->ledger()->init(parent->blockHeader(), true, _proposalIndex, 0, 0);
    for (size_t i = 0; i < _txsHashSize; i++)
    {
        block->appendTransactionHash(_cryptoSuite->hashImpl()->hash(std::to_string(i)));
    }
    return block;
}
}  // namespace test
}  // namespace bcos