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
 * @file PBFTEngine.h
 * @author: yujiechen
 * @date 2021-04-20
 */
#pragma once
#include "core/ConsensusEngine.h"
#include <bcos-framework/libutilities/ConcurrentQueue.h>
#include <bcos-framework/libutilities/Error.h>


namespace bcos
{
namespace front
{
class FrontServiceInterface;
}
namespace consensus
{
class PBFTBaseMessageInterface;
class PBFTMessageInterface;
class ViewChangeMsgInterface;
class NewViewMsgInterface;
class PBFTMessageFactory;
class PBFTConfig;
class PBFTCodecInterface;
class Validator;
class PBFTReqCache;

using PBFTMsgQueue = ConcurrentQueue<std::shared_ptr<PBFTBaseMessageInterface>>;
using PBFTMsgQueuePtr = std::shared_ptr<PBFTMsgQueue>;
class PBFTEngine : public ConsensusEngine
{
public:
    PBFTEngine(std::shared_ptr<PBFTConfig> _config, std::shared_ptr<PBFTCodecInterface> _codec,
        std::shared_ptr<PBFTMessageFactory> _pbftMessageFactory,
        std::shared_ptr<Validator> _validator,
        std::shared_ptr<bcos::front::FrontServiceInterface> _frontService);

    ~PBFTEngine() override { stop(); }

    void start() override;
    void stop() override;

protected:
    virtual void onReceivePBFTMessage(bcos::Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID,
        bytesConstRef _data, std::function<void(bytesConstRef _respData)> _respFunc);

    void executeWorker() override;

    virtual void handleMsg(std::shared_ptr<PBFTBaseMessageInterface> _msg);
    virtual bool handlePrePrepareMsg(std::shared_ptr<PBFTMessageInterface> _prePrepareMsg);
    virtual bool handlePrepareMsg(std::shared_ptr<PBFTMessageInterface> _prepareMsg);
    virtual bool handleCommitMsg(std::shared_ptr<PBFTMessageInterface> _commitMsg);
    virtual bool handleViewChangeMsg(std::shared_ptr<ViewChangeMsgInterface> _viewChangeMsg);
    virtual bool handleNewViewMsg(std::shared_ptr<NewViewMsgInterface> _newViewMsg);

private:
    void waitSignal()
    {
        boost::unique_lock<boost::mutex> l(x_signalled);
        m_signalled.wait_for(l, boost::chrono::milliseconds(5));
    }

private:
    std::shared_ptr<PBFTConfig> m_config;
    std::shared_ptr<PBFTCodecInterface> m_codec;
    std::shared_ptr<PBFTMessageFactory> m_pbftMessageFactory;
    std::shared_ptr<Validator> m_proposalValidator;
    std::shared_ptr<bcos::front::FrontServiceInterface> m_frontService;

    PBFTMsgQueuePtr m_msgQueue;
    std::shared_ptr<PBFTReqCache> m_pbftReqCache;

    const unsigned c_PopWaitSeconds = 5;

    boost::condition_variable m_signalled;
    boost::mutex x_signalled;
    mutable Mutex m_mutex;
};
}  // namespace consensus
}  // namespace bcos