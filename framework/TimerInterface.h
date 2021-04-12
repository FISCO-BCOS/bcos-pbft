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
 * @brief interface for Timer
 * @file TimerInterface.h
 * @author: yujiechen
 * @date 2021-04-09
 */
#pragma once

namespace bcos
{
namespace consensus
{
class TimerInterface
{
public:
    using Ptr = std::shared_ptr<TimerInterface>;
    TickerInterface() = default;
    virtual ~TickerInterface() {}

    virtual void init(uint64_t _timeout) = 0;
    // start the timer
    virtual void start() = 0;
    // stop the timer
    virtual void stop() = 0;
    // destroy the timer
    virtual void destroy() = 0;
    // reset the timer with the setted timeout
    virtual void reset() = 0;
    // reset the timer with the given timeout
    virtual void reset(uint64_t _timeout) = 0;

protected:
    // invoked everytime when it reaches the timeout
    virtual void run() = 0;

    // invoked when the timer is finally destroyed
    virtual void onDestroy() = 0;

    // adjust the timeout
    virtual int adjustTimeout() = 0;
};
};  // namespace consensus
}  // namespace bcos