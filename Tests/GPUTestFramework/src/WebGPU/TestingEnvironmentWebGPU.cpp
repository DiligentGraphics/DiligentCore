/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#include "WebGPU/TestingEnvironmentWebGPU.hpp"
#include "RenderDeviceWebGPU.h"
#include "DeviceContextWebGPU.h"
#include "EngineFactoryWebGPU.h"

#if !PLATFORM_EMSCRIPTEN
#    include <dawn/dawn_proc.h>
#else
#    include <emscripten.h>
#endif


namespace Diligent
{

namespace Testing
{

void CreateTestingSwapChainWebGPU(TestingEnvironmentWebGPU* pEnv,
                                  const SwapChainDesc&      SCDesc,
                                  ISwapChain**              ppSwapChain);

TestingEnvironmentWebGPU::TestingEnvironmentWebGPU(const CreateInfo&    CI,
                                                   const SwapChainDesc& SCDesc) :
    GPUTestingEnvironment{CI, SCDesc}
{
    RefCntAutoPtr<IRenderDeviceWebGPU>  pRenderDeviceWebGPU{m_pDevice, IID_RenderDeviceWebGPU};
    RefCntAutoPtr<IDeviceContextWebGPU> pDeviceContextWebGPU{GetDeviceContext(), IID_DeviceContextWebGPU};
    RefCntAutoPtr<IEngineFactoryWebGPU> pEngineFactory{m_pDevice->GetEngineFactory(), IID_EngineFactoryWebGPU};

#if !PLATFORM_EMSCRIPTEN
    dawnProcSetProcs(static_cast<const DawnProcTable*>(pEngineFactory->GetProcessTable()));
#endif
    m_wgpuDevice = pRenderDeviceWebGPU->GetWebGPUDevice();

    if (m_pSwapChain == nullptr)
        CreateTestingSwapChainWebGPU(this, SCDesc, &m_pSwapChain);
}

TestingEnvironmentWebGPU::~TestingEnvironmentWebGPU()
{
}

WGPUCommandEncoder TestingEnvironmentWebGPU::CreateCommandEncoder()
{
    WGPUCommandEncoderDescriptor wgpuCmdEncoderDesc{};
    WGPUCommandEncoder           wgpuCmdEncoder = wgpuDeviceCreateCommandEncoder(m_wgpuDevice, &wgpuCmdEncoderDesc);
    VERIFY_EXPR(wgpuCmdEncoder != nullptr);
    return wgpuCmdEncoder;
}

WGPUShaderModule TestingEnvironmentWebGPU::CreateShaderModule(const std::string& ShaderSource)
{
    WGPUShaderModuleWGSLDescriptor wgpuShaderCodeDesc{};
    wgpuShaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgpuShaderCodeDesc.code        = ShaderSource.c_str();

    WGPUShaderModuleDescriptor wgpuShaderModuleDesc{};
    wgpuShaderModuleDesc.nextInChain  = reinterpret_cast<WGPUChainedStruct*>(&wgpuShaderCodeDesc);
    WGPUShaderModule wgpuShaderModule = wgpuDeviceCreateShaderModule(m_wgpuDevice, &wgpuShaderModuleDesc);
    VERIFY_EXPR(wgpuShaderModule != nullptr);
    return wgpuShaderModule;
}

void TestingEnvironmentWebGPU::SubmitCommandEncoder(WGPUCommandEncoder wgpuCmdEncoder, bool WaitForIdle)
{
    WGPUCommandBufferDescriptor wgpuCmdBufferDesc{};
    WGPUCommandBuffer           wgpuCmdBuffer = wgpuCommandEncoderFinish(wgpuCmdEncoder, &wgpuCmdBufferDesc);
    VERIFY_EXPR(wgpuCmdBuffer != nullptr);

    WGPUQueue wgpuCmdQueue = wgpuDeviceGetQueue(m_wgpuDevice);
    VERIFY_EXPR(wgpuCmdQueue != nullptr);

    wgpuQueueSubmit(wgpuCmdQueue, 1, &wgpuCmdBuffer);
    if (WaitForIdle)
    {
        bool IsWorkDone       = false;
        auto WorkDoneCallback = [](WGPUQueueWorkDoneStatus Status, void* pUserData) {
            if (bool* pIsWorkDone = static_cast<bool*>(pUserData))
                *pIsWorkDone = Status == WGPUQueueWorkDoneStatus_Success;
            if (Status != WGPUQueueWorkDoneStatus_Success)
                DEV_ERROR("Failed wgpuQueueOnSubmittedWorkDone: ", Status);
        };

        WGPUQueue wgpuQueue = wgpuDeviceGetQueue(m_wgpuDevice);
        wgpuQueueOnSubmittedWorkDone(wgpuQueue, WorkDoneCallback, &IsWorkDone);

        while (!IsWorkDone)
        {
#if !PLATFORM_EMSCRIPTEN
            wgpuDeviceTick(m_wgpuDevice);
#endif
        }
    }
}

GPUTestingEnvironment* CreateTestingEnvironmentWebGPU(const GPUTestingEnvironment::CreateInfo& CI,
                                                      const SwapChainDesc&                     SCDesc)
{
    return new TestingEnvironmentWebGPU{CI, SCDesc};
}

} // namespace Testing

} // namespace Diligent
