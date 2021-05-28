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
 * @brief factory for PBFTMessage
 * @file PBFTMessageFactory.h
 * @author: yujiechen
 * @date 2021-04-20
 */
#pragma once
#include "NewViewMsgInterface.h"
#include "PBFTMessageInterface.h"
#include "PBFTProposalInterface.h"
#include "PBFTRequestInterface.h"
#include "ViewChangeMsgInterface.h"
#include <bcos-framework/interfaces/crypto/CryptoSuite.h>
#include <bcos-framework/interfaces/protocol/ProtocolTypeDef.h>
namespace bcos
{
namespace consensus
{
class PBFTMessageFactory
{
public:
    using Ptr = std::shared_ptr<PBFTMessageFactory>;
    PBFTMessageFactory() = default;
    virtual ~PBFTMessageFactory() {}

    virtual PBFTMessageInterface::Ptr createPBFTMsg() = 0;
    virtual PBFTMessageInterface::Ptr createPBFTMsg(
        bcos::crypto::CryptoSuite::Ptr _cryptoSuite, bytesConstRef _data) = 0;

    virtual ViewChangeMsgInterface::Ptr createViewChangeMsg() = 0;
    virtual ViewChangeMsgInterface::Ptr createViewChangeMsg(bytesConstRef _data) = 0;

    virtual NewViewMsgInterface::Ptr createNewViewMsg() = 0;
    virtual NewViewMsgInterface::Ptr createNewViewMsg(bytesConstRef _data) = 0;
    virtual PBFTProposalInterface::Ptr createPBFTProposal() = 0;
    virtual PBFTProposalInterface::Ptr createPBFTProposal(bytesConstRef _data) = 0;

    virtual PBFTRequestInterface::Ptr createPBFTRequest() = 0;
    virtual PBFTRequestInterface::Ptr createPBFTRequest(bytesConstRef _data) = 0;

    virtual PBFTRequestInterface::Ptr populateFrom(
        PacketType _packetType, bcos::protocol::BlockNumber _startIndex, int64_t _offset)
    {
        auto pbftRequest = createPBFTRequest();
        pbftRequest->setPacketType(_packetType);
        pbftRequest->setIndex(_startIndex);
        pbftRequest->setSize(_offset);
        return pbftRequest;
    }

    virtual PBFTRequestInterface::Ptr populateFrom(PacketType _packetType,
        bcos::protocol::BlockNumber _index, bcos::crypto::HashType const& _hash)
    {
        auto pbftRequest = createPBFTRequest();
        pbftRequest->setPacketType(_packetType);
        pbftRequest->setIndex(_index);
        pbftRequest->setHash(_hash);
        return pbftRequest;
    }

    virtual PBFTMessageInterface::Ptr populateFrom(PBFTBaseMessageInterface::Ptr _message)
    {
        auto pbftMessage = createPBFTMsg();
        pbftMessage->setPacketType(_message->packetType());
        pbftMessage->setVersion(_message->version());
        pbftMessage->setView(_message->view());
        pbftMessage->setTimestamp(_message->timestamp());
        pbftMessage->setGeneratedFrom(_message->generatedFrom());
        return pbftMessage;
    }

    virtual PBFTMessageInterface::Ptr populateFrom(PacketType _packetType, int32_t _version,
        ViewType _view, int64_t _timestamp, IndexType _generatedFrom,
        PBFTProposalInterface::Ptr _proposal, bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair, bool _needSign = true)
    {
        auto pbftMessage = createPBFTMsg();
        pbftMessage->setPacketType(_packetType);
        pbftMessage->setVersion(_version);
        pbftMessage->setView(_view);
        pbftMessage->setTimestamp(_timestamp);
        pbftMessage->setGeneratedFrom(_generatedFrom);
        pbftMessage->setHash(_proposal->hash());
        PBFTProposalList populatedProposalList;
        // create the proposal
        auto signedProposal = createPBFTProposal();
        signedProposal->setIndex(_proposal->index());
        signedProposal->setHash(_proposal->hash());
        if (_needSign)
        {
            auto signatureData = _cryptoSuite->signatureImpl()->sign(_keyPair, _proposal->hash());
            signedProposal->setSignature(*signatureData);
        }
        pbftMessage->setConsensusProposal(signedProposal);
        return pbftMessage;
    }

    virtual ViewChangeMsgInterface::Ptr populateFrom(ViewChangeMsgInterface::Ptr _viewChangeReq)
    {
        auto viewChangeReq = createViewChangeMsg();
        viewChangeReq->setVersion(_viewChangeReq->version());
        viewChangeReq->setPacketType(PacketType::ViewChangePacket);
        viewChangeReq->setIndex(_viewChangeReq->index());
        viewChangeReq->setView(_viewChangeReq->view());
        viewChangeReq->setTimestamp(_viewChangeReq->timestamp());
        viewChangeReq->setGeneratedFrom(_viewChangeReq->generatedFrom());
        viewChangeReq->setCommittedProposal(_viewChangeReq->committedProposal());
        return viewChangeReq;
    }

    virtual PBFTProposalInterface::Ptr populateFrom(
        PBFTProposalInterface::Ptr _proposal, bool _withData = true, bool _withProof = true)
    {
        auto proposal = createPBFTProposal();
        proposal->setIndex(_proposal->index());
        proposal->setHash(_proposal->hash());
        if (_withData)
        {
            proposal->setData(_proposal->data().toBytes());
        }
        // set the signature proof
        if (_withProof)
        {
            auto signatureSize = _proposal->signatureProofSize();
            for (size_t i = 0; i < signatureSize; i++)
            {
                auto proof = _proposal->signatureProof(i);
                proposal->appendSignatureProof(proof.first, proof.second);
            }
        }
        return proposal;
    }

    virtual PBFTProposalInterface::Ptr populateEmptyProposal(
        int64_t _index, bcos::crypto::HashType const& _emptyHash)
    {
        auto proposal = createPBFTProposal();
        proposal->setIndex(_index);
        proposal->setHash(_emptyHash);
        return proposal;
    }
};
}  // namespace consensus
}  // namespace bcos
