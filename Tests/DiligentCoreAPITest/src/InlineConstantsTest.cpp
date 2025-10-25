/*
 *  Copyright 2025 Diligent Graphics LLC
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

#include "gtest/gtest.h"

#include "RenderStateCache.hpp"
#include "GraphicsTypesX.hpp"
#include "FastRand.hpp"


namespace Diligent
{
namespace Testing
{
void RenderDrawCommandReference(ISwapChain* pSwapChain, const float* pClearColor = nullptr);
}
} // namespace Diligent


using namespace Diligent;
using namespace Diligent::Testing;

#include "InlineShaders/DrawCommandTestHLSL.h"


namespace
{

namespace HLSL
{

const std::string InlineConstantsTest_VS{
    R"(
cbuffer cbInlinePositions
{
    float4 g_Positions[6];
}

cbuffer cbInlineColors
{
    float4 g_Colors[3];
}

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
};

void main(uint VertexId : SV_VertexId, 
          out  PSInput  PSIn)
{
    PSIn.Pos   = g_Positions[VertexId];
    PSIn.Color = g_Colors[VertexId % 3].rgb;
}
)"};

}

float4 g_Positions[] = {
    float4{-1.0f, -0.5f, 0.f, 1.f},
    float4{-0.5f, +0.5f, 0.f, 1.f},
    float4{0.0f, -0.5f, 0.f, 1.f},

    float4{+0.0f, -0.5f, 0.f, 1.f},
    float4{+0.5f, +0.5f, 0.f, 1.f},
    float4{+1.0f, -0.5f, 0.f, 1.f},
};

float4 g_Colors[] = {
    float4{1.f, 0.f, 0.f, 1.f},
    float4{0.f, 1.f, 0.f, 1.f},
    float4{0.f, 0.f, 1.f, 1.f},
};

constexpr Uint32 kNumPosConstants = sizeof(g_Positions) / 4;
constexpr Uint32 kNumColConstants = sizeof(g_Colors) / 4;

class InlineConstants : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
        IRenderDevice*         pDevice = pEnv->GetDevice();

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

        {
            ShaderCI.Desc       = {"Inline constants test", SHADER_TYPE_VERTEX, true};
            ShaderCI.EntryPoint = "main";
            ShaderCI.Source     = HLSL::InlineConstantsTest_VS.c_str();
            pDevice->CreateShader(ShaderCI, &sm_Res.pVS);
            ASSERT_NE(sm_Res.pVS, nullptr);
        }

        {
            ShaderCI.Desc       = {"Inline constants test", SHADER_TYPE_PIXEL, true};
            ShaderCI.EntryPoint = "main";
            ShaderCI.Source     = HLSL::DrawTest_PS.c_str();
            pDevice->CreateShader(ShaderCI, &sm_Res.pPS);
            ASSERT_NE(sm_Res.pPS, nullptr);
        }
    }

    static void TearDownTestSuite()
    {
        sm_Res = {};

        GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
        pEnv->Reset();
    }

    static void TestSignatures(Uint32 NumSignatures);

    static void Present()
    {
        GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
        ISwapChain*            pSwapChain = pEnv->GetSwapChain();
        IDeviceContext*        pContext   = pEnv->GetDeviceContext();

        pSwapChain->Present();

        pContext->Flush();
        pContext->InvalidateState();
    }

    static void VerifyPSOFromCache(IPipelineState*         pPSO,
                                   IShaderResourceBinding* pSRB);

    struct Resources
    {
        RefCntAutoPtr<IShader> pVS;
        RefCntAutoPtr<IShader> pPS;
    };
    static Resources sm_Res;

    static FastRandFloat sm_Rnd;
};

InlineConstants::Resources InlineConstants::sm_Res;
FastRandFloat              InlineConstants::sm_Rnd{0, 0.f, 1.f};


TEST_F(InlineConstants, ResourceLayout)
{
    GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice    = pEnv->GetDevice();
    IDeviceContext*        pContext   = pEnv->GetDeviceContext();
    ISwapChain*            pSwapChain = pEnv->GetSwapChain();
    if (pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D12)
    {
        GTEST_SKIP();
    }

    for (Uint32 pos_type = 0; pos_type < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; ++pos_type)
    {
        for (Uint32 col_type = 0; col_type < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; ++col_type)
        {
            const float ClearColor[] = {sm_Rnd(), sm_Rnd(), sm_Rnd(), sm_Rnd()};
            RenderDrawCommandReference(pSwapChain, ClearColor);

            SHADER_RESOURCE_VARIABLE_TYPE PosType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(pos_type);
            SHADER_RESOURCE_VARIABLE_TYPE ColType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(col_type);

            GraphicsPipelineStateCreateInfoX PsoCI{"Inline constants test"};

            PipelineResourceLayoutDescX ResLayoutDesc;
            ResLayoutDesc
                .AddVariable(SHADER_TYPE_VERTEX, "cbInlinePositions", PosType, SHADER_VARIABLE_FLAG_INLINE_CONSTANTS)
                .AddVariable(SHADER_TYPE_VERTEX, "cbInlineColors", ColType, SHADER_VARIABLE_FLAG_INLINE_CONSTANTS);

            PsoCI
                .AddRenderTarget(pSwapChain->GetDesc().ColorBufferFormat)
                .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                .AddShader(sm_Res.pVS)
                .AddShader(sm_Res.pPS)
                .SetResourceLayout(ResLayoutDesc);
            PsoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

            RefCntAutoPtr<IPipelineState> pPSO;
            pDevice->CreateGraphicsPipelineState(PsoCI, &pPSO);
            ASSERT_TRUE(pPSO);

            if (PosType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                IShaderResourceVariable* pVar = pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbInlinePositions");
                ASSERT_TRUE(pVar);
                pVar->SetInlineConstants(g_Positions, 0, kNumPosConstants);
            }

            if (ColType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                IShaderResourceVariable* pVar = pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbInlineColors");
                ASSERT_TRUE(pVar);
                pVar->SetInlineConstants(g_Colors, 0, kNumColConstants);
            }

            RefCntAutoPtr<IShaderResourceBinding> pSRB;
            pPSO->CreateShaderResourceBinding(&pSRB, true);
            ASSERT_TRUE(pSRB);

            IShaderResourceVariable* pPosVar = nullptr;
            if (PosType != SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                pPosVar = pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbInlinePositions");
                ASSERT_TRUE(pPosVar);
            }

            IShaderResourceVariable* pColVar = nullptr;
            if (ColType != SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                pColVar = pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbInlineColors");
                ASSERT_TRUE(pColVar);
            }

            ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
            pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);


            if (pColVar != nullptr)
            {
                // Set first half of color constants before committing SRB
                pColVar->SetInlineConstants(g_Colors, 0, kNumColConstants / 2);
            }

            pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            if (pColVar != nullptr)
            {
                // Set second half of color constants after committing SRB
                pColVar->SetInlineConstants(g_Colors[0].Data() + kNumColConstants / 2, kNumColConstants / 2, kNumColConstants / 2);
            }

            pContext->SetPipelineState(pPSO);

            if (pPosVar == nullptr)
            {
                // Draw both triangles as positions are static
                pContext->Draw({6, DRAW_FLAG_VERIFY_ALL});
            }
            else
            {
                // Draw first triangle
                pPosVar->SetInlineConstants(g_Positions, 0, kNumPosConstants / 2);
                pContext->Draw({3, DRAW_FLAG_VERIFY_ALL});

                // Draw second triangle
                pPosVar->SetInlineConstants(g_Positions[0].Data() + kNumPosConstants / 2, 0, kNumPosConstants / 2);
                pContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
            }

            Present();

            std::cout << TestingEnvironment::GetCurrentTestStatusString() << ' '
                      << " Pos " << GetShaderVariableTypeLiteralName(PosType) << ','
                      << " Col " << GetShaderVariableTypeLiteralName(ColType) << std::endl;
        }
    }
}


void InlineConstants::TestSignatures(Uint32 NumSignatures)
{
    GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice    = pEnv->GetDevice();
    IDeviceContext*        pContext   = pEnv->GetDeviceContext();
    ISwapChain*            pSwapChain = pEnv->GetSwapChain();
    if (pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D12)
    {
        GTEST_SKIP();
    }

    RefCntAutoPtr<IBuffer> pConstBuffer = pEnv->CreateBuffer({"InlineConstants - dummy const buffer", 256, BIND_UNIFORM_BUFFER});
    ASSERT_TRUE(pConstBuffer);
    RefCntAutoPtr<ITexture> pTexture = pEnv->CreateTexture("InlineConstants - dummy texture", TEX_FORMAT_RGBA8_UNORM, BIND_SHADER_RESOURCE, 64, 64);
    ASSERT_TRUE(pTexture);
    ITextureView* pTexSRV = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    ASSERT_TRUE(pTexSRV);

    for (Uint32 pos_type = 0; pos_type < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; ++pos_type)
    {
        for (Uint32 col_type = 0; col_type < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; ++col_type)
        {
            const float ClearColor[] = {sm_Rnd(), sm_Rnd(), sm_Rnd(), sm_Rnd()};
            RenderDrawCommandReference(pSwapChain, ClearColor);

            SHADER_RESOURCE_VARIABLE_TYPE PosType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(pos_type);
            SHADER_RESOURCE_VARIABLE_TYPE ColType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(col_type);

            RefCntAutoPtr<IPipelineResourceSignature> pPosSign;
            RefCntAutoPtr<IPipelineResourceSignature> pColSign;

            PipelineResourceSignatureDescX SignDesc{"Inline constants test"};
            SignDesc
                .AddResource(SHADER_TYPE_VERTEX, "cb0_stat", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                .AddResource(SHADER_TYPE_VERTEX, "cb0_mut", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                .AddResource(SHADER_TYPE_VERTEX, "cb0_dyn", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                .AddResource(SHADER_TYPE_VERTEX, "tex0_stat", SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                .AddResource(SHADER_TYPE_VERTEX, "tex0_mut", SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                .AddResource(SHADER_TYPE_VERTEX, "tex0_dyn", SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)

                .AddResource(SHADER_TYPE_VERTEX, "cbInlinePositions", kNumPosConstants, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, PosType, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS);

            if (NumSignatures == 2)
            {
                pDevice->CreatePipelineResourceSignature(SignDesc, &pPosSign);
                ASSERT_TRUE(pPosSign);

                SignDesc.Clear();
                SignDesc.Name         = "Inline constants test 2";
                SignDesc.BindingIndex = 1;
            }

            SignDesc.AddResource(SHADER_TYPE_VERTEX, "cb1_stat", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                .AddResource(SHADER_TYPE_VERTEX, "cb1_mut", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                .AddResource(SHADER_TYPE_VERTEX, "cb1_dyn", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                .AddResource(SHADER_TYPE_VERTEX, "tex1_stat", SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                .AddResource(SHADER_TYPE_VERTEX, "tex1_mut", SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                .AddResource(SHADER_TYPE_VERTEX, "tex1_dyn", SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)

                .AddResource(SHADER_TYPE_VERTEX, "cbInlineColors", kNumColConstants, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, ColType, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS)

                .AddResource(SHADER_TYPE_VERTEX, "cb2_stat", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                .AddResource(SHADER_TYPE_VERTEX, "cb2_mut", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                .AddResource(SHADER_TYPE_VERTEX, "cb2_dyn", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                .AddResource(SHADER_TYPE_VERTEX, "tex2_stat", SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                .AddResource(SHADER_TYPE_VERTEX, "tex2_mut", SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                .AddResource(SHADER_TYPE_VERTEX, "tex2_dyn", SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

            if (NumSignatures == 1)
            {
                pDevice->CreatePipelineResourceSignature(SignDesc, &pPosSign);
                ASSERT_TRUE(pPosSign);
                pColSign = pPosSign;
            }
            else if (NumSignatures == 2)
            {
                pDevice->CreatePipelineResourceSignature(SignDesc, &pColSign);
                ASSERT_TRUE(pColSign);
            }
            else
            {
                GTEST_FAIL() << "Invalid number of signatures: " << NumSignatures;
            }

            pPosSign->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cb0_stat")->Set(pConstBuffer);
            pPosSign->GetStaticVariableByName(SHADER_TYPE_VERTEX, "tex0_stat")->Set(pTexSRV);
            pColSign->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cb1_stat")->Set(pConstBuffer);
            pColSign->GetStaticVariableByName(SHADER_TYPE_VERTEX, "tex1_stat")->Set(pTexSRV);
            pColSign->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cb2_stat")->Set(pConstBuffer);
            pColSign->GetStaticVariableByName(SHADER_TYPE_VERTEX, "tex2_stat")->Set(pTexSRV);

            GraphicsPipelineStateCreateInfoX PsoCI{"Inline constants test"};
            PsoCI
                .AddRenderTarget(pSwapChain->GetDesc().ColorBufferFormat)
                .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                .AddShader(sm_Res.pVS)
                .AddShader(sm_Res.pPS)
                .AddSignature(pPosSign);
            if (NumSignatures == 2)
            {
                PsoCI.AddSignature(pColSign);
            }
            PsoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

            RefCntAutoPtr<IPipelineState> pPSO;
            pDevice->CreateGraphicsPipelineState(PsoCI, &pPSO);
            ASSERT_TRUE(pPSO);

            if (PosType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                IShaderResourceVariable* pVar = pPosSign->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbInlinePositions");
                ASSERT_TRUE(pVar);
                pVar->SetInlineConstants(g_Positions, 0, kNumPosConstants);
            }

            if (ColType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                IShaderResourceVariable* pVar = pColSign->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbInlineColors");
                ASSERT_TRUE(pVar);
                pVar->SetInlineConstants(g_Colors, 0, kNumColConstants);
            }

            RefCntAutoPtr<IShaderResourceBinding> pPosSRB;
            pPosSign->CreateShaderResourceBinding(&pPosSRB, true);
            ASSERT_TRUE(pPosSRB);

            RefCntAutoPtr<IShaderResourceBinding> pColSRB;
            if (NumSignatures == 1)
            {
                pColSRB = pPosSRB;
            }
            else if (NumSignatures == 2)
            {
                pColSign->CreateShaderResourceBinding(&pColSRB, true);
                ASSERT_TRUE(pColSRB);
            }

            pPosSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cb0_mut")->Set(pConstBuffer);
            pPosSRB->GetVariableByName(SHADER_TYPE_VERTEX, "tex0_mut")->Set(pTexSRV);
            pColSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cb1_mut")->Set(pConstBuffer);
            pColSRB->GetVariableByName(SHADER_TYPE_VERTEX, "tex1_mut")->Set(pTexSRV);
            pColSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cb2_mut")->Set(pConstBuffer);
            pColSRB->GetVariableByName(SHADER_TYPE_VERTEX, "tex2_mut")->Set(pTexSRV);
            pPosSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cb0_dyn")->Set(pConstBuffer);
            pPosSRB->GetVariableByName(SHADER_TYPE_VERTEX, "tex0_dyn")->Set(pTexSRV);
            pColSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cb1_dyn")->Set(pConstBuffer);
            pColSRB->GetVariableByName(SHADER_TYPE_VERTEX, "tex1_dyn")->Set(pTexSRV);
            pColSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cb2_dyn")->Set(pConstBuffer);
            pColSRB->GetVariableByName(SHADER_TYPE_VERTEX, "tex2_dyn")->Set(pTexSRV);

            IShaderResourceVariable* pPosVar = nullptr;
            if (PosType != SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                pPosVar = pPosSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbInlinePositions");
                ASSERT_TRUE(pPosVar);
            }

            IShaderResourceVariable* pColVar = nullptr;
            if (ColType != SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                pColVar = pColSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbInlineColors");
                ASSERT_TRUE(pColVar);
            }

            ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
            pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);


            if (pColVar != nullptr)
            {
                // Set first half of color constants before committing SRB
                pColVar->SetInlineConstants(g_Colors, 0, kNumColConstants / 2);
            }

            pContext->CommitShaderResources(pPosSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            if (NumSignatures == 2)
            {
                pContext->TransitionShaderResources(pColSRB);
                pContext->CommitShaderResources(pColSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
            }

            if (pColVar != nullptr)
            {
                // Set second half of color constants after committing SRB
                pColVar->SetInlineConstants(g_Colors[0].Data() + kNumColConstants / 2, kNumColConstants / 2, kNumColConstants / 2);
            }

            pContext->SetPipelineState(pPSO);

            if (pPosVar == nullptr)
            {
                // Draw both triangles as positions are static
                pContext->Draw({6, DRAW_FLAG_VERIFY_ALL});
            }
            else
            {
                // Draw first triangle
                pPosVar->SetInlineConstants(g_Positions, 0, kNumPosConstants / 2);
                pContext->Draw({3, DRAW_FLAG_VERIFY_ALL});

                // Draw second triangle
                pPosVar->SetInlineConstants(g_Positions[0].Data() + kNumPosConstants / 2, 0, kNumPosConstants / 2);
                pContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
            }

            Present();

            std::cout << TestingEnvironment::GetCurrentTestStatusString() << ' '
                      << " Pos " << GetShaderVariableTypeLiteralName(PosType) << ','
                      << " Col " << GetShaderVariableTypeLiteralName(ColType) << std::endl;
        }
    }
}

TEST_F(InlineConstants, ResourceSignature)
{
    TestSignatures(1);
}

TEST_F(InlineConstants, TwoResourceSignatures)
{
    TestSignatures(2);
}

constexpr Uint32 kCacheContentVersion = 7;

RefCntAutoPtr<IRenderStateCache> CreateCache(IRenderDevice*                   pDevice,
                                             bool                             HotReload,
                                             bool                             OptimizeGLShaders,
                                             IDataBlob*                       pCacheData           = nullptr,
                                             IShaderSourceInputStreamFactory* pShaderReloadFactory = nullptr)
{
    RenderStateCacheCreateInfo CacheCI{
        pDevice,
        GPUTestingEnvironment::GetInstance()->GetArchiverFactory(),
        RENDER_STATE_CACHE_LOG_LEVEL_VERBOSE,
        RENDER_STATE_CACHE_FILE_HASH_MODE_BY_CONTENT,
        HotReload,
    };

    RefCntAutoPtr<IRenderStateCache> pCache;
    CreateRenderStateCache(CacheCI, &pCache);

    if (pCacheData != nullptr)
        pCache->Load(pCacheData, kCacheContentVersion);

    return pCache;
}


void CreateShadersFromCache(IRenderStateCache* pCache, bool PresentInCache, IShader** ppVS, IShader** ppPS)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

    {
        ShaderCI.Desc       = {"Inline constants test", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.Source     = HLSL::InlineConstantsTest_VS.c_str();
        if (pCache != nullptr)
        {
            EXPECT_EQ(pCache->CreateShader(ShaderCI, ppVS), PresentInCache);
        }
        else
        {
            pDevice->CreateShader(ShaderCI, ppVS);
            EXPECT_EQ(PresentInCache, false);
        }
    }

    {
        ShaderCI.Desc       = {"Inline constants test", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.Source     = HLSL::DrawTest_PS.c_str();
        if (pCache != nullptr)
        {
            EXPECT_EQ(pCache->CreateShader(ShaderCI, ppPS), PresentInCache);
        }
        else
        {
            pDevice->CreateShader(ShaderCI, ppPS);
            EXPECT_EQ(PresentInCache, false);
        }
    }
}

void CreatePSOFromCache(IRenderStateCache* pCache, bool PresentInCache, IShader* pVS, IShader* pPS, IPipelineState** ppPSO)
{
    GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
    ISwapChain*            pSwapChain = pEnv->GetSwapChain();

    GraphicsPipelineStateCreateInfo PsoCI;
    PsoCI.PSODesc.Name = "Render State Cache Test";

    PsoCI.pVS = pVS;
    PsoCI.pPS = pPS;

    PsoCI.GraphicsPipeline.NumRenderTargets = 1;
    PsoCI.GraphicsPipeline.RTVFormats[0]    = pSwapChain->GetDesc().ColorBufferFormat;

    PsoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    static ShaderResourceVariableDesc Vars[] =
        {
            {SHADER_TYPE_VERTEX, "cbInlinePositions", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_FLAG_INLINE_CONSTANTS},
            {SHADER_TYPE_VERTEX, "cbInlineColors", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_FLAG_INLINE_CONSTANTS},
        };
    PsoCI.PSODesc.ResourceLayout.Variables    = Vars;
    PsoCI.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    if (pCache != nullptr)
    {
        bool PSOFound = pCache->CreateGraphicsPipelineState(PsoCI, ppPSO);
        EXPECT_EQ(PSOFound, PresentInCache);
    }
    else
    {
        EXPECT_FALSE(PresentInCache);
        pEnv->GetDevice()->CreateGraphicsPipelineState(PsoCI, ppPSO);
        ASSERT_NE(*ppPSO, nullptr);
    }

    if (*ppPSO != nullptr && (*ppPSO)->GetStatus() == PIPELINE_STATE_STATUS_READY)
    {
        const PipelineStateDesc& Desc = (*ppPSO)->GetDesc();
        EXPECT_EQ(PsoCI.PSODesc, Desc);
    }
}

void InlineConstants::VerifyPSOFromCache(IPipelineState*         pPSO,
                                         IShaderResourceBinding* pSRB)
{
    GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pContext   = pEnv->GetDeviceContext();
    ISwapChain*            pSwapChain = pEnv->GetSwapChain();

    const float ClearColor[] = {sm_Rnd(), sm_Rnd(), sm_Rnd(), sm_Rnd()};
    RenderDrawCommandReference(pSwapChain, ClearColor);

    ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    RefCntAutoPtr<IShaderResourceBinding> _pSRB;
    if (pSRB == nullptr)
    {
        pPSO->CreateShaderResourceBinding(&_pSRB, true);
        pSRB = _pSRB;
    }

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    IShaderResourceVariable* pColVar = pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbInlineColors");
    ASSERT_TRUE(pColVar);
    pColVar->SetInlineConstants(g_Colors, 0, kNumColConstants);

    IShaderResourceVariable* pPosVar = pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbInlinePositions");
    ASSERT_TRUE(pPosVar);
    pPosVar->SetInlineConstants(g_Positions, 0, kNumPosConstants / 2);
    pContext->Draw({3, DRAW_FLAG_VERIFY_ALL});

    pPosVar->SetInlineConstants(g_Positions[0].Data() + kNumPosConstants / 2, 0, kNumPosConstants / 2);
    pContext->Draw({3, DRAW_FLAG_VERIFY_ALL});

    Present();
}

TEST_F(InlineConstants, RenderStateCache)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();

    if (pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D12)
    {
        GTEST_SKIP();
    }

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/RenderStateCache", &pShaderSourceFactory);
    ASSERT_TRUE(pShaderSourceFactory);

    RefCntAutoPtr<IShader> pUncachedVS, pUncachedPS;
    CreateShadersFromCache(nullptr, false, &pUncachedVS, &pUncachedPS);
    ASSERT_NE(pUncachedVS, nullptr);
    ASSERT_NE(pUncachedPS, nullptr);

    RefCntAutoPtr<IPipelineState> pRefPSO;
    CreatePSOFromCache(nullptr, false, pUncachedVS, pUncachedPS, &pRefPSO);
    ASSERT_NE(pRefPSO, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pRefSRB;
    pRefPSO->CreateShaderResourceBinding(&pRefSRB);

    RefCntAutoPtr<IDataBlob> pData;
    for (Uint32 pass = 0; pass < 3; ++pass)
    {
        // 0: empty cache
        // 1: loaded cache
        // 2: reloaded cache (loaded -> stored -> loaded)

        RefCntAutoPtr<IRenderStateCache> pCache = CreateCache(pDevice, false, false, pData);
        ASSERT_TRUE(pCache);

        RefCntAutoPtr<IShader> pVS1, pPS1;
        CreateShadersFromCache(pCache, pData != nullptr, &pVS1, &pPS1);
        ASSERT_NE(pVS1, nullptr);
        ASSERT_NE(pPS1, nullptr);

        RefCntAutoPtr<IPipelineState> pPSO;
        CreatePSOFromCache(pCache, pData != nullptr, pVS1, pPS1, &pPSO);
        ASSERT_NE(pPSO, nullptr);
        ASSERT_EQ(pPSO->GetStatus(), PIPELINE_STATE_STATUS_READY);
        EXPECT_TRUE(pRefPSO->IsCompatibleWith(pPSO));
        EXPECT_TRUE(pPSO->IsCompatibleWith(pRefPSO));

        VerifyPSOFromCache(pPSO, nullptr);
        VerifyPSOFromCache(pPSO, pRefSRB);

        {
            RefCntAutoPtr<IPipelineState> pPSO2;
            CreatePSOFromCache(pCache, true, pVS1, pPS1, &pPSO2);
            EXPECT_EQ(pPSO, pPSO2);
        }

        {
            RefCntAutoPtr<IPipelineState> pPSO2;

            bool PresentInCache = pData != nullptr;
#if !DILIGENT_DEBUG
            if (pDevice->GetDeviceInfo().IsD3DDevice())
            {
                // For some reason, hash computation consistency depends on D3DCOMPILE_DEBUG flag and differs between debug and release builds
                PresentInCache = true;
            }
#endif
            CreatePSOFromCache(pCache, PresentInCache, pUncachedVS, pUncachedPS, &pPSO2);
            ASSERT_NE(pPSO2, nullptr);
            ASSERT_EQ(pPSO2->GetStatus(), PIPELINE_STATE_STATUS_READY);
            EXPECT_TRUE(pRefPSO->IsCompatibleWith(pPSO2));
            EXPECT_TRUE(pPSO2->IsCompatibleWith(pRefPSO));
            VerifyPSOFromCache(pPSO2, nullptr);
            VerifyPSOFromCache(pPSO2, pRefSRB);
        }

        pData.Release();
        pCache->WriteToBlob(pass == 0 ? kCacheContentVersion : ~0u, &pData);
    }
}

} // namespace
