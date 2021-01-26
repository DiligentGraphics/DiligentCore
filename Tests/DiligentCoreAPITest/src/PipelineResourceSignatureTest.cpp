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

#include <array>
#include <vector>

#include "TestingEnvironment.hpp"
#include "ShaderMacroHelper.hpp"

#include "gtest/gtest.h"

#include "InlineShaders/PipelineResourceSignatureTestHLSL.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace Diligent
{

class PipelineResourceSignatureTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        auto* pEnv = TestingEnvironment::GetInstance();

        {
            auto pRenderTarget = pEnv->CreateTexture("ShaderResourceLayoutTest: test RTV", TEX_FORMAT_RGBA8_UNORM, BIND_RENDER_TARGET, 512, 512);
            ASSERT_NE(pRenderTarget, nullptr);
            pRTV = pRenderTarget->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        }

        {
            auto pTexture = pEnv->CreateTexture("ShaderResourceLayoutTest: test RTV", TEX_FORMAT_RGBA8_UNORM, BIND_SHADER_RESOURCE, 512, 512);
            ASSERT_NE(pTexture, nullptr);
            pTexSRV = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
        {
            SamplerDesc SamDesc;
            pEnv->GetDevice()->CreateSampler(SamDesc, &pSampler);
            pTexSRV->SetSampler(pSampler);
        }
    }

    static void TearDownTestSuite()
    {
        pRTV.Release();
        pTexSRV.Release();
        TestingEnvironment::GetInstance()->Reset();
    }

    static RefCntAutoPtr<IPipelineState> CreateGraphicsPSO(IShader*                                                         pVS,
                                                           IShader*                                                         pPS,
                                                           std::initializer_list<RefCntAutoPtr<IPipelineResourceSignature>> pSignatures)
    {
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        PSOCreateInfo.PSODesc.Name = "Resource signature test";

        std::vector<IPipelineResourceSignature*> Signatures;
        for (auto& pPRS : pSignatures)
            Signatures.push_back(pPRS.RawPtr<IPipelineResourceSignature>());

        PSOCreateInfo.ppResourceSignatures    = Signatures.data();
        PSOCreateInfo.ResourceSignaturesCount = static_cast<Uint32>(Signatures.size());

        auto* pEnv    = TestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;

        auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

        GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        GraphicsPipeline.NumRenderTargets  = 1;
        GraphicsPipeline.RTVFormats[0]     = TEX_FORMAT_RGBA8_UNORM;
        GraphicsPipeline.DSVFormat         = TEX_FORMAT_UNKNOWN;

        GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        RefCntAutoPtr<IPipelineState> pPSO;
        pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
        return pPSO;
    }


    static RefCntAutoPtr<ITextureView> pRTV;
    static RefCntAutoPtr<ITextureView> pTexSRV;
    static RefCntAutoPtr<ISampler>     pSampler;
};

RefCntAutoPtr<ITextureView> PipelineResourceSignatureTest::pRTV;
RefCntAutoPtr<ITextureView> PipelineResourceSignatureTest::pTexSRV;
RefCntAutoPtr<ISampler>     PipelineResourceSignatureTest::pSampler;


#define SET_STATIC_VAR(PRS, ShaderFlags, VarName, SetMethod, ...)                                \
    do                                                                                           \
    {                                                                                            \
        auto pStaticVar = PRS->GetStaticVariableByName(ShaderFlags, VarName);                    \
        EXPECT_NE(pStaticVar, nullptr) << "Unable to find static variable '" << VarName << '\''; \
        if (pStaticVar != nullptr)                                                               \
            pStaticVar->SetMethod(__VA_ARGS__);                                                  \
    } while (false)


#define SET_SRB_VAR(SRB, ShaderFlags, VarName, SetMethod, ...)                          \
    do                                                                                  \
    {                                                                                   \
        auto pVar = SRB->GetVariableByName(ShaderFlags, VarName);                       \
        EXPECT_NE(pVar, nullptr) << "Unable to find SRB variable '" << VarName << '\''; \
        if (pVar != nullptr)                                                            \
            pVar->SetMethod(__VA_ARGS__);                                               \
    } while (false)

