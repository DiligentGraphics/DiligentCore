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

#include "WGSLUtils.hpp"
#include "WGSLShaderResources.hpp"
#include "WGSLShaderReflection.hpp"
#include "SPIRVShaderResources.hpp"
#include "GLSLangUtils.hpp"
#include "DefaultShaderSourceStreamFactory.h"
#include "RefCntAutoPtr.hpp"
#include "EngineMemory.h"

#include "TestingEnvironment.hpp"
#include "gtest/gtest.h"

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4324) //  warning C4324: structure was padded due to alignment specifier
#endif

#include <tint/tint.h>
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/ast/identifier_expression.h"
#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/sem/variable.h"

#ifdef _MSC_VER
#    pragma warning(pop)
#endif

using namespace Diligent;
using namespace Diligent::Testing;

namespace Diligent
{

inline std::ostream& operator<<(std::ostream& os, const WGSLEmulatedResourceArrayElement& Elem)
{
    return os << '\'' << Elem.Name << "'[" << Elem.Index << ']';
}

} // namespace Diligent

namespace
{

TEST(WGSLUtils, ConvertSPIRVtoWGSLPreservesHLSLReflection)
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc           = {"HLSL reflection preservation test", SHADER_TYPE_VERTEX};
    ShaderCI.EntryPoint     = "main";
    ShaderCI.Source         = R"(
cbuffer Constants
{
    float Offset;
    float3 Scale;
};

float4 main(float3 Position : ATTRIB0) : SV_Position
{
    return float4(Position * Scale + Offset, 1.0);
}
)";

    GLSLangUtils::InitializeGlslang();
    auto SPIRV = GLSLangUtils::HLSLtoSPIRV(ShaderCI, GLSLangUtils::SpirvVersion::Vk110, nullptr, nullptr);
    GLSLangUtils::FinalizeGlslang();
    ASSERT_FALSE(SPIRV.empty());

    // This shader only uses SPIR-V 1.0 instructions. Keep its binary version at 1.0
    // to verify that Vulkan 1.1 relaxed block layout is not inferred from the version.
    SPIRV[1] = 0x00010000u;

    const auto OriginalSPIRV = SPIRV;
    const auto WGSL          = ConvertSPIRVtoWGSL(SPIRV);
    ASSERT_FALSE(WGSL.empty());
    EXPECT_EQ(SPIRV, OriginalSPIRV);

    tint::Source::File SrcFile{"", WGSL};
    tint::Program      Program = tint::wgsl::reader::Parse(&SrcFile, {tint::wgsl::AllowedFeatures::Everything()});
    ASSERT_TRUE(Program.IsValid()) << Program.Diagnostics().Str();

    SPIRVShaderResources::CreateInfo ResCI;
    ResCI.ShaderType            = SHADER_TYPE_VERTEX;
    ResCI.Name                  = ShaderCI.Desc.Name;
    ResCI.LoadShaderStageInputs = true;
    const SPIRVShaderResources Resources{GetRawAllocator(), SPIRV, ResCI};
    ASSERT_EQ(Resources.GetNumShaderStageInputs(), 1u);
    EXPECT_STREQ(Resources.GetShaderStageInputAttribs(0).Semantic, "ATTRIB0");
}


std::vector<uint32_t> CompileWGSLConversionTestShader()
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc           = {"WGSL conversion test", SHADER_TYPE_PIXEL};
    ShaderCI.EntryPoint     = "main";
    ShaderCI.Source         = R"(
cbuffer Constants
{
    float4 Color;
};
float4 main() : SV_Target
{
    return Color;
}
)";
    GLSLangUtils::InitializeGlslang();
    auto SPIRV = GLSLangUtils::HLSLtoSPIRV(ShaderCI, GLSLangUtils::SpirvVersion::Vk110, nullptr, nullptr);
    GLSLangUtils::FinalizeGlslang();
    return SPIRV;
}

TEST(WGSLUtils, ConvertSPIRVtoWGSLWithoutSourceMetadata)
{
    const auto SPIRV = CompileWGSLConversionTestShader();
    ASSERT_GT(SPIRV.size(), 5u);

    // Remove OpSource (opcode 3) and OpSourceContinued (opcode 2), retaining
    // names and layout decorations. Supplied bytecode need not identify its
    // original language; conversion must then use WGSL-derived reflection.
    std::vector<uint32_t> Stripped{SPIRV.begin(), SPIRV.begin() + 5};
    bool                  RemovedSource = false;
    for (size_t Offset = 5; Offset < SPIRV.size();)
    {
        const Uint32 WordCount = SPIRV[Offset] >> 16;
        const Uint32 OpCode    = SPIRV[Offset] & 0xffffu;
        ASSERT_NE(WordCount, 0u);
        ASSERT_LE(WordCount, SPIRV.size() - Offset);
        if (OpCode == 3 || OpCode == 2)
            RemovedSource |= OpCode == 3;
        else
            Stripped.insert(Stripped.end(), SPIRV.begin() + Offset, SPIRV.begin() + Offset + WordCount);
        Offset += WordCount;
    }
    ASSERT_TRUE(RemovedSource);
    const auto OriginalStripped = Stripped;
    for (bool LoadReflection : {false, true})
    {
        SCOPED_TRACE(LoadReflection);
        const std::string WGSL = ConvertSPIRVtoWGSL(Stripped, LoadReflection);
        ASSERT_FALSE(WGSL.empty());
        EXPECT_EQ(Stripped, OriginalStripped);

        WGSLShaderReflection Metadata;
        ASSERT_TRUE(ParseWGSLShaderReflection(WGSL, Metadata));
        EXPECT_TRUE(Metadata.Resources.empty());
        EXPECT_TRUE(Metadata.SpecializationConstants.empty());

        // The legacy type-name alias still permits resource remapping after
        // source metadata has been stripped from the input SPIR-V.
        const auto Remapped = RemapWGSLResourceBindings(WGSL, {{"Constants", {2, 3}}}, nullptr, "main");
        ASSERT_FALSE(Remapped.empty());
        const WGSLShaderResources Resources{GetRawAllocator(), Remapped, SHADER_SOURCE_LANGUAGE_WGSL,
                                            "Stripped source", nullptr, "main", nullptr, LoadReflection, nullptr};
        ASSERT_EQ(Resources.GetNumUBs(), 1u);
        EXPECT_STREQ(Resources.GetUB(0).Name, "Constants");
        EXPECT_EQ(Resources.GetUB(0).BindGroup, 2u);
        EXPECT_EQ(Resources.GetUB(0).BindIndex, 3u);
        if (LoadReflection)
        {
            const auto* Buffer = Resources.GetUniformBufferDesc(0);
            ASSERT_NE(Buffer, nullptr);
            EXPECT_EQ(Buffer->Size, 16u);
            ASSERT_EQ(Buffer->NumVariables, 1u);
            EXPECT_STREQ(Buffer->pVariables[0].Name, "Color");
            EXPECT_EQ(Buffer->pVariables[0].BasicType, SHADER_CODE_BASIC_TYPE_FLOAT);
            EXPECT_EQ(Buffer->pVariables[0].Class, SHADER_CODE_VARIABLE_CLASS_VECTOR);
        }
    }
}

