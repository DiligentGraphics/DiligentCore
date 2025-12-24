/*
 *  Copyright 2025 Diligent Graphics LLC
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

#include "SPIRVShaderResources.hpp"
#include "GLSLangUtils.hpp"
#include "DXCompiler.hpp"
#include "DefaultShaderSourceStreamFactory.h"
#include "RefCntAutoPtr.hpp"
#include "EngineMemory.h"
#include "BasicFileSystem.hpp"

#include <unordered_map>
#include <string>
#include <vector>

#include "TestingEnvironment.hpp"

// Forward declaration for ConvertUBOToPushConstants from SPIRVTools.hpp
// We cannot include SPIRVTools.hpp directly because it depends on spirv-tools headers
namespace Diligent
{
std::vector<uint32_t> ConvertUBOToPushConstants(const std::vector<uint32_t>& SPIRV,
                                                const std::string&           BlockName);
}

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

class SPIRVShaderResourcesTest : public ::testing::Test
{
public:
    static std::unique_ptr<IDXCompiler> DXCompiler;

protected:
    static void SetUpTestSuite()
    {
        GLSLangUtils::InitializeGlslang();

        DXCompiler = CreateDXCompiler(DXCompilerTarget::Vulkan, 0, nullptr);
    }

    static void TearDownTestSuite()
    {
        GLSLangUtils::FinalizeGlslang();

        DXCompiler.reset();
    }
};

std::unique_ptr<IDXCompiler> SPIRVShaderResourcesTest::DXCompiler;

struct SPIRVShaderResourceRefAttribs
{
    const char* const                              Name;
    const Uint16                                   ArraySize;
    const SPIRVShaderResourceAttribs::ResourceType Type;
    const RESOURCE_DIMENSION                       ResourceDim;
    const Uint8                                    IsMS;
    const Uint32                                   BufferStaticSize;
    const Uint32                                   BufferStride;
};

std::vector<unsigned int> LoadSPIRVFromHLSL(const char*     FilePath,
                                            SHADER_TYPE     ShaderType,
                                            SHADER_COMPILER Compiler = SHADER_COMPILER_DEFAULT)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceStreamFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/SPIRV", &pShaderSourceStreamFactory);
    if (!pShaderSourceStreamFactory)
        return {};

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.FilePath       = FilePath;
    ShaderCI.Desc           = {"SPIRV test shader", ShaderType};
    ShaderCI.EntryPoint     = "main";

    ShaderCI.pShaderSourceStreamFactory = pShaderSourceStreamFactory;

    std::vector<unsigned int> SPIRV;

    if (Compiler == SHADER_COMPILER_DXC)
    {
        if (!SPIRVShaderResourcesTest::DXCompiler || !SPIRVShaderResourcesTest::DXCompiler->IsLoaded())
        {
            UNEXPECTED("Test should be skipped if DXCompiler is not available");
            return {};
        }

        RefCntAutoPtr<IDataBlob> pCompilerOutput;
        SPIRVShaderResourcesTest::DXCompiler->Compile(ShaderCI, ShaderVersion{6, 0}, nullptr, nullptr, &SPIRV, &pCompilerOutput);

        if (pCompilerOutput && pCompilerOutput->GetSize() > 0)
        {
            const char* CompilerOutput = static_cast<const char*>(pCompilerOutput->GetConstDataPtr());
            if (*CompilerOutput != 0)
                LOG_INFO_MESSAGE("DXC compiler output:\n", CompilerOutput);
        }
    }
    else
    {
        SPIRV = GLSLangUtils::HLSLtoSPIRV(ShaderCI, GLSLangUtils::SpirvVersion::Vk100, nullptr, nullptr);
    }

    return SPIRV;
}

std::vector<unsigned int> LoadSPIRVFromGLSL(const char* FilePath, SHADER_TYPE ShaderType = SHADER_TYPE_PIXEL)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceStreamFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/SPIRV", &pShaderSourceStreamFactory);
    if (!pShaderSourceStreamFactory)
        return {};

    RefCntAutoPtr<IFileStream> pShaderSourceStream;
    pShaderSourceStreamFactory->CreateInputStream(FilePath, &pShaderSourceStream);
    if (!pShaderSourceStream)
        return {};

    size_t ShaderSourceSize = pShaderSourceStream->GetSize();
    if (ShaderSourceSize == 0)
        return {};

    std::vector<char> ShaderSource(ShaderSourceSize);
    pShaderSourceStream->Read(ShaderSource.data(), ShaderSourceSize);

    // Ray tracing shaders require Vulkan 1.1 or higher SPIR-V version
    GLSLangUtils::SpirvVersion Version = GLSLangUtils::SpirvVersion::Vk100;
    if (ShaderType == SHADER_TYPE_RAY_GEN ||
        ShaderType == SHADER_TYPE_RAY_MISS ||
        ShaderType == SHADER_TYPE_RAY_CLOSEST_HIT ||
        ShaderType == SHADER_TYPE_RAY_ANY_HIT ||
        ShaderType == SHADER_TYPE_RAY_INTERSECTION ||
        ShaderType == SHADER_TYPE_CALLABLE)
    {
        Version = GLSLangUtils::SpirvVersion::Vk110_Spirv14;
    }

    GLSLangUtils::GLSLtoSPIRVAttribs Attribs;
    Attribs.ShaderType                 = ShaderType;
    Attribs.ShaderSource               = ShaderSource.data();
    Attribs.SourceCodeLen              = static_cast<int>(ShaderSourceSize);
    Attribs.pShaderSourceStreamFactory = pShaderSourceStreamFactory;
    Attribs.Version                    = Version;
    Attribs.AssignBindings             = true;

    return GLSLangUtils::GLSLtoSPIRV(Attribs);
}

void CompileSPIRV(const char*                FilePath,
                  SHADER_COMPILER            Compiler,
                  SHADER_TYPE                ShaderType,
                  SHADER_SOURCE_LANGUAGE     SourceLanguage,
                  std::vector<unsigned int>& SPIRV)
{
    if (Compiler == SHADER_COMPILER_DXC)
    {
        VERIFY(SourceLanguage == SHADER_SOURCE_LANGUAGE_HLSL, "DXC only supports HLSL");
        if (!SPIRVShaderResourcesTest::DXCompiler || !SPIRVShaderResourcesTest::DXCompiler->IsLoaded())
        {
            GTEST_SKIP() << "DXC compiler is not available";
        }
    }

    SPIRV = (SourceLanguage == SHADER_SOURCE_LANGUAGE_GLSL) ?
        LoadSPIRVFromGLSL(FilePath, ShaderType) :
        LoadSPIRVFromHLSL(FilePath, ShaderType, Compiler);
    ASSERT_FALSE(SPIRV.empty()) << "Failed to compile shader: " << FilePath;
}

void TestSPIRVResources(const char*                                                  FilePath,
                        const std::vector<SPIRVShaderResourceRefAttribs>&            RefResources,
                        SHADER_COMPILER                                              Compiler,
                        SHADER_TYPE                                                  ShaderType         = SHADER_TYPE_PIXEL,
                        SHADER_SOURCE_LANGUAGE                                       SourceLanguage     = SHADER_SOURCE_LANGUAGE_HLSL,
                        const std::function<void(std::vector<unsigned int>& SPIRV)>& PatchSPIRVCallback = nullptr)
{
    std::vector<unsigned int> SPIRV;
    ASSERT_NO_FATAL_FAILURE(CompileSPIRV(FilePath, Compiler, ShaderType, SourceLanguage, SPIRV));

    if (::testing::Test::IsSkipped())
        return;

    if (PatchSPIRVCallback)
    {
        PatchSPIRVCallback(SPIRV);
        ASSERT_FALSE(SPIRV.empty()) << "Failed to patch shader: " << FilePath;
    }

    ShaderDesc ShaderDesc;
    ShaderDesc.Name       = "SPIRVResources test";
    ShaderDesc.ShaderType = ShaderType;

    std::string          EntryPoint;
    SPIRVShaderResources Resources{
        GetRawAllocator(),
        SPIRV,
        ShaderDesc,
        nullptr,
        false, // LoadShaderStageInputs
        false, // LoadUniformBufferReflection
        EntryPoint,
    };

    LOG_INFO_MESSAGE("SPIRV Resources:\n", Resources.DumpResources());

    EXPECT_EQ(size_t{Resources.GetTotalResources()}, RefResources.size());

    std::unordered_map<std::string, const SPIRVShaderResourceRefAttribs*> RefResourcesMap;
    for (const SPIRVShaderResourceRefAttribs& RefRes : RefResources)
    {
        RefResourcesMap[RefRes.Name] = &RefRes;
    }

    for (Uint32 i = 0; i < Resources.GetTotalResources(); ++i)
    {
        const auto& Res     = const_cast<const SPIRVShaderResources&>(Resources).GetResource(i);
        const auto* pRefRes = RefResourcesMap[Res.Name];
        ASSERT_NE(pRefRes, nullptr) << "Resource '" << Res.Name << "' is not found in the reference list";

        EXPECT_STREQ(SPIRVShaderResourceAttribs::ResourceTypeToString(Res.Type), SPIRVShaderResourceAttribs::ResourceTypeToString(pRefRes->Type)) << Res.Name;
        EXPECT_EQ(Res.ArraySize, pRefRes->ArraySize) << Res.Name;
        EXPECT_EQ(Res.ResourceDim, pRefRes->ResourceDim) << Res.Name;
        EXPECT_EQ(Res.IsMS, pRefRes->IsMS) << Res.Name;
        EXPECT_EQ(Res.BufferStaticSize, pRefRes->BufferStaticSize) << Res.Name;
        EXPECT_EQ(Res.BufferStride, pRefRes->BufferStride) << Res.Name;

        if (Res.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer ||
            Res.Type == SPIRVShaderResourceAttribs::ResourceType::PushConstant)
        {
            EXPECT_EQ(Res.GetInlineConstantCountOrThrow(FilePath), pRefRes->BufferStaticSize / 4) << Res.Name;
        }
    }
}

using SPIRVResourceType = SPIRVShaderResourceAttribs::ResourceType;

void TestUniformBuffers(SHADER_COMPILER Compiler)
{
    TestSPIRVResources("UniformBuffers.psh",
                       {
                           SPIRVShaderResourceRefAttribs{"CB1", 1, SPIRVResourceType::UniformBuffer, RESOURCE_DIM_BUFFER, 0, 48, 0},
                           SPIRVShaderResourceRefAttribs{"CB2", 1, SPIRVResourceType::UniformBuffer, RESOURCE_DIM_BUFFER, 0, 16, 0},
                           SPIRVShaderResourceRefAttribs{"CB3", 1, SPIRVResourceType::UniformBuffer, RESOURCE_DIM_BUFFER, 0, 32, 0},
                           SPIRVShaderResourceRefAttribs{"CB4", 1, SPIRVResourceType::UniformBuffer, RESOURCE_DIM_BUFFER, 0, 32, 0},
                       },
                       Compiler);
}

TEST_F(SPIRVShaderResourcesTest, UniformBuffers_GLSLang)
{
    TestUniformBuffers(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, UniformBuffers_DXC)
{
    TestUniformBuffers(SHADER_COMPILER_DXC);
}

void TestConvertUBOToPushConstant(SHADER_COMPILER Compiler)
{
    const std::vector<SPIRVShaderResourceRefAttribs> BaseRefAttribs = {
        SPIRVShaderResourceRefAttribs{"CB1", 1, SPIRVResourceType::UniformBuffer, RESOURCE_DIM_BUFFER, 0, 48, 0},
        SPIRVShaderResourceRefAttribs{"CB2", 1, SPIRVResourceType::UniformBuffer, RESOURCE_DIM_BUFFER, 0, 16, 0},
        SPIRVShaderResourceRefAttribs{"CB3", 1, SPIRVResourceType::UniformBuffer, RESOURCE_DIM_BUFFER, 0, 32, 0},
        SPIRVShaderResourceRefAttribs{"CB4", 1, SPIRVResourceType::UniformBuffer, RESOURCE_DIM_BUFFER, 0, 32, 0},
    };

    //Try to patch uniform buffer block to push constants one for each
    for (size_t i = 0; i < BaseRefAttribs.size(); ++i)
    {
        const char* PatchedAttribName = BaseRefAttribs[i].Name;

        std::vector<SPIRVShaderResourceRefAttribs> PatchedRefAttribs;
        PatchedRefAttribs.reserve(BaseRefAttribs.size());
        for (size_t j = 0; j < BaseRefAttribs.size(); ++j)
        {
            const SPIRVShaderResourceRefAttribs& RefAttrib = BaseRefAttribs[j];
            // Build a new attrib with Type changed to PushConstant, other members remain the same
            PatchedRefAttribs.push_back({RefAttrib.Name,
                                         RefAttrib.ArraySize,
                                         i == j ? SPIRVResourceType::PushConstant : RefAttrib.Type,
                                         RefAttrib.ResourceDim,
                                         RefAttrib.IsMS,
                                         RefAttrib.BufferStaticSize,
                                         RefAttrib.BufferStride});
        }

        TestSPIRVResources("UniformBuffers.psh",
                           PatchedRefAttribs,
                           Compiler,
                           SHADER_TYPE_PIXEL,
                           SHADER_SOURCE_LANGUAGE_HLSL,
                           [PatchedAttribName](std::vector<unsigned int>& SPIRV) {
                               SPIRV = ConvertUBOToPushConstants(SPIRV, PatchedAttribName);
                           });
    }
}

TEST_F(SPIRVShaderResourcesTest, ConvertUBOToPushConstant_GLSLang)
{
    TestConvertUBOToPushConstant(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, ConvertUBOToPushConstant_DXC)
{
    TestConvertUBOToPushConstant(SHADER_COMPILER_DXC);
}

void TestConvertUBOToPushConstant_InvalidBlockName(SHADER_COMPILER Compiler)
{
    //"CB5" is not available in given HLSL thus cannot be patched with ConvertUBOToPushConstants.
    std::string PatchedAttribName = "CB5";

    std::vector<unsigned int> SPIRV;
    ASSERT_NO_FATAL_FAILURE(CompileSPIRV("UniformBuffers.psh", Compiler, SHADER_TYPE_PIXEL, SHADER_SOURCE_LANGUAGE_HLSL, SPIRV));
    if (::testing::Test::IsSkipped())
        return;

    std::vector<unsigned int> PatchedSPIRV = ConvertUBOToPushConstants(SPIRV, PatchedAttribName);
    EXPECT_EQ(SPIRV, PatchedSPIRV);
}

TEST_F(SPIRVShaderResourcesTest, ConvertUBOToPushConstant_InvalidBlockName_GLSLang)
{
    TestConvertUBOToPushConstant_InvalidBlockName(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, ConvertUBOToPushConstant_InvalidBlockName_DXC)
{
    TestConvertUBOToPushConstant_InvalidBlockName(SHADER_COMPILER_DXC);
}

void TestConvertUBOToPushConstant_InvalidResourceType(SHADER_COMPILER Compiler)
{
    //"g_ROBuffer" is a ROStorageBuffer and cannot be patched with ConvertUBOToPushConstants.
    std::string PatchedAttribName = "g_ROBuffer";

    std::vector<unsigned int> SPIRV;
    ASSERT_NO_FATAL_FAILURE(CompileSPIRV("StorageBuffers.psh", Compiler, SHADER_TYPE_PIXEL, SHADER_SOURCE_LANGUAGE_HLSL, SPIRV));
    if (::testing::Test::IsSkipped())
        return;

    std::vector<unsigned int> PatchedSPIRV = ConvertUBOToPushConstants(SPIRV, PatchedAttribName);
    EXPECT_EQ(SPIRV, PatchedSPIRV);
}

TEST_F(SPIRVShaderResourcesTest, ConvertUBOToPushConstant_InvalidResourceType_GLSLang)
{
    TestConvertUBOToPushConstant_InvalidResourceType(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, ConvertUBOToPushConstant_InvalidResourceType_DXC)
{
    TestConvertUBOToPushConstant_InvalidResourceType(SHADER_COMPILER_DXC);
}

void TestStorageBuffers(SHADER_COMPILER Compiler)
{
    TestSPIRVResources("StorageBuffers.psh",
                       {
                           // StructuredBuffers have BufferStaticSize=0 (runtime array) and BufferStride is the element size
                           SPIRVShaderResourceRefAttribs{"g_ROBuffer", 1, SPIRVResourceType::ROStorageBuffer, RESOURCE_DIM_BUFFER, 0, 0, 32},
                           SPIRVShaderResourceRefAttribs{"g_RWBuffer", 1, SPIRVResourceType::RWStorageBuffer, RESOURCE_DIM_BUFFER, 0, 0, 64},
                           // ByteAddressBuffers also have BufferStaticSize=0 and BufferStride=4 (uint size)
                           SPIRVShaderResourceRefAttribs{"g_ROAtomicBuffer", 1, SPIRVResourceType::ROStorageBuffer, RESOURCE_DIM_BUFFER, 0, 0, 4},
                           SPIRVShaderResourceRefAttribs{"g_RWAtomicBuffer", 1, SPIRVResourceType::RWStorageBuffer, RESOURCE_DIM_BUFFER, 0, 0, 4},
                       },
                       Compiler);
}

TEST_F(SPIRVShaderResourcesTest, StorageBuffers_GLSLang)
{
    TestStorageBuffers(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, StorageBuffers_DXC)
{
    TestStorageBuffers(SHADER_COMPILER_DXC);
}

void TestTexelBuffers(SHADER_COMPILER Compiler)
{
    TestSPIRVResources("TexelBuffers.psh",
                       {
                           SPIRVShaderResourceRefAttribs{"g_UniformTexelBuffer", 1, SPIRVResourceType::UniformTexelBuffer, RESOURCE_DIM_BUFFER, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_StorageTexelBuffer", 1, SPIRVResourceType::StorageTexelBuffer, RESOURCE_DIM_BUFFER, 0, 0, 0},
                       },
                       Compiler);
}

TEST_F(SPIRVShaderResourcesTest, TexelBuffers_GLSLang)
{
    TestTexelBuffers(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, TexelBuffers_DXC)
{
    TestTexelBuffers(SHADER_COMPILER_DXC);
}

void TestTextures(SHADER_COMPILER Compiler)
{
    TestSPIRVResources("Textures.psh",
                       {
                           // When textures and samplers are declared separately in HLSL, they are compiled as separate_images
                           // instead of sampled_images. This is the correct behavior for separate sampler/texture declarations.
                           SPIRVShaderResourceRefAttribs{"g_SampledImage", 1, SPIRVResourceType::SeparateImage, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_SampledImageMS", 1, SPIRVResourceType::SeparateImage, RESOURCE_DIM_TEX_2D, 1, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_SampledImage3D", 1, SPIRVResourceType::SeparateImage, RESOURCE_DIM_TEX_3D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_SampledImageCube", 1, SPIRVResourceType::SeparateImage, RESOURCE_DIM_TEX_CUBE, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_Sampler", 1, SPIRVResourceType::SeparateSampler, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_SeparateImage", 1, SPIRVResourceType::SeparateImage, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           // Combined sampler: g_Texture and g_Texture_sampler
                           // Note: Even with CombinedSamplerSuffix, SPIRV may still classify them as separate_images
                           // if they are declared separately. The CombinedSamplerSuffix is mainly used for naming convention.
                           SPIRVShaderResourceRefAttribs{"g_Texture", 1, SPIRVResourceType::SeparateImage, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_Texture_sampler", 1, SPIRVResourceType::SeparateSampler, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                       },
                       Compiler);
}

TEST_F(SPIRVShaderResourcesTest, Textures_GLSLang)
{
    TestTextures(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, Textures_DXC)
{
    TestTextures(SHADER_COMPILER_DXC);
}

void TestStorageImages(SHADER_COMPILER Compiler)
{
    TestSPIRVResources("StorageImages.psh",
                       {
                           // Note: HLSL does not support RWTextureCube, so we only test 2D, 2DArray, and 3D storage images
                           SPIRVShaderResourceRefAttribs{"g_RWImage2D", 1, SPIRVResourceType::StorageImage, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_RWImage2DArray", 1, SPIRVResourceType::StorageImage, RESOURCE_DIM_TEX_2D_ARRAY, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_RWImage3D", 1, SPIRVResourceType::StorageImage, RESOURCE_DIM_TEX_3D, 0, 0, 0},
                       },
                       Compiler);
}

TEST_F(SPIRVShaderResourcesTest, StorageImages_GLSLang)
{
    TestStorageImages(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, StorageImages_DXC)
{
    TestStorageImages(SHADER_COMPILER_DXC);
}

TEST_F(SPIRVShaderResourcesTest, AtomicCounters_GLSLang)
{
    // Use GLSL for atomic counters. Note: Vulkan does not support atomic_uint (AtomicCounter storage class).
    // We use a storage buffer with atomic operations to simulate atomic counters.
    // This will be reflected as RWStorageBuffer, not AtomicCounter.
    // The resource name is the buffer block name (AtomicCounterBuffer), not the instance name (g_AtomicCounter).
    TestSPIRVResources("AtomicCounters.glsl",
                       {
                           SPIRVShaderResourceRefAttribs{"AtomicCounterBuffer", 1, SPIRVResourceType::RWStorageBuffer, RESOURCE_DIM_BUFFER, 0, 4, 0},
                       },
                       SHADER_COMPILER_GLSLANG,
                       SHADER_TYPE_PIXEL,
                       SHADER_SOURCE_LANGUAGE_GLSL);
}

void TestInputAttachments(SHADER_COMPILER Compiler)
{
    TestSPIRVResources("InputAttachments.psh",
                       {
                           SPIRVShaderResourceRefAttribs{"g_InputAttachment", 1, SPIRVResourceType::InputAttachment, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                       },
                       Compiler);
}

TEST_F(SPIRVShaderResourcesTest, InputAttachments_GLSLang)
{
    TestInputAttachments(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, InputAttachments_DXC)
{
    TestInputAttachments(SHADER_COMPILER_DXC);
}

TEST_F(SPIRVShaderResourcesTest, AccelerationStructures_GLSLang)
{
    // Use GLSL for acceleration structures since HLSLtoSPIRV doesn't support raytracing shaders
    // Acceleration structures are used in raytracing shaders, so we use SHADER_TYPE_RAY_GEN
    // The ray gen shader uses traceRayEXT with g_AccelStruct to ensure it's not optimized away
    TestSPIRVResources("AccelerationStructures.glsl",
                       {
                           SPIRVShaderResourceRefAttribs{"g_AccelStruct", 1, SPIRVResourceType::AccelerationStructure, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                       },
                       SHADER_COMPILER_GLSLANG,
                       SHADER_TYPE_RAY_GEN,
                       SHADER_SOURCE_LANGUAGE_GLSL);
}

void TestPushConstants(SHADER_COMPILER Compiler)
{
    // Push constant ArraySize represents the number of 32-bit words, not array elements
    // PushConstants struct: float4x4 (16 floats) + float4 (4 floats) + float2 (2 floats) + float (1 float) + uint (1 uint)
    // Total: 16 + 4 + 2 + 1 + 1 = 24 floats/uints = 24 * 4 bytes = 96 bytes = 24 words
    TestSPIRVResources("PushConstants.psh",
                       {
                           SPIRVShaderResourceRefAttribs{"PushConstants", 1, SPIRVResourceType::PushConstant, RESOURCE_DIM_BUFFER, 0, 96, 0},
                       },
                       Compiler);
}

TEST_F(SPIRVShaderResourcesTest, PushConstants_GLSLang)
{
    TestPushConstants(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, PushConstants_DXC)
{
    TestPushConstants(SHADER_COMPILER_DXC);
}

void TestMixedResources(SHADER_COMPILER Compiler)
{
    TestSPIRVResources("MixedResources.psh",
                       {
                           // UniformBuff: float4x4 (64 bytes) + float4 (16 bytes) = 80 bytes
                           SPIRVShaderResourceRefAttribs{"UniformBuff", 1, SPIRVResourceType::UniformBuffer, RESOURCE_DIM_BUFFER, 0, 80, 0},
                           // ROStorageBuff: StructuredBuffer<BufferData> where BufferData = float4[4] = 64 bytes
                           // StructuredBuffers have BufferStaticSize=0 (runtime array) and BufferStride is the element size
                           SPIRVShaderResourceRefAttribs{"ROStorageBuff", 1, SPIRVResourceType::ROStorageBuffer, RESOURCE_DIM_BUFFER, 0, 0, 64},
                           // RWStorageBuff: same as ROStorageBuff
                           SPIRVShaderResourceRefAttribs{"RWStorageBuff", 1, SPIRVResourceType::RWStorageBuffer, RESOURCE_DIM_BUFFER, 0, 0, 64},
                           // SampledTex: When Texture2D and SamplerState are declared separately, they are compiled as SeparateImage
                           SPIRVShaderResourceRefAttribs{"SampledTex", 1, SPIRVResourceType::SeparateImage, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"StorageTex", 1, SPIRVResourceType::StorageImage, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"Sampler", 1, SPIRVResourceType::SeparateSampler, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                           // PushConstants: float2 (2 floats) + float (1 float) + uint (1 uint) = 4 words = 16 bytes
                           SPIRVShaderResourceRefAttribs{"PushConstants", 1, SPIRVResourceType::PushConstant, RESOURCE_DIM_BUFFER, 0, 16, 0},
                       },
                       Compiler);
}

TEST_F(SPIRVShaderResourcesTest, MixedResources_GLSLang)
{
    TestMixedResources(SHADER_COMPILER_GLSLANG);
}

TEST_F(SPIRVShaderResourcesTest, MixedResources_DXC)
{
    TestMixedResources(SHADER_COMPILER_DXC);
}

} // namespace
