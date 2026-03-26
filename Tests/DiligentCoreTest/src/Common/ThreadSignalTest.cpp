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

#include "ThreadSignal.hpp"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

using namespace Threading;

namespace
{

using namespace std::chrono_literals;

TEST(Common_Threading, TickSignal_WaitsForNextTickWhenArmed)
{
    TickSignal Sig;

    // Arm: capture current epoch so the next wait must observe a future tick.
    uint64_t E = Sig.CurrentEpoch();

    std::atomic<bool> Started{false};
    std::atomic<bool> Finished{false};
    std::atomic<int>  GotValue{0};

    std::thread T{[&] {
        Started.store(true, std::memory_order_release);
        int V = Sig.WaitNext(E);
        GotValue.store(V, std::memory_order_release);
        Finished.store(true, std::memory_order_release);
    }};

    // Give the thread time to block.
    while (!Started.load(std::memory_order_acquire))
        std::this_thread::yield();

    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(Finished.load(std::memory_order_acquire)) << "Thread should still be waiting before Tick()";

    Sig.Tick(7);

    // Join and verify it woke up with the right payload.
    T.join();
    EXPECT_TRUE(Finished.load(std::memory_order_acquire));
    EXPECT_EQ(GotValue.load(std::memory_order_acquire), 7);
}

TEST(Common_Threading, TickSignal_NoLostWakeupIfTickHappensBeforeWait)
{
    TickSignal Sig;

    uint64_t E = Sig.CurrentEpoch();

    // Tick before we even start the waiter.
    Sig.Tick(11);

    // Wait should not block because epoch != e already.
    int V = Sig.WaitNext(E);
    EXPECT_EQ(V, 11);
}

TEST(Common_Threading, TickSignal_BroadcastWakesAllWaiters)
{
    TickSignal Sig;

    const int NumWaiters = static_cast<int>(std::max(4, static_cast<int>(std::thread::hardware_concurrency())));

    std::atomic<int> WaitersReady{0};

    std::vector<int>         Results(NumWaiters, -1);
    std::vector<std::thread> Threads;
    Threads.reserve(NumWaiters);

    for (int i = 0; i < NumWaiters; ++i)
    {
        Threads.emplace_back([&, i] {
            uint64_t E = Sig.CurrentEpoch();
            WaitersReady.fetch_add(1);
            Results[i] = Sig.WaitNext(E);
        });
    }

    // Wait until all waiters have started and are about to block.
    while (WaitersReady.load() < NumWaiters)
        std::this_thread::yield();

    // Broadcast tick.
    Sig.Tick(3);

    for (auto& t : Threads)
        t.join();

    for (int i = 0; i < NumWaiters; ++i)
        EXPECT_EQ(Results[i], 3) << "Waiter " << i << " did not receive the broadcast value";
}

TEST(Common_Threading, TickSignal_RequestStopWakesAllWaitersAndReturnsZero)
{
    TickSignal Sig;

    const int NumWaiters = static_cast<int>(std::max(4, static_cast<int>(std::thread::hardware_concurrency())));

    std::vector<int>         Results(NumWaiters, -1);
    std::vector<std::thread> Threads;
    std::atomic<int>         WaitersReady{0};
    Threads.reserve(NumWaiters);

    for (int i = 0; i < NumWaiters; ++i)
    {
        Threads.emplace_back([&, i] {
            uint64_t E = Sig.CurrentEpoch();
            WaitersReady.fetch_add(1);
            Results[i] = Sig.WaitNext(E);
        });
    }

    while (WaitersReady.load() < NumWaiters)
        std::this_thread::yield();

    Sig.RequestStop();

    for (auto& t : Threads)
        t.join();

    for (int i = 0; i < NumWaiters; ++i)
        EXPECT_EQ(Results[i], 0) << "Waiter " << i << " should return 0 after stop";
}

TEST(Common_Threading, TickSignal_CoalescesMultipleTicksToLatestValue)
{
    TickSignal Sig;
    uint64_t   E = Sig.CurrentEpoch();

    std::atomic<bool> Started{false};
    std::atomic<int>  Got{0};
    std::atomic<bool> TicksHappened{false};

    std::thread t{[&] {
        Started.store(true, std::memory_order_release);

        while (!TicksHappened.load())
            std::this_thread::yield();

        int v = Sig.WaitNext(E);
        Got.store(v, std::memory_order_release);
    }};

    while (!Started.load(std::memory_order_acquire))
        std::this_thread::yield();

    // Issue multiple ticks while the waiter is "busy".
    for (int i = 1; i <= 99; ++i)
        Sig.Tick(i);

    TicksHappened.store(true, std::memory_order_release);

    t.join();

    EXPECT_EQ(Got.load(std::memory_order_acquire), 99)
        << "If multiple ticks happen before a waiter consumes, it should observe the latest value";
}

TEST(Common_Threading, TickSignal_WaitNextUpdatesSeenEpochSoSecondWaitBlocksUntilNextTick)
{
    TickSignal Sig;

    uint64_t E = Sig.CurrentEpoch();

    // First tick should unblock immediately.
    Sig.Tick(5);
    int v1 = Sig.WaitNext(E);
    EXPECT_EQ(v1, 5);

    // Now we're armed at the latest epoch; without another tick, WaitNext should block.
    std::atomic<bool> Done{false};
    std::thread       t{[&] {
        int v2 = Sig.WaitNext(E);
        EXPECT_EQ(v2, 6);
        Done.store(true, std::memory_order_release);
    }};

    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(Done.load(std::memory_order_acquire)) << "Second WaitNext should block until a new Tick()";

    Sig.Tick(6);
    t.join();
    EXPECT_TRUE(Done.load(std::memory_order_acquire));
}

} // namespace
