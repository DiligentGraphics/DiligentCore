/*
 *  Copyright 2025-2026 Diligent Graphics LLC
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

#include "ObjectBase.hpp"
#include "MemoryFileStream.hpp"
#include "ProxyDataBlob.hpp"
#include "ShaderSourceFactoryUtils.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

constexpr char g_PS[] = R"(
Texture2D    g_Texture;
SamplerState g_Texture_sampler;

float4 main(in float2 UV : TEXCOORD) : SV_Target
{
    return g_Texture.Sample(g_Texture_sampler, UV);
}
)";

TEST(ShaderCreationTest, FromSource)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();

    ShaderCreateInfo ShaderCI;
    ShaderCI.Desc           = {"ShaderCreationTest.FromSource", SHADER_TYPE_PIXEL, true};
    ShaderCI.Source         = g_PS;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    {
        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        EXPECT_NE(pShader, nullptr);
    }

    {
        std::string Source = g_PS;
        Source += "invalid syntax";

        ShaderCI.Source       = Source.c_str();
        ShaderCI.SourceLength = sizeof(g_PS) - 1;

        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        EXPECT_NE(pShader, nullptr);
    }
}

TEST(ShaderCreationTest, NestedLocalAndSystemIncludesFXC)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().IsD3DDevice())
        GTEST_SKIP() << "FXC is only supported by Direct3D devices";

    constexpr Char MainSource[] =
        "#include \"Nested/Types.hlsli\"\n"
        "float4 main() : SV_Target\n"
        "{\n"
        "    return GetLocalColor() + GetSystemColor() + GetFallbackColor() + GetLocalOnlyColor() + GetSystemOnlyColor();\n"
        "}\n";

    auto pShaderSourceFactory = CreateMemoryShaderSourceFactory(
        {
            {"Shaders/Main.hlsl", MainSource},
            {"Shaders/Nested/Types.hlsli",
             "#include \"Config.hlsli\"\n"
             "#include <Config.hlsli>\n"
             "#include \"Fallback.hlsli\"\n"
             "#include \"LocalOnly.hlsli\"\n"
             "#include <SystemOnly.hlsli>\n"
             "float4 GetLocalColor() { return float4(LOCAL_VALUE, 0.0, 0.0, 0.0); }\n"
             "float4 GetSystemColor() { return float4(0.0, SYSTEM_VALUE, 0.0, 0.0); }\n"
             "float4 GetFallbackColor() { return float4(0.0, 0.0, FALLBACK_VALUE, 0.0); }\n"
             "float4 GetLocalOnlyColor() { return LOCAL_ONLY_COLOR; }\n"
             "float4 GetSystemOnlyColor() { return SYSTEM_ONLY_COLOR; }\n"},
            {"Shaders/Nested/Config.hlsli",
             "#define LOCAL_VALUE 1.0\n"},
            {"Config.hlsli",
             "#define SYSTEM_VALUE 1.0\n"},
            {"Fallback.hlsli",
             "#define FALLBACK_VALUE 1.0\n"},
            {"Shaders/Nested/LocalOnly.hlsli",
             "#define LOCAL_ONLY_COLOR float4(0.0, 0.0, 0.0, 0.0)\n"},
            {"LocalOnly.hlsli",
             "#error The search path must not be used when the local source exists\n"},
            {"Shaders/Nested/SystemOnly.hlsli",
             "#error A system include must not be resolved relative to its includer\n"},
            {"SystemOnly.hlsli",
             "#define SYSTEM_ONLY_COLOR float4(0.0, 0.0, 0.0, 0.0)\n"},
        },
        false);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    ShaderCreateInfo ShaderCI;
    ShaderCI.Desc                       = {"ShaderCreationTest.NestedLocalAndSystemIncludesFXC", SHADER_TYPE_PIXEL};
    ShaderCI.FilePath                   = "Shaders/Main.hlsl";
    ShaderCI.EntryPoint                 = "main";
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler             = SHADER_COMPILER_FXC;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pShader;
    pDevice->CreateShader(ShaderCI, &pShader);
    EXPECT_NE(pShader, nullptr);
}

static std::vector<uint8_t> CompilePS(IRenderDevice* pDevice)
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.Desc           = {"ShaderCreationTest.FromSource", SHADER_TYPE_PIXEL, true};
    ShaderCI.Source         = g_PS;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    std::vector<uint8_t>   Bytecode;
    RefCntAutoPtr<IShader> pShader;
    pDevice->CreateShader(ShaderCI, &pShader);
    if (pShader)
    {
        const void* pBytecode = nullptr;
        Uint64      Size      = 0;
        pShader->GetBytecode(&pBytecode, Size);

        Bytecode.resize(static_cast<size_t>(Size));
        memcpy(Bytecode.data(), pBytecode, static_cast<size_t>(Size));
    }

    return Bytecode;
}

TEST(ShaderCreationTest, FromBytecode)
{
    GPUTestingEnvironment*  pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*          pDevice    = pEnv->GetDevice();
    const RenderDeviceInfo& DeviceInfo = pDevice->GetDeviceInfo();

    if (!(DeviceInfo.IsD3DDevice() || DeviceInfo.IsVulkanDevice() || DeviceInfo.IsMetalDevice()))
    {
        GTEST_SKIP() << "Creating shader from bytecode is not supported on this device type";
    }

    std::vector<uint8_t> Bytecode = CompilePS(pDevice);
    ASSERT_FALSE(Bytecode.empty()) << "Failed to compile shader";

    {
        ShaderCreateInfo ShaderCI;
        ShaderCI.Desc         = {"ShaderCreationTest.FromBytecode - Src", SHADER_TYPE_PIXEL, true};
        ShaderCI.ByteCode     = Bytecode.data();
        ShaderCI.ByteCodeSize = Bytecode.size();
        if (DeviceInfo.IsMetalDevice())
        {
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_MSL_VERBATIM;
        }

        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        EXPECT_NE(pShader, nullptr);
    }

    {
        class TestShaderSourceFactoryImpl : public ObjectBase<IShaderSourceInputStreamFactory>
        {
        public:
            static RefCntAutoPtr<IShaderSourceInputStreamFactory> Create(const std::vector<uint8_t>& Bytecode)
            {
                return RefCntAutoPtr<IShaderSourceInputStreamFactory>{MakeNewRCObj<TestShaderSourceFactoryImpl>()(Bytecode)};
            }

            TestShaderSourceFactoryImpl(IReferenceCounters* pRefCounters, const std::vector<uint8_t>& Bytecode) :
                ObjectBase<IShaderSourceInputStreamFactory>(pRefCounters),
                m_Bytecode{Bytecode}
            {
            }

            virtual Bool DILIGENT_CALL_TYPE CreateInputStream(const Char* Name, IFileStream** ppStream) override final
            {
                return CreateInputStream2(Name, CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, ppStream);
            }

            virtual Bool DILIGENT_CALL_TYPE CreateInputStream2(const Char*                             Name,
                                                               CREATE_SHADER_SOURCE_INPUT_STREAM_FLAGS Flags,
                                                               IFileStream**                           ppStream) override final
            {
                if (ppStream != nullptr)
                    *ppStream = nullptr;
                Bool SourceFound = strcmp(Name, "ps.bc") == 0;
                if (SourceFound && ppStream != nullptr)
                {
                    RefCntAutoPtr<IDataBlob>        pDataBlob = ProxyDataBlob::Create(m_Bytecode.data(), m_Bytecode.size());
                    RefCntAutoPtr<MemoryFileStream> pMemStream{MakeNewRCObj<MemoryFileStream>()(pDataBlob)};

                    pMemStream->QueryInterface(IID_FileStream, reinterpret_cast<IObject**>(ppStream));
                    SourceFound = *ppStream != nullptr;
                }
                return SourceFound;
            }

        private:
            const std::vector<uint8_t>& m_Bytecode;
        };

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory = TestShaderSourceFactoryImpl::Create(Bytecode);

        ShaderCreateInfo ShaderCI;
        ShaderCI.Desc                       = {"ShaderCreationTest.FromBytecode", SHADER_TYPE_PIXEL, true};
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        ShaderCI.FilePath                   = "ps.bc";
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_BYTECODE;

        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        EXPECT_NE(pShader, nullptr);
    }
}


void TestWGSLEntryPointRemapping(PSO_CREATE_FLAGS Flags)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().IsWebGPUDevice())
        GTEST_SKIP() << "WGSL entry point remapping requires WebGPU";

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    // Tint renames f32 when generating WGSL. Pipeline creation must use the
    // generated name for every shader stage, including shaders without resources.
    const SHADER_TYPE ShaderTypes[] = {SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL, SHADER_TYPE_COMPUTE};
    const char*       Sources[]     = {
        "@vertex fn f32() -> @builtin(position) vec4f { return vec4f(0.0, 0.0, 0.0, 1.0); }",
        "@fragment fn f32() -> @location(0) vec4f { return vec4f(1.0); }",
        "@compute @workgroup_size(1) fn f32() {}",
    };
    RefCntAutoPtr<IShader> Shaders[3];
    for (Uint32 i = 0; i < _countof(Shaders); ++i)
    {
        ShaderCreateInfo ShaderCI;
        ShaderCI.Desc           = {"WGSLEntryPointRemapping", ShaderTypes[i]};
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_WGSL;
        ShaderCI.Source         = Sources[i];
        ShaderCI.EntryPoint     = "f32";
        pDevice->CreateShader(ShaderCI, &Shaders[i]);
        ASSERT_NE(Shaders[i], nullptr);
    }

    GraphicsPipelineStateCreateInfo GraphicsCI;
    GraphicsCI.PSODesc.Name                                  = "WGSLEntryPointRemapping - Graphics";
    GraphicsCI.GraphicsPipeline.NumRenderTargets             = 1;
    GraphicsCI.GraphicsPipeline.RTVFormats[0]                = pEnv->GetSwapChain()->GetDesc().ColorBufferFormat;
    GraphicsCI.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsCI.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    GraphicsCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    GraphicsCI.Flags                                         = Flags;
    GraphicsCI.pVS                                           = Shaders[0];
    GraphicsCI.pPS                                           = Shaders[1];

    RefCntAutoPtr<IPipelineState> pGraphicsPSO;
    pDevice->CreateGraphicsPipelineState(GraphicsCI, &pGraphicsPSO);
    ASSERT_NE(pGraphicsPSO, nullptr);
    EXPECT_EQ(pGraphicsPSO->GetStatus(true), PIPELINE_STATE_STATUS_READY);

    ComputePipelineStateCreateInfo ComputeCI;
    ComputeCI.PSODesc.Name         = "WGSLEntryPointRemapping - Compute";
    ComputeCI.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
    ComputeCI.Flags                = Flags;
    ComputeCI.pCS                  = Shaders[2];

    RefCntAutoPtr<IPipelineState> pComputePSO;
    pDevice->CreateComputePipelineState(ComputeCI, &pComputePSO);
    ASSERT_NE(pComputePSO, nullptr);
    EXPECT_EQ(pComputePSO->GetStatus(true), PIPELINE_STATE_STATUS_READY);
}

TEST(ShaderCreationTest, WGSLEntryPointRemapping)
{
    TestWGSLEntryPointRemapping(PSO_CREATE_FLAG_NONE);
}

TEST(ShaderCreationTest, WGSLEntryPointRemapping_Async)
{
#if PLATFORM_WEB
    GTEST_SKIP() << "The synchronous test harness cannot wait for browser asynchronous pipelines";
#else
    TestWGSLEntryPointRemapping(PSO_CREATE_FLAG_ASYNCHRONOUS);
#endif
}

} // namespace
