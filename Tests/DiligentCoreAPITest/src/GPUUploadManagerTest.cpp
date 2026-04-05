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

#include "GPUTestingEnvironment.hpp"
#include "ThreadSignal.hpp"
#include "CallbackWrapper.hpp"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <array>
#include <cstring>
#include <chrono>

using namespace Diligent;
using namespace Diligent::Testing;

using namespace std::chrono_literals;

namespace
{

void LogUploadManagerStats(IGPUUploadManager* pUploadManager)
{
    GPUUploadManagerStats Stats;
    pUploadManager->GetStats(Stats);
    LOG_INFO_MESSAGE("GPU Upload Manager Stats:\n", GetGPUUploadManagerStatsString(Stats));
}

TEST(GPUUploadManagerTest, Creation)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext};
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);
}

void VerifyBufferContents(IBuffer* pBuffer, const std::vector<Uint8>& ExpectedData)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    BufferDesc Desc     = pBuffer->GetDesc();
    Desc.Name           = "GPUUploadManagerTest readback buffer";
    Desc.Usage          = USAGE_STAGING;
    Desc.CPUAccessFlags = CPU_ACCESS_READ;
    Desc.BindFlags      = BIND_NONE;

    RefCntAutoPtr<IBuffer> pReadbackBuffer;
    pDevice->CreateBuffer(Desc, nullptr, &pReadbackBuffer);
    ASSERT_TRUE(pReadbackBuffer);

    pContext->CopyBuffer(pBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                         pReadbackBuffer, 0, ExpectedData.size(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->WaitForIdle();

    void* pBufferData = nullptr;

    RENDER_DEVICE_TYPE DeviceType = pDevice->GetDeviceInfo().Type;

    MAP_FLAGS MapFlags = (DeviceType == RENDER_DEVICE_TYPE_D3D12 ||
                          DeviceType == RENDER_DEVICE_TYPE_VULKAN ||
                          DeviceType == RENDER_DEVICE_TYPE_WEBGPU ||
                          DeviceType == RENDER_DEVICE_TYPE_METAL) ?
        MAP_FLAG_DO_NOT_WAIT :
        MAP_FLAG_NONE;

    pContext->MapBuffer(pReadbackBuffer, MAP_READ, MapFlags, pBufferData);
    ASSERT_NE(pBufferData, nullptr);
    EXPECT_TRUE(std::memcmp(pBufferData, ExpectedData.data(), ExpectedData.size()) == 0) << "Buffer contents do not match expected data";
    pContext->UnmapBuffer(pReadbackBuffer, MAP_READ);
}


void TestWriterScheduleBufferUpdates(bool UseWriteCallback, bool UseCopyCallback)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    std::vector<Uint8> BufferData(8192);
    for (size_t i = 0; i < BufferData.size(); ++i)
    {
        BufferData[i] = static_cast<Uint8>(i % 256);
    }

    static constexpr Uint32                   kNumUpdates = 10;
    constexpr std::array<Uint32, kNumUpdates> UpdateSizes = {256, 512, 256, 1024, 2048, 1024, 2048, 512, 256, 256};

    BufferDesc Desc;
    Desc.Name      = "GPUUploadManagerTest buffer";
    Desc.Size      = BufferData.size();
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_VERTEX_BUFFER;

    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
    ASSERT_TRUE(pBuffer);

    Uint32 CurrOffset = 0;

    GPUUploadManagerImpl::Page Page{nullptr, pDevice, static_cast<Uint32>(BufferData.size())};
    Page.Reset(pContext);
    Page.Unseal();

    GPUUploadManagerImpl::Page::Writer Writer = Page.TryBeginWriting();
    EXPECT_TRUE(Writer) << "Should be able to begin writing to a new page";

    Uint32 NumWriteDataCallbackCalled = 0;

    auto WriteDataCallback = MakeCallback([&](void* pDstData, Uint32 NumBytes) {
        std::memcpy(pDstData, &BufferData[CurrOffset], NumBytes);
        ++NumWriteDataCallbackCalled;
    });

    Uint32 NumCopyBufferCallbackCalled = 0;
    Uint32 CopyBufferCallbackDstOffset = 0;

    auto CopyBufferCallback = MakeCallback([&](IDeviceContext* pContext,
                                               IBuffer*        pSrcBuffer,
                                               Uint32          SrcOffset,
                                               Uint32          NumBytes) {
        EXPECT_EQ(NumBytes, UpdateSizes[NumCopyBufferCallbackCalled]);
        pContext->CopyBuffer(pSrcBuffer, SrcOffset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                             pBuffer, CopyBufferCallbackDstOffset, NumBytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        CopyBufferCallbackDstOffset += NumBytes;
        ++NumCopyBufferCallbackCalled;
    });

    Uint32 NumUploadEnqueuedCallbackCalled = 0;
    Uint32 UploadEnqueuedCallbackDstOffset = 0;

    auto UploadEnqueuedCallback = MakeCallback(
        [&](IBuffer* pDstBuffer,
            Uint32   DstOffset,
            Uint32   NumBytes) {
            EXPECT_EQ(NumBytes, UpdateSizes[NumUploadEnqueuedCallbackCalled]);
            EXPECT_EQ(DstOffset, UploadEnqueuedCallbackDstOffset);
            UploadEnqueuedCallbackDstOffset += NumBytes;
            ++NumUploadEnqueuedCallbackCalled;
        });

    for (Uint32 NumBytes : UpdateSizes)
    {
        ScheduleBufferUpdateInfo UpdateInfo;
        UpdateInfo.NumBytes = NumBytes;

        if (UseCopyCallback)
        {
            UpdateInfo.CopyBuffer      = CopyBufferCallback;
            UpdateInfo.pCopyBufferData = CopyBufferCallback;
        }
        else
        {
            UpdateInfo.pDstBuffer = pBuffer;
            UpdateInfo.DstOffset  = CurrOffset;
        }

        if (UseWriteCallback)
        {
            UpdateInfo.WriteDataCallback          = WriteDataCallback;
            UpdateInfo.pWriteDataCallbackUserData = WriteDataCallback;
        }
        else
        {
            UpdateInfo.pSrcData = &BufferData[CurrOffset];
        }

        UpdateInfo.UploadEnqueued      = UploadEnqueuedCallback;
        UpdateInfo.pUploadEnqueuedData = UploadEnqueuedCallback;

        EXPECT_TRUE(Writer.ScheduleBufferUpdate(UpdateInfo));

        CurrOffset += NumBytes;
    }
    VERIFY_EXPR(CurrOffset == BufferData.size());

    {
        ScheduleBufferUpdateInfo UpdateInfo;
        UpdateInfo.pDstBuffer = pBuffer;
        UpdateInfo.NumBytes   = 16;
        UpdateInfo.DstOffset  = 0;
        UpdateInfo.pSrcData   = &BufferData[0];
        EXPECT_FALSE(Writer.ScheduleBufferUpdate(UpdateInfo)) << "Should not be able to schedule updates when the page is full";
    }

    EXPECT_EQ(Page.GetNumPendingOps(), UpdateSizes.size());
    EXPECT_TRUE(Writer.EndWriting() == GPUUploadManagerImpl::Page::WritingStatus::NotSealed) << "Page should not be sealed";

    EXPECT_EQ(Page.TrySeal(), GPUUploadManagerImpl::Page::SealStatus::Ready) << "Page with no active writers should be ready immediately";
    EXPECT_EQ(Page.GetNumPendingOps(), UpdateSizes.size());
    Page.ExecutePendingOps(pContext, 1);
    EXPECT_EQ(Page.GetNumPendingOps(), size_t{0});
    EXPECT_EQ(NumUploadEnqueuedCallbackCalled, UseCopyCallback ? 0u : kNumUpdates);
    Page.ReleaseStagingBuffer(pContext);

    EXPECT_EQ(UseWriteCallback ? kNumUpdates : 0u, NumWriteDataCallbackCalled);
    EXPECT_EQ(UseCopyCallback ? kNumUpdates : 0u, NumCopyBufferCallbackCalled);

    VerifyBufferContents(pBuffer, BufferData);
}

TEST(GPUUploadManagerTest, Writer_ScheduleBufferUpdates)
{
    TestWriterScheduleBufferUpdates(/*UseWriteCallback = */ false, /*UseCopyCallback = */ false);
}

TEST(GPUUploadManagerTest, Writer_ScheduleBufferUpdatesWithWriteBufferCallback)
{
    TestWriterScheduleBufferUpdates(/*UseWriteCallback = */ true, /*UseCopyCallback = */ false);
}

TEST(GPUUploadManagerTest, Writer_ScheduleBufferUpdatesWithCopyBufferCallback)
{
    TestWriterScheduleBufferUpdates(/*UseWriteCallback = */ false, /*UseCopyCallback = */ true);
}

TEST(GPUUploadManagerTest, Writer_ScheduleBufferUpdatesWithWriteAndCopyCallbacks)
{
    TestWriterScheduleBufferUpdates(/*UseWriteCallback = */ true, /*UseCopyCallback = */ true);
}

TEST(GPUUploadManagerTest, Writer_ScheduleBufferUpdateParallel)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    constexpr Uint32 kPageSize   = 65536;
    constexpr Uint32 kUpdateSize = 32;
    constexpr Uint32 kNumUpdates = kPageSize / kUpdateSize;

    std::vector<Uint8> BufferData(kPageSize);
    for (size_t i = 0; i < BufferData.size(); ++i)
    {
        BufferData[i] = static_cast<Uint8>(i % 256);
    }

    BufferDesc Desc;
    Desc.Name      = "GPUUploadManagerTest buffer";
    Desc.Size      = BufferData.size();
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_VERTEX_BUFFER;
    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
    ASSERT_TRUE(pBuffer);

    GPUUploadManagerImpl::Page Page{nullptr, pDevice, kPageSize};
    Page.Reset(pContext);
    Page.Unseal();

    Threading::Signal StartSignal;

    std::vector<std::thread> threads;

    const size_t kNumThreads    = std::max(4u, std::thread::hardware_concurrency());
    const size_t kNumIterations = (kNumUpdates + kNumThreads - 1) / kNumThreads;

    std::atomic<size_t> UpdatesScheduled{0};
    std::atomic<Uint32> DstOffset{0};
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        threads.emplace_back(
            [&](size_t ThreadId) {
                StartSignal.Wait(true, static_cast<int>(kNumThreads));

                for (size_t i = 0; i < kNumIterations; ++i)
                {
                    GPUUploadManagerImpl::Page::Writer Writer = Page.TryBeginWriting();
                    EXPECT_TRUE(Writer) << "Writer should be valid because the page should not be sealed yet";
                    if (!Writer)
                        break;

                    Uint32 Offset = DstOffset.fetch_add(kUpdateSize);
                    if (Offset < kPageSize)
                    {
                        ScheduleBufferUpdateInfo UpdateInfo;
                        UpdateInfo.pDstBuffer = pBuffer;
                        UpdateInfo.pSrcData   = &BufferData[Offset];
                        UpdateInfo.DstOffset  = Offset;
                        UpdateInfo.NumBytes   = kUpdateSize;
                        if (Writer.ScheduleBufferUpdate(UpdateInfo))
                        {
                            UpdatesScheduled.fetch_add(1);
                        }
                    }
                    Writer.EndWriting();
                }
            },
            t);
    }

    StartSignal.Trigger(true);

    for (auto& thread : threads)
        thread.join();

    EXPECT_EQ(Page.GetNumPendingOps(), kNumUpdates);
    EXPECT_EQ(UpdatesScheduled.load(), kNumUpdates) << "Should be able to schedule updates until the page size is reached";
    EXPECT_EQ(Page.TrySeal(), GPUUploadManagerImpl::Page::SealStatus::Ready) << "Page should be ready for sealing after all updates are scheduled";
    Page.ExecutePendingOps(pContext, 1);
    Page.ReleaseStagingBuffer(pContext);

    VerifyBufferContents(pBuffer, BufferData);
}

