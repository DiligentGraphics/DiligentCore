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

#include <stdexcept>
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


TEST(WGSLShaderResources, NativeUniformBufferLayout)
{
    const std::string WGSL = R"(
struct Nested { uv: vec2u, signed_value: i32 }
struct Constants {
    scalar: f32,
    vector: vec3f,
    tail: u32,
    transform: mat2x4f,
    values: array<vec4i, 2>,
    nested: Nested,
}
@group(2) @binding(3) var<uniform> constants: Constants;
@fragment fn main() -> @location(0) vec4f {
    return constants.transform[0] + vec4f(constants.scalar + constants.vector.x +
        f32(constants.tail + constants.nested.uv.x) +
        f32(constants.values[1].x + constants.nested.signed_value));
}
)";
    // Independently specified offsets exercise WGSL alignment, nested members,
    // arrays, and matrix orientation without source-language metadata.
    const ShaderCodeVariableDesc NestedMembers[] = {
        {"uv", "vec2<u32>", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_UINT, 2, 1, 0},
        {"signed_value", "i32", SHADER_CODE_BASIC_TYPE_INT, 8},
    };
    const ShaderCodeVariableDesc Variables[] = {
        {"scalar", "f32", SHADER_CODE_BASIC_TYPE_FLOAT, 0},
        {"vector", "vec3<f32>", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_FLOAT, 3, 1, 16},
        {"tail", "u32", SHADER_CODE_BASIC_TYPE_UINT, 28},
        {"transform", "mat2x4<f32>", SHADER_CODE_VARIABLE_CLASS_MATRIX_ROWS, SHADER_CODE_BASIC_TYPE_FLOAT, 4, 2, 32},
        {"values", "vec4<i32>", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_INT, 4, 1, 64, 2},
        {"nested", "Nested", 2, NestedMembers, 96},
    };
    const ShaderCodeBufferDesc Expected{112, 6, Variables};
    for (bool LoadReflection : {false, true})
    {
        SCOPED_TRACE(LoadReflection);
        const WGSLShaderResources Resources{GetRawAllocator(), WGSL, SHADER_SOURCE_LANGUAGE_WGSL,
                                            "Native uniform layout", nullptr, nullptr, nullptr, LoadReflection, nullptr};
        ASSERT_EQ(Resources.GetTotalResources(), 1u);
        ASSERT_EQ(Resources.GetNumUBs(), 1u);
        EXPECT_STREQ(Resources.GetEntryPoint(), "main");
        EXPECT_EQ(Resources.GetShaderType(), SHADER_TYPE_PIXEL);
        EXPECT_STREQ(Resources.GetUB(0).Name, "constants");
        EXPECT_EQ(Resources.GetUB(0).BindGroup, 2u);
        EXPECT_EQ(Resources.GetUB(0).BindIndex, 3u);
        EXPECT_EQ(Resources.GetUB(0).BufferStaticSize, 112u);
        if (LoadReflection)
        {
            const auto* Buffer = Resources.GetUniformBufferDesc(0);
            ASSERT_NE(Buffer, nullptr);
            EXPECT_EQ(*Buffer, Expected);
        }
    }
}

