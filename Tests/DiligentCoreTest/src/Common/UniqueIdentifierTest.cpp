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

#include "UniqueIdentifier.hpp"
#include "ThreadSignal.hpp"
#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <utility>
#include <vector>

using namespace Diligent;

namespace
{

struct ConcurrentTestObjectClass;
struct MoveTestObjectClass;
struct CounterTestObjectClass;
struct OtherCounterTestObjectClass;

} // namespace

TEST(Common_UniqueIdentifier, ReturnsStableIdFromMultipleThreads)
{
    static constexpr Uint32 ThreadCount = 32;

    UniqueIdHelper<ConcurrentTestObjectClass> Helper;
    Threading::Signal                         StartSignal;
    std::atomic<Uint32>                       ReadyCount{0};
    std::vector<std::thread>                  Threads;
    std::vector<UniqueIdentifier>             IDs(ThreadCount);

    Threads.reserve(ThreadCount);
    for (Uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back([&, ThreadIndex]() {
            ReadyCount.fetch_add(1, std::memory_order_acq_rel);
            StartSignal.Wait(true, ThreadCount);

            IDs[ThreadIndex] = Helper.GetID();
        });
    }

    while (ReadyCount.load(std::memory_order_acquire) != ThreadCount)
        std::this_thread::yield();

    StartSignal.Trigger(true, ThreadCount);

    for (std::thread& Thread : Threads)
        Thread.join();

    ASSERT_NE(IDs[0], 0);
    for (const UniqueIdentifier ID : IDs)
        EXPECT_EQ(ID, IDs[0]);

    EXPECT_EQ(Helper.GetID(), IDs[0]);
}

TEST(Common_UniqueIdentifier, MoveTransfersId)
{
    UniqueIdHelper<MoveTestObjectClass> Helper0;
    const UniqueIdentifier              ID0 = Helper0.GetID();

    UniqueIdHelper<MoveTestObjectClass> Helper1{std::move(Helper0)};
    EXPECT_EQ(Helper1.GetID(), ID0);

    const UniqueIdentifier NewID0 = Helper0.GetID();
    EXPECT_NE(NewID0, 0);
    EXPECT_NE(NewID0, ID0);

    UniqueIdHelper<MoveTestObjectClass> Helper2;
    const UniqueIdentifier              ID2 = Helper2.GetID();
    EXPECT_NE(ID2, ID0);
    EXPECT_NE(ID2, NewID0);

    Helper2 = std::move(Helper1);
    EXPECT_EQ(Helper2.GetID(), ID0);
}

TEST(Common_UniqueIdentifier, ObjectClassesUseDistinctCounters)
{
    UniqueIdHelper<CounterTestObjectClass>      Helper0;
    UniqueIdHelper<OtherCounterTestObjectClass> Helper1;

    EXPECT_EQ(Helper0.GetID(), 1);
    EXPECT_EQ(Helper1.GetID(), 1);
}