TEST(WGSLUtils, ConvertSPIRVtoWGSLRejectsMalformedInput)
{
    const auto SPIRV = CompileWGSLConversionTestShader();
    ASSERT_GT(SPIRV.size(), 5u);
    for (Uint32 Case = 0; Case < 5; ++Case)
    {
        SCOPED_TRACE(Case);
        auto Invalid = SPIRV;
        switch (Case)
        {
            case 0: Invalid.clear(); break;
            case 1: Invalid.resize(4); break;         // Truncated module header.
            case 2: Invalid[0] = 0; break;            // Invalid magic number.
            case 3: Invalid[5] &= 0xffffu; break;     // Instruction with zero words.
            case 4: Invalid[5] |= 0xffff0000u; break; // Instruction extends past the module.
        }
        const auto Original = Invalid;
        // ErrorScope checks expected messages in reverse order.
        TestingEnvironment::ErrorScope Errors{
            "Failed to prepare SPIR-V for Tint conversion",
            "Spirv optimizer error"};
        EXPECT_TRUE(ConvertSPIRVtoWGSL(Invalid).empty());
        EXPECT_EQ(Invalid, Original);
    }
}

TEST(WGSLUtils, GetWGSLEmulatedArrayElementInfo)
{
    EXPECT_EQ(GetWGSLEmulatedArrayElement("", ""), WGSLEmulatedResourceArrayElement{});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("", "_"), WGSLEmulatedResourceArrayElement{});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D", ""), WGSLEmulatedResourceArrayElement{"Tex2D"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D", "_"), WGSLEmulatedResourceArrayElement{"Tex2D"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_X", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_X"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_-1", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_-1"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_+1", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_+1"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_2147483648", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_2147483648"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_123x", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_123x"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_123_", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_123_"});

    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_0", "_"), WGSLEmulatedResourceArrayElement("Tex2D", 0));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_1", "_"), WGSLEmulatedResourceArrayElement("Tex2D", 1));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_17", "_"), WGSLEmulatedResourceArrayElement("Tex2D", 17));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_00017", "_"), WGSLEmulatedResourceArrayElement("Tex2D", 17));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_2147483647", "_"), WGSLEmulatedResourceArrayElement("Tex2D", 2147483647));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_0_5", "_"), WGSLEmulatedResourceArrayElement("Tex2D_0", 5));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_1_18", "_"), WGSLEmulatedResourceArrayElement("Tex2D_1", 18));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_17_3", "_"), WGSLEmulatedResourceArrayElement("Tex2D_17", 3));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_17__4", "_"), WGSLEmulatedResourceArrayElement("Tex2D_17_", 4));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_17___5", "_"), WGSLEmulatedResourceArrayElement("Tex2D_17__", 5));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_17____6", "_"), WGSLEmulatedResourceArrayElement("Tex2D_17___", 6));

    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_i0", "_i"), WGSLEmulatedResourceArrayElement("Tex2D", 0));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_i0_1", "_i"), WGSLEmulatedResourceArrayElement("Tex2D_i0_1"));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_id1", "_id"), WGSLEmulatedResourceArrayElement("Tex2D", 1));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_id1_i1", "_id"), WGSLEmulatedResourceArrayElement("Tex2D_id1_i1"));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_ind19", "_ind"), WGSLEmulatedResourceArrayElement("Tex2D", 19));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_idx999", "_idx"), WGSLEmulatedResourceArrayElement("Tex2D", 999));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_idx0_idx35", "_idx"), WGSLEmulatedResourceArrayElement("Tex2D_idx0", 35));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_i1_i2_i3_i52", "_i"), WGSLEmulatedResourceArrayElement("Tex2D_i1_i2_i3", 52));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_i1_i2_i3_52", "_i"), WGSLEmulatedResourceArrayElement("Tex2D_i1_i2_i3_52"));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_i1_i2_i3_i4_i52", "_i"), WGSLEmulatedResourceArrayElement("Tex2D_i1_i2_i3_i4", 52));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_i1_i2_i3_i4_52", "_i"), WGSLEmulatedResourceArrayElement("Tex2D_i1_i2_i3_i4_52"));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_nn17_nn4", "_nn"), WGSLEmulatedResourceArrayElement("Tex2D_nn17", 4));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2Dxx1", "xx"), WGSLEmulatedResourceArrayElement("Tex2D", 1));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2Dxxx2", "xx"), WGSLEmulatedResourceArrayElement("Tex2Dx", 2));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2Dxxxx3", "xx"), WGSLEmulatedResourceArrayElement("Tex2Dxx", 3));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2Dxxxxx4", "xx"), WGSLEmulatedResourceArrayElement("Tex2Dxxx", 4));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2Dxxxxxx5", "xx"), WGSLEmulatedResourceArrayElement("Tex2Dxxxx", 5));
}

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

void TestResourceRemapping(const char*                FilePath,
                           const WGSLResourceMapping& ResRemapping,
                           WGSLResourceMapping        RefResources = {})
{
    if (RefResources.empty())
        RefResources = ResRemapping;

    const auto WGSL = HLSLtoWGLS(FilePath);
    ASSERT_FALSE(WGSL.empty());

    const auto RemappedWGSL = RemapWGSLResourceBindings(WGSL, ResRemapping, "_");
    ASSERT_FALSE(RemappedWGSL.empty());

    tint::Source::File srcFile("", RemappedWGSL);
    tint::Program      Program = tint::wgsl::reader::Parse(&srcFile, {tint::wgsl::AllowedFeatures::Everything()});
    ASSERT_TRUE(Program.IsValid()) << Program.Diagnostics().Str();

    tint::inspector::Inspector Inspector{Program};
    ASSERT_EQ(Inspector.GetEntryPoints().size(), size_t{1}) << "Program is expected to have a single entry point";
    for (auto& EntryPoint : Inspector.GetEntryPoints())
    {
        for (auto& Binding : Inspector.GetResourceBindings(EntryPoint.name))
        {
            auto RemappedBindingIt = RefResources.find(Binding.variable_name);
            if (RemappedBindingIt == RefResources.end() &&
                (Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kUniformBuffer ||
                 Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kStorageBuffer ||
                 Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kReadOnlyStorageBuffer))
            {
                // Search variable by alternative name
                RemappedBindingIt = RefResources.find(GetWGSLResourceAlternativeName(Program, Binding));
            }

            if (RemappedBindingIt == RefResources.end())
            {
                GTEST_FAIL() << "Unable to find remapping for resource '" << Binding.variable_name << "'";
            }
            else
            {
                EXPECT_EQ(Binding.bind_group, RemappedBindingIt->second.Group) << "Bind group mismatch (" << Binding.bind_group << " vs " << RemappedBindingIt->second.Group << ") for resource '" << Binding.variable_name << "'";
                EXPECT_EQ(Binding.binding, RemappedBindingIt->second.Index) << "Binding index mismatch (" << Binding.binding << " vs " << RemappedBindingIt->second.Index << ") for resource '" << Binding.variable_name << "'";
            }
        }
    }
}

