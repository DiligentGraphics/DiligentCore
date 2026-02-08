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

#include "GPUUploadManager.h"
#include "../include/GPUUploadManagerImpl.hpp"
#include "ThreadSignal.hpp"
#include "CallbackWrapper.hpp"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <array>

using namespace Diligent;

namespace
{

static const size_t kNumThreads = std::max(4u, std::thread::hardware_concurrency());

TEST(GPUUploadManagerPageTest, States)
{
    {
        GPUUploadManagerImpl::Page Page{0};
        EXPECT_TRUE(Page.TryBeginWriting()) << "Should be able to begin writing to a new page";
        EXPECT_TRUE(Page.EndWriting() == GPUUploadManagerImpl::Page::WritingStatus::NotSealed) << "Page should not be sealed after the first writer finishes";
    }

    {
        GPUUploadManagerImpl::Page Page{0};
        EXPECT_EQ(Page.TrySeal(), GPUUploadManagerImpl::Page::SealStatus::Ready) << "Page with no active writers should be sealed immediately";
        EXPECT_EQ(Page.TrySeal(), GPUUploadManagerImpl::Page::SealStatus::AlreadySealed) << "Sealing an already sealed page should return AlreadySealed";
        EXPECT_FALSE(Page.TryBeginWriting()) << "Should not be able to begin writing to a sealed page";

        Page.Reset(nullptr);
        EXPECT_TRUE(Page.TryBeginWriting()) << "Should be able to begin writing after resetting the page";
        EXPECT_EQ(Page.EndWriting(), GPUUploadManagerImpl::Page::WritingStatus::NotSealed) << "Page should not be sealed after the first writer finishes";
        EXPECT_EQ(Page.TrySeal(), GPUUploadManagerImpl::Page::SealStatus::Ready) << "Page with no active writers should be sealed immediately";
    }

    {
        GPUUploadManagerImpl::Page Page{0};
        EXPECT_TRUE(Page.TryBeginWriting()) << "Should be able to begin writing to a new page";
        EXPECT_TRUE(Page.TryBeginWriting());
        EXPECT_EQ(Page.TrySeal(), GPUUploadManagerImpl::Page::SealStatus::NotReady) << "Page with active writers should not be ready immediately after sealing";
        EXPECT_TRUE(Page.EndWriting() == GPUUploadManagerImpl::Page::WritingStatus::NotLastWriter) << "Page should not be sealed after the first writer finishes";
        EXPECT_TRUE(Page.EndWriting() == GPUUploadManagerImpl::Page::WritingStatus::LastWriterSealed) << "Page should be sealed after the last writer finishes";
    }

    {
        GPUUploadManagerImpl::Page Page{1024};
        EXPECT_TRUE(Page.TryBeginWriting()) << "Should be able to begin writing to a new page";
        EXPECT_TRUE(Page.ScheduleBufferUpdate(nullptr, 0, 512, nullptr, nullptr, nullptr));
        EXPECT_TRUE(Page.ScheduleBufferUpdate(nullptr, 512, 512, nullptr, nullptr, nullptr));
        EXPECT_FALSE(Page.ScheduleBufferUpdate(nullptr, 1024, 512, nullptr, nullptr, nullptr)) << "Should not be able to schedule an update that exceeds the page size";
        EXPECT_EQ(Page.GetNumPendingOps(), size_t{2});
        EXPECT_TRUE(Page.EndWriting() == GPUUploadManagerImpl::Page::WritingStatus::NotSealed) << "Page should not be sealed after the first writer finishes";
        EXPECT_EQ(Page.TrySeal(), GPUUploadManagerImpl::Page::SealStatus::Ready) << "Page with no active writers should be sealed immediately";
        EXPECT_EQ(Page.GetNumPendingOps(), size_t{2});
        Page.ExecutePendingOps(nullptr, 0);
        EXPECT_EQ(Page.GetNumPendingOps(), size_t{0});
    }

    {
        GPUUploadManagerImpl::Page Page{1024};
        EXPECT_TRUE(Page.TryBeginWriting()) << "Should be able to begin writing to a new page";
        EXPECT_FALSE(Page.ScheduleBufferUpdate(nullptr, 0, 4096, nullptr, nullptr, nullptr)) << "Should not be able to schedule an update that exceeds the page size";
        EXPECT_FALSE(Page.ScheduleBufferUpdate(nullptr, 0, 128, nullptr, nullptr, nullptr)) << "Should not be able to schedule an update since the offset should be past the page size";
        EXPECT_EQ(Page.EndWriting(), GPUUploadManagerImpl::Page::WritingStatus::NotSealed) << "Page should not be sealed after the first writer finishes";
    }
}

TEST(GPUUploadManagerPageTest, ParallelTryBeginWriting)
{
    GPUUploadManagerImpl::Page Page{0};

    Threading::Signal StartSignal;

    std::vector<std::thread> threads;

    static constexpr size_t kNumIterations = 1000;

    std::atomic<Uint32> NumPagesAlreadySealed{0};
    std::atomic<Uint32> NumLastWriters{0};
    std::atomic<Uint32> TotalWrites{0};
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        threads.emplace_back([&]() {
            StartSignal.Wait(true, static_cast<int>(kNumThreads));

            Uint32 NumWrites = 0;

            bool IsSealed = false;
            for (size_t i = 0; i < kNumIterations; ++i)
            {
                bool WriteStarted = Page.TryBeginWriting();
                if (WriteStarted)
                    ++NumWrites;
                else
                    IsSealed = true;

                if (IsSealed)
                    EXPECT_FALSE(WriteStarted) << "No writes can be started after the page is sealed";
            }

            if (Page.TrySeal() == GPUUploadManagerImpl::Page::SealStatus::AlreadySealed)
                NumPagesAlreadySealed.fetch_add(1);

            for (size_t i = 0; i < kNumIterations; ++i)
            {
                EXPECT_FALSE(Page.TryBeginWriting()) << "No writes can be started after the page is sealed";
            }

            for (size_t i = 0; i < NumWrites; ++i)
            {
                if (Page.EndWriting() == GPUUploadManagerImpl::Page::WritingStatus::LastWriterSealed)
                    NumLastWriters.fetch_add(1);
            }

            TotalWrites.fetch_add(NumWrites);
        });
    }

