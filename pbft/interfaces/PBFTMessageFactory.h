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
#include "pbft/interfaces/NewViewMsgInterface.h"
#include "pbft/interfaces/PBFTMessageInterface.h"
#include "pbft/interfaces/PBFTProposalInterface.h"
#include "pbft/interfaces/ViewChangeMsgInterface.h"
#include <bcos-framework/interfaces/crypto/CryptoSuite.h>
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

    virtual PBFTMessageInterface::Ptr populateFrom(PacketType _packetType, int32_t _version,
        ViewType _view, int64_t _timestamp, IndexType _generatedFrom,
        PBFTProposalList const& _proposals, bcos::crypto::CryptoSuite::Ptr _cryptoSuite,
        bcos::crypto::KeyPairInterface::Ptr _keyPair, bool _needSign = true)
    {
        auto pbftMessage = createPBFTMsg();
        pbftMessage->setPacketType(_packetType);
        pbftMessage->setVersion(_version);
        pbftMessage->setView(_view);
        pbftMessage->setTimestamp(_timestamp);
        pbftMessage->setGeneratedFrom(_generatedFrom);
        PBFTProposalList populatedProposalList;
        for (auto proposal : _proposals)
        {
            // create the proposal
            auto signedProposal = createPBFTProposal();
            signedProposal->setIndex(proposal->index());
            signedProposal->setHash(proposal->hash());
            if (_needSign)
            {
                auto signatureData =
                    _cryptoSuite->signatureImpl()->sign(_keyPair, proposal->hash());
                signedProposal->setSignature(*signatureData);
            }
            populatedProposalList.push_back(signedProposal);
        }
        pbftMessage->setProposals(populatedProposalList);
        return pbftMessage;
    }

    virtual PBFTProposalInterface::Ptr populateFrom(
        PBFTProposalInterface::Ptr _proposal, bool _withData = true)
    {
        auto proposal = createPBFTProposal();
        proposal->setIndex(_proposal->index());
        proposal->setHash(_proposal->hash());
        if (_withData)
        {
            proposal->setData(_proposal->data().toBytes());
        }
        proposal->setView(_proposal->view());
        proposal->setGeneratedFrom(_proposal->generatedFrom());
        // set the signature proof
        auto signatureSize = _proposal->signatureProofSize();
        for (size_t i = 0; i < signatureSize; i++)
        {
            auto proof = _proposal->signatureProof(i);
            proposal->appendSignatureProof(proof.first, proof.second);
        }
        return proposal;
    }
};
}  // namespace consensus
}  // namespace bcos
