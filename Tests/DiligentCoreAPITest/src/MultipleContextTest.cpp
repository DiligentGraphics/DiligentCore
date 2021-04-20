/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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
#include "BasicMath.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

// clang-format off
const std::string MultipleContextTest_QuadVS{R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
    float2 UV    : TEXCOORD;
};

void main(in uint vid : SV_VertexID,
          out PSInput PSIn) 
{
    float2 uv  = float2(vid & 1, vid >> 1);
    PSIn.Pos   = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    PSIn.UV    = float2(uv.x, 1.0 - uv.y);
    PSIn.Color = float3(vid & 1, (vid + 1) & 1, (vid + 2) & 1);
}
)"};

const std::string MultipleContextTest_ColorPS{R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
    float2 UV    : TEXCOORD;
};

float4 main(in PSInput PSIn) : SV_Target
{
    return float4(PSIn.Color.rgb, 1.0);
}
)"};

const std::string MultipleContextTest_TexturedPS{R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
    float2 UV    : TEXCOORD;
};

Texture2D<float4> g_Texture;
SamplerState      g_Texture_sampler;

float4 main(in PSInput PSIn) : SV_Target
{
    return g_Texture.Sample(g_Texture_sampler, PSIn.UV, 0);
}
)"};

const std::string MultipleContextTest_CS{R"(
RWTexture2D<float4> g_DstTexture;
Texture2D<float4>   g_SrcTexture;

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 Dim;
    g_DstTexture.GetDimensions(Dim.x, Dim.y);
    if (DTid.x >= Dim.x || DTid.y >= Dim.y)
        return;

    float2 uv  = float2(DTid.xy) / float2(Dim) * 10.0;
    float4 col = float(0.0).xxxx;

    col.r = sin(uv.x) * cos(uv.y);
    col.g = frac(uv.x) * frac(uv.y);

    float4 src = g_SrcTexture.Load(DTid);

    g_DstTexture[DTid.xy] = col + src * 0.00005;
}
)"};
// clang-format on


class MultipleContextTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        auto* pEnv       = TestingEnvironment::GetInstance();
        auto* pDevice    = pEnv->GetDevice();
        auto* pSwapChain = pEnv->GetSwapChain();

        if (pEnv->GetNumImmediateContexts() == 1)
            GTEST_SKIP() << "Multiple contexts is not supported by this device";

        TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

        // Graphics PSO
        {
            GraphicsPipelineStateCreateInfo PSOCreateInfo;
            auto&                           PSODesc          = PSOCreateInfo.PSODesc;
            auto&                           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

            PSODesc.Name         = "Multiple context test - graphics PSO";
            PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

            PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

            GraphicsPipeline.NumRenderTargets             = 1;
            GraphicsPipeline.RTVFormats[0]                = pSwapChain->GetDesc().ColorBufferFormat;
            GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
            GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

            RefCntAutoPtr<IShader> pVS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
                ShaderCI.EntryPoint      = "main";
                ShaderCI.Desc.Name       = "Multiple context test - VS";
                ShaderCI.Source          = MultipleContextTest_QuadVS.c_str();
                pDevice->CreateShader(ShaderCI, &pVS);
                ASSERT_NE(pVS, nullptr);
            }

            RefCntAutoPtr<IShader> pPS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                ShaderCI.EntryPoint      = "main";
                ShaderCI.Desc.Name       = "Multiple context test - PS";
                ShaderCI.Source          = MultipleContextTest_ColorPS.c_str();
                pDevice->CreateShader(ShaderCI, &pPS);
                ASSERT_NE(pPS, nullptr);
            }

            PSOCreateInfo.pVS = pVS;
            PSOCreateInfo.pPS = pPS;
            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &sm_pDrawPSO);
            ASSERT_NE(sm_pDrawPSO, nullptr);

            SamplerDesc SamLinearWrapDesc{
                FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
                TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP};
            ImmutableSamplerDesc ImmutableSamplers[] =
                {
                    {SHADER_TYPE_PIXEL, "g_Texture_sampler", SamLinearWrapDesc} //
                };
            PSODesc.ResourceLayout.ImmutableSamplers    = ImmutableSamplers;
            PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImmutableSamplers);
            PSODesc.ResourceLayout.DefaultVariableType  = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

            RefCntAutoPtr<IShader> pTexturedPS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                ShaderCI.EntryPoint      = "main";
                ShaderCI.Desc.Name       = "Multiple context test - textured PS";
                ShaderCI.Source          = MultipleContextTest_TexturedPS.c_str();
                pDevice->CreateShader(ShaderCI, &pTexturedPS);
                ASSERT_NE(pTexturedPS, nullptr);
            }

            PSODesc.Name      = "Multiple context test - textured graphics PSO";
            PSOCreateInfo.pPS = pTexturedPS;
            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &sm_pDrawTexturedPSO);
            ASSERT_NE(sm_pDrawTexturedPSO, nullptr);
        }

        // Compute PSO
        {
            ComputePipelineStateCreateInfo PSOCreateInfo;

            RefCntAutoPtr<IShader> pCS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
                ShaderCI.EntryPoint      = "main";
                ShaderCI.Desc.Name       = "Multiple context test - CS";
                ShaderCI.Source          = MultipleContextTest_CS.c_str();
                pDevice->CreateShader(ShaderCI, &pCS);
                ASSERT_NE(pCS, nullptr);
            }

            PSOCreateInfo.PSODesc.Name = "Multiple context test - compute PSO";
            PSOCreateInfo.pCS          = pCS;

            PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

            pDevice->CreateComputePipelineState(PSOCreateInfo, &sm_pCompPSO);
            ASSERT_NE(sm_pCompPSO, nullptr);
        }

        sm_pDrawTexturedPSO->CreateShaderResourceBinding(&sm_pDrawTexturedSRB, true);
        ASSERT_NE(sm_pDrawTexturedSRB, nullptr);
        sm_pCompPSO->CreateShaderResourceBinding(&sm_pCompSRB, true);
        ASSERT_NE(sm_pCompSRB, nullptr);
    }

    static void TearDownTestSuite()
    {
        sm_pDrawPSO.Release();
        sm_pDrawTexturedPSO.Release();
        sm_pCompPSO.Release();

        auto* pEnv = TestingEnvironment::GetInstance();
        pEnv->Reset();
    }

    static RefCntAutoPtr<ITexture> CreateTexture(BIND_FLAGS Flags, USAGE Usage, Uint64 QueueMask, const char* Name)
    {
        auto*       pEnv       = TestingEnvironment::GetInstance();
        auto*       pDevice    = pEnv->GetDevice();
        auto*       pSwapChain = pEnv->GetSwapChain();
        const auto& SCDesc     = pSwapChain->GetDesc();

        TextureDesc Desc;
        Desc.Name             = Name;
        Desc.Type             = RESOURCE_DIM_TEX_2D;
        Desc.Width            = SCDesc.Width;
        Desc.Height           = SCDesc.Height;
        Desc.Format           = TEX_FORMAT_RGBA8_UNORM;
        Desc.Usage            = Usage;
        Desc.BindFlags        = Flags;
        Desc.CommandQueueMask = QueueMask;

        RefCntAutoPtr<ITexture> pTexture;
        pDevice->CreateTexture(Desc, nullptr, &pTexture);
        return pTexture;
    }

    static RefCntAutoPtr<IPipelineState> sm_pDrawPSO;
    static RefCntAutoPtr<IPipelineState> sm_pDrawTexturedPSO;
    static RefCntAutoPtr<IPipelineState> sm_pCompPSO;

    static RefCntAutoPtr<IShaderResourceBinding> sm_pDrawTexturedSRB;
    static RefCntAutoPtr<IShaderResourceBinding> sm_pCompSRB;
};

RefCntAutoPtr<IPipelineState> MultipleContextTest::sm_pDrawPSO;
RefCntAutoPtr<IPipelineState> MultipleContextTest::sm_pDrawTexturedPSO;
RefCntAutoPtr<IPipelineState> MultipleContextTest::sm_pCompPSO;

