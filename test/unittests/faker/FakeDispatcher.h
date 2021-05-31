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
 * @brief faker for the Dispatcher
 * @file FakeDispatcher.h
 * @author: yujiechen
 * @date 2021-05-28
 */
#pragma once
#include <bcos-framework/interfaces/dispatcher/DispatcherInterface.h>

using namespace bcos;
using namespace bcos::dispatcher;
using namespace bcos::protocol;

namespace bcos
{
namespace test
{
class FakeDispatcher : public DispatcherInterface
{
public:
    using Ptr = std::shared_ptr<FakeDispatcher>;
    FakeDispatcher() = default;
    ~FakeDispatcher() override {}

    void asyncExecuteBlock(Block::Ptr _block, bool,
        std::function<void(Error::Ptr, BlockHeader::Ptr)> _callback) override
    {
        if (m_blocks.empty())
        {
            m_blocks.push_back(_block);
            _callback(nullptr, _block->blockHeader());
            return;
        }
        auto latestBlock = m_blocks[m_blocks.size() - 1];
        if (_block->blockHeader()->number() != latestBlock->blockHeader()->number() + 1)
        {
            _callback(std::make_shared<Error>(-1, "invalid block for not consistent"), nullptr);
            return;
        }
        std::vector<ParentInfo> parentList;
        parentList.push_back(
            ParentInfo{latestBlock->blockHeader()->number(), latestBlock->blockHeader()->hash()});
        _block->blockHeader()->setParentInfo(parentList);
        _callback(nullptr, _block->blockHeader());
    }
    // useless for PBFT, maybe useful for executors
    void asyncGetLatestBlock(std::function<void(Error::Ptr, Block::Ptr)>) override {}

    // useless for PBFT, maybe useful for executors
    void asyncNotifyExecutionResult(
        Error::Ptr, std::shared_ptr<BlockHeader>, std::function<void(Error::Ptr)>) override
    {}

private:
    std::vector<Block::Ptr> m_blocks;
};
}  // namespace test
}  // namespace bcos