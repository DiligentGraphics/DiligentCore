/*
 *  Copyright 2019-2024 Diligent Graphics LLC
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
#include <cmath>

#include "ThreadSignal.hpp"


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

class WaitTask : public AsyncTaskBase
{
public:
    WaitTask(IReferenceCounters* pRefCounters,
             Threading::Signal&  WaitSignal) :
        AsyncTaskBase{pRefCounters},
        m_WaitSignal{WaitSignal}
    {}

    virtual void DILIGENT_CALL_TYPE Run(Uint32 ThreadId) override final
    {
        m_WaitSignal.Wait();
        SetStatus(ASYNC_TASK_STATUS_COMPLETE);
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

    virtual void DILIGENT_CALL_TYPE Run(Uint32 ThreadId) override final
    {
        SetStatus(ASYNC_TASK_STATUS_COMPLETE);
    }
};

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

} // namespace