TEST_F(PipelineResourceSignatureTest, VariableTypes)
{
    auto* const pEnv     = TestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto*       pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/PipelineResourceSignature", &pShaderSourceFactory);
    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.FilePath                   = "VariableTypes.hlsl";

    static constexpr Uint32 StaticTexArraySize  = 2;
    static constexpr Uint32 MutableTexArraySize = 4;
    static constexpr Uint32 DynamicTexArraySize = 3;
    ShaderMacroHelper       Macros;
    Macros.AddShaderMacro("STATIC_TEX_ARRAY_SIZE", static_cast<int>(StaticTexArraySize));
    Macros.AddShaderMacro("MUTABLE_TEX_ARRAY_SIZE", static_cast<int>(MutableTexArraySize));
    Macros.AddShaderMacro("DYNAMIC_TEX_ARRAY_SIZE", static_cast<int>(DynamicTexArraySize));
    ShaderCI.Macros = Macros;

    RefCntAutoPtr<IShader> pVS, pPS;
    {
        ShaderCI.Desc.Name       = "Res signature variable types test: VS";
        ShaderCI.EntryPoint      = "VSMain";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);
    }

    {
        ShaderCI.Desc.Name       = "Res signature variable types test: PS";
        ShaderCI.EntryPoint      = "PSMain";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);
    }

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Variable types test";

    constexpr auto SHADER_TYPE_VS_PS = SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL;
    // clang-format off
    PipelineResourceDesc Resources[]
    {
        {SHADER_TYPE_VS_PS, "g_Tex2D_Static", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VS_PS, "g_Tex2D_Mut",    1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VS_PS, "g_Tex2D_Dyn",    1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VS_PS, "g_Tex2DArr_Static", StaticTexArraySize,  SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VS_PS, "g_Tex2DArr_Mut",    MutableTexArraySize, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VS_PS, "g_Tex2DArr_Dyn",    DynamicTexArraySize, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VS_PS, "g_Sampler",         1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    };
    // clang-format on
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);

    RefCntAutoPtr<IPipelineResourceSignature> pPRS;
    pDevice->CreatePipelineResourceSignature(PRSDesc, &pPRS);
    ASSERT_TRUE(pPRS);

    auto pPSO = CreateGraphicsPSO(pVS, pPS, {pPRS});
    ASSERT_TRUE(pPSO);

    std::array<IDeviceObject*, 4> pTexSRVs = {pTexSRV, pTexSRV, pTexSRV, pTexSRV};

    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Tex2D_Static", Set, pTexSRVs[0]);
    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Tex2DArr_Static", SetArray, pTexSRVs.data(), 0, StaticTexArraySize);
    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Sampler", Set, pSampler);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPRS->CreateShaderResourceBinding(&pSRB, true);

    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2D_Mut", Set, pTexSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2DArr_Mut", SetArray, pTexSRVs.data(), 0, MutableTexArraySize);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2D_Dyn", Set, pTexSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2DArr_Dyn", SetArray, pTexSRVs.data(), 0, DynamicTexArraySize);

    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);
}