TEST(GPUUploadManagerTest, ScheduleBufferUpdates)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 512, 1024};
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    std::vector<Uint8> BufferData(4096);
    for (size_t i = 0; i < BufferData.size(); ++i)
    {
        BufferData[i] = static_cast<Uint8>(i % 256);
    }

    BufferDesc Desc;
    Desc.Name      = "GPUUploadManagerTest buffer";
    Desc.Size      = BufferData.size();
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_VERTEX_BUFFER;

    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
    ASSERT_TRUE(pBuffer);

    pUploadManager->ScheduleBufferUpdate({pContext, pBuffer, 0, 0, nullptr});
    pUploadManager->ScheduleBufferUpdate({pContext, pBuffer, 0, 256, &BufferData[0]});
    pUploadManager->ScheduleBufferUpdate({pContext, pBuffer, 256, 256, &BufferData[256]});
    pUploadManager->ScheduleBufferUpdate({pContext, pBuffer, 512, 1024, &BufferData[512]});
    pUploadManager->ScheduleBufferUpdate({pContext, pBuffer, 1536, 512, &BufferData[1536]});
    pUploadManager->ScheduleBufferUpdate({pContext, pBuffer, 2048, 2048, &BufferData[2048]});
    pUploadManager->RenderThreadUpdate(pContext);
    pUploadManager->RenderThreadUpdate(pContext);

    VerifyBufferContents(pBuffer, BufferData);
    LogUploadManagerStats(pUploadManager);
}


TEST(GPUUploadManagerTest, ScheduleBufferUpdatesWithCopyBufferCallback)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 1024, 2048};
    CreateInfo.InitialPageCount = 2;
    // Set small max page count to test that ScheduleBufferUpdate creates new pages even when the limit
    // is reached when called from the render thread.
    CreateInfo.MaxPageCount      = 2;
    CreateInfo.MaxLargePageCount = 1;
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    std::vector<Uint8> BufferData(16384);
    for (size_t i = 0; i < BufferData.size(); ++i)
    {
        BufferData[i] = static_cast<Uint8>(i % 256);
    }

    BufferDesc Desc;
    Desc.Name      = "GPUUploadManagerTest buffer";
    Desc.Size      = BufferData.size();
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_VERTEX_BUFFER;

    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
    ASSERT_TRUE(pBuffer);

    Uint32 CurrOffset = 0;

    auto CopyBufferCallback = MakeCallback([&](IDeviceContext* pContext,
                                               IBuffer*        pSrcBuffer,
                                               Uint32          SrcOffset,
                                               Uint32          NumBytes) {
        pContext->CopyBuffer(pSrcBuffer, SrcOffset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                             pBuffer, CurrOffset, NumBytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        CurrOffset += NumBytes;
    });

    pUploadManager->ScheduleBufferUpdate({pContext, 256, &BufferData[0], CopyBufferCallback, CopyBufferCallback});
    pUploadManager->ScheduleBufferUpdate({pContext, 256, &BufferData[256], CopyBufferCallback, CopyBufferCallback});
    pUploadManager->ScheduleBufferUpdate({pContext, 1024, &BufferData[512], CopyBufferCallback, CopyBufferCallback});
    pUploadManager->ScheduleBufferUpdate({pContext, 512, &BufferData[1536], CopyBufferCallback, CopyBufferCallback});
    pUploadManager->ScheduleBufferUpdate({pContext, 2048, &BufferData[2048], CopyBufferCallback, CopyBufferCallback});
    pUploadManager->ScheduleBufferUpdate({pContext, 4096, &BufferData[4096], CopyBufferCallback, CopyBufferCallback});
    pUploadManager->ScheduleBufferUpdate({pContext, 8192, &BufferData[8192], CopyBufferCallback, CopyBufferCallback});

    pUploadManager->RenderThreadUpdate(pContext);
    pUploadManager->RenderThreadUpdate(pContext);

    VerifyBufferContents(pBuffer, BufferData);
    LogUploadManagerStats(pUploadManager);
}


TEST(GPUUploadManagerTest, ScheduleBufferUpdatesWithWriteDataCallback)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 512, 1024};
    CreateInfo.InitialPageCount = 2;
    CreateInfo.MaxPageCount     = 0;
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    std::vector<Uint8> BufferData(16384);
    for (size_t i = 0; i < BufferData.size(); ++i)
    {
        BufferData[i] = static_cast<Uint8>(i % 256);
    }

    BufferDesc Desc;
    Desc.Name      = "GPUUploadManagerTest buffer";
    Desc.Size      = BufferData.size();
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_VERTEX_BUFFER;

    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
    ASSERT_TRUE(pBuffer);

    Uint32 CurrOffset = 0;

    auto WriteDataCallback = MakeCallback([&](void* pDstData, Uint32 NumBytes) {
        std::memcpy(pDstData, &BufferData[CurrOffset], NumBytes);
    });

    for (Uint32 NumBytes : {256, 4096, 256, 8192, 1024, 2048, 512})
    {
        ScheduleBufferUpdateInfo UpdateInfo{pContext, pBuffer, CurrOffset, NumBytes, nullptr};
        UpdateInfo.WriteDataCallback          = WriteDataCallback;
        UpdateInfo.pWriteDataCallbackUserData = WriteDataCallback;
        pUploadManager->ScheduleBufferUpdate(UpdateInfo);

        CurrOffset += NumBytes;
    }

    pUploadManager->RenderThreadUpdate(pContext);
    pUploadManager->RenderThreadUpdate(pContext);

    VerifyBufferContents(pBuffer, BufferData);
    LogUploadManagerStats(pUploadManager);
}


