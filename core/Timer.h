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
 * @file Timer.h
 * @author: yujiechen
 * @date 2021-04-26
 */
#pragma once
#include "framework/TimerInterface.h"
#include <bcos-framework/libutilities/ThreadPool.h>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
namespace bcos
{
namespace consensus
{
class Timer : public TimerInterface, public std::enable_shared_from_this<Timer>
{
public:
    explicit Timer(uint64_t _timeout)
      : m_timeout(_timeout),
        m_ioService(std::make_shared<boost::asio::io_service>()),
        m_timer(std::make_shared<boost::asio::steady_timer>(*m_ioService))
    {
        m_thread.create_thread([this] {
            bcos::pthread_setThreadName("timer");
            m_ioService->run();
        });
    }

    ~Timer() override
    {
        stop();
        m_ioService->stop();
        if (!m_thread.is_this_thread_in())
        {
            m_thread.join_all();
        }
    }

    void start() override;
    void stop() override;
    void restart() override
    {
        stop();
        start();
    }

    void reset(uint64_t _timeout) override { m_timeout = _timeout; }

    bool running() override { return m_running; }
    int64_t timeout() override { return m_timeout; }

protected:
    virtual void startTimer();
    // adjust the timeout
    uint64_t adjustTimeout() override { return m_timeout; }
    std::atomic<uint64_t> m_timeout = {0};

private:
    std::atomic_bool m_running = {false};
    std::shared_ptr<boost::asio::io_service> m_ioService;
    std::shared_ptr<boost::asio::steady_timer> m_timer;
    boost::thread_group m_thread;
};
}  // namespace consensus
}  // namespace bcos