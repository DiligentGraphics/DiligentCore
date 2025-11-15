/*
 *  Copyright 2025 Diligent Graphics LLC
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

#include "WeakValueHashMap.hpp"

#include "gtest/gtest.h"

#include <vector>
#include <thread>

#include "ThreadSignal.hpp"

using namespace Diligent;

namespace
{

TEST(Common_WeakValueHashMap, GetOrInsert)
{
    {
        WeakValueHashMap<int, std::string> Map;

        auto Handle1 = Map.GetOrInsert(1, "Value");
        EXPECT_TRUE(Handle1);
        EXPECT_STREQ(Handle1->c_str(), "Value");
        EXPECT_EQ(*Handle1, std::string{"Value"});

        auto Handle2 = Map.Get(2);
        EXPECT_FALSE(Handle2);
    }

    {
        WeakValueHashMap<int, std::string> Map;

        auto Handle1 = Map.GetOrInsert(1, "Value");

        // Release map while the handle is still alive
        Map = {};

        EXPECT_TRUE(Handle1);
        EXPECT_STREQ(Handle1->c_str(), "Value");
        EXPECT_EQ(*Handle1, std::string{"Value"});
    }

    {
        WeakValueHashMap<int, std::string> Map;

        auto Handle1 = Map.GetOrInsert(1, "Value");

        // Release map while the handle is still alive
        Map = {};

        WeakValueHashMap<int, std::string>::ValueHandle Handle2{std::move(Handle1)};
        EXPECT_FALSE(Handle1);
        EXPECT_TRUE(Handle2);
        EXPECT_STREQ(Handle2->c_str(), "Value");
        EXPECT_EQ(*Handle2, std::string{"Value"});
    }

    {
        WeakValueHashMap<int, std::string> Map;

        auto Handle1 = Map.GetOrInsert(1, "Value");

        // Release map while the handle is still alive
        Map = {};

        WeakValueHashMap<int, std::string>::ValueHandle Handle2;
        Handle2 = std::move(Handle1);
        EXPECT_FALSE(Handle1);
        EXPECT_TRUE(Handle2);
        EXPECT_STREQ(Handle2->c_str(), "Value");
        EXPECT_EQ(*Handle2, std::string{"Value"});
    }

    {
        WeakValueHashMap<int, std::string> Map;

        auto Handle1 = Map.GetOrInsert(1, "Value1");
        Handle1      = Map.GetOrInsert(2, "Value2");
        EXPECT_TRUE(Handle1);
        EXPECT_STREQ(Handle1->c_str(), "Value2");
        EXPECT_EQ(*Handle1, std::string{"Value2"});

        Handle1 = Map.Get(1);
        EXPECT_FALSE(Handle1);
        Handle1 = Map.Get(2);
        EXPECT_FALSE(Handle1);
    }

    {
        WeakValueHashMap<int, std::string> Map;

        auto Handle1 = Map.GetOrInsert(1, "Value1");
        auto Handle2 = Map.GetOrInsert(1, "Value2");
        EXPECT_TRUE(Handle1);
        EXPECT_TRUE(Handle2);
        EXPECT_STREQ(Handle1->c_str(), "Value1");
        EXPECT_STREQ(Handle1->c_str(), Handle2->c_str());
        EXPECT_EQ(*Handle1, *Handle2);
    }
}


static constexpr size_t kNumThreads = 8;
#ifdef DILIGENT_DEBUG
static constexpr int kNumParallelKeys = 1024;
#else
static constexpr int kNumParallelKeys = 16384;
#endif

// Test that multiple threads can concurrently get or insert values into the map
TEST(Common_WeakValueHashMap, ParallelGetOrInsert1)
{
    std::vector<std::thread> Threads(kNumThreads);

    Threading::Signal StartSignal;

    WeakValueHashMap<int, std::string> Map;
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        Threads[t] = std::thread{
            [&Map, &StartSignal]() //
            {
                StartSignal.Wait(true, kNumThreads);

                for (int k = 0; k < kNumParallelKeys; ++k)
                {
                    std::string Value = "Value" + std::to_string(k);

                    auto Handle = Map.GetOrInsert(k, Value);
                    EXPECT_TRUE(Handle);
                    EXPECT_EQ(*Handle, Value);
                }
            }};
    }

    StartSignal.Trigger(true);
    for (auto& Thread : Threads)
    {
        Thread.join();
    }
}

// Similar to the previous test, but all values are kept alive
TEST(Common_WeakValueHashMap, ParallelGetOrInsert2)
{
    std::vector<std::thread> Threads(kNumThreads);

    Threading::Signal StartSignal;

    std::vector<WeakValueHashMap<int, std::string>::ValueHandle> Handles(kNumThreads * kNumParallelKeys);

    WeakValueHashMap<int, std::string> Map;
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        Threads[t] = std::thread{
            [&Map, &StartSignal, &Handles](size_t ThreadId) //
            {
                StartSignal.Wait(true, kNumThreads);

                for (int k = 0; k < kNumParallelKeys; ++k)
                {
                    std::string Value = "Value" + std::to_string(k);

                    auto Handle = Map.GetOrInsert(k, Value);
                    EXPECT_TRUE(Handle);
                    EXPECT_EQ(*Handle, Value);

                    Handles[ThreadId * kNumParallelKeys + k] = std::move(Handle);
                }
            },
            t,
        };
    }

    StartSignal.Trigger(true);
    for (auto& Thread : Threads)
    {
        Thread.join();
    }
}

} // namespace
