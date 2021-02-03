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
#include "GraphicsAccessories.hpp"
#include "ResourceLayoutTestCommon.hpp"

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
        auto* pEnv    = TestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        {
            auto pRenderTarget = pEnv->CreateTexture("PipelineResourceSignatureTest: test RTV", TEX_FORMAT_RGBA8_UNORM, BIND_RENDER_TARGET, 512, 512);
            ASSERT_NE(pRenderTarget, nullptr);
            pRTV = pRenderTarget->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
            ASSERT_NE(pRTV, nullptr);
        }

        {
            SamplerDesc SamDesc;
            pEnv->GetDevice()->CreateSampler(SamDesc, &pSampler);
        }

        for (size_t i = 0; i < pTexSRVs.size(); ++i)
        {
            auto pTexture = pEnv->CreateTexture("PipelineResourceSignatureTest: test SRV", TEX_FORMAT_RGBA8_UNORM, BIND_SHADER_RESOURCE, 512, 512);
            ASSERT_NE(pTexture, nullptr);
            pTexSRVs[i] = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            ASSERT_NE(pTexSRVs[i], nullptr);
            pTexSRVs[i]->SetSampler(pSampler);

            {
                TextureViewDesc SRVDesc;
                SRVDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
                pTexture->CreateView(SRVDesc, &pTexSRVsNoSampler[i]);
            }
        }

        {
            float      ConstData[256] = {};
            BufferDesc BuffDesc;
            BuffDesc.uiSizeInBytes = sizeof(ConstData);
            BuffDesc.BindFlags     = BIND_UNIFORM_BUFFER;
            BuffDesc.Usage         = USAGE_IMMUTABLE;
            BufferData BuffData{ConstData, sizeof(ConstData)};
            pDevice->CreateBuffer(BuffDesc, &BuffData, &pConstBuff);
            ASSERT_NE(pConstBuff, nullptr);
        }

        pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/PipelineResourceSignature", &pShaderSourceFactory);
    }

    static void TearDownTestSuite()
    {
        pSampler.Release();
        for (auto& pTexSRV : pTexSRVs)
            pTexSRV.Release();
        pConstBuff.Release();
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
        GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;

        RefCntAutoPtr<IPipelineState> pPSO;
        pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
        return pPSO;
    }

    static RefCntAutoPtr<IShader> CreateShaderFromFile(SHADER_TYPE        ShaderType,
                                                       const char*        File,
                                                       const char*        EntryPoint,
                                                       const char*        Name,
                                                       bool               UseCombinedSamplers,
                                                       const ShaderMacro* Macros = nullptr)
    {
        ShaderCreateInfo ShaderCI;
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.FilePath                   = File;
        ShaderCI.Macros                     = Macros;
        ShaderCI.Desc.Name                  = Name;
        ShaderCI.EntryPoint                 = EntryPoint;
        ShaderCI.Desc.ShaderType            = ShaderType;
        ShaderCI.UseCombinedTextureSamplers = UseCombinedSamplers;

        RefCntAutoPtr<IShader> pShader;
        TestingEnvironment::GetInstance()->GetDevice()->CreateShader(ShaderCI, &pShader);
        return pShader;
    }

    static RefCntAutoPtr<IShader> CreateShaderFromFile(SHADER_TYPE        ShaderType,
                                                       const char*        File,
                                                       const char*        EntryPoint,
                                                       const char*        Name,
                                                       const ShaderMacro* Macros = nullptr)
    {
        return CreateShaderFromFile(ShaderType, File, EntryPoint, Name, false, Macros);
    }

    static RefCntAutoPtr<IShader> CreateShaderFromSource(SHADER_TYPE        ShaderType,
                                                         const char*        Source,
                                                         const char*        EntryPoint,
                                                         const char*        Name,
                                                         bool               UseCombinedSamplers,
                                                         const ShaderMacro* Macros = nullptr)
    {
        ShaderCreateInfo ShaderCI;
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.Source                     = Source;
        ShaderCI.Macros                     = Macros;
        ShaderCI.Desc.Name                  = Name;
        ShaderCI.EntryPoint                 = EntryPoint;
        ShaderCI.Desc.ShaderType            = ShaderType;
        ShaderCI.UseCombinedTextureSamplers = UseCombinedSamplers;

        RefCntAutoPtr<IShader> pShader;
        TestingEnvironment::GetInstance()->GetDevice()->CreateShader(ShaderCI, &pShader);
        return pShader;
    }


    static RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    static RefCntAutoPtr<ITextureView>                    pRTV;
    static std::array<RefCntAutoPtr<ITextureView>, 4>     pTexSRVs;
    static std::array<RefCntAutoPtr<ITextureView>, 4>     pTexSRVsNoSampler;
    static RefCntAutoPtr<ISampler>                        pSampler;
    static RefCntAutoPtr<IBuffer>                         pConstBuff;
};

