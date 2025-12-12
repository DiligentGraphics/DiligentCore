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

#include "TestingEnvironment.hpp"
#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

const char* SPIRVShaderResourceTypeToString(SPIRVShaderResourceAttribs::ResourceType Type)
{
    switch (Type)
    {
        case SPIRVShaderResourceAttribs::ResourceType::UniformBuffer:
            return "UniformBuffer";
        case SPIRVShaderResourceAttribs::ResourceType::ROStorageBuffer:
            return "ROStorageBuffer";
        case SPIRVShaderResourceAttribs::ResourceType::RWStorageBuffer:
            return "RWStorageBuffer";
        case SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer:
            return "UniformTexelBuffer";
        case SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer:
            return "StorageTexelBuffer";
        case SPIRVShaderResourceAttribs::ResourceType::StorageImage:
            return "StorageImage";
        case SPIRVShaderResourceAttribs::ResourceType::SampledImage:
            return "SampledImage";
        case SPIRVShaderResourceAttribs::ResourceType::AtomicCounter:
            return "AtomicCounter";
        case SPIRVShaderResourceAttribs::ResourceType::SeparateImage:
            return "SeparateImage";
        case SPIRVShaderResourceAttribs::ResourceType::SeparateSampler:
            return "SeparateSampler";
        case SPIRVShaderResourceAttribs::ResourceType::InputAttachment:
            return "InputAttachment";
        case SPIRVShaderResourceAttribs::ResourceType::AccelerationStructure:
            return "AccelerationStructure";
        case SPIRVShaderResourceAttribs::ResourceType::PushConstant:
            return "PushConstant";
        default:
            return "Unknown";
    }
}

std::vector<uint32_t> HLSLtoSPIRV(const char* FilePath, SHADER_TYPE ShaderType = SHADER_TYPE_PIXEL)
{
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
    auto SPIRV = GLSLangUtils::HLSLtoSPIRV(ShaderCI, GLSLangUtils::SpirvVersion::Vk100, nullptr, nullptr);
    GLSLangUtils::FinalizeGlslang();

    return SPIRV;
}

std::vector<uint32_t> GLSLtoSPIRV(const char* FilePath, SHADER_TYPE ShaderType = SHADER_TYPE_PIXEL)
{
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
    auto SPIRV = GLSLangUtils::GLSLtoSPIRV(Attribs);
    GLSLangUtils::FinalizeGlslang();

    return SPIRV;
}

void TestSPIRVResources(const char*                                    FilePath,
                        const std::vector<SPIRVShaderResourceAttribs>& RefResources,
                        SHADER_TYPE                                    ShaderType            = SHADER_TYPE_PIXEL,
                        const char*                                    CombinedSamplerSuffix = nullptr,
                        bool                                           IsGLSL                = false)
{
    const auto SPIRV = IsGLSL ? GLSLtoSPIRV(FilePath, ShaderType) : HLSLtoSPIRV(FilePath, ShaderType);
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

    std::unordered_map<std::string, const SPIRVShaderResourceAttribs*> RefResourcesMap;
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
        EXPECT_STREQ(SPIRVShaderResourceTypeToString(Res.Type), SPIRVShaderResourceTypeToString(pRefRes->Type)) << Res.Name;
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
                           SPIRVShaderResourceAttribs{"CB1", SPIRVResourceType::UniformBuffer, 1, RESOURCE_DIM_BUFFER, 0, 48, 0},
                           SPIRVShaderResourceAttribs{"CB2", SPIRVResourceType::UniformBuffer, 1, RESOURCE_DIM_BUFFER, 0, 16, 0},
                       });
}

TEST(SPIRVShaderResources, StorageBuffers)
{
    TestSPIRVResources("StorageBuffers.psh",
                       {
                           // StructuredBuffers have BufferStaticSize=0 (runtime array) and BufferStride is the element size
                           SPIRVShaderResourceAttribs{"g_ROBuffer", SPIRVResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 32},
                           SPIRVShaderResourceAttribs{"g_RWBuffer", SPIRVResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 64},
                           // ByteAddressBuffers also have BufferStaticSize=0 and BufferStride=4 (uint size)
                           SPIRVShaderResourceAttribs{"g_ROAtomicBuffer", SPIRVResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 4},
                           SPIRVShaderResourceAttribs{"g_RWAtomicBuffer", SPIRVResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 4},
                       });
}

TEST(SPIRVShaderResources, TexelBuffers)
{
    TestSPIRVResources("TexelBuffers.psh",
                       {
                           SPIRVShaderResourceAttribs{"g_UniformTexelBuffer", SPIRVResourceType::UniformTexelBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"g_StorageTexelBuffer", SPIRVResourceType::StorageTexelBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 0},
                       });
}