TEST(GPUUploadManagerTest, ReleaseBufferCallbackResources)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 2048, 4096};
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    bool UploadEnqueuedCallbackCalled = false;
    bool CopyBufferCallbackCalled     = false;

    auto UploadEnqueuedCallback = MakeCallback(
        [&UploadEnqueuedCallbackCalled](IBuffer* pDstBuffer,
                                        Uint32   DstOffset,
                                        Uint32   NumBytes) {
            EXPECT_EQ(pDstBuffer, nullptr);
            EXPECT_EQ(DstOffset, 128u);
            EXPECT_EQ(NumBytes, 256u);
            UploadEnqueuedCallbackCalled = true;
        });

    auto CopyBufferCallback = MakeCallback(
        [&CopyBufferCallbackCalled](IDeviceContext* pContext,
                                    IBuffer*        pSrcBuffer,
                                    Uint32          SrcOffset,
                                    Uint32          NumBytes) {
            EXPECT_EQ(pContext, nullptr);
            EXPECT_EQ(pSrcBuffer, nullptr);
            EXPECT_EQ(SrcOffset, ~0u);
            EXPECT_EQ(NumBytes, 1024u);
            CopyBufferCallbackCalled = true;
        });

    std::thread Worker{
        [&]() {
            pUploadManager->ScheduleBufferUpdate({nullptr, 128, 256, nullptr, UploadEnqueuedCallback, UploadEnqueuedCallback});
            pUploadManager->ScheduleBufferUpdate({1024, nullptr, CopyBufferCallback, CopyBufferCallback});
        }};

    Worker.join();

    EXPECT_FALSE(UploadEnqueuedCallbackCalled) << "UploadEnqueued callback should not have been called before the upload manager is destroyed";
    EXPECT_FALSE(CopyBufferCallbackCalled) << "CopyBuffer callback should not have been called before the upload manager is destroyed";

    LogUploadManagerStats(pUploadManager);
    pUploadManager.Release();

    EXPECT_TRUE(UploadEnqueuedCallbackCalled) << "UploadEnqueued callback should have been called before the upload manager is destroyed";
    EXPECT_TRUE(CopyBufferCallbackCalled) << "CopyBuffer callback should have been called before the upload manager is destroyed";
}

TEST(GPUUploadManagerTest, ParallelBufferUpdates)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 8192, 16384};
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    std::vector<Uint8> BufferData(4 << 20);
    for (size_t i = 0; i < BufferData.size(); ++i)
    {
        BufferData[i] = static_cast<Uint8>(i % 256);
    }

    BufferDesc Desc;
    Desc.Name      = "GPUUploadManagerTest buffer";
    Desc.Size      = BufferData.size();
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_VERTEX_BUFFER;

    RefCntAutoPtr<IBuffer> pBuffer;

    pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
    ASSERT_TRUE(pBuffer);

    const size_t kNumThreads = std::max(2u, std::thread::hardware_concurrency() - 1);
    LOG_INFO_MESSAGE("Number of threads: ", kNumThreads);

    std::atomic<Uint32> CurrOffset{0};
    std::atomic<Uint32> NumUpdatesScheduled{0};
    std::atomic<Uint32> NumThreadsCompleted{0};

    std::vector<std::thread> Threads;
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        Threads.emplace_back(
            [&]() {
                while (true)
                {
                    const Uint32 UpdateSize = 64;

                    Uint32 Offset = CurrOffset.fetch_add(UpdateSize);
                    if (Offset >= BufferData.size())
                        break;
                    pUploadManager->ScheduleBufferUpdate({pBuffer, Offset, UpdateSize, &BufferData[Offset]});
                    NumUpdatesScheduled.fetch_add(1);
                }
                NumThreadsCompleted.fetch_add(1);
            });
    }

    const Uint32 NumUpdatesToRenderThreadUpdate = 256;

    Uint32 LastNumUpdatesScheduled    = 0;
    Uint32 NumIterationsWithoutUpdate = 0;
    Uint32 NumRenderThreadUpdates     = 0;
    while (NumThreadsCompleted.load() < kNumThreads)
    {
        if (LastNumUpdatesScheduled == NumUpdatesScheduled.load())
        {
            ++NumIterationsWithoutUpdate;
        }
        else
        {
            LastNumUpdatesScheduled    = NumUpdatesScheduled.load();
            NumIterationsWithoutUpdate = 0;
        }

        if (NumUpdatesScheduled.load() >= NumUpdatesToRenderThreadUpdate || NumIterationsWithoutUpdate >= 100)
        {
            pUploadManager->RenderThreadUpdate(pContext);
            NumUpdatesScheduled.store(0);
            pContext->Flush();
            pContext->FinishFrame();
            ++NumRenderThreadUpdates;
        }
    }

    LOG_INFO_MESSAGE("Total render thread updates: ", NumRenderThreadUpdates);

    pUploadManager->RenderThreadUpdate(pContext);

    for (std::thread& thread : Threads)
    {
        thread.join();
    }

    LogUploadManagerStats(pUploadManager);
    VerifyBufferContents(pBuffer, BufferData);
}


TEST(GPUUploadManagerTest, DestroyWhileBufferUpdatesAreRunning)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    constexpr size_t kNumIterations = 10;
    for (size_t i = 0; i < kNumIterations; ++i)
    {
        RefCntAutoPtr<IGPUUploadManager> pUploadManager;
        // Use small page size to make the uploaded attempt to create a new page and block in ScheduleBufferUpdate
        GPUUploadManagerCreateInfo CreateInfo{pDevice, pContext, 1024, 2048};
        CreateGPUUploadManager(CreateInfo, &pUploadManager);
        ASSERT_TRUE(pUploadManager != nullptr);

        const size_t             kNumThreads = 4;
        std::vector<std::thread> Threads;
        std::atomic<Uint32>      NumUpdatesRunning{0};
        Threading::Signal        AllThreadsRunningSignal;
        for (size_t t = 0; t < kNumThreads; ++t)
        {
            Threads.emplace_back(
                [&]() {
                    if (NumUpdatesRunning.fetch_add(1) == kNumThreads - 1)
                    {
                        AllThreadsRunningSignal.Trigger();
                    }
                    // Set update size to be larger than page size to make the manager create new large page and block in ScheduleBufferUpdate
                    pUploadManager->ScheduleBufferUpdate({nullptr, 0, 4096, nullptr});
                    NumUpdatesRunning.fetch_sub(1);
                });
        }

        AllThreadsRunningSignal.Wait();

        // Wait for some time to ensure that ScheduleTextureUpdate starts.
        std::this_thread::sleep_for(10ms);
        EXPECT_EQ(NumUpdatesRunning.load(), kNumThreads) << "All threads sh0ould be running updates because RenderThreadUpdate() was not called";

        if (i == kNumIterations - 1)
        {
            LogUploadManagerStats(pUploadManager);
        }

        pUploadManager.Release();

        for (std::thread& thread : Threads)
        {
            thread.join();
        }
        EXPECT_EQ(NumUpdatesRunning.load(), 0u);

        pContext->Flush();
        pContext->FinishFrame();
        pDevice->ReleaseStaleResources();
    }
}


TEST(GPUUploadManagerTest, CreateWithNullContext)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice};
    CreateInfo.PageSize              = 512;
    CreateInfo.InitialPageCount      = 8;
    CreateInfo.LargePageSize         = 1024;
    CreateInfo.InitialLargePageCount = 2;
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    const size_t kNumThreads = std::max(2u, std::thread::hardware_concurrency() - 1);
    LOG_INFO_MESSAGE("Number of threads: ", kNumThreads);

    constexpr size_t kNumUpdatesPerThread = 16;
    constexpr Uint32 kUpdateSize          = 2048;

    std::vector<Uint8> BufferData(kNumUpdatesPerThread * kUpdateSize * kNumThreads);
    for (size_t i = 0; i < BufferData.size(); ++i)
    {
        BufferData[i] = static_cast<Uint8>(i % 256);
    }

    BufferDesc Desc;
    Desc.Name      = "GPUUploadManagerTest buffer";
    Desc.Size      = BufferData.size();
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_VERTEX_BUFFER;

    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
    ASSERT_TRUE(pBuffer);

    std::vector<std::thread> Threads;
    std::atomic<Uint32>      NumUpdatesRunning{0};
    Threading::Signal        AllThreadsRunningSignal;

    std::atomic<Uint32> CurrOffset{0};
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        Threads.emplace_back(
            [&]() {
                if (NumUpdatesRunning.fetch_add(1) == kNumThreads - 1)
                {
                    AllThreadsRunningSignal.Trigger();
                }
                for (size_t i = 0; i < kNumUpdatesPerThread; ++i)
                {
                    Uint32 Offset = static_cast<Uint32>(CurrOffset.fetch_add(kUpdateSize));
                    pUploadManager->ScheduleBufferUpdate({pBuffer, Offset, kUpdateSize, &BufferData[Offset]});
                }
                NumUpdatesRunning.fetch_sub(1);
            });
    }

    AllThreadsRunningSignal.Wait();

    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(NumUpdatesRunning.load(), kNumThreads) << "All threads should be running updates because RenderThreadUpdate() was not called";

    while (NumUpdatesRunning.load() > 0)
    {
        pUploadManager->RenderThreadUpdate(pContext);
        pContext->Flush();
        pContext->FinishFrame();
        std::this_thread::sleep_for(10ms);
    }

    pUploadManager->RenderThreadUpdate(pContext);

    for (std::thread& thread : Threads)
    {
        thread.join();
    }

    VerifyBufferContents(pBuffer, BufferData);

    LogUploadManagerStats(pUploadManager);
}