TEST(WGSLUtils, RemapUniformBuffers)
{
    TestResourceRemapping("UniformBuffers.psh",
                          {
                              {"CB0", {1, 2}},
                              {"CB1", {3, 4}},
                              {"CB2", {5, 6}},
                          });
}

TEST(WGSLUtils, RemapTextures)
{
    TestResourceRemapping("Textures.psh",
                          {
                              // clang-format off
                              {"g_Tex1D",      {1, 2}},
                              {"g_Tex2D",      {3, 4}},
                              {"g_Tex2DArr",   {5, 6}},
                              {"g_TexCube",    {7, 8}},
                              {"g_TexCubeArr", {9, 10}},
                              {"g_Tex3D",      {11, 12}},
                              {"g_Tex2DMS",    {13, 14}},
                              {"g_Tex2DDepth", {15, 16}},
                              {"g_Sampler",    {17, 18}},
                              {"g_SamplerCmp", {19, 20}},
                              // clang-format on
                          });
}

TEST(WGSLUtils, RemapRWTextures)
{
    TestResourceRemapping("RWTextures.psh",
                          {
                              // clang-format off
                              {"g_WOTex1D",    {1, 2}},
                              {"g_WOTex2D",    {3, 4}},
                              {"g_WOTex2DArr", {5, 6}},
                              {"g_WOTex3D",    {7, 8}},

                              {"g_ROTex1D",    { 9, 10}},
                              {"g_ROTex2D",    {11, 12}},
                              {"g_ROTex2DArr", {13, 14}},
                              {"g_ROTex3D",    {15, 16}},

                              {"g_RWTex1D",    {17, 18}},
                              {"g_RWTex2D",    {19, 20}},
                              {"g_RWTex2DArr", {21, 22}},
                              {"g_RWTex3D",    {23, 24}},
                              // clang-format on
                          });
}

TEST(WGSLUtils, RemapStructBuffers)
{
    TestResourceRemapping("StructBuffers.psh",
                          {
                              {"g_Buff0", {1, 2}},
                              {"g_Buff1", {3, 4}},
                              {"g_Buff2", {5, 6}},
                              {"g_Buff3", {7, 8}},
                          });
}

TEST(WGSLUtils, RemapRWStructBuffers)
{
    TestResourceRemapping("RWStructBuffers.psh",
                          {
                              {"g_RWBuff0", {1, 2}},
                              {"g_RWBuff1", {3, 4}},
                              {"g_RWBuff2", {5, 6}},
                              {"g_RWBuff3", {7, 8}},
                              {"g_RWBuffAtomic0", {9, 10}},
                              {"g_RWBuffAtomic1", {11, 12}},
                              {"g_RWBuff0_atomic", {13, 14}},
                              {"g_RWBuff1_atomic", {15, 16}},
                              {"g_RWBuff0Atomic_atomic", {17, 18}},
                              {"g_RWBuff1Atomic_atomic", {19, 20}},
                              {"g_RWBuff2Atomic", {21, 22}},
                              {"g_RWBuff3Atomic", {23, 24}},
                              {"g_RWBuff4Atomic_atomic", {25, 26}},
                              {"g_RWBuff5Atomic_atomic", {27, 28}},
                          });
}

TEST(WGSLUtils, RemapTextureArrays)
{
    TestResourceRemapping(
        "TextureArrays.psh",
        {
            // clang-format off
            {"g_Tex2DArr0",       {1, 2, 8}},
            {"g_Tex2DNotArr0_2",  {3, 4}},
            {"g_Tex2DNotArr0_4",  {5, 6}},
            {"g_Tex2DNotArr1_1",  {7, 8}},
            {"g_Tex2DNotArr1_2",  {9, 10}},
            {"g_Tex2DNotArr2_3",  {11, 12}},
            {"g_Tex2DNotArr2_5",  {13, 14}},
            {"g_Tex2DNotArr3_3x", {15, 16}},
            {"g_Tex2DNotArr4_",   {17, 18}}
            // clang-format on
        },
        {
            // clang-format off
            {"g_Tex2DArr0_1",     {1, 3}},
            {"g_Tex2DArr0_2",     {1, 4}},
            {"g_Tex2DArr0_3",     {1, 5}},
            {"g_Tex2DArr0_7",     {1, 9}},

            {"g_Tex2DNotArr0_2",  {3, 4}},
            {"g_Tex2DNotArr0_4",  {5, 6}},
            {"g_Tex2DNotArr1_1",  {7, 8}},
            {"g_Tex2DNotArr1_2",  {9, 10}},
            {"g_Tex2DNotArr2_3",  {11, 12}},
            {"g_Tex2DNotArr2_5",  {13, 14}},
            {"g_Tex2DNotArr3_3x", {15, 16}},
            {"g_Tex2DNotArr4_",   {17, 18}}
            // clang-format on
        });
}

TEST(WGSLUtils, RemapSamplerArrays)
{
    TestResourceRemapping(
        "SamplerArrays.psh",
        {
            // clang-format off
            {"g_Tex2D",            {1, 2}},
			{"g_SamplerArr0",      {3, 4, 8}},
			{"g_SamplerNotArr1_3", {9, 10}},
			{"g_SamplerNotArr1_5", {11, 12}}
            // clang-format on
        },
        {
            // clang-format off
            {"g_Tex2D",            {1, 2}},

            {"g_SamplerArr0_2",    {3, 6}},
			{"g_SamplerArr0_5",    {3, 9}},
			{"g_SamplerArr0_7",    {3, 11}},

			{"g_SamplerNotArr1_3", {9, 10}},
			{"g_SamplerNotArr1_5", {11, 12}}
            // clang-format on
        });
}

TEST(WGSLUtils, RemapStructBufferArrays)
{
    TestResourceRemapping(
        "StructBufferArrays.psh",
        {
            // clang-format off
			{"g_BuffArr0", {1, 2, 6}},
			{"g_BuffArr1", {3, 4, 3}},
			{"g_BuffArr2", {5, 6, 5}}
            // clang-format on
        },
        {
            // clang-format off
			{"g_BuffArr0_3", {1, 5}},
			{"g_BuffArr0_5", {1, 7}},

			{"g_BuffArr1_1", {3, 5}},
			{"g_BuffArr1_2", {3, 6}},

			{"g_BuffArr2_0", {5, 6}},
			{"g_BuffArr2_4", {5, 10}}
            // clang-format on
        });
}

