/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "InlineShaders/VariableShadingRateTestHLSL.h"
#include "VariableShadingRateTestConstants.hpp"

namespace Diligent
{

namespace Testing
{

#if D3D12_SUPPORTED
void VariableShadingRatePerPipelineTestReferenceD3D12(ISwapChain* pSwapChain);
void VariableShadingRatePerPrimitiveTestReferenceD3D12(ISwapChain* pSwapChain);
void VariableShadingRateTextureBasedTestReferenceD3D12(ISwapChain* pSwapChain);
#endif

#if VULKAN_SUPPORTED
void VariableShadingRatePerPipelineTestReferenceVk(ISwapChain* pSwapChain);
void VariableShadingRatePerPrimitiveTestReferenceVk(ISwapChain* pSwapChain);
void VariableShadingRateTextureBasedTestReferenceVk(ISwapChain* pSwapChain);
#endif

#if METAL_SUPPORTED
void VariableShadingRateTestReferenceMtl(ISwapChain* pSwapChain);
#endif

} // namespace Testing

} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(VariableShadingRateTest, PerPipeline)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.VariableRateShading)
    {
        GTEST_SKIP() << "Variable shading rate is not supported by this device";
    }

    const auto& SRProps = pDevice->GetAdapterInfo().ShadingRate;
    if (!(SRProps.CapFlags & SHADING_RATE_CAP_FLAG_PER_DRAW))
    {
        GTEST_SKIP() << "Per pipeline shading rate is not supported by this device";
    }

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
    if (pTestingSwapChain)
    {
        pContext->Flush();
        pContext->InvalidateState();

        auto deviceType = pDevice->GetDeviceInfo().Type;
        switch (deviceType)
        {
#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
                VariableShadingRatePerPipelineTestReferenceD3D12(pSwapChain);
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                VariableShadingRatePerPipelineTestReferenceVk(pSwapChain);
                break;
#endif

            default:
                LOG_ERROR_AND_THROW("Unsupported device type");
        }

        pTestingSwapChain->TakeSnapshot();
    }
    TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.Name = "Per pipeline shading test";

    GraphicsPipeline.NumRenderTargets                     = 1;
    GraphicsPipeline.RTVFormats[0]                        = pSwapChain->GetDesc().ColorBufferFormat;
    GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
    GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;

    GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Per pipeline shading test - VS";
        ShaderCI.Source          = HLSL::PerDrawShadingRate_VS.c_str();

        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Per pipeline shading test - PS";
        ShaderCI.Source          = HLSL::PerDrawShadingRate_PS.c_str();

        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const float ClearColor[] = {0.f, 0.f, 0.f, 0.f};
    pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    pContext->SetShadingRate(SHADING_RATE_2x2, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_PASSTHROUGH);

    DrawAttribs drawAttrs{3, DRAW_FLAG_VERIFY_ALL};
    pContext->Draw(drawAttrs);

    pSwapChain->Present();
}


TEST(VariableShadingRateTest, PerPrimitive)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.VariableRateShading)
    {
        GTEST_SKIP() << "Variable shading rate is not supported by this device";
    }

    const auto& SRProps = pDevice->GetAdapterInfo().ShadingRate;
    if (!(SRProps.CapFlags & SHADING_RATE_CAP_FLAG_PER_PRIMITIVE))
    {
        GTEST_SKIP() << "Per primitive shading rate is not supported by this device";
    }

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
    if (pTestingSwapChain)
    {
        pContext->Flush();
        pContext->InvalidateState();

        auto deviceType = pDevice->GetDeviceInfo().Type;
        switch (deviceType)
        {
#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
                VariableShadingRatePerPrimitiveTestReferenceD3D12(pSwapChain);
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                VariableShadingRatePerPrimitiveTestReferenceVk(pSwapChain);
                break;
#endif

            default:
                LOG_ERROR_AND_THROW("Unsupported device type");
        }

        pTestingSwapChain->TakeSnapshot();
    }
    TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.Name = "Per primitive shading test";

    GraphicsPipeline.NumRenderTargets                     = 1;
    GraphicsPipeline.RTVFormats[0]                        = pSwapChain->GetDesc().ColorBufferFormat;
    GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
    GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;

    GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    const LayoutElement Elements[] = {
        {0, 0, 2, VT_FLOAT32, False, offsetof(PosAndRate, Pos)},
        {1, 0, 1, VT_UINT32, False, offsetof(PosAndRate, Rate)} //
    };
    GraphicsPipeline.InputLayout.NumElements    = _countof(Elements);
    GraphicsPipeline.InputLayout.LayoutElements = Elements;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Per primitive shading test - VS";
        ShaderCI.Source          = HLSL::PerPrimitiveShadingRate_VS.c_str();

        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Per primitive shading test - PS";
        ShaderCI.Source          = HLSL::PerPrimitiveShadingRate_PS.c_str();

        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    const auto& Verts = TestingConstants::PerPrimitive::Vertices;

    BufferData BuffData{Verts, sizeof(Verts)};
    BufferDesc BuffDesc;
    BuffDesc.Name          = "Vertex buffer";
    BuffDesc.uiSizeInBytes = BuffData.DataSize;
    BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
    BuffDesc.Usage         = USAGE_IMMUTABLE;

    RefCntAutoPtr<IBuffer> pVB;
    pDevice->CreateBuffer(BuffDesc, &BuffData, &pVB);
    ASSERT_NE(pVB, nullptr);

    ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const float ClearColor[] = {0.f, 0.f, 0.f, 0.f};
    pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Use shading rate from vertex shader
    pContext->SetShadingRate(SHADING_RATE_1x1, SHADING_RATE_COMBINER_OVERRIDE, SHADING_RATE_COMBINER_PASSTHROUGH);

    pContext->SetPipelineState(pPSO);

    IBuffer*     VBuffers[] = {pVB};
    const Uint32 Offsets[]  = {0};
    pContext->SetVertexBuffers(0, 1, VBuffers, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    DrawAttribs drawAttrs{_countof(Verts), DRAW_FLAG_VERIFY_ALL};
    pContext->Draw(drawAttrs);

    pSwapChain->Present();
}


