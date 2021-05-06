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
#include <bcos-framework/interfaces/protocol/CommonError.h>
#include <bcos-framework/interfaces/protocol/ProtocolTypeDef.h>

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::ledger;
using namespace bcos::protocol;

void LedgerStorage::asyncCommitProposal(PBFTProposalInterface::Ptr _proposal)
{
    std::vector<protocol::Signature> signatureList;
    for (size_t i = 0; i < _proposal->signatureProofSize(); i++)
    {
        auto proof = _proposal->signatureProof(i);
        Signature signature;
        signature.index = proof.first;
        signature.signature = proof.second.toBytes();
        signatureList.push_back(signature);
    }
    // TODO: init PBFTConfig according to the information notified by the ledger module
    m_ledger->asyncCommitBlock(_proposal->index(), signatureList,
        [_proposal, this](Error::Ptr _error, LedgerConfig::Ptr _ledgerConfig) {
            if (_error->errorCode() != CommonError::SUCCESS)
            {
                PBFT_LOG(ERROR) << LOG_DESC("Commit proposal failed")
                                << LOG_KV("errorCode", _error->errorCode())
                                << LOG_KV("errorInfo", _error->errorMessage())
                                << LOG_KV("proposal", _proposal->index());
                // TODO: retry or do something
                return;
            }
            // resetConfig
            if (m_resetConfigHandler)
            {
                m_resetConfigHandler(_ledgerConfig);
            }
            // finalize consensus
            if (m_finalizeHandler)
            {
                m_finalizeHandler(_ledgerConfig);
            }
        });
}
