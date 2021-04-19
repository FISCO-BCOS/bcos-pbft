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
 * @file PBFTCodec.cpp
 * @author: yujiechen
 * @date 2021-04-13
 */
#include "PBFTCodec.h"
#include "PBFTMessage.h"
#include "PBFTNewViewMsg.h"
#include "PBFTViewChangeMsg.h"
#include "pbft/protocol/proto/PBFT.pb.h"
#include <bcos-framework/libprotocol/Common.h>

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::crypto;

bytesPointer PBFTCodec::encode(PBFTBaseMessageInterface::Ptr _pbftMessage, int32_t _version) const
{
    auto pbMessage = std::make_shared<RawMessage>();

    // set packetType
    auto packetType = _pbftMessage->packetType();
    pbMessage->set_type((int32_t)packetType);

    // set payLoad
    auto payLoad = _pbftMessage->encode(m_cryptoSuite, m_keyPair);
    pbMessage->set_payload(payLoad->data(), payLoad->size());

    // set signature
    if (shouldHandleSignature(packetType))
    {
        auto signatureData = signPayLoad(payLoad);
        pbMessage->set_signaturedata(signatureData->data(), signatureData->size());
    }
    // set version
    pbMessage->set_version(_version);
    return bcos::protocol::encodePBObject(pbMessage);
}

PBFTBaseMessageInterface::Ptr PBFTCodec::decode(bytesConstRef _data) const
{
    auto pbMessage = std::make_shared<RawMessage>();
    bcos::protocol::decodePBObject(pbMessage, _data);
    // get packetType
    PacketType packetType = (PacketType)(pbMessage->type());
    // get payLoad
    auto const& payLoad = pbMessage->payload();
    auto payLoadRefData = bytesConstRef((byte const*)payLoad.c_str(), payLoad.size());
    // decode the packet according to the packetType
    PBFTBaseMessageInterface::Ptr decodedMsg = nullptr;
    switch (packetType)
    {
    case PacketType::PrePreparePacket:
    case PacketType::PreparePacket:
    case PacketType::CommitPacket:
        decodedMsg = std::make_shared<PBFTMessage>(m_cryptoSuite, payLoadRefData);
        break;
    case ViewChangePacket:
        decodedMsg = std::make_shared<PBFTViewChangeMsg>(payLoadRefData);
        break;
    case NewViewPacket:
        decodedMsg = std::make_shared<PBFTNewViewMsg>(payLoadRefData);
        break;
    default:
        BOOST_THROW_EXCEPTION(UnknownPBFTMsgType() << errinfo_comment(
                                  "unknow pbft packetType: " + std::to_string(packetType)));
    }
    if (shouldHandleSignature(packetType))
    {
        // set signature data for the message
        auto hash = m_cryptoSuite->hashImpl()->hash(payLoadRefData);
        decodedMsg->setSignatureDataHash(hash);

        auto const& signatureData = pbMessage->signaturedata();
        bytes signatureBytes(signatureData.begin(), signatureData.end());
        decodedMsg->setSignatureData(std::move(signatureBytes));
    }
    decodedMsg->setPacketType(packetType);
    return decodedMsg;
}

bytesPointer PBFTCodec::signPayLoad(bytesPointer _payLoadData) const
{
    // get hash of the payLoad
    auto hash = m_cryptoSuite->hashImpl()->hash(*_payLoadData);
    // sign for the payload
    return m_cryptoSuite->signatureImpl()->sign(m_keyPair, hash, false);
}