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
 * @brief cache for the PBFTReq
 * @file PBFTReqCache.h
 * @author: yujiechen
 * @date 2021-04-21
 */
#pragma once
#include "pbft/config/PBFTConfig.h"
#include "pbft/protocol/interfaces/PBFTMessageFactory.h"
#include "pbft/protocol/interfaces/PBFTMessageInterface.h"
namespace bcos
{
namespace consensus
{
class PBFTReqCache
{
public:
    explicit PBFTReqCache(PBFTConfig::Ptr _config) : m_config(_config) {}
    virtual ~PBFTReqCache() {}

    virtual void addPrePrepareCache(PBFTMessageInterface::Ptr _prePrepareMsg);
    virtual bool prePrepareExists(PBFTMessageInterface::Ptr _prePrepareMsg);
    // clear the expired cache periodically
    virtual void clearExpiredCache() {}

protected:
    PBFTConfig::Ptr m_config;
    PBFTMessageInterface::Ptr m_prePrepareCache = nullptr;
    mutable SharedMutex x_prePrepareCache;
};
}  // namespace consensus
}  // namespace bcos