/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include "TestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"

#include "gtest/gtest.h"

extern "C"
{
    int TestSwapChainCInterface(void* pSwapChain);
};

namespace Diligent
{

namespace Testing
{

#if D3D11_SUPPORTED
void ClearRenderTargetReferenceD3D11(ISwapChain* pSwapChain);
#endif

#if D3D12_SUPPORTED
void ClearRenderTargetReferenceD3D12(ISwapChain* pSwapChain);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
void ClearRenderTargetReferenceGL(ISwapChain* pSwapChain);
#endif

#if VULKAN_SUPPORTED
void ClearRenderTargetReferenceVk(ISwapChain* pSwapChain);
#endif

#if METAL_SUPPORTED

#endif

} // namespace Testing

} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(ClearRenderTargetTest, Clear)
{
    auto* pEnv       = TestingEnvironment::GetInstance();
    auto* pDevice    = pEnv->GetDevice();
    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pConext    = pEnv->GetDeviceContext();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
    if (pTestingSwapChain)
    {
        pConext->Flush();
        pConext->InvalidateState();

        auto deviceType = pDevice->GetDeviceCaps().DevType;
        switch (deviceType)
        {
#if D3D11_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D11:
                ClearRenderTargetReferenceD3D11(pSwapChain);
                break;
#endif

#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
                ClearRenderTargetReferenceD3D12(pSwapChain);
                break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
                ClearRenderTargetReferenceGL(pSwapChain);
                break;

#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                ClearRenderTargetReferenceVk(pSwapChain);
                break;

#endif

            default:
                LOG_ERROR_AND_THROW("Unsupported device type");
        }

        pTestingSwapChain->TakeSnapshot();
    }
    TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

    auto* pContext = pEnv->GetDeviceContext();

    ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    float ClearColor[] = {0.25f, 0.5f, 0.75f, 1.0f};
    pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pSwapChain->Present();

    TestSwapChainCInterface(pSwapChain);
}

} // namespace