TEST(WGSLUtils, RemapRWTExtureArrays)
{
    TestResourceRemapping(
        "RWTextureArrays.psh",
        {
            // clang-format off
			{"g_WOTex2DArr0", {1, 2, 4}},
			{"g_RWTex2DArr0", {3, 4, 3}},
			{"g_WOTex2DNotArr1_2", {5, 6}},
			{"g_WOTex2DNotArr1_4", {7, 8}},
			{"g_RWTex2DNotArr2_5", {9, 10}},
			{"g_RWTex2DNotArr2_9", {11, 12}},
            // clang-format on
        },
        {
            // clang-format off
			{"g_WOTex2DArr0_1", {1, 3}},
			{"g_WOTex2DArr0_3", {1, 5}},
			
            {"g_RWTex2DArr0_0", {3, 4}},
            {"g_RWTex2DArr0_2", {3, 6}},

			{"g_WOTex2DNotArr1_2", {5, 6}},
			{"g_WOTex2DNotArr1_4", {7, 8}},
			{"g_RWTex2DNotArr2_5", {9, 10}},
			{"g_RWTex2DNotArr2_9", {11, 12}},
            // clang-format on
        });
}

TEST(WGSLUtils, RWStructBufferArrays)
{
    TestResourceRemapping(
        "RWStructBufferArrays.psh",
        {
            // clang-format off
			{"g_RWBuffArr0", {1, 2, 6}},
			{"g_RWBuffArr1", {3, 4, 3}},
			{"g_RWBuffArr2", {5, 6, 2}},
            // clang-format on
        },
        {
            // clang-format off
			{"g_RWBuffArr0_3", {1, 5, 4}},
			{"g_RWBuffArr0_5", {1, 7, 4}},

			{"g_RWBuffArr1_0", {3, 4, 3}},
			{"g_RWBuffArr1_2", {3, 6, 3}},

            {"g_RWBuffArr2_0", {5, 6}},
            {"g_RWBuffArr2_1", {5, 7}},
            // clang-format on
        });
}


TEST(WGSLUtils, RemapReturnsGeneratedEntryPoint)
{
    const struct
    {
        const char* Description;
        const char* WGSL;
        const char* EntryPoint;
        bool        Renamed;
    } Cases[] = {
        {
            "Name renamed while writing floating-point IR",
            "@fragment fn f32() -> @location(0) vec4f { return vec4f(1.0); }",
            "f32",
            true,
        },
        {
            "Name renamed while generating the AST",
            "@compute @workgroup_size(1) fn f32() {}",
            "f32",
            true,
        },
        {
            "Ordinary entry point name",
            "@compute @workgroup_size(1) fn main() {}",
            "main",
            false,
        },
    };
    for (const auto& Case : Cases)
    {
        SCOPED_TRACE(Case.Description);
        std::string Current            = Case.WGSL;
        std::string RemappedEntryPoint = Case.EntryPoint;
        for (Uint32 Pass = 0; Pass < 2; ++Pass)
        {
            SCOPED_TRACE(Pass);
            // The selected name may alias the output, including on subsequent remaps.
            Current = RemapWGSLResourceBindings(Current, {}, nullptr, RemappedEntryPoint.c_str(), &RemappedEntryPoint);
            ASSERT_FALSE(Current.empty());

            tint::Source::File File{"", Current};
            auto               Program = tint::wgsl::reader::Parse(&File, {tint::wgsl::AllowedFeatures::Everything()});
            ASSERT_TRUE(Program.IsValid()) << Program.Diagnostics().Str();
            tint::inspector::Inspector Inspector{Program};
            const auto                 EntryPoints = Inspector.GetEntryPoints();
            ASSERT_EQ(EntryPoints.size(), 1u);
            EXPECT_EQ(RemappedEntryPoint, EntryPoints[0].name);
            if (Case.Renamed)
                EXPECT_NE(RemappedEntryPoint, Case.EntryPoint);
            else
                EXPECT_EQ(RemappedEntryPoint, Case.EntryPoint);
        }
    }
}

TEST(WGSLUtils, RemapWholeModuleClearsEntryPointOutput)
{
    const std::string WGSL               = R"(
@compute @workgroup_size(1) fn first() {}
@compute @workgroup_size(1) fn second() {}
)";
    std::string       RemappedEntryPoint = "previous_entry_point";
    const std::string Remapped           = RemapWGSLResourceBindings(WGSL, {}, nullptr, nullptr, &RemappedEntryPoint);
    ASSERT_FALSE(Remapped.empty());
    EXPECT_TRUE(RemappedEntryPoint.empty());

    tint::Source::File File{"", Remapped};
    auto               Program = tint::wgsl::reader::Parse(&File, {tint::wgsl::AllowedFeatures::Everything()});
    ASSERT_TRUE(Program.IsValid()) << Program.Diagnostics().Str();
    tint::inspector::Inspector Inspector{Program};
    const auto                 EntryPoints = Inspector.GetEntryPoints();
    ASSERT_EQ(EntryPoints.size(), 2u);
    EXPECT_NE(EntryPoints[0].name, EntryPoints[1].name);
    for (const auto& EntryPoint : EntryPoints)
        EXPECT_TRUE(EntryPoint.name == "first" || EntryPoint.name == "second");
}

TEST(WGSLUtils, RemapPreservesEntryPointOutputOnFailure)
{
    const std::string WGSL               = R"(
@group(0) @binding(0) var<storage, read_write> Output: array<u32>;
@compute @workgroup_size(1) fn main() { Output[0] = 1; }
)";
    std::string       RemappedEntryPoint = "unchanged";
    {
        TestingEnvironment::ErrorScope Errors{"Tint WGSL reader failure"};
        EXPECT_TRUE(RemapWGSLResourceBindings("invalid WGSL", {}, nullptr, "main", &RemappedEntryPoint).empty());
        EXPECT_EQ(RemappedEntryPoint, "unchanged");
    }
    {
        TestingEnvironment::ErrorScope Errors{"Binding for variable 'Output' is not found"};
        EXPECT_TRUE(RemapWGSLResourceBindings(WGSL, {}, nullptr, "main", &RemappedEntryPoint).empty());
        EXPECT_EQ(RemappedEntryPoint, "unchanged");
    }
}

