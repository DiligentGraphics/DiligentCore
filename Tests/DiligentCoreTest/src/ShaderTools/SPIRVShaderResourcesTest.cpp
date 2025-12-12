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
#include "DefaultShaderSourceStreamFactory.h"
#include "RefCntAutoPtr.hpp"
#include "EngineMemory.h"
#include "BasicFileSystem.hpp"

#include <unordered_map>
#include <string>

#include "TestingEnvironment.hpp"
#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

struct SPIRVShaderResourceRefAttribs
{
    // clang-format off

/*  0  */const char* const                                  Name;
/*  8  */const Uint16                                       ArraySize;
/* 10  */const SPIRVShaderResourceAttribs::ResourceType     Type;
/* 11.0*/const Uint8                                        ResourceDim  : 7; // RESOURCE_DIMENSION
/* 11.7*/const Uint8                                        IsMS         : 1;

      // Offset in SPIRV words (uint32_t) of binding & descriptor set decorations in SPIRV binary
/* 12 */const uint32_t                                      BindingDecorationOffset;
/* 16 */const uint32_t                                      DescriptorSetDecorationOffset;

/* 20 */const Uint32                                        BufferStaticSize;
/* 24 */const Uint32                                        BufferStride;
/* 28 */
/* 32 */ // End of structure

    // clang-format on
    //
    // Test-only constructor for creating reference resources
    SPIRVShaderResourceRefAttribs(const char*                              _Name,
                                  SPIRVShaderResourceAttribs::ResourceType _Type,
                                  Uint16                                   _ArraySize,
                                  RESOURCE_DIMENSION                       _ResourceDim,
                                  Uint8                                    _IsMS,
                                  Uint32                                   _BufferStaticSize,
                                  Uint32                                   _BufferStride) noexcept :
        // clang-format off
    Name                          {_Name},
    ArraySize                     {_ArraySize},
    Type                          {_Type},
    ResourceDim                   {static_cast<Uint8>(_ResourceDim)},
    IsMS                          {_IsMS},
    BindingDecorationOffset       {0}, // Not used in tests
    DescriptorSetDecorationOffset {0}, // Not used in tests
    BufferStaticSize              {_BufferStaticSize},
    BufferStride                  {_BufferStride}
    // clang-format on
    {}
};

static_assert(sizeof(SPIRVShaderResourceRefAttribs) == sizeof(SPIRVShaderResourceAttribs), "Size of SPIRVShaderResourceRefAttribs struct must be equal to SPIRVShaderResourceAttribs");

std::vector<unsigned int> LoadSPIRVFromHLSL(const char* FilePath, SHADER_TYPE ShaderType = SHADER_TYPE_PIXEL)
{
    std::vector<unsigned int> SPIRV;

#if DILIGENT_NO_HLSL
    // When DILIGENT_NO_HLSL is defined, use {FilePath}.spv as raw SPIRV shader source.
    // i.e. FilePath="Textures.psh", then we load RAW SPIRV from "Textures.psh.spv"
    std::string SPIRVFilePath = std::string(FilePath) + ".spv";

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceStreamFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/SPIRV", &pShaderSourceStreamFactory);
    if (!pShaderSourceStreamFactory)
        return {};

    RefCntAutoPtr<IFileStream> pSPIRVStream;
    pShaderSourceStreamFactory->CreateInputStream(SPIRVFilePath.c_str(), &pSPIRVStream);
    if (!pSPIRVStream)
        return {};

    const size_t ByteCodeSize = pSPIRVStream->GetSize();
    if (ByteCodeSize == 0 || ByteCodeSize % 4 != 0)
        return {};

    SPIRV.resize(ByteCodeSize / 4);
    pSPIRVStream->Read(SPIRV.data(), ByteCodeSize);

#else

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.FilePath       = FilePath;
    ShaderCI.Desc           = {"SPIRV test shader", ShaderType};
    ShaderCI.EntryPoint     = "main";

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceStreamFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/SPIRV", &pShaderSourceStreamFactory);
    if (!pShaderSourceStreamFactory)
        return {};

    ShaderCI.pShaderSourceStreamFactory = pShaderSourceStreamFactory;

    GLSLangUtils::InitializeGlslang();
    SPIRV = GLSLangUtils::HLSLtoSPIRV(ShaderCI, GLSLangUtils::SpirvVersion::Vk100, nullptr, nullptr);
    GLSLangUtils::FinalizeGlslang();
#endif

    return SPIRV;
}

