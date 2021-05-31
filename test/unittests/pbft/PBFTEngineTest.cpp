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
 * @brief unit tests for PBFTEngine
 * @file PBFTEngineTest.cpp
 * @author: yujiechen
 * @date 2021-05-31
 */
#include "test/unittests/pbft/PBFTFixture.h"
#include "test/unittests/protocol/FakePBFTMessage.h"
#include <bcos-framework/interfaces/crypto/CryptoSuite.h>
#include <bcos-framework/testutils/TestPromptFixture.h>
#include <bcos-framework/testutils/crypto/HashImpl.h>
#include <bcos-framework/testutils/crypto/SignatureImpl.h>
#include <boost/test/unit_test.hpp>

using namespace bcos;
using namespace bcos::consensus;

namespace bcos
{
namespace test
{
BOOST_FIXTURE_TEST_SUITE(PBFTEngineTest, TestPromptFixture)
inline bool shouldExit(
    std::map<IndexType, PBFTFixture::Ptr> const& _consensusNodes, BlockNumber _expectedNumber)
{
    for (auto const& node : _consensusNodes)
    {
        if (node.second->ledger()->blockNumber() != _expectedNumber)
        {
            return false;
        }
    }
    return true;
}

BOOST_AUTO_TEST_CASE(testPBFTEngine)
{
    auto hashImpl = std::make_shared<Keccak256Hash>();
    auto signatureImpl = std::make_shared<Secp256k1SignatureImpl>();
    auto cryptoSuite = std::make_shared<CryptoSuite>(hashImpl, signatureImpl, nullptr);

    size_t consensusNodeSize = 10;
    BlockNumber currentBlockNumber = 19;
    auto fakerMap = createFakers(cryptoSuite, consensusNodeSize, currentBlockNumber);

    // check the leader notify the sealer to seal proposals
    IndexType leaderIndex = 0;
    auto leaderFaker = fakerMap[leaderIndex];
    size_t expectedProposal = (size_t)(leaderFaker->ledger()->blockNumber() + 1);
    BOOST_CHECK(leaderFaker->sealer()->proposalStartIndex() == expectedProposal);
    BOOST_CHECK(leaderFaker->sealer()->proposalEndIndex() == expectedProposal);
    BOOST_CHECK(leaderFaker->sealer()->maxTxsToSeal() == 1000);

    // the leader submit proposals
    auto pbftMsgFixture = std::make_shared<PBFTMessageFixture>(cryptoSuite, leaderFaker->keyPair());
    auto block = fakeBlock(cryptoSuite, leaderFaker, expectedProposal, 10);
    auto blockData = std::make_shared<bytes>();
    block->encode(*blockData);
    BOOST_CHECK(block->blockHeader());
    // handle pre-prepare message ,broadcast prepare messages and handle the collectted
    // prepare-request
    // check the duplicated case
    for (size_t i = 0; i < 3; i++)
    {
        leaderFaker->pbftEngine()->asyncSubmitProposal(
            ref(*blockData), block->blockHeader()->number(), block->blockHeader()->hash(), nullptr);
    }
    // Discontinuous case
    auto faker = fakerMap[3];
    block = fakeBlock(cryptoSuite, faker, currentBlockNumber + 4, 10);
    blockData = std::make_shared<bytes>();
    block->encode(*blockData);
    faker->pbftEngine()->asyncSubmitProposal(
        ref(*blockData), block->blockHeader()->number(), block->blockHeader()->hash(), nullptr);

    // the next leader seal the next block
    IndexType nextLeaderIndex = 1;
    auto nextLeaderFacker = fakerMap[nextLeaderIndex];
    auto nextBlock = expectedProposal + 1;
    block = fakeBlock(cryptoSuite, nextLeaderFacker, nextBlock, 10);
    blockData = std::make_shared<bytes>();
    block->encode(*blockData);
    nextLeaderFacker->pbftEngine()->asyncSubmitProposal(
        ref(*blockData), block->blockHeader()->number(), block->blockHeader()->hash(), nullptr);

    // handle prepare message and broadcast commit messages
    while (!shouldExit(fakerMap, currentBlockNumber + 2))
    {
        for (auto const& node : fakerMap)
        {
            node.second->pbftEngine()->executeWorker();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // supplement expectedProposal + 2
    faker = fakerMap[2];
    block = fakeBlock(cryptoSuite, faker, currentBlockNumber + 3, 10);
    blockData = std::make_shared<bytes>();
    block->encode(*blockData);
    faker->pbftEngine()->asyncSubmitProposal(
        ref(*blockData), block->blockHeader()->number(), block->blockHeader()->hash(), nullptr);

    while (!shouldExit(fakerMap, currentBlockNumber + 4))
    {
        for (auto const& node : fakerMap)
        {
            node.second->pbftEngine()->executeWorker();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

BOOST_AUTO_TEST_CASE(testHandlePrePrepareMsg)
{
    auto hashImpl = std::make_shared<Keccak256Hash>();
    auto signatureImpl = std::make_shared<Secp256k1SignatureImpl>();
    auto cryptoSuite = std::make_shared<CryptoSuite>(hashImpl, signatureImpl, nullptr);

    size_t consensusNodeSize = 2;
    size_t currentBlockNumber = 10;
    auto fakerMap = createFakers(cryptoSuite, consensusNodeSize, currentBlockNumber);

    auto expectedIndex = (fakerMap[0])->pbftConfig()->progressedIndex();
    auto expectedLeader = (fakerMap[0])->pbftConfig()->leaderIndex(expectedIndex);
    auto leaderFaker = fakerMap[expectedLeader];
    auto nonLeaderFaker = fakerMap[(expectedLeader + 1) % consensusNodeSize];

    // case1: invalid block number
    auto hash = hashImpl->hash(std::string("invalidCase"));
    auto leaderMsgFixture =
        std::make_shared<PBFTMessageFixture>(cryptoSuite, leaderFaker->keyPair());
    auto index = (expectedIndex - 1);
    auto pbftMsg = fakePBFTMessage(utcTime(), 1, leaderFaker->pbftConfig()->view(), expectedLeader,
        hash, index, bytes(), 0, leaderMsgFixture, PacketType::PrePreparePacket);
    auto data = leaderFaker->pbftConfig()->codec()->encode(pbftMsg);
    auto decodedMsg = leaderFaker->pbftConfig()->codec()->decode(ref(*data));
    nonLeaderFaker->pbftEngine()->onReceivePBFTMessage(
        nullptr, leaderFaker->keyPair()->publicKey(), ref(*data), nullptr);
    nonLeaderFaker->pbftEngine()->executeWorker();
    BOOST_CHECK(!nonLeaderFaker->pbftEngine()->cacheProcessor()->existPrePrepare(pbftMsg));


#if 0
    fakePBFTMessage(int64_t orgTimestamp, int32_t version, ViewType view,
    IndexType generatedFrom, HashType const& proposalHash, BlockNumber _index, bytes const& _data,
    size_t proposalSize, std::shared_ptr<PBFTMessageFixture> _faker, PacketType _packetType)
       // case1: invalid view
    auto view = 10;
    
    faker->pbftConfig()->setView(view);
    auto data = pbftMsg->encode();

#endif


#if 0
    inline PBFTFixture::Ptr createPBFTFixture(CryptoSuite::Ptr _cryptoSuite,
    FakeLedger::Ptr _ledger = nullptr, size_t _consensusTimeout = 3, size_t _txCountLimit = 1000)
#endif
}
BOOST_AUTO_TEST_SUITE_END()
}  // namespace test
}  // namespace bcos