TEST(GPUUploadManagerTest, MaxPageCount)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext};
    CreateInfo.PageSize              = 1024;
    CreateInfo.LargePageSize         = 2048;
    CreateInfo.InitialPageCount      = 8;
    CreateInfo.InitialLargePageCount = 8;
    CreateInfo.MaxPageCount          = 2;
    CreateInfo.MaxLargePageCount     = 2;
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    const size_t kNumThreads = std::max(2u, std::thread::hardware_concurrency() - 1);
    LOG_INFO_MESSAGE("Number of threads: ", kNumThreads);

    constexpr size_t kNumUpdatesPerThread = 32;
    constexpr Uint32 kUpdateSize          = 4096;

    std::vector<Uint8> BufferData(kNumUpdatesPerThread * kUpdateSize * kNumThreads);
    for (size_t i = 0; i < BufferData.size(); ++i)
    {
        BufferData[i] = static_cast<Uint8>(i % 256);
    }

    BufferDesc Desc;
    Desc.Name      = "GPUUploadManagerTest buffer";
    Desc.Size      = BufferData.size();
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_VERTEX_BUFFER;

    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
    ASSERT_TRUE(pBuffer);

    std::vector<std::thread> Threads;
    std::atomic<Uint32>      NumThreadsCompleted{0};

    std::atomic<Uint32> CurrOffset{0};
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        Threads.emplace_back(
            [&]() {
                for (size_t i = 0; i < kNumUpdatesPerThread; ++i)
                {
                    Uint32 UpadateSize = kUpdateSize >> (i % 5);
                    for (Uint32 j = 0; j < kUpdateSize / UpadateSize; ++j)
                    {
                        Uint32 Offset = CurrOffset.fetch_add(UpadateSize);
                        pUploadManager->ScheduleBufferUpdate({pBuffer, Offset, UpadateSize, &BufferData[Offset]});
                    }
                }
                NumThreadsCompleted.fetch_add(1);
            });
    }

    while (NumThreadsCompleted.load() < kNumThreads)
    {
        pUploadManager->RenderThreadUpdate(pContext);
        pContext->Flush();
        pContext->FinishFrame();
        std::this_thread::yield();
    }

    pUploadManager->RenderThreadUpdate(pContext);
    pContext->WaitForIdle();
    pUploadManager->RenderThreadUpdate(pContext);

    VerifyBufferContents(pBuffer, BufferData);

    for (std::thread& thread : Threads)
    {
        thread.join();
    }

    LogUploadManagerStats(pUploadManager);
    GPUUploadManagerStats Stats;
    pUploadManager->GetStats(Stats);
    ASSERT_EQ(Stats.NumStreams, 2u);
    EXPECT_EQ(Stats.pStreamStats[0].NumPages, CreateInfo.MaxPageCount) << "Normal page count should not exceed the specified maximum";
    EXPECT_EQ(Stats.pStreamStats[1].NumPages, CreateInfo.MaxLargePageCount) << "Large page count should not exceed the specified maximum";
}

Uint32 GetSubresourceIndex(const TextureDesc& TexDesc, const Uint32 Mip, Uint32 Slice)
{
    return Mip * TexDesc.GetArraySize() + Slice;
}

std::vector<std::vector<Uint8>> GenerateTextureSubresData(const TextureDesc& TexDesc)
{
    std::vector<std::vector<Uint8>> SubresData(TexDesc.GetSubresourceCount());
    for (Uint32 ArraySlice = 0; ArraySlice < TexDesc.GetArraySize(); ++ArraySlice)
    {
        for (Uint32 MipLevel = 0; MipLevel < TexDesc.MipLevels; ++MipLevel)
        {
            MipLevelProperties  Mip  = GetMipLevelProperties(TexDesc, MipLevel);
            std::vector<Uint8>& Data = SubresData[GetSubresourceIndex(TexDesc, MipLevel, ArraySlice)];
            Data.resize(static_cast<size_t>(Mip.MipSize));
            for (size_t i = 0; i < Data.size(); ++i)
            {
                Data[i] = static_cast<Uint8>(i % 256);
            }
        }
    }

    return SubresData;
}

void VerifyTextureContents(ITexture* pTexture, const std::vector<std::vector<Uint8>>& ExpectedSubresData)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    TextureDesc StagingTexDesc    = pTexture->GetDesc();
    StagingTexDesc.Name           = "GPUUploadManagerTest staging texture";
    StagingTexDesc.Usage          = USAGE_STAGING;
    StagingTexDesc.BindFlags      = BIND_NONE;
    StagingTexDesc.CPUAccessFlags = CPU_ACCESS_READ;
    RefCntAutoPtr<ITexture> pStagingTexture;
    pDevice->CreateTexture(StagingTexDesc, nullptr, &pStagingTexture);
    ASSERT_NE(pStagingTexture, nullptr);

    StateTransitionDesc Barriers[2] =
        {
            {pTexture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE},
            {pStagingTexture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_DEST, STATE_TRANSITION_FLAG_UPDATE_STATE},
        };
    pContext->TransitionResourceStates(_countof(Barriers), Barriers);
    for (Uint32 MipLevel = 0; MipLevel < StagingTexDesc.MipLevels; ++MipLevel)
    {
        for (Uint32 ArraySlice = 0; ArraySlice < StagingTexDesc.GetArraySize(); ++ArraySlice)
        {
            CopyTextureAttribs CopyAttribs;
            CopyAttribs.pSrcTexture = pTexture;
            CopyAttribs.pDstTexture = pStagingTexture;
            CopyAttribs.SrcMipLevel = MipLevel;
            CopyAttribs.SrcSlice    = ArraySlice;
            CopyAttribs.DstMipLevel = MipLevel;
            CopyAttribs.DstSlice    = ArraySlice;
            pContext->CopyTexture(CopyAttribs);
        }
    }

    RENDER_DEVICE_TYPE DeviceType = pDevice->GetDeviceInfo().Type;

    MAP_FLAGS MapFlags = (DeviceType == RENDER_DEVICE_TYPE_D3D12 ||
                          DeviceType == RENDER_DEVICE_TYPE_VULKAN ||
                          DeviceType == RENDER_DEVICE_TYPE_WEBGPU ||
                          DeviceType == RENDER_DEVICE_TYPE_METAL) ?
        MAP_FLAG_DO_NOT_WAIT :
        MAP_FLAG_NONE;

    const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(StagingTexDesc.Format);

    pContext->WaitForIdle();
    for (Uint32 MipLevel = 0; MipLevel < StagingTexDesc.MipLevels; ++MipLevel)
    {
        const MipLevelProperties Mip = GetMipLevelProperties(StagingTexDesc, MipLevel);
        for (Uint32 ArraySlice = 0; ArraySlice < StagingTexDesc.GetArraySize(); ++ArraySlice)
        {
            MappedTextureSubresource MappedSubres;
            pContext->MapTextureSubresource(pStagingTexture, MipLevel, ArraySlice, MAP_READ, MapFlags, nullptr, MappedSubres);

            const std::vector<Uint8>& ExpectedData = ExpectedSubresData[GetSubresourceIndex(StagingTexDesc, MipLevel, ArraySlice)];
            for (Uint32 DepthSlice = 0; DepthSlice < Mip.Depth; ++DepthSlice)
            {
                for (Uint32 row = 0; row < Mip.StorageHeight / FmtAttribs.BlockHeight; ++row)
                {
                    const void* pRowData = static_cast<Uint8*>(MappedSubres.pData) +
                        static_cast<size_t>(DepthSlice * MappedSubres.DepthStride + row * MappedSubres.Stride);
                    const void* pExpectedRowData = &ExpectedData[static_cast<size_t>(Mip.DepthSliceSize * DepthSlice + Mip.RowSize * row)];
                    EXPECT_TRUE(std::memcmp(pRowData, pExpectedRowData, static_cast<size_t>(Mip.RowSize)) == 0)
                        << "Texture subresource data does not match expected data for MipLevel "
                        << MipLevel << ", ArraySlice " << ArraySlice << ", DepthSlice " << DepthSlice << ", Row " << row;
                }
            }

            pContext->UnmapTextureSubresource(pStagingTexture, MipLevel, ArraySlice);
        }
    }
}

enum TEST_TEXTURE_UPDATES_FLAGS : Uint32
{
    TEST_TEXTURE_UPDATES_FLAGS_NONE                    = 0u,
    TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK       = 1u << 0u,
    TEST_TEXTURE_UPDATES_FLAGS_USE_WRITE_DATA_CALLBACK = 1u << 1u,
};


