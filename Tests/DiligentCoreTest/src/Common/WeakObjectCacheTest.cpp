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

#include "WeakObjectCache.hpp"

#include "ObjectBase.hpp"
#include "TestingEnvironment.hpp"
#include "ThreadSignal.hpp"
#include "gtest/gtest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
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

} // namespace

TEST(Common_WeakObjectCache, CreatesAndReusesCachedObject)
{
    WeakObjectCache<TestObject> Cache;

    Uint32 CreateCount = 0;
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
    }

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
    EXPECT_EQ(Object->URI, "object://replacement");
    EXPECT_EQ(Object->Value, 4u);
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
    static constexpr Uint32 ThreadCount = 16;

    WeakObjectCache<TestObject> Cache{2};
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

TEST(Common_WeakObjectCache, ConcurrentRequestsReplaceExpiredEntryOnce)
{
    static constexpr Uint32 ThreadCount = 16;

    WeakObjectCache<TestObject> Cache{2};
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

TEST(Common_WeakObjectCache, ConcurrentLiveCacheHitsDoNotCallFactory)
{
    static constexpr Uint32 ThreadCount = 16;

    WeakObjectCache<TestObject> Cache{2};
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

TEST(Common_WeakObjectCache, ConcurrentRequestsForDifferentKeysCreateIndependentObjects)
{
    static constexpr Uint32 ThreadCount = 16;

    WeakObjectCache<TestObject> Cache{2};
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
