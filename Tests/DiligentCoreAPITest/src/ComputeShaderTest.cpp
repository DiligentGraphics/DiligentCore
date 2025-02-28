/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#include "GPUTestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"
#include "GraphicsTypesX.hpp"

#include "gtest/gtest.h"

#include "InlineShaders/ComputeShaderTestHLSL.h"

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
void ComputeShaderReferenceMtl(ISwapChain* pSwapChain);
#endif

#if WEBGPU_SUPPORTED
void ComputeShaderReferenceWebGPU(ISwapChain* pSwapChain);
#endif


void ComputeShaderReference(ISwapChain* pSwapChain)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();

    RENDER_DEVICE_TYPE deviceType = pDevice->GetDeviceInfo().Type;
    switch (deviceType)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
            ComputeShaderReferenceD3D11(pSwapChain);
            break;
#endif

#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
            ComputeShaderReferenceD3D12(pSwapChain);
            break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            ComputeShaderReferenceGL(pSwapChain);
            break;

#endif

#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
            ComputeShaderReferenceVk(pSwapChain);
            break;
#endif

#if METAL_SUPPORTED
        case RENDER_DEVICE_TYPE_METAL:
            ComputeShaderReferenceMtl(pSwapChain);
            break;
#endif

#if WEBGPU_SUPPORTED
        case RENDER_DEVICE_TYPE_WEBGPU:
            ComputeShaderReferenceWebGPU(pSwapChain);
            break;
#endif
        default:
            LOG_ERROR_AND_THROW("Unsupported device type");
    }

    if (RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain})
    {
        pTestingSwapChain->TakeSnapshot();
    }
}

} // namespace Testing

} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(ComputeShaderTest, FillTexture)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.ComputeShaders)
    {
        GTEST_SKIP() << "Compute shaders are not supported by this device";
    }

    ISwapChain*     pSwapChain = pEnv->GetSwapChain();
    IDeviceContext* pContext   = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "Compute shader test requires testing swap chain";
    }

    pContext->Flush();
    pContext->InvalidateState();

    ComputeShaderReference(pSwapChain);

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
    ShaderCI.Desc           = {"Compute shader test - FillTextureCS", SHADER_TYPE_COMPUTE, true};
    ShaderCI.EntryPoint     = "main";
    ShaderCI.Source         = HLSL::FillTextureCS.c_str();
    RefCntAutoPtr<IShader> pCS;
    pDevice->CreateShader(ShaderCI, &pCS);
    ASSERT_NE(pCS, nullptr);

    ComputePipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Compute shader test";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
    PSOCreateInfo.pCS                  = pCS;

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateComputePipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    const SwapChainDesc& SCDesc = pSwapChain->GetDesc();

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

// Test that GenerateMips does not mess up compute pipeline in D3D12
TEST(ComputeShaderTest, GenerateMips_CSInterference)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.ComputeShaders)
    {
        GTEST_SKIP() << "Compute shaders are not supported by this device";
    }

    ISwapChain*     pSwapChain = pEnv->GetSwapChain();
    IDeviceContext* pContext   = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "Compute shader test requires testing swap chain";
    }

    pContext->Flush();
    pContext->InvalidateState();

    ComputeShaderReference(pSwapChain);

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
    ShaderCI.Desc           = {"Compute shader test - FillTextureCS2", SHADER_TYPE_COMPUTE, true};
    ShaderCI.EntryPoint     = "main";
    ShaderCI.Source         = HLSL::FillTextureCS2.c_str();
    RefCntAutoPtr<IShader> pCS;
    pDevice->CreateShader(ShaderCI, &pCS);
    ASSERT_NE(pCS, nullptr);

    ComputePipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Generate Mips - CS interference test";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
    PSOCreateInfo.pCS                  = pCS;

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateComputePipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    const SwapChainDesc& SCDesc = pSwapChain->GetDesc();

    RefCntAutoPtr<ITexture> pWhiteTex;
    {
        std::vector<Uint8> WhiteRGBA(SCDesc.Width * SCDesc.Width * 4, 255);
        pWhiteTex = pEnv->CreateTexture("White Texture", TEX_FORMAT_RGBA8_UNORM, BIND_SHADER_RESOURCE, SCDesc.Width, SCDesc.Width, WhiteRGBA.data());
        ASSERT_NE(pWhiteTex, nullptr);
    }

    RefCntAutoPtr<ITexture> pBlackTex;
    {
        TextureDesc TexDesc{"Black texture", RESOURCE_DIM_TEX_2D, SCDesc.Width, SCDesc.Height, 1, TEX_FORMAT_RGBA8_UNORM, 4, 1, USAGE_DEFAULT, BIND_SHADER_RESOURCE};
        TexDesc.MiscFlags = MISC_TEXTURE_FLAG_GENERATE_MIPS;

        std::vector<Uint8>             BlackRGBA(SCDesc.Width * SCDesc.Width * 4);
        std::vector<TextureSubResData> MipData(TexDesc.MipLevels);
        for (Uint32 i = 0; i < TexDesc.MipLevels; ++i)
        {
            MipData[i] = TextureSubResData{BlackRGBA.data(), SCDesc.Width * 4};
        }
        TextureData InitData{MipData.data(), TexDesc.MipLevels};

        pDevice->CreateTexture(TexDesc, &InitData, &pBlackTex);
        ASSERT_NE(pBlackTex, nullptr);
    }

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPSO->CreateShaderResourceBinding(&pSRB, true);
    ASSERT_NE(pSRB, nullptr);

    pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_tex2DWhiteTexture")->Set(pWhiteTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_tex2DUAV")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Do not populate the entire texture
    DispatchComputeAttribs DispatchAttribs;
    DispatchAttribs.ThreadGroupCountX = 1;
    DispatchAttribs.ThreadGroupCountY = 1;
    pContext->DispatchCompute(DispatchAttribs);

    // In D3D12 generate mips uses compute pipeline
    pContext->GenerateMips(pBlackTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    DispatchAttribs.ThreadGroupCountX = (SCDesc.Width + 15) / 16;
    DispatchAttribs.ThreadGroupCountY = (SCDesc.Height + 15) / 16;
    pContext->DispatchCompute(DispatchAttribs);

    pSwapChain->Present();
}

