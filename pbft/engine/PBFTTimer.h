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
 * @brief implementation for PBFTTimer
 * @file PBFTTimer.h
 * @author: yujiechen
 * @date 2021-04-26
 */
#pragma once
#include <bcos-framework/libutilities/Timer.h>
namespace bcos
{
namespace consensus
{
class PBFTTimer : public Timer
{
public:
    using Ptr = std::shared_ptr<PBFTTimer>;
    explicit PBFTTimer(uint64_t _timeout) : Timer(_timeout) { updateAdjustedTimeout(); }

    ~PBFTTimer() override {}

    void updateChangeCycle(int64_t _changeCycle)
    {
        m_changeCycle = _changeCycle;
        updateAdjustedTimeout();
    }
    void incChangeCycle(int64_t _increasedValue)
    {
        m_changeCycle += _increasedValue;
        updateAdjustedTimeout();
    }
    void resetChangeCycle()
    {
        m_changeCycle = 0;
        updateAdjustedTimeout();
    }
    uint64_t changeCycle() const { return m_changeCycle; }

    void reset(uint64_t _timeout) override
    {
        Timer::reset(_timeout);
        updateAdjustedTimeout();
    }

protected:
    void updateAdjustedTimeout()
    {
        uint64_t timeout = m_timeout.load() * std::pow(m_base, m_changeCycle.load());
        m_adjustedTimeout.store(timeout);
    }
    uint64_t adjustTimeout() override { return m_adjustedTimeout; }

private:
    std::atomic<uint64_t> m_adjustedTimeout;
    std::atomic<uint64_t> m_changeCycle = 0;
    double const m_base = 1.5;
};
}  // namespace consensus
}  // namespace bcos