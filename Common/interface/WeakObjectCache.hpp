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
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Diligent
{

/// Thread-safe cache for reference-counted objects stored as weak references.
///
/// Ensures that at most one object creation is in flight for a given key.
/// Object factories are invoked outside the shard lock, so factories may
/// recursively request other keys from the same cache.
///
/// The cache owns only weak references. A cached object may expire when all
/// external strong references are released; the key remains in the map until
/// the object is replaced by a later GetOrCreate() call or explicitly removed
/// by EraseIfExpired().
///
/// Example:
///
/// \code
/// WeakObjectCache<IMyObject> Cache;
///
/// auto [pObject, Created] =
///     Cache.GetOrCreate(
///         "object-key",
///         [&]() {
///             return RefCntAutoPtr<IMyObject>{MakeNewRCObj<MyObject>()(...)};
///         });
///
/// if (pObject == nullptr)
/// {
///     // The key was invalid, the factory returned null, or another
///     // in-flight factory for the same key failed.
/// }
/// else if (Created)
/// {
///     // This call created and published a new object.
/// }
/// \endcode
///
/// Same-thread recursion for the same key is detected and fails:
///
/// \code
/// Cache.GetOrCreate("X", [&] {
///     return Cache.GetOrCreate("X", ...).first; // returns null
/// });
/// \endcode
///
/// Cross-thread dependency cycles are not detected and may deadlock:
///
/// \code
/// // Thread A creates key X and, from its factory, requests key Y.
/// // Thread B creates key Y and, from its factory, requests key X.
/// // Both threads can wait for each other's in-flight creation.
/// \endcode
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
        // Per-key creation state. The shard lock only protects the map entry;
        // once a thread has copied the shared_ptr<ObjectEntry>, creation and
        // waiting are coordinated here without holding the shard lock.
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

        struct CreateResult
        {
            Uint64 Generation = 0;
            bool   Succeeded  = false;
        };

        RefCntAutoPtr<InterfaceType> Lock()
        {
            RefCntWeakPtr<InterfaceType> Object;
            {
                Threading::SpinLockGuard LockGuard{m_ObjectLock};
                Object = m_Object;
            }

            // Promote the weak pointer after releasing m_ObjectLock. Lock()
            // may touch reference counters and query interfaces; doing that
            // outside the spin lock keeps hot cache hits from serializing on
            // more than the weak-pointer copy.
            return Object.Lock();
        }

        void Set(InterfaceType* pObject)
        {
            RefCntWeakPtr<InterfaceType> NewObject{pObject};
            RefCntWeakPtr<InterfaceType> OldObject;

            {
                Threading::SpinLockGuard LockGuard{m_ObjectLock};
                OldObject = std::move(m_Object);
                m_Object  = std::move(NewObject);
            }

            // OldObject is released after m_ObjectLock is dropped. Releasing a
            // weak pointer can touch reference-counter metadata and allocator
            // state, so keep that work outside the spin lock.
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

            // The current thread owns the creation attempt for m_Generation
            // until it calls EndCreate(). Generation is incremented only when
            // the attempt completes, so waiters can identify the attempt they
            // joined.
            m_IsCreating    = true;
            m_CreatorThread = std::this_thread::get_id();
            return CreateState{CreateAction::Create, m_Generation};
        }

        CreateResult WaitCreate(Uint64 Generation)
        {
            std::unique_lock<std::mutex> LockGuard{m_CreateMtx};
            // Wait for the observed attempt to complete. We intentionally use
            // generation change as the completion signal rather than
            // !m_IsCreating: if another retry starts before this waiter runs,
            // the cache may be creating again, but the attempt this waiter
            // joined is already over.
            m_CreateCV.wait(LockGuard, [&]() {
                return m_Generation != Generation;
            });

            // The returned result describes the latest completed generation.
            // A very late waiter may observe a later generation than the one
            // it joined; GetOrCreate() detects that and retries instead of
            // consuming a result that belongs to another attempt.
            return CreateResult{m_Generation, m_LastCreateSucceeded};
        }

        void EndCreate(bool Succeeded)
        {
            {
                std::lock_guard<std::mutex> LockGuard{m_CreateMtx};
                VERIFY_EXPR(m_IsCreating);
                VERIFY_EXPR(m_CreatorThread == std::this_thread::get_id());

                // Store the result before advancing the generation. Waiters
                // that wake on this exact generation will see this result;
                // waiters that missed it will retry.
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

    class CreateGuard
    {
    public:
        explicit CreateGuard(ObjectEntry& Entry) noexcept :
            m_pEntry{&Entry}
        {
        }

        ~CreateGuard() noexcept
        {
            // Once BeginCreate() returns Create, every exit path must wake
            // waiters. The guard converts exceptions or early returns in the
            // create path into a failed creation attempt instead of leaving the
            // entry permanently in m_IsCreating state.
            if (m_pEntry != nullptr)
                m_pEntry->EndCreate(false);
        }

        // clang-format off
        CreateGuard           (const CreateGuard&) = delete;
        CreateGuard& operator=(const CreateGuard&) = delete;
        CreateGuard           (CreateGuard&&)      = delete;
        CreateGuard& operator=(CreateGuard&&)      = delete;
        // clang-format on

        void End(bool Succeeded)
        {
            if (m_pEntry != nullptr)
            {
                m_pEntry->EndCreate(Succeeded);
                m_pEntry = nullptr;
            }
        }

    private:
        ObjectEntry* m_pEntry = nullptr;
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
#ifdef DILIGENT_WEAK_OBJECT_CACHE_TEST_HOOKS
    using WaitCreateCallbackType = void (*)(const Char* CacheKey, void* pUserData);

    void SetWaitCreateCallback(WaitCreateCallbackType Callback, void* pUserData = nullptr)
    {
        m_WaitCreateCallback     = Callback;
        m_pWaitCreateCallbackCtx = pUserData;
    }
#endif

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

    /// Returns the number of map entries in the cache.
    ///
    /// This method is non-blocking and reads a single atomic counter. The
    /// returned value includes live entries, expired entries that have not been
    /// erased yet, and in-flight creation placeholders. Under concurrent
    /// modification, the value is a snapshot and may become stale immediately.
    size_t Size() const noexcept
    {
        return m_Size.load(std::memory_order_relaxed);
    }

    /// Reserves space for approximately ExpectedTotalEntries map entries.
    ///
    /// The reservation is distributed evenly between cache shards. This can be
    /// used before bulk loading to reduce unordered_map rehashes while shard
    /// locks are contended.
    void Reserve(size_t ExpectedTotalEntries)
    {
        if (ExpectedTotalEntries == 0)
            return;

        const size_t PerShard = (ExpectedTotalEntries + m_ShardCount - 1) / m_ShardCount;
        for (size_t ShardIdx = 0; ShardIdx < m_ShardCount; ++ShardIdx)
        {
            Shard& CacheShard = m_Shards[ShardIdx];

            std::unique_lock<std::shared_mutex> Lock{CacheShard.Mutex};
            CacheShard.Objects.reserve(PerShard);
        }
    }

    /// Returns a live object for the key, creating one if needed.
    ///
    /// If a live object already exists, the method returns it with Created set
    /// to false.
    ///
    /// If the key is missing or the weak object has expired, one caller becomes
    /// the creator and invokes CreateObjectFunc outside the shard lock. Other
    /// callers for the same key wait for that creation attempt to finish.
    ///
    /// If creation succeeds, the created object is stored as a weak reference
    /// and returned to the creator with Created set to true. Waiting callers
    /// retry the lookup. They usually return the newly created object with
    /// Created set to false, but because the cache stores only a weak
    /// reference, the object may already have expired if no external strong
    /// reference remains.
    ///
    /// If CreateObjectFunc returns null, the creator logs an error and returns
    /// null with Created set to false. Waiting callers wake and may return
    /// null with Created set to false. The failed placeholder entry is removed
    /// once no current caller still references it, and a later call may retry
    /// creation.
    ///
    /// If CreateObjectFunc throws, the exception is propagated to the creator.
    /// Waiting callers wake and may return null with Created set to false. A
    /// later call may retry creation. As with null factory results, the failed
    /// placeholder entry is removed once current callers release it.
    ///
    /// A waiter retries after the creation attempt it observed succeeds. If it
    /// wakes after later attempts have already completed, it retries the normal
    /// lookup path instead of using a result from the wrong generation. If the
    /// exact attempt it observed fails, the waiter returns null with Created
    /// set to false.
    ///
    /// If the key is null or empty, the method logs an error and returns null
    /// with Created set to false without invoking CreateObjectFunc.
    ///
    /// If a factory recursively requests the same key on the same thread, the
    /// recursive call logs an error and returns null with Created set to false.
    ///
    /// The factory may request other keys from the same cache, but callers must
    /// avoid cross-thread dependency cycles between keys.
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
            // Allocate outside the shard write lock. The map is rechecked
            // under the lock before inserting because another thread may have
            // inserted the entry after our shared-lock miss.
            std::shared_ptr<ObjectEntry> pNewEntry = std::make_shared<ObjectEntry>();

            std::unique_lock<std::shared_mutex> Lock{CacheShard.Mutex};

            const auto It = CacheShard.Objects.find(Key);
            if (It != CacheShard.Objects.end())
            {
                pEntry = It->second;
            }
            else
            {
                pEntry                 = std::move(pNewEntry);
                auto [NewIt, Inserted] = CacheShard.Objects.emplace(HashMapStringKey{CacheKey, true}, pEntry);
                VERIFY_EXPR(Inserted);
                if (Inserted)
                    m_Size.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // The loop is entered again after waiting for a successful creation,
        // after missing the exact generation we waited for, or after an object
        // expires before a waiter can promote the weak reference.
        for (;;)
        {
            if (RefCntAutoPtr<InterfaceType> pExisting = pEntry->Lock())
                return {std::move(pExisting), false};

            const typename ObjectEntry::CreateState State = pEntry->BeginCreate();
            if (State.Action == ObjectEntry::CreateAction::Wait)
            {
#ifdef DILIGENT_WEAK_OBJECT_CACHE_TEST_HOOKS
                if (m_WaitCreateCallback != nullptr)
                    m_WaitCreateCallback(CacheKey, m_pWaitCreateCallbackCtx);
#endif

                const typename ObjectEntry::CreateResult Result = pEntry->WaitCreate(State.Generation);
                // EndCreate() advances the generation exactly once. If the
                // observed generation is State.Generation + 1, Result belongs
                // to the attempt this waiter joined. Any other generation means
                // this waiter resumed late and the result may belong to a
                // different attempt, so retry the lookup/create path.
                if (Result.Generation != State.Generation + 1 || Result.Succeeded)
                    continue;

                TryEraseFailedEntry(CacheKey, std::move(pEntry));
                return {};
            }

            if (State.Action == ObjectEntry::CreateAction::Recursive)
            {
                LOG_ERROR_MESSAGE("Recursive object creation detected for cache key '", CacheKey, "'");
                return {};
            }

            CreateGuard Guard{*pEntry};

            // Re-check after becoming the creator. A previous creator may have
            // published an object between our initial Lock() and BeginCreate().
            // If so, do not run a duplicate factory.
            if (RefCntAutoPtr<InterfaceType> pExisting = pEntry->Lock())
            {
                Guard.End(true);
                return {std::move(pExisting), false};
            }

            // The factory is arbitrary user code and may mutate or destroy the
            // storage behind CacheKey. Keep an owned copy for all code that
            // runs after the factory starts, including logging and cleanup.
            const std::string StableCacheKey{CacheKey};

            RefCntAutoPtr<InterfaceType> pObject;
            try
            {
                pObject = std::forward<CreateObjectFuncType>(CreateObjectFunc)();
            }
            catch (...)
            {
                Guard.End(false);
                TryEraseFailedEntry(StableCacheKey.c_str(), std::move(pEntry));
                throw;
            }

            if (!pObject)
            {
                LOG_ERROR_MESSAGE("Failed to create object for cache key '", StableCacheKey.c_str(), "'");
                Guard.End(false);
                TryEraseFailedEntry(StableCacheKey.c_str(), std::move(pEntry));
                return {};
            }

            pEntry->Set(pObject.RawPtr());
            Guard.End(true);

            return {std::move(pObject), true};
        }
    }

    /// Removes the key if it exists and no live object or in-flight operation
    /// is associated with it.
    ///
    /// Returns true only when the entry is actually erased.
    ///
    /// Returns false if the key is missing, the object is still live, another
    /// thread currently holds the entry for creation or waiting, or the key is
    /// null or empty. Null or empty keys also log an error.
    bool EraseIfExpired(const Char* CacheKey)
    {
        if (CacheKey == nullptr || CacheKey[0] == '\0')
        {
            LOG_ERROR_MESSAGE("WeakObjectCache key must not be null or empty");
            return false;
        }

        const HashMapStringKey Key{CacheKey};
        Shard&                 CacheShard = GetShard(Key.GetHash());

        RefCntAutoPtr<InterfaceType> pLiveObject;
        std::shared_ptr<ObjectEntry> pRemovedEntry;
        bool                         Erased = false;

        {
            std::unique_lock<std::shared_mutex> Lock{CacheShard.Mutex};

            const auto It = CacheShard.Objects.find(Key);
            if (It == CacheShard.Objects.end())
                return false;

            const std::shared_ptr<ObjectEntry>& pEntry = It->second;
            // use_count() is checked while holding the shard mutex. This is required:
            // GetOrCreate() may only copy an ObjectEntry shared_ptr while holding the
            // same shard mutex, so use_count() == 1 means the map is the only owner.
            if (pEntry.use_count() == 1)
            {
                pLiveObject = pEntry->Lock();
                if (!pLiveObject)
                {
                    pRemovedEntry = std::move(It->second);
                    CacheShard.Objects.erase(It);
                    m_Size.fetch_sub(1, std::memory_order_relaxed);
                    Erased = true;
                }
            }
        }

        return Erased;
    }

    /// Removes all keys that have no live object or in-flight operation.
    ///
    /// Returns the number of entries actually erased.
    size_t EraseExpired()
    {
        size_t RemovedCount = 0;

        // Keep erased entries and temporary live references alive until after
        // the shard lock is released. Destroying an ObjectEntry or a temporary
        // strong reference can run reference-counter/allocator work and may
        // call back into code that touches the cache.
        std::vector<std::shared_ptr<ObjectEntry>> RemovedEntries;
        std::vector<RefCntAutoPtr<InterfaceType>> LiveObjects;

        for (size_t ShardIdx = 0; ShardIdx < m_ShardCount; ++ShardIdx)
        {
            RemovedEntries.clear();
            LiveObjects.clear();

            Shard& CacheShard = m_Shards[ShardIdx];

            size_t RemovedFromShard = 0;
            {
                std::unique_lock<std::shared_mutex> Lock{CacheShard.Mutex};

                for (auto It = CacheShard.Objects.begin(); It != CacheShard.Objects.end();)
                {
                    const std::shared_ptr<ObjectEntry>& pEntry = It->second;
                    // use_count() is checked while holding the shard mutex. This is required:
                    // GetOrCreate() may only copy an ObjectEntry shared_ptr while holding the
                    // same shard mutex, so use_count() == 1 means the map is the only owner.
                    if (pEntry.use_count() != 1)
                    {
                        ++It;
                        continue;
                    }

                    RefCntAutoPtr<InterfaceType> pLiveObject = pEntry->Lock();
                    if (pLiveObject)
                    {
                        LiveObjects.emplace_back(std::move(pLiveObject));
                        ++It;
                        continue;
                    }

                    RemovedEntries.emplace_back(std::move(It->second));
                    It = CacheShard.Objects.erase(It);
                    ++RemovedFromShard;
                }

                if (RemovedFromShard != 0)
                    m_Size.fetch_sub(RemovedFromShard, std::memory_order_relaxed);
            }

            RemovedCount += RemovedFromShard;
        }

        return RemovedCount;
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

    void TryEraseFailedEntry(const Char* CacheKey, std::shared_ptr<ObjectEntry> pEntry)
    {
        // The current call's shared_ptr would make ObjectEntry::use_count() > 1
        // and prevent EraseIfExpired() from removing a failed placeholder.
        // Drop it first; if other waiters are still returning from the same
        // failure, the last one to leave will erase the entry.
        pEntry.reset();
        EraseIfExpired(CacheKey);
    }

    const size_t             m_ShardCount;
    std::unique_ptr<Shard[]> m_Shards;
    std::atomic<size_t>      m_Size{0};
#ifdef DILIGENT_WEAK_OBJECT_CACHE_TEST_HOOKS
    WaitCreateCallbackType m_WaitCreateCallback     = nullptr;
    void*                  m_pWaitCreateCallbackCtx = nullptr;
#endif
};

} // namespace Diligent