void TestWriterScheduleTextureUpdates(Uint32 Flags = TEST_TEXTURE_UPDATES_FLAGS_NONE)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    TextureDesc TexDesc;
    TexDesc.Name      = "GPUUploadManagerTest texture";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
    TexDesc.Usage     = USAGE_DEFAULT;
    TexDesc.Width     = 512;
    TexDesc.Height    = 512;
    TexDesc.MipLevels = 1;
    TexDesc.BindFlags = BIND_SHADER_RESOURCE;

    RefCntAutoPtr<ITexture> pTexture;
    pDevice->CreateTexture(TexDesc, nullptr, &pTexture);
    ASSERT_TRUE(pTexture != nullptr);

    const std::vector<std::vector<Uint8>> SubresData = GenerateTextureSubresData(TexDesc);

    const Uint32 ElementSize = GetTextureFormatAttribs(TexDesc.Format).GetElementSize();

    std::unique_ptr<GPUUploadManagerImpl::Page> Page;
    if (pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D11)
    {
        Page = std::make_unique<GPUUploadManagerImpl::Page>(nullptr, pDevice, TexDesc.Width, TexDesc.Format);
    }
    else
    {
        Page = std::make_unique<GPUUploadManagerImpl::Page>(nullptr, pDevice, TexDesc.Width * TexDesc.Height * ElementSize);
    }

    const std::vector<Uint8>& Mip0Data = SubresData[0];
    Page->Reset(pContext);
    Page->Unseal();

    Uint32 NumWriteDataCallbackCalled   = 0;
    Uint32 NumCopyTextureCallbackCalled = 0;

    auto WriteDataCallback = MakeCallback([&](void* pDstData, Uint32 Stride, Uint32 DepthStride, const Box& DstBox) {
        for (Uint32 row = 0; row < DstBox.Height(); ++row)
        {
            const void* pSrcRow = &Mip0Data[(DstBox.MinX + (DstBox.MinY + row) * TexDesc.Width) * ElementSize];
            void*       pDstRow = static_cast<Uint8*>(pDstData) + row * Stride;
            std::memcpy(pDstRow, pSrcRow, DstBox.Width() * ElementSize);
        }
        ++NumWriteDataCallbackCalled;
    });

    struct CopyTextureCallbackData
    {
        ITexture* const pDstTexture;
        Uint32&         NumCopyTextureCallbackCalled;
    };
    CopyTextureCallbackData CopyCallbackData{pTexture, NumCopyTextureCallbackCalled};

    auto CopyTextureCallback = [](IDeviceContext*          pContext,
                                  Uint32                   DstMipLevel,
                                  Uint32                   DstSlice,
                                  const Box&               DstBox,
                                  const TextureSubResData& SrcData,
                                  void*                    pUserData) {
        CopyTextureCallbackData* pCallbackData = static_cast<CopyTextureCallbackData*>(pUserData);
        pContext->UpdateTexture(pCallbackData->pDstTexture, DstMipLevel, DstSlice, DstBox, SrcData,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ++pCallbackData->NumCopyTextureCallbackCalled;
    };

    auto CopyD3D11TextureCallback = [](IDeviceContext* pContext,
                                       Uint32          DstMipLevel,
                                       Uint32          DstSlice,
                                       const Box&      DstBox,
                                       ITexture*       pSrcTexture,
                                       Uint32          SrcX,
                                       Uint32          SrcY,
                                       void*           pUserData) {
        CopyTextureCallbackData* pCallbackData = static_cast<CopyTextureCallbackData*>(pUserData);

        CopyTextureAttribs CopyAttribs;
        CopyAttribs.pSrcTexture = pSrcTexture;
        CopyAttribs.pDstTexture = pCallbackData->pDstTexture;
        CopyAttribs.DstMipLevel = DstMipLevel;
        CopyAttribs.DstSlice    = DstSlice;
        CopyAttribs.DstX        = DstBox.MinX;
        CopyAttribs.DstY        = DstBox.MinY;
        CopyAttribs.DstZ        = DstBox.MinZ;

        Box SrcBox;
        SrcBox.MinX = SrcX;
        SrcBox.MinY = SrcY;
        SrcBox.MaxX = SrcX + DstBox.Width();
        SrcBox.MaxY = SrcY + DstBox.Height();

        CopyAttribs.pSrcBox = &SrcBox;

        CopyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        pContext->CopyTexture(CopyAttribs);
        ++pCallbackData->NumCopyTextureCallbackCalled;
    };

    Uint32 NumUploadEnqueuedCallbackCalled = 0;

    auto UploadEnqueuedCallback = MakeCallback([&](ITexture*  pDstTexture,
                                                   Uint32     DstMipLevel,
                                                   Uint32     DstSlice,
                                                   const Box& DstBox) {
        ++NumUploadEnqueuedCallbackCalled;
    });

    {
        GPUUploadManagerImpl::Page::Writer Writer = Page->TryBeginWriting();
        EXPECT_TRUE(Writer) << "Should be able to begin writing to a new page";

        auto ScheduleUpdate = [&](Uint32 col, Uint32 row, Uint32 Width, Uint32 Height) {
            ScheduleTextureUpdateInfo UpdateInfo;
            if (Flags & TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK)
            {
                UpdateInfo.Format           = TEX_FORMAT_RGBA8_UNORM;
                UpdateInfo.CopyTexture      = CopyTextureCallback;
                UpdateInfo.CopyD3D11Texture = CopyD3D11TextureCallback;
                UpdateInfo.pCopyTextureData = &CopyCallbackData;
            }
            else
            {
                UpdateInfo.pDstTexture = pTexture;
            }
            UpdateInfo.DstBox.MinX = col;
            UpdateInfo.DstBox.MinY = row;
            UpdateInfo.DstBox.MaxX = UpdateInfo.DstBox.MinX + Width;
            UpdateInfo.DstBox.MaxY = UpdateInfo.DstBox.MinY + Height;
            if (Flags & TEST_TEXTURE_UPDATES_FLAGS_USE_WRITE_DATA_CALLBACK)
            {
                UpdateInfo.WriteDataCallback          = WriteDataCallback;
                UpdateInfo.pWriteDataCallbackUserData = WriteDataCallback;
            }
            else
            {
                UpdateInfo.pSrcData = &Mip0Data[(UpdateInfo.DstBox.MinX + UpdateInfo.DstBox.MinY * TexDesc.Width) * ElementSize];
                UpdateInfo.Stride   = TexDesc.Width * ElementSize;
            }

            UpdateInfo.UploadEnqueued      = UploadEnqueuedCallback;
            UpdateInfo.pUploadEnqueuedData = UploadEnqueuedCallback;

            return Writer.ScheduleTextureUpdate(UpdateInfo, GetBufferToTextureCopyInfo(TexDesc.Format, UpdateInfo.DstBox, 1024), 1024);
        };

        for (Uint32 row = 0; row < 2; ++row)
        {
            for (Uint32 col = 0; col < 2; ++col)
            {
                EXPECT_TRUE(ScheduleUpdate(col * 256, row * 256, 256, 256)) << "Should be able to schedule update that fits in the page";
            }
        }

        EXPECT_FALSE(ScheduleUpdate(0, 0, 256, 256)) << "Should not be able to schedule update when the page is full";

        EXPECT_EQ(Page->GetNumPendingOps(), size_t{4});
        EXPECT_TRUE(Writer.EndWriting() == GPUUploadManagerImpl::Page::WritingStatus::NotSealed) << "Page should not be sealed";
    }

    EXPECT_EQ(Page->TrySeal(), GPUUploadManagerImpl::Page::SealStatus::Ready) << "Page with no active writers should be ready immediately";
    EXPECT_EQ(Page->GetNumPendingOps(), size_t{4});
    Page->ExecutePendingOps(pContext, 1);
    EXPECT_EQ(Page->GetNumPendingOps(), size_t{0});
    EXPECT_EQ(NumUploadEnqueuedCallbackCalled, (Flags & TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK) ? 0u : 4u);
    Page->ReleaseStagingBuffer(pContext);

    EXPECT_EQ((Flags & TEST_TEXTURE_UPDATES_FLAGS_USE_WRITE_DATA_CALLBACK) ? 4u : 0u, NumWriteDataCallbackCalled);
    EXPECT_EQ((Flags & TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK) ? 4u : 0u, NumCopyTextureCallbackCalled);

    VerifyTextureContents(pTexture, SubresData);
}

TEST(GPUUploadManagerTest, Writer_ScheduleTextureUpdates)
{
    TestWriterScheduleTextureUpdates();
}

TEST(GPUUploadManagerTest, Writer_ScheduleTextureUpdates_WithWriteDataCallback)
{
    TestWriterScheduleTextureUpdates(TEST_TEXTURE_UPDATES_FLAGS_USE_WRITE_DATA_CALLBACK);
}

TEST(GPUUploadManagerTest, Writer_ScheduleTextureUpdates_WithCopyCallback)
{
    TestWriterScheduleTextureUpdates(TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK);
}

TEST(GPUUploadManagerTest, Writer_ScheduleTextureUpdates_WithWriteAndCopyCallbacks)
{
    TestWriterScheduleTextureUpdates(TEST_TEXTURE_UPDATES_FLAGS_USE_WRITE_DATA_CALLBACK | TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK);
}

