/*
 *  Copyright 2025-2026 Diligent Graphics LLC
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

#include "GPUTestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"
#include "GraphicsTypesX.hpp"
#include "FastRand.hpp"

#include "gtest/gtest.h"

#include <array>

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

namespace HLSL
{

// clang-format off
const std::string VS{
R"(
struct VSInput
{
    float4 Color  : ATTRIB0;
    uint   VertId : SV_VertexID;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    float4 Pos[6];
    Pos[0] = float4(-1.0, -0.5, 0.0, 1.0);
    Pos[1] = float4(-0.5, +0.5, 0.0, 1.0);
    Pos[2] = float4( 0.0, -0.5, 0.0, 1.0);

    Pos[3] = float4(+0.0, -0.5, 0.0, 1.0);
    Pos[4] = float4(+0.5, +0.5, 0.0, 1.0);
    Pos[5] = float4(+1.0, -0.5, 0.0, 1.0);

    PSIn.Pos   = Pos[VSIn.VertId];
    PSIn.Color = VSIn.Color;
}
)"
};

const std::string PS{
R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(in PSInput PSIn) : SV_Target
{
    return PSIn.Color;
}
)"
};

const std::string Target1PS{
R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(in PSInput PSIn) : SV_Target1
{
    return PSIn.Color;
}
)"
};

const std::string Target2PS{
R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(in PSInput PSIn) : SV_Target2
{
    return PSIn.Color;
}
)"
};

const std::string MultiTargetPS{
R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

struct PSOutput
{
	float4 Color0 : SV_Target0;
	float4 Color1 : SV_Target1;
	float4 Color2 : SV_Target2;
	float4 Color3 : SV_Target3;
	float4 Color4 : SV_Target4;
};

PSOutput main(in PSInput PSIn)
{
    PSOutput Out;
    Out.Color0 = PSIn.Color;
	Out.Color1 = PSIn.Color;
	Out.Color2 = PSIn.Color;
	Out.Color3 = PSIn.Color;
	Out.Color4 = PSIn.Color;
    return Out;
}
)"
};

// clang-format on

} // namespace HLSL


constexpr std::array<float4, 6> RefColors = {
    float4{1, 0, 0, 0.0},
    float4{0, 1, 0, 0.5},
    float4{0, 0, 1, 1.0},

    float4{0, 1, 1, 1.0},
    float4{1, 0, 1, 0.5},
    float4{1, 1, 0, 0.0},
};

class RenderTargetTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
        IRenderDevice*         pDevice    = pEnv->GetDevice();
        ISwapChain*            pSwapChain = pEnv->GetSwapChain();
        const SwapChainDesc&   SCDesc     = pSwapChain->GetDesc();

        sm_Resources.pVS = CreateShader("Render Target Test VS", HLSL::VS.c_str(), SHADER_TYPE_VERTEX);
        ASSERT_NE(sm_Resources.pVS, nullptr);

        sm_Resources.pPS = CreateShader("Render Target Test PS", HLSL::PS.c_str(), SHADER_TYPE_PIXEL);
        ASSERT_NE(sm_Resources.pPS, nullptr);

        GraphicsPipelineStateCreateInfoX PSOCreateInfo{"Render Target Test Reference"};

        InputLayoutDescX InputLayout{{0u, 0u, 4u, VT_FLOAT32}};

        PSOCreateInfo
            .AddRenderTarget(SCDesc.ColorBufferFormat)
            .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetInputLayout(InputLayout)
            .AddShader(sm_Resources.pVS)
            .AddShader(sm_Resources.pPS);
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &sm_Resources.pPSO);
        ASSERT_NE(sm_Resources.pPSO, nullptr);

        sm_Resources.pPSO->CreateShaderResourceBinding(&sm_Resources.pSRB, true);
        ASSERT_NE(sm_Resources.pSRB, nullptr);

        sm_Resources.pColorsVB = pEnv->CreateBuffer({"Render Target Test - Ref Colors", sizeof(RefColors), BIND_VERTEX_BUFFER}, RefColors.data());
        ASSERT_NE(sm_Resources.pColorsVB, nullptr);

        sm_Resources.pRT = pEnv->CreateTexture("Render Target Test - RTV", SCDesc.ColorBufferFormat, BIND_RENDER_TARGET | BIND_SHADER_RESOURCE, SCDesc.Width, SCDesc.Height);
    }

    static void TearDownTestSuite()
    {
        sm_Resources = {};

        GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
        pEnv->Reset();
    }

    void RenderReference(COLOR_MASK Mask, const float4& ClearColor)
    {
        GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
        IDeviceContext*        pContext   = pEnv->GetDeviceContext();
        ISwapChain*            pSwapChain = pEnv->GetSwapChain();

        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
        ASSERT_NE(pTestingSwapChain, nullptr);

        std::array<float4, 6> Colors = RefColors;
        for (float4& Color : Colors)
        {
            if ((Mask & COLOR_MASK_RED) == 0)
                Color.r = ClearColor.r;
            if ((Mask & COLOR_MASK_GREEN) == 0)
                Color.g = ClearColor.g;
            if ((Mask & COLOR_MASK_BLUE) == 0)
                Color.b = ClearColor.b;
            if ((Mask & COLOR_MASK_ALPHA) == 0)
                Color.a = ClearColor.a;
        }

        RefCntAutoPtr<IBuffer> pColorsVB = pEnv->CreateBuffer({"Render Target Test - Ref Colors", sizeof(Colors), BIND_VERTEX_BUFFER}, Colors.data());

        ITextureView* pRTVs[] = {sm_Resources.pRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)};
        pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->ClearRenderTarget(pRTVs[0], ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        IBuffer* pVBs[] = {pColorsVB};
        pContext->SetVertexBuffers(0, _countof(pVBs), pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        pContext->SetPipelineState(sm_Resources.pPSO);
        pContext->CommitShaderResources(sm_Resources.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->Draw({6, DRAW_FLAG_VERIFY_ALL});

        StateTransitionDesc Barrier{sm_Resources.pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceState(Barrier);

        pContext->Flush();
        pContext->WaitForIdle();

        pTestingSwapChain->TakeSnapshot(sm_Resources.pRT);

        pContext->InvalidateState();
    }

    static RefCntAutoPtr<IShader> CreateShader(const char* Name, const char* Source, SHADER_TYPE Type)
    {
        GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
        IRenderDevice*         pDevice = pEnv->GetDevice();

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

        ShaderCI.Desc       = {Name, Type, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.Source     = Source;

        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        return pShader;
    }

    struct Resources
    {
        RefCntAutoPtr<IPipelineState>         pPSO;
        RefCntAutoPtr<IShader>                pVS;
        RefCntAutoPtr<IShader>                pPS;
        RefCntAutoPtr<IShaderResourceBinding> pSRB;
        RefCntAutoPtr<IBuffer>                pColorsVB;
        RefCntAutoPtr<ITexture>               pRT;
    };
    static Resources     sm_Resources;
    static FastRandFloat sm_Rnd;
};

RenderTargetTest::Resources RenderTargetTest::sm_Resources;
FastRandFloat               RenderTargetTest::sm_Rnd{31, 0.f, 1.f};

TEST_F(RenderTargetTest, RenderTargetWriteMask)
{
    GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice    = pEnv->GetDevice();
    IDeviceContext*        pContext   = pEnv->GetDeviceContext();
    ISwapChain*            pSwapChain = pEnv->GetSwapChain();
    const SwapChainDesc&   SCDesc     = pSwapChain->GetDesc();

    for (COLOR_MASK Mask : {COLOR_MASK_RED, COLOR_MASK_GREEN, COLOR_MASK_BLUE, COLOR_MASK_ALPHA, COLOR_MASK_ALL})
    {
        float4 ClearColor{sm_Rnd(), sm_Rnd(), sm_Rnd(), sm_Rnd()};

        RenderReference(Mask, ClearColor);

        ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
        pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->ClearRenderTarget(pRTVs[0], ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        RefCntAutoPtr<IPipelineState> pPSO;
        {
            GraphicsPipelineStateCreateInfoX PSOCreateInfo{"RenderTargetTest.RenderTargetWriteMask"};

            InputLayoutDescX InputLayout{{0u, 0u, 4u, VT_FLOAT32}};

            PSOCreateInfo
                .AddRenderTarget(SCDesc.ColorBufferFormat)
                .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                .SetInputLayout(InputLayout)
                .AddShader(sm_Resources.pVS)
                .AddShader(sm_Resources.pPS);
            PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode                          = CULL_MODE_NONE;
            PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable                     = False;
            PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].RenderTargetWriteMask = Mask;

            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
            ASSERT_NE(pPSO, nullptr);
        }

        IBuffer* pVBs[] = {sm_Resources.pColorsVB};
        pContext->SetVertexBuffers(0, _countof(pVBs), pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

        pContext->SetPipelineState(pPSO);
        pContext->CommitShaderResources(sm_Resources.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->Draw({6, DRAW_FLAG_VERIFY_ALL});

        pSwapChain->Present();
    }
}

TEST_F(RenderTargetTest, MultipleRenderTargetWriteMasks)
{
    GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice    = pEnv->GetDevice();
    IDeviceContext*        pContext   = pEnv->GetDeviceContext();
    ISwapChain*            pSwapChain = pEnv->GetSwapChain();
    const SwapChainDesc&   SCDesc     = pSwapChain->GetDesc();

    RefCntAutoPtr<IShader> pPS = CreateShader("RenderTargetTest.MultipleRenderTargetWriteMasks PS", HLSL::MultiTargetPS.c_str(), SHADER_TYPE_PIXEL);
    ASSERT_NE(pPS, nullptr);

    const std::array<COLOR_MASK, 5> ColorMasks = {COLOR_MASK_RED, COLOR_MASK_GREEN, COLOR_MASK_BLUE, COLOR_MASK_ALPHA, COLOR_MASK_ALL};

    RefCntAutoPtr<IPipelineState> pPSO;
    {
        GraphicsPipelineStateCreateInfoX PSOCreateInfo{"RenderTargetTest.RenderTargetWriteMask"};

        InputLayoutDescX InputLayout{{0u, 0u, 4u, VT_FLOAT32}};

        PSOCreateInfo
            .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetInputLayout(InputLayout)
            .AddShader(sm_Resources.pVS)
            .AddShader(pPS);
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
        for (size_t i = 0; i < ColorMasks.size(); ++i)
        {
            PSOCreateInfo.AddRenderTarget(SCDesc.ColorBufferFormat);
            PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[i].RenderTargetWriteMask = ColorMasks[i];
        }

        pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
        ASSERT_NE(pPSO, nullptr);
    }

    std::array<RefCntAutoPtr<ITexture>, ColorMasks.size()> pRTs;
    for (RefCntAutoPtr<ITexture>& pRT : pRTs)
    {
        pRT = pEnv->CreateTexture("Render Target Test - RTV", SCDesc.ColorBufferFormat, BIND_RENDER_TARGET, SCDesc.Width, SCDesc.Height);
        ASSERT_NE(pRT, nullptr);
    }

    for (Uint32 rt = 0; rt < ColorMasks.size(); ++rt)
    {
        std::array<ITextureView*, ColorMasks.size()> ppRTVs;
        std::array<float4, ColorMasks.size()>        ClearColors;
        for (size_t i = 0; i < ColorMasks.size(); ++i)
        {
            ppRTVs[i] = (i == rt) ?
                pSwapChain->GetCurrentBackBufferRTV() :
                pRTs[i]->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
            ASSERT_NE(ppRTVs[i], nullptr);

            ClearColors[i] = float4{sm_Rnd(), sm_Rnd(), sm_Rnd(), sm_Rnd()};
        }

        RenderReference(ColorMasks[rt], ClearColors[rt]);

        pContext->SetRenderTargets(static_cast<Uint32>(ppRTVs.size()), ppRTVs.data(), nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        for (size_t i = 0; i < ColorMasks.size(); ++i)
        {
            pContext->ClearRenderTarget(ppRTVs[i], ClearColors[i].Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        IBuffer* pVBs[] = {sm_Resources.pColorsVB};
        pContext->SetVertexBuffers(0, _countof(pVBs), pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

        pContext->SetPipelineState(pPSO);
        pContext->CommitShaderResources(sm_Resources.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->Draw({6, DRAW_FLAG_VERIFY_ALL});

        pSwapChain->Present();
    }
}

TEST_F(RenderTargetTest, InactiveRenderTargets)
{
    GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice    = pEnv->GetDevice();
    IDeviceContext*        pContext   = pEnv->GetDeviceContext();
    ISwapChain*            pSwapChain = pEnv->GetSwapChain();
    const SwapChainDesc&   SCDesc     = pSwapChain->GetDesc();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    ASSERT_NE(pTestingSwapChain, nullptr);

    RefCntAutoPtr<IShader> pPS1 = CreateShader("RenderTargetTest.InactiveRenderTargets - PS1", HLSL::Target1PS.c_str(), SHADER_TYPE_PIXEL);
    ASSERT_NE(pPS1, nullptr);
    RefCntAutoPtr<IShader> pPS2 = CreateShader("RenderTargetTest.InactiveRenderTargets - PS2", HLSL::Target2PS.c_str(), SHADER_TYPE_PIXEL);
    ASSERT_NE(pPS1, nullptr);

    static constexpr Uint32                               NumRenderTargets = 3;
    std::array<RefCntAutoPtr<ITexture>, NumRenderTargets> pRTs;
    for (RefCntAutoPtr<ITexture>& pRT : pRTs)
    {
        pRT = pEnv->CreateTexture("Render Target Test - RTV", SCDesc.ColorBufferFormat, BIND_RENDER_TARGET, SCDesc.Width, SCDesc.Height);
        ASSERT_NE(pRT, nullptr);
    }

    for (Uint32 rt = 0; rt < NumRenderTargets; ++rt)
    {
        RefCntAutoPtr<IPipelineState> pPSO;
        {
            GraphicsPipelineStateCreateInfoX PSOCreateInfo{"RenderTargetTest.InactiveRenderTargets"};

            InputLayoutDescX InputLayout{{0u, 0u, 4u, VT_FLOAT32}};

            PSOCreateInfo
                .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                .SetInputLayout(InputLayout)
                .AddShader(sm_Resources.pVS);
            switch (rt)
            {
                case 0:
                    PSOCreateInfo.AddShader(sm_Resources.pPS);
                    break;
                case 1:
                    PSOCreateInfo.AddShader(pPS1);
                    break;
                case 2:
                    PSOCreateInfo.AddShader(pPS2);
                    break;
                default:
                    UNEXPECTED("Unexpected render target index");
                    break;
            }
            PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
            PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
            for (size_t i = 0; i < NumRenderTargets; ++i)
            {
                PSOCreateInfo.AddRenderTarget(SCDesc.ColorBufferFormat);
                PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[i].RenderTargetWriteMask = i == rt ? COLOR_MASK_ALL : COLOR_MASK_NONE;
            }

            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
            ASSERT_NE(pPSO, nullptr);
        }

        std::array<ITextureView*, NumRenderTargets> ppRTVs;
        std::array<float4, NumRenderTargets>        ClearColors;
        for (size_t i = 0; i < NumRenderTargets; ++i)
        {
            ppRTVs[i] = (i == rt) ?
                pSwapChain->GetCurrentBackBufferRTV() :
                pRTs[i]->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
            ASSERT_NE(ppRTVs[i], nullptr);

            ClearColors[i] = float4{sm_Rnd(), sm_Rnd(), sm_Rnd(), sm_Rnd()};
        }

        RenderReference(COLOR_MASK_ALL, ClearColors[rt]);

        pContext->SetRenderTargets(static_cast<Uint32>(ppRTVs.size()), ppRTVs.data(), nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        for (size_t i = 0; i < NumRenderTargets; ++i)
        {
            pContext->ClearRenderTarget(ppRTVs[i], ClearColors[i].Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        IBuffer* pVBs[] = {sm_Resources.pColorsVB};
        pContext->SetVertexBuffers(0, _countof(pVBs), pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

        pContext->SetPipelineState(pPSO);

        pContext->CommitShaderResources(sm_Resources.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->Draw({6, DRAW_FLAG_VERIFY_ALL});

        pSwapChain->Present();

        for (size_t i = 0; i < NumRenderTargets; ++i)
        {
            if (i == rt)
                continue;

            ITextureView* pRTV = sm_Resources.pRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
            pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            pContext->ClearRenderTarget(pRTV, ClearColors[i].Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            StateTransitionDesc Barrier{sm_Resources.pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
            pContext->TransitionResourceState(Barrier);

            pContext->Flush();
            pContext->WaitForIdle();

            pTestingSwapChain->TakeSnapshot(sm_Resources.pRT);

            pContext->InvalidateState();

            pTestingSwapChain->CompareWithSnapshot(pRTs[i]);
        }
    }
}

} // namespace
