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
 * @brief test for Timer
 * @file TimerTest.cpp
 * @author: yujiechen
 * @date 2021-04-26
 */
#include "core/Timer.h"
#include <bcos-test/libutils/TestPromptFixture.h>
#include <boost/test/unit_test.hpp>
using namespace bcos;
using namespace bcos::consensus;
namespace bcos
{
namespace test
{
class FakeTimer : public Timer
{
public:
    explicit FakeTimer(uint64_t _timeout) : Timer(_timeout) {}
    ~FakeTimer() override {}
    void setTriggerTimeout(bool _triggerTimeout) { m_triggerTimeout = _triggerTimeout; }
    bool triggerTimeout() { return m_triggerTimeout; }

protected:
    // invoked everytime when it reaches the timeout
    void run() override
    {
        std::cout << "### run timeout handler now" << std::endl;
        m_triggerTimeout = true;
    }

private:
    std::atomic_bool m_triggerTimeout = false;
};

BOOST_FIXTURE_TEST_SUITE(TimerTest, TestPromptFixture)
BOOST_AUTO_TEST_CASE(testTimer)
{
    auto timeoutInterval = 300;
    auto timer = std::make_shared<FakeTimer>(timeoutInterval);
    auto startT = utcTime();
    for (size_t i = 0; i < 4; i++)
    {
        timer->setTriggerTimeout(false);
        // start the timer
        timer->start();
        // sleep
        startT = utcTime();
        usleep((timeoutInterval + 200) * 1000);
        std::cout << "#### sleep eclipse:" << utcTime() - startT;
        // stop the timer
        timer->stop();
        // check the value
        std::cout << "##### testTimer: index" << i << std::endl;
        std::cout << std::endl;
        BOOST_CHECK(timer->triggerTimeout() == true);
    }

    std::cout << std::endl;
    std::cout << "#### case1" << std::endl;
    timer->setTriggerTimeout(false);
    timer->start();
    startT = utcTime();
    usleep(1000 * (timeoutInterval - 200));
    std::cout << "#### sleep eclipse:" << utcTime() - startT;
    BOOST_CHECK(timer->triggerTimeout() == false);
    timer->stop();

    std::cout << std::endl;
    std::cout << "#### case2" << std::endl;
    // reset the timer
    timer->setTriggerTimeout(false);
    timer->reset(60);
    timer->start();
    startT = utcTime();
    usleep(200 * 1000);
    std::cout << "#### sleep eclipse:" << utcTime() - startT;
    BOOST_CHECK(timer->triggerTimeout() == true);
    timer->stop();
}

BOOST_AUTO_TEST_CASE(testTimerWithoutWait)
{
    auto timeoutInterval = 100;
    auto timer = std::make_shared<FakeTimer>(timeoutInterval);
    timer->start();
    BOOST_CHECK(timer->triggerTimeout() == false);
}
BOOST_AUTO_TEST_SUITE_END()
}  // namespace test
}  // namespace bcos