TEST(WGSLShaderResources, NativeNonStructUniforms)
{
    const ShaderCodeVariableDesc ElementMembers[] = {
        {"xy", "vec2<f32>", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_FLOAT, 2, 1, 0},
        {"z", "u32", SHADER_CODE_BASIC_TYPE_UINT, 8},
    };
    const struct
    {
        const char*            Type;
        const char*            Expression;
        Uint32                 Size;
        ShaderCodeVariableDesc Variable;
    } Cases[] = {
        {"f32", "value", 4, {"value", "f32", SHADER_CODE_BASIC_TYPE_FLOAT, 0}},
        {"i32", "f32(value)", 4, {"value", "i32", SHADER_CODE_BASIC_TYPE_INT, 0}},
        {"u32", "f32(value)", 4, {"value", "u32", SHADER_CODE_BASIC_TYPE_UINT, 0}},
        {"f16", "f32(value)", 2, {"value", "f16", SHADER_CODE_BASIC_TYPE_FLOAT16, 0}},
        {"vec3f", "value.x", 12, {"value", "vec3<f32>", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_FLOAT, 3, 1, 0}},
        {"vec4h", "f32(value.x)", 8, {"value", "vec4<f16>", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_FLOAT16, 4, 1, 0}},
        {"mat2x4f", "value[1].x", 32, {"value", "mat2x4<f32>", SHADER_CODE_VARIABLE_CLASS_MATRIX_ROWS, SHADER_CODE_BASIC_TYPE_FLOAT, 4, 2, 0}},
        {"array<vec4i, 3>", "f32(value[2].x)", 48, {"value", "vec4<i32>", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_INT, 4, 1, 0, 3}},
        {"array<Element, 2>", "value[1].xy.x + f32(value[1].z)", 32, {"value", "Element", 2, ElementMembers, 0, 2}},
    };
    for (const auto& Test : Cases)
    {
        SCOPED_TRACE(Test.Type);
        const std::string Source = std::string{R"(
enable f16;
struct Element { xy: vec2f, z: u32 }
@group(2) @binding(3) var<uniform> value: )"} +
            Test.Type + R"(;
@fragment fn main() -> @location(0) vec4f { return vec4f()" +
            Test.Expression + "); }";

        for (bool HasMetadata : {false, true})
        {
            SCOPED_TRACE(HasMetadata);
            std::string WGSL = Source;
            if (HasMetadata)
            {
                // Names-only metadata must still load the native layout safely.
                WGSLShaderReflection Metadata;
                Metadata.Resources[{2, 3}].Name = "OriginalValue";
                AppendWGSLShaderReflection(WGSL, Metadata);
            }

            for (bool LoadReflection : {false, true})
            {
                SCOPED_TRACE(LoadReflection);
                const WGSLShaderResources Resources{GetRawAllocator(), WGSL, SHADER_SOURCE_LANGUAGE_WGSL,
                                                    "Non-struct uniform", nullptr, "main", nullptr, LoadReflection, nullptr};
                ASSERT_EQ(Resources.GetNumUBs(), 1u);
                EXPECT_EQ(Resources.GetTotalResources(), 1u);
                const char* Name = HasMetadata ? "OriginalValue" : "value";
                EXPECT_STREQ(Resources.GetUB(0).Name, Name);
                EXPECT_EQ(Resources.GetUB(0).BufferStaticSize, Test.Size);
                EXPECT_EQ(Resources.GetUB(0).BindGroup, 2u);
                EXPECT_EQ(Resources.GetUB(0).BindIndex, 3u);
                if (LoadReflection)
                {
                    const auto* Buffer = Resources.GetUniformBufferDesc(0);
                    ASSERT_NE(Buffer, nullptr);
                    ShaderCodeVariableDesc ExpectedVariable = Test.Variable;
                    ExpectedVariable.Name                   = Name;
                    const ShaderCodeBufferDesc Expected{Test.Size, 1, &ExpectedVariable};
                    EXPECT_EQ(*Buffer, Expected);
                }
            }
        }
    }
}

TEST(WGSLShaderResources, NativeHalfPrecisionUniforms)
{
    const std::string         WGSL = R"(
enable f16;
struct Constants { @align(16) value: vec4h }
@group(0) @binding(0) var<uniform> constants: Constants;
@fragment fn main() -> @location(0) vec4f { return vec4f(constants.value); }
)";
    const WGSLShaderResources Resources{GetRawAllocator(), WGSL, SHADER_SOURCE_LANGUAGE_WGSL,
                                        "Half precision uniforms", nullptr, "main", nullptr, true, nullptr};
    ASSERT_EQ(Resources.GetNumUBs(), 1u);
    const auto* Buffer = Resources.GetUniformBufferDesc(0);
    ASSERT_NE(Buffer, nullptr);
    EXPECT_EQ(Buffer->Size, 16u);
    ASSERT_EQ(Buffer->NumVariables, 1u);
    const ShaderCodeVariableDesc Expected{"value", "vec4<f16>", SHADER_CODE_VARIABLE_CLASS_VECTOR,
                                          SHADER_CODE_BASIC_TYPE_FLOAT16, 4, 1, 0};
    EXPECT_EQ(Buffer->pVariables[0], Expected);
}

