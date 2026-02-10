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

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(GPUUploadManagerTest, Creation)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

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
    ASSERT_TRUE(pBuffer != nullptr);

    pContext->CopyBuffer(pBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                         pReadbackBuffer, 0, ExpectedData.size(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->WaitForIdle();

    void* pBufferData = nullptr;
    pContext->MapBuffer(pReadbackBuffer, MAP_READ, MAP_FLAG_DO_NOT_WAIT, pBufferData);
    ASSERT_NE(pBufferData, nullptr);

    EXPECT_TRUE(std::memcmp(pBufferData, ExpectedData.data(), ExpectedData.size()) == 0) << "Buffer contents do not match expected data";
}

TEST(GPUUploadManagerTest, ScheduleUpdates)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

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

    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 0, 256, &BufferData[0]);
    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 256, 256, &BufferData[256]);
    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 512, 1024, &BufferData[512]);
    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 1536, 512, &BufferData[1536]);
    pUploadManager->ScheduleBufferUpdate(pContext, pBuffer, 2048, 2048, &BufferData[2048]);
    pUploadManager->RenderThreadUpdate(pContext);

    VerifyBufferContents(pBuffer, BufferData);
}

} // namespace