TEST(WGSLUtils, RemapSelectedEntryPoint)
{
    const std::string WGSL = R"(
@group(0) @binding(0) var<storage, read_write> FirstOutput: array<u32>;
@group(0) @binding(0) var<storage, read_write> SecondOutput: array<u32>;
override Shared: u32 = 1;
@id(7) override FirstValue: u32 = 2;
@id(9) override SecondValue: u32 = 3;
@compute @workgroup_size(1) fn first() { FirstOutput[0] = Shared + FirstValue; }
@compute @workgroup_size(1) fn second() { SecondOutput[0] = Shared + SecondValue; }
)";
    for (const char* EntryPoint : {"first", "second"})
    {
        SCOPED_TRACE(EntryPoint);
        const char*               ResourceName = strcmp(EntryPoint, "first") == 0 ? "FirstOutput" : "SecondOutput";
        const WGSLResourceMapping Mapping{{ResourceName, {2, 3}}};
        std::string               RemappedEntryPoint;
        const std::string         Remapped = RemapWGSLResourceBindings(WGSL, Mapping, nullptr, EntryPoint, &RemappedEntryPoint);
        ASSERT_FALSE(Remapped.empty());
        tint::Source::File File{"", Remapped};
        auto               Program = tint::wgsl::reader::Parse(&File, {tint::wgsl::AllowedFeatures::Everything()});
        ASSERT_TRUE(Program.IsValid()) << Program.Diagnostics().Str();
        tint::inspector::Inspector Inspector{Program};
        const auto                 EntryPoints = Inspector.GetEntryPoints();
        ASSERT_EQ(EntryPoints.size(), 1u);
        EXPECT_EQ(EntryPoints[0].name, EntryPoint);
        EXPECT_EQ(RemappedEntryPoint, EntryPoints[0].name);
        EXPECT_EQ(EntryPoints[0].overrides.size(), 2u);
        const auto Bindings = Inspector.GetResourceBindings(EntryPoint);
        ASSERT_EQ(Bindings.size(), 1u);
        EXPECT_EQ(Bindings[0].bind_group, 2u);
        EXPECT_EQ(Bindings[0].binding, 3u);
        EXPECT_EQ(Bindings[0].variable_name, ResourceName);
    }

    {
        TestingEnvironment::ErrorScope Errors{"Conflicting remappings for binding"};
        std::string                    RemappedEntryPoint = "unchanged";
        EXPECT_TRUE(RemapWGSLResourceBindings(WGSL, {{"FirstOutput", {1, 0}}, {"SecondOutput", {2, 0}}}, nullptr, nullptr, &RemappedEntryPoint).empty());
        EXPECT_EQ(RemappedEntryPoint, "unchanged");
    }
    {
        TestingEnvironment::ErrorScope Errors{"WGSL entry point 'missing' was not found"};
        std::string                    RemappedEntryPoint = "missing";
        EXPECT_TRUE(RemapWGSLResourceBindings(WGSL, {}, nullptr, RemappedEntryPoint.c_str(), &RemappedEntryPoint).empty());
        EXPECT_EQ(RemappedEntryPoint, "missing");
    }
}

TEST(WGSLUtils, RemapPreservesOverrideIds)
{
    const std::string         WGSL = R"(
enable f16;
override UnusedImplicit: u32 = 9;
@id(0) override Explicit: u32 = 2;
override Named: u32 = 1;
override Floating: f32 = 1.5;
override Signed: i32 = -2;
override Half: f16 = 0.5h;
override Boolean: bool = true;
override Required: u32;
override Size: u32 = 4;
@id(65535) override Unused: u32 = 7;
var<workgroup> Shared: array<u32, Size>;
@group(0) @binding(0) var<storage, read_write> Output: array<u32>;
@compute @workgroup_size(Size) fn main() {
    Shared[0] = Named + Explicit + Required;
    Output[0] = Shared[0] + u32(Floating) + u32(Signed) + u32(Half) + select(0u, 1u, Boolean);
}
)";
    const WGSLShaderResources Original{GetRawAllocator(), WGSL, SHADER_SOURCE_LANGUAGE_WGSL,
                                       "Override remapping", nullptr, "main", nullptr, false, nullptr};
    ASSERT_EQ(Original.GetNumSpecConstants(), 8u);
    for (Uint32 i = 0; i < Original.GetNumSpecConstants(); ++i)
        EXPECT_EQ(Original.GetSpecConstant(i).IsIdSpecified, strcmp(Original.GetSpecConstant(i).Name, "Explicit") == 0);

    std::string Current = WGSL;
    for (Uint32 Pass = 0; Pass < 2; ++Pass)
    {
        Current = RemapWGSLResourceBindings(Current, {{"Output", {1, 2 + Pass}}}, nullptr, "main");
        ASSERT_FALSE(Current.empty());
        const WGSLShaderResources Remapped{GetRawAllocator(), Current, SHADER_SOURCE_LANGUAGE_WGSL,
                                           "Remapped overrides", nullptr, "main", nullptr, false, nullptr};
        ASSERT_EQ(Remapped.GetNumSpecConstants(), Original.GetNumSpecConstants());
        for (Uint32 i = 0; i < Original.GetNumSpecConstants(); ++i)
        {
            const auto& Before = Original.GetSpecConstant(i);
            bool        Found  = false;
            for (Uint32 j = 0; j < Remapped.GetNumSpecConstants(); ++j)
            {
                const auto& After = Remapped.GetSpecConstant(j);
                if (After.OverrideId == Before.OverrideId)
                {
                    Found = true;
                    EXPECT_STREQ(After.Name, Before.Name);
                    EXPECT_EQ(After.GetType(), Before.GetType());
                    EXPECT_TRUE(After.IsIdSpecified);
                }
            }
            EXPECT_TRUE(Found) << Before.Name;
        }
        tint::Source::File File{"", Current};
        auto               Program = tint::wgsl::reader::Parse(&File, {tint::wgsl::AllowedFeatures::Everything()});
        ASSERT_TRUE(Program.IsValid()) << Program.Diagnostics().Str();
        tint::inspector::Inspector Inspector{Program};
        const auto                 EntryPoints = Inspector.GetEntryPoints();
        ASSERT_EQ(EntryPoints.size(), 1u);
        for (const auto& Override : EntryPoints[0].overrides)
            EXPECT_EQ(Override.is_initialized, Override.name != "Required");
    }
}

TEST(WGSLUtils, RemapRejectsInvalidResourceMappings)
{
    const std::string WGSL = R"(
@group(0) @binding(0) var<storage, read_write> A: array<u32>;
@group(0) @binding(1) var<storage, read_write> B: array<u32>;
@compute @workgroup_size(1) fn main() { A[0] = B[0]; }
)";
    {
        TestingEnvironment::ErrorScope Errors{"Binding for variable 'B' is not found"};
        EXPECT_TRUE(RemapWGSLResourceBindings(WGSL, {{"A", {0, 2}}}, nullptr, "main").empty());
    }
    {
        TestingEnvironment::ErrorScope Errors{"Resource binding collision"};
        EXPECT_TRUE(RemapWGSLResourceBindings(WGSL, {{"A", {0, 2}}, {"B", {0, 2}}}, nullptr, "main").empty());
    }
    {
        TestingEnvironment::ErrorScope Errors{"Invalid resource array size"};
        EXPECT_TRUE(RemapWGSLResourceBindings(WGSL, {{"A", {0, 2, 0}}, {"B", {0, 3}}}, nullptr, "main").empty());
    }

    const std::string ArrayWGSL = R"(
