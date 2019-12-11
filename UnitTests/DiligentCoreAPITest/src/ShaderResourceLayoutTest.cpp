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
#include <unordered_map>
#include <vector>
#include <array>
#include <algorithm>

#include "TestingEnvironment.h"
#include "ShaderMacroHelper.h"
#include "GraphicsAccessories.h"

#include "gtest/gtest.h"

using namespace Diligent;

namespace Diligent
{
namespace Test
{
void PrintShaderResources(IShader* pShader);
}
} // namespace Diligent

namespace
{

class ShaderResourceLayoutTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        auto* pEnv          = TestingEnvironment::GetInstance();
        auto  pRenderTarget = pEnv->CreateTexture("ShaderResourceLayoutTest: test RTV", TEX_FORMAT_RGBA8_UNORM, BIND_RENDER_TARGET, 512, 512);
        ASSERT_NE(pRenderTarget, nullptr);
        pRTV = pRenderTarget->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    }

    static void TearDownTestSuite()
    {
        TestingEnvironment::GetInstance()->Reset();
    }

    static void VerifyShaderResources(IShader*                 pShader,
                                      const ShaderResourceDesc ExpectedResources[],
                                      Uint32                   ExpectedResCount)
    {
        auto ResCount = pShader->GetResourceCount();
        EXPECT_EQ(ResCount, ExpectedResCount) << "Actual number of resources (" << ResCount << ") in shader '"
                                              << pShader->GetDesc().Name << "' does not match the expected number of resources (" << ExpectedResCount << ')';
        std::unordered_map<std::string, ShaderResourceDesc> Resources;
        for (Uint32 i = 0; i < ResCount; ++i)
        {
            const auto& ResDesc = pShader->GetResource(i);
            Resources.emplace(ResDesc.Name, ResDesc);
        }

        for (Uint32 i = 0; i < ExpectedResCount; ++i)
        {
            const auto& ExpectedRes = ExpectedResources[i];

            auto it = Resources.find(ExpectedRes.Name);
            if (it != Resources.end())
            {
                EXPECT_EQ(it->second.Type, ExpectedRes.Type) << "Unexpected type of resource '" << ExpectedRes.Name << '\'';
                EXPECT_EQ(it->second.ArraySize, ExpectedRes.ArraySize) << "Unexpected array size of resource '" << ExpectedRes.Name << '\'';
                Resources.erase(it);
            }
            else
            {
                ADD_FAILURE() << "Unable to find resource '" << ExpectedRes.Name << "' in shader '" << pShader->GetDesc().Name << "'";
            }
        }

        for (auto it : Resources)
        {
            ADD_FAILURE() << "Unexpected resource '" << it.second.Name << "' in shader '" << pShader->GetDesc().Name << "'";
        }
    }

    static RefCntAutoPtr<IShader> CreateShader(const char*               ShaderName,
                                               const char*               FileName,
                                               const char*               EntryPoint,
                                               SHADER_TYPE               ShaderType,
                                               SHADER_SOURCE_LANGUAGE    SrcLang,
                                               const ShaderMacro*        Macros,
                                               const ShaderResourceDesc* ExpectedResources,
                                               Uint32                    NumExpectedResources)
    {
        auto* pEnv    = TestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/ShaderResourceLayout", &pShaderSourceFactory);

        ShaderCreateInfo ShaderCI;
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        ShaderCI.UseCombinedTextureSamplers = false;

        ShaderCI.FilePath        = FileName;
        ShaderCI.Desc.Name       = ShaderName;
        ShaderCI.EntryPoint      = EntryPoint;
        ShaderCI.Desc.ShaderType = ShaderType;
        ShaderCI.SourceLanguage  = SrcLang;
        ShaderCI.Macros          = Macros;

        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        if (pShader)
        {
            VerifyShaderResources(pShader, ExpectedResources, NumExpectedResources);
            Diligent::Test::PrintShaderResources(pShader);
        }

        return pShader;
    }
    static void CreateGraphicsPSO(IShader*                               pVS,
                                  IShader*                               pPS,
                                  const PipelineResourceLayoutDesc&      ResourceLayout,
                                  RefCntAutoPtr<IPipelineState>&         pPSO,
                                  RefCntAutoPtr<IShaderResourceBinding>& pSRB)
    {
        PipelineStateDesc PSODesc;

        auto* pEnv    = TestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        PSODesc.ResourceLayout = ResourceLayout;

        PSODesc.Name                                          = "Shader resource layout test";
        PSODesc.GraphicsPipeline.pVS                          = pVS;
        PSODesc.GraphicsPipeline.pPS                          = pPS;
        PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
        PSODesc.GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_RGBA8_UNORM;
        PSODesc.GraphicsPipeline.DSVFormat                    = TEX_FORMAT_UNKNOWN;
        PSODesc.SRBAllocationGranularity                      = 16;
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        pDevice->CreatePipelineState(PSODesc, &pPSO);
        if (pPSO)
            pPSO->CreateShaderResourceBinding(&pSRB, false);
    }

    static RefCntAutoPtr<IBufferView> CreateResourceBufferView(BUFFER_MODE BufferMode, BUFFER_VIEW_TYPE ViewType)
    {
        VERIFY_EXPR(ViewType == BUFFER_VIEW_SHADER_RESOURCE || ViewType == BUFFER_VIEW_UNORDERED_ACCESS);

        auto* pEnv    = TestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        BufferDesc BuffDesc;
        BuffDesc.Name              = "Formatted buffer";
        BuffDesc.uiSizeInBytes     = 256;
        BuffDesc.BindFlags         = ViewType == BUFFER_VIEW_SHADER_RESOURCE ? BIND_SHADER_RESOURCE : BIND_UNORDERED_ACCESS;
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.ElementByteStride = 16;
        BuffDesc.Mode              = BufferMode;
        RefCntAutoPtr<IBuffer>     pBuffer;
        RefCntAutoPtr<IBufferView> pBufferView;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pBuffer);
        if (!pBuffer)
        {
            ADD_FAILURE() << "Unable to create buffer " << BuffDesc;
            return pBufferView;
        }

        if (BufferMode == BUFFER_MODE_FORMATTED)
        {
            BufferViewDesc BuffViewDesc;
            BuffViewDesc.Name                 = "Formatted buffer SRV";
            BuffViewDesc.ViewType             = ViewType;
            BuffViewDesc.Format.ValueType     = VT_FLOAT32;
            BuffViewDesc.Format.NumComponents = 4;
            BuffViewDesc.Format.IsNormalized  = false;
            pBuffer->CreateView(BuffViewDesc, &pBufferView);
        }
        else
        {
            pBufferView = pBuffer->GetDefaultView(ViewType);
        }

        return pBufferView;
    }

    static void CreateComputePSO(IShader*                               pCS,
                                 const PipelineResourceLayoutDesc&      ResourceLayout,
                                 RefCntAutoPtr<IPipelineState>&         pPSO,
                                 RefCntAutoPtr<IShaderResourceBinding>& pSRB)
    {
        PipelineStateDesc PSODesc;

        auto* pEnv    = TestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        PSODesc.Name                = "Shader resource layout test";
        PSODesc.IsComputePipeline   = true;
        PSODesc.ResourceLayout      = ResourceLayout;
        PSODesc.ComputePipeline.pCS = pCS;

        pDevice->CreatePipelineState(PSODesc, &pPSO);
        if (pPSO)
            pPSO->CreateShaderResourceBinding(&pSRB, false);
    }

    void TestStructuredOrFormattedBuffer(bool IsFormatted);
    void TestRWStructuredOrFormattedBuffer(bool IsFormatted);

    static RefCntAutoPtr<ITextureView> pRTV;
};