RefCntAutoPtr<IShaderResourceBinding> MultipleContextTest::sm_pDrawTexturedSRB;
RefCntAutoPtr<IShaderResourceBinding> MultipleContextTest::sm_pCompSRB;


TEST_F(MultipleContextTest, GraphicsAndComputeQueue)
{
    auto*           pEnv         = TestingEnvironment::GetInstance();
    auto*           pDevice      = pEnv->GetDevice();
    auto*           pSwapChain   = pEnv->GetSwapChain();
    const auto&     SCDesc       = pSwapChain->GetDesc();
    IDeviceContext* pGraphicsCtx = nullptr;
    IDeviceContext* pComputeCtx  = nullptr;

    for (Uint32 CtxInd = 0; CtxInd < pEnv->GetNumImmediateContexts(); ++CtxInd)
    {
        auto*       Ctx  = pEnv->GetDeviceContext(CtxInd);
        const auto& Desc = Ctx->GetDesc();

        if (!pGraphicsCtx && (Desc.ContextType & CONTEXT_TYPE_GRAPHICS) == CONTEXT_TYPE_GRAPHICS)
            pGraphicsCtx = Ctx;

        if (!pComputeCtx && ((Desc.ContextType & CONTEXT_TYPE_COMPUTE) == CONTEXT_TYPE_COMPUTE || ((Desc.ContextType & CONTEXT_TYPE_GRAPHICS) == CONTEXT_TYPE_GRAPHICS && Ctx != pGraphicsCtx)))
            pComputeCtx = Ctx;
    }

    if (!pGraphicsCtx || !pComputeCtx)
    {
        GTEST_SKIP() << "Compute queue is not supported by this device";
    }

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);

    // Draw reference in single queue
    {
        auto pBackBufferUAV = pTestingSwapChain->GetCurrentBackBufferUAV();
        auto pTexture       = CreateTexture(BIND_SHADER_RESOURCE | BIND_RENDER_TARGET, USAGE_DEFAULT, (1ull << pGraphicsCtx->GetDesc().CommandQueueId), "Ref-RenderTarget");
        ASSERT_NE(pTexture, nullptr);

        // graphics pass
        {
            ITextureView* pRTVs[] = {pTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)};
            pGraphicsCtx->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            const float ClearColor[4] = {1.0f, 0.0f, 0.0f, 0.0f};
            pGraphicsCtx->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            pGraphicsCtx->SetPipelineState(sm_pDrawPSO);
            pGraphicsCtx->Draw(DrawAttribs{4, DRAW_FLAG_VERIFY_STATES | DRAW_FLAG_VERIFY_DRAW_ATTRIBS});

            pGraphicsCtx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
        }

        // compute pass
        {
            sm_pCompSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_SrcTexture")->Set(pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            sm_pCompSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_DstTexture")->Set(pBackBufferUAV);

            pGraphicsCtx->SetPipelineState(sm_pCompPSO);
            pGraphicsCtx->CommitShaderResources(sm_pCompSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            pGraphicsCtx->DispatchCompute(DispatchComputeAttribs{SCDesc.Width, SCDesc.Height, 1});

            // Transition to CopySrc state to use in TakeSnapshot()
            StateTransitionDesc Barrier{pBackBufferUAV->GetTexture(), RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, true};
            pGraphicsCtx->TransitionResourceStates(1, &Barrier);
        }

        pGraphicsCtx->Flush();
        pGraphicsCtx->FinishFrame();
        pTestingSwapChain->TakeSnapshot(pBackBufferUAV->GetTexture());
    }


    RefCntAutoPtr<IFence> pGraphicsFence;
    RefCntAutoPtr<IFence> pComputeFence;
    {
        FenceDesc Desc;
        Desc.Name = "Graphics sync";
        pDevice->CreateFence(Desc, &pGraphicsFence);
        ASSERT_NE(pGraphicsFence, nullptr);

        Desc.Name = "Compute sync";
        pDevice->CreateFence(Desc, &pComputeFence);
        ASSERT_NE(pComputeFence, nullptr);
    }

    const Uint64 QueueMask   = (1ull << pGraphicsCtx->GetDesc().CommandQueueId) | (1ull << pComputeCtx->GetDesc().CommandQueueId);
    auto         pTextureRT  = CreateTexture(BIND_SHADER_RESOURCE | BIND_RENDER_TARGET, USAGE_DEFAULT, QueueMask, "TextureRT");
    auto         pTextureUAV = CreateTexture(BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS, USAGE_DEFAULT, QueueMask, "TextureUAV");
    ASSERT_NE(pTextureRT, nullptr);
    ASSERT_NE(pTextureUAV, nullptr);

    // disable implicit state transitions
    pTextureRT->SetState(RESOURCE_STATE_UNKNOWN);
    pTextureUAV->SetState(RESOURCE_STATE_UNKNOWN);

    Uint64 GraphicsFenceValue = 11;
    Uint64 ComputeFenceValue  = 22;

    // graphics pass
    {
        const StateTransitionDesc Barriers1[] = {
            {pTextureRT, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_RENDER_TARGET} //
        };
        pGraphicsCtx->TransitionResourceStates(_countof(Barriers1), Barriers1);

        ITextureView* pRTVs[] = {pTextureRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)};
        pGraphicsCtx->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

        const float ClearColor[4] = {0.0f, 1.0f, 0.0f, 0.0f};
        pGraphicsCtx->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_NONE);

        pGraphicsCtx->SetPipelineState(sm_pDrawPSO);
        pGraphicsCtx->Draw(DrawAttribs{4, DRAW_FLAG_NONE});

        pGraphicsCtx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

        const StateTransitionDesc Barriers2[] = {
            {pTextureRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE} //
        };
        pGraphicsCtx->TransitionResourceStates(_countof(Barriers2), Barriers2);

        pGraphicsCtx->SignalFence(pGraphicsFence, GraphicsFenceValue);
        pGraphicsCtx->Flush();
    }

    // compute pass
    {
        pComputeCtx->DeviceWaitForFence(pGraphicsFence, GraphicsFenceValue);

        sm_pCompSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_SrcTexture")->Set(pTextureRT->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        sm_pCompSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_DstTexture")->Set(pTextureUAV->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

        const StateTransitionDesc Barriers[] = {
            {pTextureUAV, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_UNORDERED_ACCESS} //
        };
        pComputeCtx->TransitionResourceStates(_countof(Barriers), Barriers);

        pComputeCtx->SetPipelineState(sm_pCompPSO);
        pComputeCtx->CommitShaderResources(sm_pCompSRB, RESOURCE_STATE_TRANSITION_MODE_NONE);
        pComputeCtx->DispatchCompute(DispatchComputeAttribs{SCDesc.Width, SCDesc.Height, 1});

        pComputeCtx->SignalFence(pComputeFence, ComputeFenceValue);
        pComputeCtx->Flush();
    }

    // present
    {
        pGraphicsCtx->DeviceWaitForFence(pComputeFence, ComputeFenceValue);

        sm_pDrawTexturedSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pTextureUAV->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

        auto*                     pRTV       = pSwapChain->GetCurrentBackBufferRTV();
        const StateTransitionDesc Barriers[] = {
            {pRTV->GetTexture(), RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_RENDER_TARGET, true},
            {pTextureUAV, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE} //
        };
        pGraphicsCtx->TransitionResourceStates(_countof(Barriers), Barriers);

        pGraphicsCtx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

        pGraphicsCtx->SetPipelineState(sm_pDrawTexturedPSO);
        pGraphicsCtx->CommitShaderResources(sm_pDrawTexturedSRB, RESOURCE_STATE_TRANSITION_MODE_NONE);
        pGraphicsCtx->Draw(DrawAttribs{4, DRAW_FLAG_NONE});

        pGraphicsCtx->Flush();
        pSwapChain->Present();
    }

    pGraphicsCtx->FinishFrame();
    pComputeCtx->FinishFrame();

    pGraphicsFence->Wait(GraphicsFenceValue);
    pComputeFence->Wait(ComputeFenceValue);
}
} // namespace
