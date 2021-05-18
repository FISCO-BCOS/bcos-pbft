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
 * @file LedgerStorage.cpp
 * @author: yujiechen
 * @date 2021-04-26
 */
#include "LedgerStorage.h"
#include "pbft/utilities/Common.h"
#include <bcos-framework/interfaces/protocol/ProtocolTypeDef.h>

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::ledger;
using namespace bcos::protocol;
void LedgerStorage::asyncCommitProposal(PBFTProposalInterface::Ptr _committedProposal)
{
    if (m_maxCommittedProposalIndex.load() < _committedProposal->index())
    {
        m_maxCommittedProposalIndex.store(_committedProposal->index());
    }
    PBFT_LOG(INFO) << LOG_DESC("asyncCommitProposal: write the committed proposal into db")
                   << LOG_KV("index", _committedProposal->index());
    // commit the max-index proposal information
    auto maxIndexStr = std::to_string(m_maxCommittedProposalIndex);
    auto maxIndexData = std::make_shared<bytes>(maxIndexStr.begin(), maxIndexStr.end());
    asyncPutProposal(
        m_pbftCommitDB, m_maxCommittedProposalKey, maxIndexData, _committedProposal->index());

    // commit the data
    auto proposalKey = std::make_shared<std::string>(std::to_string(_committedProposal->index()));
    auto encodedData = _committedProposal->encode();
    asyncPutProposal(m_pbftCommitDB, proposalKey, encodedData, _committedProposal->index());
}

void LedgerStorage::asyncPutProposal(std::shared_ptr<std::string> _dbName,
    std::shared_ptr<std::string> _key, bytesPointer _committedData,
    bcos::protocol::BlockNumber _proposalIndex)
{
    auto self = std::weak_ptr<LedgerStorage>(shared_from_this());
    m_storage->asyncPut(_dbName, _key, _committedData,
        [_dbName, _committedData, _key, _proposalIndex, self](Error::Ptr _error) {
            if (_error->errorCode() == bcos::protocol::CommonError::SUCCESS)
            {
                PBFT_LOG(INFO) << LOG_DESC("asyncPutProposal: commit success")
                               << LOG_KV("dbName", _dbName) << LOG_KV("key", _key)
                               << LOG_KV("number", _proposalIndex);
                return;
            }
            PBFT_LOG(WARNING) << LOG_DESC("asyncPutProposal failed: retry again")
                              << LOG_KV("proposalIndex", _proposalIndex) << LOG_KV("key", _key)
                              << LOG_KV("dbName", _dbName)
                              << LOG_KV("errorCode", _error->errorCode())
                              << LOG_KV("errorMessage", _error->errorMessage());
            try
            {
                auto ledgerStorage = self.lock();
                if (!ledgerStorage)
                {
                    return;
                }
                ledgerStorage->asyncPutProposal(_dbName, _key, _committedData, _proposalIndex);
            }
            catch (std::exception const& e)
            {
                PBFT_LOG(WARNING) << LOG_DESC("asyncPutProposal exception")
                                  << LOG_KV("error", boost::diagnostic_information(e));
            }
        });
}

void LedgerStorage::asyncCommmitStableCheckPoint(PBFTProposalInterface::Ptr _stableProposal)
{
    std::shared_ptr<std::vector<protocol::Signature>> signatureList =
        std::make_shared<std::vector<protocol::Signature>>();
    for (size_t i = 0; i < _stableProposal->signatureProofSize(); i++)
    {
        auto proof = _stableProposal->signatureProof(i);
        Signature signature;
        signature.index = proof.first;
        signature.signature = proof.second.toBytes();
        signatureList->push_back(signature);
    }
    auto blockHeader =
        m_blockFactory->blockHeaderFactory()->createBlockHeader(_stableProposal->data());
    blockHeader->setSignatureList(*signatureList);
    asyncCommitStableCheckPoint(blockHeader);
}

void LedgerStorage::asyncCommitStableCheckPoint(BlockHeader::Ptr _blockHeader)
{
    auto self = std::weak_ptr<LedgerStorage>(shared_from_this());
    m_ledger->asyncCommitBlock(
        _blockHeader, [_blockHeader, self](Error::Ptr _error, LedgerConfig::Ptr _ledgerConfig) {
            try
            {
                auto ledgerStorage = self.lock();
                if (!ledgerStorage)
                {
                    return;
                }
                if (_error->errorCode() != CommonError::SUCCESS)
                {
                    PBFT_LOG(ERROR) << LOG_DESC("asyncCommitStableCheckPoint failed")
                                    << LOG_KV("errorCode", _error->errorCode())
                                    << LOG_KV("errorInfo", _error->errorMessage())
                                    << LOG_KV("proposalIndex", _blockHeader->number());
                    // retry to commit
                    ledgerStorage->asyncCommitStableCheckPoint(_blockHeader);
                    return;
                }
                PBFT_LOG(INFO) << LOG_DESC("asyncCommitStableCheckPoint success")
                               << LOG_KV("index", _blockHeader->number())
                               << LOG_KV("hash", _ledgerConfig->hash().abridged());
                // resetConfig
                if (ledgerStorage->m_resetConfigHandler)
                {
                    ledgerStorage->m_resetConfigHandler(_ledgerConfig);
                }
                // finalize consensus
                if (ledgerStorage->m_finalizeHandler)
                {
                    ledgerStorage->m_finalizeHandler(_ledgerConfig);
                }
                // remove the proposal committed into the ledger
                ledgerStorage->asyncRemoveStabledCheckPoint(_blockHeader->number());
            }
            catch (std::exception const& e)
            {
                PBFT_LOG(WARNING) << LOG_DESC("asyncCommitStableCheckPoint exception")
                                  << LOG_KV("error", boost::diagnostic_information(e));
            }
        });
}

void LedgerStorage::asyncRemoveStabledCheckPoint(size_t _stabledCheckPointIndex)
{
    PBFT_LOG(INFO) << LOG_DESC("asyncRemoveStabledCheckPoint")
                   << LOG_KV("index", _stabledCheckPointIndex);
    auto key = std::make_shared<std::string>(std::to_string(_stabledCheckPointIndex));
    asyncRemove(m_pbftCommitDB, key);
}

void LedgerStorage::asyncRemove(
    std::shared_ptr<std::string> _dbName, std::shared_ptr<std::string> _key)
{
    m_storage->asyncRemove(_dbName, _key, [_dbName, _key](Error::Ptr _error) {
        if (_error->errorCode() == bcos::protocol::CommonError::SUCCESS)
        {
            PBFT_LOG(INFO) << LOG_DESC("asyncRemove success") << LOG_KV("dbName", _dbName)
                           << LOG_KV("key", _key);
            return;
        }
        // TODO: remove failed
        PBFT_LOG(WARNING) << LOG_DESC("asyncRemove failed") << LOG_KV("dbName", _dbName)
                          << LOG_KV("key", _key);
    });
}
