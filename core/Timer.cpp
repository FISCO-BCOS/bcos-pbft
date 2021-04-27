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
 * @brief implementation for Timer
 * @file Timer.cpp
 * @author: yujiechen
 * @date 2021-04-26
 */
#include "core/Timer.h"
#include "core/Common.h"
#include <bcos-framework/libutilities/Common.h>
#include <iostream>

using namespace bcos::consensus;

void Timer::start()
{
    auto timer = std::weak_ptr<Timer>(shared_from_this());
    m_ioService->post([timer]() {
        try
        {
            auto t = timer.lock();
            if (!t)
            {
                return;
            }
            t->startTimer();
        }
        catch (std::exception const& e)
        {
            CONSENSUS_LOG(WARNING) << LOG_DESC("startTimer failed")
                                   << LOG_KV("errorInfo", boost::diagnostic_information(e));
        }
    });
}

void Timer::startTimer()
{
    m_running = true;
    m_timer->expires_from_now(std::chrono::milliseconds(adjustTimeout()));
    auto timer = std::weak_ptr<Timer>(shared_from_this());
    // calls the timeout handler
    m_timer->async_wait([timer](const boost::system::error_code& error) {
        // the timer has been cancelled
        if (error == boost::asio::error::operation_aborted)
        {
            return;
        }
        if (error)
        {
            CONSENSUS_LOG(WARNING) << LOG_DESC("Timer async_wait error") << LOG_KV("error", error);
            return;
        }
        try
        {
            auto t = timer.lock();
            if (!t)
            {
                return;
            }
            t->run();
            // The timer has been stopped
            if (!t->m_running)
            {
                return;
            }
            // The timer has not been stopped
            else
            {
                t->startTimer();
            }
        }
        catch (std::exception const& e)
        {
            CONSENSUS_LOG(WARNING) << LOG_DESC("calls timeout handler failed")
                                   << LOG_KV("errorInfo", boost::diagnostic_information(e));
        }
    });
    m_ioService->run();
}

// stop the timer
void Timer::stop()
{
    if (!m_running)
    {
        return;
    }
    m_running = false;
    // cancel the timer
    m_timer->cancel();
    if (m_ioService->stopped())
    {
        m_ioService->reset();
    }
}