void TestTextureUpdates(TEXTURE_FORMAT Format, RESOURCE_DIMENSION Type, Uint32 Flags = TEST_TEXTURE_UPDATES_FLAGS_NONE)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    if (pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D11 && Type == RESOURCE_DIM_TEX_3D)
    {
        GTEST_SKIP() << "3D texture updates are not supported in D3D11";
    }

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 64 << 10, 256 << 10};
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    TextureDesc TexDesc;
    TexDesc.Name      = "GPUUploadManagerTest texture";
    TexDesc.Type      = Type;
    TexDesc.Format    = Format;
    TexDesc.Usage     = USAGE_DEFAULT;
    TexDesc.Width     = 256;
    TexDesc.Height    = 256;
    TexDesc.ArraySize = 4;
    TexDesc.MipLevels = ComputeMipLevelsCount(TexDesc.Width, TexDesc.Height);
    TexDesc.BindFlags = BIND_SHADER_RESOURCE;

    RefCntAutoPtr<ITexture> pTexture;
    pDevice->CreateTexture(TexDesc, nullptr, &pTexture);
    ASSERT_TRUE(pTexture != nullptr);

    const std::vector<std::vector<Uint8>> SubresData = GenerateTextureSubresData(TexDesc);

    const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(TexDesc.Format);

    const Uint32 ElementSize = FmtAttribs.GetElementSize();

    for (Uint32 MipLevel = 0; MipLevel < TexDesc.MipLevels; ++MipLevel)
    {
        const MipLevelProperties Mip = GetMipLevelProperties(TexDesc, MipLevel);
        for (Uint32 ArraySlice = 0; ArraySlice < TexDesc.GetArraySize(); ++ArraySlice)
        {
            const std::vector<Uint8>& Data = SubresData[GetSubresourceIndex(TexDesc, MipLevel, ArraySlice)];

            Uint32 UpdateWidth  = std::min(32u, Mip.LogicalWidth);
            Uint32 UpdateHeight = std::min(32u, Mip.LogicalHeight);
            Uint32 UpdateDepth  = std::min(4u, Mip.Depth);

            for (Uint32 z = 0; z < Mip.Depth; z += UpdateDepth)
            {
                for (Uint32 y = 0; y < Mip.LogicalHeight; y += UpdateHeight)
                {
                    for (Uint32 x = 0; x < Mip.LogicalWidth; x += UpdateWidth)
                    {
                        ScheduleTextureUpdateInfo UpdateInfo;
                        UpdateInfo.pContext = pContext;
                        if (Flags & TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK)
                        {
                            UpdateInfo.Format = Format;
                        }
                        else
                        {
                            UpdateInfo.pDstTexture = pTexture;
                        }

                        Box& DstBox{UpdateInfo.DstBox};
                        DstBox.MinX = x;
                        DstBox.MinY = y;
                        DstBox.MinZ = z;
                        DstBox.MaxX = std::min(x + UpdateWidth, Mip.LogicalWidth);
                        DstBox.MaxY = std::min(y + UpdateHeight, Mip.LogicalHeight);
                        DstBox.MaxZ = std::min(z + UpdateDepth, Mip.Depth);

                        UpdateInfo.Stride      = Mip.RowSize;
                        UpdateInfo.DepthStride = Mip.DepthSliceSize;
                        UpdateInfo.DstMipLevel = MipLevel;
                        UpdateInfo.DstSlice    = ArraySlice;

                        const void* pSrcData = &Data[static_cast<size_t>(
                            Mip.DepthSliceSize * z + Mip.RowSize * (y / FmtAttribs.BlockHeight) + (x / FmtAttribs.BlockWidth) * ElementSize)];
                        if (Flags & TEST_TEXTURE_UPDATES_FLAGS_USE_WRITE_DATA_CALLBACK)
                        {
                            struct CallbackData
                            {
                                const void*  pSrcData;
                                const Uint32 Stride;
                                const Uint32 DepthStride;
                                const Uint32 BytesToCopy;
                                const Uint32 BlockHeight;
                            };
                            UpdateInfo.pWriteDataCallbackUserData = new CallbackData{
                                pSrcData,
                                static_cast<Uint32>(UpdateInfo.Stride),
                                static_cast<Uint32>(UpdateInfo.DepthStride),
                                AlignUp(DstBox.Width(), FmtAttribs.BlockWidth) / FmtAttribs.BlockWidth * ElementSize,
                                FmtAttribs.BlockHeight,
                            };
                            UpdateInfo.WriteDataCallback = [](void* pDstData, Uint32 Stride, Uint32 DepthStride, const Box& DstBox, void* pUserData) {
                                CallbackData* pData   = static_cast<CallbackData*>(pUserData);
                                const Uint32  NumRows = AlignUp(DstBox.Height(), pData->BlockHeight) / pData->BlockHeight;
                                for (Uint32 DepthSlice = 0; DepthSlice < DstBox.Depth(); ++DepthSlice)
                                {
                                    for (Uint32 row = 0; row < NumRows; ++row)
                                    {
                                        const size_t SrcRowOffset = static_cast<size_t>(DepthSlice * pData->DepthStride + row * pData->Stride);
                                        const size_t DstRowOffset = static_cast<size_t>(DepthSlice * DepthStride + row * Stride);
                                        const void*  pSrcRow      = static_cast<const Uint8*>(pData->pSrcData) + SrcRowOffset;
                                        void*        pDstRow      = static_cast<Uint8*>(pDstData) + DstRowOffset;
                                        std::memcpy(pDstRow, pSrcRow, pData->BytesToCopy);
                                    }
                                }
                                delete pData;
                            };
                        }
                        else
                        {
                            UpdateInfo.pSrcData = pSrcData;
                        }

                        if (Flags & TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK)
                        {
                            UpdateInfo.pCopyTextureData = new ITexture* {pTexture};

                            UpdateInfo.CopyTexture =
                                [](IDeviceContext*          pContext,
                                   Uint32                   DstMipLevel,
                                   Uint32                   DstSlice,
                                   const Box&               DstBox,
                                   const TextureSubResData& SrcData,
                                   void*                    pUserData) {
                                    ITexture** ppTexture = static_cast<ITexture**>(pUserData);
                                    pContext->UpdateTexture(*ppTexture, DstMipLevel, DstSlice, DstBox, SrcData,
                                                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                                    delete ppTexture;
                                };

                            UpdateInfo.CopyD3D11Texture =
                                [](IDeviceContext* pContext,
                                   Uint32          DstMipLevel,
                                   Uint32          DstSlice,
                                   const Box&      DstBox,
                                   ITexture*       pSrcTexture,
                                   Uint32          SrcX,
                                   Uint32          SrcY,
                                   void*           pUserData) {
                                    ITexture** ppTexture = static_cast<ITexture**>(pUserData);

                                    CopyTextureAttribs CopyAttribs;
                                    CopyAttribs.pSrcTexture = pSrcTexture;
                                    CopyAttribs.pDstTexture = *ppTexture;
                                    CopyAttribs.DstMipLevel = DstMipLevel;
                                    CopyAttribs.DstSlice    = DstSlice;
                                    CopyAttribs.DstX        = DstBox.MinX;
                                    CopyAttribs.DstY        = DstBox.MinY;
                                    CopyAttribs.DstZ        = DstBox.MinZ;

                                    const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(CopyAttribs.pDstTexture->GetDesc().Format);

                                    Box SrcBox;
                                    SrcBox.MinX = SrcX;
                                    SrcBox.MinY = SrcY;
                                    SrcBox.MaxX = AlignUp(SrcX + DstBox.Width(), FmtAttribs.BlockWidth);
                                    SrcBox.MaxY = AlignUp(SrcY + DstBox.Height(), FmtAttribs.BlockHeight);

                                    CopyAttribs.pSrcBox = &SrcBox;

                                    CopyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

                                    pContext->CopyTexture(CopyAttribs);

                                    delete ppTexture;
                                };
                        }

                        pUploadManager->ScheduleTextureUpdate(UpdateInfo);
                    }
                }
            }
        }
    }

    pUploadManager->RenderThreadUpdate(pContext);
    pUploadManager->RenderThreadUpdate(pContext);

    // Reading back compressed textures is not supported on OpenGL.
    if (!(pDevice->GetDeviceInfo().IsGLDevice() && FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED))
    {
        VerifyTextureContents(pTexture, SubresData);
    }

    LogUploadManagerStats(pUploadManager);
}

TEST(GPUUploadManagerTest, ScheduleTextureUpdates_Tex2DArray_RGBA8)
{
    TestTextureUpdates(TEX_FORMAT_RGBA8_UNORM, RESOURCE_DIM_TEX_2D_ARRAY);
}

TEST(GPUUploadManagerTest, ScheduleTextureUpdates_Tex2DArray_BC1)
{
    TestTextureUpdates(TEX_FORMAT_BC1_UNORM, RESOURCE_DIM_TEX_2D_ARRAY);
}

TEST(GPUUploadManagerTest, ScheduleTextureUpdates_Tex3D_RGBA8)
{
    TestTextureUpdates(TEX_FORMAT_RGBA8_UNORM, RESOURCE_DIM_TEX_3D);
}


TEST(GPUUploadManagerTest, ScheduleTextureUpdates_Tex2DArray_RGBA8_WithCopyCallback)
{
    TestTextureUpdates(TEX_FORMAT_RGBA8_UNORM, RESOURCE_DIM_TEX_2D_ARRAY, TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK);
}

TEST(GPUUploadManagerTest, ScheduleTextureUpdates_Tex2DArray_BC1_WithCopyCallback)
{
    TestTextureUpdates(TEX_FORMAT_BC1_UNORM, RESOURCE_DIM_TEX_2D_ARRAY, TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK);
}

