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

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::crypto;

bytesPointer PBFTMessage::encode(
    CryptoSuite::Ptr _cryptoSuite, KeyPairInterface::Ptr _keyPair) const
{
    // encode the PBFTBaseMessage
    auto hashFieldsData = PBFTBaseMessage::encode(_cryptoSuite, _keyPair);
    m_pbPBFTMessage->set_hashfieldsdata(hashFieldsData->data(), hashFieldsData->size());
    generateAndSetSignatureData(_cryptoSuite, _keyPair);
    return bcos::protocol::encodePBObject(m_pbPBFTMessage);
}

void PBFTMessage::decode(bytesConstRef _data)
{
    bcos::protocol::decodePBObject(m_pbPBFTMessage, _data);
    auto const& hashFieldsData = m_pbPBFTMessage->hashfieldsdata();
    auto baseMessageData =
        bytesConstRef((byte const*)hashFieldsData.c_str(), hashFieldsData.size());
    PBFTBaseMessage::decode(baseMessageData);
    // decode the proposals
    m_proposals->clear();
    for (int i = 0; i < m_pbPBFTMessage->proposals_size(); i++)
    {
        std::shared_ptr<PBProposal> pbProposal(m_pbPBFTMessage->mutable_proposals(i));
        m_proposals->push_back(std::make_shared<Proposal>(pbProposal));
    }
}

void PBFTMessage::decodeAndSetSignature(CryptoSuite::Ptr _cryptoSuite, bytesConstRef _data)
{
    decode(_data);
    auto dataHash = getHashFieldsDataHash(_cryptoSuite);
    setSignatureDataHash(dataHash);

    auto const& signatureData = m_pbPBFTMessage->signaturedata();
    bytes signatureDataBytes(signatureData.begin(), signatureData.end());
    setSignatureData(std::move(signatureDataBytes));
}

HashType PBFTMessage::getHashFieldsDataHash(CryptoSuite::Ptr _cryptoSuite) const
{
    auto const& hashFieldsData = m_pbPBFTMessage->hashfieldsdata();
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
    m_pbPBFTMessage->set_signaturedata(signature->data(), signature->size());
}

void PBFTMessage::setProposals(ProposalListPtr _proposals)
{
    for (auto proposal : *_proposals)
    {
        Proposal::Ptr proposalImpl = std::dynamic_pointer_cast<Proposal>(proposal);
        assert(proposalImpl);
        m_pbPBFTMessage->mutable_proposals()->AddAllocated(proposalImpl->pbProposal().get());
    }
    m_proposals = _proposals;
}