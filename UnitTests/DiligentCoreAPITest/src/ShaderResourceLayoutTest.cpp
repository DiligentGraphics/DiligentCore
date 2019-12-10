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
#include <vector>

#include "TestingEnvironment.h"

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

TEST(ShaderResourceLayout, VulkanResourceLayout)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    auto deviceType = pDevice->GetDeviceCaps().DevType;
    if (deviceType != DeviceType::D3D12)
    {
        GTEST_SKIP();
    }

    auto IsD3DDevice = pDevice->GetDeviceCaps().IsD3DDevice();

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);

    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.EntryPoint                 = "main";
    ShaderCI.UseCombinedTextureSamplers = false;

    RefCntAutoPtr<ISampler> pSamplers[4];
    IDeviceObject*          pSams[4];
    for (int i = 0; i < 4; ++i)
    {
        SamplerDesc SamDesc;
        pDevice->CreateSampler(SamDesc, &(pSamplers[i]));
        ASSERT_NE(pSamplers[i], nullptr);
        pSams[i] = pSamplers[i];
    }

    RefCntAutoPtr<ITexture> pTex[4];

    TextureDesc TexDesc;
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Width     = 1024;
    TexDesc.Height    = 1024;
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
    RefCntAutoPtr<ITexture> pStorageTex[4];
    IDeviceObject*          pUAVs[4];
    for (int i = 0; i < 4; ++i)
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
    BuffDesc.uiSizeInBytes = 1024;
    BuffDesc.BindFlags     = BIND_UNIFORM_BUFFER;
    RefCntAutoPtr<IBuffer> pUniformBuffs[4];
    IDeviceObject*         pUBs[4];
    for (int i = 0; i < 4; ++i)
    {
        pDevice->CreateBuffer(BuffDesc, nullptr, &(pUniformBuffs[i]));
        ASSERT_NE(pUniformBuffs[i], nullptr);
        pUBs[i] = pUniformBuffs[i];
    }

    BuffDesc.BindFlags         = BIND_UNORDERED_ACCESS;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = 16;
    RefCntAutoPtr<IBuffer> pStorgeBuffs[4];
    IDeviceObject*         pSBUAVs[4];
    for (int i = 0; i < 4; ++i)
    {
        pDevice->CreateBuffer(BuffDesc, nullptr, &(pStorgeBuffs[i]));
        ASSERT_NE(pStorgeBuffs[i], nullptr);
        pSBUAVs[i] = pStorgeBuffs[i]->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
    }

    RefCntAutoPtr<IBuffer>     pUniformTexelBuff0, pUniformTexelBuff1, pStorageTexelBuff;
    RefCntAutoPtr<IBufferView> pUniformTexelBuffSRV, pStorageTexelBuffUAV;
    {
        BufferDesc TxlBuffDesc;
        TxlBuffDesc.Name              = "Uniform texel buffer test";
        TxlBuffDesc.uiSizeInBytes     = 256;
        TxlBuffDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        TxlBuffDesc.Usage             = USAGE_DEFAULT;
        TxlBuffDesc.ElementByteStride = 16;
        TxlBuffDesc.Mode              = BUFFER_MODE_FORMATTED;
        pDevice->CreateBuffer(TxlBuffDesc, nullptr, &pUniformTexelBuff0);
        ASSERT_NE(pUniformTexelBuff0, nullptr);
        pDevice->CreateBuffer(TxlBuffDesc, nullptr, &pUniformTexelBuff1);
        ASSERT_NE(pUniformTexelBuff1, nullptr);

        BufferViewDesc TxlBuffViewDesc;
        TxlBuffViewDesc.Name                 = "Uniform texel buffer SRV";
        TxlBuffViewDesc.ViewType             = BUFFER_VIEW_SHADER_RESOURCE;
        TxlBuffViewDesc.Format.ValueType     = VT_FLOAT32;
        TxlBuffViewDesc.Format.NumComponents = 4;
        TxlBuffViewDesc.Format.IsNormalized  = false;
        pUniformTexelBuff0->CreateView(TxlBuffViewDesc, &pUniformTexelBuffSRV);
        ASSERT_NE(pUniformTexelBuffSRV, nullptr);

        TxlBuffDesc.Name      = "Storage texel buffer test";
        TxlBuffDesc.BindFlags = BIND_UNORDERED_ACCESS;
        pDevice->CreateBuffer(TxlBuffDesc, nullptr, &pStorageTexelBuff);
        ASSERT_NE(pStorageTexelBuff, nullptr);

        TxlBuffViewDesc.Name     = "Storage texel buffer UAV";
        TxlBuffViewDesc.ViewType = BUFFER_VIEW_UNORDERED_ACCESS;
        pUniformTexelBuff1->CreateView(TxlBuffViewDesc, &pStorageTexelBuffUAV);
        ASSERT_NE(pStorageTexelBuffUAV, nullptr);
    }

    ResourceMappingDesc ResMappingDesc;
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
    };
    ResMappingDesc.pEntries = MappingEntries;
    RefCntAutoPtr<IResourceMapping> pResMapping;
    pDevice->CreateResourceMapping(ResMappingDesc, &pResMapping);
    if (IsD3DDevice)
    {
        pResMapping->AddResourceArray("g_SamArr_mut", 0, pSams, 3, true);
    }
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.Name       = "Shader resource layout test VS";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.SourceLanguage  = SHADER_SOURCE_LANGUAGE_GLSL;
        ShaderCI.FilePath        = IsD3DDevice ? "ShaderResourceLayoutTestDX.vsh" : "ShaderResourceLayoutTestGL.vsh";

        pDevice->CreateShader(ShaderCI, &pVS);
        ASSERT_NE(pVS, nullptr);

        Diligent::Test::PrintShaderResources(pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.Name       = "Shader resource layout test PS";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.SourceLanguage  = SHADER_SOURCE_LANGUAGE_GLSL;
        ShaderCI.FilePath        = IsD3DDevice ? "ShaderResourceLayoutTestDX.psh" : "ShaderResourceLayoutTestGL.psh";

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
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "storageBuffArr_Mut",       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "storageBuffArr_Dyn",       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DStorageImgArr_Mut", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DStorageImgArr_Dyn", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_tex2DNoResourceTest",    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_UniformTexelBuff_mut",   SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_StorageTexelBuff_mut",   SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
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

#define SET_STATIC_VAR(ShaderFlags, VarName, SetMethod, ...)                       \
    do                                                                             \
    {                                                                              \
        auto pStaticVar = pTestPSO->GetStaticVariableByName(ShaderFlags, VarName); \
        EXPECT_NE(pStaticVar, nullptr) << "static variable " << VarName;           \
        if (pStaticVar != nullptr)                                                 \
            pStaticVar->SetMethod(__VA_ARGS__);                                    \
    } while (false)

    {
        //clang-format off
        //SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_tex2D_Static",         Set,      pSRVs[0]);
        //SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_tex2DArr_Static",      SetArray, pSRVs, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_sepTex2D_static", Set, pSRVs[0]);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_sepTex2DArr_static", SetArray, pSRVs, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_SamArr_static", SetArray, pSams, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "UniformBuff_Stat", Set, pUBs[0]);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "UniformBuffArr_Stat", SetArray, pUBs, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "storageBuff_Static", Set, pSBUAVs[0]);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "storageBuffArr_Static", SetArray, pSBUAVs, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_tex2DStorageImg_Stat", Set, pUAVs[0]);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_UniformTexelBuff", Set, pUniformTexelBuffSRV);
        SET_STATIC_VAR(SHADER_TYPE_VERTEX, "g_StorageTexelBuff", Set, pStorageTexelBuffUAV);
        //clang-format on
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
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_SamArr_static", SetArray, pSams, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "UniformBuff_Stat", Set, pUBs[0]);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "UniformBuffArr_Stat", SetArray, pUBs, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "storageBuff_Static", Set, pSBUAVs[0]);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "storageBuffArr_Static", SetArray, pSBUAVs, 0, 2);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_tex2DStorageImg_Stat", Set, pUAVs[0]);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_UniformTexelBuff", Set, pUniformTexelBuffSRV);
        SET_STATIC_VAR(SHADER_TYPE_PIXEL, "g_StorageTexelBuff", Set, pStorageTexelBuffUAV);
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

#define SET_SRB_VAR(ShaderFlags, VarName, SetMethod, ...)          \
    do                                                             \
    {                                                              \
        auto pVar = pSRB->GetVariableByName(ShaderFlags, VarName); \
        EXPECT_NE(pVar, nullptr) << "static variable " << VarName; \
        if (pVar != nullptr)                                       \
            pVar->SetMethod(__VA_ARGS__);                          \
    } while (false)

    // clang-format off
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2D_Mut",    Set,      pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DArr_Mut", SetArray, pSRVs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2D_Dyn",    Set,      pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DArr_Dyn", SetArray, pSRVs, 0, 4);
   
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2D_mut",       Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2DArr_mut",    SetArray,  pSRVs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2D_dyn",       Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2DArr_dyn",    SetArray,  pSRVs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_Sam_mut",    Set,       pSams[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_SamArr_dyn", SetArray,  pSams, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_VERTEX, "UniformBuff_Mut",    Set,       pUBs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "UniformBuffArr_Mut", SetArray,  pUBs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "UniformBuff_Dyn",    Set,       pUBs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "UniformBuffArr_Dyn", SetArray,  pUBs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_VERTEX, "storageBuff_Mut",    Set,       pSBUAVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "storageBuffArr_Mut", SetArray,  pSBUAVs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "storageBuff_Dyn",    Set,       pSBUAVs[0]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "storageBuffArr_Dyn", SetArray,  pSBUAVs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DStorageImgArr_Mut", SetArray,  pUAVs, 0, 2);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DStorageImgArr_Dyn", SetArray,  pUAVs, 0, 2);

    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_UniformTexelBuff_mut", Set,  pUniformTexelBuffSRV);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_StorageTexelBuff_mut", Set,  pStorageTexelBuffUAV);



    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2D_Mut",    Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2DArr_Mut", SetArray,  pSRVs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2D_Dyn",    Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2DArr_Dyn", SetArray,  pSRVs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_sepTex2D_mut",    Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_sepTex2DArr_mut", SetArray,  pSRVs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_sepTex2D_dyn",    Set,       pSRVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_sepTex2DArr_dyn", SetArray,  pSRVs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_Sam_mut",    Set,       pSams[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_SamArr_dyn", SetArray,  pSams, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuff_Mut",    Set,       pUBs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuffArr_Mut", SetArray,  pUBs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuff_Dyn",    Set,       pUBs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuffArr_Dyn", SetArray,  pUBs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuff_Mut",    Set,       pSBUAVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuffArr_Mut", SetArray,  pSBUAVs, 0, 3);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuff_Dyn",    Set,       pSBUAVs[0]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuffArr_Dyn", SetArray,  pSBUAVs, 0, 4);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2DStorageImgArr_Mut", SetArray,  pUAVs, 0, 2);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_tex2DStorageImgArr_Dyn", SetArray,  pUAVs, 0, 2);

    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_UniformTexelBuff_mut", Set,  pUniformTexelBuffSRV);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_StorageTexelBuff_mut", Set,  pStorageTexelBuffUAV);
    // clang-format on

    pSRB->BindResources(SHADER_TYPE_PIXEL | SHADER_TYPE_VERTEX, pResMapping, BIND_SHADER_RESOURCES_KEEP_EXISTING | BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED | BIND_SHADER_RESOURCES_UPDATE_MUTABLE | BIND_SHADER_RESOURCES_UPDATE_DYNAMIC);

    pContext->SetPipelineState(pTestPSO);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs(3, DRAW_FLAG_VERIFY_ALL);
    pContext->Draw(DrawAttrs);

    // clang-format off
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "storageBuff_Dyn",           Set, pSBUAVs[1]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2D_Dyn",              Set, pSRVs[1]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_sepTex2D_dyn",           Set, pSRVs[1]);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "g_SamArr_dyn",              SetArray, pSams + 1, 1, 3);
    SET_SRB_VAR(SHADER_TYPE_PIXEL, "UniformBuff_Dyn",           Set, pUBs[1]);
    SET_SRB_VAR(SHADER_TYPE_VERTEX, "g_tex2DStorageImgArr_Dyn", SetArray, pUAVs + 1, 1, 1);
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
}

} // namespace
