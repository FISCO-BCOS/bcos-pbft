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
inline bool shouldExit(std::map<IndexType, PBFTFixture::Ptr>& _consensusNodes,
    BlockNumber _expectedNumber, size_t _connectedNodes)
{
    for (IndexType i = 0; i < _connectedNodes; i++)
    {
        auto faker = _consensusNodes[i];
        if (faker->ledger()->blockNumber() != _expectedNumber)
        {
            return false;
        }
    }
    return true;
}

void testPBFTEngineWithFaulty(size_t _consensusNodes, size_t _connectedNodes)
{
    auto hashImpl = std::make_shared<Keccak256Hash>();
    auto signatureImpl = std::make_shared<Secp256k1SignatureImpl>();
    auto cryptoSuite = std::make_shared<CryptoSuite>(hashImpl, signatureImpl, nullptr);

    BlockNumber currentBlockNumber = 19;
    auto fakerMap = createFakers(cryptoSuite, _consensusNodes, currentBlockNumber, _connectedNodes);

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
    while (!shouldExit(fakerMap, currentBlockNumber + 2, _connectedNodes))
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

    while (!shouldExit(fakerMap, currentBlockNumber + 4, _connectedNodes))
    {
        for (auto const& node : fakerMap)
        {
            node.second->pbftEngine()->executeWorker();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

BOOST_AUTO_TEST_CASE(testPBFTEngineWithAllNonFaulty)
{
    size_t consensusNodeSize = 10;
    // case1: all non-faulty
    testPBFTEngineWithFaulty(consensusNodeSize, consensusNodeSize);
    // case2: with f=3 faulty
    testPBFTEngineWithFaulty(consensusNodeSize, 7);
}

BOOST_AUTO_TEST_CASE(testHandlePrePrepareMsg)
{
    auto hashImpl = std::make_shared<Keccak256Hash>();
    auto signatureImpl = std::make_shared<Secp256k1SignatureImpl>();
    auto cryptoSuite = std::make_shared<CryptoSuite>(hashImpl, signatureImpl, nullptr);

    size_t consensusNodeSize = 2;
    size_t currentBlockNumber = 10;
    auto fakerMap =
        createFakers(cryptoSuite, consensusNodeSize, currentBlockNumber, consensusNodeSize);

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
    nonLeaderFaker->pbftEngine()->onReceivePBFTMessage(
        nullptr, nonLeaderFaker->keyPair()->publicKey(), ref(*data), nullptr);
    nonLeaderFaker->pbftEngine()->executeWorker();
    BOOST_CHECK(!nonLeaderFaker->pbftEngine()->cacheProcessor()->existPrePrepare(pbftMsg));

    // case2: invalid view
    index = expectedIndex;
    ViewType view = 10;
    for (auto node : fakerMap)
    {
        node.second->pbftConfig()->setView(view);
    }
    expectedLeader = (fakerMap[0])->pbftConfig()->leaderIndex(index);
    leaderFaker = fakerMap[expectedLeader];
    pbftMsg = fakePBFTMessage(utcTime(), 1, (leaderFaker->pbftConfig()->view() - 1), expectedLeader,
        hash, index, bytes(), 0, leaderMsgFixture, PacketType::PrePreparePacket);
    data = leaderFaker->pbftConfig()->codec()->encode(pbftMsg);
    nonLeaderFaker = fakerMap[(expectedLeader + 1) % consensusNodeSize];
    nonLeaderFaker->pbftEngine()->onReceivePBFTMessage(
        nullptr, nonLeaderFaker->keyPair()->publicKey(), ref(*data), nullptr);
    nonLeaderFaker->pbftEngine()->executeWorker();
    BOOST_CHECK(!nonLeaderFaker->pbftEngine()->cacheProcessor()->existPrePrepare(pbftMsg));

    // case3: not from the leader
    pbftMsg = fakePBFTMessage(utcTime(), 1, (nonLeaderFaker->pbftConfig()->view()),
        (expectedLeader + 1) % consensusNodeSize, hash, index, bytes(), 0, leaderMsgFixture,
        PacketType::PrePreparePacket);
    data = nonLeaderFaker->pbftConfig()->codec()->encode(pbftMsg);
    leaderFaker->pbftEngine()->onReceivePBFTMessage(
        nullptr, leaderFaker->keyPair()->publicKey(), ref(*data), nullptr);
    leaderFaker->pbftEngine()->executeWorker();
    BOOST_CHECK(!leaderFaker->pbftEngine()->cacheProcessor()->existPrePrepare(pbftMsg));

    // case4: invalid signature
    pbftMsg = fakePBFTMessage(utcTime(), 1, (leaderFaker->pbftConfig()->view()), expectedLeader,
        hash, index, bytes(), 0, leaderMsgFixture, PacketType::PrePreparePacket);
    data = nonLeaderFaker->pbftConfig()->codec()->encode(pbftMsg);
    nonLeaderFaker->pbftEngine()->onReceivePBFTMessage(
        nullptr, nonLeaderFaker->keyPair()->publicKey(), ref(*data), nullptr);
    nonLeaderFaker->pbftEngine()->executeWorker();
    BOOST_CHECK(!nonLeaderFaker->pbftEngine()->cacheProcessor()->existPrePrepare(pbftMsg));

    // case5: invalid pre-prepare for txpool verify failed
    data = leaderFaker->pbftConfig()->codec()->encode(pbftMsg);
    nonLeaderFaker->txpool()->setVerifyResult(false);
    nonLeaderFaker->pbftEngine()->onReceivePBFTMessage(
        nullptr, nonLeaderFaker->keyPair()->publicKey(), ref(*data), nullptr);
    nonLeaderFaker->pbftEngine()->executeWorker();
    BOOST_CHECK(!nonLeaderFaker->pbftEngine()->cacheProcessor()->existPrePrepare(pbftMsg));

    // case6: valid pre-prepare
    nonLeaderFaker->txpool()->setVerifyResult(true);
    nonLeaderFaker->pbftEngine()->onReceivePBFTMessage(
        nullptr, nonLeaderFaker->keyPair()->publicKey(), ref(*data), nullptr);
    nonLeaderFaker->pbftEngine()->executeWorker();
    BOOST_CHECK(nonLeaderFaker->pbftEngine()->cacheProcessor()->existPrePrepare(pbftMsg));
    nonLeaderFaker->pbftConfig()->setConsensusTimeout(200);
    leaderFaker->pbftConfig()->setConsensusTimeout(200);
    BOOST_CHECK(nonLeaderFaker->pbftConfig()->timer()->running());
    while (!leaderFaker->pbftEngine()->isTimeout() || !nonLeaderFaker->pbftEngine()->isTimeout())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // wait to trigger viewchange since not reach consensus
    while (leaderFaker->pbftEngine()->isTimeout() || nonLeaderFaker->pbftEngine()->isTimeout())
    {
        nonLeaderFaker->pbftEngine()->executeWorker();
        leaderFaker->pbftEngine()->executeWorker();
    }
}
BOOST_AUTO_TEST_SUITE_END()
}  // namespace test
}  // namespace bcos