TEST(WGSLShaderResources, PartialMetadataOverridesNamesAndLayout)
{
    std::string                  WGSL = R"(
struct Generated { generated_member: vec4f }
struct Native { native_member: vec4f }
@group(1) @binding(4) var<uniform> generated: Generated;
@group(2) @binding(4) var<uniform> native: Native;
@id(7) override generated_scale: f32 = 1.0;
@id(8) override native_scale: f32 = 2.0;
@id(9) override unused: f32 = 3.0;
@fragment fn main() -> @location(0) vec4f {
    return generated.generated_member * generated_scale + native.native_member * native_scale;
}
)";
    const ShaderCodeVariableDesc OriginalMember{"original_member", "float4", SHADER_CODE_VARIABLE_CLASS_VECTOR,
                                                SHADER_CODE_BASIC_TYPE_FLOAT, 1, 4, 0};
    const ShaderCodeBufferDesc   OriginalBuffer{16, 1, &OriginalMember};
    WGSLShaderReflection         Metadata;
    auto&                        Resource = Metadata.Resources[{1, 4}];
    Resource.Name                         = "OriginalConstants";
    Resource.UniformBuffer.reset(new ShaderCodeBufferDescX{});
    Resource.UniformBuffer->Size = 16;
    Resource.UniformBuffer->AddVariable(OriginalMember);
    Metadata.Resources[{3, 4}].Name     = "NotInShader";
    Metadata.SpecializationConstants[7] = "OriginalScale";
    Metadata.SpecializationConstants[9] = "UnusedScale";
    AppendWGSLShaderReflection(WGSL, Metadata);

    // A metadata record applies to the complete binding point. Missing records
    // fall back to WGSL reflection; unrelated records do not create resources.
    for (bool LoadReflection : {false, true})
    {
        SCOPED_TRACE(LoadReflection);
        const WGSLShaderResources Resources{GetRawAllocator(), WGSL, SHADER_SOURCE_LANGUAGE_WGSL,
                                            "Partial reflection metadata", nullptr, "main", nullptr, LoadReflection, nullptr};
        ASSERT_EQ(Resources.GetNumUBs(), 2u);
        EXPECT_EQ(Resources.GetTotalResources(), 2u);
        for (Uint32 i = 0; i < Resources.GetNumUBs(); ++i)
        {
            const auto& UB = Resources.GetUB(i);
            ASSERT_TRUE(UB.BindGroup == 1 || UB.BindGroup == 2);
            EXPECT_EQ(UB.BindIndex, 4u);
            EXPECT_STREQ(UB.Name, UB.BindGroup == 1 ? "OriginalConstants" : "native");
            if (LoadReflection)
            {
                const auto* Buffer = Resources.GetUniformBufferDesc(i);
                ASSERT_NE(Buffer, nullptr);
                if (UB.BindGroup == 1)
                    EXPECT_EQ(*Buffer, OriginalBuffer);
                else
                {
                    ASSERT_EQ(Buffer->NumVariables, 1u);
                    EXPECT_STREQ(Buffer->pVariables[0].Name, "native_member");
                    EXPECT_STREQ(Buffer->pVariables[0].TypeName, "vec4<f32>");
                    EXPECT_EQ(Buffer->pVariables[0].NumRows, 4u);
                    EXPECT_EQ(Buffer->pVariables[0].NumColumns, 1u);
                }
            }
        }
        ASSERT_EQ(Resources.GetNumSpecConstants(), 2u);
        for (Uint32 i = 0; i < Resources.GetNumSpecConstants(); ++i)
        {
            const auto& SC = Resources.GetSpecConstant(i);
            ASSERT_TRUE(SC.OverrideId == 7 || SC.OverrideId == 8);
            EXPECT_STREQ(SC.Name, SC.OverrideId == 7 ? "OriginalScale" : "native_scale");
            EXPECT_TRUE(SC.IsIdSpecified);
            EXPECT_EQ(SC.GetType(), SHADER_CODE_BASIC_TYPE_FLOAT);
        }
    }
}

