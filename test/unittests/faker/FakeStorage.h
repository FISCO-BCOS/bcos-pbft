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
 * @brief faker for the Storage
 * @file FakeStorage.h
 * @author: yujiechen
 * @date 2021-05-28
 */
#pragma once
#include <bcos-framework/interfaces/storage/Common.h>
#include <bcos-framework/interfaces/storage/StorageInterface.h>

using namespace bcos::storage;

namespace bcos
{
namespace test
{
class FakeStorage : public StorageInterface
{
public:
    using Ptr = std::shared_ptr<FakeStorage>;
    FakeStorage() = default;
    ~FakeStorage() override {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    std::vector<std::string> getPrimaryKeys(
        const TableInfo::Ptr&, const Condition::Ptr&) const override
    {
        return std::vector<std::string>();
    }

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    Entry::Ptr getRow(const TableInfo::Ptr&, const std::string_view&) override { return nullptr; }

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    std::map<std::string, Entry::Ptr> getRows(
        const TableInfo::Ptr&, const std::vector<std::string>&) override
    {
        return std::map<std::string, Entry::Ptr>();
    }

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    std::pair<size_t, Error::Ptr> commitBlock(protocol::BlockNumber _number,
        const std::vector<TableInfo::Ptr>&,
        const std::vector<std::shared_ptr<std::map<std::string, Entry::Ptr>>>&) override
    {
        return std::make_pair(_number, nullptr);
    }

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    void asyncGetPrimaryKeys(const TableInfo::Ptr&, const Condition::Ptr&,
        std::function<void(const Error::Ptr&, const std::vector<std::string>&)>) override
    {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    void asyncGetRow(const TableInfo::Ptr&, const std::string_view&,
        std::function<void(const Error::Ptr&, const Entry::Ptr&)>) override
    {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    void asyncGetRows(const TableInfo::Ptr&, const std::shared_ptr<std::vector<std::string>>&,
        std::function<void(const Error::Ptr&, const std::map<std::string, Entry::Ptr>&)>) override
    {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    void asyncCommitBlock(protocol::BlockNumber,
        const std::shared_ptr<std::vector<TableInfo::Ptr>>&,
        const std::shared_ptr<std::vector<std::shared_ptr<std::map<std::string, Entry::Ptr>>>>&,
        std::function<void(const Error::Ptr&, size_t)>) override
    {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    void asyncAddStateCache(protocol::BlockNumber, const std::shared_ptr<TableFactoryInterface>&,
        std::function<void(const Error::Ptr&)>) override
    {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    void asyncDropStateCache(protocol::BlockNumber, std::function<void(const Error::Ptr&)>) override
    {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    void asyncGetStateCache(protocol::BlockNumber,
        std::function<void(const Error::Ptr&, const std::shared_ptr<TableFactoryInterface>&)>)
        override
    {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    std::shared_ptr<TableFactoryInterface> getStateCache(protocol::BlockNumber) override
    {
        return nullptr;
    }
    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    void dropStateCache(protocol::BlockNumber) override {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    void addStateCache(
        protocol::BlockNumber, const std::shared_ptr<TableFactoryInterface>&) override
    {}

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    Error::Ptr put(
        const std::string_view&, const std::string_view&, const std::string_view&) override
    {
        return nullptr;
    }

    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    std::pair<std::string, Error::Ptr> get(
        const std::string_view&, const std::string_view&) override
    {
        return std::make_pair("", nullptr);
    }
    // useless for PBFT, maybe can merge with MockStorage of bcos-ledger
    Error::Ptr remove(const std::string_view&, const std::string_view&) override { return nullptr; }

    void asyncPut(const std::string_view& _columnFamily, const std::string_view& _key,
        const std::string_view& _value, std::function<void(const Error::Ptr&)> _callback) override
    {
        auto key = getKey(_columnFamily, _key);
        m_key2Data[key] = _value;
        _callback(nullptr);
    }

    void asyncRemove(const std::string_view& _columnFamily, const std::string_view& _key,
        std::function<void(const Error::Ptr&)> _callback) override
    {
        auto key = getKey(_columnFamily, _key);
        if (!m_key2Data.count(key))
        {
            _callback(std::make_shared<Error>(StorageErrorCode::NotFound, "key NotFound"));
            return;
        }
        m_key2Data.erase(key);
        _callback(nullptr);
    }

    void asyncGet(const std::string_view& _columnFamily, const std::string_view& _key,
        std::function<void(const Error::Ptr&, const std::string& value)> _callback) override
    {
        auto key = getKey(_columnFamily, _key);
        if (!m_key2Data.count(key))
        {
            _callback(std::make_shared<Error>(StorageErrorCode::NotFound, "key NotFound"), "");
            return;
        }
        _callback(nullptr, m_key2Data[key]);
    }

    void asyncGetBatch(const std::string_view& _columnFamily,
        const std::shared_ptr<std::vector<std::string>>& _keys,
        std::function<void(const Error::Ptr&, const std::shared_ptr<std::vector<std::string>>&)>
            callback) override
    {
        auto result = std::make_shared<std::vector<std::string>>();
        for (auto const& _key : *_keys)
        {
            auto key = getKey(_columnFamily, _key);
            if (!m_key2Data.count(key))
            {
                result->push_back("");
                continue;
            }
            result->push_back(m_key2Data[key]);
        }
        callback(nullptr, result);
    }


protected:
    std::string getKey(const std::string_view& _columnFamily, const std::string_view& _key)
    {
        std::string columnFamily(_columnFamily.data(), _columnFamily.size());
        std::string key(_key.data(), _key.size());
        return columnFamily + "_" + key;
    }

    std::map<std::string, std::string> m_key2Data;
};
}  // namespace test
}  // namespace bcos
