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
 * @brief implementation for PBFT log syncing
 * @file PBFTLogSync.cpp
 * @author: yujiechen
 * @date 2021-04-28
 */
#include "PBFTLogSync.h"
#include <bcos-framework/interfaces/protocol/CommonError.h>
#include <bcos-framework/interfaces/protocol/Protocol.h>

using namespace bcos;
using namespace bcos::front;
using namespace bcos::protocol;
using namespace bcos::crypto;
using namespace bcos::consensus;

PBFTLogSync::PBFTLogSync(PBFTConfig::Ptr _config, PBFTCacheProcessor::Ptr _pbftCache)
  : m_config(_config),
    m_pbftCache(_pbftCache),
    m_requestThread(std::make_shared<ThreadPool>("pbftLogSync", 1))
{}

void PBFTLogSync::requestCommittedProposals(
    PublicPtr _from, BlockNumber _startIndex, size_t _offset)
{
    auto pbftRequest = m_config->pbftMessageFactory()->populateFrom(
        PacketType::CommittedProposalRequest, _startIndex, _offset);
    requestPBFTData(_from, pbftRequest,
        [this](Error::Ptr _error, NodeIDPtr _nodeID, bytesConstRef _data,
            SendResponseCallback _sendResponse) {
            return this->onRecvCommittedProposalsResponse(_error, _nodeID, _data, _sendResponse);
        });
}

void PBFTLogSync::requestPrecommitData(
    bcos::crypto::PublicPtr _from, BlockNumber _index, bcos::crypto::HashType const& _hash)
{
    auto pbftRequest = m_config->pbftMessageFactory()->populateFrom(
        PacketType::PreparedProposalRequest, _index, _hash);
    requestPBFTData(_from, pbftRequest,
        [this](Error::Ptr _error, NodeIDPtr _nodeID, bytesConstRef _data,
            SendResponseCallback _sendResponse) {
            return this->onRecvPrecommitResponse(_error, _nodeID, _data, _sendResponse);
        });
}

void PBFTLogSync::requestPBFTData(
    PublicPtr _from, PBFTRequestInterface::Ptr _pbftRequest, CallbackFunc _callback)
{
    auto self = std::weak_ptr<PBFTLogSync>(shared_from_this());
    m_requestThread->enqueue([self, _from, _pbftRequest, _callback]() {
        try
        {
            auto pbftLogSync = self.lock();
            if (!pbftLogSync)
            {
                return;
            }
            auto config = pbftLogSync->m_config;
            // encode
            auto encodedData =
                config->codec()->encode(_pbftRequest, config->pbftMsgDefaultVersion());
            // send the request
            config->frontService()->asyncSendMessageByNodeID(ModuleID::PBFT, _from,
                ref(*encodedData), config->networkTimeoutInterval(), _callback);
        }
        catch (std::exception const& e)
        {
            PBFT_LOG(WARNING) << LOG_DESC("requestCommittedProposals exception")
                              << LOG_KV("to", _from->shortHex())
                              << LOG_KV("startIndex", _pbftRequest->index())
                              << LOG_KV("offset", _pbftRequest->size())
                              << LOG_KV("hash", _pbftRequest->hash().abridged())
                              << LOG_KV("error", boost::diagnostic_information(e));
        }
    });
}

void PBFTLogSync::onRecvCommittedProposalsResponse(
    Error::Ptr _error, NodeIDPtr _nodeID, bytesConstRef _data, SendResponseCallback)
{
    if (_error->errorCode() != CommonError::SUCCESS)
    {
        PBFT_LOG(WARNING) << LOG_DESC("onRecvCommittedProposalResponse error")
                          << LOG_KV("from", _nodeID->shortHex())
                          << LOG_KV("errorCode", _error->errorCode())
                          << LOG_KV("errorMsg", _error->errorMessage());
    }
    auto response = m_config->codec()->decode(_data);
    if (response->packetType() != PacketType::CommittedProposalResponse)
    {
        return;
    }
    auto proposalResponse = std::dynamic_pointer_cast<PBFTMessageInterface>(response);
    // TODO: check the proposal to ensure security
    // commit the proposals
    auto proposals = proposalResponse->proposals();
    for (auto proposal : proposals)
    {
        m_config->storage()->asyncCommitProposal(proposal->index(), proposal->data());
    }
}