RefCntAutoPtr<ITextureView> ShaderResourceLayoutTest::pRTV;

#define SET_STATIC_VAR(PSO, ShaderFlags, VarName, SetMethod, ...)                                \
    do                                                                                           \
    {                                                                                            \
        auto pStaticVar = PSO->GetStaticVariableByName(ShaderFlags, VarName);                    \
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

TEST_F(ShaderResourceLayoutTest, Textures)
{
    TestingEnvironment::ScopedReset AutoResetEnvironment;

    static constexpr int StaticTexArraySize  = 2;
    static constexpr int MutableTexArraySize = 4;
    static constexpr int DynamicTexArraySize = 3;
    ShaderMacroHelper    Macros;
    Macros.AddShaderMacro("STATIC_TEX_ARRAY_SIZE", StaticTexArraySize);
    Macros.AddShaderMacro("MUTABLE_TEX_ARRAY_SIZE", MutableTexArraySize);
    Macros.AddShaderMacro("DYNAMIC_TEX_ARRAY_SIZE", DynamicTexArraySize);

    RefCntAutoPtr<IPipelineState>         pPSO;
    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    // clang-format off
    ShaderResourceDesc Resources[] = 
    {
        {"g_Tex2D_Static",      SHADER_RESOURCE_TYPE_TEXTURE_SRV, 1},
        {"g_Tex2D_Mut",         SHADER_RESOURCE_TYPE_TEXTURE_SRV, 1},
        {"g_Tex2D_Dyn",         SHADER_RESOURCE_TYPE_TEXTURE_SRV, 1},
        {"g_Tex2DArr_Static",   SHADER_RESOURCE_TYPE_TEXTURE_SRV, StaticTexArraySize},
        {"g_Tex2DArr_Mut",      SHADER_RESOURCE_TYPE_TEXTURE_SRV, MutableTexArraySize},
        {"g_Tex2DArr_Dyn",      SHADER_RESOURCE_TYPE_TEXTURE_SRV, DynamicTexArraySize},
        {"g_Sampler",           SHADER_RESOURCE_TYPE_SAMPLER,     1},
    };
    // clang-format on

    auto pVS = CreateShader("ShaderResourceLayoutTest.Textures - VS", "Textures.hlsl", "VSMain",
                            SHADER_TYPE_VERTEX, SHADER_SOURCE_LANGUAGE_HLSL, Macros,
                            Resources, _countof(Resources));
    auto pPS = CreateShader("ShaderResourceLayoutTest.Textures - PS", "Textures.hlsl", "PSMain",
                            SHADER_TYPE_PIXEL, SHADER_SOURCE_LANGUAGE_HLSL, Macros,
                            Resources, _countof(Resources));
    ASSERT_NE(pVS, nullptr);
    ASSERT_NE(pPS, nullptr);


    // clang-format off
    ShaderResourceVariableDesc Vars[] =
    {
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Tex2D_Static",    SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Tex2D_Mut",       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Tex2D_Dyn",       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},

        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Tex2DArr_Static", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Tex2DArr_Mut",    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Tex2DArr_Dyn",    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    StaticSamplerDesc StaticSamplers[] =
    {
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Sampler", SamplerDesc{}}
    };
    // clang-format on

    PipelineResourceLayoutDesc ResourceLayout;
    ResourceLayout.Variables         = Vars;
    ResourceLayout.NumVariables      = _countof(Vars);
    ResourceLayout.StaticSamplers    = StaticSamplers;
    ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);

    CreateGraphicsPSO(pVS, pPS, ResourceLayout, pPSO, pSRB);
    ASSERT_NE(pPSO, nullptr);
    ASSERT_NE(pSRB, nullptr);

    constexpr auto MaxTextures = std::max(std::max(StaticTexArraySize, MutableTexArraySize), DynamicTexArraySize);

    std::array<RefCntAutoPtr<ITexture>, MaxTextures> pTextures;
    std::array<IDeviceObject*, MaxTextures>          pTexSRVs = {};

    auto* pEnv = TestingEnvironment::GetInstance();
    for (Uint32 i = 0; i < MaxTextures; ++i)
    {
        pTextures[i] = pEnv->CreateTexture("Test texture", TEX_FORMAT_RGBA8_UNORM, BIND_SHADER_RESOURCE, 256, 256);
        pTexSRVs[i]  = pTextures[i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }

    SET_STATIC_VAR(pPSO, SHADER_TYPE_VERTEX, "g_Tex2D_Static", Set, pTexSRVs[0]);
    SET_STATIC_VAR(pPSO, SHADER_TYPE_VERTEX, "g_Tex2DArr_Static", SetArray, pTexSRVs.data(), 0, StaticTexArraySize);

    SET_STATIC_VAR(pPSO, SHADER_TYPE_PIXEL, "g_Tex2D_Static", Set, pTexSRVs[0]);
    SET_STATIC_VAR(pPSO, SHADER_TYPE_PIXEL, "g_Tex2DArr_Static", SetArray, pTexSRVs.data(), 0, StaticTexArraySize);

    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2D_Mut", Set, pTexSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2D_Dyn", Set, pTexSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2DArr_Mut", SetArray, pTexSRVs.data(), 0, MutableTexArraySize);
    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2DArr_Dyn", SetArray, pTexSRVs.data(), 0, DynamicTexArraySize);

    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2D_Mut", Set, pTexSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2D_Dyn", Set, pTexSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2DArr_Mut", SetArray, pTexSRVs.data(), 0, MutableTexArraySize);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2DArr_Dyn", SetArray, pTexSRVs.data(), 0, DynamicTexArraySize);

    pSRB->InitializeStaticResources(pPSO);

    auto* pContext = pEnv->GetDeviceContext();

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);

    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2D_Dyn", Set, pTexSRVs[1]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Tex2DArr_Dyn", SetArray, pTexSRVs.data(), 1, DynamicTexArraySize - 1);

    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2D_Dyn", Set, pTexSRVs[1]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Tex2DArr_Dyn", SetArray, pTexSRVs.data(), 1, DynamicTexArraySize - 1);

    pContext->Draw(DrawAttrs);
}

