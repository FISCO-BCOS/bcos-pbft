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
 * @brief PB implementation for PBFT Message
 * @file PBFTMessage.cpp
 * @author: yujiechen
 * @date 2021-04-13
 */
#include "PBFTMessage.h"
#include "core/Proposal.h"
#include "pbft/protocol/PB/PBFTProposal.h"

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::crypto;
using namespace bcos::protocol;

bytesPointer PBFTMessage::encode(
    CryptoSuite::Ptr _cryptoSuite, KeyPairInterface::Ptr _keyPair) const
{
    // encode the PBFTBaseMessage
    auto hashFieldsData = PBFTBaseMessage::encode(_cryptoSuite, _keyPair);
    m_pbftRawMessage->set_hashfieldsdata(hashFieldsData->data(), hashFieldsData->size());
    generateAndSetSignatureData(_cryptoSuite, _keyPair);
    return encodePBObject(m_pbftRawMessage);
}

void PBFTMessage::decode(bytesConstRef _data)
{
    decodePBObject(m_pbftRawMessage, _data);
    auto const& hashFieldsData = m_pbftRawMessage->hashfieldsdata();
    auto baseMessageData =
        bytesConstRef((byte const*)hashFieldsData.c_str(), hashFieldsData.size());
    PBFTBaseMessage::decode(baseMessageData);
    PBFTMessage::deserializeToObject();
}

void PBFTMessage::deserializeToObject()
{
    PBFTBaseMessage::deserializeToObject();
    // decode the proposals
    m_proposals->clear();
    for (int i = 0; i < m_pbftRawMessage->proposals_size(); i++)
    {
        std::shared_ptr<PBFTRawProposal> rawProposal(m_pbftRawMessage->mutable_proposals(i));
        m_proposals->push_back(std::make_shared<PBFTProposal>(rawProposal));
    }
}

void PBFTMessage::decodeAndSetSignature(CryptoSuite::Ptr _cryptoSuite, bytesConstRef _data)
{
    decode(_data);
    auto dataHash = getHashFieldsDataHash(_cryptoSuite);
    setSignatureDataHash(dataHash);

    auto const& signatureData = m_pbftRawMessage->signaturedata();
    bytes signatureDataBytes(signatureData.begin(), signatureData.end());
    setSignatureData(std::move(signatureDataBytes));
}

HashType PBFTMessage::getHashFieldsDataHash(CryptoSuite::Ptr _cryptoSuite) const
{
    auto const& hashFieldsData = m_pbftRawMessage->hashfieldsdata();
    auto hashFieldsDataRef =
        bytesConstRef((byte const*)hashFieldsData.data(), hashFieldsData.size());
    return _cryptoSuite->hash(hashFieldsDataRef);
}

void PBFTMessage::generateAndSetSignatureData(
    CryptoSuite::Ptr _cryptoSuite, KeyPairInterface::Ptr _keyPair) const
{
    auto dataHash = getHashFieldsDataHash(_cryptoSuite);
    auto signature = _cryptoSuite->signatureImpl()->sign(_keyPair, dataHash, false);
    // set the signature data
    m_pbftRawMessage->set_signaturedata(signature->data(), signature->size());
}

void PBFTMessage::setProposals(ProposalList const& _proposals)
{
    *m_proposals = _proposals;
    for (auto proposal : _proposals)
    {
        auto proposalImpl = std::dynamic_pointer_cast<PBFTProposal>(proposal);
        assert(proposalImpl);
        m_pbftRawMessage->mutable_proposals()->UnsafeArenaAddAllocated(
            proposalImpl->pbftRawProposal().get());
    }
}

bool PBFTMessage::operator==(PBFTMessage const& _pbftMessage)
{
    if (!PBFTBaseMessage::operator==(_pbftMessage))
    {
        return false;
    }
    // check proposal
    for (size_t i = 0; i < _pbftMessage.proposals().size(); i++)
    {
        auto proposal = std::dynamic_pointer_cast<PBFTProposal>((*m_proposals)[i]);
        auto comparedProposal =
            std::dynamic_pointer_cast<PBFTProposal>((_pbftMessage.proposals())[i]);
        if (*proposal != *comparedProposal)
        {
            return false;
        }
    }
    return true;
}