void PBFTLogSync::onReceivePrecommitRequest(
    std::shared_ptr<PBFTBaseMessageInterface> _pbftMessage, SendResponseCallback _sendResponse)
{
    // receive the precommitted proposals request message
    auto pbftRequest = std::dynamic_pointer_cast<PBFTRequestInterface>(_pbftMessage);
    auto self = std::weak_ptr<PBFTLogSync>(shared_from_this());
    m_requestThread->enqueue([self, pbftRequest, _sendResponse]() {
        try
        {
            auto pbftLogSync = self.lock();
            if (!pbftLogSync)
            {
                return;
            }
            // get the local precommitData
            auto pbftMessage = pbftLogSync->m_config->pbftMessageFactory()->createPBFTMsg();
            pbftMessage->setPacketType(PacketType::PreparedProposalResponse);
            auto precommitData = pbftLogSync->m_pbftCache->precommitProposalData(
                pbftMessage, pbftRequest->index(), pbftRequest->hash());
            // response the precommitData
            _sendResponse(ref(*precommitData));
            PBFT_LOG(DEBUG) << LOG_DESC("Receive precommitRequest and send response")
                            << LOG_KV("hash", pbftRequest->hash().abridged())
                            << LOG_KV("index", pbftRequest->index());
        }
        catch (std::exception const& e)
        {
            PBFT_LOG(WARNING) << LOG_DESC("handle PreparedProposalRequest exception")
                              << LOG_KV("error", boost::diagnostic_information(e));
        }
    });
}

void PBFTLogSync::onReceiveCommittedProposalRequest(
    PBFTBaseMessageInterface::Ptr _pbftMsg, SendResponseCallback _sendResponse)
{
    auto pbftRequest = std::dynamic_pointer_cast<PBFTRequestInterface>(_pbftMsg);
    PBFT_LOG(DEBUG) << LOG_DESC("Receive CommittedProposalRequest")
                    << LOG_KV("fromIndex", pbftRequest->index())
                    << LOG_KV("size", pbftRequest->size());
    m_config->storage()->asyncGetCommittedProposals(pbftRequest->index(), pbftRequest->size(),
        [this, _sendResponse](PBFTProposalList const& _proposalList) {
            auto pbftMessage = m_config->pbftMessageFactory()->createPBFTMsg();
            pbftMessage->setPacketType(PacketType::CommittedProposalResponse);
            pbftMessage->setProposals(_proposalList);
            auto encodedData = m_config->codec()->encode(pbftMessage);
            _sendResponse(ref(*encodedData));
        });
}


void PBFTLogSync::onRecvPrecommitResponse(
    Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID, bytesConstRef _data, SendResponseCallback)
{
    if (_error->errorCode() != CommonError::SUCCESS)
    {
        PBFT_LOG(WARNING) << LOG_DESC("onRecvPrecommitResponse error")
                          << LOG_KV("from", _nodeID->shortHex())
                          << LOG_KV("errorCode", _error->errorCode())
                          << LOG_KV("errorMsg", _error->errorMessage());
    }
    auto response = m_config->codec()->decode(_data);
    if (response->packetType() != PacketType::PreparedProposalResponse)
    {
        return;
    }
    auto pbftMessage = std::dynamic_pointer_cast<PBFTMessageInterface>(response);
    for (auto proposal : pbftMessage->proposals())
    {
        if (!m_pbftCache->checkPrecommitProposal(proposal))
        {
            PBFT_LOG(WARNING) << LOG_DESC("Recv invalid precommit response")
                              << printPBFTProposal(proposal);
            return;
        }
        // fill the fetched proposal to the cache
        m_pbftCache->fillMissedProposal(proposal);
    }
}