TEST_F(PipelineResourceSignatureTest, MultiSignatures)
{
    auto* const pEnv     = TestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto*       pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/PipelineResourceSignature", &pShaderSourceFactory);
    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.FilePath                   = "MultiSignatures.hlsl";

    RefCntAutoPtr<IShader> pVS, pPS;
    {
        ShaderCI.Desc.Name       = "Multi signatures test: VS";
        ShaderCI.EntryPoint      = "VSMain";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);
    }

    {
        ShaderCI.Desc.Name       = "Multi signatures test: PS";
        ShaderCI.EntryPoint      = "PSMain";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);
    }

    PipelineResourceSignatureDesc PRSDesc;

    RefCntAutoPtr<IPipelineResourceSignature> pPRS[3];
    RefCntAutoPtr<IShaderResourceBinding>     pSRB[3];
    std::vector<PipelineResourceDesc>         Resources[3];
    // clang-format off
    Resources[0].emplace_back(SHADER_TYPE_VERTEX, "g_Tex2D_1", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    Resources[0].emplace_back(SHADER_TYPE_PIXEL,  "g_Tex2D_2", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);
    Resources[0].emplace_back(SHADER_TYPE_PIXEL,  "g_Tex2D_3", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

    Resources[1].emplace_back(SHADER_TYPE_PIXEL,  "g_Tex2D_1", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);
    Resources[1].emplace_back(SHADER_TYPE_VERTEX, "g_Tex2D_2", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
    Resources[1].emplace_back(SHADER_TYPE_VERTEX, "g_Tex2D_3", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

    Resources[2].emplace_back(SHADER_TYPE_PIXEL,  "g_Tex2D_4", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);
    Resources[2].emplace_back(SHADER_TYPE_VERTEX, "g_Tex2D_4", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
    Resources[2].emplace_back(SHADER_TYPE_PIXEL | SHADER_TYPE_VERTEX, "g_Sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    // clang-format on

    for (Uint32 i = 0; i < _countof(pPRS); ++i)
    {
        std::string PRSName  = "Multi signatures " + std::to_string(i);
        PRSDesc.Name         = PRSName.c_str();
        PRSDesc.BindingIndex = static_cast<Uint8>(i);

        PRSDesc.Resources    = Resources[i].data();
        PRSDesc.NumResources = static_cast<Uint32>(Resources[i].size());

        pDevice->CreatePipelineResourceSignature(PRSDesc, &pPRS[i]);
        ASSERT_TRUE(pPRS[i]);
    }

    auto pPSO = CreateGraphicsPSO(pVS, pPS, {pPRS[0], pPRS[1], pPRS[2]});
    ASSERT_TRUE(pPSO);

    SET_STATIC_VAR(pPRS[0], SHADER_TYPE_VERTEX, "g_Tex2D_1", Set, pTexSRV);
    SET_STATIC_VAR(pPRS[1], SHADER_TYPE_VERTEX, "g_Tex2D_3", Set, pTexSRV);
    SET_STATIC_VAR(pPRS[2], SHADER_TYPE_PIXEL, "g_Sampler", Set, pSampler);
    for (Uint32 i = 0; i < _countof(pPRS); ++i)
    {
        pPRS[i]->CreateShaderResourceBinding(&pSRB[i], true);
    }

    SET_SRB_VAR(pSRB[0], SHADER_TYPE_PIXEL, "g_Tex2D_2", Set, pTexSRV);
    SET_SRB_VAR(pSRB[1], SHADER_TYPE_PIXEL, "g_Tex2D_1", Set, pTexSRV);
    SET_SRB_VAR(pSRB[2], SHADER_TYPE_PIXEL, "g_Tex2D_4", Set, pTexSRV);

    SET_SRB_VAR(pSRB[0], SHADER_TYPE_PIXEL, "g_Tex2D_3", Set, pTexSRV);
    SET_SRB_VAR(pSRB[1], SHADER_TYPE_VERTEX, "g_Tex2D_2", Set, pTexSRV);
    SET_SRB_VAR(pSRB[2], SHADER_TYPE_VERTEX, "g_Tex2D_4", Set, pTexSRV);

    for (Uint32 i = 0; i < _countof(pSRB); ++i)
    {
        pContext->CommitShaderResources(pSRB[i], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);
}


TEST_F(PipelineResourceSignatureTest, StaticSamplers)
{
    auto* const pEnv     = TestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto*       pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/PipelineResourceSignature", &pShaderSourceFactory);
    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.FilePath                   = "StaticSamplers.hlsl";

    RefCntAutoPtr<IShader> pVS, pPS;
    {
        ShaderCI.Desc.Name       = "Res signature static samplers test: VS";
        ShaderCI.EntryPoint      = "VSMain";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);
    }

    {
        ShaderCI.Desc.Name       = "Res signature static samplers test: PS";
        ShaderCI.EntryPoint      = "PSMain";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);
    }

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Variable types test";

    constexpr auto SHADER_TYPE_VS_PS = SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL;
    // clang-format off
    PipelineResourceDesc Resources[]
    {
        {SHADER_TYPE_VS_PS, "g_Tex2D_Static", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VS_PS, "g_Tex2D_Mut",    1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VS_PS, "g_Tex2D_Dyn",    1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    // clang-format on
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);

    ImmutableSamplerDesc ImmutableSamplers[] = //
        {
            {SHADER_TYPE_VERTEX, "g_Sampler", SamplerDesc{}},
            {SHADER_TYPE_PIXEL, "g_Sampler", SamplerDesc{}} //
        };
    PRSDesc.ImmutableSamplers    = ImmutableSamplers;
    PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

    RefCntAutoPtr<IPipelineResourceSignature> pPRS;
    pDevice->CreatePipelineResourceSignature(PRSDesc, &pPRS);
    ASSERT_TRUE(pPRS);

    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Tex2D_Static", Set, pTexSRV);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPRS->CreateShaderResourceBinding(&pSRB, true);

    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2D_Mut", Set, pTexSRV);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2D_Dyn", Set, pTexSRV);

    auto pPSO = CreateGraphicsPSO(pVS, pPS, {pPRS});
    ASSERT_TRUE(pPSO);

    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);
}


TEST_F(PipelineResourceSignatureTest, StaticSamplers2)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<IPipelineResourceSignature> pSignature1;
    {
        const PipelineResourceDesc Resources[] = {
            {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "Constants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};

        PipelineResourceSignatureDesc Desc;
        Desc.Resources    = Resources;
        Desc.NumResources = _countof(Resources);
        Desc.BindingIndex = 0;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignature1);
        ASSERT_NE(pSignature1, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignature2;
    {
        const PipelineResourceDesc Resources[] = {
            {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};

        SamplerDesc SamLinearWrapDesc{
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP};
        ImmutableSamplerDesc ImmutableSamplers[] = {//
                                                    {SHADER_TYPE_PIXEL, "g_Texture", SamLinearWrapDesc},
                                                    {SHADER_TYPE_PIXEL, "g_Sampler", SamLinearWrapDesc}};

        PipelineResourceSignatureDesc Desc;
        Desc.Resources                  = Resources;
        Desc.NumResources               = _countof(Resources);
        Desc.ImmutableSamplers          = ImmutableSamplers;
        Desc.NumImmutableSamplers       = _countof(ImmutableSamplers);
        Desc.UseCombinedTextureSamplers = true;
        Desc.CombinedSamplerSuffix      = "_sampler";
        Desc.BindingIndex               = 2;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignature2);
        ASSERT_NE(pSignature2, nullptr);
    }

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.Name = "PRS test";

    PSODesc.PipelineType                          = PIPELINE_TYPE_GRAPHICS;
    GraphicsPipeline.NumRenderTargets             = 1;
    GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_RGBA8_UNORM;
    GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler             = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "PRS test - VS";
        ShaderCI.Source          = HLSL::PRSTest1_VS.c_str();
        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "PRS test - PS";
        ShaderCI.Source          = HLSL::PRSTest1_PS.c_str();
        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    IPipelineResourceSignature* Signatures[] = {pSignature1, pSignature2};

    PSOCreateInfo.ppResourceSignatures    = Signatures;
    PSOCreateInfo.ResourceSignaturesCount = _countof(Signatures);

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    ASSERT_EQ(pPSO->GetResourceSignatureCount(), 3u);
    ASSERT_EQ(pPSO->GetResourceSignature(0), pSignature1);
    ASSERT_EQ(pPSO->GetResourceSignature(1), nullptr);
    ASSERT_EQ(pPSO->GetResourceSignature(2), pSignature2);

    RefCntAutoPtr<IShaderResourceBinding> pSRB1;
    pSignature1->CreateShaderResourceBinding(&pSRB1, true);
    ASSERT_NE(pSRB1, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pSRB2;
    pSignature2->CreateShaderResourceBinding(&pSRB2, true);
    ASSERT_NE(pSRB2, nullptr);

    RefCntAutoPtr<IBuffer> pConstBuf;
    {
        float      ConstData[8] = {};
        BufferDesc BuffDesc;
        BuffDesc.uiSizeInBytes = sizeof(ConstData);
        BuffDesc.BindFlags     = BIND_UNIFORM_BUFFER;
        BuffDesc.Usage         = USAGE_IMMUTABLE;
        BufferData BuffData{ConstData, sizeof(ConstData)};
        pDevice->CreateBuffer(BuffDesc, &BuffData, &pConstBuf);
        ASSERT_NE(pConstBuf, nullptr);
    }

    RefCntAutoPtr<ITexture> pTexture;
    {
        TextureDesc TexDesc;
        TexDesc.Type      = RESOURCE_DIM_TEX_2D;
        TexDesc.Width     = 256;
        TexDesc.Height    = 256;
        TexDesc.Usage     = USAGE_IMMUTABLE;
        TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE;

        std::vector<Uint8> Data(TexDesc.Width * TexDesc.Height * 4, 128);
        TextureSubResData  TexSubData{Data.data(), TexDesc.Width * 4};
        TextureData        TexData{&TexSubData, 1};
        pDevice->CreateTexture(TexDesc, &TexData, &pTexture);
        ASSERT_NE(pTexture, nullptr);
    }

    pSRB1->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(pConstBuf);
    pSRB2->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->CommitShaderResources(pSRB1, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CommitShaderResources(pSRB2, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    DrawAttribs drawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(drawAttrs);
}


TEST_F(PipelineResourceSignatureTest, SRBCompatibility)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<IPipelineResourceSignature> pSignature1;
    {
        const PipelineResourceDesc Resources[] = {
            {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "Constants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};

        PipelineResourceSignatureDesc Desc;
        Desc.Resources    = Resources;
        Desc.NumResources = _countof(Resources);
        Desc.BindingIndex = 0;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignature1);
        ASSERT_NE(pSignature1, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignature2;
    {
        const PipelineResourceDesc Resources[] = {
            {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL, "g_Texture_sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};

        SamplerDesc SamLinearWrapDesc{
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP};
        ImmutableSamplerDesc ImmutableSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", SamLinearWrapDesc}};

        PipelineResourceSignatureDesc Desc;
        Desc.Resources                  = Resources;
        Desc.NumResources               = _countof(Resources);
        Desc.ImmutableSamplers          = ImmutableSamplers;
        Desc.NumImmutableSamplers       = _countof(ImmutableSamplers);
        Desc.UseCombinedTextureSamplers = true;
        Desc.CombinedSamplerSuffix      = "_sampler";
        Desc.BindingIndex               = 2;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignature2);
        ASSERT_NE(pSignature2, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignature3;
    {
        const PipelineResourceDesc Resources[] = {
            {SHADER_TYPE_PIXEL, "g_Texture2", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL, "g_Texture2_sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};

        SamplerDesc SamLinearWrapDesc{
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP};
        ImmutableSamplerDesc ImmutableSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture2", SamLinearWrapDesc}};

        PipelineResourceSignatureDesc Desc;
        Desc.Resources                  = Resources;
        Desc.NumResources               = _countof(Resources);
        Desc.ImmutableSamplers          = ImmutableSamplers;
        Desc.NumImmutableSamplers       = _countof(ImmutableSamplers);
        Desc.UseCombinedTextureSamplers = true;
        Desc.CombinedSamplerSuffix      = "_sampler";
        Desc.BindingIndex               = 3;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignature3);
        ASSERT_NE(pSignature3, nullptr);
    }

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.Name = "PRS test";

    PSODesc.PipelineType                          = PIPELINE_TYPE_GRAPHICS;
    GraphicsPipeline.NumRenderTargets             = 1;
    GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_RGBA8_UNORM;
    GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler             = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "PRS test - VS";
        ShaderCI.Source          = HLSL::PRSTest1_VS.c_str();
        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "PRS test - PS";
        ShaderCI.Source          = HLSL::PRSTest1_PS.c_str();
        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);
    }

    RefCntAutoPtr<IShader> pPS2;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "PRS test 2 - PS";
        ShaderCI.Source          = HLSL::PRSTest2_PS.c_str();
        pDevice->CreateShader(ShaderCI, &pPS2);
        ASSERT_NE(pPS, nullptr);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    IPipelineResourceSignature* Signatures1[] = {pSignature1, pSignature2};

    PSOCreateInfo.ppResourceSignatures    = Signatures1;
    PSOCreateInfo.ResourceSignaturesCount = _countof(Signatures1);

    RefCntAutoPtr<IPipelineState> pPSO1;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO1);
    ASSERT_NE(pPSO1, nullptr);

    ASSERT_EQ(pPSO1->GetResourceSignatureCount(), 3u);
    ASSERT_EQ(pPSO1->GetResourceSignature(0), pSignature1);
    ASSERT_EQ(pPSO1->GetResourceSignature(1), nullptr);
    ASSERT_EQ(pPSO1->GetResourceSignature(2), pSignature2);

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS2;

    IPipelineResourceSignature* Signatures2[] = {pSignature1, pSignature2, pSignature3};

    PSOCreateInfo.ppResourceSignatures    = Signatures2;
    PSOCreateInfo.ResourceSignaturesCount = _countof(Signatures2);

    RefCntAutoPtr<IPipelineState> pPSO2;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO2);
    ASSERT_NE(pPSO2, nullptr);

    ASSERT_EQ(pPSO2->GetResourceSignatureCount(), 4u);
    ASSERT_EQ(pPSO2->GetResourceSignature(0), pSignature1);
    ASSERT_EQ(pPSO2->GetResourceSignature(1), nullptr);
    ASSERT_EQ(pPSO2->GetResourceSignature(2), pSignature2);
    ASSERT_EQ(pPSO2->GetResourceSignature(3), pSignature3);

    RefCntAutoPtr<IShaderResourceBinding> pSRB1;
    pSignature1->CreateShaderResourceBinding(&pSRB1, true);
    ASSERT_NE(pSRB1, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pSRB2;
    pSignature2->CreateShaderResourceBinding(&pSRB2, true);
    ASSERT_NE(pSRB2, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pSRB3;
    pSignature3->CreateShaderResourceBinding(&pSRB3, true);
    ASSERT_NE(pSRB3, nullptr);

    RefCntAutoPtr<IBuffer> pConstBuf;
    {
        float      ConstData[8] = {};
        BufferDesc BuffDesc;
        BuffDesc.uiSizeInBytes = sizeof(ConstData);
        BuffDesc.BindFlags     = BIND_UNIFORM_BUFFER;
        BuffDesc.Usage         = USAGE_IMMUTABLE;
        BufferData BuffData{ConstData, sizeof(ConstData)};
        pDevice->CreateBuffer(BuffDesc, &BuffData, &pConstBuf);
        ASSERT_NE(pConstBuf, nullptr);
    }

    RefCntAutoPtr<ITexture> pTexture;
    RefCntAutoPtr<ITexture> pTexture2;
    {
        TextureDesc TexDesc;
        TexDesc.Type      = RESOURCE_DIM_TEX_2D;
        TexDesc.Width     = 256;
        TexDesc.Height    = 256;
        TexDesc.Usage     = USAGE_IMMUTABLE;
        TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE;

        std::vector<Uint8> Data(TexDesc.Width * TexDesc.Height * 4, 128);
        TextureSubResData  TexSubData{Data.data(), TexDesc.Width * 4};
        TextureData        TexData{&TexSubData, 1};
        pDevice->CreateTexture(TexDesc, &TexData, &pTexture);
        ASSERT_NE(pTexture, nullptr);
        pDevice->CreateTexture(TexDesc, &TexData, &pTexture2);
        ASSERT_NE(pTexture2, nullptr);
    }

    pSRB1->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(pConstBuf);
    pSRB2->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    pSRB3->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture2")->Set(pTexture2->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // draw 1
    pContext->CommitShaderResources(pSRB1, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CommitShaderResources(pSRB2, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO1);

    DrawAttribs drawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(drawAttrs);

    // draw 2
    pContext->CommitShaderResources(pSRB3, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    // reuse pSRB1, pSRB2

    pContext->SetPipelineState(pPSO2);

    pContext->Draw(drawAttrs);
}

TEST_F(PipelineResourceSignatureTest, GraphicsAndMeshShader)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();
    if (!pDevice->GetDeviceCaps().Features.MeshShaders)
    {
        GTEST_SKIP() << "Mesh shader is not supported by this device";
    }

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<IPipelineResourceSignature> pSignaturePS;
    {
        const PipelineResourceDesc Resources[] = {
            {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL, "g_Texture_sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};

        SamplerDesc SamLinearWrapDesc{
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP};
        ImmutableSamplerDesc ImmutableSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", SamLinearWrapDesc}};

        PipelineResourceSignatureDesc Desc;
        Desc.Resources                  = Resources;
        Desc.NumResources               = _countof(Resources);
        Desc.ImmutableSamplers          = ImmutableSamplers;
        Desc.NumImmutableSamplers       = _countof(ImmutableSamplers);
        Desc.UseCombinedTextureSamplers = true;
        Desc.CombinedSamplerSuffix      = "_sampler";
        Desc.BindingIndex               = 0;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignaturePS);
        ASSERT_NE(pSignaturePS, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignatureVS;
    {
        const PipelineResourceDesc Resources[] = {
            {SHADER_TYPE_VERTEX, "Constants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};

        PipelineResourceSignatureDesc Desc;
        Desc.Resources    = Resources;
        Desc.NumResources = _countof(Resources);
        Desc.BindingIndex = 1;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignatureVS);
        ASSERT_NE(pSignatureVS, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignatureMS;
    {
        const PipelineResourceDesc Resources[] = {
            {SHADER_TYPE_MESH, "Constants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};

        PipelineResourceSignatureDesc Desc;
        Desc.Resources    = Resources;
        Desc.NumResources = _countof(Resources);
        Desc.BindingIndex = 1;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignatureMS);
        ASSERT_NE(pSignatureMS, nullptr);
    }

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.Name = "Graphics PSO";

    PSODesc.PipelineType                          = PIPELINE_TYPE_GRAPHICS;
    GraphicsPipeline.NumRenderTargets             = 1;
    GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_RGBA8_UNORM;
    GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler             = SHADER_COMPILER_DXC;
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "PRS test - VS";
        ShaderCI.Source          = HLSL::PRSTest3_VS.c_str();
        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "PRS test - PS";
        ShaderCI.Source          = HLSL::PRSTest3_PS.c_str();
        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    IPipelineResourceSignature* GraphicsSignatures[] = {pSignatureVS, pSignaturePS};

    PSOCreateInfo.ppResourceSignatures    = GraphicsSignatures;
    PSOCreateInfo.ResourceSignaturesCount = _countof(GraphicsSignatures);

    RefCntAutoPtr<IPipelineState> pGraphicsPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pGraphicsPSO);
    ASSERT_NE(pGraphicsPSO, nullptr);

    ASSERT_EQ(pGraphicsPSO->GetResourceSignatureCount(), 2u);
    ASSERT_EQ(pGraphicsPSO->GetResourceSignature(0), pSignaturePS);
    ASSERT_EQ(pGraphicsPSO->GetResourceSignature(1), pSignatureVS);


    RefCntAutoPtr<IShader> pMS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_MESH;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "PRS test - MS";
        ShaderCI.Source          = HLSL::PRSTest3_MS.c_str();
        pDevice->CreateShader(ShaderCI, &pMS);
        ASSERT_NE(pMS, nullptr);
    }

    PSODesc.PipelineType               = PIPELINE_TYPE_MESH;
    GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_UNDEFINED; // unused

    PSOCreateInfo.pVS = nullptr;
    PSOCreateInfo.pMS = pMS;
    PSOCreateInfo.pPS = pPS;

    IPipelineResourceSignature* MeshSignatures[] = {pSignatureMS, pSignaturePS};

    PSOCreateInfo.ppResourceSignatures    = MeshSignatures;
    PSOCreateInfo.ResourceSignaturesCount = _countof(MeshSignatures);

    RefCntAutoPtr<IPipelineState> pMeshPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pMeshPSO);
    ASSERT_NE(pMeshPSO, nullptr);

    ASSERT_EQ(pMeshPSO->GetResourceSignatureCount(), 2u);
    ASSERT_EQ(pMeshPSO->GetResourceSignature(0), pSignaturePS);
    ASSERT_EQ(pMeshPSO->GetResourceSignature(1), pSignatureMS);


    RefCntAutoPtr<IBuffer> pConstBuf;
    {
        float      ConstData[8] = {};
        BufferDesc BuffDesc;
        BuffDesc.uiSizeInBytes = sizeof(ConstData);
        BuffDesc.BindFlags     = BIND_UNIFORM_BUFFER;
        BuffDesc.Usage         = USAGE_IMMUTABLE;
        BufferData BuffData{ConstData, sizeof(ConstData)};
        pDevice->CreateBuffer(BuffDesc, &BuffData, &pConstBuf);
        ASSERT_NE(pConstBuf, nullptr);
    }

    RefCntAutoPtr<ITexture> pTexture;
    {
        TextureDesc TexDesc;
        TexDesc.Type      = RESOURCE_DIM_TEX_2D;
        TexDesc.Width     = 256;
        TexDesc.Height    = 256;
        TexDesc.Usage     = USAGE_IMMUTABLE;
        TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE;

        std::vector<Uint8> Data(TexDesc.Width * TexDesc.Height * 4, 128);
        TextureSubResData  TexSubData{Data.data(), TexDesc.Width * 4};
        TextureData        TexData{&TexSubData, 1};
        pDevice->CreateTexture(TexDesc, &TexData, &pTexture);
        ASSERT_NE(pTexture, nullptr);
    }

    RefCntAutoPtr<IShaderResourceBinding> PixelSRB;
    pSignaturePS->CreateShaderResourceBinding(&PixelSRB, true);
    ASSERT_NE(PixelSRB, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> VertexSRB;
    pSignatureVS->CreateShaderResourceBinding(&VertexSRB, true);
    ASSERT_NE(VertexSRB, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> MeshSRB;
    pSignatureMS->CreateShaderResourceBinding(&MeshSRB, true);
    ASSERT_NE(MeshSRB, nullptr);

    PixelSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    VertexSRB->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(pConstBuf);
    MeshSRB->GetVariableByName(SHADER_TYPE_MESH, "Constants")->Set(pConstBuf);

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // draw triangles
    pContext->CommitShaderResources(PixelSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CommitShaderResources(VertexSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pGraphicsPSO);

    DrawAttribs drawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(drawAttrs);

    // draw meshes
    pContext->CommitShaderResources(MeshSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    // reuse PixelSRB

    pContext->SetPipelineState(pMeshPSO);

    DrawMeshAttribs drawMeshAttrs(1, DRAW_FLAG_VERIFY_ALL);
    pContext->DrawMesh(drawMeshAttrs);
}
} // namespace Diligent