RefCntAutoPtr<IShaderSourceInputStreamFactory> PipelineResourceSignatureTest::pShaderSourceFactory;
RefCntAutoPtr<ITextureView>                    PipelineResourceSignatureTest::pRTV;
std::array<RefCntAutoPtr<ITextureView>, 4>     PipelineResourceSignatureTest::pTexSRVs;
std::array<RefCntAutoPtr<ITextureView>, 4>     PipelineResourceSignatureTest::pTexSRVsNoSampler;
RefCntAutoPtr<ISampler>                        PipelineResourceSignatureTest::pSampler;
RefCntAutoPtr<IBuffer>                         PipelineResourceSignatureTest::pConstBuff;


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

    auto* pSwapChain = pEnv->GetSwapChain();

    float ClearColor[] = {0.125, 0.25, 0.375, 0.5};
    RenderDrawCommandReference(pSwapChain, ClearColor);

    static constexpr Uint32 StaticTexArraySize  = 2;
    static constexpr Uint32 MutableTexArraySize = 4;
    static constexpr Uint32 DynamicTexArraySize = 3;

    ReferenceTextures RefTextures{
        3 + StaticTexArraySize + MutableTexArraySize + DynamicTexArraySize,
        128, 128,
        USAGE_DEFAULT,
        BIND_SHADER_RESOURCE,
        TEXTURE_VIEW_SHADER_RESOURCE //
    };

    // Texture indices for vertex/shader bindings
    static constexpr size_t Tex2D_StaticIdx = 2;
    static constexpr size_t Tex2D_MutIdx    = 0;
    static constexpr size_t Tex2D_DynIdx    = 1;

    static constexpr size_t Tex2DArr_StaticIdx = 7;
    static constexpr size_t Tex2DArr_MutIdx    = 3;
    static constexpr size_t Tex2DArr_DynIdx    = 9;

    ShaderMacroHelper Macros;

    Macros.AddShaderMacro("STATIC_TEX_ARRAY_SIZE", static_cast<int>(StaticTexArraySize));
    Macros.AddShaderMacro("MUTABLE_TEX_ARRAY_SIZE", static_cast<int>(MutableTexArraySize));
    Macros.AddShaderMacro("DYNAMIC_TEX_ARRAY_SIZE", static_cast<int>(DynamicTexArraySize));

    RefTextures.ClearUsedValues();

    // Add macros that define reference colors
    Macros.AddShaderMacro("Tex2D_Static_Ref", RefTextures.GetColor(Tex2D_StaticIdx));
    Macros.AddShaderMacro("Tex2D_Mut_Ref", RefTextures.GetColor(Tex2D_MutIdx));
    Macros.AddShaderMacro("Tex2D_Dyn_Ref", RefTextures.GetColor(Tex2D_DynIdx));

    for (Uint32 i = 0; i < StaticTexArraySize; ++i)
        Macros.AddShaderMacro((std::string{"Tex2DArr_Static_Ref"} + std::to_string(i)).c_str(), RefTextures.GetColor(Tex2DArr_StaticIdx + i));

    for (Uint32 i = 0; i < MutableTexArraySize; ++i)
        Macros.AddShaderMacro((std::string{"Tex2DArr_Mut_Ref"} + std::to_string(i)).c_str(), RefTextures.GetColor(Tex2DArr_MutIdx + i));

    for (Uint32 i = 0; i < DynamicTexArraySize; ++i)
        Macros.AddShaderMacro((std::string{"Tex2DArr_Dyn_Ref"} + std::to_string(i)).c_str(), RefTextures.GetColor(Tex2DArr_DynIdx + i));

    auto pVS = CreateShaderFromFile(SHADER_TYPE_VERTEX, "shaders/ShaderResourceLayout/Textures.hlsl", "VSMain", "PRS variable types test: VS", Macros);
    auto pPS = CreateShaderFromFile(SHADER_TYPE_PIXEL, "shaders/ShaderResourceLayout/Textures.hlsl", "PSMain", "PRS variable types test: PS", Macros);
    ASSERT_TRUE(pVS && pPS);

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

    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Tex2D_Static", Set, RefTextures.GetViewObjects(Tex2D_StaticIdx)[0]);
    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Tex2DArr_Static", SetArray, RefTextures.GetViewObjects(Tex2DArr_StaticIdx), 0, StaticTexArraySize);
    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Sampler", Set, pSampler);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPRS->CreateShaderResourceBinding(&pSRB, true);

    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2D_Mut", Set, RefTextures.GetViewObjects(Tex2D_MutIdx)[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2DArr_Mut", SetArray, RefTextures.GetViewObjects(Tex2DArr_MutIdx), 0, MutableTexArraySize);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2D_Dyn", Set, RefTextures.GetViewObjects(Tex2D_DynIdx)[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2DArr_Dyn", SetArray, RefTextures.GetViewObjects(Tex2DArr_DynIdx), 0, DynamicTexArraySize);

    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    ITextureView* ppRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(ppRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    DrawAttribs DrawAttrs{6, DRAW_FLAG_VERIFY_ALL};
    pContext->Draw(DrawAttrs);

    pSwapChain->Present();
}


