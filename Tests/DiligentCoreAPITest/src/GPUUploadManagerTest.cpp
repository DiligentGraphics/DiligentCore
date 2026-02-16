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

    MAP_FLAGS MapFlags = (DeviceType == RENDER_DEVICE_TYPE_D3D12 || DeviceType == RENDER_DEVICE_TYPE_VULKAN || DeviceType == RENDER_DEVICE_TYPE_WEBGPU) ?
        MAP_FLAG_DO_NOT_WAIT :
        MAP_FLAG_NONE;

    pContext->MapBuffer(pReadbackBuffer, MAP_READ, MapFlags, pBufferData);
    ASSERT_NE(pBufferData, nullptr);
    EXPECT_TRUE(std::memcmp(pBufferData, ExpectedData.data(), ExpectedData.size()) == 0) << "Buffer contents do not match expected data";
    pContext->UnmapBuffer(pReadbackBuffer, MAP_READ);
}

TEST(GPUUploadManagerTest, ScheduleUpdates)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 1024};
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

    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 0, 0, nullptr);
    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 0, 256, &BufferData[0]);
    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 256, 256, &BufferData[256]);
    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 512, 1024, &BufferData[512]);
    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 1536, 512, &BufferData[1536]);
    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 2048, 2048, &BufferData[2048]);
    pUploadManager->RenderThreadUpdate(pContext);
    pUploadManager->RenderThreadUpdate(pContext);

    VerifyBufferContents(pBuffer, BufferData);
}

TEST(GPUUploadManagerTest, ParallelUpdates)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 16384};
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
                    pUploadManager->ScheduleBufferUpdate(nullptr, pBuffer, Offset, UpdateSize, &BufferData[Offset]);
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

    GPUUploadManagerStats Stats;
    pUploadManager->GetStats(Stats);
    LOG_INFO_MESSAGE("GPU Upload Manager Stats:"
                     "\n    NumPages                   ",
                     Stats.NumPages,
                     "\n    NumFreePages               ", Stats.NumFreePages,
                     "\n    NumInFlightPages           ", Stats.NumInFlightPages,
                     "\n    PeakTotalPendingUpdateSize ", Stats.PeakTotalPendingUpdateSize,
                     "\n    PeakUpdateSize             ", Stats.PeakUpdateSize);

    VerifyBufferContents(pBuffer, BufferData);
}


TEST(GPUUploadManagerTest, DestroyWhileUpdatesAreRunning)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice, pContext, 1024};
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
                pUploadManager->ScheduleBufferUpdate(nullptr, nullptr, 0, 2048, nullptr);
                NumUpdatesRunning.fetch_sub(1);
            });
    }

    AllThreadsRunningSignal.Wait();

    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(NumUpdatesRunning.load(), kNumThreads) << "All threads should be running updates because RenderThreadUpdate() was not called";

    pUploadManager.Release();

    for (std::thread& thread : Threads)
    {
        thread.join();
    }
    EXPECT_EQ(NumUpdatesRunning.load(), 0u);
}


TEST(GPUUploadManagerTest, CreateWithNullContext)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IGPUUploadManager> pUploadManager;
    GPUUploadManagerCreateInfo       CreateInfo{pDevice};
    CreateInfo.PageSize         = 1024;
    CreateInfo.InitialPageCount = 8;
    CreateGPUUploadManager(CreateInfo, &pUploadManager);
    ASSERT_TRUE(pUploadManager != nullptr);

    const size_t kNumThreads = std::max(2u, std::thread::hardware_concurrency() - 1);
    LOG_INFO_MESSAGE("Number of threads: ", kNumThreads);

    constexpr size_t kNumUpdatesPerThread = 16;
    constexpr size_t kUpdateSize          = 2048;

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
                    Uint32 Offset = CurrOffset.fetch_add(kUpdateSize);
                    pUploadManager->ScheduleBufferUpdate(nullptr, pBuffer, Offset, kUpdateSize, &BufferData[Offset]);
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

    GPUUploadManagerStats Stats;
    pUploadManager->GetStats(Stats);
    LOG_INFO_MESSAGE("GPU Upload Manager Stats:"
                     "\n    NumPages                   ",
                     Stats.NumPages,
                     "\n    NumFreePages               ", Stats.NumFreePages,
                     "\n    NumInFlightPages           ", Stats.NumInFlightPages,
                     "\n    PeakTotalPendingUpdateSize ", Stats.PeakTotalPendingUpdateSize,
                     "\n    PeakUpdateSize             ", Stats.PeakUpdateSize);
}

} // namespace
