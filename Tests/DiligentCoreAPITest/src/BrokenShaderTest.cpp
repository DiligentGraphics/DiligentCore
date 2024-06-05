/*
 *  Copyright 2019-2024 Diligent Graphics LLC
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

#include "GPUTestingEnvironment.hpp"

#include "gtest/gtest.h"

#include <thread>

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static const char g_BrokenHLSL[] = R"(
void VSMain(out float4 pos : SV_POSITION)
{
    pos = float3(0.0, 0.0, 0.0, 0.0);
}
)";

static const char g_BrokenGLSL[] = R"(
void VSMain()
{
    gl_Position = vec3(0.0, 0.0, 0.0);
}
)";

static const char g_BrokenMSL[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct VSOut
{
    float4 pos [[position]];
};

vertex VSOut VSMain()
{
    VSOut out = {};
    out.pos = float3(0.0, 0.0, 0.0);
    return out;
}
)";

void CreateBrokenShader(const char*            Source,
                        const char*            Name,
                        SHADER_SOURCE_LANGUAGE SourceLanguage,
                        SHADER_COMPILE_FLAGS   CompileFlags,
                        int                    ErrorAllowance,
                        IShader**              ppBrokenShader,
                        IDataBlob**            ppErrors)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    ShaderCreateInfo ShaderCI;
    ShaderCI.Source         = Source;
    ShaderCI.EntryPoint     = "VSMain";
    ShaderCI.Desc           = {Name, SHADER_TYPE_VERTEX, true};
    ShaderCI.SourceLanguage = SourceLanguage;
    ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
    ShaderCI.CompileFlags   = CompileFlags;

    ShaderMacro Macros[] = {{"TEST", "MACRO"}};
    ShaderCI.Macros      = {Macros, _countof(Macros)};

    pEnv->SetErrorAllowance(ErrorAllowance, "\n\nNo worries, testing broken shader...\n\n");
    pDevice->CreateShader(ShaderCI, ppBrokenShader, ppErrors);
}

void TestBrokenShader(const char*            Source,
                      const char*            Name,
                      SHADER_SOURCE_LANGUAGE SourceLanguage,
                      SHADER_COMPILE_FLAGS   CompileFlags,
                      int                    ErrorAllowance)
{
    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<IShader>   pBrokenShader;
    RefCntAutoPtr<IDataBlob> pErrors;
    CreateBrokenShader(Source, Name, SourceLanguage, CompileFlags, ErrorAllowance, &pBrokenShader, pErrors.RawDblPtr());
    if (CompileFlags & SHADER_COMPILE_FLAG_ASYNCHRONOUS)
    {
        ASSERT_NE(pBrokenShader, nullptr);
        Uint32 Iter = 0;
        while (pBrokenShader->GetStatus() == SHADER_STATUS_COMPILING)
        {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            ++Iter;
        }
        LOG_INFO_MESSAGE("Shader '", Name, "' was compiled in ", Iter, " iterations");
        EXPECT_EQ(pBrokenShader->GetStatus(), SHADER_STATUS_FAILED);
    }
    else
    {
        EXPECT_FALSE(pBrokenShader);
        ASSERT_NE(pErrors, nullptr);
    }
    ASSERT_NE(pErrors, nullptr);
    const char* Msg = reinterpret_cast<const char*>(pErrors->GetDataPtr());
    LOG_INFO_MESSAGE("Compiler output:\n", Msg);
}

TEST(Shader, BrokenHLSL)
{
    const auto& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    // HLSL is supported in all backends
    TestBrokenShader(g_BrokenHLSL, "Broken HLSL test", SHADER_SOURCE_LANGUAGE_HLSL, SHADER_COMPILE_FLAG_NONE,
                     DeviceInfo.IsGLDevice() || DeviceInfo.IsD3DDevice() ? 2 : 3);
}

TEST(Shader, BrokenHLSL_Async)
{
    const auto& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (!DeviceInfo.Features.AsyncShaderCompilation)
    {
        GTEST_SKIP() << "Asynchronous shader compilation is not supported by this device";
    }

    // HLSL is supported in all backends
    TestBrokenShader(g_BrokenHLSL, "Broken HLSL test", SHADER_SOURCE_LANGUAGE_HLSL, SHADER_COMPILE_FLAG_ASYNCHRONOUS,
                     DeviceInfo.IsGLDevice() || DeviceInfo.IsD3DDevice() ? 2 : 3);
}

TEST(Shader, BrokenGLSL)
{
    const auto& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (DeviceInfo.IsD3DDevice() || DeviceInfo.IsWebGPUDevice())
    {
        GTEST_SKIP() << "GLSL is not supported in Direct3D and WebGPU";
    }

    TestBrokenShader(g_BrokenGLSL, "Broken GLSL test", SHADER_SOURCE_LANGUAGE_GLSL, SHADER_COMPILE_FLAG_NONE,
                     DeviceInfo.IsGLDevice() ? 2 : 3);
}

TEST(Shader, BrokenGLSL_Async)
{
    const auto& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (DeviceInfo.IsD3DDevice() || DeviceInfo.IsWebGPUDevice())
    {
        GTEST_SKIP() << "GLSL is not supported in Direct3D and WebGPU";
    }
    if (!DeviceInfo.Features.AsyncShaderCompilation)
    {
        GTEST_SKIP() << "Asynchronous shader compilation is not supported by this device";
    }

    TestBrokenShader(g_BrokenGLSL, "Broken GLSL test", SHADER_SOURCE_LANGUAGE_GLSL, SHADER_COMPILE_FLAG_ASYNCHRONOUS,
                     DeviceInfo.IsGLDevice() ? 2 : 3);
}

TEST(Shader, BrokenMSL)
{
    const auto& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (!DeviceInfo.IsMetalDevice())
    {
        GTEST_SKIP() << "MSL is only supported in Metal";
    }

    TestBrokenShader(g_BrokenMSL, "Broken MSL test", SHADER_SOURCE_LANGUAGE_MSL, SHADER_COMPILE_FLAG_NONE, 2);
}

TEST(Shader, BrokenMSL_Async)
{
    const auto& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (!DeviceInfo.IsMetalDevice())
    {
        GTEST_SKIP() << "MSL is only supported in Metal";
    }
    if (!DeviceInfo.Features.AsyncShaderCompilation)
    {
        GTEST_SKIP() << "Asynchronous shader compilation is not supported by this device";
    }

    TestBrokenShader(g_BrokenMSL, "Broken MSL test", SHADER_SOURCE_LANGUAGE_MSL, SHADER_COMPILE_FLAG_ASYNCHRONOUS, 2);
}

TEST(Shader, AsyncPipelineWithBrokenShader)
{
    auto*       pEnv       = GPUTestingEnvironment::GetInstance();
    auto*       pDevice    = pEnv->GetDevice();
    const auto& DeviceInfo = pDevice->GetDeviceInfo();
    if (!DeviceInfo.Features.AsyncShaderCompilation)
    {
        GTEST_SKIP() << "Asynchronous shader compilation is not supported by this device";
    }

    RefCntAutoPtr<IShader>   pBrokenShader;
    RefCntAutoPtr<IDataBlob> pErrors;
    CreateBrokenShader(g_BrokenHLSL, "Broken HLSL test", SHADER_SOURCE_LANGUAGE_HLSL, SHADER_COMPILE_FLAG_ASYNCHRONOUS,
                       DeviceInfo.IsGLDevice() || DeviceInfo.IsD3DDevice() ? 2 : 3, &pBrokenShader, pErrors.RawDblPtr());
    ASSERT_NE(pBrokenShader, nullptr);

    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.pVS   = pBrokenShader;
    PSOCreateInfo.Flags = PSO_CREATE_FLAG_ASYNCHRONOUS;

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreatePipelineState(PSOCreateInfo, &pPSO);
    ASSERT_NE(pPSO, nullptr);

    PIPELINE_STATE_STATUS Status = pPSO->GetStatus(true);
    EXPECT_EQ(Status, PIPELINE_STATE_STATUS_FAILED);
}

} // namespace