void ShaderResourceLayoutTest::TestStructuredOrFormattedBuffer(bool IsFormatted)
{
    TestingEnvironment::ScopedReset AutoResetEnvironment;

    static constexpr int StaticBuffArraySize  = 4;
    static constexpr int MutableBuffArraySize = 3;
    static constexpr int DynamicBuffArraySize = 2;
    ShaderMacroHelper    Macros;
    Macros.AddShaderMacro("STATIC_BUFF_ARRAY_SIZE", StaticBuffArraySize);
    Macros.AddShaderMacro("MUTABLE_BUFF_ARRAY_SIZE", MutableBuffArraySize);
    Macros.AddShaderMacro("DYNAMIC_BUFF_ARRAY_SIZE", DynamicBuffArraySize);

    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    // Vulkan only allows 16 dynamic storage buffer bindings among all stages, so
    // use arrays only in fragment shader for structured buffer test.
    const auto UseArraysInPSOnly = !IsFormatted && pDevice->GetDeviceCaps().IsVulkanDevice();

    // clang-format off
    std::vector<ShaderResourceDesc> Resources = 
    {
        {"g_Buff_Static", SHADER_RESOURCE_TYPE_BUFFER_SRV, 1},
        {"g_Buff_Mut",    SHADER_RESOURCE_TYPE_BUFFER_SRV, 1},
        {"g_Buff_Dyn",    SHADER_RESOURCE_TYPE_BUFFER_SRV, 1}
    };
    
    auto AddArrayResources = [&Resources]()
    {
        Resources.emplace_back(ShaderResourceDesc{"g_BuffArr_Static", SHADER_RESOURCE_TYPE_BUFFER_SRV, StaticBuffArraySize});
        Resources.emplace_back(ShaderResourceDesc{"g_BuffArr_Mut",    SHADER_RESOURCE_TYPE_BUFFER_SRV, MutableBuffArraySize});
        Resources.emplace_back(ShaderResourceDesc{"g_BuffArr_Dyn",    SHADER_RESOURCE_TYPE_BUFFER_SRV, DynamicBuffArraySize});
    };
    // clang-format on
    if (!UseArraysInPSOnly)
    {
        AddArrayResources();
    }

    const char*            ShaderFileName = nullptr;
    SHADER_SOURCE_LANGUAGE SrcLang        = SHADER_SOURCE_LANGUAGE_DEFAULT;
    if (pDevice->GetDeviceCaps().IsD3DDevice())
    {
        ShaderFileName = IsFormatted ? "FormattedBuffers.hlsl" : "StructuredBuffers.hlsl";
        SrcLang        = SHADER_SOURCE_LANGUAGE_HLSL;
    }
    else if (pDevice->GetDeviceCaps().IsVulkanDevice())
    {
        ShaderFileName = IsFormatted ? "FormattedBuffers.hlsl" : "StructuredBuffers.glsl";
        SrcLang        = IsFormatted ? SHADER_SOURCE_LANGUAGE_HLSL : SHADER_SOURCE_LANGUAGE_GLSL;
    }
    else if (pDevice->GetDeviceCaps().IsGLDevice())
    {
        ShaderFileName = IsFormatted ? "FormattedBuffers.glsl" : "StructuredBuffers.glsl";
        SrcLang        = SHADER_SOURCE_LANGUAGE_GLSL;
    }
    else
    {
        GTEST_FAIL() << "Unexpected device type";
    }
    auto pVS = CreateShader(IsFormatted ? "ShaderResourceLayoutTest.FormattedBuffers - VS" : "ShaderResourceLayoutTest.StructuredBuffers - VS",
                            ShaderFileName, "VSMain",
                            SHADER_TYPE_VERTEX, SrcLang, Macros,
                            Resources.data(), static_cast<Uint32>(Resources.size()));
    if (UseArraysInPSOnly)
    {
        AddArrayResources();
    }
    auto pPS = CreateShader(IsFormatted ? "ShaderResourceLayoutTest.FormattedBuffers - PS" : "ShaderResourceLayoutTest.StructuredBuffers - PS",
                            ShaderFileName, "PSMain",
                            SHADER_TYPE_PIXEL, SrcLang, Macros,
                            Resources.data(), static_cast<Uint32>(Resources.size()));
    ASSERT_NE(pVS, nullptr);
    ASSERT_NE(pPS, nullptr);


    // clang-format off
    ShaderResourceVariableDesc Vars[] =
    {
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Buff_Static",    SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Buff_Mut",       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Buff_Dyn",       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},

        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_BuffArr_Static", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_BuffArr_Mut",    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_BuffArr_Dyn",    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    // clang-format on

    PipelineResourceLayoutDesc ResourceLayout;
    ResourceLayout.Variables    = Vars;
    ResourceLayout.NumVariables = _countof(Vars);

    RefCntAutoPtr<IPipelineState>         pPSO;
    RefCntAutoPtr<IShaderResourceBinding> pSRB;

    CreateGraphicsPSO(pVS, pPS, ResourceLayout, pPSO, pSRB);
    ASSERT_NE(pPSO, nullptr);
    ASSERT_NE(pSRB, nullptr);

    constexpr auto MaxBuffers = std::max(std::max(StaticBuffArraySize, MutableBuffArraySize), DynamicBuffArraySize);

    std::array<RefCntAutoPtr<IBufferView>, MaxBuffers> pBufferViews;
    std::array<IDeviceObject*, MaxBuffers>             pBuffSRVs = {};

    for (Uint32 i = 0; i < MaxBuffers; ++i)
    {
        pBufferViews[i] = CreateResourceBufferView(IsFormatted ? BUFFER_MODE_FORMATTED : BUFFER_MODE_STRUCTURED, BUFFER_VIEW_SHADER_RESOURCE);
        ASSERT_NE(pBufferViews[i], nullptr) << "Unable to formatted buffer view ";
        pBuffSRVs[i] = pBufferViews[i];
    }

    SET_STATIC_VAR(pPSO, SHADER_TYPE_VERTEX, "g_Buff_Static", Set, pBuffSRVs[0]);
    if (!UseArraysInPSOnly)
    {
        SET_STATIC_VAR(pPSO, SHADER_TYPE_VERTEX, "g_BuffArr_Static", SetArray, pBuffSRVs.data(), 0, StaticBuffArraySize);
    }
    else
    {
        EXPECT_EQ(pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "g_BuffArr_Static"), nullptr);
    }
    SET_STATIC_VAR(pPSO, SHADER_TYPE_PIXEL, "g_Buff_Static", Set, pBuffSRVs[0]);
    SET_STATIC_VAR(pPSO, SHADER_TYPE_PIXEL, "g_BuffArr_Static", SetArray, pBuffSRVs.data(), 0, StaticBuffArraySize);


    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Buff_Mut", Set, pBuffSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Buff_Dyn", Set, pBuffSRVs[0]);
    if (!UseArraysInPSOnly)
    {
        SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_BuffArr_Mut", SetArray, pBuffSRVs.data(), 0, MutableBuffArraySize);
        SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_BuffArr_Dyn", SetArray, pBuffSRVs.data(), 0, DynamicBuffArraySize);
    }
    else
    {
        EXPECT_EQ(pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_BuffArr_Mut"), nullptr);
        EXPECT_EQ(pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_BuffArr_Dyn"), nullptr);
    }

    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Buff_Mut", Set, pBuffSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Buff_Dyn", Set, pBuffSRVs[0]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_BuffArr_Mut", SetArray, pBuffSRVs.data(), 0, MutableBuffArraySize);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_BuffArr_Dyn", SetArray, pBuffSRVs.data(), 0, DynamicBuffArraySize);

    pSRB->InitializeStaticResources(pPSO);

    auto* pContext = pEnv->GetDeviceContext();

    ITextureView* ppRTVs[] = {pRTV};
    pContext->SetRenderTargets(1, ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);

    SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_Buff_Dyn", Set, pBuffSRVs[1]);
    if (!UseArraysInPSOnly)
    {
        SET_SRB_VAR(pSRB, SHADER_TYPE_VERTEX, "g_BuffArr_Dyn", SetArray, pBuffSRVs.data(), 1, DynamicBuffArraySize - 1);
    }

    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_Buff_Dyn", Set, pBuffSRVs[1]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_PIXEL, "g_BuffArr_Dyn", SetArray, pBuffSRVs.data(), 1, DynamicBuffArraySize - 1);

    pContext->Draw(DrawAttrs);
}

TEST_F(ShaderResourceLayoutTest, FormattedBuffers)
{
    TestStructuredOrFormattedBuffer(true);
}

TEST_F(ShaderResourceLayoutTest, StructuredBuffers)
{
    TestStructuredOrFormattedBuffer(false);
}