@group(0) @binding(0) var<storage, read_write> Buffers_1: array<u32>;
@compute @workgroup_size(1) fn main() { Buffers_1[0] = 1; }
)";
    {
        TestingEnvironment::ErrorScope Errors{"Binding index overflows"};
        EXPECT_TRUE(RemapWGSLResourceBindings(ArrayWGSL, {{"Buffers", {0, 0xffffffffu, 2}}}, "_", "main").empty());
    }
    {
        TestingEnvironment::ErrorScope Errors{"Binding for variable 'Buffers_1' is not found"};
        EXPECT_TRUE(RemapWGSLResourceBindings(ArrayWGSL, {{"Buffers", {0, 0, 1}}}, "_", "main").empty());
    }
}


TEST(WGSLUtils, RemapPreservesNativeNames)
{
    // These legal WGSL identifiers shadow predeclared types, so Tint renames them.
    const std::string         WGSL = R"(
struct Uniforms { value: u32 }
@group(0) @binding(0) var<uniform> f32: Uniforms;
@group(0) @binding(1) var<storage, read_write> vec4f: array<u32>;
override texture_storage_2d_array: u32 = 1;
@id(65535) override vec4u: u32 = 2;
@compute @workgroup_size(texture_storage_2d_array) fn main() {
    vec4f[0] = f32.value + texture_storage_2d_array + vec4u;
}
@group(0) @binding(0) var<storage, read_write> OtherOutput: array<u32>;
override OtherValue: u32 = 3;
@compute @workgroup_size(1) fn other() { OtherOutput[0] = OtherValue; }
)";
    const WGSLShaderResources Original{GetRawAllocator(), WGSL, SHADER_SOURCE_LANGUAGE_WGSL,
                                       "Native names", nullptr, "main", nullptr, true, nullptr};
    ASSERT_EQ(Original.GetNumSpecConstants(), 2u);

    std::string Current = WGSL;
    for (Uint32 Pass = 0; Pass < 3; ++Pass)
    {
        SCOPED_TRACE(Pass);
        Current = RemapWGSLResourceBindings(Current, {{"f32", {1, 2 + Pass}}, {"vec4f", {2, 3 + Pass}}}, nullptr, "main");
        ASSERT_FALSE(Current.empty());
        const WGSLShaderResources Remapped{GetRawAllocator(), Current, SHADER_SOURCE_LANGUAGE_WGSL,
                                           "Remapped native names", nullptr, "main", nullptr, true, nullptr};
        ASSERT_EQ(Remapped.GetNumUBs(), 1u);
        ASSERT_EQ(Remapped.GetNumSBs(), 1u);
        EXPECT_STREQ(Remapped.GetUB(0).Name, "f32");
        EXPECT_EQ(Remapped.GetUB(0).BindGroup, 1u);
        EXPECT_EQ(Remapped.GetUB(0).BindIndex, 2 + Pass);
        EXPECT_STREQ(Remapped.GetSB(0).Name, "vec4f");
        EXPECT_EQ(Remapped.GetSB(0).BindGroup, 2u);
        EXPECT_EQ(Remapped.GetSB(0).BindIndex, 3 + Pass);
        ASSERT_EQ(Remapped.GetNumSpecConstants(), Original.GetNumSpecConstants());
        for (Uint32 i = 0; i < Original.GetNumSpecConstants(); ++i)
        {
            const auto& Before = Original.GetSpecConstant(i);
            bool        Found  = false;
            for (Uint32 j = 0; j < Remapped.GetNumSpecConstants(); ++j)
            {
                const auto& After = Remapped.GetSpecConstant(j);
                if (After.OverrideId == Before.OverrideId)
                {
                    Found = true;
                    EXPECT_STREQ(After.Name, Before.Name);
                    EXPECT_EQ(After.GetType(), Before.GetType());
                    EXPECT_TRUE(After.IsIdSpecified);
                }
            }
            EXPECT_TRUE(Found) << Before.Name;
        }
        if (::testing::Test::HasFailure())
            return;
    }
}


TEST(WGSLUtils, RemapPreservesNamesAcrossEntryPoints)
{
    std::string Current = R"(
@group(0) @binding(0) var<storage, read_write> f32: array<u32>;
@group(0) @binding(1) var<storage, read_write> vec4f: array<u32>;
override texture_storage_2d_array: u32 = 1;
@compute @workgroup_size(1) fn first() { f32[0] = texture_storage_2d_array; }
@compute @workgroup_size(1) fn second() { vec4f[0] = texture_storage_2d_array; }
)";
    for (Uint32 Pass = 0; Pass < 2; ++Pass)
    {
        SCOPED_TRACE(Pass);
        Current = RemapWGSLResourceBindings(Current, {{"f32", {1, 2}}, {"vec4f", {2, 3}}}, nullptr);
        ASSERT_FALSE(Current.empty());
        for (const char* EntryPoint : {"first", "second"})
        {
            SCOPED_TRACE(EntryPoint);
            const WGSLShaderResources Resources{GetRawAllocator(), Current, SHADER_SOURCE_LANGUAGE_WGSL,
                                                "Multiple entry points", nullptr, EntryPoint, nullptr, false, nullptr};
            ASSERT_EQ(Resources.GetNumSBs(), 1u);
            EXPECT_STREQ(Resources.GetSB(0).Name, strcmp(EntryPoint, "first") == 0 ? "f32" : "vec4f");
            ASSERT_EQ(Resources.GetNumSpecConstants(), 1u);
            EXPECT_STREQ(Resources.GetSpecConstant(0).Name, "texture_storage_2d_array");
            EXPECT_TRUE(Resources.GetSpecConstant(0).IsIdSpecified);
        }
    }
}

TEST(WGSLUtils, RemapRejectsConflictingResourceNames)
{
    const std::string WGSL = R"(
@group(0) @binding(0) var<storage, read_write> FirstOutput: array<u32>;
@group(0) @binding(0) var<storage, read_write> SecondOutput: array<u32>;
@compute @workgroup_size(1) fn first() { FirstOutput[0] = 1; }
@compute @workgroup_size(1) fn second() { SecondOutput[0] = 2; }
)";
    // Binding-point metadata cannot represent different names at one location.
    // Selecting either entry point is supported; a whole-module rewrite must not
    // silently publish the first entry point's resource name for the second one.
    TestingEnvironment::ErrorScope Errors{"Conflicting resource names at remapped binding"};
    EXPECT_TRUE(RemapWGSLResourceBindings(WGSL, {{"FirstOutput", {1, 0}}, {"SecondOutput", {1, 0}}}, nullptr).empty());
}


