/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

#include "LRUCache.hpp"

#include "gtest/gtest.h"

#include <thread>
#include <functional>

#include "ThreadSignal.hpp"

using namespace Diligent;

namespace
{

struct CacheData
{
    Uint32 Value = ~0u;
};

TEST(Common_LRUCache, Get)
{
    LRUCache<int, CacheData> Cache{16};

    constexpr Uint32         NumThreads = 16;
    std::vector<std::thread> Threads(NumThreads);
    std::vector<CacheData>   Data(NumThreads);

    Threading::Signal StartSignal;
    for (Uint32 i = 0; i < NumThreads; ++i)
    {
        Threads[i] = std::thread(
            [&](Uint32 ThreadId) {
                StartSignal.Wait();
                // Get data with the same key from all threads
                Data[ThreadId] = Cache.Get(1,
                                           [&](CacheData& Data, size_t& Size) //
                                           {
                                               Data.Value = ThreadId;
                                               Size       = 1;
                                           });
            },
            i);
    }
    StartSignal.Trigger(true);

    for (auto& T : Threads)
        T.join();

    EXPECT_EQ(Cache.GetCurrSize(), size_t{1});
    for (size_t i = 1; i < Data.size(); ++i)
    {
        // Whatever thread first set the value should be the same for all threads
        EXPECT_EQ(Data[0].Value, Data[i].Value);
    }
}


TEST(Common_LRUCache, ReleaseQueue)
{
    LRUCache<int, CacheData> Cache{16};

    constexpr Uint32                    NumThreads = 16;
    std::vector<std::thread>            Threads(NumThreads);
    std::vector<std::vector<CacheData>> ThreadsData(NumThreads);

    Threading::Signal StartSignal;
    for (Uint32 i = 0; i < NumThreads; ++i)
    {
        ThreadsData[i].resize(128);

        Threads[i] = std::thread(
            [&](Uint32 ThreadId) {
                StartSignal.Wait();

                auto& Data = ThreadsData[ThreadId];
                for (Uint32 i = 0; i < Data.size(); ++i)
                {
                    // Set elements with the same keys from all threads
                    Data[i] = Cache.Get(i,
                                        [&](CacheData& Data, size_t& Size) //
                                        {
                                            Data.Value = i;
                                            Size       = 1;
                                        });
                }
            },
            i);
    }
    StartSignal.Trigger(true);

    for (auto& T : Threads)
        T.join();

    for (auto& Data : ThreadsData)
    {
        for (Uint32 i = 0; i < Data.size(); ++i)
        {
            EXPECT_EQ(Data[i].Value, i);
        }
    }
}


TEST(Common_LRUCache, Exceptions)
{
    LRUCache<int, CacheData> Cache{16};

    constexpr Uint32                    NumThreads = 15; // Use odd number
    std::vector<std::thread>            Threads(NumThreads);
    std::vector<std::vector<CacheData>> ThreadsData(NumThreads);

    Threading::Signal StartSignal;
    for (Uint32 i = 0; i < NumThreads; ++i)
    {
        ThreadsData[i].resize(128);

        Threads[i] = std::thread(
            [&](Uint32 ThreadId) {
                StartSignal.Wait();

                auto& Data = ThreadsData[ThreadId];
                for (Uint32 i = 0; i < Data.size(); ++i)
                {
                    try
                    {
                        // Set elements with the same keys from all threads
                        Data[i] = Cache.Get(i,
                                            [&](CacheData& Data, size_t& Size) //
                                            {
                                                // Throw exception from every other request.
                                                if ((i * NumThreads + ThreadId) % 2 == 0)
                                                    throw std::runtime_error("test error");

                                                Data.Value = i;
                                                Size       = 1;
                                            });
                    }
                    catch (...)
                    {
                    }
                }
            },
            i);
    }
    StartSignal.Trigger(true);

    for (auto& T : Threads)
        T.join();

    for (auto& Data : ThreadsData)
    {
        for (Uint32 i = 0; i < Data.size(); ++i)
        {
            auto Value = Data[i].Value;
            EXPECT_TRUE(Value == ~0u || Value == i);
        }
    }
}

} // namespace
