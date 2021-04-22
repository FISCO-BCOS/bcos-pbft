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
 * @file PBFTMessage.h
 * @author: yujiechen
 * @date 2021-04-13
 */
#pragma once
#include "PBFTBaseMessage.h"
#include "pbft/config/PBFTConfig.h"
#include "pbft/protocol/interfaces/PBFTMessageInterface.h"
#include "pbft/protocol/proto/PBFT.pb.h"

namespace bcos
{
namespace consensus
{
class PBFTMessage : public PBFTBaseMessage, public PBFTMessageInterface
{
public:
    using Ptr = std::shared_ptr<PBFTMessage>;
    PBFTMessage()
      : PBFTBaseMessage(),
        m_pbftRawMessage(std::make_shared<PBFTRawMessage>()),
        m_proposals(std::make_shared<ProposalList>())
    {}

    explicit PBFTMessage(std::shared_ptr<PBFTRawMessage> _pbftRawMessage) : PBFTBaseMessage()
    {
        m_pbftRawMessage = _pbftRawMessage;
        m_proposals = std::make_shared<ProposalList>();
        auto const& hashFieldsData = m_pbftRawMessage->hashfieldsdata();
        auto baseMessageData =
            bytesConstRef((byte const*)hashFieldsData.c_str(), hashFieldsData.size());
        PBFTBaseMessage::decode(baseMessageData);
        PBFTMessage::deserializeToObject();
    }

    PBFTMessage(bcos::crypto::CryptoSuite::Ptr _cryptoSuite, bytesConstRef _data) : PBFTMessage()
    {
        decodeAndSetSignature(_cryptoSuite, _data);
    }

    ~PBFTMessage() override
    {
        // return the ownership of rawProposal to the passed-in proposal
        auto allocatedProposalSize = m_pbftRawMessage->proposals_size();
        for (int i = 0; i < allocatedProposalSize; i++)
        {
            m_pbftRawMessage->mutable_proposals()->UnsafeArenaReleaseLast();
        }
    }

    std::shared_ptr<PBFTRawMessage> pbftRawMessage() { return m_pbftRawMessage; }
    bytesPointer encode(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair) const override;
    void decode(bytesConstRef _data) override;

    void setProposals(ProposalList const& _proposals) override;
    ProposalList const& proposals() const override { return *m_proposals; }

    virtual void decodeAndSetSignature(
        bcos::crypto::CryptoSuite::Ptr _pbftConfig, bytesConstRef _data);

    bool operator==(PBFTMessage const& _pbftMessage);

protected:
    void deserializeToObject() override;
    virtual bcos::crypto::HashType getHashFieldsDataHash(
        bcos::crypto::CryptoSuite::Ptr _cryptoSuite) const;
    virtual void generateAndSetSignatureData(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair) const;

private:
    std::shared_ptr<PBFTRawMessage> m_pbftRawMessage;
    ProposalListPtr m_proposals;
};
}  // namespace consensus
}  // namespace bcos