    StartSignal.Trigger(true);

    for (auto& thread : threads)
        thread.join();

    LOG_INFO_MESSAGE("Total writes: ", TotalWrites.load(), " out of ", kNumThreads * kNumIterations);

    EXPECT_EQ(NumPagesAlreadySealed.load(), kNumThreads - 1) << "Only one thread should be able to seal the page";
    EXPECT_EQ(NumLastWriters.load(), 1) << "Only one thread should be the last writer";
}


TEST(GPUUploadManagerPageTest, NoWritesAfterSeal)
{
    GPUUploadManagerImpl::Page Page{0};

    Threading::Signal StartSignal;

    std::vector<std::thread> threads;

    static constexpr size_t kNumIterations = 1000;

    std::atomic<bool> IsSealed{false};
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        threads.emplace_back(
            [&](size_t ThreadId) {
                StartSignal.Wait(true, static_cast<int>(kNumThreads));
                if (ThreadId == 0)
                {
                    Page.TrySeal();
                    IsSealed.store(true);
                }
                else
                {
                    for (size_t i = 0; i < kNumIterations; ++i)
                    {
                        if (IsSealed.load())
                        {
                            EXPECT_FALSE(Page.TryBeginWriting()) << "No writes can be started after the page is sealed";
                        }
                        else
                        {
                            Page.TryBeginWriting();
                        }
                    }
                }
            },
            t);
    }

    StartSignal.Trigger(true);

    for (auto& thread : threads)
        thread.join();
}


TEST(GPUUploadManagerPageTest, ScheduleBufferUpdateParallel)
{
    constexpr Uint32 kPageSize   = 16384;
    constexpr Uint32 kUpdateSize = 32;
    constexpr Uint32 kNumUpdates = kPageSize / kUpdateSize;

    GPUUploadManagerImpl::Page Page{kPageSize};

    Threading::Signal StartSignal;

    std::vector<std::thread> threads;

    static constexpr size_t kNumIterations = kNumUpdates;

    Uint32 NumUpdatesScheduled = 0;
    auto   Callback            = MakeCallback([&NumUpdatesScheduled] {
        NumUpdatesScheduled++;
    });

    std::atomic<size_t> UpdatesScheduled{0};
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        threads.emplace_back(
            [&](size_t ThreadId) {
                StartSignal.Wait(true, static_cast<int>(kNumThreads));
                if (Page.TryBeginWriting())
                {
                    for (size_t i = 0; i < kNumIterations; ++i)
                    {
                        if (Page.ScheduleBufferUpdate(nullptr, 0, kUpdateSize, nullptr, Callback, Callback))
                        {
                            UpdatesScheduled.fetch_add(1);
                        }
                    }
                    Page.EndWriting();
                }
            },
            t);
    }

    StartSignal.Trigger(true);

    for (auto& thread : threads)
        thread.join();

    EXPECT_EQ(Page.GetNumPendingOps(), kNumUpdates);
    EXPECT_EQ(UpdatesScheduled.load(), kNumUpdates) << "Should be able to schedule updates until the page size is reached";
    EXPECT_EQ(Page.TrySeal() == GPUUploadManagerImpl::Page::SealStatus::Ready, true) << "Page should be ready for sealing after all updates are scheduled";
    Page.ExecutePendingOps(nullptr, 0);
    EXPECT_EQ(NumUpdatesScheduled, kNumUpdates) << "All scheduled updates should have been executed";
}

TEST(GPUUploadManagerPageTest, TryEnqueueParallel)
{
    GPUUploadManagerImpl::Page Page{0};

    Threading::Signal StartSignal;

    std::vector<std::thread> threads;

    EXPECT_EQ(Page.TrySeal(), GPUUploadManagerImpl::Page::SealStatus::Ready) << "Page with no active writers should be sealed immediately";

    std::atomic<size_t> NumEnqueued{0};
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        threads.emplace_back(
            [&]() {
                StartSignal.Wait(true, static_cast<int>(kNumThreads));
                if (Page.TryEnqueue())
                {
                    NumEnqueued.fetch_add(1);
                }
            });
    }

    StartSignal.Trigger(true);

    for (auto& thread : threads)
        thread.join();

    EXPECT_EQ(NumEnqueued.load(), size_t{1}) << "Only one thread should be able to enqueue the page";
}

} // namespace
