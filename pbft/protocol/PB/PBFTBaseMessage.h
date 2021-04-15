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
 * @brief PB implementation for PBFTBaseMessage
 * @file PBFTBaseMessage.h
 * @author: yujiechen
 * @date 2021-04-13
 */
#pragma once
#include "pbft/protocol/interfaces/PBFTBaseMessageInterface.h"
#include "pbft/protocol/proto/PBFT.pb.h"
#include <bcos-framework/libprotocol/Common.h>
namespace bcos
{
namespace consensus
{
class PBFTBaseMessage : public PBFTBaseMessageInterface
{
public:
    using Ptr = std::shared_ptr<PBFTBaseMessage>;
    PBFTBaseMessage()
      : m_baseMessage(std::make_shared<BaseMessage>()), m_signatureData(std::make_shared<bytes>())
    {}

    explicit PBFTBaseMessage(std::shared_ptr<BaseMessage> _baseMessage)
      : m_baseMessage(_baseMessage), m_signatureData(std::make_shared<bytes>())
    {}

    ~PBFTBaseMessage() override {}

    int64_t timestamp() const override { return m_baseMessage->timestamp(); }
    int32_t version() const override { return m_baseMessage->version(); }
    int64_t view() const override { return m_baseMessage->view(); }
    int64_t generatedFrom() const override { return m_baseMessage->generatedfrom(); }
    bcos::crypto::HashType const& hash() const override { return m_hash; }

    void setTimestamp(int64_t _timestamp) override { m_baseMessage->set_timestamp(_timestamp); }
    void setVersion(int32_t _version) override { m_baseMessage->set_version(_version); }
    void setView(ViewType _view) override { m_baseMessage->set_view(_view); }
    void setGeneratedFrom(int64_t _generatedFrom) override
    {
        m_baseMessage->set_generatedfrom(_generatedFrom);
    }

    void setHash(bcos::crypto::HashType const& _hash) override
    {
        m_hash = _hash;
        m_baseMessage->set_hash(m_hash.data(), bcos::crypto::HashType::size);
    }

    PacketType packetType() const override { return m_packetType; }
    void setPacketType(PacketType _packetType) override { m_packetType = _packetType; }

    bytesPointer encode(
        bcos::crypto::CryptoSuite::Ptr, bcos::crypto::KeyPairInterface::Ptr) const override
    {
        return bcos::protocol::encodePBObject(m_baseMessage);
    }
    void decode(bytesConstRef _data) override
    {
        bcos::protocol::decodePBObject(m_baseMessage, _data);
        auto const& hashData = m_baseMessage->hash();
        m_hash =
            bcos::crypto::HashType((byte const*)hashData.c_str(), bcos::crypto::HashType::size);
    }

    bytes const& signatureData() override { return *m_signatureData; }
    bcos::crypto::HashType const& signatureDataHash() override { return m_dataHash; }
    void setSignatureData(bytes&& _signatureData) override
    {
        *m_signatureData = std::move(_signatureData);
    }
    void setSignatureData(bytes const& _signatureData) override
    {
        *m_signatureData = _signatureData;
    }
    void setSignatureDataHash(bcos::crypto::HashType const& _hash) override { m_dataHash = _hash; }
    bool verifySignature(
        bcos::crypto::CryptoSuite::Ptr _cryptoSuite, bcos::crypto::PublicPtr _pubKey) override
    {
        return _cryptoSuite->signatureImpl()->verify(_pubKey, m_dataHash, ref(*m_signatureData));
    }

private:
    std::shared_ptr<BaseMessage> m_baseMessage;
    bcos::crypto::HashType m_hash;
    PacketType m_packetType;

    bcos::crypto::HashType m_dataHash;
    bytesPointer m_signatureData;
};
}  // namespace consensus
}  // namespace bcos
