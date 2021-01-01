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

#ifndef NTDDI_WIN10_19H1 // Defined in Win SDK 19041
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

TEST(DXCompilerTest, RemapBindings)
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
    BindigMap["g_TLAS"]        = 15;
    BindigMap["g_ColorBuffer"] = 7;
    BindigMap["g_Tex"]         = 101;
    BindigMap["g_TexSampler"]  = 0;
    BindigMap["cbConstants"]   = 9;
    BindigMap["g_AnotherRes"]  = 567;
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
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_Tex", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 101U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("g_TexSampler", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 0U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("cbConstants", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 9U);
        EXPECT_EQ(BindDesc.Space, 0U);
    }

    BindigMap["g_TLAS"]        = 0;
    BindigMap["g_ColorBuffer"] = 1;
    BindigMap["g_Tex"]         = 2;
    BindigMap["g_TexSampler"]  = 3;
    BindigMap["cbConstants"]   = 4;
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
        EXPECT_EQ(BindDesc.BindPoint, 3U);
        EXPECT_EQ(BindDesc.Space, 0U);

        EXPECT_HRESULT_SUCCEEDED(pReflection->GetResourceBindingDescByName("cbConstants", &BindDesc));
        EXPECT_EQ(BindDesc.BindPoint, 4U);
        EXPECT_EQ(BindDesc.Space, 0U);
    }
}

} // namespace
