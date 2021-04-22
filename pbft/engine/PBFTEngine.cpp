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
 * @brief implementation for PBFTEngine
 * @file PBFTEngine.cpp
 * @author: yujiechen
 * @date 2021-04-20
 */
#include "PBFTEngine.h"
#include "boost/bind.hpp"
#include "core/Validator.h"
#include "interfaces/protocol/CommonError.h"
#include "pbft/cache/PBFTReqCache.h"
#include "pbft/protocol/interfaces/PBFTCodecInterface.h"
#include <bcos-framework/interfaces/front/FrontServiceInterface.h>
#include <bcos-framework/interfaces/protocol/Protocol.h>

using namespace bcos;
using namespace bcos::consensus;
using namespace bcos::front;
using namespace bcos::protocol;

PBFTEngine::PBFTEngine(PBFTConfig::Ptr _config, PBFTCodecInterface::Ptr _codec,
    PBFTMessageFactory::Ptr _pbftMessageFactory, Validator::Ptr _validator,
    FrontServiceInterface::Ptr _frontService)
  : ConsensusEngine("pbft", 0),
    m_config(_config),
    m_codec(_codec),
    m_pbftMessageFactory(_pbftMessageFactory),
    m_proposalValidator(_validator),
    m_frontService(_frontService),
    m_msgQueue(std::make_shared<PBFTMsgQueue>())
{
    m_pbftReqCache = std::make_shared<PBFTReqCache>(_config);
}

void PBFTEngine::start()
{
    // register the message dispatcher callback to front service
    m_frontService->registerMessageDispatcher(
        ModuleID::PBFT, boost::bind(&PBFTEngine::onReceivePBFTMessage, this, _1, _2, _3, _4));
    // TODO: init the storage state
    ConsensusEngine::start();
}

void PBFTEngine::stop()
{
    ConsensusEngine::stop();
}

void PBFTEngine::onReceivePBFTMessage(bcos::Error::Ptr _error, bcos::crypto::NodeIDPtr _fromNode,
    bytesConstRef _data, std::function<void(bytesConstRef _respData)>)
{
    try
    {
        if (_error->errorCode() != CommonError::SUCCESS)
        {
            return;
        }
        // the node is not the consensusNode
        if (!m_config->isConsensusNode())
        {
            PBFT_LOG(TRACE) << LOG_DESC(
                "onReceivePBFTMessage: reject the message for the node is not the consensus "
                "node");
            return;
        }
        // decode the message and push the message into the queue
        m_msgQueue->push(m_codec->decode(_data));
    }
    catch (std::exception const& _e)
    {
        PBFT_LOG(WARNING) << LOG_DESC("onReceivePBFTMessage exception")
                          << LOG_KV("fromNode", _fromNode->hex())
                          << LOG_KV("idx", m_config->nodeIndex())
                          << LOG_KV("nodeId", m_config->nodeID()->hex())
                          << LOG_KV("error", boost::diagnostic_information(_e));
    }
}

void PBFTEngine::executeWorker()
{
    // the node is not the consensusNode
    if (!m_config->isConsensusNode())
    {
        waitSignal();
        return;
    }
    // handle the PBFT message(here will wait when the msgQueue is empty)
    auto messageResult = m_msgQueue->tryPop(c_PopWaitSeconds);
    if (messageResult.first)
    {
        handleMsg(messageResult.second);
    }
    m_pbftReqCache->clearExpiredCache();
}

void PBFTEngine::handleMsg(std::shared_ptr<PBFTBaseMessageInterface> _msg)
{
    // TODO: Consider carefully whether this lock can be removed
    Guard l(m_mutex);
    switch (_msg->packetType())
    {
    case PacketType::PrePreparePacket:
    {
        auto prePrepareMsg = std::dynamic_pointer_cast<PBFTMessageInterface>(_msg);
        handlePrePrepareMsg(prePrepareMsg);
        break;
    }
    case PacketType::PreparePacket:
    {
        auto prepareMsg = std::dynamic_pointer_cast<PBFTMessageInterface>(_msg);
        handlePrepareMsg(prepareMsg);
        break;
    }
    case PacketType::CommitPacket:
    {
        auto commitMsg = std::dynamic_pointer_cast<PBFTMessageInterface>(_msg);
        handleCommitMsg(commitMsg);
        break;
    }
    case PacketType::ViewChangePacket:
    {
        auto viewChangeMsg = std::dynamic_pointer_cast<ViewChangeMsgInterface>(_msg);
        handleViewChangeMsg(viewChangeMsg);
        break;
    }
    case PacketType::NewViewPacket:
    {
        auto newViewMsg = std::dynamic_pointer_cast<NewViewMsgInterface>(_msg);
        handleNewViewMsg(newViewMsg);
        break;
    }
    default:
    {
        PBFT_LOG(DEBUG) << LOG_DESC("handleMsg: unknown PBFT message")
                        << LOG_KV("type", std::to_string(_msg->packetType()))
                        << LOG_KV("genIdx", _msg->generatedFrom())
                        << LOG_KV("nodesef", m_config->nodeID()->hex());
        return;
    }
    }
}

bool PBFTEngine::handlePrePrepareMsg(PBFTMessageInterface::Ptr)
{
    return true;
}
bool PBFTEngine::handlePrepareMsg(PBFTMessageInterface::Ptr)
{
    return true;
}
bool PBFTEngine::handleCommitMsg(PBFTMessageInterface::Ptr)
{
    return true;
}
bool PBFTEngine::handleViewChangeMsg(ViewChangeMsgInterface::Ptr)
{
    return true;
}
bool PBFTEngine::handleNewViewMsg(NewViewMsgInterface::Ptr)
{
    return true;
}