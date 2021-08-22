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

#include "InlineShaders/RasterizationRateMapTestMSL.h"
#include "VariableShadingRateTestConstants.hpp"

#include <Metal/Metal.h>
#include "DeviceContextMtl.h"
#include "RenderDeviceMtl.h"

namespace Diligent
{
namespace Testing
{

void RasterizationRateMapReferenceMtl(ISwapChain* pSwapChain);

} // namespace Testing
} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

using VRSTestingConstants::TextureBased::GenColRowFp32;

TEST(VariableShadingRateTest, RasterRateMap)
{
    if (@available(macos 10.15.4, ios 13.0, *))
    {
        auto*       pEnv       = TestingEnvironment::GetInstance();
        auto*       pDevice    = pEnv->GetDevice();
        const auto& deviceInfo = pDevice->GetDeviceInfo();

        if (!deviceInfo.IsMetalDevice())
            GTEST_SKIP();

        if (!deviceInfo.Features.VariableRateShading)
            GTEST_SKIP();

        auto*       pSwapChain = pEnv->GetSwapChain();
        auto*       pContext   = pEnv->GetDeviceContext();
        const auto& SCDesc     = pSwapChain->GetDesc();

        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
        if (pTestingSwapChain)
        {
            pContext->Flush();
            pContext->InvalidateState();

            RasterizationRateMapReferenceMtl(pSwapChain);
            pTestingSwapChain->TakeSnapshot();
        }

        TestingEnvironment::ScopedReset EnvironmentAutoReset;

        RefCntAutoPtr<IPipelineState> pPSOPass1;
        {
            GraphicsPipelineStateCreateInfo PSOCreateInfo;

            auto& PSODesc          = PSOCreateInfo.PSODesc;
            auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

            PSODesc.Name = "Raster rate map Pass1 PSO";

            GraphicsPipeline.NumRenderTargets                     = 1;
            GraphicsPipeline.RTVFormats[0]                        = SCDesc.ColorBufferFormat;
            GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
            GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
            GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;

            GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
            GraphicsPipeline.ShadingRateFlags             = PIPELINE_SHADING_RATE_FLAG_TEXTURE_BASED;

            const LayoutElement Elements[] = {
                {0, 0, 2, VT_FLOAT32, False, offsetof(PosAndRate, Pos)},
                {1, 0, 1, VT_UINT32, False, offsetof(PosAndRate, Rate)} //
            };
            GraphicsPipeline.InputLayout.NumElements    = _countof(Elements);
            GraphicsPipeline.InputLayout.LayoutElements = Elements;

            ShaderCreateInfo ShaderCI;
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_MSL;

            RefCntAutoPtr<IShader> pVS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
                ShaderCI.EntryPoint      = "VSmain";
                ShaderCI.Desc.Name       = "Raster rate map Pass1 - VS";
                ShaderCI.Source          = MSL::RasterRateMapTest_Pass1.c_str();

                pDevice->CreateShader(ShaderCI, &pVS);
                ASSERT_NE(pVS, nullptr);
            }

            RefCntAutoPtr<IShader> pPS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                ShaderCI.EntryPoint      = "PSmain";
                ShaderCI.Desc.Name       = "Raster rate map Pass1 - PS";
                ShaderCI.Source          = MSL::RasterRateMapTest_Pass1.c_str();

                pDevice->CreateShader(ShaderCI, &pPS);
                ASSERT_NE(pPS, nullptr);
            }

            PSOCreateInfo.pVS = pVS;
            PSOCreateInfo.pPS = pPS;
            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSOPass1);
            ASSERT_NE(pPSOPass1, nullptr);
        }

        RefCntAutoPtr<IDeviceObject> pShadingRateMap;
        RefCntAutoPtr<IBuffer>       pShadingRateParamBuffer;
        RefCntAutoPtr<ITexture>      pIntermediateRT;
        {
            RasterizationRateMapCreateInfo RasterRateMapCI;
            RasterRateMapCI.Desc.ScreenWidth  = SCDesc.Width;
            RasterRateMapCI.Desc.ScreenHeight = SCDesc.Height;
            RasterRateMapCI.Desc.LayerCount   = 1;

            const Uint32       TileSize = 4;
            std::vector<float> Horizontal(RasterRateMapCI.Desc.ScreenWidth / TileSize);
            std::vector<float> Vertical(RasterRateMapCI.Desc.ScreenHeight / TileSize);

            for (size_t i = 0; i < Horizontal.size(); ++i)
                Horizontal[i] = GenColRowFp32(i, Horizontal.size());

            for (size_t i = 0; i < Vertical.size(); ++i)
                Vertical[i] = GenColRowFp32(i, Vertical.size());

            RasterizationRateLayerDesc Layer;
            Layer.HorizontalCount   = static_cast<Uint32>(Horizontal.size());
            Layer.VerticalCount     = static_cast<Uint32>(Vertical.size());
            Layer.pHorizontal       = Horizontal.data();
            Layer.pVertical         = Vertical.data();
            RasterRateMapCI.pLayers = &Layer;

            RefCntAutoPtr<IRenderDeviceMtl> pDeviceMtl{pDevice, IID_RenderDeviceMtl};
            RefCntAutoPtr<IRasterizationRateMapMtl> pRasterizationRateMap;
            pDeviceMtl->CreateRasterizationRateMap(RasterRateMapCI, &pRasterizationRateMap);
            ASSERT_NE(pRasterizationRateMap, nullptr);
            pShadingRateMap = pRasterizationRateMap;

            Uint32 BufferSize, BufferAlign;
            pRasterizationRateMap->GetParameterBufferSizeAndAlign(BufferSize, BufferAlign);

            BufferDesc BuffDesc;
            BuffDesc.Name           = "RRM parameters buffer";
            BuffDesc.uiSizeInBytes  = BufferSize;
            BuffDesc.Usage          = USAGE_UNIFIED;       // buffer is used for host access and will be accessed in shader
            BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER; // only uniform buffer is compatible with unified memory
            BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            pDevice->CreateBuffer(BuffDesc, nullptr, &pShadingRateParamBuffer);

            pRasterizationRateMap->CopyParameterDataToBuffer(pShadingRateParamBuffer, 0);
            ASSERT_NE(pShadingRateParamBuffer, nullptr);

            Uint32 Width, Height;
            pRasterizationRateMap->GetPhysicalSizeForLayer(0, Width, Height);

            TextureDesc TexDesc;
            TexDesc.Name      = "Intermediate render target";
            TexDesc.Type      = RESOURCE_DIM_TEX_2D;
            TexDesc.Width     = Width;
            TexDesc.Height    = Height;
            TexDesc.Format    = SCDesc.ColorBufferFormat;
            TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

            pDevice->CreateTexture(TexDesc, nullptr, &pIntermediateRT);
            ASSERT_NE(pIntermediateRT, nullptr);
        }

        RefCntAutoPtr<IPipelineState>         pPSOPass2;
        RefCntAutoPtr<IShaderResourceBinding> pSRBPass2;
        {
            GraphicsPipelineStateCreateInfo PSOCreateInfo;

            auto& PSODesc          = PSOCreateInfo.PSODesc;
            auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

            PSODesc.Name = "Raster rate map Pass1 PSO";

            PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

            GraphicsPipeline.NumRenderTargets                     = 1;
            GraphicsPipeline.RTVFormats[0]                        = SCDesc.ColorBufferFormat;
            GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
            GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
            GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
            GraphicsPipeline.DepthStencilDesc.DepthEnable         = False;

            ShaderCreateInfo ShaderCI;
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_MSL;

            RefCntAutoPtr<IShader> pVS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
                ShaderCI.EntryPoint      = "VSmain";
                ShaderCI.Desc.Name       = "Raster rate map Pass2 - VS";
                ShaderCI.Source          = MSL::RasterRateMapTest_Pass2.c_str();

                pDevice->CreateShader(ShaderCI, &pVS);
                ASSERT_NE(pVS, nullptr);
            }

            RefCntAutoPtr<IShader> pPS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                ShaderCI.EntryPoint      = "PSmain";
                ShaderCI.Desc.Name       = "Raster rate map Pass2 - PS";
                ShaderCI.Source          = MSL::RasterRateMapTest_Pass2.c_str();

                pDevice->CreateShader(ShaderCI, &pPS);
                ASSERT_NE(pPS, nullptr);
            }

            PSOCreateInfo.pVS = pVS;
            PSOCreateInfo.pPS = pPS;
            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSOPass2);
            ASSERT_NE(pPSOPass2, nullptr);

            pPSOPass2->CreateShaderResourceBinding(&pSRBPass2);
            pSRBPass2->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pIntermediateRT->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            pSRBPass2->GetVariableByName(SHADER_TYPE_PIXEL, "g_RRMData")->Set(pShadingRateParamBuffer);
        }

        const auto& Verts = VRSTestingConstants::PerPrimitive::Vertices;

        BufferData BuffData{Verts, sizeof(Verts)};
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Vertex buffer";
        BuffDesc.uiSizeInBytes = BuffData.DataSize;
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.Usage         = USAGE_IMMUTABLE;

        RefCntAutoPtr<IBuffer> pVB;
        pDevice->CreateBuffer(BuffDesc, &BuffData, &pVB);
        ASSERT_NE(pVB, nullptr);

        // Pass 1
        {
            ITextureView*           pRTVs[] = {pIntermediateRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)};
            SetRenderTargetsAttribs RTAttrs;
            RTAttrs.NumRenderTargets    = 1;
            RTAttrs.ppRenderTargets     = pRTVs;
            RTAttrs.pShadingRateMap     = pShadingRateMap;
            RTAttrs.StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            pContext->SetRenderTargetsExt(RTAttrs);

            const float ClearColor[] = {1.f, 0.f, 0.f, 1.f};
            pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            pContext->SetShadingRate(SHADING_RATE_1X1, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_OVERRIDE);

            pContext->SetPipelineState(pPSOPass1);

            IBuffer*     VBuffers[] = {pVB};
            const Uint32 Offsets[]  = {0};
            pContext->SetVertexBuffers(0, 1, VBuffers, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

            DrawAttribs drawAttrs{_countof(Verts), DRAW_FLAG_VERIFY_ALL};
            pContext->Draw(drawAttrs);
        }

        // Pass 2
        {
            ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
            pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            pContext->SetPipelineState(pPSOPass2);
            pContext->CommitShaderResources(pSRBPass2, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            DrawAttribs drawAttrs{3, DRAW_FLAG_VERIFY_ALL};
            pContext->Draw(drawAttrs);
        }

        pSwapChain->Present();
    }
}

} // namespace
