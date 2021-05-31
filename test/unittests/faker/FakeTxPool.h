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
 * @brief faker for the TxPool
 * @file FakeTxPool.h
 * @author: yujiechen
 * @date 2021-05-28
 */
#pragma once
#include <bcos-framework/interfaces/protocol/CommonError.h>
#include <bcos-framework/interfaces/txpool/TxPoolInterface.h>

using namespace bcos;
using namespace bcos::txpool;
using namespace bcos::protocol;
using namespace bcos::crypto;
using namespace bcos::consensus;

namespace bcos
{
namespace test
{
class FakeTxPool : public TxPoolInterface
{
public:
    FakeTxPool() = default;
    ~FakeTxPool() override {}

    void start() override {}
    void stop() override {}

    // useless for PBFT, maybe needed by RPC
    void asyncSubmit(bytesPointer, TxSubmitCallback, std::function<void(Error::Ptr)>) override {}
    // useless for PBFT, needed by dispatcher to fetch block transactions
    void asyncFillBlock(HashListPtr, std::function<void(Error::Ptr, TransactionsPtr)>) override {}

    // useless for PBFT, maybe useful for the ledger
    void asyncNotifyBlockResult(
        BlockNumber, TransactionSubmitResultsPtr, std::function<void(Error::Ptr)>) override
    {}

    // called by frontService to dispatch message
    // useless for PBFT
    void asyncNotifyTxsSyncMessage(Error::Ptr, NodeIDPtr, bytesPointer,
        std::function<void(bytesConstRef _respData)>,
        std::function<void(Error::Ptr _error)>) override
    {}

    // notify related interfaces: useless for the PBFT module
    void notifyConnectedNodes(NodeIDSet const&, std::function<void(Error::Ptr)>) override {}
    // notify related interfaces: useless for the PBFT module
    void notifyConsensusNodeList(ConsensusNodeList const&, std::function<void(Error::Ptr)>) override
    {}
    // notify related interfaces: useless for the PBFT module
    void notifyObserverNodeList(ConsensusNodeList const&, std::function<void(Error::Ptr)>) override
    {}

    // TODO: fake this interface for libsealer
    void asyncSealTxs(size_t, TxsHashSetPtr, std::function<void(Error::Ptr, HashListPtr)>) override
    {}

    // TODO: fake this interface for libsealer
    void asyncMarkTxs(HashListPtr, bool, std::function<void(Error::Ptr)>) override {}

    void asyncVerifyBlock(PublicPtr, bytesConstRef const&,
        std::function<void(Error::Ptr, bool)> _onVerifyFinished) override
    {
        if (m_verifyResult)
        {
            _onVerifyFinished(nullptr, m_verifyResult);
            return;
        }
        _onVerifyFinished(
            std::make_shared<Error>(CommonError::TransactionsMissing, "TransactionsMissing"),
            m_verifyResult);
    }

    void setVerifyResult(bool _verifyResult) { m_verifyResult = _verifyResult; }
    bool verifyResult() const { return m_verifyResult; }

private:
    bool m_verifyResult = true;
};
}  // namespace test
}  // namespace bcos