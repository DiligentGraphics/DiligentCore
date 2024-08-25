/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "WGSLShaderResources.hpp"
#include "WGSLUtils.hpp"
#include "GLSLangUtils.hpp"
#include "DefaultShaderSourceStreamFactory.h"
#include "RefCntAutoPtr.hpp"
#include "EngineMemory.h"

#include <unordered_map>

#include "TestingEnvironment.hpp"
#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

std::string HLSLtoWGLS(const char* FilePath)
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.FilePath       = FilePath;
    ShaderCI.Desc           = {"WGSL test shader", SHADER_TYPE_PIXEL};
    ShaderCI.EntryPoint     = "main";

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceStreamFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/WGSL", &pShaderSourceStreamFactory);
    if (!pShaderSourceStreamFactory)
        return {};

    ShaderCI.pShaderSourceStreamFactory = pShaderSourceStreamFactory;

    GLSLangUtils::InitializeGlslang();
    auto SPIRV = GLSLangUtils::HLSLtoSPIRV(ShaderCI, GLSLangUtils::SpirvVersion::Vk100, nullptr, nullptr);
    GLSLangUtils::FinalizeGlslang();

    if (SPIRV.empty())
        return {};

    return ConvertSPIRVtoWGSL(SPIRV);
}

void TestWGSLResources(const char*                                   FilePath,
                       const std::vector<WGSLShaderResourceAttribs>& RefResources)
{
    const auto WGSL = HLSLtoWGLS(FilePath);
    ASSERT_FALSE(WGSL.empty());

    WGSLShaderResources Resources{
        GetRawAllocator(),
        WGSL,
        SHADER_SOURCE_LANGUAGE_HLSL,
        "WGSLResources test",
        nullptr, // CombinedSamplerSuffix
        nullptr, // EntryPoint
        "_",     // ArrayIndexSuffix
        false,   // LoadUniformBufferReflection
        nullptr  // ppTintOutput
    };
    LOG_INFO_MESSAGE("WGSL Resources:\n", Resources.DumpResources());

    EXPECT_EQ(size_t{Resources.GetTotalResources()}, RefResources.size());

    std::unordered_map<std::string, const WGSLShaderResourceAttribs*> RefResourcesMap;
    for (const auto& RefRes : RefResources)
    {
        RefResourcesMap[RefRes.Name] = &RefRes;
    }

    for (Uint32 i = 0; i < Resources.GetTotalResources(); ++i)
    {
        const auto& Res     = const_cast<const WGSLShaderResources&>(Resources).GetResource(i);
        const auto* pRefRes = RefResourcesMap[Res.Name];
        ASSERT_NE(pRefRes, nullptr) << "Resource '" << Res.Name << "' is not found in the reference list";

        EXPECT_EQ(Res.ArraySize, pRefRes->ArraySize) << Res.Name;
        EXPECT_EQ(Res.Type, pRefRes->Type) << Res.Name;
        EXPECT_EQ(Res.ArraySize, pRefRes->ArraySize) << Res.Name;
        EXPECT_EQ(Res.ResourceDim, pRefRes->ResourceDim) << Res.Name;
        EXPECT_EQ(Res.Format, pRefRes->Format) << Res.Name;
        EXPECT_EQ(Res.SampleType, pRefRes->SampleType) << Res.Name;
    }
}

using WGSLResourceType = WGSLShaderResourceAttribs::ResourceType;
using WGSLSampleType   = WGSLShaderResourceAttribs::TextureSampleType;
TEST(WGSLShaderResources, UniformBuffers)
{
    TestWGSLResources("UniformBuffers.psh",
                      {
                          {"CB0", WGSLResourceType::UniformBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"CB1", WGSLResourceType::UniformBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"CB2", WGSLResourceType::UniformBuffer, 1, RESOURCE_DIM_BUFFER},
                      });
}