TEST(WGSLShaderResources, MetadataNamesAreRestoredBeforeArrayMerging)
{
    std::string          WGSL = R"(
@group(0) @binding(3) var generated_a: texture_2d<f32>;
@group(0) @binding(8) var generated_b: texture_2d<f32>;
@group(1) @binding(3) var native_texture: texture_2d<f32>;
@fragment fn main() -> @location(0) vec4f {
    return textureLoad(generated_a, vec2i(0), 0) +
        textureLoad(generated_b, vec2i(0), 0) + textureLoad(native_texture, vec2i(0), 0);
}
)";
    WGSLShaderReflection Metadata;
    Metadata.Resources[{0, 3}].Name = "Textures_element_2";
    Metadata.Resources[{0, 8}].Name = "Textures_element_5";
    AppendWGSLShaderReflection(WGSL, Metadata);
    for (const char* Suffix : {static_cast<const char*>(nullptr), "", "_element_"})
    {
        SCOPED_TRACE(Suffix != nullptr ? Suffix : "null");
        const bool                Merge = Suffix != nullptr && Suffix[0] != '\0';
        const WGSLShaderResources Resources{GetRawAllocator(), WGSL, SHADER_SOURCE_LANGUAGE_WGSL,
                                            "Metadata resource arrays", nullptr, "main", Suffix, false, nullptr};
        ASSERT_EQ(Resources.GetNumTextures(), Merge ? 2u : 3u);
        for (Uint32 i = 0; i < Resources.GetNumTextures(); ++i)
        {
            const auto& Texture = Resources.GetTexture(i);
            EXPECT_EQ(Texture.Type, WGSLResourceType::Texture);
            if (Texture.BindGroup == 1)
            {
                EXPECT_STREQ(Texture.Name, "native_texture");
                EXPECT_EQ(Texture.ArraySize, 1u);
            }
            else if (Merge)
            {
                EXPECT_STREQ(Texture.Name, "Textures");
                EXPECT_EQ(Texture.ArraySize, 6u);
            }
            else
            {
                EXPECT_STREQ(Texture.Name, Texture.BindIndex == 3 ? "Textures_element_2" : "Textures_element_5");
                EXPECT_EQ(Texture.ArraySize, 1u);
            }
        }
    }
}

