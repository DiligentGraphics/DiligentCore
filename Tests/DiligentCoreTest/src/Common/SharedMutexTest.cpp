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

#include "SharedMutex.hpp"
#include "ThreadSignal.hpp"
#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#if !(defined(__MINGW32__) || defined(__MINGW64__))
static_assert(Threading::SharedMutexSupportsConcurrentReaders, "SharedMutex is expected to support concurrent readers on non-MinGW platforms.");
#endif

namespace
{

class ThreadStartGate
{
public:
    explicit ThreadStartGate(uint32_t ThreadCount) :
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

bool WaitUntilEquals(const std::atomic<uint32_t>& Value, uint32_t Expected)
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

void UpdateMax(std::atomic<uint32_t>& MaxValue, uint32_t Value)
{
    uint32_t PrevMax = MaxValue.load(std::memory_order_acquire);
    while (PrevMax < Value &&
           !MaxValue.compare_exchange_weak(PrevMax, Value, std::memory_order_acq_rel, std::memory_order_acquire))
    {
    }
}

} // namespace

TEST(Common_SharedMutex, UniqueLockSerializesWriters)
{
    static constexpr uint32_t ThreadCount    = 16;
    static constexpr uint32_t IterationCount = 128;

    Threading::SharedMutex   Mutex;
    ThreadStartGate          StartGate{ThreadCount};
    std::atomic<uint32_t>    ActiveWriters{0};
    std::atomic<uint32_t>    MaxActiveWriters{0};
    std::atomic<uint32_t>    EnterCount{0};
    std::vector<std::thread> Threads;

    Threads.reserve(ThreadCount);
    for (uint32_t ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back([&]() {
            StartGate.Wait();

            for (uint32_t Iteration = 0; Iteration < IterationCount; ++Iteration)
            {
                std::unique_lock<Threading::SharedMutex> Lock{Mutex};

                const uint32_t CurrentActiveWriters = ActiveWriters.fetch_add(1, std::memory_order_acq_rel) + 1;
                UpdateMax(MaxActiveWriters, CurrentActiveWriters);
                std::this_thread::yield();
                ActiveWriters.fetch_sub(1, std::memory_order_acq_rel);
                EnterCount.fetch_add(1, std::memory_order_acq_rel);
            }
        });
    }

    for (std::thread& Thread : Threads)
        Thread.join();

    EXPECT_EQ(EnterCount.load(std::memory_order_acquire), ThreadCount * IterationCount);
    EXPECT_EQ(ActiveWriters.load(std::memory_order_acquire), 0u);
    EXPECT_EQ(MaxActiveWriters.load(std::memory_order_acquire), 1u);
}

TEST(Common_SharedMutex, SharedLockExcludesUniqueLock)
{
    Threading::SharedMutex Mutex;

    std::atomic<uint32_t> ActiveReaders{0};
    std::atomic<bool>     ReleaseReader{false};

    std::thread ReaderThread{[&]() {
        std::shared_lock<Threading::SharedMutex> Lock{Mutex};

        ActiveReaders.fetch_add(1, std::memory_order_release);

        while (!ReleaseReader.load(std::memory_order_acquire))
            std::this_thread::yield();

        ActiveReaders.fetch_sub(1, std::memory_order_release);
    }};

    const bool ReaderAcquiredLock = WaitUntilEquals(ActiveReaders, 1);

    if (ReaderAcquiredLock)
    {
        std::unique_lock<Threading::SharedMutex> WriteLock{Mutex, std::try_to_lock};
        EXPECT_FALSE(WriteLock.owns_lock());
    }

    ReleaseReader.store(true, std::memory_order_release);
    ReaderThread.join();

    ASSERT_TRUE(ReaderAcquiredLock);

    {
        std::unique_lock<Threading::SharedMutex> WriteLock{Mutex, std::try_to_lock};
        EXPECT_TRUE(WriteLock.owns_lock());
    }
}

TEST(Common_SharedMutex, UniqueLockExcludesSharedLock)
{
    Threading::SharedMutex Mutex;

    std::unique_lock<Threading::SharedMutex> WriteLock{Mutex};

    std::atomic<bool> TryFinished{false};
    std::atomic<bool> ReadLockAcquired{false};

    std::thread ReaderThread{[&]() {
        std::shared_lock<Threading::SharedMutex> ReadLock{Mutex, std::try_to_lock};
        ReadLockAcquired.store(ReadLock.owns_lock(), std::memory_order_release);
        TryFinished.store(true, std::memory_order_release);
    }};

    ReaderThread.join();

    EXPECT_TRUE(TryFinished.load(std::memory_order_acquire));
    EXPECT_FALSE(ReadLockAcquired.load(std::memory_order_acquire));

    WriteLock.unlock();

    std::shared_lock<Threading::SharedMutex> ReadLock{Mutex, std::try_to_lock};
    EXPECT_TRUE(ReadLock.owns_lock());
}

TEST(Common_SharedMutex, SharedLockAllowsParallelReaders)
{
    if (!Threading::SharedMutexSupportsConcurrentReaders)
        GTEST_SKIP() << "SharedMutex uses exclusive fallback on this platform.";

    static constexpr uint32_t ThreadCount = 8;

    Threading::SharedMutex   Mutex;
    ThreadStartGate          StartGate{ThreadCount};
    std::atomic<uint32_t>    ActiveReaders{0};
    std::atomic<uint32_t>    MaxActiveReaders{0};
    std::atomic<bool>        ReleaseReaders{false};
    std::vector<std::thread> Threads;

    Threads.reserve(ThreadCount);
    for (uint32_t ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        Threads.emplace_back([&]() {
            StartGate.Wait();

            {
                std::shared_lock<Threading::SharedMutex> Lock{Mutex};

                const uint32_t CurrentActiveReaders = ActiveReaders.fetch_add(1, std::memory_order_acq_rel) + 1;
                UpdateMax(MaxActiveReaders, CurrentActiveReaders);

                while (!ReleaseReaders.load(std::memory_order_acquire))
                    std::this_thread::yield();

                ActiveReaders.fetch_sub(1, std::memory_order_acq_rel);
            }
        });
    }

    const bool AllReadersAcquiredLock = WaitUntilEquals(ActiveReaders, ThreadCount);
    ReleaseReaders.store(true, std::memory_order_release);

    for (std::thread& Thread : Threads)
        Thread.join();

    ASSERT_TRUE(AllReadersAcquiredLock);
    EXPECT_EQ(ActiveReaders.load(std::memory_order_acquire), 0u);
    EXPECT_EQ(MaxActiveReaders.load(std::memory_order_acquire), ThreadCount);
}
