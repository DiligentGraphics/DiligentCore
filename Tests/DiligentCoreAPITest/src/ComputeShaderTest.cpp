/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "TestingEnvironment.h"
#include "TestingSwapChainBase.h"

#include "gtest/gtest.h"

namespace Diligent
{

namespace Testing
{

#if D3D11_SUPPORTED
void ComputeShaderReferenceD3D11(ISwapChain* pSwapChain);
#endif

#if D3D12_SUPPORTED
void ComputeShaderReferenceD3D12(ISwapChain* pSwapChain);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
void ComputeShaderReferenceGL(ISwapChain* pSwapChain);
#endif

#if VULKAN_SUPPORTED
void ComputeShaderReferenceVk(ISwapChain* pSwapChain);
#endif

#if METAL_SUPPORTED

#endif

} // namespace Testing

} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

const char* CSSource = R"(
RWTexture2D</*format=rgba8*/ float4> g_tex2DUAV;

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 ui2Dim;
	g_tex2DUAV.GetDimensions(ui2Dim.x, ui2Dim.y);
	if (DTid.x >= ui2Dim.x || DTid.y >= ui2Dim.y)
        return;

	g_tex2DUAV[DTid.xy] = float4(float2(DTid.xy % 256u) / 256.0, 0.0, 1.0);
}
)";


TEST(ComputeShaderTest, FillTexture)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceCaps().bComputeShadersSupported)
    {
        GTEST_SKIP() << "Compute shaders are not supported by this device";
    }

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "Compute shader test requires testing swap chain";
    }

    pContext->Flush();
    pContext->InvalidateState();

    auto deviceType = pDevice->GetDeviceCaps().DevType;
    switch (deviceType)
    {
#if D3D11_SUPPORTED
        case DeviceType::D3D11:
            ComputeShaderReferenceD3D11(pSwapChain);
            break;
#endif

#if D3D12_SUPPORTED
        case DeviceType::D3D12:
            ComputeShaderReferenceD3D12(pSwapChain);
            break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case DeviceType::OpenGL:
        case DeviceType::OpenGLES:
            ComputeShaderReferenceGL(pSwapChain);
            break;

#endif

#if VULKAN_SUPPORTED
        case DeviceType::Vulkan:
            ComputeShaderReferenceVk(pSwapChain);
            break;

#endif

        default:
            LOG_ERROR_AND_THROW("Unsupported device type");
    }

    pTestingSwapChain->TakeSnapshot();

    TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = true;
    ShaderCI.Desc.ShaderType            = SHADER_TYPE_COMPUTE;
    ShaderCI.EntryPoint                 = "main";
    ShaderCI.Desc.Name                  = "Compute shader test";
    ShaderCI.Source                     = CSSource;
    RefCntAutoPtr<IShader> pCS;
    pDevice->CreateShader(ShaderCI, &pCS);
    ASSERT_NE(pCS, nullptr);

    PipelineStateDesc PSODesc;
    PSODesc.Name                = "Compute shader test";
    PSODesc.IsComputePipeline   = true;
    PSODesc.ComputePipeline.pCS = pCS;

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreatePipelineState(PSODesc, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    const auto& SCDesc = pSwapChain->GetDesc();

    pPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "g_tex2DUAV")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPSO->CreateShaderResourceBinding(&pSRB, true);
    ASSERT_NE(pSRB, nullptr);

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DispatchComputeAttribs DispatchAttribs;
    DispatchAttribs.ThreadGroupCountX = (SCDesc.Width + 15) / 16;
    DispatchAttribs.ThreadGroupCountY = (SCDesc.Height + 15) / 16;
    pContext->DispatchCompute(DispatchAttribs);

    pSwapChain->Present();
}

} // namespace
