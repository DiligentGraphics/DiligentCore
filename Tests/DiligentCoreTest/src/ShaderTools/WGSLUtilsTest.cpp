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

void TestResourceRemapping(const char* FilePath, const WGSLResourceMapping& ResRemapping)
{
    const auto WGSL = HLSLtoWGLS(FilePath);
    ASSERT_FALSE(WGSL.empty());

    const auto RemappedWGSL = RamapWGSLResourceBindings(WGSL, ResRemapping);
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
            auto RemappedBindingIt = ResRemapping.find(Binding.variable_name);
            if (RemappedBindingIt == ResRemapping.end() &&
                (Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kUniformBuffer ||
                 Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kStorageBuffer ||
                 Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kReadOnlyStorageBuffer))
            {
                // Search variable by type
                for (const auto* Variable : Program.AST().GlobalVariables())
                {
                    if (Variable->name->symbol.Name() == Binding.variable_name)
                    {
                        RemappedBindingIt = ResRemapping.find(Variable->type->identifier->symbol.Name());
                    }
                }
            }

            if (RemappedBindingIt == ResRemapping.end())
            {
                GTEST_FAIL() << "Unable to find remapping for resource '" << Binding.variable_name << "'";
            }
            else
            {
                EXPECT_EQ(Binding.bind_group, RemappedBindingIt->second.Group) << "Bind group mismatch (" << Binding.bind_group << " vs " << RemappedBindingIt->second.Group << " for resource '" << Binding.variable_name << "'";
                EXPECT_EQ(Binding.binding, RemappedBindingIt->second.Index) << "Binding index mismatch (" << Binding.binding << " vs " << RemappedBindingIt->second.Index << " for resource '" << Binding.variable_name << "'";
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
                          });
}

} // namespace