TEST(GPUUploadManagerTest, ScheduleTextureUpdates_Tex3D_RGBA8_WithCopyCallback)
{
    TestTextureUpdates(TEX_FORMAT_RGBA8_UNORM, RESOURCE_DIM_TEX_3D, TEST_TEXTURE_UPDATES_FLAGS_USE_COPY_CALLBACK);
}


TEST(GPUUploadManagerTest, ScheduleTextureUpdates_Tex2DArray_RGBA8_WithWriteCallback)
{
    TestTextureUpdates(TEX_FORMAT_RGBA8_UNORM, RESOURCE_DIM_TEX_2D_ARRAY, TEST_TEXTURE_UPDATES_FLAGS_USE_WRITE_DATA_CALLBACK);
}

TEST(GPUUploadManagerTest, ScheduleTextureUpdates_Tex2DArray_BC1_WithWriteCallback)
{
    TestTextureUpdates(TEX_FORMAT_BC1_UNORM, RESOURCE_DIM_TEX_2D_ARRAY, TEST_TEXTURE_UPDATES_FLAGS_USE_WRITE_DATA_CALLBACK);
}

TEST(GPUUploadManagerTest, ScheduleTextureUpdates_Tex3D_RGBA8_WithWriteCallback)
{
    TestTextureUpdates(TEX_FORMAT_RGBA8_UNORM, RESOURCE_DIM_TEX_3D, TEST_TEXTURE_UPDATES_FLAGS_USE_WRITE_DATA_CALLBACK);
}


TEST(GPUUploadManagerTest, ReleaseTextureCallbackResources)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 16384, 65536};
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    int UploadEnqueuedCallbackCalled = 0;
    int CopyTextureCallbackCalled    = 0;

    auto UploadEnqueuedCallback = MakeCallback(
        [&UploadEnqueuedCallbackCalled](ITexture*  pDstTexture,
                                        Uint32     DstMipLevel,
                                        Uint32     DstSlice,
                                        const Box& DstBox) {
            EXPECT_EQ(pDstTexture, nullptr);
            EXPECT_EQ(DstMipLevel, 1u);
            EXPECT_EQ(DstSlice, 2u);
            ++UploadEnqueuedCallbackCalled;
        });

    auto CopyTextureCallback =
        [](IDeviceContext*          pContext,
           Uint32                   DstMipLevel,
           Uint32                   DstSlice,
           const Box&               DstBox,
           const TextureSubResData& SrcData,
           void*                    pUserData) {
            int& CopyTextureCallbackCalled = *static_cast<int*>(pUserData);
            EXPECT_EQ(pContext, nullptr);
            EXPECT_EQ(DstMipLevel, 3u);
            EXPECT_EQ(DstSlice, 4u);
            EXPECT_EQ(SrcData.pData, nullptr);
            EXPECT_EQ(SrcData.pSrcBuffer, nullptr);
            ++CopyTextureCallbackCalled;
        };

    auto CopyD3D11TextureCallback =
        [](IDeviceContext* pContext,
           Uint32          DstMipLevel,
           Uint32          DstSlice,
           const Box&      DstBox,
           ITexture*       pSrcTexture,
           Uint32          SrcX,
           Uint32          SrcY,
           void*           pUserData) {
            int& CopyTextureCallbackCalled = *static_cast<int*>(pUserData);
            EXPECT_EQ(pContext, nullptr);
            EXPECT_EQ(DstMipLevel, 3u);
            EXPECT_EQ(DstSlice, 4u);
            EXPECT_EQ(pSrcTexture, nullptr);
            ++CopyTextureCallbackCalled;
        };

    TextureDesc TexDesc;
    TexDesc.Name      = "GPUUploadManagerTest texture";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
    TexDesc.Usage     = USAGE_DEFAULT;
    TexDesc.Width     = 256;
    TexDesc.Height    = 256;
    TexDesc.ArraySize = 5;
    TexDesc.MipLevels = 4;
    TexDesc.BindFlags = BIND_SHADER_RESOURCE;
    RefCntAutoPtr<ITexture> pTexture;
    pDevice->CreateTexture(TexDesc, nullptr, &pTexture);
    ASSERT_NE(pTexture, nullptr);

    if (pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D11)
    {
        // Force creation of texture upload page, otherwise ScheduleTextureUpdate()
        // will block in the worker thread waiting for the RenderThreadUpdate().
        ScheduleTextureUpdateInfo UpdateInfo;
        UpdateInfo.pContext    = pContext;
        UpdateInfo.pDstTexture = pTexture;
        UpdateInfo.DstMipLevel = 0;
        UpdateInfo.DstSlice    = 0;
        UpdateInfo.DstBox      = {0, 16, 0, 16};
        pUploadManager->ScheduleTextureUpdate(UpdateInfo);
    }

    std::thread Worker{
        [&]() {
            {
                ScheduleTextureUpdateInfo UpdateInfo;
                UpdateInfo.pDstTexture         = pTexture;
                UpdateInfo.DstMipLevel         = 1;
                UpdateInfo.DstSlice            = 2;
                UpdateInfo.UploadEnqueued      = UploadEnqueuedCallback;
                UpdateInfo.pUploadEnqueuedData = UploadEnqueuedCallback;
                UpdateInfo.DstBox              = {0, 16, 0, 16};
                pUploadManager->ScheduleTextureUpdate(UpdateInfo);
            }

            {
                ScheduleTextureUpdateInfo UpdateInfo;
                UpdateInfo.pDstTexture      = pTexture;
                UpdateInfo.DstMipLevel      = 3;
                UpdateInfo.DstSlice         = 4;
                UpdateInfo.CopyTexture      = CopyTextureCallback;
                UpdateInfo.CopyD3D11Texture = CopyD3D11TextureCallback;
                UpdateInfo.pCopyTextureData = &CopyTextureCallbackCalled;
                UpdateInfo.DstBox           = {0, 16, 0, 16};
                pUploadManager->ScheduleTextureUpdate(UpdateInfo);
            }
        }};

    Worker.join();

    EXPECT_EQ(UploadEnqueuedCallbackCalled, 0) << "UploadEnqueued callback should not have been called before the upload manager is destroyed";
    EXPECT_EQ(CopyTextureCallbackCalled, 0) << "CopyTexture callback should not have been called before the upload manager is destroyed";

    LogUploadManagerStats(pUploadManager);

    pUploadManager.Release();

    EXPECT_EQ(UploadEnqueuedCallbackCalled, 1) << "UploadEnqueued callback should have been called before the upload manager is destroyed";
    EXPECT_EQ(CopyTextureCallbackCalled, 1) << "CopyTexture callback should have been called before the upload manager is destroyed";
}