std::vector<unsigned int> LoadSPIRVFromGLSL(const char* FilePath, SHADER_TYPE ShaderType = SHADER_TYPE_PIXEL)
{
    std::vector<unsigned int> SPIRV;

#if DILIGENT_NO_GLSL
    // When DILIGENT_NO_GLSL is defined, use {FilePath}.spv as raw SPIRV shader source.
    // i.e. FilePath="AtomicCounters.glsl", then we load RAW SPIRV from "AtomicCounters.glsl.spv"
    std::string SPIRVFilePath = std::string(FilePath) + ".spv";

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceStreamFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/SPIRV", &pShaderSourceStreamFactory);
    if (!pShaderSourceStreamFactory)
        return {};

    RefCntAutoPtr<IFileStream> pSPIRVStream;
    pShaderSourceStreamFactory->CreateInputStream(SPIRVFilePath.c_str(), &pSPIRVStream);
    if (!pSPIRVStream)
        return {};

    const size_t ByteCodeSize = pSPIRVStream->GetSize();
    if (ByteCodeSize == 0 || ByteCodeSize % 4 != 0)
        return {};

    SPIRV.resize(ByteCodeSize / 4);
    pSPIRVStream->Read(SPIRV.data(), ByteCodeSize);

#else

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceStreamFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/SPIRV", &pShaderSourceStreamFactory);
    if (!pShaderSourceStreamFactory)
        return {};

    RefCntAutoPtr<IFileStream> pShaderSourceStream;
    pShaderSourceStreamFactory->CreateInputStream(FilePath, &pShaderSourceStream);
    if (!pShaderSourceStream)
        return {};

    auto ShaderSourceSize = pShaderSourceStream->GetSize();
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

    GLSLangUtils::InitializeGlslang();
    SPIRV = GLSLangUtils::GLSLtoSPIRV(Attribs);
    GLSLangUtils::FinalizeGlslang();

#endif
    return SPIRV;
}

void TestSPIRVResources(const char*                                       FilePath,
                        const std::vector<SPIRVShaderResourceRefAttribs>& RefResources,
                        SHADER_TYPE                                       ShaderType            = SHADER_TYPE_PIXEL,
                        const char*                                       CombinedSamplerSuffix = nullptr,
                        bool                                              IsGLSL                = false)
{
    const auto SPIRV = IsGLSL ? LoadSPIRVFromGLSL(FilePath, ShaderType) : LoadSPIRVFromHLSL(FilePath, ShaderType);
    ASSERT_FALSE(SPIRV.empty()) << "Failed to compile HLSL to SPIRV: " << FilePath;

    ShaderDesc ShaderDesc;
    ShaderDesc.Name       = "SPIRVResources test";
    ShaderDesc.ShaderType = ShaderType;

    std::string          EntryPoint;
    SPIRVShaderResources Resources{
        GetRawAllocator(),
        SPIRV,
        ShaderDesc,
        CombinedSamplerSuffix,
        false, // LoadShaderStageInputs
        false, // LoadUniformBufferReflection
        EntryPoint};

    LOG_INFO_MESSAGE("Testing shader:", FilePath);
    if (CombinedSamplerSuffix != nullptr)
    {
        LOG_INFO_MESSAGE("Using CombinedSamplerSuffix:", CombinedSamplerSuffix);
    }
    LOG_INFO_MESSAGE("SPIRV Resources:\n", Resources.DumpResources());

    EXPECT_EQ(size_t{Resources.GetTotalResources()}, RefResources.size());

    std::unordered_map<std::string, const SPIRVShaderResourceRefAttribs*> RefResourcesMap;
    for (const auto& RefRes : RefResources)
    {
        RefResourcesMap[RefRes.Name] = &RefRes;
    }

    for (Uint32 i = 0; i < Resources.GetTotalResources(); ++i)
    {
        const auto& Res     = const_cast<const SPIRVShaderResources&>(Resources).GetResource(i);
        const auto* pRefRes = RefResourcesMap[Res.Name];
        ASSERT_NE(pRefRes, nullptr) << "Resource '" << Res.Name << "' is not found in the reference list";

        EXPECT_EQ(Res.ArraySize, pRefRes->ArraySize) << Res.Name;
        EXPECT_STREQ(SPIRVShaderResourceAttribs::ResourceTypeToString(Res.Type), SPIRVShaderResourceAttribs::ResourceTypeToString(pRefRes->Type)) << Res.Name;
        EXPECT_EQ(Res.ArraySize, pRefRes->ArraySize) << Res.Name;
        EXPECT_EQ(Res.ResourceDim, pRefRes->ResourceDim) << Res.Name;
        EXPECT_EQ(Res.IsMS, pRefRes->IsMS) << Res.Name;
        EXPECT_EQ(Res.BufferStaticSize, pRefRes->BufferStaticSize) << Res.Name;
        EXPECT_EQ(Res.BufferStride, pRefRes->BufferStride) << Res.Name;
    }
}

