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
    PBFTMessage(bcos::crypto::CryptoSuite::Ptr _cryptoSuite, bcos::crypto::PublicPtr _pubKey,
        bytesConstRef _data)
      : PBFTBaseMessage()
    {
        m_proposals = std::make_shared<ProposalList>();
        m_pbPBFTMessage = std::make_shared<PBPBFTMessage>();
        decodeAndVerify(_cryptoSuite, _pubKey, _data);
    }

    bytesPointer encode(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair) const override;
    void decode(bytesConstRef _data) override;

    void setProposals(ProposalListPtr _proposals) override;
    ProposalListPtr proposals() const override { return m_proposals; }

    virtual void decodeAndVerify(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::PublicPtr _pubKey, bytesConstRef _data);

protected:
    virtual bcos::crypto::HashType getHashFieldsDataHash(
        bcos::crypto::CryptoSuite::Ptr _cryptoSuite) const;
    virtual void generateAndSetSignatureData(bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair) const;

    virtual void verifySignature(
        bcos::crypto::CryptoSuite::Ptr _cryptoSuite, bcos::crypto::PublicPtr _pubKey);

private:
    std::shared_ptr<PBPBFTMessage> m_pbPBFTMessage;
    ProposalListPtr m_proposals;
};
}  // namespace consensus
}  // namespace bcos