TEST(VariableShadingRateTest, TextureBased)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.VariableRateShading)
    {
        GTEST_SKIP() << "Variable shading rate is not supported by this device";
    }

    const auto& SRProps = pDevice->GetAdapterInfo().ShadingRate;
    if (!(SRProps.CapFlags & SHADING_RATE_CAP_FLAG_TEXTURE_BASED))
    {
        GTEST_SKIP() << "Shading rate texture is not supported by this device";
    }

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
    if (pTestingSwapChain)
    {
        pContext->Flush();
        pContext->InvalidateState();

        auto deviceType = pDevice->GetDeviceInfo().Type;
        switch (deviceType)
        {
#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
                VariableShadingRateTextureBasedTestReferenceD3D12(pSwapChain);
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                VariableShadingRateTextureBasedTestReferenceVk(pSwapChain);
                break;
#endif

            default:
                LOG_ERROR_AND_THROW("Unsupported device type");
        }

        pTestingSwapChain->TakeSnapshot();
    }
    TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

    const auto& SCDesc = pSwapChain->GetDesc();

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.Name = "Texture based shading test";

    GraphicsPipeline.NumRenderTargets                     = 1;
    GraphicsPipeline.RTVFormats[0]                        = SCDesc.ColorBufferFormat;
    GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
    GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;

    GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Texture based shading test - VS";
        ShaderCI.Source          = HLSL::TextureBasedShadingRate_VS.c_str();

        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Texture based shading test - PS";
        ShaderCI.Source          = HLSL::TextureBasedShadingRate_PS.c_str();

        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    TextureDesc TexDesc;
    TexDesc.Name      = "Shading rate texture";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Width     = SCDesc.Width / SRProps.MaxTileWidth;
    TexDesc.Height    = SCDesc.Height / SRProps.MaxTileHeight;
    TexDesc.Format    = TEX_FORMAT_R8_UINT;
    TexDesc.BindFlags = BIND_SHADING_RATE;
    TexDesc.Usage     = USAGE_IMMUTABLE;

    std::vector<Uint8> SRData;
    SRData.resize(TexDesc.Width * TexDesc.Height);
    for (Uint32 y = 0; y < TexDesc.Height; ++y)
    {
        for (Uint32 x = 0; x < TexDesc.Width; ++x)
        {
            SRData[x + y * TexDesc.Width] = TestingConstants::TextureBased::GenTexture(x, y, TexDesc.Width, TexDesc.Height);
        }
    }

    TextureSubResData SubResData;
    SubResData.pData  = SRData.data();
    SubResData.Stride = TexDesc.Width;

    TextureData TexData;
    TexData.pSubResources   = &SubResData;
    TexData.NumSubresources = 1;

    RefCntAutoPtr<ITexture> pSRTex;
    pDevice->CreateTexture(TexDesc, &TexData, &pSRTex);
    ASSERT_NE(pSRTex, nullptr);

    auto* pSRView = pSRTex->GetDefaultView(TEXTURE_VIEW_SHADING_RATE);
    ASSERT_NE(pSRView, nullptr);

    ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const float ClearColor[] = {0.f, 0.f, 0.f, 0.f};
    pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetShadingRate(SHADING_RATE_1x1, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_OVERRIDE);
    pContext->SetShadingRateTexture(pSRView, SRProps.MaxTileWidth, SRProps.MaxTileHeight, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    DrawAttribs drawAttrs{3, DRAW_FLAG_VERIFY_ALL};
    pContext->Draw(drawAttrs);

    pSwapChain->Present();
}

} // namespace