TEST(WGSLShaderResources, SelectedEntryPointReflection)
{
    const std::string WGSL = R"(
struct Uniforms { value: vec4f }
@group(0) @binding(0) var<uniform> vertex_uniforms: Uniforms;
@group(0) @binding(0) var<uniform> fragment_uniforms: Uniforms;
@group(0) @binding(1) var<uniform> shared_uniforms: Uniforms;
@group(0) @binding(2) var<storage, read_write> output: array<vec4f>;
@id(7) override VertexScale: f32 = 1.0;
@id(9) override FragmentScale: f32 = 2.0;
@id(11) override Width: u32 = 4;
override Unused: u32 = 1;
fn read_vertex() -> vec4f { return vertex_uniforms.value * VertexScale; }
fn read_fragment() -> vec4f { return fragment_uniforms.value * FragmentScale; }
@vertex fn vs() -> @builtin(position) vec4f { return read_vertex() + shared_uniforms.value; }
@fragment fn fs() -> @location(0) vec4f { return read_fragment() + shared_uniforms.value; }
@compute @workgroup_size(Width) fn cs() { output[0] = shared_uniforms.value; }
)";
    const struct
    {
        const char* EntryPoint;
        SHADER_TYPE Stage;
        const char* ResourceName;
        const char* ConstantName;
        Uint32      ConstantId;
    } Cases[] = {
        {"vs", SHADER_TYPE_VERTEX, "vertex_uniforms", "VertexScale", 7},
        {"fs", SHADER_TYPE_PIXEL, "fragment_uniforms", "FragmentScale", 9},
        {"cs", SHADER_TYPE_COMPUTE, "output", "Width", 11},
    };
    for (const auto& Test : Cases)
    {
        SCOPED_TRACE(Test.EntryPoint);
        const WGSLShaderResources Resources{GetRawAllocator(), WGSL, SHADER_SOURCE_LANGUAGE_WGSL,
                                            "Selected entry point", nullptr, Test.EntryPoint, nullptr, false, nullptr};
        EXPECT_STREQ(Resources.GetEntryPoint(), Test.EntryPoint);
        EXPECT_EQ(Resources.GetShaderType(), Test.Stage);
        ASSERT_EQ(Resources.GetTotalResources(), 2u);
        EXPECT_EQ(Resources.GetNumUBs(), Test.Stage == SHADER_TYPE_COMPUTE ? 1u : 2u);
        EXPECT_EQ(Resources.GetNumSBs(), Test.Stage == SHADER_TYPE_COMPUTE ? 1u : 0u);
        for (Uint32 i = 0; i < Resources.GetTotalResources(); ++i)
        {
            const auto& Resource = Resources.GetResource(i);
            EXPECT_STREQ(Resource.Name, Resource.BindIndex == 1 ? "shared_uniforms" : Test.ResourceName);
        }
        ASSERT_EQ(Resources.GetNumSpecConstants(), 1u);
        EXPECT_STREQ(Resources.GetSpecConstant(0).Name, Test.ConstantName);
        EXPECT_EQ(Resources.GetSpecConstant(0).OverrideId, Test.ConstantId);
        EXPECT_TRUE(Resources.GetSpecConstant(0).IsIdSpecified);
    }
}