static void TestFillTexturePS(bool UseRenderPass)
{
    GPUTestingEnvironment*  pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*          pDevice    = pEnv->GetDevice();
    const RenderDeviceInfo& DeviceInfo = pDevice->GetDeviceInfo();
    if (!DeviceInfo.Features.ComputeShaders)
    {
        GTEST_SKIP() << "Compute shaders are not supported by this device";
    }

    ISwapChain*     pSwapChain = pEnv->GetSwapChain();
    IDeviceContext* pContext   = pEnv->GetDeviceContext();

    const SwapChainDesc& SCDesc = pSwapChain->GetDesc();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "Compute shader test requires testing swap chain";
    }

    RefCntAutoPtr<ITextureView> pDummyRTV;
    if (DeviceInfo.IsWebGPUDevice())
    {
        // WebGPU does not support render passes without attachments (https://github.com/gpuweb/gpuweb/issues/503)
        RefCntAutoPtr<ITexture> pDummyTex = pEnv->CreateTexture("Dummy render target", SCDesc.ColorBufferFormat, BIND_RENDER_TARGET, SCDesc.Width, SCDesc.Height);
        ASSERT_TRUE(pDummyTex != nullptr);
        pDummyRTV = pDummyTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    }

    pContext->Flush();
    pContext->InvalidateState();

    ComputeShaderReference(pSwapChain);

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

    ShaderCI.Desc       = {"Compute shader test - FillTextureVS", SHADER_TYPE_VERTEX, true};
    ShaderCI.EntryPoint = "main";
    ShaderCI.Source     = HLSL::FillTextureVS.c_str();
    RefCntAutoPtr<IShader> pVS;
    pDevice->CreateShader(ShaderCI, &pVS);
    ASSERT_NE(pVS, nullptr);

    ShaderCI.Desc       = {"Compute shader test - FillTexturePS", SHADER_TYPE_PIXEL, true};
    ShaderCI.EntryPoint = "main";
    ShaderCI.Source     = HLSL::FillTexturePS.c_str();
    RefCntAutoPtr<IShader> pPS;
    pDevice->CreateShader(ShaderCI, &pPS);
    ASSERT_NE(pPS, nullptr);

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name                                  = "Compute shader test - output from PS";
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    RefCntAutoPtr<IRenderPass>  pRenderPass;
    RefCntAutoPtr<IFramebuffer> pFramebuffer;
    if (UseRenderPass)
    {
        RenderPassDescX RPDesc{"Compute shader test - render pass"};
        SubpassDescX    Subpass;
        if (DeviceInfo.IsWebGPUDevice())
        {
            RenderPassAttachmentDesc RTAttachment;
            RTAttachment.Format       = SCDesc.ColorBufferFormat;
            RTAttachment.InitialState = RESOURCE_STATE_RENDER_TARGET;
            RTAttachment.FinalState   = RESOURCE_STATE_RENDER_TARGET;
            RPDesc.AddAttachment(RTAttachment);
            Subpass.AddRenderTarget({0, RESOURCE_STATE_RENDER_TARGET});
        }
        RPDesc.AddSubpass(Subpass);
        pDevice->CreateRenderPass(RPDesc, &pRenderPass);
        ASSERT_TRUE(pRenderPass != nullptr);

        PSOCreateInfo.GraphicsPipeline.pRenderPass = pRenderPass;

        FramebufferDescX FBDesc;
        FBDesc.Name           = "Compute shader test - framebuffer";
        FBDesc.pRenderPass    = pRenderPass;
        FBDesc.Width          = SCDesc.Width;
        FBDesc.Height         = SCDesc.Height;
        FBDesc.NumArraySlices = 1;
        if (DeviceInfo.IsWebGPUDevice())
        {
            FBDesc.AddAttachment(pDummyRTV);
        }
        pDevice->CreateFramebuffer(FBDesc, &pFramebuffer);
        ASSERT_TRUE(pFramebuffer != nullptr);
    }
    else
    {
        if (DeviceInfo.IsWebGPUDevice())
        {
            PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
            PSOCreateInfo.GraphicsPipeline.RTVFormats[0]    = SCDesc.ColorBufferFormat;
        }
    }
    if (DeviceInfo.IsWebGPUDevice())
    {
        PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_NONE;
    }

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_tex2DUAV")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPSO->CreateShaderResourceBinding(&pSRB, true);
    ASSERT_NE(pSRB, nullptr);

    ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pContext->SetRenderTargets(1, pRTVs, pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (UseRenderPass)
    {
        BeginRenderPassAttribs BeginRPAttribs;
        BeginRPAttribs.pRenderPass  = pRenderPass;
        BeginRPAttribs.pFramebuffer = pFramebuffer;
        pContext->BeginRenderPass(BeginRPAttribs);
    }
    else
    {
        if (DeviceInfo.IsWebGPUDevice())
        {
            ITextureView* pDummyRTVs[] = {pDummyRTV};
            pContext->SetRenderTargets(1, pDummyRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
        else
        {
            pContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
        }
    }

    Viewport VP{SCDesc};
    pContext->SetViewports(1, &VP, SCDesc.Width, SCDesc.Height);

    pContext->Draw(DrawAttribs{3, DRAW_FLAG_VERIFY_ALL});

    if (UseRenderPass)
    {
        pContext->EndRenderPass();
    }

    pSwapChain->Present();
}

TEST(ComputeShaderTest, FillTexturePS)
{
    TestFillTexturePS(false);
}

TEST(ComputeShaderTest, FillTexturePS_InRenderPass)
{
    TestFillTexturePS(true);
}

TEST(ComputeShaderTest, FillTexturePS_Signatures)
{
    GPUTestingEnvironment*  pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*          pDevice    = pEnv->GetDevice();
    const RenderDeviceInfo& DeviceInfo = pDevice->GetDeviceInfo();
    if (!DeviceInfo.Features.ComputeShaders)
    {
        GTEST_SKIP() << "Compute shaders are not supported by this device";
    }

    ISwapChain*     pSwapChain = pEnv->GetSwapChain();
    IDeviceContext* pContext   = pEnv->GetDeviceContext();

    const SwapChainDesc& SCDesc = pSwapChain->GetDesc();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "Compute shader test requires testing swap chain";
    }

    RefCntAutoPtr<ITextureView> pDummyRTV;
    if (DeviceInfo.IsWebGPUDevice())
    {
        // WebGPU does not support render passes without attachments (https://github.com/gpuweb/gpuweb/issues/503)
        RefCntAutoPtr<ITexture> pDummyTex = pEnv->CreateTexture("Dummy render target", SCDesc.ColorBufferFormat, BIND_RENDER_TARGET, SCDesc.Width, SCDesc.Height);
        ASSERT_TRUE(pDummyTex != nullptr);
        pDummyRTV = pDummyTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    }

    pContext->Flush();
    pContext->InvalidateState();

    ComputeShaderReference(pSwapChain);

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

    ShaderCI.Desc       = {"Compute shader test - FillTextureVS", SHADER_TYPE_VERTEX, true};
    ShaderCI.EntryPoint = "main";
    ShaderCI.Source     = HLSL::FillTextureVS.c_str();
    RefCntAutoPtr<IShader> pVS;
    pDevice->CreateShader(ShaderCI, &pVS);
    ASSERT_NE(pVS, nullptr);

    ShaderCI.Desc       = {"Compute shader test - FillTexturePS", SHADER_TYPE_PIXEL, true};
    ShaderCI.EntryPoint = "main";
    ShaderCI.Source     = HLSL::FillTexturePS2.c_str();
    RefCntAutoPtr<IShader> pPS;
    pDevice->CreateShader(ShaderCI, &pPS);
    ASSERT_NE(pPS, nullptr);

    RefCntAutoPtr<IPipelineResourceSignature> pSignature0;
    {
        PipelineResourceSignatureDescX SignDesc{"ComputeShaderTest.FillTexturePS_InRenderPass - Signature 0"};
        SignDesc.AddResource(SHADER_TYPE_PIXEL, "Constants", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);
        pDevice->CreatePipelineResourceSignature(SignDesc, &pSignature0);
        ASSERT_NE(pSignature0, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignature1;
    {
        PipelineResourceSignatureDescX SignDesc{"ComputeShaderTest.FillTexturePS_InRenderPass - Signature 1"};
        SignDesc.AddResource(SHADER_TYPE_PIXEL, "g_tex2DUAV", 1u, SHADER_RESOURCE_TYPE_TEXTURE_UAV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, PIPELINE_RESOURCE_FLAG_NONE,
                             WebGPUResourceAttribs{WEB_GPU_BINDING_TYPE_WRITE_ONLY_TEXTURE_UAV, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA8_UNORM});
        SignDesc.SetBindingIndex(1);
        pDevice->CreatePipelineResourceSignature(SignDesc, &pSignature1);
        ASSERT_NE(pSignature1, nullptr);
    }

    GraphicsPipelineStateCreateInfoX PSOCreateInfo{"Compute shader test - output from PS"};

    PSOCreateInfo.AddSignature(pSignature0);
    PSOCreateInfo.AddSignature(pSignature1);

    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    if (DeviceInfo.IsWebGPUDevice())
    {
        PSOCreateInfo.GraphicsPipeline.NumRenderTargets                                 = 1;
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                                    = SCDesc.ColorBufferFormat;
        PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_NONE;
    }

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    const float4 Zero{0};
    const float4 One{1};

    RefCntAutoPtr<IBuffer> pBuffer0 = pEnv->CreateBuffer({"ComputeShaderTest.FillTexturePS_InRenderPass - Buffer 0", sizeof(Zero), BIND_UNIFORM_BUFFER}, &Zero);
    ASSERT_NE(pBuffer0, nullptr);
    RefCntAutoPtr<IBuffer> pBuffer1 = pEnv->CreateBuffer({"ComputeShaderTest.FillTexturePS_InRenderPass - Buffer 1", sizeof(One), BIND_UNIFORM_BUFFER}, &One);
    ASSERT_NE(pBuffer1, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pSRB0[2];
    pSignature0->CreateShaderResourceBinding(&pSRB0[0], true);
    ASSERT_NE(pSRB0[0], nullptr);
    pSRB0[0]->GetVariableByName(SHADER_TYPE_PIXEL, "Constants")->Set(pBuffer0);

    pSignature0->CreateShaderResourceBinding(&pSRB0[1], true);
    ASSERT_NE(pSRB0[1], nullptr);
    pSRB0[1]->GetVariableByName(SHADER_TYPE_PIXEL, "Constants")->Set(pBuffer1);


    RefCntAutoPtr<IShaderResourceBinding> pSRB1;
    pSignature1->CreateShaderResourceBinding(&pSRB1, true);
    ASSERT_NE(pSRB1, nullptr);
    pSRB1->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DUAV")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

    ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pContext->SetRenderTargets(1, pRTVs, pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB0[0], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CommitShaderResources(pSRB1, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (DeviceInfo.IsWebGPUDevice())
    {
        ITextureView* pDummyRTVs[] = {pDummyRTV};
        pContext->SetRenderTargets(1, pDummyRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    else
    {
        pContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    }

    Viewport VP{SCDesc};
    pContext->SetViewports(1, &VP, SCDesc.Width, SCDesc.Height);

    pContext->Draw(DrawAttribs{3, DRAW_FLAG_VERIFY_ALL});

    pContext->CommitShaderResources(pSRB0[1], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->Draw(DrawAttribs{3, DRAW_FLAG_VERIFY_ALL});

    pSwapChain->Present();
}

} // namespace
