/*
 *  Copyright 2023 Diligent Graphics LLC
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

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

// clang-format off
const std::string ReadOnlyDepthTest_VS{
R"(
void main(in  uint VertexID : SV_VertexID,
          out float4 OutPos : SV_POSITION)
{
    float ExpectedDepth = 0.5;
#ifndef GLSL
    float GeometryDepth = ExpectedDepth;
#else
    float GeometryDepth = ExpectedDepth * 2.0 - 1.0;
#endif

    float4 Pos[4];
    Pos[0] = float4(-1.0, -1.0, GeometryDepth, 1.0);
    Pos[1] = float4(-1.0, +1.0, GeometryDepth, 1.0);
    Pos[2] = float4(+1.0, -1.0, GeometryDepth, 1.0);
    Pos[3] = float4(+1.0, +1.0, GeometryDepth, 1.0);

    OutPos = Pos[VertexID];
}
)"
};

const std::string ReadOnlyDepthTest_PSDepth{
R"(
void main() {}
)"
};

const std::string ReadOnlyDepthTest_PSColor{
R"(
Texture2D<float4> g_Input;

float4 main(float4 Pos : SV_Position) : SV_Target
{
    float depth = g_Input.Load(int3(Pos.xy, 0)).r;
    return float4(depth, depth * 0.5, 0.75, 1.0);
}
)"
};
// clang-format on

// Arbitrary clear color and depth that are never used anywhere else in this test
constexpr float ClearColor[] = {0.125f, 0.250f, 0.5f, 1.0f};
constexpr float ClearDepth   = 0.125f;

constexpr float ExpectedDepth    = 0.5f;
constexpr float ReferenceColor[] = {ExpectedDepth, ExpectedDepth * 0.5f, 0.75f, 1.0f};

constexpr TEXTURE_FORMAT RTVFormat = TEX_FORMAT_RGBA8_UNORM;

class ReadOnlyDepthTest : public testing::TestWithParam<std::tuple<int>>
{
protected:
    static void SetUpTestSuite()
    {
    }

    static void TearDownTestSuite()
    {
        auto* pEnv = GPUTestingEnvironment::GetInstance();
        pEnv->Reset();
    }

    virtual void SetUp() override
    {
        auto* pEnv       = GPUTestingEnvironment::GetInstance();
        auto* pSwapChain = pEnv->GetSwapChain();

        m_pRTV = pSwapChain->GetCurrentBackBufferRTV();
    }

    virtual void TearDown() override
    {
        m_pDepthPSO.Release();
        m_pColorPSO.Release();

        m_pDepthTexture.Release();
        m_pReadWriteDSV.Release();
        m_pReadOnlyDSV.Release();

        m_pRTV.Release();

        m_pColorSRB.Release();
    }

    void TakeSnapshot()
    {
        auto* pEnv       = GPUTestingEnvironment::GetInstance();
        auto* pSwapChain = pEnv->GetSwapChain();
        auto* pContext   = pEnv->GetDeviceContext();

        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
        if (pTestingSwapChain)
        {
            ITextureView* pRTV = pSwapChain->GetCurrentBackBufferRTV();

            // Make reference image
            pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            pContext->ClearRenderTarget(pRTV, ReferenceColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Transition to CopySrc state to use in TakeSnapshot()
            StateTransitionDesc Barrier{pRTV->GetTexture(), RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
            pContext->TransitionResourceStates(1, &Barrier);

            pContext->WaitForIdle();
            pTestingSwapChain->TakeSnapshot(pRTV->GetTexture());
        }
    }

    void InitializeDepthTexture()
    {
        auto* pEnv       = GPUTestingEnvironment::GetInstance();
        auto* pDevice    = pEnv->GetDevice();
        auto* pSwapChain = pEnv->GetSwapChain();

        TextureDesc DepthTexDesc       = pSwapChain->GetDepthBufferDSV()->GetTexture()->GetDesc();
        DepthTexDesc.Name              = "Readable depth texture";
        DepthTexDesc.Format            = GetDepthFormat();
        DepthTexDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
        DepthTexDesc.ClearValue.Format = GetDepthFormat();
        pDevice->CreateTexture(DepthTexDesc, nullptr, &m_pDepthTexture);
        VERIFY_EXPR(m_pDepthTexture);

        TextureViewDesc ReadOnlyDSVDesc;
        ReadOnlyDSVDesc.ViewType   = TEXTURE_VIEW_READ_ONLY_DEPTH_STENCIL;
        ReadOnlyDSVDesc.TextureDim = RESOURCE_DIM_TEX_2D;
        m_pDepthTexture->CreateView(ReadOnlyDSVDesc, &m_pReadOnlyDSV);
        VERIFY_EXPR(m_pReadOnlyDSV);

        m_pReadWriteDSV = m_pDepthTexture->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
        VERIFY_EXPR(m_pReadWriteDSV);
    }

    void InitializePipelineStates(IRenderPass* pDepthRenderPass, IRenderPass* pColorRenderPass)
    {
        auto* pEnv    = GPUTestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        auto& PSODesc          = PSOCreateInfo.PSODesc;
        auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

        PSODesc.Name = "Read-only depth test";

        PSODesc.PipelineType                     = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipeline.PrimitiveTopology       = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc       = {"Read only depth buffer test vertex shader", SHADER_TYPE_VERTEX, true};
            ShaderCI.EntryPoint = "main";
            ShaderCI.Source     = ReadOnlyDepthTest_VS.c_str();
            pDevice->CreateShader(ShaderCI, &pVS);
            VERIFY_EXPR(pVS);
        }

        RefCntAutoPtr<IShader> pPSDepth;
        {
            ShaderCI.Desc       = {"Read only depth buffer test pixel shader -- depth output", SHADER_TYPE_PIXEL, true};
            ShaderCI.EntryPoint = "main";
            ShaderCI.Source     = ReadOnlyDepthTest_PSDepth.c_str();
            pDevice->CreateShader(ShaderCI, &pPSDepth);
            VERIFY_EXPR(pPSDepth);
        }

        RefCntAutoPtr<IShader> pPSColor;
        {
            ShaderCI.Desc       = {"Read only depth buffer test pixel shader -- color output", SHADER_TYPE_PIXEL, true};
            ShaderCI.EntryPoint = "main";
            ShaderCI.Source     = ReadOnlyDepthTest_PSColor.c_str();
            pDevice->CreateShader(ShaderCI, &pPSColor);
            VERIFY_EXPR(pPSColor);
        }

        PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

        {
            PSODesc.Name = "Read only depth buffer test -- depth pre-pass";

            GraphicsPipeline.DepthStencilDesc.DepthEnable                     = True;
            GraphicsPipeline.DepthStencilDesc.DepthFunc                       = COMPARISON_FUNC_ALWAYS;
            GraphicsPipeline.DepthStencilDesc.DepthWriteEnable                = True;
            GraphicsPipeline.BlendDesc.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_NONE;

            InitializeRenderPassOrRenderTargets(GraphicsPipeline, TEX_FORMAT_UNKNOWN, pDepthRenderPass, false);

            PSOCreateInfo.pVS = pVS;
            PSOCreateInfo.pPS = pPSDepth;

            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pDepthPSO);
            VERIFY_EXPR(m_pDepthPSO);
        }

        {
            PSODesc.Name = "Read only depth buffer test -- color pass";

            GraphicsPipeline.DepthStencilDesc.DepthEnable                     = True;
            GraphicsPipeline.DepthStencilDesc.DepthFunc                       = COMPARISON_FUNC_EQUAL;
            GraphicsPipeline.DepthStencilDesc.DepthWriteEnable                = False;
            GraphicsPipeline.BlendDesc.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_ALL;

            InitializeRenderPassOrRenderTargets(GraphicsPipeline, RTVFormat, pColorRenderPass, true);

            PSOCreateInfo.pVS = pVS;
            PSOCreateInfo.pPS = pPSColor;

            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pColorPSO);
            VERIFY_EXPR(m_pColorPSO);
        }
    }

    void InitializeSRB()
    {
        auto pDepthSRV = m_pDepthTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        VERIFY_EXPR(pDepthSRV);

        m_pColorPSO->CreateShaderResourceBinding(&m_pColorSRB, true);
        m_pColorSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Input")->Set(pDepthSRV);
    }

    void CreateDepthRenderPassAndFramebuffer(RefCntAutoPtr<IRenderPass>& pRenderPass, RefCntAutoPtr<IFramebuffer>& pFramebuffer)
    {
        auto* pEnv    = GPUTestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        RenderPassAttachmentDesc Attachments[1];
        Attachments[0].Format       = GetDepthFormat();
        Attachments[0].InitialState = RESOURCE_STATE_DEPTH_WRITE;
        Attachments[0].FinalState   = RESOURCE_STATE_DEPTH_WRITE;
        Attachments[0].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
        Attachments[0].StoreOp      = ATTACHMENT_STORE_OP_STORE;

        SubpassDesc Subpasses[1];

        AttachmentReference ReadWriteDepthAttachmentRef{0, RESOURCE_STATE_DEPTH_WRITE};
        Subpasses[0].pDepthStencilAttachment = &ReadWriteDepthAttachmentRef;

        RenderPassDesc RPDesc;
        RPDesc.Name            = "Read only depth test -- depth render pass";
        RPDesc.AttachmentCount = _countof(Attachments);
        RPDesc.pAttachments    = Attachments;
        RPDesc.SubpassCount    = _countof(Subpasses);
        RPDesc.pSubpasses      = Subpasses;

        pDevice->CreateRenderPass(RPDesc, &pRenderPass);
        VERIFY_EXPR(pRenderPass);

        FramebufferDesc FBDesc;
        FBDesc.Name               = "Read only depth test -- depth framebuffer";
        FBDesc.pRenderPass        = pRenderPass;
        FBDesc.AttachmentCount    = _countof(Attachments);
        ITextureView* pTexViews[] = {m_pReadWriteDSV};
        FBDesc.ppAttachments      = pTexViews;

        pDevice->CreateFramebuffer(FBDesc, &pFramebuffer);
        VERIFY_EXPR(pFramebuffer);
    }

    void CreateColorRenderPassAndFramebuffer(RefCntAutoPtr<IRenderPass>& pRenderPass, RefCntAutoPtr<IFramebuffer>& pFramebuffer)
    {
        auto* pEnv    = GPUTestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        RenderPassAttachmentDesc Attachments[2];
        Attachments[0].Format       = GetDepthFormat();
        Attachments[0].InitialState = RESOURCE_STATE_DEPTH_READ;
        Attachments[0].FinalState   = RESOURCE_STATE_DEPTH_READ;
        Attachments[0].LoadOp       = ATTACHMENT_LOAD_OP_LOAD;
        Attachments[0].StoreOp      = ATTACHMENT_STORE_OP_DISCARD;

        if (pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D12)
        {
            // The tests fail on NVidia GPU in D3D12 mode when the store op is discard.
            // This might be a driver bug as everything looks correct otherwise.
            Attachments[0].StoreOp = ATTACHMENT_STORE_OP_STORE;
        }

        Attachments[1].Format       = RTVFormat;
        Attachments[1].InitialState = RESOURCE_STATE_RENDER_TARGET;
        Attachments[1].FinalState   = RESOURCE_STATE_RENDER_TARGET;
        Attachments[1].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
        Attachments[1].StoreOp      = ATTACHMENT_STORE_OP_STORE;

        SubpassDesc Subpasses[1];

        Subpasses[0].RenderTargetAttachmentCount = 1;
        AttachmentReference ReadOnlyDepthAttachmentRef{0, RESOURCE_STATE_DEPTH_READ};
        Subpasses[0].pDepthStencilAttachment = &ReadOnlyDepthAttachmentRef;
        AttachmentReference RTAttachmentRef{1, RESOURCE_STATE_RENDER_TARGET};
        Subpasses[0].pRenderTargetAttachments = &RTAttachmentRef;

        RenderPassDesc RPDesc;
        RPDesc.Name            = "Read only depth test -- color render pass";
        RPDesc.AttachmentCount = _countof(Attachments);
        RPDesc.pAttachments    = Attachments;
        RPDesc.SubpassCount    = _countof(Subpasses);
        RPDesc.pSubpasses      = Subpasses;

        pDevice->CreateRenderPass(RPDesc, &pRenderPass);
        VERIFY_EXPR(pRenderPass);

        FramebufferDesc FBDesc;
        FBDesc.Name               = "Read only depth test -- color framebuffer";
        FBDesc.pRenderPass        = pRenderPass;
        FBDesc.AttachmentCount    = _countof(Attachments);
        ITextureView* pTexViews[] = {m_pReadOnlyDSV, m_pRTV};
        FBDesc.ppAttachments      = pTexViews;

        pDevice->CreateFramebuffer(FBDesc, &pFramebuffer);
        VERIFY_EXPR(pFramebuffer);
    }

    TEXTURE_FORMAT GetDepthFormat() const
    {
        const auto& Param = GetParam();
        return static_cast<TEXTURE_FORMAT>(std::get<0>(Param));
    }

    CLEAR_DEPTH_STENCIL_FLAGS GetDepthStencilClearFlags()
    {
        const bool bHasStencil = GetTextureFormatAttribs(GetDepthFormat()).ComponentType == COMPONENT_TYPE_DEPTH_STENCIL;
        return bHasStencil ? (CLEAR_DEPTH_FLAG | CLEAR_STENCIL_FLAG) : CLEAR_DEPTH_FLAG;
    }

    RefCntAutoPtr<IPipelineState> m_pDepthPSO;
    RefCntAutoPtr<IPipelineState> m_pColorPSO;

    RefCntAutoPtr<ITexture>     m_pDepthTexture;
    RefCntAutoPtr<ITextureView> m_pReadWriteDSV;
    RefCntAutoPtr<ITextureView> m_pReadOnlyDSV;

    RefCntAutoPtr<ITextureView> m_pRTV;

    RefCntAutoPtr<IShaderResourceBinding> m_pColorSRB;

private:
    void InitializeRenderPassOrRenderTargets(GraphicsPipelineDesc& GraphicsPipeline, TEXTURE_FORMAT ColorFormat, IRenderPass* pRenderPass, bool bReadOnlyDSV)
    {
        if (pRenderPass)
        {
            GraphicsPipeline.pRenderPass      = pRenderPass;
            GraphicsPipeline.DSVFormat        = TEX_FORMAT_UNKNOWN;
            GraphicsPipeline.NumRenderTargets = 0;
        }
        else
        {
            GraphicsPipeline.DSVFormat   = GetDepthFormat();
            GraphicsPipeline.ReadOnlyDSV = bReadOnlyDSV;
            if (ColorFormat != TEX_FORMAT_UNKNOWN)
            {
                GraphicsPipeline.NumRenderTargets = 1;
                GraphicsPipeline.RTVFormats[0]    = ColorFormat;
            }
            else
            {
                GraphicsPipeline.NumRenderTargets = 0;
            }
        }
    }
};

TEST_P(ReadOnlyDepthTest, AsRenderTarget)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    const bool bSupported = (pDevice->GetTextureFormatInfoExt(GetDepthFormat()).BindFlags & BIND_DEPTH_STENCIL) != 0;
    if (!bSupported)
        GTEST_SKIP_("Depth stencil format is not supported");

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    // Prepare reference image
    TakeSnapshot();

    InitializeDepthTexture();
    InitializePipelineStates(nullptr, nullptr);
    InitializeSRB();

    // Clear color and depth to unused colors
    pContext->SetRenderTargets(1, &m_pRTV, m_pReadWriteDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(m_pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearDepthStencil(m_pReadWriteDSV, GetDepthStencilClearFlags(), ClearDepth, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw depth-only fullscreen quad
    DrawAttribs drawAttrs{4, DRAW_FLAG_VERIFY_ALL};
    pContext->SetRenderTargets(0, nullptr, m_pReadWriteDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->SetPipelineState(m_pDepthPSO);
    pContext->Draw(drawAttrs);

    // Draw color fullscreen quad that reads depth from the texture and performs depth test simultaneously
    pContext->SetRenderTargets(1, &m_pRTV, m_pReadOnlyDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->SetPipelineState(m_pColorPSO);
    pContext->CommitShaderResources(m_pColorSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->Draw(drawAttrs);

    pSwapChain->Present();
}

TEST_P(ReadOnlyDepthTest, InRenderPass)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    const bool bSupported = (pDevice->GetTextureFormatInfoExt(GetDepthFormat()).BindFlags & BIND_DEPTH_STENCIL) != 0;
    if (!bSupported)
        GTEST_SKIP_("Depth stencil format is not supported");

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    // Prepare reference image
    TakeSnapshot();

    // Create render passes and framebuffers
    InitializeDepthTexture();

    RefCntAutoPtr<IRenderPass>  pDepthRenderPass;
    RefCntAutoPtr<IFramebuffer> pDepthFramebuffer;
    CreateDepthRenderPassAndFramebuffer(pDepthRenderPass, pDepthFramebuffer);

    RefCntAutoPtr<IRenderPass>  pColorRenderPass;
    RefCntAutoPtr<IFramebuffer> pColorFramebuffer;
    CreateColorRenderPassAndFramebuffer(pColorRenderPass, pColorFramebuffer);

    InitializePipelineStates(pDepthRenderPass, pColorRenderPass);
    InitializeSRB();

    // Clear color and depth to unused colors
    pContext->SetRenderTargets(1, &m_pRTV, m_pReadWriteDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(m_pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearDepthStencil(m_pReadWriteDSV, GetDepthStencilClearFlags(), ClearDepth, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw depth-only fullscreen quad
    {
        OptimizedClearValue    ClearValues[1];
        BeginRenderPassAttribs RPBeginAttribs;

        ClearValues[0].DepthStencil.Depth = ClearDepth;

        RPBeginAttribs.pRenderPass         = pDepthRenderPass;
        RPBeginAttribs.pFramebuffer        = pDepthFramebuffer;
        RPBeginAttribs.ClearValueCount     = _countof(ClearValues);
        RPBeginAttribs.pClearValues        = ClearValues;
        RPBeginAttribs.StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        pContext->BeginRenderPass(RPBeginAttribs);

        DrawAttribs drawAttrs{4, DRAW_FLAG_VERIFY_ALL};
        pContext->SetPipelineState(m_pDepthPSO);
        pContext->Draw(drawAttrs);

        pContext->EndRenderPass();
    }

    pContext->CommitShaderResources(m_pColorSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw color fullscreen quad that reads depth from the texture and performs depth test simultaneously
    {
        OptimizedClearValue    ClearValues[2];
        BeginRenderPassAttribs RPBeginAttribs;

        ClearValues[0].Format = RTVFormat;
        memcpy(ClearValues[0].Color, ClearColor, sizeof(ClearColor));

        RPBeginAttribs.pRenderPass         = pColorRenderPass;
        RPBeginAttribs.pFramebuffer        = pColorFramebuffer;
        RPBeginAttribs.ClearValueCount     = _countof(ClearValues);
        RPBeginAttribs.pClearValues        = ClearValues;
        RPBeginAttribs.StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        pContext->BeginRenderPass(RPBeginAttribs);

        DrawAttribs drawAttrs{4, DRAW_FLAG_VERIFY_ALL};
        pContext->SetPipelineState(m_pColorPSO);
        pContext->Draw(drawAttrs);

        pContext->EndRenderPass();
    }


    pSwapChain->Present();
}

std::string PrintTextureFormatsTestName(const testing::TestParamInfo<std::tuple<int>>& info) //
{
    auto TextureFormat = static_cast<TEXTURE_FORMAT>(std::get<0>(info.param));

    std::stringstream name_ss;
    name_ss << GetTextureFormatAttribs(TextureFormat).Name;
    return name_ss.str();
}

INSTANTIATE_TEST_SUITE_P(ReadOnlyDepth,
                         ReadOnlyDepthTest,
                         testing::Combine(
                             testing::Values<int>(TEX_FORMAT_D16_UNORM,
                                                  TEX_FORMAT_D24_UNORM_S8_UINT,
                                                  TEX_FORMAT_D32_FLOAT,
                                                  TEX_FORMAT_D32_FLOAT_S8X24_UINT)),
                         PrintTextureFormatsTestName);

} // namespace
