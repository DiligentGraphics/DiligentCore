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

#include "DXCompiler.hpp"
#include "TestingEnvironment.hpp"

#include "gtest/gtest.h"

#include <atlcomcli.h>
#include <d3d12shader.h>

#include "dxc/dxcapi.h"

#ifndef NTDDI_WIN10_VB // First defined in Win SDK 10.0.19041.0
#    define D3D_SIT_RTACCELERATIONSTRUCTURE (D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER + 1)
#endif

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

const std::string ReflectionTest_RG = R"hlsl(

#ifdef ASSIGN_BINDINGS
#   define REGISTER(r, s) : register(r, s)
#else
#   define REGISTER(r, s)
#endif

RaytracingAccelerationStructure g_TLAS        REGISTER(t11, space2);
RWTexture2D<float4>             g_ColorBuffer REGISTER(u2,  space1);
Texture2D                       g_Tex[2]      REGISTER(t23, space5);
SamplerState                    g_TexSampler  REGISTER(s15, space4);

cbuffer cbConstants REGISTER(b17, space15)
{
    float4 g_CBData;
}

struct RTPayload
{
    float4 Color;
};

[shader("raygeneration")]
void main()
{
    const float2 uv = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy - 1);

    RayDesc ray;
    ray.Origin    = float3(uv.x, 1.0 - uv.y, -1.0);
    ray.Direction = float3(0.0, 0.0, 1.0);
    ray.TMin      = 0.01;
    ray.TMax      = 10.0;

    RTPayload payload = {float4(0, 0, 0, 0)};
    TraceRay(g_TLAS,         // Acceleration Structure
             RAY_FLAG_NONE,  // Ray Flags
             ~0,             // Instance Inclusion Mask
             0,              // Ray Contribution To Hit Group Index
             1,              // Multiplier For Geometry Contribution To Hit Group Index
             0,              // Miss Shader Index
             ray,
             payload);

    g_ColorBuffer[DispatchRaysIndex().xy] = 
        payload.Color + 
        g_Tex[0].SampleLevel(g_TexSampler, uv, 0) +
        g_Tex[1].SampleLevel(g_TexSampler, uv, 0) +
        g_CBData;
}
)hlsl";

const wchar_t* DXCArgs[] = {
    L"-Zpc", // Matrices in column-major order
#ifdef DILIGENT_DEBUG
    L"-Zi", // Debug info
    L"-Od"  // Disable optimization

// Silence the following warning:
// no output provided for debug - embedding PDB in shader container.  Use -Qembed_debug to silence this warning.
// L"-Qembed_debug", // Requires DXC1.5+
#else
    L"-O3" // Optimization level 3
#endif
};

TEST(DXCompilerTest, Reflection)
{
    auto pDXC = CreateDXCompiler(DXCompilerTarget::Direct3D12, nullptr);
    ASSERT_TRUE(pDXC);

    IDXCompiler::CompileAttribs CA;
    CA.Source       = ReflectionTest_RG.c_str();
    CA.SourceLength = static_cast<Uint32>(ReflectionTest_RG.length());
    CA.EntryPoint   = L"main";
    CA.Profile      = L"lib_6_3";
    CA.pArgs        = DXCArgs;
    CA.ArgsCount    = _countof(DXCArgs);

    DxcDefine Defines[] = {{L"ASSIGN_BINDINGS", L"1"}};
    CA.pDefines         = Defines;
    CA.DefinesCount     = _countof(Defines);

    CComPtr<IDxcBlob> pDXIL, pOutput;
    CA.ppBlobOut        = &pDXIL.p;
    CA.ppCompilerOutput = &pOutput.p;
    pDXC->Compile(CA);
    ASSERT_TRUE(pDXIL) << (pOutput ? std::string{reinterpret_cast<const char*>(pOutput->GetBufferPointer()), pOutput->GetBufferSize()} : "");

    CComPtr<ID3D12ShaderReflection> pReflection;
    pDXC->GetD3D12ShaderReflection(pDXIL, &pReflection);
    ASSERT_TRUE(pReflection);

    D3D12_SHADER_DESC ShaderDesc;
    EXPECT_HRESULT_SUCCEEDED(pReflection->GetDesc(&ShaderDesc));
    EXPECT_EQ(ShaderDesc.BoundResources, 5U);

    D3D12_SHADER_INPUT_BIND_DESC BindDesc = {};
    EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TLAS", &BindDesc));
    EXPECT_STREQ(BindDesc.Name, "g_TLAS");
    EXPECT_EQ(BindDesc.Type, D3D_SIT_RTACCELERATIONSTRUCTURE);
    EXPECT_EQ(BindDesc.BindPoint, 11U);
    EXPECT_EQ(BindDesc.BindCount, 1U);
    EXPECT_EQ(BindDesc.Space, 2U);

    EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_ColorBuffer", &BindDesc));
    EXPECT_STREQ(BindDesc.Name, "g_ColorBuffer");
    EXPECT_EQ(BindDesc.Type, D3D_SIT_UAV_RWTYPED);
    EXPECT_EQ(BindDesc.BindPoint, 2U);
    EXPECT_EQ(BindDesc.BindCount, 1U);
    EXPECT_EQ(BindDesc.Space, 1U);

    EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex", &BindDesc));
    EXPECT_STREQ(BindDesc.Name, "g_Tex");
    EXPECT_EQ(BindDesc.Type, D3D_SIT_TEXTURE);
    EXPECT_EQ(BindDesc.BindPoint, 23U);
    EXPECT_EQ(BindDesc.BindCount, 2U);
    EXPECT_EQ(BindDesc.Space, 5U);

    EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TexSampler", &BindDesc));
    EXPECT_STREQ(BindDesc.Name, "g_TexSampler");
    EXPECT_EQ(BindDesc.Type, D3D_SIT_SAMPLER);
    EXPECT_EQ(BindDesc.BindPoint, 15U);
    EXPECT_EQ(BindDesc.BindCount, 1U);
    EXPECT_EQ(BindDesc.Space, 4U);

    EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("cbConstants", &BindDesc));
    EXPECT_STREQ(BindDesc.Name, "cbConstants");
    EXPECT_EQ(BindDesc.Type, D3D_SIT_CBUFFER);
    EXPECT_EQ(BindDesc.BindPoint, 17U);
    EXPECT_EQ(BindDesc.BindCount, 1U);
    EXPECT_EQ(BindDesc.Space, 15U);
}

