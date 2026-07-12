/*
 *  Copyright 2019-2026 Diligent Graphics LLC
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

#include "ThreadPool.hpp"

#include "gtest/gtest.h"

#include <array>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "ThreadSignal.hpp"
#include "TestingEnvironment.hpp"


using namespace Diligent;

namespace
{

TEST(Common_ThreadPool, EnqueueTask)
{
    constexpr Uint32     NumThreads = 4;
    constexpr Uint32     NumTasks   = 32;
    ThreadPoolCreateInfo PoolCI{NumThreads};

    std::array<std::atomic<bool>, NumThreads> ThreadStarted{};

    std::atomic<size_t> NumThreadsFinished{0};
    PoolCI.OnThreadStarted = [&ThreadStarted](Uint32 ThreadId) {
        ThreadStarted[ThreadId].store(true);
    };
    PoolCI.OnThreadExiting = [&NumThreadsFinished](Uint32 ThreadId) {
        NumThreadsFinished.fetch_add(1);
    };

    auto pThreadPool = CreateThreadPool(PoolCI);
    ASSERT_NE(pThreadPool, nullptr);

    std::array<std::atomic<float>, NumTasks>        Results{};
    std::array<std::atomic<bool>, NumTasks>         WorkComplete{};
    std::array<RefCntAutoPtr<IAsyncTask>, NumTasks> Tasks{};
    for (size_t i = 0; i < NumTasks; ++i)
    {
        Tasks[i] =
            EnqueueAsyncWork(pThreadPool,
                             [i, &Results, &ThreadStarted, &WorkComplete](Uint32 ThreadId) //
                             {
                                 constexpr size_t NumIterations = 4096;

                                 EXPECT_TRUE(ThreadStarted[ThreadId]);
                                 float f = 0.5;
                                 for (size_t k = 0; k < NumIterations; ++k)
                                     f = std::sin(f + 1.f);
                                 Results[i].store(f);
                                 WorkComplete[i].store(true);

                                 return ASYNC_TASK_STATUS_COMPLETE;
                             });
    }

    pThreadPool->WaitForAllTasks();

    EXPECT_EQ(pThreadPool->GetQueueSize(), 0u);
    EXPECT_EQ(pThreadPool->GetRunningTaskCount(), 0u);

    for (size_t i = 0; i < NumTasks; ++i)
    {
        EXPECT_TRUE(Tasks[i]->IsFinished()) << "i=" << i;
        EXPECT_EQ(Tasks[i]->GetStatus(), ASYNC_TASK_STATUS_COMPLETE) << "i=" << i;
        EXPECT_TRUE(WorkComplete[i]) << "i=" << i;
        EXPECT_NE(Results[i], 0.f);
    }

    // Check that multiple calls to WaitForAllTasks work fine
    pThreadPool->WaitForAllTasks();

    pThreadPool.Release();
    EXPECT_EQ(NumThreadsFinished.load(), PoolCI.NumThreads);
}

TEST(Common_ThreadPool, EnqueueTaskAfterStopCancelsTask)
{
    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{0});
    ASSERT_NE(pThreadPool, nullptr);

    pThreadPool->StopThreads();

    std::atomic<Uint32> NumRuns{0};
    auto                pTask =
        CreateAsyncWorkTask(
            [&NumRuns](Uint32) //
            {
                NumRuns.fetch_add(1);
                return ASYNC_TASK_STATUS_COMPLETE;
            });

    // Expected behavior: enqueueing after StopThreads() is an error; the task
    // is cancelled and is not placed into the queue.
    {
        Testing::TestingEnvironment::ErrorScope ExpectedErrors{"Enqueue on a stopped ThreadPool"};
        EXPECT_FALSE(pThreadPool->EnqueueTask(pTask));
    }

    EXPECT_EQ(pTask->GetStatus(), ASYNC_TASK_STATUS_CANCELLED);
    EXPECT_TRUE(pTask->IsFinished());
    EXPECT_EQ(pThreadPool->GetQueueSize(), 0u);
    EXPECT_EQ(pThreadPool->GetRunningTaskCount(), 0u);
    EXPECT_EQ(NumRuns.load(), 0u);

    pTask->WaitForCompletion();
}


TEST(Common_ThreadPool, ProcessTask)
{
    constexpr Uint32 NumThreads = 4;
    constexpr Uint32 NumTasks   = 32;

    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{0});
    ASSERT_NE(pThreadPool, nullptr);

    std::vector<std::thread> WorkerThreads(NumThreads);
    for (Uint32 i = 0; i < NumThreads; ++i)
    {
        WorkerThreads[i] = std::thread{
            [&ThreadPool = *pThreadPool, i] //
            {
                while (ThreadPool.ProcessTask(i, true))
                {
                }
            }};
    }

    std::array<std::atomic<float>, NumTasks> Results{};
    std::array<std::atomic<bool>, NumTasks>  WorkComplete{};
    for (size_t i = 0; i < Results.size(); ++i)
    {
        EnqueueAsyncWork(pThreadPool,
                         [i, &Results, &WorkComplete](Uint32 ThreadId) //
                         {
                             constexpr size_t NumIterations = 4096;

                             float f = 0.5;
                             for (size_t k = 0; k < NumIterations; ++k)
                                 f = std::sin(f + 1.f);
                             Results[i].store(f);
                             WorkComplete[i].store(true);

                             return ASYNC_TASK_STATUS_COMPLETE;
                         });
    }

    pThreadPool->WaitForAllTasks();

    EXPECT_EQ(pThreadPool->GetQueueSize(), 0u);
    EXPECT_EQ(pThreadPool->GetRunningTaskCount(), 0u);

    for (size_t i = 0; i < WorkComplete.size(); ++i)
    {
        EXPECT_TRUE(WorkComplete[i]) << "i=" << i;
        EXPECT_NE(Results[i], 0.f);
    }

    // Check that multiple calls to WaitForAllTasks work fine
    pThreadPool->WaitForAllTasks();

    // We must stop all threads
    pThreadPool->StopThreads();

    // Cleanup (must be done after the pool is destroyed)
    for (auto& Thread : WorkerThreads)
    {
        Thread.join();
    }
}

TEST(Common_ThreadPool, ProcessTaskNoWaitReturnsTrueWhilePoolIsRunning)
{
    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{0});
    ASSERT_NE(pThreadPool, nullptr);

    EXPECT_TRUE(pThreadPool->ProcessTask(0, false));

    std::atomic<Uint32> NumRuns{0};
    EnqueueAsyncWork(pThreadPool,
                     [&NumRuns](Uint32 ThreadId) //
                     {
                         NumRuns.fetch_add(1);
                         return ASYNC_TASK_STATUS_COMPLETE;
                     });

    EXPECT_TRUE(pThreadPool->ProcessTask(0, false));
    EXPECT_EQ(NumRuns.load(), 1u);
    EXPECT_EQ(pThreadPool->GetQueueSize(), 0u);
    EXPECT_EQ(pThreadPool->GetRunningTaskCount(), 0u);
    EXPECT_TRUE(pThreadPool->ProcessTask(0, false));

    pThreadPool->StopThreads();
    EXPECT_FALSE(pThreadPool->ProcessTask(0, false));
}

TEST(Common_ThreadPool, ProcessTaskCancelsTaskOnException)
{
    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{0});
    ASSERT_NE(pThreadPool, nullptr);

    auto pTask =
        EnqueueAsyncWork(
            pThreadPool,
            [](Uint32) -> ASYNC_TASK_STATUS //
            {
                throw std::runtime_error{"test exception"};
            });

    // Expected behavior: task exceptions are contained by ProcessTask(), the
    // task is cancelled, and the running-task counter is decremented.
    {
        Testing::TestingEnvironment::ErrorScope ExpectedErrors{"Unhandled exception in asynchronous task"};
        EXPECT_NO_THROW(EXPECT_TRUE(pThreadPool->ProcessTask(0, false)));
    }
    EXPECT_EQ(pTask->GetStatus(), ASYNC_TASK_STATUS_CANCELLED);
    EXPECT_TRUE(pTask->IsFinished());
    EXPECT_EQ(pThreadPool->GetQueueSize(), 0u);
    EXPECT_EQ(pThreadPool->GetRunningTaskCount(), 0u);

    pThreadPool->WaitForAllTasks();
}

TEST(Common_ThreadPool, ProcessTaskCancelsTaskOnInvalidReturnStatus)
{
    const ASYNC_TASK_STATUS InvalidStatuses[] =
        {
            ASYNC_TASK_STATUS_UNKNOWN,
            ASYNC_TASK_STATUS_RUNNING,
        };

    for (auto InvalidStatus : InvalidStatuses)
    {
        auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{0});
        ASSERT_NE(pThreadPool, nullptr);

        auto pTask =
            EnqueueAsyncWork(
                pThreadPool,
                [InvalidStatus](Uint32) //
                {
                    return InvalidStatus;
                });

        // Expected behavior: invalid Run() return statuses are rejected in
        // release builds too, so the task is cancelled and not requeued.
        {
            Testing::TestingEnvironment::ErrorScope ExpectedErrors{"Invalid async task return status"};
            EXPECT_TRUE(pThreadPool->ProcessTask(0, false));
        }

        EXPECT_EQ(pTask->GetStatus(), ASYNC_TASK_STATUS_CANCELLED);
        EXPECT_TRUE(pTask->IsFinished());
        EXPECT_EQ(pThreadPool->GetQueueSize(), 0u);
        EXPECT_EQ(pThreadPool->GetRunningTaskCount(), 0u);

        pThreadPool->WaitForAllTasks();
    }
}

class WaitTask : public AsyncTaskBase
{
public:
    WaitTask(IReferenceCounters* pRefCounters,
             Threading::Signal&  WaitSignal) :
        AsyncTaskBase{pRefCounters},
        m_WaitSignal{WaitSignal}
    {}

    virtual ASYNC_TASK_STATUS DILIGENT_CALL_TYPE Run(Uint32 ThreadId) override final
    {
        m_WaitSignal.Wait();
        return ASYNC_TASK_STATUS_COMPLETE;
    }

private:
    Threading::Signal& m_WaitSignal;
};

class DummyTask : public AsyncTaskBase
{
public:
    DummyTask(IReferenceCounters* pRefCounters,
              float               fPriority = 0) :
        AsyncTaskBase{pRefCounters, fPriority}
    {}

    virtual ASYNC_TASK_STATUS DILIGENT_CALL_TYPE Run(Uint32 ThreadId) override final
    {
        return ASYNC_TASK_STATUS_COMPLETE;
    }
};

bool WaitForFutures(std::vector<std::future<void>>& Futures,
                    std::chrono::milliseconds       Timeout)
{
    const auto EndTime = std::chrono::steady_clock::now() + Timeout;
    for (auto& Future : Futures)
    {
        const auto Now = std::chrono::steady_clock::now();
        if (Now >= EndTime || Future.wait_for(EndTime - Now) != std::future_status::ready)
            return false;
    }
    return true;
}

template <typename WaitFuncType, typename SignalFuncType>
void TestAsyncTaskWaitNotifiesAllWaiters(WaitFuncType&&   WaitFunc,
                                         SignalFuncType&& SignalFunc,
                                         const char*      WaiterName,
                                         const char*      FailureMessage)
{
    constexpr Uint32 NumWaiters = 4;

    RefCntAutoPtr<DummyTask> pTask;
    pTask = MakeNewRCObj<DummyTask>()();

    std::vector<std::thread>       Waiters;
    std::vector<std::future<void>> WaiterStarted;
    std::vector<std::future<void>> WaiterFinished;
    Waiters.reserve(NumWaiters);
    WaiterStarted.reserve(NumWaiters);
    WaiterFinished.reserve(NumWaiters);

    for (Uint32 i = 0; i < NumWaiters; ++i)
    {
        auto pStarted  = std::make_shared<std::promise<void>>();
        auto pFinished = std::make_shared<std::promise<void>>();

        WaiterStarted.emplace_back(pStarted->get_future());
        WaiterFinished.emplace_back(pFinished->get_future());
        Waiters.emplace_back(
            [pTask, pStarted, pFinished, WaitFunc]() //
            {
                pStarted->set_value();
                WaitFunc(pTask);
                pFinished->set_value();
            });
    }

    const auto Cleanup = [&]() {
        pTask->SetStatus(ASYNC_TASK_STATUS_CANCELLED);
        if (WaitForFutures(WaiterFinished, std::chrono::seconds{5}))
        {
            for (auto& Waiter : Waiters)
                Waiter.join();
        }
        else
        {
            for (auto& Waiter : Waiters)
                Waiter.detach();
        }
    };

    if (!WaitForFutures(WaiterStarted, std::chrono::seconds{5}))
    {
        ADD_FAILURE() << WaiterName << " waiter did not start";
        Cleanup();
        return;
    }

    for (auto& Future : WaiterFinished)
        EXPECT_EQ(Future.wait_for(std::chrono::milliseconds{0}), std::future_status::timeout);

    SignalFunc(pTask);

    if (!WaitForFutures(WaiterFinished, std::chrono::seconds{5}))
    {
        ADD_FAILURE() << FailureMessage;
        Cleanup();
        return;
    }

    for (auto& Waiter : Waiters)
        Waiter.join();

    if (pTask->GetStatus() == ASYNC_TASK_STATUS_RUNNING)
        pTask->SetStatus(ASYNC_TASK_STATUS_CANCELLED);
}

TEST(Common_ThreadPool, WaitForCompletionNotifiesAllWaiters)
{
    // Expected behavior: all WaitForCompletion() callers block while the task
    // is not finished, then wake when the task becomes cancelled or complete.
    TestAsyncTaskWaitNotifiesAllWaiters(
        [](IAsyncTask* pTask) //
        {
            pTask->WaitForCompletion();
        },
        [](IAsyncTask* pTask) //
        {
            pTask->SetStatus(ASYNC_TASK_STATUS_CANCELLED);
        },
        "WaitForCompletion",
        "WaitForCompletion did not wake all waiters when the task finished");
}

TEST(Common_ThreadPool, WaitUntilRunningNotifiesAllWaiters)
{
    // Expected behavior: all WaitUntilRunning() callers block while the task is
    // not started, then wake when the task enters the running state.
    TestAsyncTaskWaitNotifiesAllWaiters(
        [](IAsyncTask* pTask) //
        {
            pTask->WaitUntilRunning();
        },
        [](IAsyncTask* pTask) //
        {
            pTask->SetStatus(ASYNC_TASK_STATUS_RUNNING);
        },
        "WaitUntilRunning",
        "WaitUntilRunning did not wake all waiters when the task started");
}

TEST(Common_ThreadPool, WaitUntilRunningNotifiesAllWaitersWhenCancelled)
{
    // Expected behavior: WaitUntilRunning() also returns when a task is
    // cancelled before it starts, because it can no longer become running.
    TestAsyncTaskWaitNotifiesAllWaiters(
        [](IAsyncTask* pTask) //
        {
            pTask->WaitUntilRunning();
        },
        [](IAsyncTask* pTask) //
        {
            pTask->SetStatus(ASYNC_TASK_STATUS_CANCELLED);
        },
        "WaitUntilRunning",
        "WaitUntilRunning did not wake all waiters when the task was cancelled");
}

TEST(Common_ThreadPool, NaNTaskPriorityUsesDefault)
{
    const float NaN = std::numeric_limits<float>::quiet_NaN();

    // Expected behavior: NaN priority supplied at construction is rejected
    // because NaN cannot be used as a strict weak ordering key in the queue.
    {
        Testing::TestingEnvironment::ErrorScope ExpectedErrors{"Task priority must not be NaN"};
        RefCntAutoPtr<DummyTask>                pTask;
        pTask = MakeNewRCObj<DummyTask>()(NaN);
        EXPECT_EQ(pTask->GetPriority(), 0.f);
    }

    RefCntAutoPtr<DummyTask> pTask;
    pTask = MakeNewRCObj<DummyTask>()();
    pTask->SetPriority(10.f);
    EXPECT_EQ(pTask->GetPriority(), 10.f);

    // Expected behavior: NaN priority supplied through SetPriority() is
    // rejected and the task falls back to the default priority.
    {
        Testing::TestingEnvironment::ErrorScope ExpectedErrors{"Task priority must not be NaN"};
        pTask->SetPriority(NaN);
    }
    EXPECT_EQ(pTask->GetPriority(), 0.f);
}

TEST(Common_ThreadPool, RemoveTask)
{
    constexpr Uint32 NumThreads = 4;

    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{NumThreads});
    ASSERT_NE(pThreadPool, nullptr);

    Threading::Signal Signal;

    std::array<RefCntAutoPtr<WaitTask>, NumThreads> WaitTasks;
    for (auto& Task : WaitTasks)
    {
        Task = MakeNewRCObj<WaitTask>()(Signal);
        pThreadPool->EnqueueTask(Task);
    }

    std::array<RefCntAutoPtr<DummyTask>, 16> DummyTasks;
    for (auto& Task : DummyTasks)
    {
        Task = MakeNewRCObj<DummyTask>()();
        pThreadPool->EnqueueTask(Task);
    }

    EXPECT_GE(pThreadPool->GetQueueSize(), DummyTasks.size());
    // Dummy tasks can't start since all threads are waiting for the signal
    for (auto& Task : DummyTasks)
    {
        auto res = pThreadPool->RemoveTask(Task);
        EXPECT_TRUE(res);
    }

    // Wait until tasks are started
    for (auto& Task : WaitTasks)
    {
        Task->WaitUntilRunning();
    }

    EXPECT_EQ(pThreadPool->GetQueueSize(), 0u);
    EXPECT_EQ(pThreadPool->GetRunningTaskCount(), 4u);

    for (auto& Task : WaitTasks)
    {
        // The task will not be removed since it is running
        auto res = pThreadPool->RemoveTask(Task);
        EXPECT_FALSE(res);
    }

    Signal.Trigger(true, 1);

    pThreadPool->WaitForAllTasks();
    EXPECT_EQ(pThreadPool->GetQueueSize(), 0u);
}

TEST(Common_ThreadPool, RemoveTaskCancelsQueuedTask)
{
    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{0});
    ASSERT_NE(pThreadPool, nullptr);

    RefCntAutoPtr<DummyTask> pTask;
    pTask = MakeNewRCObj<DummyTask>()();
    pThreadPool->EnqueueTask(pTask);

    EXPECT_TRUE(pThreadPool->RemoveTask(pTask));
    EXPECT_EQ(pTask->GetStatus(), ASYNC_TASK_STATUS_CANCELLED);
    EXPECT_TRUE(pTask->IsFinished());

    auto pWaiterFinished = std::make_shared<std::promise<void>>();
    auto WaiterFinished  = pWaiterFinished->get_future();

    // Expected behavior: a removed queued task is cancelled, so callers that
    // kept a reference can wait for completion without spinning forever.
    std::thread Waiter{
        [pTask, pWaiterFinished]() //
        {
            pTask->WaitForCompletion();
            pWaiterFinished->set_value();
        }};

    if (WaiterFinished.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
    {
        ADD_FAILURE() << "WaitForCompletion did not finish after RemoveTask cancelled the queued task";

        pTask->SetStatus(ASYNC_TASK_STATUS_CANCELLED);
        if (WaiterFinished.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
        {
            Waiter.detach();
            return;
        }
    }

    Waiter.join();
}

TEST(Common_ThreadPool, WaitForAllTasksNotifiedAfterRemoveTask)
{
    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{0});
    ASSERT_NE(pThreadPool, nullptr);

    // With no worker threads, removing this task is the only operation that
    // can make WaitForAllTasks() observe an idle pool.
    auto pTask = MakeNewRCObj<DummyTask>()();
    pThreadPool->EnqueueTask(pTask);

    auto pWaiterStarted  = std::make_shared<std::promise<void>>();
    auto pWaiterFinished = std::make_shared<std::promise<void>>();

    auto WaiterStarted  = pWaiterStarted->get_future();
    auto WaiterFinished = pWaiterFinished->get_future();

    std::thread Waiter{
        [pThreadPool, pWaiterStarted, pWaiterFinished]() //
        {
            pWaiterStarted->set_value();
            pThreadPool->WaitForAllTasks();
            pWaiterFinished->set_value();
        }};

    if (WaiterStarted.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
    {
        ADD_FAILURE() << "WaitForAllTasks waiter did not start";
        EXPECT_TRUE(pThreadPool->RemoveTask(pTask));

        auto pCleanupTask = EnqueueAsyncWork(
            pThreadPool,
            [](Uint32 ThreadId) //
            {
                return ASYNC_TASK_STATUS_COMPLETE;
            });
        EXPECT_TRUE(pThreadPool->ProcessTask(0, false));
        pCleanupTask->WaitForCompletion();

        if (WaiterFinished.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
        {
            Waiter.detach();
            return;
        }

        Waiter.join();
        return;
    }

    if (WaiterFinished.wait_for(std::chrono::milliseconds{50}) != std::future_status::timeout)
    {
        ADD_FAILURE() << "WaitForAllTasks returned before the queued task was removed";
        Waiter.join();
        return;
    }

    // Expected behavior: removing the last queued task wakes the waiter because
    // the queue is empty and there are no running tasks.
    EXPECT_TRUE(pThreadPool->RemoveTask(pTask));

    if (WaiterFinished.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
    {
        ADD_FAILURE() << "WaitForAllTasks was not notified after RemoveTask made the pool idle";

        // Give the waiter another completion notification so the test can
        // report the failure and still clean up instead of hanging.
        auto pCleanupTask = EnqueueAsyncWork(
            pThreadPool,
            [](Uint32 ThreadId) //
            {
                return ASYNC_TASK_STATUS_COMPLETE;
            });
        EXPECT_TRUE(pThreadPool->ProcessTask(0, false));
        pCleanupTask->WaitForCompletion();

        if (WaiterFinished.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
        {
            Waiter.detach();
            return;
        }
    }

    Waiter.join();
}

TEST(Common_ThreadPool, WaitForAllTasksNotifiesAllWaiters)
{
    constexpr Uint32 NumWaiters = 4;

    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{1});
    ASSERT_NE(pThreadPool, nullptr);

    Threading::Signal       Signal;
    RefCntAutoPtr<WaitTask> pWaitTask;
    pWaitTask = MakeNewRCObj<WaitTask>()(Signal);
    pThreadPool->EnqueueTask(pWaitTask);
    pWaitTask->WaitUntilRunning();

    // All waiters block on the same running task. When it finishes, the idle
    // state is global, so every WaitForAllTasks() caller must wake.
    std::vector<std::thread>       Waiters;
    std::vector<std::future<void>> WaiterStarted;
    std::vector<std::future<void>> WaiterFinished;
    Waiters.reserve(NumWaiters);
    WaiterStarted.reserve(NumWaiters);
    WaiterFinished.reserve(NumWaiters);

    for (Uint32 i = 0; i < NumWaiters; ++i)
    {
        auto pStarted  = std::make_shared<std::promise<void>>();
        auto pFinished = std::make_shared<std::promise<void>>();

        WaiterStarted.emplace_back(pStarted->get_future());
        WaiterFinished.emplace_back(pFinished->get_future());
        Waiters.emplace_back(
            [pThreadPool, pStarted, pFinished]() //
            {
                pStarted->set_value();
                pThreadPool->WaitForAllTasks();
                pFinished->set_value();
            });
    }

    bool AllWaitersStarted = true;
    for (auto& Future : WaiterStarted)
    {
        if (Future.wait_for(std::chrono::seconds{5}) != std::future_status::ready)
        {
            AllWaitersStarted = false;
            ADD_FAILURE() << "WaitForAllTasks waiter did not start";
        }
    }

    if (!AllWaitersStarted)
    {
        Signal.Trigger(true, 1);

        for (Uint32 i = 0; i < NumWaiters && !WaitForFutures(WaiterFinished, std::chrono::milliseconds{0}); ++i)
        {
            auto pCleanupTask = EnqueueAsyncWork(
                pThreadPool,
                [](Uint32 ThreadId) //
                {
                    return ASYNC_TASK_STATUS_COMPLETE;
                });
            pCleanupTask->WaitForCompletion();
        }

        if (!WaitForFutures(WaiterFinished, std::chrono::seconds{5}))
        {
            for (auto& Waiter : Waiters)
                Waiter.detach();
            return;
        }

        for (auto& Waiter : Waiters)
            Waiter.join();
        return;
    }

    // Give waiter threads a chance to enter WaitForAllTasks() before the task
    // completes. The bounded wait below is the correctness check; this delay
    // makes the old notify_one() behavior more likely to be observed.
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    for (auto& Future : WaiterFinished)
        EXPECT_EQ(Future.wait_for(std::chrono::milliseconds{0}), std::future_status::timeout);

    // Expected behavior: task completion broadcasts to all waiters, not just
    // one of them.
    Signal.Trigger(true, 1);

    if (!WaitForFutures(WaiterFinished, std::chrono::seconds{5}))
    {
        ADD_FAILURE() << "WaitForAllTasks did not notify all waiters when the pool became idle";

        // Under the old notify_one() behavior, extra completions wake remaining
        // waiters so the regression is reported without leaving threads behind.
        for (Uint32 i = 0; i < NumWaiters && !WaitForFutures(WaiterFinished, std::chrono::milliseconds{0}); ++i)
        {
            auto pCleanupTask = EnqueueAsyncWork(
                pThreadPool,
                [](Uint32 ThreadId) //
                {
                    return ASYNC_TASK_STATUS_COMPLETE;
                });
            pCleanupTask->WaitForCompletion();
        }

        if (!WaitForFutures(WaiterFinished, std::chrono::seconds{5}))
        {
            for (auto& Waiter : Waiters)
                Waiter.detach();
            return;
        }
    }

    for (auto& Waiter : Waiters)
        Waiter.join();
}


TEST(Common_ThreadPool, Reprioritize)
{
    constexpr Uint32 NumThreads = 4;

    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{NumThreads});
    ASSERT_NE(pThreadPool, nullptr);

    Threading::Signal Signal;

    std::array<RefCntAutoPtr<WaitTask>, NumThreads> WaitTasks;
    for (auto& Task : WaitTasks)
    {
        Task = MakeNewRCObj<WaitTask>()(Signal);
        pThreadPool->EnqueueTask(Task);
    }

    std::array<RefCntAutoPtr<DummyTask>, 16> DummyTasks;
    for (auto& Task : DummyTasks)
    {
        Task = MakeNewRCObj<DummyTask>()();
        pThreadPool->EnqueueTask(Task);
    }

    EXPECT_GE(pThreadPool->GetQueueSize(), DummyTasks.size());

    // Dummy tasks can't start since all threads are waiting for the signal
    float Priority = 0;
    for (auto& Task : DummyTasks)
    {
        Task->SetPriority(Priority);
        auto res = pThreadPool->ReprioritizeTask(Task);
        EXPECT_TRUE(res);
        Priority += 1.f;
    }

    for (size_t i = 0; i < DummyTasks.size(); i += 2)
    {
        DummyTasks[i]->SetPriority(DummyTasks[i]->GetPriority() * 2.f);
    }

    pThreadPool->ReprioritizeAllTasks();

    Signal.Trigger(true, 1);

    pThreadPool->WaitForAllTasks();
}


TEST(Common_ThreadPool, Priorities)
{
    constexpr Uint32 NumThreads  = 1;
    constexpr Uint32 NumTasks    = 8;
    constexpr Uint32 RepeatCount = 10;

    for (Uint32 k = 0; k < RepeatCount; ++k)
    {
        auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{NumThreads});
        ASSERT_NE(pThreadPool, nullptr);

        Threading::Signal       Signal;
        RefCntAutoPtr<WaitTask> pWaitTask;
        {
            pWaitTask = MakeNewRCObj<WaitTask>()(Signal);
            pThreadPool->EnqueueTask(pWaitTask);
        }

        // Wait until the task is running to make sure that higher-priority tasks don't start first
        pWaitTask->WaitUntilRunning();

        std::vector<int> CompletionOrder;
        CompletionOrder.reserve(NumTasks);
        std::array<RefCntAutoPtr<IAsyncTask>, NumTasks> Tasks;
        for (Uint32 i = 0; i < NumTasks; ++i)
        {
            Tasks[i] =
                EnqueueAsyncWork(pThreadPool,
                                 [&CompletionOrder, i](Uint32 ThreadId) //
                                 {
                                     CompletionOrder.push_back(i);
                                     return ASYNC_TASK_STATUS_COMPLETE;
                                 });
        }

        Tasks[0]->SetPriority(10);
        Tasks[1]->SetPriority(10);
        auto res = pThreadPool->ReprioritizeTask(Tasks[1]);
        EXPECT_TRUE(res);
        res = pThreadPool->ReprioritizeTask(Tasks[0]);
        EXPECT_TRUE(res);

        Tasks[4]->SetPriority(100);
        Tasks[5]->SetPriority(100);
        Tasks[7]->SetPriority(101);
        pThreadPool->ReprioritizeAllTasks();

        // The tasks can't start since the thread is waiting for the signal
        EXPECT_GE(pThreadPool->GetQueueSize(), Tasks.size());
        EXPECT_FALSE(pWaitTask->IsFinished());

        Signal.Trigger(true, 1);

        pThreadPool->WaitForAllTasks();

        const std::vector<int> ExpectedOrder = {7, 4, 5, 1, 0, 2, 3, 6};
        ASSERT_EQ(ExpectedOrder.size(), CompletionOrder.size());
        for (size_t i = 0; i < ExpectedOrder.size(); ++i)
            EXPECT_EQ(ExpectedOrder[i], CompletionOrder[i]) << "i=" << i << " (N=" << k << ")";
    }
}


TEST(Common_ThreadPool, Prerequisites)
{
    for (Uint32 NumThreads : {1, 8})
    {
        auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{NumThreads});
        ASSERT_NE(pThreadPool, nullptr);

        constexpr Uint32               NumTasks = 16;
        std::vector<std::atomic<bool>> TaskComplete(NumTasks);

        std::atomic<Uint32> NumTasksCorrectlyOrdered{0};
        {
            std::vector<IAsyncTask*>               Tasks(NumTasks);
            std::vector<RefCntAutoPtr<IAsyncTask>> spTasks(NumTasks);
            for (Uint32 task = 0; task < NumTasks; ++task)
            {
                spTasks[task] =
                    EnqueueAsyncWork(
                        pThreadPool,
                        // Make the task dependent on all previous tasks
                        task > 0 ? Tasks.data() : nullptr,
                        task > 0 ? task - 1 : 0,
                        [task, &TaskComplete, &NumTasksCorrectlyOrdered](Uint32 ThreadId) //
                        {
                            // Make earlier tasks longer to run
                            std::this_thread::sleep_for(std::chrono::milliseconds(TaskComplete.size() - task));
                            TaskComplete[task].store(true);

                            bool CorrectOrder = true;
                            for (Uint32 i = 0; i + 1 < task; ++i)
                            {
                                if (!TaskComplete[i].load())
                                {
                                    CorrectOrder = false;
                                    break;
                                }
                            }
                            if (CorrectOrder)
                                NumTasksCorrectlyOrdered.fetch_add(1);

                            return ASYNC_TASK_STATUS_COMPLETE;
                        },
                        static_cast<float>(task) // Inverse priority so that the thread pool fixes it
                    );
                Tasks[task] = spTasks[task];
            }
        }
        pThreadPool->WaitForAllTasks();
        EXPECT_EQ(NumTasksCorrectlyOrdered.load(), NumTasks);
    }
}


TEST(Common_ThreadPool, ReRunTasks)
{
    auto pThreadPool = CreateThreadPool(ThreadPoolCreateInfo{4});
    ASSERT_NE(pThreadPool, nullptr);

    constexpr Uint32              NumTasks = 32;
    std::vector<std::atomic<int>> ReRunCounters(NumTasks);

    for (int i = 0; i < static_cast<int>(ReRunCounters.size()); ++i)
        ReRunCounters[i] = 32 + i;

    for (Uint32 task = 0; task < NumTasks; ++task)
    {
        EnqueueAsyncWork(
            pThreadPool,
            [task, &ReRunCounters](Uint32 ThreadId) //
            {
                int ReRunCounter = ReRunCounters[task].fetch_add(-1) - 1;
                return ReRunCounter > 0 ? ASYNC_TASK_STATUS_NOT_STARTED : ASYNC_TASK_STATUS_COMPLETE;
            });
    }

    pThreadPool->WaitForAllTasks();
    for (size_t i = 0; i < ReRunCounters.size(); ++i)
        EXPECT_EQ(ReRunCounters[i], 0) << i;
}

} // namespace
