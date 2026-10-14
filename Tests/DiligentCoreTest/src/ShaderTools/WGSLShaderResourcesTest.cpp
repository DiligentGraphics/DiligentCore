/*
 *  Copyright 2024-2026 Diligent Graphics LLC
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
#include "WGSLShaderReflection.hpp"
#include "SPIRVShaderResources.hpp"
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

std::string HLSLtoWGLS(const char* FilePath, bool LoadUniformBufferReflection = false, std::vector<uint32_t>* pSPIRV = nullptr)
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

    std::string WGSL = ConvertSPIRVtoWGSL(SPIRV, LoadUniformBufferReflection);
    if (pSPIRV != nullptr)
        *pSPIRV = std::move(SPIRV);
    return WGSL;
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

TEST(WGSLShaderResources, TextureAccessThroughFunctions)
{
    // Identical HLSL resource types must retain distinct access modes after conversion.
    // The source compiler may inline the helpers before Tint processes the SPIR-V.
    TestWGSLResources("TextureAccessThroughFunctions.psh",
                      {
                          {"ReadTexture", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"WriteTexture", WGSLResourceType::WOStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA32_FLOAT, WGSLSampleType::Float},
                          {"ReadWriteTexture", WGSLResourceType::RWStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA32_FLOAT, WGSLSampleType::Float},
                      });
}

TEST(WGSLShaderResources, TextureAccessQueries)
{
    // Dimension queries must not turn a write-only texture into a read/write texture
    // or narrow the access of a texture used only for queries.
    TestWGSLResources("TextureAccessQueries.psh",
                      {
                          {"ReadTexture", WGSLResourceType::Texture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
                          {"WriteTexture", WGSLResourceType::WOStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA32_FLOAT, WGSLSampleType::Float},
                          {"QueryTexture", WGSLResourceType::RWStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_RGBA32_FLOAT, WGSLSampleType::Float},
                      });
}

TEST(WGSLShaderResources, PreservesSourceReflection)
{
    std::vector<uint32_t> SPIRV;
    const std::string     WGSL = HLSLtoWGLS("SourceReflection.psh", true, &SPIRV);
    ASSERT_FALSE(WGSL.empty());

    SPIRVShaderResources::CreateInfo ResCI;
    ResCI.ShaderType                  = SHADER_TYPE_PIXEL;
    ResCI.Name                        = "Source reflection test";
    ResCI.LoadUniformBufferReflection = true;
    const SPIRVShaderResources SourceResources{GetRawAllocator(), SPIRV, ResCI};
    ASSERT_EQ(SourceResources.GetNumUBs(), 2u);

    const WGSLResourceMapping Mapping = {
        {"fn", {2, 5}},
        {"fn_1", {3, 7}},
        {"var", {1, 9}},
        {"var_1", {1, 10}},
    };
    auto CheckReflection = [&](const std::string& Text, bool Remapped) {
        // Reload the emitted text as WGSL, as GetBytecode and shader archives do.
        const WGSLShaderResources Resources{GetRawAllocator(), Text, SHADER_SOURCE_LANGUAGE_WGSL,
                                            "Source reflection test", nullptr, nullptr, nullptr, true, nullptr};
        ASSERT_EQ(Resources.GetTotalResources(), Mapping.size());
        ASSERT_EQ(Resources.GetNumUBs(), SourceResources.GetNumUBs());
        for (Uint32 i = 0; i < Resources.GetTotalResources(); ++i)
        {
            const auto& Resource = Resources.GetResource(i);
            auto        It       = Mapping.find(Resource.Name);
            ASSERT_NE(It, Mapping.end()) << Resource.Name;
            if (Remapped)
            {
                EXPECT_EQ(Resource.BindGroup, It->second.Group);
                EXPECT_EQ(Resource.BindIndex, It->second.Index);
            }
            if (i < Resources.GetNumUBs())
            {
                bool Found = false;
                for (Uint32 j = 0; j < SourceResources.GetNumUBs(); ++j)
                {
                    if (strcmp(Resource.Name, SourceResources.GetUB(j).Name) == 0)
                    {
                        Found = true;
                        EXPECT_EQ(*Resources.GetUniformBufferDesc(i), *SourceResources.GetUniformBufferDesc(j)) << Resource.Name;
                    }
                }
                EXPECT_TRUE(Found) << Resource.Name;
            }
        }
    };
    CheckReflection(WGSL, false);
    const std::string Remapped = RemapWGSLResourceBindings(WGSL, Mapping, nullptr);
    ASSERT_FALSE(Remapped.empty());
    CheckReflection(Remapped, true);
    // A second rewrite must preserve the same names and layouts as the first.
    const std::string RemappedAgain = RemapWGSLResourceBindings(Remapped, Mapping, nullptr);
    ASSERT_FALSE(RemappedAgain.empty());
    CheckReflection(RemappedAgain, true);
}

TEST(WGSLShaderResources, ReflectionMetadataRoundTrip)
{
    const std::string WGSL = HLSLtoWGLS("SourceReflection.psh", true);
    ASSERT_FALSE(WGSL.empty());
    WGSLShaderReflection Reflection;
    ASSERT_TRUE(ParseWGSLShaderReflection(WGSL, Reflection));
    ASSERT_EQ(Reflection.Resources.size(), 4u);

    const size_t Start = WGSL.find("/*$DILIGENT_WGSL_REFLECTION=");
    ASSERT_NE(Start, std::string::npos);
    std::string Metadata;
    AppendWGSLShaderReflection(Metadata, Reflection);
    EXPECT_EQ(Metadata, "\n" + WGSL.substr(Start));
}

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

TEST(WGSLShaderResources, SpecializationConstants)
{
    // WGSL source with override (specialization) constants of various types.
    static constexpr char WGSL[] = R"(
        override sc_float: f32 = 1.0;
        override sc_int: i32 = 0;
        override sc_uint: u32 = 0;
        override sc_bool: bool = false;

        @group(0) @binding(0) var<storage, read_write> output: array<f32>;

        @compute @workgroup_size(1)
        fn main() {
            output[0] = sc_float;
            output[1] = f32(sc_int);
            output[2] = f32(sc_uint);
            if sc_bool {
                output[3] = 1.0;
            }
        }
    )";

    WGSLShaderResources Resources{
        GetRawAllocator(),
        WGSL,
        SHADER_SOURCE_LANGUAGE_WGSL,
        "SpecConst test",
        nullptr, // CombinedSamplerSuffix
        "main",  // EntryPoint
        nullptr, // ArrayIndexSuffix
        false,   // LoadUniformBufferReflection
        nullptr  // ppTintOutput
    };
    LOG_INFO_MESSAGE("WGSL Resources:\n", Resources.DumpResources());

    // One storage buffer resource
    EXPECT_EQ(Resources.GetTotalResources(), 1u);
    EXPECT_EQ(Resources.GetNumSBs(), 1u);

    const std::vector<WGSLSpecializationConstantAttribs> RefSpecConstants = {
        {"sc_float", 0, SHADER_CODE_BASIC_TYPE_FLOAT, false},
        {"sc_int", 0, SHADER_CODE_BASIC_TYPE_INT, false},
        {"sc_uint", 0, SHADER_CODE_BASIC_TYPE_UINT, false},
        {"sc_bool", 0, SHADER_CODE_BASIC_TYPE_BOOL, false},
    };

    const WGSLShaderResources& ConstResources = Resources;
    EXPECT_EQ(ConstResources.GetNumSpecConstants(), static_cast<Uint32>(RefSpecConstants.size()));

    // Build a map from name to reference for order-independent matching
    std::unordered_map<std::string, const WGSLSpecializationConstantAttribs*> RefMap;
    for (const WGSLSpecializationConstantAttribs& Ref : RefSpecConstants)
        RefMap[Ref.Name] = &Ref;

    for (Uint32 i = 0; i < ConstResources.GetNumSpecConstants(); ++i)
    {
        const WGSLSpecializationConstantAttribs& SC = ConstResources.GetSpecConstant(i);
        const auto                               it = RefMap.find(SC.Name);
        ASSERT_NE(it, RefMap.end()) << "Specialization constant '" << SC.Name << "' is not found in the reference list";

        const WGSLSpecializationConstantAttribs* pRef = it->second;
        EXPECT_EQ(SC.GetType(), pRef->GetType()) << SC.Name;
        EXPECT_EQ(SC.IsIdSpecified, pRef->IsIdSpecified) << SC.Name;
    }
}

} // namespace
