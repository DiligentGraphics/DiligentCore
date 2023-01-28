/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static const char g_TestShaderSource[] = R"(

Texture2D    g_Tex1;
SamplerState g_Tex1_sampler;

Texture2D    g_Tex2;
SamplerState g_Tex2_sampler;

Buffer<float4> g_Buffer;

struct Struct1
{
    float4 f4;
    bool4  b4;
};

StructuredBuffer<Struct1> g_StructBuff;

struct Struct2
{
    uint4   u4;
    Struct1 s1;
};

struct Struct3
{
    Struct1 s1[2];
    int4    i4;
    Struct2 s2;
};

cbuffer CBuffer1
{
    float f;
    uint  u;
    int   i;
    bool  b;

    float4 f4;

    float4x4 f4x4;
    uint4x4  u4x4;

    Struct1 s1;

    float4 af4[2];
    int4x4 ai4x4[4];
}

cbuffer CBuffer2
{
    uint4    u4;
    int4     i4;
    bool4    b4;
    Struct2  s2;
    int4x4   i4x4;
    Struct3  s3;
    bool4x4  b4x4;
}

void main(out float4 pos : SV_POSITION)
{
    pos = f4;
    pos += s1.f4;
    pos += s2.s1.f4;
    pos += s3.s1[1].f4;
    pos += s3.s2.s1.f4;
    pos += g_Tex1.SampleLevel(g_Tex1_sampler, float2(0.5, 0.5), 0.0);
    pos += g_Tex2.SampleLevel(g_Tex2_sampler, float2(0.5, 0.5), 0.0);
    pos += g_Buffer.Load(0);
    pos += g_StructBuff[0].f4;
}
)";

