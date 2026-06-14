/*
 *  Copyright 2026 Diligent Graphics LLC
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
/// Defines WeakObjectCache class.

#include "DebugUtilities.hpp"
#include "HashUtils.hpp"
#include "RefCntAutoPtr.hpp"
#include "SpinLock.hpp"

#include <algorithm>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <utility>

namespace Diligent
{

/// Thread-safe cache for reference-counted objects stored as weak references.
///
/// Ensures that at most one object creation is in flight for a given key.
/// Object factories are invoked outside the shard lock, so factories may
/// recursively request other keys from the same cache.
template <typename InterfaceType>
class WeakObjectCache
{
private:
#ifdef __cpp_lib_hardware_interference_size
    static constexpr size_t CacheLineSize = std::max(std::hardware_destructive_interference_size, size_t{64});
#else
    static constexpr size_t CacheLineSize = 64;
#endif

    class ObjectEntry
    {
    public:
        enum class CreateAction
        {
            Create,
            Wait,
            Recursive
        };

        struct CreateState
        {
            CreateAction Action     = CreateAction::Wait;
            Uint64       Generation = 0;
        };

        RefCntAutoPtr<InterfaceType> Lock()
        {
            RefCntWeakPtr<InterfaceType> Object;
            {
                Threading::SpinLockGuard LockGuard{m_ObjectLock};
                Object = m_Object;
            }

            return Object.Lock();
        }

        void Set(InterfaceType* pObject)
        {
            Threading::SpinLockGuard LockGuard{m_ObjectLock};
            m_Object = RefCntWeakPtr<InterfaceType>{pObject};
        }

        CreateState BeginCreate()
        {
            std::lock_guard<std::mutex> LockGuard{m_CreateMtx};

            if (m_IsCreating)
            {
                return CreateState{
                    m_CreatorThread == std::this_thread::get_id() ? CreateAction::Recursive : CreateAction::Wait,
                    m_Generation,
                };
            }

            m_IsCreating    = true;
            m_CreatorThread = std::this_thread::get_id();
            return CreateState{CreateAction::Create, m_Generation};
        }

        bool WaitCreate(Uint64 Generation)
        {
            std::unique_lock<std::mutex> LockGuard{m_CreateMtx};
            m_CreateCV.wait(LockGuard, [&]() {
                return !m_IsCreating || m_Generation != Generation;
            });

            return m_LastCreateSucceeded;
        }

        void EndCreate(bool Succeeded)
        {
            {
                std::lock_guard<std::mutex> LockGuard{m_CreateMtx};
                VERIFY_EXPR(m_IsCreating);
                VERIFY_EXPR(m_CreatorThread == std::this_thread::get_id());

                m_IsCreating          = false;
                m_LastCreateSucceeded = Succeeded;
                m_CreatorThread       = {};
                ++m_Generation;
            }

            m_CreateCV.notify_all();
        }

    private:
        // While multiple weak pointers referencing the same object may be safely used concurrently,
        // the weak pointer itself is not thread safe and must be protected by a lock.
        Threading::SpinLock          m_ObjectLock;
        RefCntWeakPtr<InterfaceType> m_Object;

        std::mutex              m_CreateMtx;
        std::condition_variable m_CreateCV;
        bool                    m_IsCreating          = false;
        bool                    m_LastCreateSucceeded = false;
        Uint64                  m_Generation          = 0;
        std::thread::id         m_CreatorThread       = {};
    };

    using ObjectMapType = std::unordered_map<HashMapStringKey, std::shared_ptr<ObjectEntry>>;

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

    struct alignas(CacheLineSize) Shard
    {
        mutable std::shared_mutex Mutex;
        ObjectMapType             Objects;
    };

#ifdef _MSC_VER
#    pragma warning(pop)
#endif

public:
    explicit WeakObjectCache(size_t ShardCount = 0) :
        m_ShardCount{GetActualShardCount(ShardCount)},
        m_Shards{std::make_unique<Shard[]>(m_ShardCount)}
    {
    }

    // clang-format off
    WeakObjectCache           (const WeakObjectCache&) = delete;
    WeakObjectCache& operator=(const WeakObjectCache&) = delete;
    WeakObjectCache           (WeakObjectCache&&)      = delete;
    WeakObjectCache& operator=(WeakObjectCache&&)      = delete;
    // clang-format on

    template <typename CreateObjectFuncType>
    std::pair<RefCntAutoPtr<InterfaceType>, bool> GetOrCreate(const Char*            CacheKey,
                                                              CreateObjectFuncType&& CreateObjectFunc)
    {
        if (CacheKey == nullptr || CacheKey[0] == '\0')
        {
            LOG_ERROR_MESSAGE("WeakObjectCache key must not be null or empty");
            return {};
        }

        const HashMapStringKey       Key{CacheKey};
        Shard&                       CacheShard = GetShard(Key.GetHash());
        std::shared_ptr<ObjectEntry> pEntry;

        {
            std::shared_lock<std::shared_mutex> Lock{CacheShard.Mutex};

            const auto It = CacheShard.Objects.find(Key);
            if (It != CacheShard.Objects.end())
                pEntry = It->second;
        }

        if (!pEntry)
        {
            std::unique_lock<std::shared_mutex> Lock{CacheShard.Mutex};

            const auto It = CacheShard.Objects.find(Key);
            if (It != CacheShard.Objects.end())
            {
                pEntry = It->second;
            }
            else
            {
                pEntry                 = std::make_shared<ObjectEntry>();
                auto [NewIt, Inserted] = CacheShard.Objects.emplace(HashMapStringKey{CacheKey, true}, pEntry);
                VERIFY_EXPR(Inserted);
            }
        }

        for (;;)
        {
            if (RefCntAutoPtr<InterfaceType> pExisting = pEntry->Lock())
                return {pExisting, false};

            const typename ObjectEntry::CreateState State = pEntry->BeginCreate();
            if (State.Action == ObjectEntry::CreateAction::Wait)
            {
                if (pEntry->WaitCreate(State.Generation))
                    continue;

                return {};
            }

            if (State.Action == ObjectEntry::CreateAction::Recursive)
            {
                LOG_ERROR_MESSAGE("Recursive object creation detected for cache key '", CacheKey, "'");
                return {};
            }

            RefCntAutoPtr<InterfaceType> pObject;
            try
            {
                pObject = std::forward<CreateObjectFuncType>(CreateObjectFunc)();
            }
            catch (...)
            {
                pEntry->EndCreate(false);
                throw;
            }

            if (!pObject)
            {
                LOG_ERROR_MESSAGE("Failed to create object for cache key '", CacheKey, "'");
                pEntry->EndCreate(false);
                return {};
            }

            pEntry->Set(pObject.RawPtr());
            pEntry->EndCreate(true);

            return {pObject, true};
        }
    }

private:
    static size_t GetActualShardCount(size_t ShardCount)
    {
        if (ShardCount != 0)
            return ShardCount;

        const unsigned int ThreadCount = std::thread::hardware_concurrency();
        return ThreadCount != 0 ? static_cast<size_t>(ThreadCount) : size_t{1};
    }

    Shard& GetShard(size_t Hash)
    {
        VERIFY_EXPR(m_ShardCount > 0);
        return m_Shards[Hash % m_ShardCount];
    }

    const size_t             m_ShardCount;
    std::unique_ptr<Shard[]> m_Shards;
};

} // namespace Diligent