TEST(WGSLShaderResources, Textures)
{
    TestWGSLResources("Textures.psh",
                      {
                          // clang-format off
                          {"g_Tex1D",      WGSLResourceType::Texture,      1, RESOURCE_DIM_TEX_1D,         TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_Tex2D",      WGSLResourceType::Texture,      1, RESOURCE_DIM_TEX_2D,         TEX_FORMAT_UNKNOWN, WGSLSampleType::UInt},
                          {"g_Tex2DArr",   WGSLResourceType::Texture,      1, RESOURCE_DIM_TEX_2D_ARRAY,   TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_TexCube",    WGSLResourceType::Texture,      1, RESOURCE_DIM_TEX_CUBE,       TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_TexCubeArr", WGSLResourceType::Texture,      1, RESOURCE_DIM_TEX_CUBE_ARRAY, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_Tex3D",      WGSLResourceType::Texture,      1, RESOURCE_DIM_TEX_3D,         TEX_FORMAT_UNKNOWN, WGSLSampleType::SInt},
                          {"g_Tex2DMS",    WGSLResourceType::TextureMS,    1, RESOURCE_DIM_TEX_2D,         TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_Tex2DDepth", WGSLResourceType::DepthTexture, 1, RESOURCE_DIM_TEX_2D,         TEX_FORMAT_UNKNOWN, WGSLSampleType::Depth},
                          {"g_Sampler",    WGSLResourceType::Sampler},
                          {"g_SamplerCmp", WGSLResourceType::ComparisonSampler},
                          // clang-format on
                      });
}

TEST(WGSLShaderResources, RWTextures)
{
    TestWGSLResources("RWTextures.psh",
                      {
                          // clang-format off
                          {"g_WOTex1D",    WGSLResourceType::WOStorageTexture, 1, RESOURCE_DIM_TEX_1D, TEX_FORMAT_RGBA32_FLOAT,      WGSLSampleType::Float},
                          {"g_WOTex2D",    WGSLResourceType::WOStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA32_SINT,       WGSLSampleType::SInt},
                          {"g_WOTex2DArr", WGSLResourceType::WOStorageTexture, 1, RESOURCE_DIM_TEX_2D_ARRAY, TEX_FORMAT_RGBA32_UINT, WGSLSampleType::UInt},
                          {"g_WOTex3D",    WGSLResourceType::WOStorageTexture, 1, RESOURCE_DIM_TEX_3D, TEX_FORMAT_RGBA32_FLOAT,      WGSLSampleType::Float},

                          {"g_ROTex1D",    WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_1D, TEX_FORMAT_UNKNOWN,       WGSLSampleType::SInt},
                          {"g_ROTex2D",    WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN,       WGSLSampleType::Float},
                          {"g_ROTex2DArr", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D_ARRAY, TEX_FORMAT_UNKNOWN, WGSLSampleType::UInt},
                          {"g_ROTex3D",    WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_3D, TEX_FORMAT_UNKNOWN,       WGSLSampleType::Float},

                          {"g_RWTex1D",    WGSLResourceType::RWStorageTexture, 1, RESOURCE_DIM_TEX_1D, TEX_FORMAT_R32_SINT,       WGSLSampleType::SInt},
                          {"g_RWTex2D",    WGSLResourceType::RWStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_R32_FLOAT,      WGSLSampleType::Float},
                          {"g_RWTex2DArr", WGSLResourceType::RWStorageTexture, 1, RESOURCE_DIM_TEX_2D_ARRAY, TEX_FORMAT_R32_UINT, WGSLSampleType::UInt},
                          {"g_RWTex3D",    WGSLResourceType::RWStorageTexture, 1, RESOURCE_DIM_TEX_3D, TEX_FORMAT_R32_FLOAT,      WGSLSampleType::Float},
                          // clang-format on
                      });
}

TEST(WGSLShaderResources, StructBuffers)
{
    TestWGSLResources("StructBuffers.psh",
                      {
                          {"g_Buff0", WGSLResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_Buff1", WGSLResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_Buff2", WGSLResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_Buff3", WGSLResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                      });
}

TEST(WGSLShaderResources, RWStructBuffers)
{
    TestWGSLResources("RWStructBuffers.psh",
                      {
                          {"g_RWBuff0", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff1", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff2", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff3", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuffAtomic0", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuffAtomic1", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff0_atomic", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff1_atomic", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff0Atomic_atomic", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff1Atomic_atomic", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff2Atomic", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff3Atomic", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff4Atomic_atomic", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_RWBuff5Atomic_atomic", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                      });
}

TEST(WGSLShaderResources, TextureArrays)
{
    TestWGSLResources("TextureArrays.psh",
                      {
                          {"g_Tex2DArr0", WGSLResourceType::Texture, 8, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::UInt},
                          {"g_Tex2DNotArr0_2", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::SInt},
                          {"g_Tex2DNotArr0_4", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_Tex2DNotArr1_1", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_Tex2DNotArr1_2", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_3D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_Tex2DNotArr2_3", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_Tex2DNotArr2_5", WGSLResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER},
                          {"g_Tex2DNotArr3_3x", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_Tex2DNotArr4_", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                      });
}

TEST(WGSLShaderResources, SamplerArrays)
{
    TestWGSLResources("SamplerArrays.psh",
                      {
                          {"g_Tex2D", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_SamplerArr0", WGSLResourceType::Sampler, 8},
                          {"g_SamplerNotArr1_5", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"g_SamplerNotArr1_3", WGSLResourceType::Sampler},

                      });
}

TEST(WGSLShaderResources, StructBufferArrays)
{
    TestWGSLResources("StructBufferArrays.psh",
                      {
                          {"g_BuffArr0", WGSLResourceType::ROStorageBuffer, 6, RESOURCE_DIM_BUFFER},
                          {"g_BuffArr1", WGSLResourceType::ROStorageBuffer, 3, RESOURCE_DIM_BUFFER},
                          {"g_BuffArr2", WGSLResourceType::ROStorageBuffer, 5, RESOURCE_DIM_BUFFER},
                      });
}

TEST(WGSLShaderResources, RWTextureArrays)
{
    TestWGSLResources("RWTextureArrays.psh",
                      {
                          // clang-format off
                          {"g_WOTex2DArr0", WGSLResourceType::WOStorageTexture, 4, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA32_FLOAT, WGSLSampleType::Float},
                          {"g_RWTex2DArr0", WGSLResourceType::RWStorageTexture, 3, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA32_FLOAT, WGSLSampleType::Float},
                          {"g_WOTex2DNotArr1_2", WGSLResourceType::WOStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA32_FLOAT, WGSLSampleType::Float},
                          {"g_WOTex2DNotArr1_4", WGSLResourceType::WOStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA32_SINT, WGSLSampleType::SInt},
                          {"g_RWTex2DNotArr2_5", WGSLResourceType::RWStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RG32_FLOAT, WGSLSampleType::Float},
                          {"g_RWTex2DNotArr2_9", WGSLResourceType::RWStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_R32_FLOAT, WGSLSampleType::Float},
                          // clang-format on
                      });
}

TEST(WGSLShaderResources, RWStructBufferArrays)
{
    TestWGSLResources("RWStructBufferArrays.psh",
                      {
                          {"g_RWBuffArr0", WGSLResourceType::RWStorageBuffer, 6, RESOURCE_DIM_BUFFER},
                          {"g_RWBuffArr1", WGSLResourceType::RWStorageBuffer, 3, RESOURCE_DIM_BUFFER},
                          {"g_RWBuffArr2", WGSLResourceType::RWStorageBuffer, 2, RESOURCE_DIM_BUFFER},
                      });
}

} // namespace