TEST_F(PipelineResourceSignatureTest, MultiSignatures)
{
    auto* const pEnv     = TestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto*       pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto pVS = CreateShaderFromFile(SHADER_TYPE_VERTEX, "MultiSignatures.hlsl", "VSMain", "PRS multi signatures test: VS");
    auto pPS = CreateShaderFromFile(SHADER_TYPE_PIXEL, "MultiSignatures.hlsl", "PSMain", "PRS multi signatures test: PS");
    ASSERT_TRUE(pVS && pPS);

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

    SET_STATIC_VAR(pPRS[0], SHADER_TYPE_VERTEX, "g_Tex2D_1", Set, pTexSRVs[0]);
    SET_STATIC_VAR(pPRS[1], SHADER_TYPE_VERTEX, "g_Tex2D_3", Set, pTexSRVs[1]);
    SET_STATIC_VAR(pPRS[2], SHADER_TYPE_PIXEL, "g_Sampler", Set, pSampler);
    for (Uint32 i = 0; i < _countof(pPRS); ++i)
    {
        pPRS[i]->CreateShaderResourceBinding(&pSRB[i], true);
    }

    SET_SRB_VAR(pSRB[0], SHADER_TYPE_PIXEL, "g_Tex2D_2", Set, pTexSRVs[2]);
    SET_SRB_VAR(pSRB[1], SHADER_TYPE_PIXEL, "g_Tex2D_1", Set, pTexSRVs[3]);
    SET_SRB_VAR(pSRB[2], SHADER_TYPE_PIXEL, "g_Tex2D_4", Set, pTexSRVs[0]);

    SET_SRB_VAR(pSRB[0], SHADER_TYPE_PIXEL, "g_Tex2D_3", Set, pTexSRVs[1]);
    SET_SRB_VAR(pSRB[1], SHADER_TYPE_VERTEX, "g_Tex2D_2", Set, pTexSRVs[2]);
    SET_SRB_VAR(pSRB[2], SHADER_TYPE_VERTEX, "g_Tex2D_4", Set, pTexSRVs[3]);

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


TEST_F(PipelineResourceSignatureTest, SingleVarType)
{
    auto* const pEnv     = TestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto*       pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto pVS = CreateShaderFromFile(SHADER_TYPE_VERTEX, "SingleVarType.hlsl", "VSMain", "PRS single var type test: VS");
    auto pPS = CreateShaderFromFile(SHADER_TYPE_PIXEL, "SingleVarType.hlsl", "PSMain", "PRS single var type test: PS");
    ASSERT_TRUE(pVS && pPS);

    for (Uint32 var_type = 0; var_type < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; ++var_type)
    {
        const auto VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(var_type);

        const PipelineResourceDesc Resources[] = //
            {
                {SHADER_TYPE_ALL_GRAPHICS, "g_Tex2D_1", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "g_Tex2D_2", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "ConstBuff_1", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "ConstBuff_2", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType}, //
            };

        std::string Name = std::string{"PRS test - "} + GetShaderVariableTypeLiteralName(VarType) + " vars";

        PipelineResourceSignatureDesc PRSDesc;
        PRSDesc.Name         = Name.c_str();
        PRSDesc.Resources    = Resources;
        PRSDesc.NumResources = _countof(Resources);

        ImmutableSamplerDesc ImmutableSamplers[] = //
            {
                {SHADER_TYPE_ALL_GRAPHICS, "g_Sampler", SamplerDesc{}} //
            };
        PRSDesc.ImmutableSamplers    = ImmutableSamplers;
        PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

        RefCntAutoPtr<IPipelineResourceSignature> pPRS;
        pDevice->CreatePipelineResourceSignature(PRSDesc, &pPRS);
        ASSERT_TRUE(pPRS);

        EXPECT_EQ(pPRS->GetStaticVariableByName(SHADER_TYPE_VERTEX, "g_Sampler"), nullptr);
        EXPECT_EQ(pPRS->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_Sampler"), nullptr);

        if (VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
        {
            SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Tex2D_1", Set, pTexSRVs[0]);
            SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Tex2D_2", Set, pTexSRVs[0]);
            SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "ConstBuff_1", Set, pConstBuff);
            SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "ConstBuff_2", Set, pConstBuff);
        }

        RefCntAutoPtr<IShaderResourceBinding> pSRB;

        auto pPSO = CreateGraphicsPSO(pVS, pPS, {pPRS});
        ASSERT_TRUE(pPSO);

        pPRS->CreateShaderResourceBinding(&pSRB, true);

        EXPECT_EQ(pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Sampler"), nullptr);
        EXPECT_EQ(pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Sampler"), nullptr);

        if (VarType != SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
        {
            SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2D_1", Set, pTexSRVs[0]);
            SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2D_2", Set, pTexSRVs[0]);
            SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "ConstBuff_1", Set, pConstBuff);
            SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "ConstBuff_2", Set, pConstBuff);
        }

        pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        ITextureView* ppRTVs[] = {pRTV};
        pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        pContext->SetPipelineState(pPSO);

        DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
        pContext->Draw(DrawAttrs);
    }
}

TEST_F(PipelineResourceSignatureTest, ImmutableSamplers)
{
    auto* const pEnv     = TestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto*       pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto pVS = CreateShaderFromFile(SHADER_TYPE_VERTEX, "StaticSamplers.hlsl", "VSMain", "PRS static samplers test: VS");
    auto pPS = CreateShaderFromFile(SHADER_TYPE_PIXEL, "StaticSamplers.hlsl", "PSMain", "PRS static samplers test: PS");
    ASSERT_TRUE(pVS && pPS);

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

    EXPECT_EQ(pPRS->GetStaticVariableByName(SHADER_TYPE_VERTEX, "g_Sampler"), nullptr);
    EXPECT_EQ(pPRS->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_Sampler"), nullptr);

    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Tex2D_Static", Set, pTexSRVs[0]);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPRS->CreateShaderResourceBinding(&pSRB, true);

    EXPECT_EQ(pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Sampler"), nullptr);
    EXPECT_EQ(pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Sampler"), nullptr);

    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2D_Mut", Set, pTexSRVs[1]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2D_Dyn", Set, pTexSRVs[2]);

    auto pPSO = CreateGraphicsPSO(pVS, pPS, {pPRS});
    ASSERT_TRUE(pPSO);

    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);
}


TEST_F(PipelineResourceSignatureTest, ImmutableSamplers2)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<IPipelineResourceSignature> pSignature1;
    {
        const PipelineResourceDesc Resources[] =
            {
                {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "Constants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE} //
            };

        PipelineResourceSignatureDesc Desc;
        Desc.Name         = "ImmutableSamplers2 - PRS1";
        Desc.Resources    = Resources;
        Desc.NumResources = _countof(Resources);
        Desc.BindingIndex = 0;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignature1);
        ASSERT_NE(pSignature1, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignature2;
    {
        const PipelineResourceDesc Resources[] =
            {
                {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC} //
            };

        SamplerDesc SamLinearWrapDesc{
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP};
        ImmutableSamplerDesc ImmutableSamplers[] =
            {
                {SHADER_TYPE_PIXEL, "g_Texture", SamLinearWrapDesc},
                {SHADER_TYPE_PIXEL, "g_Sampler", SamLinearWrapDesc} //
            };

        PipelineResourceSignatureDesc Desc;
        Desc.Name                       = "ImmutableSamplers2 - PRS2";
        Desc.Resources                  = Resources;
        Desc.NumResources               = _countof(Resources);
        Desc.ImmutableSamplers          = ImmutableSamplers;
        Desc.NumImmutableSamplers       = _countof(ImmutableSamplers);
        Desc.UseCombinedTextureSamplers = true;
        Desc.CombinedSamplerSuffix      = "_sampler";
        Desc.BindingIndex               = 2;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignature2);
        ASSERT_NE(pSignature2, nullptr);

        EXPECT_EQ(pSignature2->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_Sampler"), nullptr);
        EXPECT_EQ(pSignature2->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_Texture_sampler"), nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignature3;
    {
        PipelineResourceSignatureDesc Desc;
        Desc.Name         = "ImmutableSamplers2 - PRS3";
        Desc.BindingIndex = 3;
        pDevice->CreatePipelineResourceSignature(Desc, &pSignature3);
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

    auto pVS = CreateShaderFromSource(SHADER_TYPE_VERTEX, HLSL::PRSTest1_VS.c_str(), "main", "PRS test 1 - VS", true);
    auto pPS = CreateShaderFromSource(SHADER_TYPE_PIXEL, HLSL::PRSTest1_PS.c_str(), "main", "PRS test 1 - PS", true);
    ASSERT_TRUE(pVS && pPS);

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    IPipelineResourceSignature* Signatures[] = {pSignature1, pSignature2, pSignature3};

    PSOCreateInfo.ppResourceSignatures    = Signatures;
    PSOCreateInfo.ResourceSignaturesCount = _countof(Signatures);

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    ASSERT_EQ(pPSO->GetResourceSignatureCount(), 4u);
    ASSERT_EQ(pPSO->GetResourceSignature(0), pSignature1);
    ASSERT_EQ(pPSO->GetResourceSignature(1), nullptr);
    ASSERT_EQ(pPSO->GetResourceSignature(2), pSignature2);
    ASSERT_EQ(pPSO->GetResourceSignature(3), pSignature3);

    RefCntAutoPtr<IShaderResourceBinding> pSRB1;
    pSignature1->CreateShaderResourceBinding(&pSRB1, true);
    ASSERT_NE(pSRB1, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pSRB2;
    pSignature2->CreateShaderResourceBinding(&pSRB2, true);
    ASSERT_NE(pSRB2, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pSRB3;
    pSignature3->CreateShaderResourceBinding(&pSRB3, true);
    ASSERT_NE(pSRB3, nullptr);

    pSRB1->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(pConstBuff);
    pSRB2->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pTexSRVsNoSampler[0]);

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->CommitShaderResources(pSRB1, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CommitShaderResources(pSRB2, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CommitShaderResources(pSRB3, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

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
        const PipelineResourceDesc Resources[] =
            {
                {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "Constants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE} //
            };

        PipelineResourceSignatureDesc Desc;
        Desc.Resources    = Resources;
        Desc.NumResources = _countof(Resources);
        Desc.BindingIndex = 0;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignature1);
        ASSERT_NE(pSignature1, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignature2;
    {
        const PipelineResourceDesc Resources[] =
            {
                {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_PIXEL, "g_Texture_sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE} //
            };

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
        const PipelineResourceDesc Resources[] =
            {
                {SHADER_TYPE_PIXEL, "g_Texture2", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_PIXEL, "g_Texture2_sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE} //
            };

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

    auto pVS  = CreateShaderFromSource(SHADER_TYPE_VERTEX, HLSL::PRSTest1_VS.c_str(), "main", "PRS test - VS", true);
    auto pPS  = CreateShaderFromSource(SHADER_TYPE_PIXEL, HLSL::PRSTest1_PS.c_str(), "main", "PRS test - PS", true);
    auto pPS2 = CreateShaderFromSource(SHADER_TYPE_PIXEL, HLSL::PRSTest2_PS.c_str(), "main", "PRS test 2 - PS", true);
    ASSERT_TRUE(pVS && pPS && pPS2);

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

    pSRB1->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(pConstBuff);
    pSRB2->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pTexSRVsNoSampler[0]);
    pSRB3->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture2")->Set(pTexSRVsNoSampler[1]);

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
        const PipelineResourceDesc Resources[] =
            {
                {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_PIXEL, "g_Texture_sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE} //
            };

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
        const PipelineResourceDesc Resources[] =
            {
                {SHADER_TYPE_VERTEX, "Constants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE} //
            };

        PipelineResourceSignatureDesc Desc;
        Desc.Resources    = Resources;
        Desc.NumResources = _countof(Resources);
        Desc.BindingIndex = 1;

        pDevice->CreatePipelineResourceSignature(Desc, &pSignatureVS);
        ASSERT_NE(pSignatureVS, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pSignatureMS;
    {
        const PipelineResourceDesc Resources[] =
            {
                {SHADER_TYPE_MESH, "Constants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC} //
            };

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

    auto pVS = CreateShaderFromSource(SHADER_TYPE_VERTEX, HLSL::PRSTest3_VS.c_str(), "main", "PRS test 3 - VS", true);
    auto pPS = CreateShaderFromSource(SHADER_TYPE_PIXEL, HLSL::PRSTest3_PS.c_str(), "main", "PRS test 3 - PS", true);
    ASSERT_TRUE(pVS && pPS);

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

    auto pMS = CreateShaderFromSource(SHADER_TYPE_VERTEX, HLSL::PRSTest3_MS.c_str(), "main", "PRS test - MS", true);

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

    RefCntAutoPtr<IShaderResourceBinding> PixelSRB;
    pSignaturePS->CreateShaderResourceBinding(&PixelSRB, true);
    ASSERT_NE(PixelSRB, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> VertexSRB;
    pSignatureVS->CreateShaderResourceBinding(&VertexSRB, true);
    ASSERT_NE(VertexSRB, nullptr);

    RefCntAutoPtr<IShaderResourceBinding> MeshSRB;
    pSignatureMS->CreateShaderResourceBinding(&MeshSRB, true);
    ASSERT_NE(MeshSRB, nullptr);

    PixelSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pTexSRVsNoSampler[0]);
    VertexSRB->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(pConstBuff);
    MeshSRB->GetVariableByName(SHADER_TYPE_MESH, "Constants")->Set(pConstBuff);

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


TEST_F(PipelineResourceSignatureTest, CombinedImageSamplers)
{
    auto* const pEnv    = TestingEnvironment::GetInstance();
    auto* const pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceCaps().IsVulkanDevice() && !pDevice->GetDeviceCaps().IsGLDevice())
    {
        GTEST_SKIP();
    }

    auto* pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_GLSL;
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.FilePath        = "CombinedImageSamplersGL.vsh";
        ShaderCI.Desc.Name       = "CombinedImageSamplers - VS";
        pDevice->CreateShader(ShaderCI, &pVS);
    }
    ASSERT_NE(pVS, nullptr);

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.FilePath        = "CombinedImageSamplersGL.psh";
        ShaderCI.Desc.Name       = "CombinedImageSamplers - PS";
        pDevice->CreateShader(ShaderCI, &pPS);
    }
    ASSERT_NE(pPS, nullptr);

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Combined image samplers test";

    constexpr auto SHADER_TYPE_VS_PS = SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL;
    // clang-format off
    PipelineResourceDesc Resources[]
    {
        {SHADER_TYPE_VS_PS, "g_tex2D_Static",   1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC,  PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER},
        {SHADER_TYPE_VS_PS, "g_tex2D_Mut",      1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER},
        {SHADER_TYPE_VS_PS, "g_tex2D_Dyn",      1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER},
        {SHADER_TYPE_VS_PS, "g_tex2D_StaticArr",2, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC,  PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER},
        {SHADER_TYPE_VS_PS, "g_tex2D_MutArr",   2, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER},
        {SHADER_TYPE_VS_PS, "g_tex2D_DynArr",   2, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER}
    };
    // clang-format on
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);

    ImmutableSamplerDesc ImmutableSamplers[] = //
        {
            {SHADER_TYPE_ALL_GRAPHICS, "g_tex2D_StaticArr", SamplerDesc{}},
            {SHADER_TYPE_ALL_GRAPHICS, "g_tex2D_MutArr", SamplerDesc{}},
            {SHADER_TYPE_ALL_GRAPHICS, "g_tex2D_DynArr", SamplerDesc{}} //
        };
    PRSDesc.ImmutableSamplers    = ImmutableSamplers;
    PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

    RefCntAutoPtr<IPipelineResourceSignature> pPRS;
    pDevice->CreatePipelineResourceSignature(PRSDesc, &pPRS);
    ASSERT_TRUE(pPRS);

    auto pPSO = CreateGraphicsPSO(pVS, pPS, {pPRS});
    ASSERT_TRUE(pPSO);

    std::array<IDeviceObject*, 2> ppSRVsNoSampler = {pTexSRVsNoSampler[0], pTexSRVsNoSampler[1]};

    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_tex2D_Static", Set, pTexSRVs[0]);
    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_tex2D_StaticArr", SetArray, ppSRVsNoSampler.data(), 0, 2);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPRS->CreateShaderResourceBinding(&pSRB, true);

    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_tex2D_Mut", Set, pTexSRVs[1]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_tex2D_MutArr", SetArray, ppSRVsNoSampler.data(), 0, 2);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_tex2D_Dyn", Set, pTexSRVs[2]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_tex2D_DynArr", SetArray, ppSRVsNoSampler.data(), 0, 2);

    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);
}


TEST_F(PipelineResourceSignatureTest, FormattedBuffers)
{
    auto* const pEnv     = TestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto*       pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = true;
    ShaderCI.FilePath                   = "shaders/ShaderResourceLayout/FormattedBuffers.hlsl";

    static constexpr Uint32 StaticBuffArraySize  = 4;
    static constexpr Uint32 MutableBuffArraySize = 3;
    static constexpr Uint32 DynamicBuffArraySize = 2;
    ShaderMacroHelper       Macros;
    Macros.AddShaderMacro("STATIC_BUFF_ARRAY_SIZE", static_cast<int>(StaticBuffArraySize));
    Macros.AddShaderMacro("MUTABLE_BUFF_ARRAY_SIZE", static_cast<int>(MutableBuffArraySize));
    Macros.AddShaderMacro("DYNAMIC_BUFF_ARRAY_SIZE", static_cast<int>(DynamicBuffArraySize));
    ShaderCI.Macros = Macros;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "VSMain";
        ShaderCI.Desc.Name       = "FormattedBuffers - VS";
        pDevice->CreateShader(ShaderCI, &pVS);
    }
    ASSERT_NE(pVS, nullptr);

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "PSMain";
        ShaderCI.Desc.Name       = "FormattedBuffers - PS";
        pDevice->CreateShader(ShaderCI, &pPS);
    }
    ASSERT_NE(pPS, nullptr);

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Formatted buffer test";

    constexpr auto SHADER_TYPE_VS_PS = SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL;
    // clang-format off
    PipelineResourceDesc Resources[]
    {
        {SHADER_TYPE_VS_PS, "g_Buff_Static",   1, SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC,  PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER | PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS},
        {SHADER_TYPE_VS_PS, "g_Buff_Mut",      1, SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER | PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS},
        {SHADER_TYPE_VS_PS, "g_Buff_Dyn",      1, SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER | PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS},
        {SHADER_TYPE_VS_PS, "g_BuffArr_Static",StaticBuffArraySize,  SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC,  PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER},
        {SHADER_TYPE_VS_PS, "g_BuffArr_Mut",   MutableBuffArraySize, SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER},
        {SHADER_TYPE_VS_PS, "g_BuffArr_Dyn",   DynamicBuffArraySize, SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER}
    };
    // clang-format on
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);

    RefCntAutoPtr<IPipelineResourceSignature> pPRS;
    pDevice->CreatePipelineResourceSignature(PRSDesc, &pPRS);
    ASSERT_TRUE(pPRS);

    auto pPSO = CreateGraphicsPSO(pVS, pPS, {pPRS});
    ASSERT_TRUE(pPSO);

    std::array<RefCntAutoPtr<IBuffer>, 4>     pBuffer;
    std::array<RefCntAutoPtr<IBufferView>, 4> pBufferView;

    for (size_t i = 0; i < pBuffer.size(); ++i)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Formatted buffer";
        BuffDesc.uiSizeInBytes     = 256;
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.ElementByteStride = 16;
        BuffDesc.Mode              = BUFFER_MODE_FORMATTED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pBuffer[i]);
        ASSERT_NE(pBuffer[i], nullptr);

        BufferViewDesc BuffViewDesc;
        BuffViewDesc.Name                 = "Formatted buffer SRV";
        BuffViewDesc.ViewType             = BUFFER_VIEW_SHADER_RESOURCE;
        BuffViewDesc.Format.ValueType     = VT_FLOAT32;
        BuffViewDesc.Format.NumComponents = 4;
        BuffViewDesc.Format.IsNormalized  = false;
        pBuffer[i]->CreateView(BuffViewDesc, &pBufferView[i]);
    }
    std::array<IDeviceObject*, 4> ppSRVs = {pBufferView[0], pBufferView[1], pBufferView[2], pBufferView[3]};

    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_Buff_Static", Set, ppSRVs[0]);
    SET_STATIC_VAR(pPRS, SHADER_TYPE_VERTEX, "g_BuffArr_Static", SetArray, ppSRVs.data(), 0, StaticBuffArraySize);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pPRS->CreateShaderResourceBinding(&pSRB, true);

    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Buff_Mut", Set, ppSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_BuffArr_Mut", SetArray, ppSRVs.data(), 0, MutableBuffArraySize);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Buff_Dyn", Set, ppSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_BuffArr_Dyn", SetArray, ppSRVs.data(), 0, DynamicBuffArraySize);

    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);
}

} // namespace Diligent