using SPIRVResourceType = SPIRVShaderResourceAttribs::ResourceType;

TEST(SPIRVShaderResources, UniformBuffers)
{
    TestSPIRVResources("UniformBuffers.psh",
                       {
                           // CB0 is optimized away as it's not used in the shader
                           SPIRVShaderResourceRefAttribs{"CB1", SPIRVResourceType::UniformBuffer, 1, RESOURCE_DIM_BUFFER, 0, 48, 0},
                           SPIRVShaderResourceRefAttribs{"CB2", SPIRVResourceType::UniformBuffer, 1, RESOURCE_DIM_BUFFER, 0, 16, 0},
                       });
}

TEST(SPIRVShaderResources, StorageBuffers)
{
    TestSPIRVResources("StorageBuffers.psh",
                       {
                           // StructuredBuffers have BufferStaticSize=0 (runtime array) and BufferStride is the element size
                           SPIRVShaderResourceRefAttribs{"g_ROBuffer", SPIRVResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 32},
                           SPIRVShaderResourceRefAttribs{"g_RWBuffer", SPIRVResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 64},
                           // ByteAddressBuffers also have BufferStaticSize=0 and BufferStride=4 (uint size)
                           SPIRVShaderResourceRefAttribs{"g_ROAtomicBuffer", SPIRVResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 4},
                           SPIRVShaderResourceRefAttribs{"g_RWAtomicBuffer", SPIRVResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 4},
                       });
}

