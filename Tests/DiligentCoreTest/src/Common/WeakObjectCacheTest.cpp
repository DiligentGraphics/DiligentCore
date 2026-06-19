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

#define DILIGENT_WEAK_OBJECT_CACHE_TEST_HOOKS 1
#include "WeakObjectCache.hpp"
#undef DILIGENT_WEAK_OBJECT_CACHE_TEST_HOOKS

#include "ObjectBase.hpp"
#include "TestingEnvironment.hpp"
#include "ThreadSignal.hpp"
#include "gtest/gtest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static constexpr INTERFACE_ID IID_TestObject = {0xe9f6ef96, 0x3eda, 0x4f09, {0x9e, 0x3b, 0x9c, 0xc8, 0x90, 0x34, 0x64, 0x28}};

class TestObject final : public ObjectBase<IObject>
{
public:
    TestObject(IReferenceCounters* pRefCounters, const char* _URI, Uint32 _Value) :
        ObjectBase<IObject>{pRefCounters},
        URI{_URI},
        Value{_Value}
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_TestObject, ObjectBase<IObject>)

    std::string URI;
    Uint32      Value = 0;
};

using TestObjectPtr = RefCntAutoPtr<TestObject>;

TestObjectPtr CreateTestObject(const char* URI, Uint32 Value)
{
    return TestObjectPtr{MakeNewRCObj<TestObject>()(URI, Value)};
}

class ThreadStartGate
{
public:
    explicit ThreadStartGate(Uint32 ThreadCount) :
        m_ThreadCount{static_cast<int>(ThreadCount)}
    {
    }

    void Wait()
    {
        if (m_ReadyCount.fetch_add(1, std::memory_order_acq_rel) + 1 == m_ThreadCount)
            m_StartSignal.Trigger(true);

        m_StartSignal.Wait(true, m_ThreadCount);
    }

private:
    const int         m_ThreadCount = 0;
    std::atomic<int>  m_ReadyCount{0};
    Threading::Signal m_StartSignal;
};

constexpr size_t ConcurrentShardCounts[] = {1, 2};

bool WaitUntilEquals(const std::atomic<Uint32>& Value, Uint32 Expected)
{
    const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (Value.load(std::memory_order_acquire) != Expected)
    {
        if (std::chrono::steady_clock::now() >= Deadline)
            return false;

        std::this_thread::yield();
    }

    return true;
}

