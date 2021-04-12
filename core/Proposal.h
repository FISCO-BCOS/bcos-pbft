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
 * @brief implementation of Proposal
 * @file Proposal.h
 * @author: yujiechen
 * @date 2021-04-09
 */
#pragma once
#include "Common.h"
#include "Exceptions.h"
#include <bcos-framework/interfaces/protocol/BlockHeader.h>
#include <bcos-pbft/core/proto/Consensus.pb.h>
#include <bcos-pbft/framework/ProposalInterface.h>

namespace bcos
{
namespace consensus
{
const bcos::protocol::BlockNumber InvalidBlockNumber = -1;
class Proposal : public ProposalInterface
{
public:
    using Ptr = std::shared_ptr<Proposal>;
    Proposal(bcos::protocol::BlockNumber _index, bcos::crypto::HashType _hash) : Proposal()
    {
        m_hash = _hash;
        m_pbProposal->set_index(_index);
        m_pbProposal->set_hash(_hash.data(), bcos::crypto::HashType::size);
    }

    Proposal(
        bcos::protocol::BlockNumber _index, bcos::crypto::HashType _hash, bcos::bytes const& _data)
      : Proposal(_index, _hash)
    {
        m_pbProposal->set_data(_data.data(), _data.size());
    }


    explicit Proposal(std::shared_ptr<PBProposal> _pbProposal) : m_pbProposal(_pbProposal)
    {
        m_hash = bcos::crypto::HashType(
            (byte*)_pbProposal->hash().c_str(), bcos::crypto::HashType::size);
    }

    ~Proposal() override {}

    // the index of the proposal
    bcos::protocol::BlockNumber index() const override { return m_pbProposal->index(); }
    // the hash of the proposal
    bcos::crypto::HashType const& hash() const override { return m_hash; }

    // the payload of the proposal
    bcos::bytesConstRef data() const override
    {
        auto const& data = m_pbProposal->data();
        return bcos::bytesConstRef((byte const*)data.c_str(), data.size());
    }


protected:
    Proposal() : m_pbProposal(std::make_shared<PBProposal>()) {}
    std::shared_ptr<PBProposal> pbProposal() { return m_pbProposal; }


    std::shared_ptr<PBProposal> m_pbProposal;
    bcos::crypto::HashType m_hash;
};
}  // namespace consensus
}  // namespace bcos