void CheckConstantBufferReflection(IShader* pShader, bool PrintBufferContents = false)
{
    static constexpr ShaderCodeVariableDesc Struct1[] =
        {
            {"f4", "float4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_FLOAT, 1, 4, 0},
            {"b4", "bool4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_BOOL, 1, 4, 16},
        };

    static constexpr ShaderCodeVariableDesc Struct2[] =
        {
            {"u4", "uint4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_UINT, 1, 4, 0},
            {"s1", "Struct1", _countof(Struct1), Struct1, 16},
        };

    static constexpr ShaderCodeVariableDesc Struct3[] =
        {
            {"s1", "Struct1", _countof(Struct1), Struct1, 0, 2},
            {"i4", "int4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_INT, 1, 4, 64},
            {"s2", "Struct2", _countof(Struct2), Struct2, 80},
        };

    static constexpr ShaderCodeVariableDesc CBuffer1Vars[] =
        {
            {"f", "float", SHADER_CODE_BASIC_TYPE_FLOAT, 0},
            {"u", "uint", SHADER_CODE_BASIC_TYPE_UINT, 4},
            {"i", "int", SHADER_CODE_BASIC_TYPE_INT, 8},
            {"b", "bool", SHADER_CODE_BASIC_TYPE_BOOL, 12},

            {"f4", "float4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_FLOAT, 1, 4, 16},
            {"f4x4", "float4x4", SHADER_CODE_VARIABLE_CLASS_MATRIX_COLUMNS, SHADER_CODE_BASIC_TYPE_FLOAT, 4, 4, 32},
            {"u4x4", "uint4x4", SHADER_CODE_VARIABLE_CLASS_MATRIX_COLUMNS, SHADER_CODE_BASIC_TYPE_UINT, 4, 4, 96},

            {"s1", "Struct1", _countof(Struct1), Struct1, 160},

            {"af4", "float4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_FLOAT, 1, 4, 192, 2},
            {"ai4x4", "int4x4", SHADER_CODE_VARIABLE_CLASS_MATRIX_COLUMNS, SHADER_CODE_BASIC_TYPE_INT, 4, 4, 224, 4},
        };

    static constexpr ShaderCodeBufferDesc CBuffer1{480, _countof(CBuffer1Vars), CBuffer1Vars};

    static constexpr ShaderCodeVariableDesc CBuffer2Vars[] =
        {
            {"u4", "uint4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_UINT, 1, 4, 0},
            {"i4", "int4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_INT, 1, 4, 16},
            {"b4", "bool4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_BOOL, 1, 4, 32},
            {"s2", "Struct2", _countof(Struct2), Struct2, 48},
            {"i4x4", "int4x4", SHADER_CODE_VARIABLE_CLASS_MATRIX_COLUMNS, SHADER_CODE_BASIC_TYPE_INT, 4, 4, 96},
            {"s3", "Struct3", _countof(Struct3), Struct3, 160},
            {"b4x4", "bool4x4", SHADER_CODE_VARIABLE_CLASS_MATRIX_COLUMNS, SHADER_CODE_BASIC_TYPE_BOOL, 4, 4, 288},
        };

    static constexpr ShaderCodeBufferDesc CBuffer2{352, _countof(CBuffer2Vars), CBuffer2Vars};

    auto ResCount = pShader->GetResourceCount();
    for (Uint32 i = 0; i < ResCount; ++i)
    {
        ShaderResourceDesc ResDesc;
        pShader->GetResourceDesc(i, ResDesc);
        if (ResDesc.Type != SHADER_RESOURCE_TYPE_CONSTANT_BUFFER)
            continue;

        if (strcmp(ResDesc.Name, "CBuffer1") == 0)
        {
            const auto* pBuffDesc = pShader->GetConstantBufferDesc(i);
            EXPECT_NE(pBuffDesc, nullptr);
            if (pBuffDesc != nullptr)
            {
                EXPECT_EQ(*pBuffDesc, CBuffer1);
                if (PrintBufferContents)
                {
                    std::cout << std::endl
                              << "CBuffer1" << std::endl
                              << GetShaderCodeBufferDescString(*pBuffDesc, 4);
                }
            }
        }
        else if (strcmp(ResDesc.Name, "CBuffer2") == 0)
        {
            const auto* pBuffDesc = pShader->GetConstantBufferDesc(i);
            EXPECT_NE(pBuffDesc, nullptr);
            if (pBuffDesc != nullptr)
            {
                EXPECT_EQ(*pBuffDesc, CBuffer2);
                if (PrintBufferContents)
                {
                    std::cout << std::endl
                              << "CBuffer2" << std::endl
                              << GetShaderCodeBufferDescString(*pBuffDesc, 4);
                }
            }
        }
        else
        {
            GTEST_FAIL() << "Unexpected constant buffer " << ResDesc.Name;
        }
    }
}

void TestShaderReflection(SHADER_COMPILER Compiler, bool PrintBufferContents = false)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage               = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler               = Compiler;
    ShaderCI.Desc                         = {"Constant buffer reflection test", SHADER_TYPE_VERTEX, true};
    ShaderCI.EntryPoint                   = "main";
    ShaderCI.Source                       = g_TestShaderSource;
    ShaderCI.LoadConstantBufferReflection = true;

    RefCntAutoPtr<IShader> pShader;
    pDevice->CreateShader(ShaderCI, &pShader);
    ASSERT_NE(pShader, nullptr);

    CheckConstantBufferReflection(pShader, PrintBufferContents);

    // Create shader from byte code
    Uint64 ByteCodeSize = 0;
    pShader->GetBytecode(&ShaderCI.ByteCode, ByteCodeSize);
    ShaderCI.ByteCodeSize = static_cast<size_t>(ByteCodeSize);
    ShaderCI.Source       = nullptr;

    RefCntAutoPtr<IShader> pShader2;
    pDevice->CreateShader(ShaderCI, &pShader2);
    ASSERT_NE(pShader2, nullptr);

    pShader.Release();
    CheckConstantBufferReflection(pShader2);
}

TEST(ConstantBufferReflectionTest, HLSL)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().IsD3DDevice())
        GTEST_SKIP();

    constexpr auto PrintBufferContents = true;
    TestShaderReflection(SHADER_COMPILER_DEFAULT, PrintBufferContents);
}

TEST(ConstantBufferReflectionTest, HLSL_DXC)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D12)
        GTEST_SKIP();

    TestShaderReflection(SHADER_COMPILER_DXC);
}

} // namespace
