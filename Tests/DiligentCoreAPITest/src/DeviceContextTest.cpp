/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include <array>
#include <cstring>

#include "DynamicBuffer.hpp"
#include "GPUTestingEnvironment.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(DeviceContextTest, DebugGroups)
{
    auto* pEnv = GPUTestingEnvironment::GetInstance();
    auto* pCtx = pEnv->GetDeviceContext();

    pCtx->BeginDebugGroup("Test group");
    pCtx->InsertDebugLabel("Debug Label");
    pCtx->EndDebugGroup();

    constexpr float Color[] = {0.25, 0.5, 0.75, 1.0};
    pCtx->BeginDebugGroup("Test group with color", Color);
    pCtx->InsertDebugLabel("Debug Label with color", Color);
    pCtx->EndDebugGroup();
}

// Repeated waits cover both empty submissions and reuse of the backend's completion
// event. Check fence completion before mapping to catch query results that are
// still pending after the idle wait. Nonblocking readback ensures WaitForIdle
// itself completes each GPU copy.
TEST(DeviceContextTest, WaitForIdleReadback)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice  = pEnv->GetDevice();
    IDeviceContext*        pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    std::array<Uint32, 1024> Expected{};
    BufferDesc               Desc;
    Desc.Name      = "WaitForIdle source buffer";
    Desc.Size      = sizeof(Expected);
    Desc.Usage     = USAGE_DEFAULT;
    Desc.BindFlags = BIND_UNIFORM_BUFFER;
    RefCntAutoPtr<IBuffer> pSource;
    pDevice->CreateBuffer(Desc, nullptr, &pSource);
    ASSERT_NE(pSource, nullptr);

    Desc.Name           = "WaitForIdle readback buffer";
    Desc.Usage          = USAGE_STAGING;
    Desc.BindFlags      = BIND_NONE;
    Desc.CPUAccessFlags = CPU_ACCESS_READ;
    RefCntAutoPtr<IBuffer> pReadback;
    pDevice->CreateBuffer(Desc, nullptr, &pReadback);
    ASSERT_NE(pReadback, nullptr);

    FenceDesc FenceCI;
    FenceCI.Name = "WaitForIdle readback fence";
    RefCntAutoPtr<IFence> pFence;
    pDevice->CreateFence(FenceCI, &pFence);
    ASSERT_NE(pFence, nullptr);

    for (Uint32 Iteration = 0; Iteration < 32; ++Iteration)
    {
        SCOPED_TRACE(Iteration);
        pContext->WaitForIdle();

        for (Uint32 i = 0; i < Expected.size(); ++i)
            Expected[i] = (Iteration + 1) * 1024 + i;

        pContext->UpdateBuffer(pSource, 0, Desc.Size, Expected.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->CopyBuffer(pSource, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                             pReadback, 0, Desc.Size, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const Uint64 FenceValue = Uint64{Iteration} + 1;
        pContext->EnqueueSignal(pFence, FenceValue);
        pContext->WaitForIdle();
        EXPECT_EQ(pFence->GetCompletedValue(), FenceValue);

        void* pData = nullptr;
        pContext->MapBuffer(pReadback, MAP_READ, MAP_FLAG_DO_NOT_WAIT, pData);
        ASSERT_NE(pData, nullptr);
        std::array<Uint32, 1024> Actual{};
        std::memcpy(Actual.data(), pData, sizeof(Actual));
        pContext->UnmapBuffer(pReadback, MAP_READ);
        EXPECT_EQ(Actual, Expected);
    }
}

} // namespace
