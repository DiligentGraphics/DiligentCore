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

#include "WGSLUtils.hpp"
#include "GLSLangUtils.hpp"
#include "DefaultShaderSourceStreamFactory.h"
#include "RefCntAutoPtr.hpp"

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

TEST(WGSLUtils, GetWGSLEmulatedArrayElementInfo)
{
    EXPECT_EQ(GetWGSLEmulatedArrayElement("", ""), WGSLEmulatedResourceArrayElement{});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("", "_"), WGSLEmulatedResourceArrayElement{});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D", ""), WGSLEmulatedResourceArrayElement{"Tex2D"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D", "_"), WGSLEmulatedResourceArrayElement{"Tex2D"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_X", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_X"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_123x", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_123x"});
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_123_", "_"), WGSLEmulatedResourceArrayElement{"Tex2D_123_"});

    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_0", "_"), WGSLEmulatedResourceArrayElement("Tex2D", 0));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_1", "_"), WGSLEmulatedResourceArrayElement("Tex2D", 1));
    EXPECT_EQ(GetWGSLEmulatedArrayElement("Tex2D_17", "_"), WGSLEmulatedResourceArrayElement("Tex2D", 17));
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

    const auto RemappedWGSL = RamapWGSLResourceBindings(WGSL, ResRemapping, "_");
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

} // namespace