void ShaderResourceLayoutTest::TestRWStructuredOrFormattedBuffer(bool IsFormatted)
{
    TestingEnvironment::ScopedReset AutoResetEnvironment;

    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    auto deviceType = pDevice->GetDeviceCaps().DevType;

    const Uint32      StaticBuffArraySize  = deviceType == DeviceType::D3D11 ? 1 : 4;
    const Uint32      MutableBuffArraySize = deviceType == DeviceType::D3D11 ? 2 : 3;
    const Uint32      DynamicBuffArraySize = 2;
    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("STATIC_BUFF_ARRAY_SIZE", static_cast<int>(StaticBuffArraySize));
    Macros.AddShaderMacro("MUTABLE_BUFF_ARRAY_SIZE", static_cast<int>(MutableBuffArraySize));
    Macros.AddShaderMacro("DYNAMIC_BUFF_ARRAY_SIZE", static_cast<int>(DynamicBuffArraySize));

    // clang-format off
    ShaderResourceDesc Resources[] = 
    {
        {"g_RWBuff_Static",    SHADER_RESOURCE_TYPE_BUFFER_UAV, 1},
        {"g_RWBuff_Mut",       SHADER_RESOURCE_TYPE_BUFFER_UAV, 1},
        {"g_RWBuff_Dyn",       SHADER_RESOURCE_TYPE_BUFFER_UAV, 1},
        {"g_RWBuffArr_Static", SHADER_RESOURCE_TYPE_BUFFER_UAV, StaticBuffArraySize },
        {"g_RWBuffArr_Mut",    SHADER_RESOURCE_TYPE_BUFFER_UAV, MutableBuffArraySize},
        {"g_RWBuffArr_Dyn",    SHADER_RESOURCE_TYPE_BUFFER_UAV, DynamicBuffArraySize}
    };

    const char*            ShaderFileName = nullptr;
    SHADER_SOURCE_LANGUAGE SrcLang        = SHADER_SOURCE_LANGUAGE_DEFAULT;
    if (pDevice->GetDeviceCaps().IsD3DDevice())
    {
        ShaderFileName = IsFormatted ? "RWFormattedBuffers.hlsl" : "RWStructuredBuffers.hlsl";
        SrcLang        = SHADER_SOURCE_LANGUAGE_HLSL;
    }
    else if (pDevice->GetDeviceCaps().IsVulkanDevice())
    {
        ShaderFileName = IsFormatted ? "RWFormattedBuffers.hlsl" : "RWStructuredBuffers.glsl";
        SrcLang        = IsFormatted ? SHADER_SOURCE_LANGUAGE_HLSL : SHADER_SOURCE_LANGUAGE_GLSL;
    }
    else if (pDevice->GetDeviceCaps().IsGLDevice())
    {
        ShaderFileName = IsFormatted ? "RWFormattedBuffers.glsl" : "RWStructuredBuffers.glsl";
        SrcLang        = SHADER_SOURCE_LANGUAGE_GLSL;
    }
    else
    {
        GTEST_FAIL() << "Unexpected device type";
    }
    auto pCS = CreateShader(IsFormatted ? "ShaderResourceLayoutTest.RWFormattedBuffers - CS" : "ShaderResourceLayoutTest.RWtructuredBuffers - CS",
                            ShaderFileName, "main",
                            SHADER_TYPE_COMPUTE, SrcLang, Macros,
                            Resources, _countof(Resources));
    ASSERT_NE(pCS, nullptr);

    // clang-format off
    ShaderResourceVariableDesc Vars[] =
    {
        {SHADER_TYPE_COMPUTE, "g_RWBuff_Static",    SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_COMPUTE, "g_RWBuff_Mut",       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_COMPUTE, "g_RWBuff_Dyn",       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},

        {SHADER_TYPE_COMPUTE, "g_RWBuffArr_Static", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_COMPUTE, "g_RWBuffArr_Mut",    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_COMPUTE, "g_RWBuffArr_Dyn",    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    // clang-format on

    PipelineResourceLayoutDesc ResourceLayout;
    ResourceLayout.Variables    = Vars;
    ResourceLayout.NumVariables = _countof(Vars);

    RefCntAutoPtr<IPipelineState>         pPSO;
    RefCntAutoPtr<IShaderResourceBinding> pSRB;

    CreateComputePSO(pCS, ResourceLayout, pPSO, pSRB);
    ASSERT_NE(pPSO, nullptr);
    ASSERT_NE(pSRB, nullptr);

    const auto TotalBuffers = StaticBuffArraySize + MutableBuffArraySize + DynamicBuffArraySize + 3 + 2;

    std::vector<RefCntAutoPtr<IBufferView>> pBufferViews(TotalBuffers);
    std::vector<IDeviceObject*>             pBuffUAVs(TotalBuffers);

    for (Uint32 i = 0; i < TotalBuffers; ++i)
    {
        pBufferViews[i] = CreateResourceBufferView(IsFormatted ? BUFFER_MODE_FORMATTED : BUFFER_MODE_STRUCTURED, BUFFER_VIEW_UNORDERED_ACCESS);
        ASSERT_NE(pBufferViews[i], nullptr) << "Unable to formatted buffer view ";
        pBuffUAVs[i] = pBufferViews[i];
    }

    Uint32 uav = 0;
    SET_STATIC_VAR(pPSO, SHADER_TYPE_COMPUTE, "g_RWBuff_Static", Set, pBuffUAVs[uav++]);
    SET_STATIC_VAR(pPSO, SHADER_TYPE_COMPUTE, "g_RWBuffArr_Static", SetArray, &pBuffUAVs[uav], 0, StaticBuffArraySize);
    uav += StaticBuffArraySize;

    SET_SRB_VAR(pSRB, SHADER_TYPE_COMPUTE, "g_RWBuff_Mut", Set, pBuffUAVs[uav++]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_COMPUTE, "g_RWBuff_Dyn", Set, pBuffUAVs[uav++]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_COMPUTE, "g_RWBuffArr_Mut", SetArray, &pBuffUAVs[uav], 0, MutableBuffArraySize);
    uav += MutableBuffArraySize;
    SET_SRB_VAR(pSRB, SHADER_TYPE_COMPUTE, "g_RWBuffArr_Dyn", SetArray, &pBuffUAVs[uav], 0, DynamicBuffArraySize);
    uav += DynamicBuffArraySize;
    VERIFY_EXPR(uav + 2 == pBuffUAVs.size());

    pSRB->InitializeStaticResources(pPSO);

    auto* pContext = pEnv->GetDeviceContext();

    pContext->SetPipelineState(pPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DispatchComputeAttribs DispatchAttribs(1, 1, 1);
    pContext->DispatchCompute(DispatchAttribs);

    SET_SRB_VAR(pSRB, SHADER_TYPE_COMPUTE, "g_RWBuff_Dyn", Set, pBuffUAVs[uav++]);
    SET_SRB_VAR(pSRB, SHADER_TYPE_COMPUTE, "g_RWBuffArr_Dyn", SetArray, &pBuffUAVs[uav++], 1, 1);
    pContext->DispatchCompute(DispatchAttribs);
}

TEST_F(ShaderResourceLayoutTest, FormattedRWBuffers)
{
    TestRWStructuredOrFormattedBuffer(true);
}

TEST_F(ShaderResourceLayoutTest, StructuredRWBuffers)
{
    //TestRWStructuredOrFormattedBuffer(false);
}


TEST_F(ShaderResourceLayoutTest, ConstantBuffers)
{
}

TEST_F(ShaderResourceLayoutTest, RWTextures)
{
}

TEST_F(ShaderResourceLayoutTest, SeparateSamplers)
{
}


#if 0
TEST(ShaderResourceLayout, ResourceLayout)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    auto deviceType  = pDevice->GetDeviceCaps().DevType;
    auto IsD3DDevice = pDevice->GetDeviceCaps().IsD3DDevice();

    const bool IsHLSL_51                     = deviceType == DeviceType::D3D12;
    const bool ConstantBufferArraysSupported = IsHLSL_51 || deviceType == DeviceType::Vulkan || deviceType == DeviceType::OpenGL;
    const bool VertexShaderUAVSupported      = IsHLSL_51 || deviceType == DeviceType::Vulkan || deviceType == DeviceType::OpenGL;
    
    static_assert(SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES == 3, "Unexpected number of shader variable types");
    struct ShaderResourceCounters
    {
        using SizeArrayType = std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES>;

        SizeArrayType TexArrSize     = {};
        SizeArrayType SamplerArrSize = {};
        SizeArrayType CbArrSize      = {};
        SizeArrayType BuffUavArrSize = {};
        SizeArrayType TexUavArrSize  = {};
    };
    ShaderResourceCounters VSResCounters;
    ShaderResourceCounters PSResCounters;

    VSResCounters.TexArrSize = ShaderResourceCounters::SizeArrayType //
        {
            2, // Static
            5, // Mutable
            3  // Dynamic
        };
    VSResCounters.SamplerArrSize = ShaderResourceCounters::SizeArrayType //
        {
            4, // Static
            3, // Mutable
            2  // Dynamic
        };
    VSResCounters.CbArrSize = ShaderResourceCounters::SizeArrayType //
        {
            ConstantBufferArraysSupported ? 4U : 1U, // Static
            ConstantBufferArraysSupported ? 3U : 1U, // Mutable
            ConstantBufferArraysSupported ? 2U : 1U  // Dynamic
        };
    if (deviceType != DeviceType::D3D11)
    {
        VSResCounters.BuffUavArrSize = ShaderResourceCounters::SizeArrayType //
            {
                4, // Static
                3, // Mutable
                2  // Dynamic
            };
        VSResCounters.TexUavArrSize = ShaderResourceCounters::SizeArrayType //
            {
                0, // Static
                3, // Mutable
                2  // Dynamic
            };
    }


    PSResCounters.TexArrSize = ShaderResourceCounters::SizeArrayType //
        {
            2, // Static
            5, // Mutable
            3  // Dynamic
        };
    PSResCounters.SamplerArrSize = ShaderResourceCounters::SizeArrayType //
        {
            4, // Static
            3, // Mutable
            2  // Dynamic
        };
    PSResCounters.CbArrSize = ShaderResourceCounters::SizeArrayType //
        {
            ConstantBufferArraysSupported ? 4U : 1U, // Static
            ConstantBufferArraysSupported ? 3U : 1U, // Mutable
            ConstantBufferArraysSupported ? 2U : 1U  // Dynamic
        };
    if (deviceType != DeviceType::D3D11)
    {
        PSResCounters.BuffUavArrSize = ShaderResourceCounters::SizeArrayType //
            {
                4, // Static
                3, // Mutable
                2  // Dynamic
            };
        PSResCounters.TexUavArrSize = ShaderResourceCounters::SizeArrayType //
            {
                0, // Static
                3, // Mutable
                2  // Dynamic
            };
    }

    Uint32 MaxSamplerArraySize = 0;
    for (auto SamArrSize : VSResCounters.SamplerArrSize)
        MaxSamplerArraySize = std::max(MaxSamplerArraySize, SamArrSize);
    for (auto SamArrSize : PSResCounters.SamplerArrSize)
        MaxSamplerArraySize = std::max(MaxSamplerArraySize, SamArrSize);

    std::vector<RefCntAutoPtr<ISampler>> pSamplers(MaxSamplerArraySize);
    std::vector<IDeviceObject*>          pSamplerObjs(MaxSamplerArraySize);
    for (Uint32 i = 0; i < MaxSamplerArraySize; ++i)
    {
        SamplerDesc SamDesc;
        pDevice->CreateSampler(SamDesc, &(pSamplers[i]));
        ASSERT_NE(pSamplers[i], nullptr);
        pSamplerObjs[i] = pSamplers[i];
    }

    RefCntAutoPtr<ITexture> pTex[4];

    TextureDesc TexDesc;
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Width     = 256;
    TexDesc.Height    = 256;
    TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM_SRGB;
    TexDesc.BindFlags = BIND_SHADER_RESOURCE;
    IDeviceObject* pSRVs[4];
    for (int i = 0; i < 4; ++i)
    {
        pDevice->CreateTexture(TexDesc, nullptr, &(pTex[i]));
        ASSERT_NE(pTex[i], nullptr);
        auto* pSRV = pTex[i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        ASSERT_NE(pSRV, nullptr);
        pSRV->SetSampler(pSamplers[i]);
        pSRVs[i] = pSRV;
    }

    TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;

    Uint32 TotalStorageTextures = 6 * 2;
    for (auto SamArrSize : VSResCounters.TexUavArrSize)
        TotalStorageTextures += std::max(MaxSamplerArraySize, SamArrSize);
    for (auto SamArrSize : PSResCounters.TexUavArrSize)
        TotalStorageTextures += std::max(MaxSamplerArraySize, SamArrSize);
    std::vector<RefCntAutoPtr<ITexture>> pStorageTex(TotalStorageTextures);
    std::vector<IDeviceObject*>          pUAVs(TotalStorageTextures);
    for (Uint32 i = 0; i < TotalStorageTextures; ++i)
    {
        pDevice->CreateTexture(TexDesc, nullptr, &(pStorageTex[i]));
        ASSERT_NE(pStorageTex[i], nullptr);
        pUAVs[i] = pStorageTex[i]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
        ASSERT_NE(pUAVs[i], nullptr);
    }

    TexDesc.BindFlags = BIND_RENDER_TARGET;
    RefCntAutoPtr<ITexture> pRenderTarget;
    pDevice->CreateTexture(TexDesc, nullptr, &pRenderTarget);
    ASSERT_NE(pRenderTarget, nullptr);
    auto* pRTV = pRenderTarget->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    float Zero[4] = {};
    pContext->ClearRenderTarget(pRTV, Zero, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    BufferDesc BuffDesc;
    BuffDesc.uiSizeInBytes                  = 1024;
    BuffDesc.BindFlags                      = BIND_UNIFORM_BUFFER;
    RefCntAutoPtr<IBuffer> pUniformBuffs[4] = {};
    IDeviceObject*         pUBs[4]          = {};
    for (int i = 0; i < 4; ++i)
    {
        pDevice->CreateBuffer(BuffDesc, nullptr, &(pUniformBuffs[i]));
        ASSERT_NE(pUniformBuffs[i], nullptr);
        pUBs[i] = pUniformBuffs[i];
    }

    BuffDesc.BindFlags         = BIND_UNORDERED_ACCESS;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = 16;

    Uint32 TotalStorageBuffers = 3 * 2;
    for (auto SamArrSize : VSResCounters.BuffUavArrSize)
        TotalStorageBuffers += std::max(MaxSamplerArraySize, SamArrSize);
    for (auto SamArrSize : PSResCounters.BuffUavArrSize)
        TotalStorageBuffers += std::max(MaxSamplerArraySize, SamArrSize);
    std::vector<RefCntAutoPtr<IBuffer>> pStorgeBuffs(TotalStorageBuffers);
    std::vector<IDeviceObject*>         pSBUAVs(TotalStorageBuffers);
    for (Uint32 i = 0; i < TotalStorageBuffers; ++i)
    {
        pDevice->CreateBuffer(BuffDesc, nullptr, &(pStorgeBuffs[i]));
        ASSERT_NE(pStorgeBuffs[i], nullptr);
        pSBUAVs[i] = pStorgeBuffs[i]->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
    }

    constexpr Uint32           TotalStorageTexelBuffers = 4;
    RefCntAutoPtr<IBuffer>     pUniformTexelBuff;
    RefCntAutoPtr<IBuffer>     pStorageTexelBuff[TotalStorageTexelBuffers];
    RefCntAutoPtr<IBufferView> pUniformTexelBuffSRV;
    RefCntAutoPtr<IBufferView> pStorageTexelBuffUAV[TotalStorageTexelBuffers];
    {
        BufferDesc TxlBuffDesc;
        TxlBuffDesc.Name              = "Uniform texel buffer test";
        TxlBuffDesc.uiSizeInBytes     = 256;
        TxlBuffDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        TxlBuffDesc.Usage             = USAGE_DEFAULT;
        TxlBuffDesc.ElementByteStride = 16;
        TxlBuffDesc.Mode              = BUFFER_MODE_FORMATTED;
        pDevice->CreateBuffer(TxlBuffDesc, nullptr, &pUniformTexelBuff);
        ASSERT_NE(pUniformTexelBuff, nullptr);

        BufferViewDesc TxlBuffViewDesc;
        TxlBuffViewDesc.Name                 = "Uniform texel buffer SRV";
        TxlBuffViewDesc.ViewType             = BUFFER_VIEW_SHADER_RESOURCE;
        TxlBuffViewDesc.Format.ValueType     = VT_FLOAT32;
        TxlBuffViewDesc.Format.NumComponents = 4;
        TxlBuffViewDesc.Format.IsNormalized  = false;
        pUniformTexelBuff->CreateView(TxlBuffViewDesc, &pUniformTexelBuffSRV);
        ASSERT_NE(pUniformTexelBuffSRV, nullptr);

        TxlBuffDesc.Name      = "Storage texel buffer test";
        TxlBuffDesc.BindFlags = BIND_UNORDERED_ACCESS;

        TxlBuffViewDesc.Name     = "Storage texel buffer UAV";
        TxlBuffViewDesc.ViewType = BUFFER_VIEW_UNORDERED_ACCESS;
        for (int i = 0; i < TotalStorageTexelBuffers; ++i)
        {
            pDevice->CreateBuffer(TxlBuffDesc, nullptr, &(pStorageTexelBuff[i]));
            ASSERT_NE(pStorageTexelBuff[i], nullptr);

            pStorageTexelBuff[i]->CreateView(TxlBuffViewDesc, &pStorageTexelBuffUAV[i]);
            ASSERT_NE(pStorageTexelBuffUAV[i], nullptr);
        }
    }

    /*ResourceMappingDesc ResMappingDesc;
    // clang-format off
    ResourceMappingEntry MappingEntries[] =
    {
        {"g_tex2D_Static",       pSRVs[0]},
        {"g_tex2DArr_Static",    pSRVs[0], 0},
        {"g_tex2DArr_Static",    pSRVs[1], 1},
        {"g_sepTex2D_static",    pSRVs[0]},
        {"g_sepTex2DArr_static", pSRVs[0], 0},
        {"g_sepTex2DArr_static", pSRVs[1], 1},
        {"g_tex2D_Mut",          pSRVs[0]},
        {"g_tex2DArr_Mut",       pSRVs[0], 0},
        {"g_tex2DArr_Mut",       pSRVs[1], 1},
        {"g_tex2DArr_Mut",       pSRVs[2], 2},
        {"g_tex2DArr_Mut",       pSRVs[3], 3},
        {"g_tex2DArr_Mut",       pSRVs[0], 4},
        {"g_tex2D_Dyn",          pSRVs[0]},
        {"g_tex2DArr_Dyn",       pSRVs[0], 0},
        {"g_tex2DArr_Dyn",       pSRVs[1], 1},
        {"g_tex2DArr_Dyn",       pSRVs[2], 2},
        {"g_tex2DArr_Dyn",       pSRVs[3], 3},
        {"g_tex2DArr_Dyn",       pSRVs[0], 4},
        {"g_tex2DArr_Dyn",       pSRVs[1], 5},
        {"g_tex2DArr_Dyn",       pSRVs[2], 6},
        {"g_tex2DArr_Dyn",       pSRVs[3], 7},
        {"g_sepTex2D_mut",       pSRVs[0]},
        {"g_sepTex2DArr_mut",    pSRVs[0], 0},
        {"g_sepTex2DArr_mut",    pSRVs[1], 1},
        {"g_sepTex2DArr_mut",    pSRVs[2], 2},
        {"g_sepTex2D_dyn",       pSRVs[0]},
        {"g_sepTex2DArr_dyn",    pSRVs[0], 0},
        {"g_sepTex2DArr_dyn",    pSRVs[1], 1},
        {"g_sepTex2DArr_dyn",    pSRVs[2], 2},
        {"g_sepTex2DArr_dyn",    pSRVs[3], 3},
        {"g_sepTex2DArr_dyn",    pSRVs[0], 4},
        {"g_sepTex2DArr_dyn",    pSRVs[1], 5},
        {"g_sepTex2DArr_dyn",    pSRVs[2], 6},
        {"g_sepTex2DArr_dyn",    pSRVs[3], 7},
        {} //
    };*/
    // clang-format on

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);

    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.EntryPoint                 = "main";
    ShaderCI.UseCombinedTextureSamplers = false;

    //ResMappingDesc.pEntries = MappingEntries;
    //RefCntAutoPtr<IResourceMapping> pResMapping;
    //pDevice->CreateResourceMapping(ResMappingDesc, &pResMapping);
    //if (IsD3DDevice)
    //{
    //    pResMapping->AddResourceArray("g_SamArr_mut", 0, pSamplerObjs.data(), 3, true);
    //}

    auto GetShaderMacros = [&](const ShaderResourceCounters& Counters, bool UAVSupported) {
        ShaderMacroHelper Macros;

        Macros.AddShaderMacro("UAV_SUPPORTED", UAVSupported);
        Macros.AddShaderMacro("CONSTANT_BUFFER_ARRAYS_SUPPORTED", ConstantBufferArraysSupported);
        Macros.AddShaderMacro("HLSL_51", IsHLSL_51);

#    define ADD_ARRAY_SIZE_MACROS(ArrayType, Sizes)                                                                       \
        do                                                                                                                \
        {                                                                                                                 \
            Macros.AddShaderMacro("STATIC_"##ArrayType, static_cast<int>(Sizes[SHADER_RESOURCE_VARIABLE_TYPE_STATIC]));   \
            Macros.AddShaderMacro("MUTABLE_"##ArrayType, static_cast<int>(Sizes[SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE])); \
            Macros.AddShaderMacro("DYNAMIC_"##ArrayType, static_cast<int>(Sizes[SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC])); \
        } while (false)

        ADD_ARRAY_SIZE_MACROS("TEX_ARRAY_SIZE", Counters.TexArrSize);
        ADD_ARRAY_SIZE_MACROS("SAM_ARRAY_SIZE", Counters.SamplerArrSize);
        ADD_ARRAY_SIZE_MACROS("CB_ARRAY_SIZE", Counters.CbArrSize);
        ADD_ARRAY_SIZE_MACROS("BUFF_UAV_ARR_SIZE", Counters.BuffUavArrSize);
        ADD_ARRAY_SIZE_MACROS("TEX_UAV_ARR_SIZE", Counters.TexUavArrSize);
#    undef ADD_ARRAY_SIZE_MACROS

        return Macros;
    };

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.Name       = "Shader resource layout test VS";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.SourceLanguage  = SHADER_SOURCE_LANGUAGE_GLSL;
        ShaderCI.FilePath        = IsD3DDevice ? "ShaderResourceLayoutTestDX.hlsl" : "ShaderResourceLayoutTestGL.vsh";
        ShaderCI.EntryPoint      = IsD3DDevice ? "VSMain" : "main";

        auto Macros     = GetShaderMacros(VSResCounters, VertexShaderUAVSupported);
        ShaderCI.Macros = Macros;

        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);

        Diligent::Test::PrintShaderResources(pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.Name       = "Shader resource layout test PS";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.SourceLanguage  = SHADER_SOURCE_LANGUAGE_GLSL;
        ShaderCI.FilePath        = IsD3DDevice ? "ShaderResourceLayoutTestDX.hlsl" : "ShaderResourceLayoutTestGL.psh";
        ShaderCI.EntryPoint      = IsD3DDevice ? "PSMain" : "main";

        auto Macros     = GetShaderMacros(PSResCounters, true);
        ShaderCI.Macros = Macros;

        pDevice->CreateShader(ShaderCI, &pPS);
        ASSERT_NE(pPS, nullptr);

        Diligent::Test::PrintShaderResources(pPS);
    }

    PipelineStateDesc PSODesc;

    // clang-format off
    std::vector<ShaderResourceVariableDesc> VarDesc =
    {
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2D_Mut",              SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2D_Dyn",              SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DArr_Static",        SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DArr_Mut",           SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DArr_Dyn",           SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_sepTex2DArr_static",     SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_sepTex2D_mut",           SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_sepTex2DArr_mut",        SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_sepTex2D_dyn",           SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_sepTex2DArr_dyn",        SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_SamArr_static",          SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Sam_mut",                SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_SamArr_mut",             SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Sam_dyn",                SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_SamArr_dyn",             SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "UniformBuff_Mut",          SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "UniformBuff_Dyn",          SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "UniformBuffArr_Mut",       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "UniformBuffArr_Dyn",       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "storageBuff_Mut",          SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "storageBuff_Dyn",          SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DNoResourceTest",    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_UniformTexelBuff_mut",   SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_StorageTexelBuff_mut",   SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    
    if (IsHLSL_51 || deviceType == DeviceType::Vulkan)
    {
        VarDesc.emplace_back(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "storageBuffArr_Mut",       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);
        VarDesc.emplace_back(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "storageBuffArr_Dyn",       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
        VarDesc.emplace_back(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DStorageImgArr_Mut", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);
        VarDesc.emplace_back(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DStorageImgArr_Dyn", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
    }
    
    if (!IsD3DDevice)
    {
        VarDesc.emplace_back(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2D_Static", SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    }
    
    std::vector<StaticSamplerDesc> StaticSamplers =
    {
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Sam_static",               SamplerDesc{}},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Sam_dyn",                  SamplerDesc{}},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DNoStaticSamplerTest", SamplerDesc{}}
    };
    if (!IsD3DDevice)
    {
        StaticSamplers.emplace_back(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2D_Static", SamplerDesc{});
        StaticSamplers.emplace_back(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DArr_Mut", SamplerDesc{});
        StaticSamplers.emplace_back(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_SamArr_mut",   SamplerDesc{});
    }
    // clang-format on

    PSODesc.ResourceLayout.Variables         = VarDesc.data();
    PSODesc.ResourceLayout.NumVariables      = static_cast<Uint32>(VarDesc.size());
    PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers.data();
    PSODesc.ResourceLayout.NumStaticSamplers = static_cast<Uint32>(StaticSamplers.size());

    PSODesc.Name                                          = "Shader resource layout test";
    PSODesc.GraphicsPipeline.pVS                          = pVS;
    PSODesc.GraphicsPipeline.pPS                          = pPS;
    PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
    PSODesc.GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_RGBA8_UNORM;
    PSODesc.GraphicsPipeline.DSVFormat                    = TEX_FORMAT_UNKNOWN;
    PSODesc.SRBAllocationGranularity                      = 16;
    PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    RefCntAutoPtr<IPipelineState> pTestPSO;
    LOG_INFO_MESSAGE("The 2 warnings below about missing shader resources are part of the test");
    pDevice->CreatePipelineState(PSODesc, &pTestPSO);
    ASSERT_NE(pTestPSO, nullptr);

#    if 0

    {
        // clang-format off
        //SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_tex2D_Static",         Set,      pSRVs[0]);
        //SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_tex2DArr_Static",      SetArray, pSRVs, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_sepTex2D_static",      Set,       pSRVs[0]);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_sepTex2DArr_static",   SetArray,  pSRVs, 0, VSTexArrSize[SHADER_RESOURCE_VARIABLE_TYPE_STATIC]);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_SamArr_static",        SetArray,  pSamplerObjs.data(), 0, VSSamplerArrSize[SHADER_RESOURCE_VARIABLE_TYPE_STATIC]);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "UniformBuff_Stat",       Set,       pUBs[0]);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "UniformBuffArr_Stat",    SetArray,  pUBs, 0, VSCBArrSize[SHADER_RESOURCE_VARIABLE_TYPE_STATIC]);
        if (VertexShaderUAVSupported)
        {
            SET_STATIC_VAR(SHADER_TYPE_VERTEX, "storageBuff_Static",     Set,       pSBUAVs[0]);
            SET_STATIC_VAR(SHADER_TYPE_VERTEX, "storageBuffArr_Static",  SetArray,  pSBUAVs+1, 0, 2);
            SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_tex2DStorageImg_Stat", Set,       pUAVs[0]);
            SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_StorageTexelBuff",     Set,       pStorageTexelBuffUAV[0]);
        }
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_UniformTexelBuff",     Set,       pUniformTexelBuffSRV);

        // clang-format on
        pTestPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "g_tex2D_Mut");
        auto* pStaticSam = pTestPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "g_Sam_static");
        EXPECT_EQ(pStaticSam, nullptr);


        auto NumVSVars = pTestPSO->GetStaticVariableCount(SHADER_TYPE_VERTEX);
        for (Uint32 v = 0; v < NumVSVars; ++v)
        {
            auto pVar = pTestPSO->GetStaticVariableByIndex(SHADER_TYPE_VERTEX, v);
            EXPECT_EQ(pVar->GetIndex(), v);
            EXPECT_EQ(pVar->GetType(), SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
            auto pVar2 = pTestPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, pVar->GetResourceDesc().Name);
            EXPECT_EQ(pVar, pVar2);
        }
    }

    {
        //clang-format off
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_tex2D_Static", Set, pSRVs[0]);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_tex2DArr_Static", SetArray, pSRVs, 0, 2);
        //SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_sepTex2D_static",      Set, pSRVs[0]);
        //SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_sepTex2DArr_static",   SetArray, pSRVs, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_SamArr_static", SetArray, pSamplerObjs, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "UniformBuff_Stat", Set, pUBs[0]);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "UniformBuffArr_Stat", SetArray, pUBs, 0, ConstantBufferArraysSupported ? 2 : 1);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "storageBuff_Static", Set, pSBUAVs[2]);
        if (IsHLSL_51 || deviceType == DeviceType::Vulkan)
        {
            SET_STATIC_VAR(SHADER_TYPE_PIXEL, "storageBuffArr_Static", SetArray, pSBUAVs + 3, 0, 2);
        }
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_tex2DStorageImg_Stat", Set, pUAVs[1]);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_UniformTexelBuff", Set, pUniformTexelBuffSRV);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_StorageTexelBuff", Set, pStorageTexelBuffUAV[1]);
        //clang-format on
        pTestPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "storageBuff_Dyn");
        auto* pStaticSam = pTestPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_Sam_static");
        EXPECT_EQ(pStaticSam, nullptr);

        auto NumPSVars = pTestPSO->GetStaticVariableCount(SHADER_TYPE_PIXEL);
        for (Uint32 v = 0; v < NumPSVars; ++v)
        {
            auto pVar = pTestPSO->GetStaticVariableByIndex(SHADER_TYPE_PIXEL, v);
            EXPECT_EQ(pVar->GetIndex(), v);
            EXPECT_EQ(pVar->GetType(), SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
            auto pVar2 = pTestPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, pVar->GetResourceDesc().Name);
            EXPECT_EQ(pVar, pVar2);
        }
    }

    pTestPSO->BindStaticResources(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, pResMapping, BIND_SHADER_RESOURCES_KEEP_EXISTING | BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED | BIND_SHADER_RESOURCES_UPDATE_STATIC);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pTestPSO->CreateShaderResourceBinding(&pSRB, true);
    ASSERT_NE(pSRB, nullptr);

    EXPECT_EQ(pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "UniformBuff_Stat"), nullptr);
    EXPECT_EQ(pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_sepTex2DArr_static"), nullptr);



    // clang-format off
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2D_Mut",    Set,      pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DArr_Mut", SetArray, pSRVs, 0, 5);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2D_Dyn",    Set,      pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DArr_Dyn", SetArray, pSRVs, 0, 3);
   
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2D_mut",       Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2DArr_mut",    SetArray,  pSRVs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2D_dyn",       Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2DArr_dyn",    SetArray,  pSRVs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_Sam_mut",    Set,       pSamplerObjs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_SamArr_dyn", SetArray,  pSamplerObjs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_VERTEX, "UniformBuff_Mut",    Set,       pUBs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "UniformBuffArr_Mut", SetArray,  pUBs, 0, ConstantBufferArraysSupported ?  3 : 1);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "UniformBuff_Dyn",    Set,       pUBs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "UniformBuffArr_Dyn", SetArray,  pUBs, 0, ConstantBufferArraysSupported ? 4 : 1);

    if (VertexShaderUAVSupported)
    {
        SET_SRB_VAR(SHADER_TYPE_VERTEX, "storageBuff_Mut",    Set,       pSBUAVs[5]);
        SET_SRB_VAR(SHADER_TYPE_VERTEX, "storageBuffArr_Mut", SetArray,  pSBUAVs+6, 0, 3);
        SET_SRB_VAR(SHADER_TYPE_VERTEX, "storageBuff_Dyn",    Set,       pSBUAVs[9]);
        SET_SRB_VAR(SHADER_TYPE_VERTEX, "storageBuffArr_Dyn", SetArray,  pSBUAVs+10, 0, 4);

        SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DStorageImgArr_Mut", SetArray,  pUAVs+2, 0, 2);
        SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DStorageImgArr_Dyn", SetArray,  pUAVs+4, 0, 2);

        SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_StorageTexelBuff_mut", Set,  pStorageTexelBuffUAV[2]);
    }
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_UniformTexelBuff_mut", Set,  pUniformTexelBuffSRV);


    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2D_Mut",    Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2DArr_Mut", SetArray,  pSRVs, 0, 5);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2D_Dyn",    Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2DArr_Dyn", SetArray,  pSRVs, 0, 3);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_sepTex2D_mut",    Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_sepTex2DArr_mut", SetArray,  pSRVs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_sepTex2D_dyn",    Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_sepTex2DArr_dyn", SetArray,  pSRVs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_Sam_mut",    Set,       pSamplerObjs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_SamArr_dyn", SetArray,  pSamplerObjs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuff_Mut",    Set,       pUBs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuff_Dyn",    Set,       pUBs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuffArr_Mut", SetArray,  pUBs, 0, ConstantBufferArraysSupported ? 3 : 1);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuffArr_Dyn", SetArray,  pUBs, 0, ConstantBufferArraysSupported ? 4 : 1);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuff_Mut",    Set,       pSBUAVs[14]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuff_Dyn",    Set,       pSBUAVs[15]);
    if (IsHLSL_51 || deviceType == DeviceType::Vulkan)
    {
        SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuffArr_Mut", SetArray,  pSBUAVs+16, 0, 3);
        SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuffArr_Dyn", SetArray,  pSBUAVs+19, 0, 4);
        SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2DStorageImgArr_Mut", SetArray,  pUAVs+6, 0, 2);
        SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2DStorageImgArr_Dyn", SetArray,  pUAVs+8, 0, 2);
    }

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_UniformTexelBuff_mut", Set,  pUniformTexelBuffSRV);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_StorageTexelBuff_mut", Set,  pStorageTexelBuffUAV[3]);
    // clang-format on

    pSRB->BindResources(SHADER_TYPE_PIXEL | SHADER_TYPE_VERTEX, pResMapping, BIND_SHADER_RESOURCES_KEEP_EXISTING | BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED | BIND_SHADER_RESOURCES_UPDATE_MUTABLE | BIND_SHADER_RESOURCES_UPDATE_DYNAMIC);

    pContext->SetPipelineState(pTestPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);

    // clang-format off
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuff_Dyn",           Set, pSBUAVs[23]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2D_Dyn",              Set, pSRVs[1]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2D_dyn",           Set, pSRVs[1]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_SamArr_dyn",              SetArray, pSamplerObjs + 1, 1, 3);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuff_Dyn",           Set, pUBs[1]);
    if (VertexShaderUAVSupported)
    {
        SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DStorageImgArr_Dyn", SetArray, pUAVs + 10, 1, 1);
    }
    // clang-format on

    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->Draw(DrawAttrs);

    {
        auto NumVSVars = pSRB->GetVariableCount(SHADER_TYPE_VERTEX);
        for (Uint32 v = 0; v < NumVSVars; ++v)
        {
            auto pVar = pSRB->GetVariableByIndex(SHADER_TYPE_VERTEX, v);
            EXPECT_EQ(pVar->GetIndex(), v);
            EXPECT_TRUE(pVar->GetType() == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE || pVar->GetType() == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
            auto pVar2 = pSRB->GetVariableByName(SHADER_TYPE_VERTEX, pVar->GetResourceDesc().Name);
            EXPECT_EQ(pVar, pVar2);
        }
    }

    {
        auto NumPSVars = pSRB->GetVariableCount(SHADER_TYPE_PIXEL);
        for (Uint32 v = 0; v < NumPSVars; ++v)
        {
            auto pVar = pSRB->GetVariableByIndex(SHADER_TYPE_PIXEL, v);
            EXPECT_EQ(pVar->GetIndex(), v);
            EXPECT_TRUE(pVar->GetType() == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE || pVar->GetType() == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
            auto pVar2 = pSRB->GetVariableByName(SHADER_TYPE_PIXEL, pVar->GetResourceDesc().Name);
            EXPECT_EQ(pVar, pVar2);
        }
    }
#    endif
}
#endif
} // namespace
