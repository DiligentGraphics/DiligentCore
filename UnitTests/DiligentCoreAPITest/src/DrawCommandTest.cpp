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
void RenderDrawCommandRefenceD3D11(ISwapChain* pSwapChain);
#endif

#if D3D12_SUPPORTED
void RenderDrawCommandRefenceD3D12(ISwapChain* pSwapChain);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
void RenderDrawCommandRefenceGL(ISwapChain* pSwapChain);
#endif

#if VULKAN_SUPPORTED
void RenderDrawCommandRefenceVk(ISwapChain* pSwapChain);
#endif

#if METAL_SUPPORTED

#endif

} // namespace Testing

} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

const char* ProceduralVSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

void main(in  uint    VertId : SV_VertexID,
          out PSInput PSIn) 
{
    float4 Pos[4];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4(-0.5, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);
    Pos[3] = float4(+0.5, +0.5, 0.0, 1.0);

    float3 Col[4];
    Col[0] = float3(1.0, 0.0, 0.0);
    Col[1] = float3(0.0, 1.0, 0.0);
    Col[2] = float3(0.0, 0.0, 1.0);
    Col[3] = float3(1.0, 1.0, 1.0);

    PSIn.Pos   = Pos[VertId];
    PSIn.Color = Col[VertId];
}
)";

const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = float4(PSIn.Color.rgb, 1.0);
}
)";

class DrawCommandTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
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
                case DeviceType::D3D11:
                    RenderDrawCommandRefenceD3D11(pSwapChain);
                    break;
#endif

#if D3D12_SUPPORTED
                case DeviceType::D3D12:
                    RenderDrawCommandRefenceD3D12(pSwapChain);
                    break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
                case DeviceType::OpenGL:
                case DeviceType::OpenGLES:
                    RenderDrawCommandRefenceGL(pSwapChain);
                    break;

#endif

#if VULKAN_SUPPORTED
                case DeviceType::Vulkan:
                    RenderDrawCommandRefenceVk(pSwapChain);
                    break;

#endif

                default:
                    LOG_ERROR_AND_THROW("Unsupported device type");
            }

            pTestingSwapChain->TakeSnapshot();
        }
        TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

        PipelineStateDesc PSODesc;
        PSODesc.Name = "Procedural triangle PSO";

        PSODesc.IsComputePipeline                             = false;
        PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
        PSODesc.GraphicsPipeline.RTVFormats[0]                = pSwapChain->GetDesc().ColorBufferFormat;
        PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.UseCombinedTextureSamplers = true;

        RefCntAutoPtr<IShader> pProceduralVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Draw command test procedural vertex shader";
            ShaderCI.Source          = ProceduralVSSource;
            pDevice->CreateShader(ShaderCI, &pProceduralVS);
            ASSERT_NE(pProceduralVS, nullptr);
        }

        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Draw command test pixel shader";
            ShaderCI.Source          = PSSource;
            pDevice->CreateShader(ShaderCI, &pPS);
            ASSERT_NE(pPS, nullptr);
        }

        PSODesc.GraphicsPipeline.pVS = pProceduralVS;
        PSODesc.GraphicsPipeline.pPS = pPS;
        pDevice->CreatePipelineState(PSODesc, &sm_pDrawProceduralPSO);
        ASSERT_NE(sm_pDrawProceduralPSO, nullptr);
    }

    static void TearDownTestSuite()
    {
        sm_pDrawProceduralPSO.Release();

        auto* pEnv = TestingEnvironment::GetInstance();
        pEnv->Reset();
    }

    static void SetRenderTargets(IPipelineState* pPSO)
    {
        auto* pEnv       = TestingEnvironment::GetInstance();
        auto* pContext   = pEnv->GetDeviceContext();
        auto* pSwapChain = pEnv->GetSwapChain();

        ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
        pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const float ClearColor[] = {0.f, 0.f, 0.f, 0.0f};
        pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        pContext->SetPipelineState(pPSO);
        // Commit shader resources. We don't really have any resources, this call also sets the shaders in OpenGL backend.
        pContext->CommitShaderResources(nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    static void Present()
    {
        auto* pEnv       = TestingEnvironment::GetInstance();
        auto* pSwapChain = pEnv->GetSwapChain();
        auto* pContext   = pEnv->GetDeviceContext();

        pSwapChain->Present();

        pContext->InvalidateState();
    }

    static RefCntAutoPtr<IPipelineState> sm_pDrawProceduralPSO;
};

RefCntAutoPtr<IPipelineState> DrawCommandTest::sm_pDrawProceduralPSO;

TEST_F(DrawCommandTest, DrawProcedural)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDrawProceduralPSO);

    DrawAttribs drawAttrs{4, DRAW_FLAG_VERIFY_ALL};
    pContext->Draw(drawAttrs);

    Present();
}

} // namespace
