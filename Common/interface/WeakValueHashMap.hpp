/*
 *  Copyright 2025-2026 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

/// \file
/// Defines WeakValueHashMap class

#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

#include "../../Platforms/Basic/interface/DebugUtilities.hpp"

namespace Diligent
{

/// WeakValueHashMap is a thread-safe hash map that holds weak pointers to its values.

/// \tparam KeyType   - Type of the keys in the map. Must be hashable and comparable.
/// \tparam ValueType - Type of the values in the map.
/// \tparam Hasher    - Hash function for the keys. Default is std::hash<KeyType>.
/// \tparam Keyeq     - Equality comparison function for the keys. Default is std::equal_to<KeyType>.
///
/// When a value is requested via GetOrInsert(), a strong pointer (shared_ptr) to the value is returned
/// wrapped in a ValueHandle object. The ValueHandle object is responsible for removing the entry from the map
/// when it is destroyed. If there are no more strong pointers to the value, the entry is removed from the map.
///
/// If a value is requested via Get(), a strong pointer to the value is returned wrapped in a ValueHandle object
/// if the entry exists and the value has not expired. Otherwise, an empty ValueHandle is returned.
///
/// The map is thread-safe and can be accessed from multiple threads simultaneously.
///
/// Example usage:
///
///     WeakValueHashMap<int, std::string> Map;
///     auto Handle = Map.GetOrInsert(1, "Value");
///     std::cout << *Handle << std::endl; // Outputs "Value"
///
template <typename KeyType,
          typename ValueType,
          typename Hasher = std::hash<KeyType>,
          typename Keyeq  = std::equal_to<KeyType>>
class WeakValueHashMap
{
private:
    class Impl;

public:
    explicit WeakValueHashMap(size_t NumShards = 1) :
        m_pImpl(NumShards != 0 ? NumShards : 1)
    {
        for (std::shared_ptr<Impl>& pImpl : m_pImpl)
        {
            pImpl = std::make_shared<Impl>();
        }
    }

    // clang-format off
    WeakValueHashMap           (const WeakValueHashMap&) = delete;
    WeakValueHashMap& operator=(const WeakValueHashMap&) = delete;
    WeakValueHashMap           (WeakValueHashMap&&) noexcept = default;
    WeakValueHashMap& operator=(WeakValueHashMap&&) noexcept = default;
    // clang-format on

    /// ValueHandle is a handle to a value in the WeakValueHashMap.

    /// It holds a strong pointer to the value (and keeps the map implementation alive while the handle exists).
    /// The map entry is removed by the value's custom deleter when the last strong reference to the value is destroyed.
    class ValueHandle
    {
    public:
        ValueHandle() = default;

        // Disable copy semantics
        ValueHandle(const ValueHandle&) = delete;
        ValueHandle& operator=(const ValueHandle&) = delete;

        ValueHandle(ValueHandle&& rhs) noexcept = default;

        ValueHandle& operator=(ValueHandle&& rhs) noexcept
        {
            if (this != &rhs)
            {
                // It is important to reset the value first to ensure that the map is alive if m_pValue keeps the last reference.
                m_pValue.reset();
                m_pMap.reset();

                m_pMap   = std::move(rhs.m_pMap);
                m_pValue = std::move(rhs.m_pValue);
            }
            return *this;
        }

        ValueType*       Get() { return m_pValue.get(); }
        const ValueType* Get() const { return m_pValue.get(); }

        ValueType&       operator*() { return *m_pValue; }
        const ValueType& operator*() const { return *m_pValue; }
        ValueType*       operator->() { return m_pValue.get(); }
        const ValueType* operator->() const { return m_pValue.get(); }

        explicit operator bool() const { return m_pMap && m_pValue; }

    private:
        friend class WeakValueHashMap<KeyType, ValueType, Hasher, Keyeq>;

        ValueHandle(Impl&                      Map,
                    std::shared_ptr<ValueType> pValue) :
            m_pMap{Map.shared_from_this()},
            m_pValue{std::move(pValue)}
        {}

    private:
        // Declaration order matters for destruction:
        // m_pValue is destroyed first, then m_pMap.
        std::shared_ptr<Impl>      m_pMap;
        std::shared_ptr<ValueType> m_pValue;
    };

    ValueHandle Get(const KeyType& Key) const
    {
        const size_t ShardIdx = GetShardIndex(Key);
        return m_pImpl[ShardIdx]->Get(Key);
    }

    template <typename... ArgsType>
    ValueHandle GetOrInsert(const KeyType& Key, ArgsType&&... Args) const
    {
        const size_t ShardIdx = GetShardIndex(Key);
        return m_pImpl[ShardIdx]->GetOrInsert(Key, std::forward<ArgsType>(Args)...);
    }

private:
    size_t GetShardIndex(const KeyType& Key) const
    {
        return m_pImpl.size() > 1 ? m_Hasher(Key) % m_pImpl.size() : 0;
    }

    class Impl : public std::enable_shared_from_this<Impl>
    {
    public:
        ValueHandle Get(const KeyType& Key)
        {
            std::lock_guard<std::mutex> Lock{m_Mtx};

            auto it = m_Map.find(Key);
            if (it != m_Map.end())
            {
                if (auto pValue = it->second.lock())
                {
                    return ValueHandle{*this, std::move(pValue)};
                }
                else
                {
                    m_Map.erase(it);
                }
            }

            return ValueHandle{};
        }

        template <typename... ArgsType>
        ValueHandle GetOrInsert(const KeyType& Key, ArgsType&&... Args)
        {
            if (ValueHandle Handle = Get(Key))
            {
                return Handle;
            }

            // Create the new value outside of the lock

            std::weak_ptr<Impl> WeakSelf{this->shared_from_this()};

            // Create shared_ptr with deleter that erases the Key from the map when
            // the last reference to the value is destroyed
            std::shared_ptr<ValueType> pNewValue{
                // Use parentheses to avoid initializer_list surprises.
                new ValueType(std::forward<ArgsType>(Args)...),
                [WeakSelf = std::move(WeakSelf), Key](ValueType* pValue) {
                    // Get a pointer before deleting the value as the value keeps the map alive.
                    std::shared_ptr<Impl> Self = WeakSelf.lock();
                    delete pValue;
                    if (Self)
                    {
                        Self->Remove(Key);
                    }
                }};

            std::lock_guard<std::mutex> Lock{m_Mtx};

            // Check again in case another thread inserted the value while we were creating it
            auto it = m_Map.find(Key);
            if (it != m_Map.end())
            {
                if (auto pValue = it->second.lock())
                {
                    // Discard the newly created value and use the one created by the other thread
                    return ValueHandle{*this, std::move(pValue)};
                }
                else
                {
                    // Replace the expired weak pointer with the newly created value
                    it->second = pNewValue;
                    return ValueHandle{*this, std::move(pNewValue)};
                }
            }

            // Insert the new value
            bool Inserted = m_Map.emplace(Key, pNewValue).second;
            VERIFY(Inserted, "Failed to insert new value into the map. This should never happen as we have already checked that the key does not exist.");
            return ValueHandle{*this, std::move(pNewValue)};
        }

        void Remove(const KeyType& Key)
        {
            std::lock_guard<std::mutex> Lock{m_Mtx};

            auto Iter = m_Map.find(Key);
            if (Iter == m_Map.end())
            {
                return;
            }

            // Only erase if the weak entry refers to no live value anymore.
            // (If the key was reused for a newer value, its weak_ptr won't be expired.)
            if (Iter->second.expired())
            {
                m_Map.erase(Iter);
            }
        }

        ~Impl()
        {
            VERIFY(m_Map.empty(), "Map is not empty upon destruction. This should never happen because all entries should have been "
                                  "removed by custom deleters, and the map can't be destroyed while any ValueHandle "
                                  "instances are alive.");
        }

    private:
        std::mutex                                                           m_Mtx;
        std::unordered_map<KeyType, std::weak_ptr<ValueType>, Hasher, Keyeq> m_Map;
    };
    std::vector<std::shared_ptr<Impl>> m_pImpl;
    Hasher                             m_Hasher;
};

} // namespace Diligent