TEST(DXCompilerTest, RemapBindingsRG)
{
    auto pDXC = CreateDXCompiler(DXCompilerTarget::Direct3D12, nullptr);
    ASSERT_TRUE(pDXC);

    IDXCompiler::CompileAttribs CA;
    CA.Source       = ReflectionTest_RG.c_str();
    CA.SourceLength = static_cast<Uint32>(ReflectionTest_RG.length());
    CA.EntryPoint   = L"main";
    CA.Profile      = L"lib_6_3";
    CA.pArgs        = DXCArgs;
    CA.ArgsCount    = _countof(DXCArgs);

    CComPtr<IDxcBlob> pDXIL, pOutput;
    CA.ppBlobOut        = &pDXIL.p;
    CA.ppCompilerOutput = &pOutput.p;
    pDXC->Compile(CA);
    ASSERT_TRUE(pDXIL) << (pOutput ? std::string{reinterpret_cast<const char*>(pOutput->GetBufferPointer()), pOutput->GetBufferSize()} : "");

    IDXCompiler::TResourceBindingMap BindigMap;
    BindigMap["g_TLAS"]        = {15, 0, 1};
    BindigMap["g_ColorBuffer"] = {7, 1, 1};
    BindigMap["g_Tex"]         = {101, 0, 2};
    BindigMap["g_TexSampler"]  = {0, 2, 1};
    BindigMap["cbConstants"]   = {9, 0, 1};
    BindigMap["g_AnotherRes"]  = {567, 5, 1};
    CComPtr<IDxcBlob> pRemappedDXIL;
    pDXC->RemapResourceBindings(BindigMap, pDXIL, &pRemappedDXIL);
    ASSERT_TRUE(pRemappedDXIL);

    {
        CComPtr<ID3D12ShaderReflection> pReflection;
        pDXC->GetD3D12ShaderReflection(pRemappedDXIL, &pReflection);
        ASSERT_TRUE(pReflection);

        D3D12_SHADER_INPUT_BIND_DESC BindDesc = {};
        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TLAS", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 15U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_ColorBuffer", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 7U);
        EXPECT_EQ(BindDesc.Space, 1U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 101U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TexSampler", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 0U);
        EXPECT_EQ(BindDesc.Space, 2U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("cbConstants", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 9U);
        EXPECT_EQ(BindDesc.Space, 0U);
    }

    BindigMap["g_TLAS"]        = {0, 0, 1};
    BindigMap["g_ColorBuffer"] = {1, 0, 1};
    BindigMap["g_Tex"]         = {2, 0, 2};
    BindigMap["g_TexSampler"]  = {0, 1, 1};
    BindigMap["cbConstants"]   = {1, 1, 1};
    CComPtr<IDxcBlob> pRemappedDXIL2;
    pDXC->RemapResourceBindings(BindigMap, pRemappedDXIL, &pRemappedDXIL2);
    ASSERT_TRUE(pRemappedDXIL2);

    {
        CComPtr<ID3D12ShaderReflection> pReflection;
        pDXC->GetD3D12ShaderReflection(pRemappedDXIL2, &pReflection);
        ASSERT_TRUE(pReflection);

        D3D12_SHADER_INPUT_BIND_DESC BindDesc = {};
        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TLAS", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 0U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_ColorBuffer", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 1U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 2U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TexSampler", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 0U);
        EXPECT_EQ(BindDesc.Space, 1U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("cbConstants", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 1U);
        EXPECT_EQ(BindDesc.Space, 1U);
    }
}

TEST(DXCompilerTest, RemapBindingsPS_1)
{
    const std::string ShaderSource = R"hlsl(
Texture2D     g_Tex1;
Texture2D     g_Tex2;
SamplerState  g_TexSampler;

cbuffer cbConstants1
{
    float4 g_CBData1;
}

cbuffer cbConstants2
{
    float4 g_CBData2;
}

float4 main() : SV_TARGET
{
    float2 uv = float2(0.0, 1.0);
    return g_Tex1.Sample(g_TexSampler, uv) * g_CBData1 +
           g_Tex2.Sample(g_TexSampler, uv) * g_CBData2;
}
)hlsl";

    auto pDXC = CreateDXCompiler(DXCompilerTarget::Direct3D12, nullptr);
    ASSERT_TRUE(pDXC);

    IDXCompiler::CompileAttribs CA;
    CA.Source       = ShaderSource.c_str();
    CA.SourceLength = static_cast<Uint32>(ShaderSource.length());
    CA.EntryPoint   = L"main";
    CA.Profile      = L"ps_6_0";
    CA.pArgs        = DXCArgs;
    CA.ArgsCount    = _countof(DXCArgs);

    CComPtr<IDxcBlob> pDXIL, pOutput;
    CA.ppBlobOut        = &pDXIL.p;
    CA.ppCompilerOutput = &pOutput.p;
    pDXC->Compile(CA);
    ASSERT_TRUE(pDXIL) << (pOutput ? std::string{reinterpret_cast<const char*>(pOutput->GetBufferPointer()), pOutput->GetBufferSize()} : "");

    IDXCompiler::TResourceBindingMap BindigMap;
    BindigMap["g_Tex1"]       = {101, 0, 1};
    BindigMap["g_Tex2"]       = {22, 0, 1};
    BindigMap["g_TexSampler"] = {0, 0, 1};
    BindigMap["cbConstants1"] = {9, 0, 1};
    BindigMap["cbConstants2"] = {3, 0, 1};
    BindigMap["g_AnotherRes"] = {567, 0, 1};
    CComPtr<IDxcBlob> pRemappedDXIL;
    pDXC->RemapResourceBindings(BindigMap, pDXIL, &pRemappedDXIL);
    ASSERT_TRUE(pRemappedDXIL);

    {
        CComPtr<ID3D12ShaderReflection> pReflection;
        pDXC->GetD3D12ShaderReflection(pRemappedDXIL, &pReflection);
        ASSERT_TRUE(pReflection);

        D3D12_SHADER_INPUT_BIND_DESC BindDesc = {};
        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex1", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 101U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex2", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 22U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TexSampler", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 0U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("cbConstants1", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 9U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("cbConstants2", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 3U);
        EXPECT_EQ(BindDesc.Space, 0U);
    }

    BindigMap.clear();
    BindigMap["g_Tex1"]       = {0, 2, 1};
    BindigMap["g_Tex2"]       = {55, 4, 1};
    BindigMap["g_TexSampler"] = {1, 2, 1};
    BindigMap["cbConstants1"] = {8, 3, 1};
    BindigMap["cbConstants2"] = {4, 6, 1};
    BindigMap["g_AnotherRes"] = {567, 0, 1};
    pRemappedDXIL             = nullptr;
    pDXC->RemapResourceBindings(BindigMap, pDXIL, &pRemappedDXIL);
    ASSERT_TRUE(pRemappedDXIL);

    {
        CComPtr<ID3D12ShaderReflection> pReflection;
        pDXC->GetD3D12ShaderReflection(pRemappedDXIL, &pReflection);
        ASSERT_TRUE(pReflection);

        D3D12_SHADER_INPUT_BIND_DESC BindDesc = {};
        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex1", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 0U);
        EXPECT_EQ(BindDesc.Space, 2U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex2", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 55U);
        EXPECT_EQ(BindDesc.Space, 4U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TexSampler", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 1U);
        EXPECT_EQ(BindDesc.Space, 2U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("cbConstants1", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 8U);
        EXPECT_EQ(BindDesc.Space, 3U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("cbConstants2", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 4U);
        EXPECT_EQ(BindDesc.Space, 6U);
    }
}

TEST(DXCompilerTest, RemapBindingsPS_2)
{
    const std::string ShaderSource = R"hlsl(
Texture2D     g_Tex[4];
Texture3D     g_Tex3D;
SamplerState  g_TexSampler;

RWTexture2D<float4> g_ColorBuffer1;
RWTexture2D<float4> g_ColorBuffer2;
RWTexture2D<float4> g_ColorBuffer3;

StructuredBuffer<float4> g_Buffer[5];

float4 main() : SV_TARGET
{
    float2 uv = float2(0.0, 1.0);
    int2   pos = int2(1,2);

    g_ColorBuffer1[pos] = g_Buffer[3][1];
    g_ColorBuffer2[pos] = g_ColorBuffer3[pos];

    return g_Tex[0].Sample(g_TexSampler, uv) *
           g_Tex[2].Sample(g_TexSampler, uv) +
           g_Tex3D.Sample(g_TexSampler, uv.xxy) +
           g_Buffer[1][9] * g_Buffer[4][100];
}
)hlsl";

    auto pDXC = CreateDXCompiler(DXCompilerTarget::Direct3D12, nullptr);
    ASSERT_TRUE(pDXC);

    IDXCompiler::CompileAttribs CA;
    CA.Source       = ShaderSource.c_str();
    CA.SourceLength = static_cast<Uint32>(ShaderSource.length());
    CA.EntryPoint   = L"main";
    CA.Profile      = L"ps_6_0";
    CA.pArgs        = DXCArgs;
    CA.ArgsCount    = _countof(DXCArgs);

    CComPtr<IDxcBlob> pDXIL, pOutput;
    CA.ppBlobOut        = &pDXIL.p;
    CA.ppCompilerOutput = &pOutput.p;
    pDXC->Compile(CA);
    ASSERT_TRUE(pDXIL) << (pOutput ? std::string{reinterpret_cast<const char*>(pOutput->GetBufferPointer()), pOutput->GetBufferSize()} : "");

    IDXCompiler::TResourceBindingMap BindigMap;
    BindigMap["g_Tex"]          = {101, 0, 4};
    BindigMap["g_Tex3D"]        = {22, 0, 1};
    BindigMap["g_TexSampler"]   = {0, 0, 1};
    BindigMap["g_Buffer"]       = {9, 0, 1};
    BindigMap["g_ColorBuffer1"] = {180, 0, 1};
    BindigMap["g_ColorBuffer2"] = {333, 0, 1};
    BindigMap["g_ColorBuffer3"] = {1, 0, 1};
    BindigMap["g_AnotherRes"]   = {567, 0, 1};
    CComPtr<IDxcBlob> pRemappedDXIL;
    pDXC->RemapResourceBindings(BindigMap, pDXIL, &pRemappedDXIL);
    ASSERT_TRUE(pRemappedDXIL);

    {
        CComPtr<ID3D12ShaderReflection> pReflection;
        pDXC->GetD3D12ShaderReflection(pRemappedDXIL, &pReflection);
        ASSERT_TRUE(pReflection);

        D3D12_SHADER_INPUT_BIND_DESC BindDesc = {};
        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 101U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex3D", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 22U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TexSampler", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 0U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Buffer", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 9U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_ColorBuffer1", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 180U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_ColorBuffer2", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 333U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_ColorBuffer3", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 1U);
        EXPECT_EQ(BindDesc.Space, 0U);
    }

    BindigMap.clear();
    BindigMap["g_Tex"]          = {77, 1, 4};
    BindigMap["g_Tex3D"]        = {90, 1, 1};
    BindigMap["g_TexSampler"]   = {0, 1, 1};
    BindigMap["g_Buffer"]       = {15, 6, 1};
    BindigMap["g_ColorBuffer1"] = {33, 6, 1};
    BindigMap["g_ColorBuffer2"] = {10, 100, 1};
    BindigMap["g_ColorBuffer3"] = {11, 100, 1};
    BindigMap["g_AnotherRes"]   = {567, 0, 1};
    pRemappedDXIL               = nullptr;
    pDXC->RemapResourceBindings(BindigMap, pDXIL, &pRemappedDXIL);
    ASSERT_TRUE(pRemappedDXIL);

    {
        CComPtr<ID3D12ShaderReflection> pReflection;
        pDXC->GetD3D12ShaderReflection(pRemappedDXIL, &pReflection);
        ASSERT_TRUE(pReflection);

        D3D12_SHADER_INPUT_BIND_DESC BindDesc = {};
        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 77U);
        EXPECT_EQ(BindDesc.Space, 1U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex3D", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 90U);
        EXPECT_EQ(BindDesc.Space, 1U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TexSampler", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 0U);
        EXPECT_EQ(BindDesc.Space, 1U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Buffer", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 15U);
        EXPECT_EQ(BindDesc.Space, 6U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_ColorBuffer1", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 33U);
        EXPECT_EQ(BindDesc.Space, 6U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_ColorBuffer2", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 10U);
        EXPECT_EQ(BindDesc.Space, 100U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_ColorBuffer3", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 11U);
        EXPECT_EQ(BindDesc.Space, 100U);
    }
}
} // namespace