TEST(SPIRVShaderResources, Textures)
{
    TestSPIRVResources("Textures.psh",
                       {
                           // When textures and samplers are declared separately in HLSL, they are compiled as separate_images
                           // instead of sampled_images. This is the correct behavior for separate sampler/texture declarations.
                           SPIRVShaderResourceAttribs{"g_SampledImage", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"g_SampledImageMS", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 1, 0, 0},
                           SPIRVShaderResourceAttribs{"g_SampledImage3D", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_3D, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"g_SampledImageCube", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_CUBE, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"g_Sampler", SPIRVResourceType::SeparateSampler, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"g_SeparateImage", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           // Combined sampler: g_Texture and g_Texture_sampler
                           // Note: Even with CombinedSamplerSuffix, SPIRV may still classify them as separate_images
                           // if they are declared separately. The CombinedSamplerSuffix is mainly used for naming convention.
                           SPIRVShaderResourceAttribs{"g_Texture", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"g_Texture_sampler", SPIRVResourceType::SeparateSampler, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                       });
}

TEST(SPIRVShaderResources, StorageImages)
{
    TestSPIRVResources("StorageImages.psh",
                       {
                           // Note: HLSL does not support RWTextureCube, so we only test 2D, 2DArray, and 3D storage images
                           SPIRVShaderResourceAttribs{"g_RWImage2D", SPIRVResourceType::StorageImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"g_RWImage2DArray", SPIRVResourceType::StorageImage, 1, RESOURCE_DIM_TEX_2D_ARRAY, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"g_RWImage3D", SPIRVResourceType::StorageImage, 1, RESOURCE_DIM_TEX_3D, 0, 0, 0},
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
                           SPIRVShaderResourceAttribs{"AtomicCounterBuffer", SPIRVResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 4, 0},
                       },
                       SHADER_TYPE_PIXEL,
                       nullptr,
                       true); // IsGLSL = true
}

TEST(SPIRVShaderResources, InputAttachments)
{
    TestSPIRVResources("InputAttachments.psh",
                       {
                           SPIRVShaderResourceAttribs{"g_InputAttachment", SPIRVResourceType::InputAttachment, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                       });
}

TEST(SPIRVShaderResources, AccelerationStructures)
{
    // Use GLSL for acceleration structures since HLSLtoSPIRV doesn't support raytracing shaders
    // Acceleration structures are used in raytracing shaders, so we use SHADER_TYPE_RAY_GEN
    // The ray gen shader uses traceRayEXT with g_AccelStruct to ensure it's not optimized away
    TestSPIRVResources("AccelerationStructures.glsl",
                       {
                           SPIRVShaderResourceAttribs{"g_AccelStruct", SPIRVResourceType::AccelerationStructure, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
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
                           SPIRVShaderResourceAttribs{"PushConstants", SPIRVResourceType::PushConstant, 24, RESOURCE_DIM_BUFFER, 0, 96, 0},
                       });
}

TEST(SPIRVShaderResources, MixedResources)
{
    TestSPIRVResources("MixedResources.psh",
                       {
                           // UniformBuff: float4x4 (64 bytes) + float4 (16 bytes) = 80 bytes
                           SPIRVShaderResourceAttribs{"UniformBuff", SPIRVResourceType::UniformBuffer, 1, RESOURCE_DIM_BUFFER, 0, 80, 0},
                           // ROStorageBuff: StructuredBuffer<BufferData> where BufferData = float4[4] = 64 bytes
                           // StructuredBuffers have BufferStaticSize=0 (runtime array) and BufferStride is the element size
                           SPIRVShaderResourceAttribs{"ROStorageBuff", SPIRVResourceType::ROStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 64},
                           // RWStorageBuff: same as ROStorageBuff
                           SPIRVShaderResourceAttribs{"RWStorageBuff", SPIRVResourceType::RWStorageBuffer, 1, RESOURCE_DIM_BUFFER, 0, 0, 64},
                           // SampledTex: When Texture2D and SamplerState are declared separately, they are compiled as SeparateImage
                           SPIRVShaderResourceAttribs{"SampledTex", SPIRVResourceType::SeparateImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"StorageTex", SPIRVResourceType::StorageImage, 1, RESOURCE_DIM_TEX_2D, 0, 0, 0},
                           SPIRVShaderResourceAttribs{"Sampler", SPIRVResourceType::SeparateSampler, 1, RESOURCE_DIM_UNDEFINED, 0, 0, 0},
                           // PushConstants: float2 (2 floats) + float (1 float) + uint (1 uint) = 4 words = 16 bytes
                           SPIRVShaderResourceAttribs{"PushConstants", SPIRVResourceType::PushConstant, 4, RESOURCE_DIM_BUFFER, 0, 16, 0},
                       });
}

} // namespace