void TestConcurrentRequestsCreateSingleObjectForSameKey(size_t ShardCount)
{
    static constexpr Uint32 ThreadCount = 16;

    WeakObjectCache<TestObject> Cache{ShardCount};
    ThreadStartGate             StartGate{ThreadCount};
    std::atomic<Uint32>         CreateCount{0};
    std::vector<std::thread>    Threads;
    std::vector<TestObjectPtr>  Objects(ThreadCount);
    std::vector<Uint8>          Created(ThreadCount, 0);

    Threads.reserve(ThreadCount);
    for (Uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back([&, ThreadIndex]() {
            StartGate.Wait();

            auto [Object, WasCreated] =
                Cache.GetOrCreate(
                    "object-key",
                    [&]() {
                        CreateCount.fetch_add(1, std::memory_order_acq_rel);
                        std::this_thread::sleep_for(std::chrono::milliseconds{1});
                        return CreateTestObject("object://threaded", 42);
                    });

            Objects[ThreadIndex] = std::move(Object);
            Created[ThreadIndex] = WasCreated ? 1 : 0;
        });
    }

    for (std::thread& Thread : Threads)
        Thread.join();

    ASSERT_NE(Objects[0], nullptr);
    EXPECT_EQ(CreateCount.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(std::count(Created.begin(), Created.end(), Uint8{1}), 1);

    for (const TestObjectPtr& Object : Objects)
    {
        ASSERT_NE(Object, nullptr);
        EXPECT_EQ(Object.RawPtr(), Objects[0].RawPtr());
        EXPECT_EQ(Object->Value, 42u);
    }
}

void TestConcurrentRequestsReplaceExpiredEntryOnce(size_t ShardCount)
{
    static constexpr Uint32 ThreadCount = 16;

    WeakObjectCache<TestObject> Cache{ShardCount};
    std::atomic<Uint32>         InitialCreateCount{0};
    {
        auto [InitialObject, InitialCreated] =
            Cache.GetOrCreate(
                "object-key",
                [&]() {
                    InitialCreateCount.fetch_add(1, std::memory_order_acq_rel);
                    return CreateTestObject("object://expired", 31);
                });

        ASSERT_NE(InitialObject, nullptr);
        EXPECT_TRUE(InitialCreated);
        EXPECT_EQ(InitialObject->Value, 31u);
    }

    EXPECT_EQ(InitialCreateCount.load(std::memory_order_acquire), 1u);

    ThreadStartGate            StartGate{ThreadCount};
    std::atomic<Uint32>        ReplacementCreateCount{0};
    std::vector<std::thread>   Threads;
    std::vector<TestObjectPtr> Objects(ThreadCount);
    std::vector<Uint8>         Created(ThreadCount, 0);

    Threads.reserve(ThreadCount);
    for (Uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back([&, ThreadIndex]() {
            StartGate.Wait();

            auto [Object, WasCreated] =
                Cache.GetOrCreate(
                    "object-key",
                    [&]() {
                        ReplacementCreateCount.fetch_add(1, std::memory_order_acq_rel);
                        std::this_thread::sleep_for(std::chrono::milliseconds{1});
                        return CreateTestObject("object://replacement", 32);
                    });

            Objects[ThreadIndex] = std::move(Object);
            Created[ThreadIndex] = WasCreated ? 1 : 0;
        });
    }

    for (std::thread& Thread : Threads)
        Thread.join();

    ASSERT_NE(Objects[0], nullptr);
    EXPECT_EQ(ReplacementCreateCount.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(std::count(Created.begin(), Created.end(), Uint8{1}), 1);

    for (const TestObjectPtr& Object : Objects)
    {
        ASSERT_NE(Object, nullptr);
        EXPECT_EQ(Object.RawPtr(), Objects[0].RawPtr());
        EXPECT_EQ(Object->URI, "object://replacement");
        EXPECT_EQ(Object->Value, 32u);
    }
}

void TestConcurrentLiveCacheHitsDoNotCallFactory(size_t ShardCount)
{
    static constexpr Uint32 ThreadCount = 16;

    WeakObjectCache<TestObject> Cache{ShardCount};
    std::atomic<Uint32>         CreateCount{0};

    auto [InitialObject, InitialCreated] =
        Cache.GetOrCreate(
            "object-key",
            [&]() {
                CreateCount.fetch_add(1, std::memory_order_acq_rel);
                return CreateTestObject("object://cached", 11);
            });

    ASSERT_NE(InitialObject, nullptr);
    EXPECT_TRUE(InitialCreated);

    ThreadStartGate            StartGate{ThreadCount};
    std::vector<std::thread>   Threads;
    std::vector<TestObjectPtr> Objects(ThreadCount);
    std::vector<Uint8>         Created(ThreadCount, 0);

    Threads.reserve(ThreadCount);
    for (Uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back([&, ThreadIndex]() {
            StartGate.Wait();

            auto [Object, WasCreated] =
                Cache.GetOrCreate(
                    "object-key",
                    [&]() {
                        CreateCount.fetch_add(1, std::memory_order_acq_rel);
                        return CreateTestObject("object://unexpected", 99);
                    });

            Objects[ThreadIndex] = std::move(Object);
            Created[ThreadIndex] = WasCreated ? 1 : 0;
        });
    }

    for (std::thread& Thread : Threads)
        Thread.join();

    EXPECT_EQ(CreateCount.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(std::count(Created.begin(), Created.end(), Uint8{1}), 0);

    for (const TestObjectPtr& Object : Objects)
    {
        ASSERT_NE(Object, nullptr);
        EXPECT_EQ(Object.RawPtr(), InitialObject.RawPtr());
        EXPECT_EQ(Object->Value, 11u);
    }
}

void TestConcurrentRequestsForDifferentKeysCreateIndependentObjects(size_t ShardCount)
{
    static constexpr Uint32 ThreadCount = 16;

    WeakObjectCache<TestObject> Cache{ShardCount};
    ThreadStartGate             StartGate{ThreadCount};
    std::atomic<Uint32>         CreateCount{0};
    std::vector<std::thread>    Threads;
    std::vector<TestObjectPtr>  Objects(ThreadCount);
    std::vector<Uint8>          Created(ThreadCount, 0);

    Threads.reserve(ThreadCount);
    for (Uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back([&, ThreadIndex]() {
            StartGate.Wait();

            const std::string CacheKey = "object-key-" + std::to_string(ThreadIndex);
            const std::string URI      = "object://threaded-" + std::to_string(ThreadIndex);
            auto [Object, WasCreated] =
                Cache.GetOrCreate(
                    CacheKey.c_str(),
                    [&, URI, ThreadIndex]() {
                        CreateCount.fetch_add(1, std::memory_order_acq_rel);
                        return CreateTestObject(URI.c_str(), ThreadIndex);
                    });

            Objects[ThreadIndex] = std::move(Object);
            Created[ThreadIndex] = WasCreated ? 1 : 0;
        });
    }

    for (std::thread& Thread : Threads)
        Thread.join();

    EXPECT_EQ(CreateCount.load(std::memory_order_acquire), ThreadCount);
    EXPECT_EQ(std::count(Created.begin(), Created.end(), Uint8{1}), ThreadCount);

    for (Uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        ASSERT_NE(Objects[ThreadIndex], nullptr);
        EXPECT_EQ(Objects[ThreadIndex]->Value, ThreadIndex);
        for (Uint32 OtherIndex = ThreadIndex + 1; OtherIndex < ThreadCount; ++OtherIndex)
            EXPECT_NE(Objects[ThreadIndex].RawPtr(), Objects[OtherIndex].RawPtr());
    }
}

} // namespace

TEST(Common_WeakObjectCache, CreatesAndReusesCachedObject)
{
    WeakObjectCache<TestObject> Cache;

    Uint32 CreateCount = 0;
    EXPECT_EQ(Cache.Size(), size_t{0});

    auto [FirstObject, FirstCreated] =
        Cache.GetOrCreate(
            "object-key",
            [&]() {
                ++CreateCount;
                return CreateTestObject("object://first", 17);
            });

    ASSERT_NE(FirstObject, nullptr);
    EXPECT_TRUE(FirstCreated);
    EXPECT_EQ(CreateCount, 1u);
    EXPECT_EQ(Cache.Size(), size_t{1});
    EXPECT_EQ(FirstObject->URI, "object://first");
    EXPECT_EQ(FirstObject->Value, 17u);

    auto [SecondObject, SecondCreated] =
        Cache.GetOrCreate(
            "object-key",
            [&]() {
                ADD_FAILURE() << "Factory must not be called when a live cache entry exists";
                ++CreateCount;
                return CreateTestObject("object://unexpected", 99);
            });

    ASSERT_NE(SecondObject, nullptr);
    EXPECT_FALSE(SecondCreated);
    EXPECT_EQ(CreateCount, 1u);
    EXPECT_EQ(Cache.Size(), size_t{1});
    EXPECT_EQ(SecondObject.RawPtr(), FirstObject.RawPtr());
    EXPECT_EQ(SecondObject->Value, 17u);
}

TEST(Common_WeakObjectCache, CreatesIndependentObjectsForDifferentKeys)
{
    WeakObjectCache<TestObject> Cache;

    Uint32 CreateCount  = 0;
    auto   CreateObject = [&](const char* URI, Uint32 Value) {
        return [&, URI, Value]() {
            ++CreateCount;
            return CreateTestObject(URI, Value);
        };
    };

    auto [FirstObject, FirstCreated] =
        Cache.GetOrCreate("object-key-0", CreateObject("object://first", 1));
    auto [SecondObject, SecondCreated] =
        Cache.GetOrCreate("object-key-1", CreateObject("object://second", 2));

    ASSERT_NE(FirstObject, nullptr);
    ASSERT_NE(SecondObject, nullptr);
    EXPECT_TRUE(FirstCreated);
    EXPECT_TRUE(SecondCreated);
    EXPECT_EQ(CreateCount, 2u);
    EXPECT_EQ(Cache.Size(), size_t{2});
    EXPECT_NE(SecondObject.RawPtr(), FirstObject.RawPtr());
    EXPECT_EQ(FirstObject->Value, 1u);
    EXPECT_EQ(SecondObject->Value, 2u);
}

TEST(Common_WeakObjectCache, ReplacesExpiredWeakEntry)
{
    WeakObjectCache<TestObject> Cache;

    Uint32 CreateCount = 0;
    {
        auto [Object, Created] =
            Cache.GetOrCreate(
                "object-key",
                [&]() {
                    ++CreateCount;
                    return CreateTestObject("object://expired", 3);
                });

        ASSERT_NE(Object, nullptr);
        EXPECT_TRUE(Created);
        EXPECT_EQ(Object->Value, 3u);
        EXPECT_EQ(Cache.Size(), size_t{1});
    }

    EXPECT_EQ(Cache.Size(), size_t{1});

    auto [Object, Created] =
        Cache.GetOrCreate(
            "object-key",
            [&]() {
                ++CreateCount;
                return CreateTestObject("object://replacement", 4);
            });

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(Created);
    EXPECT_EQ(CreateCount, 2u);
    EXPECT_EQ(Cache.Size(), size_t{1});
    EXPECT_EQ(Object->URI, "object://replacement");
    EXPECT_EQ(Object->Value, 4u);
}

TEST(Common_WeakObjectCache, EraseIfExpiredRemovesExpiredEntry)
{
    WeakObjectCache<TestObject> Cache;

    Uint32 CreateCount = 0;
    EXPECT_FALSE(Cache.EraseIfExpired("missing-key"));
    EXPECT_EQ(Cache.Size(), size_t{0});

    {
        auto [Object, Created] =
            Cache.GetOrCreate(
                "object-key",
                [&]() {
                    ++CreateCount;
                    return CreateTestObject("object://cached", 5);
                });

        ASSERT_NE(Object, nullptr);
        EXPECT_TRUE(Created);
        EXPECT_EQ(Cache.Size(), size_t{1});
        EXPECT_FALSE(Cache.EraseIfExpired("object-key"));
        EXPECT_EQ(Cache.Size(), size_t{1});

        auto [CachedObject, CachedCreated] =
            Cache.GetOrCreate(
                "object-key",
                [&]() {
                    ADD_FAILURE() << "Factory must not be called when a live cache entry exists";
                    return CreateTestObject("object://unexpected", 6);
                });

        ASSERT_NE(CachedObject, nullptr);
        EXPECT_FALSE(CachedCreated);
        EXPECT_EQ(Cache.Size(), size_t{1});
        EXPECT_EQ(CachedObject.RawPtr(), Object.RawPtr());
    }

    EXPECT_TRUE(Cache.EraseIfExpired("object-key"));
    EXPECT_EQ(Cache.Size(), size_t{0});
    EXPECT_FALSE(Cache.EraseIfExpired("object-key"));
    EXPECT_EQ(Cache.Size(), size_t{0});

    auto [Object, Created] =
        Cache.GetOrCreate(
            "object-key",
            [&]() {
                ++CreateCount;
                return CreateTestObject("object://created-after-erase", 7);
            });

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(Created);
    EXPECT_EQ(CreateCount, 2u);
    EXPECT_EQ(Cache.Size(), size_t{1});
    EXPECT_EQ(Object->Value, 7u);
}

TEST(Common_WeakObjectCache, EraseIfExpiredReturnsFalseForNullOrEmptyKey)
{
    WeakObjectCache<TestObject> Cache;

    {
        TestingEnvironment::ErrorScope ExpectedErrors{"WeakObjectCache key must not be null or empty"};
        EXPECT_FALSE(Cache.EraseIfExpired(nullptr));
    }

    {
        TestingEnvironment::ErrorScope ExpectedErrors{"WeakObjectCache key must not be null or empty"};
        EXPECT_FALSE(Cache.EraseIfExpired(""));
    }
}

TEST(Common_WeakObjectCache, EraseExpiredRemovesExpiredEntries)
{
    WeakObjectCache<TestObject> Cache{2};

    Uint32 CreateCount = 0;
    {
        auto [Object, Created] =
            Cache.GetOrCreate(
                "expired-key-0",
                [&]() {
                    ++CreateCount;
                    return CreateTestObject("object://expired-0", 8);
                });

        ASSERT_NE(Object, nullptr);
        EXPECT_TRUE(Created);
    }

    {
        auto [Object, Created] =
            Cache.GetOrCreate(
                "expired-key-1",
                [&]() {
                    ++CreateCount;
                    return CreateTestObject("object://expired-1", 9);
                });

        ASSERT_NE(Object, nullptr);
        EXPECT_TRUE(Created);
    }

    TestObjectPtr LiveObject;
    {
        auto [Object, Created] =
            Cache.GetOrCreate(
                "live-key",
                [&]() {
                    ++CreateCount;
                    return CreateTestObject("object://live", 10);
                });

        ASSERT_NE(Object, nullptr);
        EXPECT_TRUE(Created);
        LiveObject = std::move(Object);
    }

    {
        TestingEnvironment::ErrorScope ExpectedErrors{"Failed to create object for cache key 'failed-key'"};
        auto [Object, Created] =
            Cache.GetOrCreate(
                "failed-key",
                []() -> RefCntAutoPtr<TestObject> {
                    return {};
                });

        EXPECT_EQ(Object, nullptr);
        EXPECT_FALSE(Created);
    }

    EXPECT_EQ(CreateCount, 3u);
    EXPECT_EQ(Cache.Size(), size_t{3});
    EXPECT_EQ(Cache.EraseExpired(), size_t{2});
    EXPECT_EQ(Cache.Size(), size_t{1});
    EXPECT_EQ(Cache.EraseExpired(), size_t{0});
    EXPECT_EQ(Cache.Size(), size_t{1});

    ASSERT_NE(LiveObject, nullptr);
    EXPECT_EQ(LiveObject->Value, 10u);
    LiveObject.Release();

    EXPECT_EQ(Cache.EraseExpired(), size_t{1});
    EXPECT_EQ(Cache.Size(), size_t{0});
}

TEST(Common_WeakObjectCache, EraseExpiredKeepsInFlightEntry)
{
    WeakObjectCache<TestObject> Cache{1};

    {
        auto [Object, Created] =
            Cache.GetOrCreate(
                "expired-key",
                []() {
                    return CreateTestObject("object://expired", 11);
                });

        ASSERT_NE(Object, nullptr);
        EXPECT_TRUE(Created);
    }

    Threading::Signal FactoryStarted;
    Threading::Signal FinishFactory;
    TestObjectPtr     Object;
    bool              Created = false;

    std::thread Worker{[&]() {
        auto [CreatedObject, WasCreated] =
            Cache.GetOrCreate(
                "in-flight-key",
                [&]() {
                    FactoryStarted.Trigger(true);
                    FinishFactory.Wait(true, 1);
                    return CreateTestObject("object://created", 12);
                });

        Object  = std::move(CreatedObject);
        Created = WasCreated;
    }};

    FactoryStarted.Wait(true, 1);
    EXPECT_EQ(Cache.Size(), size_t{2});
    EXPECT_EQ(Cache.EraseExpired(), size_t{1});
    EXPECT_EQ(Cache.Size(), size_t{1});

    FinishFactory.Trigger(true);
    Worker.join();

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(Created);
    EXPECT_EQ(Cache.EraseExpired(), size_t{0});
    EXPECT_EQ(Cache.Size(), size_t{1});

    Object.Release();
    EXPECT_EQ(Cache.EraseExpired(), size_t{1});
    EXPECT_EQ(Cache.Size(), size_t{0});
}

TEST(Common_WeakObjectCache, EraseIfExpiredKeepsInFlightEntry)
{
    WeakObjectCache<TestObject> Cache{1};

    Threading::Signal FactoryStarted;
    Threading::Signal FinishFactory;
    TestObjectPtr     Object;
    bool              Created = false;

    std::thread Worker{[&]() {
        auto [CreatedObject, WasCreated] =
            Cache.GetOrCreate(
                "object-key",
                [&]() {
                    FactoryStarted.Trigger(true);
                    FinishFactory.Wait(true, 1);
                    return CreateTestObject("object://created", 8);
                });

        Object  = std::move(CreatedObject);
        Created = WasCreated;
    }};

    FactoryStarted.Wait(true, 1);
    EXPECT_EQ(Cache.Size(), size_t{1});
    EXPECT_FALSE(Cache.EraseIfExpired("object-key"));

    FinishFactory.Trigger(true);
    Worker.join();

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(Created);
    EXPECT_FALSE(Cache.EraseIfExpired("object-key"));

    Object.Release();
    EXPECT_TRUE(Cache.EraseIfExpired("object-key"));
    EXPECT_EQ(Cache.Size(), size_t{0});
}

TEST(Common_WeakObjectCache, ReturnsEmptyWhenFactoryFails)
{
    WeakObjectCache<TestObject> Cache;

    Uint32 CreateCount = 0;
    {
        TestingEnvironment::ErrorScope ExpectedErrors{"Failed to create object for cache key 'object-key'"};
        auto [Object, Created] =
            Cache.GetOrCreate(
                "object-key",
                [&]() -> RefCntAutoPtr<TestObject> {
                    ++CreateCount;
                    return {};
                });

        EXPECT_EQ(Object, nullptr);
        EXPECT_FALSE(Created);
    }
    EXPECT_EQ(Cache.Size(), size_t{0});

    auto [Object, Created] =
        Cache.GetOrCreate(
            "object-key",
            [&]() {
                ++CreateCount;
                return CreateTestObject("object://created-after-failure", 5);
            });

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(Created);
    EXPECT_EQ(CreateCount, 2u);
    EXPECT_EQ(Object->Value, 5u);
}

TEST(Common_WeakObjectCache, FactoryMayMutateCacheKeyStorageOnFailure)
{
    WeakObjectCache<TestObject> Cache;

    std::string Key{"object-key"};
    {
        TestingEnvironment::ErrorScope ExpectedErrors{"Failed to create object for cache key 'object-key'"};
        auto [Object, Created] =
            Cache.GetOrCreate(
                Key.c_str(),
                [&]() -> RefCntAutoPtr<TestObject> {
                    Key = "different-key";
                    return {};
                });

        EXPECT_EQ(Object, nullptr);
        EXPECT_FALSE(Created);
    }

    EXPECT_EQ(Key, "different-key");
    EXPECT_EQ(Cache.Size(), size_t{0});

    auto [Object, Created] =
        Cache.GetOrCreate(
            "object-key",
            []() {
                return CreateTestObject("object://created-after-key-mutation", 25);
            });

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(Created);
    EXPECT_EQ(Object->Value, 25u);
}

TEST(Common_WeakObjectCache, ConcurrentFactoryFailureWakesAllWaiters)
{
    static constexpr Uint32 WaiterCount = 8;

    WeakObjectCache<TestObject> Cache{1};
    Threading::Signal           FactoryStarted;
    Threading::Signal           StartWaiters;
    Threading::Signal           FinishFactory;
    std::atomic<Uint32>         WaitersReady{0};
    std::atomic<Uint32>         WaitersWaiting{0};
    std::atomic<Uint32>         CreateCount{0};
    TestObjectPtr               CreatorObject;
    bool                        CreatorCreated = false;

    Cache.SetWaitCreateCallback(
        [](const Char*, void* pUserData) {
            auto& Waiting = *static_cast<std::atomic<Uint32>*>(pUserData);
            Waiting.fetch_add(1, std::memory_order_acq_rel);
        },
        &WaitersWaiting);

    std::thread Creator{[&]() {
        auto [Object, Created] =
            Cache.GetOrCreate(
                "object-key",
                [&]() -> RefCntAutoPtr<TestObject> {
                    CreateCount.fetch_add(1, std::memory_order_acq_rel);
                    FactoryStarted.Trigger(true);
                    FinishFactory.Wait(true, 1);
                    return {};
                });

        CreatorObject  = std::move(Object);
        CreatorCreated = Created;
    }};

    FactoryStarted.Wait(true, 1);

    std::vector<std::thread>   Waiters;
    std::vector<TestObjectPtr> Objects(WaiterCount);
    std::vector<Uint8>         Created(WaiterCount, 0);

    Waiters.reserve(WaiterCount);
    for (Uint32 WaiterIndex = 0; WaiterIndex < WaiterCount; ++WaiterIndex)
    {
        Waiters.emplace_back([&, WaiterIndex]() {
            WaitersReady.fetch_add(1, std::memory_order_acq_rel);
            StartWaiters.Wait(true, WaiterCount);

            auto [Object, WasCreated] =
                Cache.GetOrCreate(
                    "object-key",
                    [&]() {
                        ADD_FAILURE() << "Waiter factory must not be called";
                        CreateCount.fetch_add(1, std::memory_order_acq_rel);
                        return CreateTestObject("object://unexpected", WaiterIndex);
                    });

            Objects[WaiterIndex] = std::move(Object);
            Created[WaiterIndex] = WasCreated ? 1 : 0;
        });
    }

    while (WaitersReady.load(std::memory_order_acquire) != WaiterCount)
        std::this_thread::yield();

    StartWaiters.Trigger(true, WaiterCount);

    while (WaitersWaiting.load(std::memory_order_acquire) != WaiterCount)
        std::this_thread::yield();

    {
        TestingEnvironment::ErrorScope ExpectedErrors{"Failed to create object for cache key 'object-key'"};
        FinishFactory.Trigger(true);
        Creator.join();
        for (std::thread& Waiter : Waiters)
            Waiter.join();
    }

    EXPECT_EQ(CreatorObject, nullptr);
    EXPECT_FALSE(CreatorCreated);
    EXPECT_EQ(CreateCount.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(Cache.Size(), size_t{0});

    for (const TestObjectPtr& Object : Objects)
        EXPECT_EQ(Object, nullptr);
    EXPECT_EQ(std::count(Created.begin(), Created.end(), Uint8{1}), 0);

    auto [Object, CreatedAfterFailure] =
        Cache.GetOrCreate(
            "object-key",
            [&]() {
                CreateCount.fetch_add(1, std::memory_order_acq_rel);
                return CreateTestObject("object://created-after-failure", 6);
            });

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(CreatedAfterFailure);
    EXPECT_EQ(CreateCount.load(std::memory_order_acquire), 2u);
    EXPECT_EQ(Object->Value, 6u);
}

TEST(Common_WeakObjectCache, PropagatesFactoryExceptionAndAllowsRetry)
{
    WeakObjectCache<TestObject> Cache;

    Uint32 CreateCount = 0;
    EXPECT_THROW(
        Cache.GetOrCreate(
            "object-key",
            [&]() -> RefCntAutoPtr<TestObject> {
                ++CreateCount;
                throw std::runtime_error{"factory failed"};
            }),
        std::runtime_error);
    EXPECT_EQ(Cache.Size(), size_t{0});

    auto [Object, Created] =
        Cache.GetOrCreate(
            "object-key",
            [&]() {
                ++CreateCount;
                return CreateTestObject("object://created-after-exception", 6);
            });

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(Created);
    EXPECT_EQ(CreateCount, 2u);
    EXPECT_EQ(Object->Value, 6u);
}

TEST(Common_WeakObjectCache, FactoryMayMutateCacheKeyStorageOnException)
{
    WeakObjectCache<TestObject> Cache;

    std::string Key{"object-key"};
    EXPECT_THROW(
        Cache.GetOrCreate(
            Key.c_str(),
            [&]() -> RefCntAutoPtr<TestObject> {
                Key = "different-key";
                throw std::runtime_error{"factory failed"};
            }),
        std::runtime_error);

    EXPECT_EQ(Key, "different-key");
    EXPECT_EQ(Cache.Size(), size_t{0});

    auto [Object, Created] =
        Cache.GetOrCreate(
            "object-key",
            []() {
                return CreateTestObject("object://created-after-key-mutation", 26);
            });

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(Created);
    EXPECT_EQ(Object->Value, 26u);
}

TEST(Common_WeakObjectCache, ConcurrentFactoryExceptionWakesAllWaiters)
{
    static constexpr Uint32 WaiterCount = 8;

    WeakObjectCache<TestObject> Cache{1};
    Threading::Signal           FactoryStarted;
    Threading::Signal           StartWaiters;
    Threading::Signal           FinishFactory;
    std::atomic<Uint32>         WaitersReady{0};
    std::atomic<Uint32>         WaitersWaiting{0};
    std::atomic<Uint32>         CreateCount{0};
    bool                        CaughtException = false;

    Cache.SetWaitCreateCallback(
        [](const Char*, void* pUserData) {
            auto& Waiting = *static_cast<std::atomic<Uint32>*>(pUserData);
            Waiting.fetch_add(1, std::memory_order_acq_rel);
        },
        &WaitersWaiting);

    std::thread Creator{[&]() {
        try
        {
            auto [Object, Created] =
                Cache.GetOrCreate(
                    "object-key",
                    [&]() -> RefCntAutoPtr<TestObject> {
                        CreateCount.fetch_add(1, std::memory_order_acq_rel);
                        FactoryStarted.Trigger(true);
                        FinishFactory.Wait(true, 1);
                        throw std::runtime_error{"factory failed"};
                    });

            ADD_FAILURE() << "Factory exception must be propagated";
            EXPECT_EQ(Object, nullptr);
            EXPECT_FALSE(Created);
        }
        catch (const std::runtime_error&)
        {
            CaughtException = true;
        }
    }};

    FactoryStarted.Wait(true, 1);

    std::vector<std::thread>   Waiters;
    std::vector<TestObjectPtr> Objects(WaiterCount);
    std::vector<Uint8>         Created(WaiterCount, 0);

    Waiters.reserve(WaiterCount);
    for (Uint32 WaiterIndex = 0; WaiterIndex < WaiterCount; ++WaiterIndex)
    {
        Waiters.emplace_back([&, WaiterIndex]() {
            WaitersReady.fetch_add(1, std::memory_order_acq_rel);
            StartWaiters.Wait(true, WaiterCount);

            auto [Object, WasCreated] =
                Cache.GetOrCreate(
                    "object-key",
                    [&]() {
                        ADD_FAILURE() << "Waiter factory must not be called";
                        CreateCount.fetch_add(1, std::memory_order_acq_rel);
                        return CreateTestObject("object://unexpected", WaiterIndex);
                    });

            Objects[WaiterIndex] = std::move(Object);
            Created[WaiterIndex] = WasCreated ? 1 : 0;
        });
    }

    while (WaitersReady.load(std::memory_order_acquire) != WaiterCount)
        std::this_thread::yield();

    StartWaiters.Trigger(true, WaiterCount);

    while (WaitersWaiting.load(std::memory_order_acquire) != WaiterCount)
        std::this_thread::yield();

    FinishFactory.Trigger(true);
    Creator.join();
    for (std::thread& Waiter : Waiters)
        Waiter.join();

    EXPECT_TRUE(CaughtException);
    EXPECT_EQ(CreateCount.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(Cache.Size(), size_t{0});

    for (const TestObjectPtr& Object : Objects)
        EXPECT_EQ(Object, nullptr);
    EXPECT_EQ(std::count(Created.begin(), Created.end(), Uint8{1}), 0);

    auto [Object, CreatedAfterException] =
        Cache.GetOrCreate(
            "object-key",
            [&]() {
                CreateCount.fetch_add(1, std::memory_order_acq_rel);
                return CreateTestObject("object://created-after-exception", 7);
            });

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(CreatedAfterException);
    EXPECT_EQ(CreateCount.load(std::memory_order_acquire), 2u);
    EXPECT_EQ(Object->Value, 7u);
}

TEST(Common_WeakObjectCache, WaiterRetriesWhenCreationGenerationIsMissed)
{
    struct WaiterHook
    {
        Threading::Signal* pWaiterReady   = nullptr;
        Threading::Signal* pReleaseWaiter = nullptr;
    };

    WeakObjectCache<TestObject> Cache{1};
    Threading::Signal           FirstFactoryStarted;
    Threading::Signal           FinishFirstFactory;
    Threading::Signal           WaiterReady;
    Threading::Signal           ReleaseWaiter;
    std::atomic<Uint32>         CreateCount{0};
    std::atomic<Uint32>         WaiterCreateCount{0};
    TestObjectPtr               WaiterObject;
    bool                        WaiterCreated = false;
    bool                        FirstCreated  = false;

    WaiterHook Hook{&WaiterReady, &ReleaseWaiter};
    Cache.SetWaitCreateCallback(
        [](const Char*, void* pUserData) {
            auto& Hook = *static_cast<WaiterHook*>(pUserData);
            Hook.pWaiterReady->Trigger(true);
            Hook.pReleaseWaiter->Wait(true, 1);
        },
        &Hook);

    std::thread FirstCreator{[&]() {
        auto [Object, Created] =
            Cache.GetOrCreate(
                "object-key",
                [&]() {
                    CreateCount.fetch_add(1, std::memory_order_acq_rel);
                    FirstFactoryStarted.Trigger(true);
                    FinishFirstFactory.Wait(true, 1);
                    return CreateTestObject("object://first", 8);
                });

        EXPECT_NE(Object, nullptr);
        EXPECT_TRUE(Created);
        FirstCreated = Created;
    }};

    FirstFactoryStarted.Wait(true, 1);

    std::thread Waiter{[&]() {
        auto [Object, Created] =
            Cache.GetOrCreate(
                "object-key",
                [&]() {
                    CreateCount.fetch_add(1, std::memory_order_acq_rel);
                    WaiterCreateCount.fetch_add(1, std::memory_order_acq_rel);
                    return CreateTestObject("object://waiter-retry", 10);
                });

        WaiterObject  = std::move(Object);
        WaiterCreated = Created;
    }};

    WaiterReady.Wait(true, 1);

    FinishFirstFactory.Trigger(true);
    FirstCreator.join();
    EXPECT_TRUE(FirstCreated);

    {
        TestingEnvironment::ErrorScope ExpectedErrors{"Failed to create object for cache key 'object-key'"};
        auto [Object, Created] =
            Cache.GetOrCreate(
                "object-key",
                [&]() -> RefCntAutoPtr<TestObject> {
                    CreateCount.fetch_add(1, std::memory_order_acq_rel);
                    return {};
                });

        EXPECT_EQ(Object, nullptr);
        EXPECT_FALSE(Created);
    }

    ReleaseWaiter.Trigger(true);
    Waiter.join();

    ASSERT_NE(WaiterObject, nullptr);
    EXPECT_TRUE(WaiterCreated);
    EXPECT_EQ(WaiterObject->URI, "object://waiter-retry");
    EXPECT_EQ(WaiterObject->Value, 10u);
    EXPECT_EQ(CreateCount.load(std::memory_order_acquire), 3u);
    EXPECT_EQ(WaiterCreateCount.load(std::memory_order_acquire), 1u);
}

TEST(Common_WeakObjectCache, ReturnsEmptyForNullOrEmptyKey)
{
    WeakObjectCache<TestObject> Cache;

    Uint32 CreateCount = 0;
    {
        TestingEnvironment::ErrorScope ExpectedErrors{"WeakObjectCache key must not be null or empty"};
        auto [Object, Created] =
            Cache.GetOrCreate(
                nullptr,
                [&]() {
                    ++CreateCount;
                    return CreateTestObject("object://unexpected", 6);
                });

        EXPECT_EQ(Object, nullptr);
        EXPECT_FALSE(Created);
    }

    {
        TestingEnvironment::ErrorScope ExpectedErrors{"WeakObjectCache key must not be null or empty"};
        auto [Object, Created] =
            Cache.GetOrCreate(
                "",
                [&]() {
                    ++CreateCount;
                    return CreateTestObject("object://unexpected", 7);
                });

        EXPECT_EQ(Object, nullptr);
        EXPECT_FALSE(Created);
    }

    EXPECT_EQ(CreateCount, 0u);
}

TEST(Common_WeakObjectCache, FactoryCanCreateDifferentKeyInSameShard)
{
    WeakObjectCache<TestObject> Cache{1};

    Uint32 CreateCount = 0;
    auto [Object, Created] =
        Cache.GetOrCreate(
            "material-key",
            [&]() {
                ++CreateCount;

                auto [DependencyObject, DependencyCreated] =
                    Cache.GetOrCreate(
                        "texture-key",
                        [&]() {
                            ++CreateCount;
                            return CreateTestObject("object://dependency", 21);
                        });

                EXPECT_NE(DependencyObject, nullptr);
                EXPECT_TRUE(DependencyCreated);
                EXPECT_EQ(DependencyObject->Value, 21u);

                return CreateTestObject("object://material", 22);
            });

    ASSERT_NE(Object, nullptr);
    EXPECT_TRUE(Created);
    EXPECT_EQ(CreateCount, 2u);
    EXPECT_EQ(Object->URI, "object://material");
    EXPECT_EQ(Object->Value, 22u);
}

TEST(Common_WeakObjectCache, DifferentKeyFactoriesInSameShardRunConcurrently)
{
    static constexpr Uint32 ThreadCount = 2;

    WeakObjectCache<TestObject> Cache{1};
    ThreadStartGate             StartGate{ThreadCount};
    Threading::Signal           ReleaseFactories;
    std::atomic<Uint32>         FactoriesEntered{0};
    std::atomic<Uint32>         CreateCount{0};
    std::vector<std::thread>    Threads;
    std::vector<TestObjectPtr>  Objects(ThreadCount);
    std::vector<Uint8>          Created(ThreadCount, 0);

    Threads.reserve(ThreadCount);
    for (Uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back([&, ThreadIndex]() {
            StartGate.Wait();

            const std::string CacheKey = "object-key-" + std::to_string(ThreadIndex);
            const std::string URI      = "object://same-shard-" + std::to_string(ThreadIndex);
            auto [Object, WasCreated] =
                Cache.GetOrCreate(
                    CacheKey.c_str(),
                    [&, URI, ThreadIndex]() {
                        FactoriesEntered.fetch_add(1, std::memory_order_acq_rel);
                        ReleaseFactories.Wait(true, ThreadCount);
                        CreateCount.fetch_add(1, std::memory_order_acq_rel);
                        return CreateTestObject(URI.c_str(), ThreadIndex);
                    });

            Objects[ThreadIndex] = std::move(Object);
            Created[ThreadIndex] = WasCreated ? 1 : 0;
        });
    }

    const bool FactoriesRanConcurrently = WaitUntilEquals(FactoriesEntered, ThreadCount);
    ReleaseFactories.Trigger(true, ThreadCount);

    for (std::thread& Thread : Threads)
        Thread.join();

    EXPECT_TRUE(FactoriesRanConcurrently) << "Different-key factories in the same shard did not overlap";
    EXPECT_EQ(CreateCount.load(std::memory_order_acquire), ThreadCount);
    EXPECT_EQ(std::count(Created.begin(), Created.end(), Uint8{1}), ThreadCount);

    for (Uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        ASSERT_NE(Objects[ThreadIndex], nullptr);
        EXPECT_EQ(Objects[ThreadIndex]->Value, ThreadIndex);
    }
}

TEST(Common_WeakObjectCache, RecursiveFactoryForSameKeyReturnsEmpty)
{
    WeakObjectCache<TestObject> Cache{1};

    Uint32 CreateCount = 0;
    {
        TestingEnvironment::ErrorScope ExpectedErrors{"Recursive object creation detected for cache key 'object-key'"};

        auto [Object, Created] =
            Cache.GetOrCreate(
                "object-key",
                [&]() {
                    ++CreateCount;

                    auto [RecursiveObject, RecursiveCreated] =
                        Cache.GetOrCreate(
                            "object-key",
                            [&]() {
                                ADD_FAILURE() << "Recursive factory must not be called";
                                return CreateTestObject("object://recursive", 23);
                            });

                    EXPECT_EQ(RecursiveObject, nullptr);
                    EXPECT_FALSE(RecursiveCreated);

                    return CreateTestObject("object://created", 24);
                });

        ASSERT_NE(Object, nullptr);
        EXPECT_TRUE(Created);
        EXPECT_EQ(Object->Value, 24u);
    }

    EXPECT_EQ(CreateCount, 1u);
}

TEST(Common_WeakObjectCache, ConcurrentRequestsCreateSingleObjectForSameKey)
{
    for (const size_t ShardCount : ConcurrentShardCounts)
    {
        SCOPED_TRACE(::testing::Message{} << "ShardCount: " << ShardCount);
        TestConcurrentRequestsCreateSingleObjectForSameKey(ShardCount);
    }
}

TEST(Common_WeakObjectCache, ConcurrentRequestsReplaceExpiredEntryOnce)
{
    for (const size_t ShardCount : ConcurrentShardCounts)
    {
        SCOPED_TRACE(::testing::Message{} << "ShardCount: " << ShardCount);
        TestConcurrentRequestsReplaceExpiredEntryOnce(ShardCount);
    }
}

TEST(Common_WeakObjectCache, ConcurrentLiveCacheHitsDoNotCallFactory)
{
    for (const size_t ShardCount : ConcurrentShardCounts)
    {
        SCOPED_TRACE(::testing::Message{} << "ShardCount: " << ShardCount);
        TestConcurrentLiveCacheHitsDoNotCallFactory(ShardCount);
    }
}

TEST(Common_WeakObjectCache, ConcurrentRequestsForDifferentKeysCreateIndependentObjects)
{
    for (const size_t ShardCount : ConcurrentShardCounts)
    {
        SCOPED_TRACE(::testing::Message{} << "ShardCount: " << ShardCount);
        TestConcurrentRequestsForDifferentKeysCreateIndependentObjects(ShardCount);
    }
}