TEST(GPUUploadManagerTest, ParallelBufferAndTextureUpdates)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 16384, 65536};
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    struct TextureAndSubresData
    {
        RefCntAutoPtr<ITexture>         pTexture;
        std::vector<std::vector<Uint8>> SubresData;
    };

    constexpr TEXTURE_FORMAT Formats[] = {
        TEX_FORMAT_R8_UNORM,
        TEX_FORMAT_RG8_UNORM,
        TEX_FORMAT_RGBA8_UNORM,
        TEX_FORMAT_RG16_UINT,
        TEX_FORMAT_RGBA32_UINT,
    };
    std::array<TextureAndSubresData, _countof(Formats)> TexAndData;

    std::vector<ScheduleTextureUpdateInfo> TextureUpdates;
    for (size_t i = 0; i < TexAndData.size(); ++i)
    {
        const std::string Name = std::string{"GPUUploadManagerTest texture"} + GetTextureFormatAttribs(Formats[i]).Name;

        TextureDesc TexDesc;
        TexDesc.Name                      = Name.c_str();
        TexDesc.Type                      = RESOURCE_DIM_TEX_2D_ARRAY;
        TexDesc.Format                    = Formats[i];
        TexDesc.Usage                     = USAGE_DEFAULT;
        TexDesc.Width                     = 512;
        TexDesc.Height                    = 512;
        TexDesc.ArraySize                 = 32;
        TexDesc.MipLevels                 = 7;
        TexDesc.BindFlags                 = BIND_SHADER_RESOURCE;
        RefCntAutoPtr<ITexture>& pTexture = TexAndData[i].pTexture;
        pDevice->CreateTexture(TexDesc, nullptr, &pTexture);
        ASSERT_NE(pTexture, nullptr);

        std::vector<std::vector<Uint8>>& SubresData{TexAndData[i].SubresData};
        SubresData = GenerateTextureSubresData(TexDesc);

        for (Uint32 MipLevel = 0; MipLevel < TexDesc.MipLevels; ++MipLevel)
        {
            const MipLevelProperties Mip = GetMipLevelProperties(TexDesc, MipLevel);
            for (Uint32 ArraySlice = 0; ArraySlice < TexDesc.ArraySize; ++ArraySlice)
            {
                const std::vector<Uint8>& MipData = SubresData[GetSubresourceIndex(TexDesc, MipLevel, ArraySlice)];

                const TextureFormatAttribs& FmtAttribs  = GetTextureFormatAttribs(TexDesc.Format);
                const Uint32                ElementSize = FmtAttribs.ComponentSize * FmtAttribs.NumComponents;

                const Uint32 UpdateWidth  = std::min((ArraySlice % 4 == 0) ? Mip.LogicalWidth / 2 : 32, Mip.LogicalWidth);
                const Uint32 UpdateHeight = std::min((ArraySlice % 4 == 0) ? Mip.LogicalHeight / 2 : 32, Mip.LogicalHeight);
                for (Uint32 x = 0; x < Mip.LogicalWidth; x += UpdateWidth)
                {
                    for (Uint32 y = 0; y < Mip.LogicalHeight; y += UpdateHeight)
                    {
                        ScheduleTextureUpdateInfo UpdateInfo;
                        UpdateInfo.pDstTexture = pTexture;
                        UpdateInfo.DstBox.MinX = x;
                        UpdateInfo.DstBox.MinY = y;
                        UpdateInfo.DstBox.MaxX = x + UpdateWidth;
                        UpdateInfo.DstBox.MaxY = y + UpdateHeight;
                        UpdateInfo.DstMipLevel = MipLevel;
                        UpdateInfo.DstSlice    = ArraySlice;
                        UpdateInfo.pSrcData    = &MipData[static_cast<size_t>(Mip.RowSize * y + x * ElementSize)];
                        UpdateInfo.Stride      = Mip.RowSize;
                        TextureUpdates.push_back(UpdateInfo);
                    }
                }
            }
        }
    }

    constexpr Uint32 BufferUpdateSize = 128;

    std::vector<Uint8> BufferData(BufferUpdateSize * TextureUpdates.size());
    for (size_t i = 0; i < BufferData.size(); ++i)
    {
        BufferData[i] = static_cast<Uint8>(i % 256);
    }

    BufferDesc Desc;
    Desc.Name      = "GPUUploadManagerTest buffer";
    Desc.Size      = BufferData.size();
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_VERTEX_BUFFER;

    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
    ASSERT_TRUE(pBuffer);

    const size_t kNumThreads = std::max(2u, std::thread::hardware_concurrency() - 1);
    LOG_INFO_MESSAGE("Number of threads: ", kNumThreads);

    std::atomic<Uint32> CurrOffset{0};
    std::atomic<Uint32> NumUpdatesScheduled{0};
    std::atomic<Uint32> NumThreadsCompleted{0};
    std::atomic<Uint32> NextTextureUpdate{0};

    std::vector<std::thread> Threads;
    for (size_t t = 0; t < kNumThreads; ++t)
    {
        Threads.emplace_back(
            [&]() {
                while (true)
                {
                    Uint32 Offset = CurrOffset.fetch_add(BufferUpdateSize);
                    if (Offset < BufferData.size())
                    {
                        pUploadManager->ScheduleBufferUpdate({pBuffer, Offset, BufferUpdateSize, &BufferData[Offset]});
                    }

                    Uint32 TexUpdateIndex = NextTextureUpdate.fetch_add(1);
                    if (TexUpdateIndex < TextureUpdates.size())
                    {
                        pUploadManager->ScheduleTextureUpdate(TextureUpdates[TexUpdateIndex]);
                    }

                    if (Offset >= BufferData.size() && TexUpdateIndex >= TextureUpdates.size())
                    {
                        break;
                    }

                    NumUpdatesScheduled.fetch_add(1);
                }
                NumThreadsCompleted.fetch_add(1);
            });
    }

    const Uint32 NumUpdatesToRenderThreadUpdate = 256;

    Uint32 LastNumUpdatesScheduled    = 0;
    Uint32 NumIterationsWithoutUpdate = 0;
    Uint32 NumRenderThreadUpdates     = 0;
    while (NumThreadsCompleted.load() < kNumThreads)
    {
        if (LastNumUpdatesScheduled == NumUpdatesScheduled.load())
        {
            ++NumIterationsWithoutUpdate;
        }
        else
        {
            LastNumUpdatesScheduled    = NumUpdatesScheduled.load();
            NumIterationsWithoutUpdate = 0;
        }

        if (NumUpdatesScheduled.load() >= NumUpdatesToRenderThreadUpdate || NumIterationsWithoutUpdate >= 100)
        {
            pUploadManager->RenderThreadUpdate(pContext);
            NumUpdatesScheduled.store(0);
            pContext->Flush();
            pContext->FinishFrame();
            ++NumRenderThreadUpdates;
        }
    }

    LOG_INFO_MESSAGE("Total render thread updates: ", NumRenderThreadUpdates);

    pUploadManager->RenderThreadUpdate(pContext);
    pContext->WaitForIdle();
    pUploadManager->RenderThreadUpdate(pContext);

    for (std::thread& thread : Threads)
    {
        thread.join();
    }

    VerifyBufferContents(pBuffer, BufferData);
    for (const TextureAndSubresData& Data : TexAndData)
    {
        VerifyTextureContents(Data.pTexture, Data.SubresData);
    }

    GPUUploadManagerStats Stats;
    pUploadManager->GetStats(Stats);
    ASSERT_GE(Stats.NumStreams, 2u);
    EXPECT_LE(Stats.pStreamStats[0].NumPages, CreateInfo.MaxPageCount) << "Normal page count should not exceed the specified maximum";
    EXPECT_LE(Stats.pStreamStats[1].NumPages, CreateInfo.MaxLargePageCount) << "Large page count should not exceed the specified maximum";

    LogUploadManagerStats(pUploadManager);
}


TEST(GPUUploadManagerTest, DestroyWhileTextureUpdatesAreRunning)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    TextureDesc TexDesc;
    TexDesc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    TexDesc.Usage     = USAGE_DEFAULT;
    TexDesc.Width     = 512;
    TexDesc.Height    = 512;
    TexDesc.ArraySize = 32;
    TexDesc.MipLevels = 4;
    TexDesc.BindFlags = BIND_SHADER_RESOURCE;

    std::array<RefCntAutoPtr<ITexture>, 4> pTextures;
    std::array<TEXTURE_FORMAT, 4>          Formats = {TEX_FORMAT_RGBA8_UNORM, TEX_FORMAT_BC1_UNORM, TEX_FORMAT_RG16_FLOAT, TEX_FORMAT_BC3_UNORM};
    for (size_t i = 0; i < pTextures.size(); ++i)
    {
        TextureDesc Desc = TexDesc;
        Desc.Format      = Formats[i];
        std::string Name = "GPUUploadManagerTest texture " + std::to_string(i);
        Desc.Name        = Name.c_str();
        pDevice->CreateTexture(Desc, nullptr, &pTextures[i]);
        ASSERT_NE(pTextures[i], nullptr);
    }

    constexpr size_t kNumIterations = 10;
    for (size_t i = 0; i < kNumIterations; ++i)
    {
        RefCntAutoPtr<IGPUUploadManager> pUploadManager;
        // Use small page size to make the uploaded attempt to create a new page and block in ScheduleTextureUpdate
        GPUUploadManagerCreateInfo CreateInfo{pDevice, pContext, 1024, 2048};
        CreateGPUUploadManager(CreateInfo, &pUploadManager);
        ASSERT_TRUE(pUploadManager != nullptr);

        const size_t             kNumThreads = pTextures.size();
        std::vector<std::thread> Threads;
        std::atomic<Uint32>      NumUpdatesRunning{0};
        Threading::Signal        AllThreadsRunningSignal;
        std::vector<Uint32>      Data(256 * 256 * 4);

        for (size_t t = 0; t < kNumThreads; ++t)
        {
            Threads.emplace_back(
                [&](ITexture* pTexture) {
                    if (NumUpdatesRunning.fetch_add(1) == kNumThreads - 1)
                    {
                        AllThreadsRunningSignal.Trigger();
                    }

                    ScheduleTextureUpdateInfo UpdateInfo;
                    UpdateInfo.pSrcData    = Data.data();
                    UpdateInfo.Stride      = 256 * 4;
                    UpdateInfo.pDstTexture = pTexture;
                    UpdateInfo.DstMipLevel = 0;
                    UpdateInfo.DstSlice    = 0;
                    // Set a large box to make the manager request a new large page for update and block in ScheduleTextureUpdate.
                    UpdateInfo.DstBox = {0, 256, 0, 256};
                    pUploadManager->ScheduleTextureUpdate(UpdateInfo);

                    NumUpdatesRunning.fetch_sub(1);
                },
                pTextures[t]);
        }

        AllThreadsRunningSignal.Wait();

        // Wait for some time to ensure that ScheduleTextureUpdate starts.
        std::this_thread::sleep_for(10ms);
        EXPECT_EQ(NumUpdatesRunning.load(), kNumThreads) << "All threads should be running updates because RenderThreadUpdate() was not called";

        if (i == kNumIterations - 1)
        {
            LogUploadManagerStats(pUploadManager);
        }

        pUploadManager.Release();

        for (std::thread& thread : Threads)
        {
            thread.join();
        }
        EXPECT_EQ(NumUpdatesRunning.load(), 0u);

        pContext->Flush();
        pContext->FinishFrame();
        pDevice->ReleaseStaleResources();
    }
}

} // namespace