TEST(SPIRVShaderResources, TexelBuffers)
{
    TestSPIRVResources("TexelBuffers.psh",
                       {
                           SPIRVShaderResourceRefAttribs{"g_UniformTexelBuffer", SPIRVResourceType::UniformTexelBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_StorageTexelBuffer", SPIRVResourceType::StorageTexelBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 0},
                       });
}

TEST(SPIRVShaderResources, Textures)
{
    TestSPIRVResources("Textures.psh",
                       {
                           // When textures and samplers are declared separately in HLSL, they are compiled as separate_images
                           // instead of sampled_images. This is the correct behavior for separate sampler/texture declarations.
                           SPIRVShaderResourceRefAttribs{"g_SampledImage", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_SampledImageMS", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 1, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_SampledImage3D", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_3D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_SampledImageCube", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_CUBE, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_Sampler", SPIRVResourceType::SeparateSampler, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_SeparateImage", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           // Combined sampler: g_Texture and g_Texture_sampler
                           // Note: Even with CombinedSamplerSuffix, SPIRV may still classify them as separate_images
                           // if they are declared separately. The CombinedSamplerSuffix is mainly used for naming convention.
                           SPIRVShaderResourceRefAttribs{"g_Texture", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_Texture_sampler", SPIRVResourceType::SeparateSampler, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                       });
}

TEST(SPIRVShaderResources, StorageImages)
{
    TestSPIRVResources("StorageImages.psh",
                       {
                           // Note: HLSL does not support RWTextureCube, so we only test 2D, 2DArray, and 3D storage images
                           SPIRVShaderResourceRefAttribs{"g_RWImage2D", SPIRVResourceType::StorageImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_RWImage2DArray", SPIRVResourceType::StorageImage, 1, RESOURCE_DIM_TEX_2D_ARRAY, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"g_RWImage3D", SPIRVResourceType::StorageImage, 1, RESOURCE_DIM_TEX_3D, 0, 0, 0},
                       });
}

TEST(SPIRVShaderResources, AtomicCounters)
{
    // Use GLSL for atomic counters. Note: Vulkan does not support atomic_uint (AtomicCounter storage class).
    // We use a storage buffer with atomic operations to simulate atomic counters.
    // This will be reflected as RWStorageBuffer, not AtomicCounter.
    // The resource name is the buffer block name (AtomicCounterBuffer), not the instance name (g_AtomicCounter).
    TestSPIRVResources("AtomicCounters.glsl",
                       {
                           SPIRVShaderResourceRefAttribs{"AtomicCounterBuffer", SPIRVResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 4, 0},
                       },
                       SHADER_TYPE_PIXEL,
                       nullptr,
                       true); // IsGLSL = true
}

TEST(SPIRVShaderResources, InputAttachments)
{
    TestSPIRVResources("InputAttachments.psh",
                       {
                           SPIRVShaderResourceRefAttribs{"g_InputAttachment", SPIRVResourceType::InputAttachment, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                       });
}

TEST(SPIRVShaderResources, AccelerationStructures)
{
    // Use GLSL for acceleration structures since HLSLtoSPIRV doesn't support raytracing shaders
    // Acceleration structures are used in raytracing shaders, so we use SHADER_TYPE_RAY_GEN
    // The ray gen shader uses traceRayEXT with g_AccelStruct to ensure it's not optimized away
    TestSPIRVResources("AccelerationStructures.glsl",
                       {
                           SPIRVShaderResourceRefAttribs{"g_AccelStruct", SPIRVResourceType::AccelerationStructure, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                       },
                       SHADER_TYPE_RAY_GEN,
                       nullptr,
                       true); // IsGLSL = true
}

TEST(SPIRVShaderResources, PushConstants)
{
    // Push constant ArraySize represents the number of 32-bit words, not array elements
    // PushConstants struct: float4x4 (16 floats) + float4 (4 floats) + float2 (2 floats) + float (1 float) + uint (1 uint)
    // Total: 16 + 4 + 2 + 1 + 1 = 24 floats/uints = 24 * 4 bytes = 96 bytes = 24 words
    TestSPIRVResources("PushConstants.psh",
                       {
                           SPIRVShaderResourceRefAttribs{"PushConstants", SPIRVResourceType::PushConstant, 24, RESOURCE_DIM_BUFFER, 0, 96, 0},
                       });
}

TEST(SPIRVShaderResources, MixedResources)
{
    TestSPIRVResources("MixedResources.psh",
                       {
                           // UniformBuff: float4x4 (64 bytes) + float4 (16 bytes) = 80 bytes
                           SPIRVShaderResourceRefAttribs{"UniformBuff", SPIRVResourceType::UniformBuffer, 1, RESOURCE_DIM_BUFFER, 0, 80, 0},
                           // ROStorageBuff: StructuredBuffer<BufferData> where BufferData = float4[4] = 64 bytes
                           // StructuredBuffers have BufferStaticSize=0 (runtime array) and BufferStride is the element size
                           SPIRVShaderResourceRefAttribs{"ROStorageBuff", SPIRVResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 64},
                           // RWStorageBuff: same as ROStorageBuff
                           SPIRVShaderResourceRefAttribs{"RWStorageBuff", SPIRVResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 64},
                           // SampledTex: When Texture2D and SamplerState are declared separately, they are compiled as SeparateImage
                           SPIRVShaderResourceRefAttribs{"SampledTex", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"StorageTex", SPIRVResourceType::StorageImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceRefAttribs{"Sampler", SPIRVResourceType::SeparateSampler, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                           // PushConstants: float2 (2 floats) + float (1 float) + uint (1 uint) = 4 words = 16 bytes
                           SPIRVShaderResourceRefAttribs{"PushConstants", SPIRVResourceType::PushConstant, 4, RESOURCE_DIM_BUFFER, 0, 16, 0},
                       });
}

} // namespace