TEST(WGSLShaderResources, NativeStorageAndMultisampledTextures)
{
    const std::string               WGSL       = R"(
@group(0) @binding(0) var Read: texture_storage_2d_array<rgba8unorm, read>;
@group(0) @binding(1) var Write: texture_storage_3d<rgba32uint, write>;
@group(0) @binding(2) var ReadWrite: texture_storage_2d<r32sint, read_write>;
@group(0) @binding(3) var Depth: texture_depth_multisampled_2d;
@group(0) @binding(4) var MS: texture_multisampled_2d<i32>;
@group(0) @binding(5) var<storage, read_write> Output: array<f32>;
@group(0) @binding(6) var External: texture_external;
@compute @workgroup_size(1) fn main() {
    Output[0] = textureLoad(Read, vec2i(0), 0).x + textureLoad(Depth, vec2i(0), 0) +
        f32(textureLoad(MS, vec2i(0), 0).x) + textureLoad(External, vec2i(0)).x;
    textureStore(Write, vec3i(0), vec4u(1));
    textureStore(ReadWrite, vec2i(0), textureLoad(ReadWrite, vec2i(0)));
}
)";
    const WGSLShaderResourceAttribs Expected[] = {
        {"Read", WGSLResourceType::ROStorageTexture, 1, RESOURCE_DIM_TEX_2D_ARRAY, TEX_FORMAT_RGBA8_UNORM, WGSLSampleType::Float},
        {"Write", WGSLResourceType::WOStorageTexture, 1, RESOURCE_DIM_TEX_3D, TEX_FORMAT_RGBA32_UINT, WGSLSampleType::UInt},
        {"ReadWrite", WGSLResourceType::RWStorageTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_R32_SINT, WGSLSampleType::SInt},
        {"Depth", WGSLResourceType::DepthTextureMS, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Depth},
        {"MS", WGSLResourceType::TextureMS, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::SInt},
        {"Output", WGSLResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER},
        {"External", WGSLResourceType::ExternalTexture, 1, RESOURCE_DIM_TEX_2D, TEX_FORMAT_UNKNOWN, WGSLSampleType::Float},
    };
    const WGSLShaderResources Resources{GetRawAllocator(), WGSL, SHADER_SOURCE_LANGUAGE_WGSL,
                                        "Native texture reflection", nullptr, "main", nullptr, false, nullptr};
    ASSERT_EQ(Resources.GetTotalResources(), 7u);
    EXPECT_EQ(Resources.GetNumStTextures(), 3u);
    EXPECT_EQ(Resources.GetNumTextures(), 2u);
    EXPECT_EQ(Resources.GetNumExtTextures(), 1u);
    EXPECT_EQ(Resources.GetNumSBs(), 1u);
    for (Uint32 i = 0; i < Resources.GetTotalResources(); ++i)
    {
        const auto& Resource = Resources.GetResource(i);
        SCOPED_TRACE(Resource.Name);
        ASSERT_LT(Resource.BindIndex, 7u);
        const auto& Ref = Expected[Resource.BindIndex];
        EXPECT_STREQ(Resource.Name, Ref.Name);
        EXPECT_EQ(Resource.Type, Ref.Type);
        EXPECT_EQ(Resource.ResourceDim, Ref.ResourceDim);
        EXPECT_EQ(Resource.Format, Ref.Format);
        EXPECT_EQ(Resource.SampleType, Ref.SampleType);
        EXPECT_EQ(Resource.IsMultisample(), Resource.BindIndex == 3 || Resource.BindIndex == 4);
    }
}

TEST(WGSLShaderResources, InvalidSourceAndReflectionDiagnostics)
{
    const struct
    {
        const char* Source;
        const char* EntryPoint;
        const char* Error;
        bool        HasTintDiagnostics;
    } Cases[] = {
        {"invalid WGSL", "main", "Failed to parse shader source", true},
        {"fn helper() {}", nullptr, "does not contain any entry points", false},
        {"@compute @workgroup_size(1) fn main() {}", "missing", "Entry point 'missing' is not found", false},
        {"@compute @workgroup_size(1) fn main() {}\n/*$DILIGENT_WGSL_REFLECTION=invalid*/",
         "main", "Invalid WGSL reflection metadata", false},
    };
    for (const auto& Test : Cases)
    {
        SCOPED_TRACE(Test.Error);
        RefCntAutoPtr<IDataBlob>       Diagnostics;
        TestingEnvironment::ErrorScope Errors{Test.Error};
        EXPECT_THROW((WGSLShaderResources{GetRawAllocator(), Test.Source, SHADER_SOURCE_LANGUAGE_WGSL,
                                          "Invalid reflection input", nullptr, Test.EntryPoint, nullptr, false, &Diagnostics}),
                     std::runtime_error);
        if (Test.HasTintDiagnostics)
        {
            ASSERT_NE(Diagnostics, nullptr);
            const char*       Text = static_cast<const char*>(Diagnostics->GetConstDataPtr());
            const std::string Message{Text};
            EXPECT_FALSE(Message.empty());
            ASSERT_EQ(Diagnostics->GetSize(), Message.size() + 1 + strlen(Test.Source) + 1);
            EXPECT_STREQ(Text + Message.size() + 1, Test.Source);
        }
        else
        {
            EXPECT_EQ(Diagnostics, nullptr);
        }
    }
}

} // namespace
