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
 * @brief implementation for PBFTCodec
 * @file PBFTCodec.h
 * @author: yujiechen
 * @date 2021-04-13
 */
#pragma once
#include "pbft/protocol/interfaces/PBFTCodecInterface.h"
#include <bcos-framework/interfaces/crypto/CryptoSuite.h>
#include <bcos-framework/interfaces/crypto/KeyPairInterface.h>
namespace bcos
{
namespace consensus
{
class PBFTCodec : public PBFTCodecInterface
{
public:
    using Ptr = std::shared_ptr<PBFTCodec>;
    PBFTCodec(
        bcos::crypto::KeyPairInterface::Ptr _keyPair, bcos::crypto::CryptoSuite::Ptr _cryptoSuite)
      : m_keyPair(_keyPair), m_cryptoSuite(_cryptoSuite)
    {}
    ~PBFTCodec() override {}

    bytesPointer encode(
        PBFTBaseMessageInterface::Ptr _pbftMessage, int32_t _version = 0) const override;

    PBFTBaseMessageInterface::Ptr decode(
        bcos::crypto::PublicPtr _pubKey, bytesConstRef _data) const override;

protected:
    virtual bool shouldHandleSignature(PacketType _packetType) const
    {
        return (_packetType == PacketType::ViewChangePacket ||
                _packetType == PacketType::NewViewPacket);
    }
    virtual bytesPointer signPayLoad(bytesPointer _payLoadData) const;
    virtual void verifySignature(bcos::crypto::PublicPtr _pubKey, bytesConstRef _payLoad,
        bytesConstRef _signatureData) const;

private:
    bcos::crypto::KeyPairInterface::Ptr m_keyPair;
    bcos::crypto::CryptoSuite::Ptr m_cryptoSuite;
};
}  // namespace consensus
}  // namespace bcos