TEST(WGSLUtils, RemapFiltersAndPreservesSourceMetadata)
{
    std::string          WGSL = R"(
struct Uniforms { value: vec4u }
@group(0) @binding(0) var<uniform> GeneratedUniform: Uniforms;
@group(0) @binding(1) var<storage, read_write> GeneratedOutput: array<u32>;
@group(0) @binding(2) var<storage, read_write> OtherOutput: array<u32>;
@id(7) override GeneratedValue: u32 = 1;
@id(8) override OtherValue: u32 = 2;
@compute @workgroup_size(1) fn main() { GeneratedOutput[0] = GeneratedUniform.value.x + GeneratedValue; }
@compute @workgroup_size(1) fn other() { OtherOutput[0] = OtherValue; }
)";
    WGSLShaderReflection Reflection;
    auto&                Uniform = Reflection.Resources[{0, 0}];
    Uniform.Name                 = "OriginalUniform";
    Uniform.UniformBuffer        = std::make_unique<ShaderCodeBufferDescX>();
    Uniform.UniformBuffer->Size  = 16;
    Uniform.UniformBuffer->AddVariable(
        {"OriginalMember", "uint4", SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_UINT, 1, 4, 0});
    Reflection.Resources[{0, 1}].Name  = "OriginalOutput";
    Reflection.Resources[{0, 2}].Name  = "OriginalOther";
    Reflection.Resources[{9, 9}].Name  = "StaleResource";
    Reflection.SpecializationConstants = {{7, "OriginalValue"}, {8, "OtherValue"}, {9, "StaleOverride"}};
    AppendWGSLShaderReflection(WGSL, Reflection);
    AppendShaderSourceLanguageDefinition(WGSL, SHADER_SOURCE_LANGUAGE_HLSL);

    std::string Current = WGSL;
    for (Uint32 Pass = 0; Pass < 2; ++Pass)
    {
        SCOPED_TRACE(Pass);
        // Swap occupied bindings on the first pass. Original names must win over
        // both generated identifiers and the legacy uniform-buffer type alias.
        Current = RemapWGSLResourceBindings(Current,
                                            {{"OriginalUniform", {0, 1 + Pass}}, {"OriginalOutput", {0, Pass}}, {"GeneratedUniform", {3, 4}}, {"GeneratedOutput", {3, 5}}, {"Uniforms", {3, 6}}},
                                            nullptr, "main");
        ASSERT_FALSE(Current.empty());
        EXPECT_EQ(ParseShaderSourceLanguageDefinition(Current), SHADER_SOURCE_LANGUAGE_HLSL);

        WGSLShaderReflection Remapped;
        ASSERT_TRUE(ParseWGSLShaderReflection(Current, Remapped));
        ASSERT_EQ(Remapped.Resources.size(), 2u);
        ASSERT_EQ(Remapped.SpecializationConstants.size(), 1u);
        EXPECT_EQ(Remapped.SpecializationConstants.at(7), "OriginalValue");
        const auto* RemappedUniform = Remapped.Find(0, 1 + Pass);
        ASSERT_NE(RemappedUniform, nullptr);
        EXPECT_EQ(RemappedUniform->Name, "OriginalUniform");
        ASSERT_NE(RemappedUniform->UniformBuffer, nullptr);
        EXPECT_EQ(*RemappedUniform->UniformBuffer, *Uniform.UniformBuffer);
        const auto* RemappedOutput = Remapped.Find(0, Pass);
        ASSERT_NE(RemappedOutput, nullptr);
        EXPECT_EQ(RemappedOutput->Name, "OriginalOutput");

        const WGSLShaderResources Resources{GetRawAllocator(), Current, SHADER_SOURCE_LANGUAGE_WGSL,
                                            "Filtered metadata", nullptr, "main", nullptr, true, nullptr};
        ASSERT_EQ(Resources.GetNumUBs(), 1u);
        ASSERT_EQ(Resources.GetNumSBs(), 1u);
        EXPECT_STREQ(Resources.GetUB(0).Name, "OriginalUniform");
        EXPECT_STREQ(Resources.GetSB(0).Name, "OriginalOutput");
        const auto* ReflectedUniform = Resources.GetUniformBufferDesc(0);
        ASSERT_NE(ReflectedUniform, nullptr);
        EXPECT_EQ(*ReflectedUniform, *Uniform.UniformBuffer);
        ASSERT_EQ(Resources.GetNumSpecConstants(), 1u);
        EXPECT_STREQ(Resources.GetSpecConstant(0).Name, "OriginalValue");
        EXPECT_EQ(Resources.GetSpecConstant(0).OverrideId, 7u);
    }

    // Metadata is authoritative: falling back to generated/type names would hide
    // a caller using a stale resource layout after loading an archive.
    TestingEnvironment::ErrorScope Errors{"Binding for variable 'OriginalUniform' is not found"};
    EXPECT_TRUE(RemapWGSLResourceBindings(WGSL,
                                          {{"GeneratedUniform", {0, 0}}, {"Uniforms", {0, 0}}, {"OriginalOutput", {0, 1}}},
                                          nullptr, "main")
                    .empty());
}

TEST(WGSLUtils, RemapPreservesEmulatedArrayMetadata)
{
    std::string          Current = R"(
@group(0) @binding(0) var<storage, read_write> GeneratedFirst: array<u32>;
@group(0) @binding(1) var<storage, read_write> GeneratedLast: array<u32>;
@compute @workgroup_size(1) fn main() { GeneratedFirst[0] = GeneratedLast[0]; }
)";
    WGSLShaderReflection Reflection;
    Reflection.Resources[{0, 0}].Name = "Buffers_elem_1";
    Reflection.Resources[{0, 1}].Name = "Buffers_elem_3";
    AppendWGSLShaderReflection(Current, Reflection);

    for (Uint32 Pass = 0; Pass < 2; ++Pass)
    {
        SCOPED_TRACE(Pass);
        Current = RemapWGSLResourceBindings(Current, {{"Buffers", {2, 4 + Pass, 4}}}, "_elem_", "main");
        ASSERT_FALSE(Current.empty());
        WGSLShaderReflection Remapped;
        ASSERT_TRUE(ParseWGSLShaderReflection(Current, Remapped));
        ASSERT_EQ(Remapped.Resources.size(), 2u);
        const auto* First = Remapped.Find(2, 5 + Pass);
        const auto* Last  = Remapped.Find(2, 7 + Pass);
        ASSERT_NE(First, nullptr);
        ASSERT_NE(Last, nullptr);
        EXPECT_EQ(First->Name, "Buffers_elem_1");
        EXPECT_EQ(Last->Name, "Buffers_elem_3");
        const WGSLShaderResources Resources{GetRawAllocator(), Current, SHADER_SOURCE_LANGUAGE_WGSL,
                                            "Emulated array metadata", nullptr, "main", "_elem_", false, nullptr};
        ASSERT_EQ(Resources.GetNumSBs(), 1u);
        EXPECT_STREQ(Resources.GetSB(0).Name, "Buffers");
        EXPECT_EQ(Resources.GetSB(0).ArraySize, 4u);
    }
}

