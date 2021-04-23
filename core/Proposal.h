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
#include "core/proto/Consensus.pb.h"
#include "framework/ProposalInterface.h"
#include <bcos-framework/interfaces/protocol/BlockHeader.h>

namespace bcos
{
namespace consensus
{
const bcos::protocol::BlockNumber InvalidBlockNumber = -1;
class Proposal : virtual public ProposalInterface
{
public:
    using Ptr = std::shared_ptr<Proposal>;
    Proposal() : m_rawProposal(std::make_shared<RawProposal>()) {}

    explicit Proposal(std::shared_ptr<RawProposal> _rawProposal) : m_rawProposal(_rawProposal)
    {
        auto const& hashData = _rawProposal->hash();
        if (hashData.size() < bcos::crypto::HashType::size)
        {
            return;
        }
        m_hash =
            bcos::crypto::HashType((byte const*)hashData.c_str(), bcos::crypto::HashType::size);
    }
    ~Proposal() override {}

    // the index of the proposal
    bcos::protocol::BlockNumber index() const override { return m_rawProposal->index(); }
    void setIndex(bcos::protocol::BlockNumber _index) override { m_rawProposal->set_index(_index); }

    // the hash of the proposal
    bcos::crypto::HashType const& hash() const override { return m_hash; }
    void setHash(bcos::crypto::HashType const& _hash) override
    {
        m_hash = _hash;
        m_rawProposal->set_hash(_hash.data(), bcos::crypto::HashType::size);
    }
    // the payload of the proposal
    bcos::bytesConstRef data() const override
    {
        auto const& data = m_rawProposal->data();
        return bcos::bytesConstRef((byte const*)data.c_str(), data.size());
    }
    void setData(bytes const& _data) override
    {
        m_rawProposal->set_data(_data.data(), _data.size());
    }

    void setData(bytes&& _data) override
    {
        auto size = _data.size();
        m_rawProposal->set_data(std::move(_data).data(), size);
    }

    bytesConstRef signature() const override
    {
        auto const& signature = m_rawProposal->signature();
        return bcos::bytesConstRef((byte const*)signature.c_str(), signature.size());
    }

    void setSignature(bytes const& _data) override
    {
        m_rawProposal->set_signature(_data.data(), _data.size());
    }

    bool operator==(Proposal const _proposal)
    {
        return _proposal.index() == index() && _proposal.hash() == hash() &&
               _proposal.data().toBytes() == data().toBytes();
    }
    bool operator!=(Proposal const _proposal) { return !(operator==(_proposal)); }

    std::shared_ptr<RawProposal> rawProposal() { return m_rawProposal; }

protected:
    std::shared_ptr<RawProposal> m_rawProposal;
    bcos::crypto::HashType m_hash;
};
}  // namespace consensus
}  // namespace bcos