TEST(WGSLUtils, RemapExactNamePrecedesArrayAlias)
{
    const std::string WGSL     = R"(
@group(0) @binding(0) var<storage, read_write> Buffers_elem_2: array<u32>;
@compute @workgroup_size(1) fn main() { Buffers_elem_2[0] = 1; }
)";
    const std::string Remapped = RemapWGSLResourceBindings(WGSL,
                                                           {{"Buffers_elem_2", {1, 7}}, {"Buffers", {2, 10, 4}}}, "_elem_", "main");
    ASSERT_FALSE(Remapped.empty());
    WGSLShaderReflection Reflection;
    ASSERT_TRUE(ParseWGSLShaderReflection(Remapped, Reflection));
    ASSERT_EQ(Reflection.Resources.size(), 1u);
    const auto* Resource = Reflection.Find(1, 7);
    ASSERT_NE(Resource, nullptr);
    EXPECT_EQ(Resource->Name, "Buffers_elem_2");

    for (const char* Suffix : {static_cast<const char*>(nullptr), ""})
    {
        // Without an array suffix the base name is not an alternative binding.
        TestingEnvironment::ErrorScope Errors{"Binding for variable 'Buffers_elem_2' is not found"};
        EXPECT_TRUE(RemapWGSLResourceBindings(WGSL, {{"Buffers", {2, 10, 4}}}, Suffix, "main").empty());
    }
}

TEST(WGSLUtils, RemapSharedResourceAcrossEntryPoints)
{
    std::string Current = R"(
@group(0) @binding(0) var<storage, read_write> f32: array<u32>;
override vec4u: u32 = 1;
@compute @workgroup_size(1) fn first() { f32[0] = vec4u; }
@compute @workgroup_size(1) fn second() { f32[1] = vec4u; }
)";
    for (Uint32 Pass = 0; Pass < 2; ++Pass)
    {
        SCOPED_TRACE(Pass);
        // Both entry points report the same resource and override. Their shared
        // destination must not be rejected as a collision or duplicate metadata.
        Current = RemapWGSLResourceBindings(Current, {{"f32", {1, 2 + Pass}}}, nullptr);
        ASSERT_FALSE(Current.empty());
        WGSLShaderReflection Reflection;
        ASSERT_TRUE(ParseWGSLShaderReflection(Current, Reflection));
        ASSERT_EQ(Reflection.Resources.size(), 1u);
        ASSERT_EQ(Reflection.SpecializationConstants.size(), 1u);
        for (const char* EntryPoint : {"first", "second"})
        {
            const WGSLShaderResources Resources{GetRawAllocator(), Current, SHADER_SOURCE_LANGUAGE_WGSL,
                                                "Shared resource", nullptr, EntryPoint, nullptr, false, nullptr};
            ASSERT_EQ(Resources.GetNumSBs(), 1u);
            EXPECT_STREQ(Resources.GetSB(0).Name, "f32");
            EXPECT_EQ(Resources.GetSB(0).BindGroup, 1u);
            EXPECT_EQ(Resources.GetSB(0).BindIndex, 2 + Pass);
            ASSERT_EQ(Resources.GetNumSpecConstants(), 1u);
            EXPECT_STREQ(Resources.GetSpecConstant(0).Name, "vec4u");
        }
    }
}

TEST(WGSLUtils, RemapRejectsMalformedMetadata)
{
    // The source is valid WGSL; a bad metadata comment must fail the remap
    // instead of silently losing application-facing names.
    const std::string WGSL = "@compute @workgroup_size(1) fn main() {}\n"
                             "/*$DILIGENT_WGSL_REFLECTION=1 0 0 1 zz 0*/";
    std::string                    EntryPoint = "main";
    TestingEnvironment::ErrorScope Errors{"Invalid WGSL reflection metadata"};
    EXPECT_TRUE(RemapWGSLResourceBindings(WGSL, {}, nullptr, EntryPoint.c_str(), &EntryPoint).empty());
    EXPECT_EQ(EntryPoint, "main");
}


TEST(WGSLUtils, RemapLegacyBufferAliases)
{
    // Older archives have no source metadata: generated variable names may use
    // the buffer type name, an extra suffix, or a type with an "_atomic" suffix.
    std::string Current = R"(
struct Constants { value: u32 }
struct Buffers_elem_2 { values: array<u32> }
struct Counters_atomic { values: array<atomic<u32>> }
@group(0) @binding(0) var<uniform> GeneratedUniform: Constants;
@group(0) @binding(1) var<storage, read_write> Buffers_elem_2_1: Buffers_elem_2;
@group(0) @binding(2) var<storage, read_write> Counters_1: Counters_atomic;
@compute @workgroup_size(1) fn main() {
    Buffers_elem_2_1.values[0] = GeneratedUniform.value;
    atomicStore(&Counters_1.values[0], GeneratedUniform.value);
}
)";
    for (Uint32 Pass = 0; Pass < 2; ++Pass)
    {
        SCOPED_TRACE(Pass);
        Current = RemapWGSLResourceBindings(Current,
                                            {{"Constants", {1, Pass}}, {"Buffers", {1, 3 + Pass, 3}}, {"Counters", {2, 5 + Pass}}},
                                            "_elem_", "main");
        ASSERT_FALSE(Current.empty());
        WGSLShaderReflection Reflection;
        ASSERT_TRUE(ParseWGSLShaderReflection(Current, Reflection));
        ASSERT_EQ(Reflection.Resources.size(), 3u);
        const auto* Uniform = Reflection.Find(1, Pass);
        const auto* Buffer  = Reflection.Find(1, 5 + Pass);
        const auto* Atomic  = Reflection.Find(2, 5 + Pass);
        ASSERT_NE(Uniform, nullptr);
        ASSERT_NE(Buffer, nullptr);
        ASSERT_NE(Atomic, nullptr);
        EXPECT_EQ(Uniform->Name, "Constants");
        EXPECT_EQ(Buffer->Name, "Buffers_elem_2");
        EXPECT_EQ(Atomic->Name, "Counters");
    }
}

} // namespace
