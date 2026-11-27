/*
 *  Copyright 2019-2026 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include <array>
#include <limits>

#include "GPUTestingEnvironment.hpp"
#include "GraphicsTypesX.hpp"
#include "StringTools.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static void TestCreatePRSFailure(PipelineResourceSignatureDesc CI, const char* ExpectedErrorSubstring)
{
    GPUTestingEnvironment* const pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice* const         pDevice = pEnv->GetDevice();

    RefCntAutoPtr<IPipelineResourceSignature> pSignature;
    pEnv->SetErrorAllowance(2, "Errors below are expected: testing PRS creation failure\n");
    pEnv->PushExpectedErrorSubstring(ExpectedErrorSubstring);
    pDevice->CreatePipelineResourceSignature(CI, &pSignature);
    ASSERT_FALSE(pSignature);

    CI.Name = nullptr;
    pEnv->SetErrorAllowance(2);
    pEnv->PushExpectedErrorSubstring(ExpectedErrorSubstring);
    pDevice->CreatePipelineResourceSignature(CI, &pSignature);
    ASSERT_FALSE(pSignature);

    pEnv->SetErrorAllowance(0);
}

TEST(PRSCreationFailureTest, InvalidBindingIndex)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name         = "Invalid binding index";
    PRSDesc.BindingIndex = std::numeric_limits<decltype(PRSDesc.BindingIndex)>::max();
    TestCreatePRSFailure(PRSDesc, "Desc.BindingIndex (255) exceeds the maximum allowed value");
}

TEST(PRSCreationFailureTest, InvalidNumResources)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name         = "Invalid NumResources";
    PRSDesc.NumResources = std::numeric_limits<decltype(PRSDesc.NumResources)>::max();
    TestCreatePRSFailure(PRSDesc, "Desc.NumResources (4294967295) exceeds the maximum allowed value");
}

TEST(PRSCreationFailureTest, NullResources)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name         = "Null Resources";
    PRSDesc.NumResources = 10;
    PRSDesc.Resources    = nullptr;
    TestCreatePRSFailure(PRSDesc, "Desc.NumResources (10) is not zero, but Desc.Resources is null");
}

TEST(PRSCreationFailureTest, NullImmutableSamplers)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name                 = "Null Resources";
    PRSDesc.NumImmutableSamplers = 12;
    PRSDesc.ImmutableSamplers    = nullptr;
    TestCreatePRSFailure(PRSDesc, "Desc.NumImmutableSamplers (12) is not zero, but Desc.ImmutableSamplers is null");
}

TEST(PRSCreationFailureTest, NullCombinedSamplerSuffix)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name                       = "Null CombinedSamplerSuffix";
    PRSDesc.UseCombinedTextureSamplers = true;
    PRSDesc.CombinedSamplerSuffix      = nullptr;
    TestCreatePRSFailure(PRSDesc, "Desc.UseCombinedTextureSamplers is true, but Desc.CombinedSamplerSuffix is null or empty");

    PRSDesc.Name                  = "Null CombinedSamplerSuffix 2";
    PRSDesc.CombinedSamplerSuffix = "";
    TestCreatePRSFailure(PRSDesc, "Desc.UseCombinedTextureSamplers is true, but Desc.CombinedSamplerSuffix is null or empty");
}

TEST(PRSCreationFailureTest, NullResourceName)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Null resource name";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, nullptr, 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Desc.Resources[1].Name must not be null");

    PRSDesc.Name      = "Null resource name 2";
    Resources[1].Name = "";
    TestCreatePRSFailure(PRSDesc, "Desc.Resources[1].Name must not be empty");
}

TEST(PRSCreationFailureTest, UnknownResourceShaderStages)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Unknown resource ShaderStages";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Buffer", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_UNKNOWN, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Desc.Resources[1].ShaderStages must not be SHADER_TYPE_UNKNOWN");
}

TEST(PRSCreationFailureTest, IncompatibleResourceShaderStages)
{
    const PipelineResourceDesc Resources[] =
        {
            {SHADER_TYPE_VERTEX, "g_VertexBuffer", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_COMPUTE, "g_ComputeBuffer", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    PipelineResourceSignatureDesc Desc;
    Desc.Name                     = "Incompatible resource shader stages";
    Desc.Resources                = Resources;
    Desc.NumResources             = _countof(Resources);
    Desc.SRBAllocationGranularity = 4;
    TestCreatePRSFailure(Desc, "must belong to a single pipeline type");
}

TEST(PRSCreationFailureTest, IncompatibleImmutableSamplerShaderStages)
{
    const ImmutableSamplerDesc    Sampler{SHADER_TYPE_ALL, "g_Sampler", SamplerDesc{}};
    PipelineResourceSignatureDesc Desc;
    Desc.Name                     = "Incompatible immutable sampler shader stages";
    Desc.ImmutableSamplers        = &Sampler;
    Desc.NumImmutableSamplers     = 1;
    Desc.SRBAllocationGranularity = 4;
    TestCreatePRSFailure(Desc, "must belong to a single pipeline type");
}

TEST(PRSCreationFailureTest, InvalidImmutableSamplerShaderStages)
{
    const PipelineResourceDesc Resource{SHADER_TYPE_PIXEL, "g_Buffer", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE};
    const ImmutableSamplerDesc Sampler{static_cast<SHADER_TYPE>(SHADER_TYPE_PIXEL | (Uint32{1} << 31)), "g_Sampler", SamplerDesc{}};

    PipelineResourceSignatureDesc Desc;
    Desc.Name                     = "Invalid immutable sampler shader stages";
    Desc.Resources                = &Resource;
    Desc.NumResources             = 1;
    Desc.ImmutableSamplers        = &Sampler;
    Desc.NumImmutableSamplers     = 1;
    Desc.SRBAllocationGranularity = 4;
    const auto Error              = FormatString("Desc.ImmutableSamplers[0].ShaderStages (", Uint32{Sampler.ShaderStages}, ") contains invalid shader stage bits.");
    TestCreatePRSFailure(Desc, Error.c_str());
}

TEST(PRSCreationFailureTest, ZeroArraySize)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Zero array size";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Buffer", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, "g_Texture", 0, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Desc.Resources[1].ArraySize must not be 0");
}

TEST(PRSCreationFailureTest, ResourceCountOverflow)
{
    PipelineResourceDesc Resources[] =
        {
            {SHADER_TYPE_VS_PS, "g_Texture0", 1u << 31, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
            {SHADER_TYPE_VS_PS, "g_Texture1", 1u << 31, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PipelineResourceSignatureDesc Desc;
    Desc.Name         = "Resource count overflow";
    Desc.Resources    = Resources;
    Desc.NumResources = _countof(Resources);
    TestCreatePRSFailure(Desc, "Total resource count, including array elements, exceeds the maximum representable value");

    Desc.NumResources      = 1;
    Resources[0].ArraySize = ~0u;
    TestCreatePRSFailure(Desc, "Total resource count, including array elements, exceeds the maximum representable value");
}

TEST(PRSCreationFailureTest, D3D12ImmutableSamplerRegisters)
{
    IRenderDevice* const pDevice = GPUTestingEnvironment::GetInstance()->GetDevice();
    if (pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D12)
        GTEST_SKIP();

    PipelineResourceDesc Resources[] =
        {
            {SHADER_TYPE_PIXEL, "g_Samplers", 65535, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL, "g_LastSampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    const ImmutableSamplerDesc Samplers[] =
        {
            {SHADER_TYPE_PIXEL, "g_Samplers", SamplerDesc{}},
            {SHADER_TYPE_PIXEL, "g_LastSampler", SamplerDesc{}}};
    PipelineResourceSignatureDesc Desc;
    Desc.Name                 = "Immutable sampler registers";
    Desc.Resources            = Resources;
    Desc.NumResources         = _countof(Resources);
    Desc.ImmutableSamplers    = Samplers;
    Desc.NumImmutableSamplers = _countof(Samplers);

    // Only signature metadata is created: immutable sampler arrays consume no resource cache slots.
    // The second sampler's register is the largest value representable in a resource attribute.
    RefCntAutoPtr<IPipelineResourceSignature> pSignature;
    pDevice->CreatePipelineResourceSignature(Desc, &pSignature);
    ASSERT_NE(pSignature, nullptr);

    Resources[0].ArraySize = 65536;
    TestCreatePRSFailure(Desc, "Shader register for resource 'g_LastSampler' exceeds the maximum representable value");

    // The immutable-sampler attribute has a wider field, with its highest value reserved.
    Resources[0].ArraySize = (1u << 24) - 1;
    TestCreatePRSFailure(Desc, "Immutable sampler shader register exceeds the maximum representable value");
}

TEST(PRSCreationFailureTest, BackendResourceCountOverflow)
{
    const auto  DeviceType = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Type;
    Uint32      ArraySize  = 0;
    const char* Error      = "Resource binding count exceeds the maximum representable value";
    switch (DeviceType)
    {
        case RENDER_DEVICE_TYPE_D3D11: ArraySize = 256; break;
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES: ArraySize = 65536; break;
        case RENDER_DEVICE_TYPE_WEBGPU: ArraySize = 65537; break;
        case RENDER_DEVICE_TYPE_D3D12:
            ArraySize = 1u << 31;
            Error     = "Resource array size exceeds the maximum representable value";
            break;
        case RENDER_DEVICE_TYPE_VULKAN:
            ArraySize = 1u << 25;
            Error     = "Resource array size for 'g_Texture' exceeds the maximum representable value";
            break;
        default: GTEST_SKIP() << "No backend-specific cache limit case";
    }

    PipelineResourceDesc          Resource{SHADER_TYPE_VS_PS, "g_Texture", ArraySize, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC};
    PipelineResourceSignatureDesc Desc;
    Desc.Name         = "Backend resource count overflow";
    Desc.Resources    = &Resource;
    Desc.NumResources = 1;
    for (Uint32 Granularity : {1u, 4u})
    {
        Desc.SRBAllocationGranularity = Granularity;
        TestCreatePRSFailure(Desc, Error);
    }

    // Also exercise cleanup after immutable sampler creation.
    ImmutableSamplerDesc Sampler{SHADER_TYPE_PIXEL, "g_UnusedSampler", SamplerDesc{}};
    Desc.ImmutableSamplers    = &Sampler;
    Desc.NumImmutableSamplers = 1;
    TestCreatePRSFailure(Desc, Error);
}

TEST(PRSCreationFailureTest, WebGPUCombinedSamplerCountOverflow)
{
    if (GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_WEBGPU)
        GTEST_SKIP();

    // The original texture array fits; its generated sampler array takes the group over the limit.
    const PipelineResourceDesc Resource{SHADER_TYPE_PIXEL, "g_Texture", 32769, SHADER_RESOURCE_TYPE_TEXTURE_SRV,
                                        SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER};

    PipelineResourceSignatureDesc Desc;
    Desc.Resources                  = &Resource;
    Desc.NumResources               = 1;
    Desc.UseCombinedTextureSamplers = true;
    TestCreatePRSFailure(Desc, "Resource binding count exceeds the maximum representable value");
}

TEST(PRSCreationFailureTest, ImmutableSamplerArrayCountOverflow)
{
    const auto Type = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Type;
    if (Type != RENDER_DEVICE_TYPE_D3D11 && Type != RENDER_DEVICE_TYPE_WEBGPU)
        GTEST_SKIP();

    const Uint32               Count = Type == RENDER_DEVICE_TYPE_D3D11 ? 256 : 65537;
    const PipelineResourceDesc Resource{SHADER_TYPE_PIXEL, "g_Sampler", Count, SHADER_RESOURCE_TYPE_SAMPLER,
                                        SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE};
    const ImmutableSamplerDesc Sampler{SHADER_TYPE_PIXEL, "g_Sampler", SamplerDesc{}};

    PipelineResourceSignatureDesc Desc;
    Desc.Resources            = &Resource;
    Desc.NumResources         = 1;
    Desc.ImmutableSamplers    = &Sampler;
    Desc.NumImmutableSamplers = 1;
    TestCreatePRSFailure(Desc, "Resource binding count exceeds the maximum representable value");
}

TEST(PRSCreationFailureTest, D3D12RootParameterCountOverflow)
{
    if (GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D12)
        GTEST_SKIP();
    PipelineResourceSignatureDescX Desc{"Root parameter count overflow"};
    for (Uint32 i = 0; i < 65; ++i)
    {
        const std::string Name = "g_Buffer" + std::to_string(i);
        Desc.AddResource(SHADER_TYPE_PIXEL, Name.c_str(), 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);
    }
    TestCreatePRSFailure(Desc, "Root table count exceeds the maximum representable value");
}

TEST(PRSCreationFailureTest, DynamicBufferCountOverflow)
{
    const auto& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (!DeviceInfo.IsVulkanDevice() && DeviceInfo.Type != RENDER_DEVICE_TYPE_WEBGPU)
        GTEST_SKIP();

    // The counts fit separately, but their sum overflows the cache's dynamic buffer counter.
    const PipelineResourceDesc Resources[] =
        {
            {SHADER_TYPE_VS_PS, "g_UniformBuffers", 32768, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_VS_PS, "g_StorageBuffers", 32768, SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        };
    PipelineResourceSignatureDesc Desc;
    Desc.Name         = "Dynamic buffer count overflow";
    Desc.Resources    = Resources;
    Desc.NumResources = _countof(Resources);
    TestCreatePRSFailure(Desc, "Dynamic buffer count exceeds the maximum representable value");
}

TEST(PRSCreationFailureTest, VulkanAggregateResourceCountOverflow)
{
    if (!GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().IsVulkanDevice())
        GTEST_SKIP();

    // Every array fits in its 25-bit field, but the aggregate exceeds the cache's 30-bit count.
    PipelineResourceSignatureDescX Desc{"Aggregate resource count overflow"};
    for (Uint32 i = 0; i < 33; ++i)
    {
        const std::string Name = "g_Texture" + std::to_string(i);
        Desc.AddResource(SHADER_TYPE_VS_PS, Name.c_str(), (1u << 25) - 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    }
    TestCreatePRSFailure(Desc, "Shader resource cache resource count exceeds the maximum representable value");
}

TEST(PRSCreationFailureTest, ResourceCacheAddressSpaceOverflow)
{
    if (sizeof(size_t) > sizeof(Uint32))
        GTEST_SKIP() << "This case targets 32-bit allocation sizes";

    const auto& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (!DeviceInfo.IsVulkanDevice() && DeviceInfo.Type != RENDER_DEVICE_TYPE_D3D12)
        GTEST_SKIP();

    // Counts fit the backend's fields, but storing the resources requires more than 4 GiB.
    PipelineResourceSignatureDescX Desc{"Resource cache address space overflow"};
    const Uint32                   NumResources = DeviceInfo.IsVulkanDevice() ? 16 : 1;
    const Uint32                   ArraySize    = DeviceInfo.IsVulkanDevice() ? 1u << 24 : 1u << 29;
    for (Uint32 i = 0; i < NumResources; ++i)
    {
        const std::string Name = "g_Texture" + std::to_string(i);
        Desc.AddResource(SHADER_TYPE_VS_PS, Name.c_str(), ArraySize, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    }
    TestCreatePRSFailure(Desc, "Resource cache memory size");
}

TEST(PRSCreationFailureTest, OpenGLResourceCacheSizeOverflow)
{
    if (!GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().IsGLDevice())
        GTEST_SKIP();

    // The binding count fits in 16 bits, but the byte offsets do not.
    PipelineResourceDesc          Resource{SHADER_TYPE_VS_PS, "g_Texture", 16384, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC};
    PipelineResourceSignatureDesc Desc;
    Desc.Name         = "Resource cache size overflow";
    Desc.Resources    = &Resource;
    Desc.NumResources = 1;
    TestCreatePRSFailure(Desc, "exceeds the maximum representable value");
}

TEST(PRSCreationFailureTest, OverlappingStages)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Overlapping Shader Stages";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_VERTEX | SHADER_TYPE_GEOMETRY, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Multiple resources with name 'g_Texture' specify overlapping shader stages");
}

TEST(PRSCreationFailureTest, InvalidResourceFlag)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Overlapping Shader Stages";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "g_Buffer", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER | PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (COMBINED_SAMPLER|FORMATTED_BUFFER). Only the following flags are valid for a constant buffer: NO_DYNAMIC_BUFFERS, INLINE_CONSTANTS, RUNTIME_ARRAY");
}

TEST(PRSCreationFailureTest, InvalidTexSRVFlag)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid Tex SRV Flag";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "g_Texture2", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS | PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER | PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (NO_DYNAMIC_BUFFERS|COMBINED_SAMPLER|FORMATTED_BUFFER). Only the following flags are valid for a texture SRV: COMBINED_SAMPLER, RUNTIME_ARRAY");
}

TEST(PRSCreationFailureTest, InvalidBuffSRVFlag)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid Buff SRV Flag";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "g_Buffer", 1, SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (COMBINED_SAMPLER). Only the following flags are valid for a buffer SRV: NO_DYNAMIC_BUFFERS, FORMATTED_BUFFER, RUNTIME_ARRAY");
}

TEST(PRSCreationFailureTest, InvalidTexUAVFlag)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid Tex UAV Flag";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "g_Texture2", 1, SHADER_RESOURCE_TYPE_TEXTURE_UAV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (COMBINED_SAMPLER). Only the following flags are valid for a texture UAV: RUNTIME_ARRAY");
}

TEST(PRSCreationFailureTest, InvalidBuffUAVFlag)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid Buff UAV Flag";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "g_Buffer", 1, SHADER_RESOURCE_TYPE_BUFFER_UAV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER | PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (COMBINED_SAMPLER|FORMATTED_BUFFER). Only the following flags are valid for a buffer UAV: NO_DYNAMIC_BUFFERS, FORMATTED_BUFFER, RUNTIME_ARRAY");
}

TEST(PRSCreationFailureTest, InvalidSamplerFlag)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid sampler Flag";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "g_Sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS | PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (NO_DYNAMIC_BUFFERS|FORMATTED_BUFFER). Only the following flags are valid for a sampler: RUNTIME_ARRAY");
}

TEST(PRSCreationFailureTest, InvalidInputAttachmentFlag)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid input attachment Flag";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, "g_InputAttachment", 1, SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (RUNTIME_ARRAY). Only the following flags are valid for a input attachment: GENERAL_INPUT_ATTACHMENT");
}

TEST(PRSCreationFailureTest, InvalidAccelStructFlag)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid accel struct Flag";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, "g_AS", 1, SHADER_RESOURCE_TYPE_ACCEL_STRUCT, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (NO_DYNAMIC_BUFFERS). Only the following flags are valid for a acceleration structure: RUNTIME_ARRAY");
}

TEST(PRSCreationFailureTest, InvalidCombinedSamplerFlag)
{
    const RenderDeviceInfo& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (!(DeviceInfo.IsD3DDevice() || DeviceInfo.IsMetalDevice()))
    {
        GTEST_SKIP() << "Direct3D11, Direct3D12 and Metal only";
    }

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name                       = "Invalid combined sampler Flag";
    PRSDesc.UseCombinedTextureSamplers = false;
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Desc.Resources[0].Flags contain COMBINED_SAMPLER flag, but Desc.UseCombinedTextureSamplers is false");
}

TEST(PRSCreationFailureTest, InvalidInlineConstantsFlag)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name                       = "Invalid inline constants Flag";
    PRSDesc.UseCombinedTextureSamplers = false;
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_InlineConstants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS | PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "INLINE_CONSTANTS flag cannot be combined with other flags");
}

TEST(PRSCreationFailureTest, InvalidInlineConstantCount)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name                       = "Invalid inline constant count";
    PRSDesc.UseCombinedTextureSamplers = false;
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_InlineConstants", 65, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Desc.Resources[0].ArraySize (65) exceeds the maximum allowed value (64) for inline constants.");
}

TEST(PRSCreationFailureTest, InvalidAssignedSamplerResourceType)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid assigned sampler resource type";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, "g_Texture_sampler", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.UseCombinedTextureSamplers = true;
    PRSDesc.Resources                  = Resources;
    PRSDesc.NumResources               = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Resource 'g_Texture_sampler' combined with texture 'g_Texture' is not a sampler");
}

TEST(PRSCreationFailureTest, InvalidAssignedSamplerStages)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid assigned sampler shader stage";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, "g_Texture_sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.UseCombinedTextureSamplers = true;
    PRSDesc.Resources                  = Resources;
    PRSDesc.NumResources               = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Texture 'g_Texture' is defined for the following shader stages: SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL, but sampler 'g_Texture_sampler' assigned to it uses only some of these stages: SHADER_TYPE_PIXEL");
}

TEST(PRSCreationFailureTest, InvalidAssignedSamplerVarType)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid assigned sampler var type";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_Texture_sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.UseCombinedTextureSamplers = true;
    PRSDesc.Resources                  = Resources;
    PRSDesc.NumResources               = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "The type (mutable) of texture resource 'g_Texture' does not match the type (static) of sampler 'g_Texture_sampler' that is assigned to it");
}

#if 0 // Unassigned sampler is a warning
TEST(PRSCreationFailureTest, UnassignedSampler)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Unassigned sampler";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_Texture2_sampler", 1, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.UseCombinedTextureSamplers = true;
    PRSDesc.Resources                  = Resources;
    PRSDesc.NumResources               = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Sampler 'g_Texture2_sampler' is not assigned to any texture");
}
#endif

TEST(PRSCreationFailureTest, NullImmutableSamplerName)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Null Immutable Sampler Name";
    ImmutableSamplerDesc ImmutableSamplers[]{
        {SHADER_TYPE_PIXEL, "g_ImmutableSampler", SamplerDesc{}},
        {SHADER_TYPE_PIXEL, nullptr, SamplerDesc{}}};
    PRSDesc.ImmutableSamplers    = ImmutableSamplers;
    PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);
    TestCreatePRSFailure(PRSDesc, "Desc.ImmutableSamplers[1].SamplerOrTextureName must not be null");

    PRSDesc.Name = "Null Immutable Sampler Name 2";

    ImmutableSamplers[1].SamplerOrTextureName = "";
    TestCreatePRSFailure(PRSDesc, "Desc.ImmutableSamplers[1].SamplerOrTextureName must not be empty");
}

TEST(PRSCreationFailureTest, OverlappingImmutableSamplerStages)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Overlapping Immutable Sampler Stages";
    ImmutableSamplerDesc ImmutableSamplers[]{
        {SHADER_TYPE_PIXEL | SHADER_TYPE_VERTEX, "g_ImmutableSampler", SamplerDesc{}},
        {SHADER_TYPE_PIXEL | SHADER_TYPE_HULL, "g_ImmutableSampler", SamplerDesc{}}};
    PRSDesc.ImmutableSamplers    = ImmutableSamplers;
    PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);
    TestCreatePRSFailure(PRSDesc, "Multiple immutable samplers with name 'g_ImmutableSampler' specify overlapping shader stages.");
}

TEST(PRSCreationFailureTest, UnknownImmutableSamplerShareStages)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Unknown Immutable Sampler ShaderStages";
    ImmutableSamplerDesc ImmutableSamplers[]{
        {SHADER_TYPE_PIXEL, "g_ImmutableSampler", SamplerDesc{}},
        {SHADER_TYPE_UNKNOWN, "g_ImmutableSampler2", SamplerDesc{}}};
    PRSDesc.ImmutableSamplers    = ImmutableSamplers;
    PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);
    TestCreatePRSFailure(PRSDesc, "Desc.ImmutableSamplers[1].ShaderStages must not be SHADER_TYPE_UNKNOWN");
}



TEST(PRSCreationFailureTest, NonSeparableProgs_ResourceStages)
{
    if (GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Features.SeparablePrograms)
    {
        GTEST_SKIP() << "This test is specific for non-separable programs";
    }

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Non-separable progs - resource stages";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_VERTEX, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "there are separate resources with the name 'g_Texture' in shader stages SHADER_TYPE_PIXEL and SHADER_TYPE_VERTEX");
}

TEST(PRSCreationFailureTest, NonSeparableProgs_ImtblSamplerStages)
{
    if (GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Features.SeparablePrograms)
    {
        GTEST_SKIP() << "This test is specific for non-separable programs";
    }

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Non-separable progs - immutable sampler stages";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);

    ImmutableSamplerDesc ImmutableSamplers[]{
        {SHADER_TYPE_VERTEX, "g_Texture", SamplerDesc{}},
        {SHADER_TYPE_PIXEL, "g_Texture", SamplerDesc{}}};
    PRSDesc.ImmutableSamplers    = ImmutableSamplers;
    PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);
    TestCreatePRSFailure(PRSDesc, "there are separate immutable samplers with the name 'g_Texture' in shader stages SHADER_TYPE_PIXEL and SHADER_TYPE_VERTEX");
}


TEST(PRSCreationFailureTest, D3D12_MultiStageResources)
{
    if (GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D12)
    {
        GTEST_SKIP() << "This test is specific for Direct3D12";
    }

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "D3D12 - multi stage resources";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_HULL | SHADER_TYPE_DOMAIN, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "separate resources with the name 'g_Texture' in shader stages SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL and SHADER_TYPE_HULL, SHADER_TYPE_DOMAIN");
}

TEST(PRSCreationFailureTest, D3D12_MultiStageImtblSamplers)
{
    if (GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D12)
    {
        GTEST_SKIP() << "This test is specific for Direct3D12";
    }

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "D3D12 - multi stage immutable samplers";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);

    ImmutableSamplerDesc ImmutableSamplers[]{
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Texture_sampler", SamplerDesc{}},
        {SHADER_TYPE_HULL | SHADER_TYPE_DOMAIN, "g_Texture_sampler", SamplerDesc{}}};
    PRSDesc.ImmutableSamplers    = ImmutableSamplers;
    PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

    TestCreatePRSFailure(PRSDesc, "separate immutable samplers with the name 'g_Texture_sampler' in shader stages SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL and SHADER_TYPE_HULL, SHADER_TYPE_DOMAIN");
}

#if 0 // Unassigned immutable sampler is a warning
TEST(PRSCreationFailureTest, UnassignedImmutableSampler)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Unassigned immutable sampler";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    ImmutableSamplerDesc ImmutableSamplers[]{
        {SHADER_TYPE_PIXEL, "g_Texture", SamplerDesc{}},
        {SHADER_TYPE_PIXEL, "g_Texture2", SamplerDesc{}}};
    PRSDesc.ImmutableSamplers    = ImmutableSamplers;
    PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

    PRSDesc.UseCombinedTextureSamplers = true;
    PRSDesc.Resources                  = Resources;
    PRSDesc.NumResources               = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Immutable sampler 'g_Texture2' is not assigned to any texture or sampler");
}
#endif

TEST(PRSCreationFailureTest, InvalidImmutableSamplerStages)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid immutable sampler stages";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    ImmutableSamplerDesc ImmutableSamplers[]{
        {SHADER_TYPE_VERTEX, "g_Texture", SamplerDesc{}}};
    PRSDesc.ImmutableSamplers    = ImmutableSamplers;
    PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

    PRSDesc.UseCombinedTextureSamplers = true;
    PRSDesc.Resources                  = Resources;
    PRSDesc.NumResources               = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Texture 'g_Texture' is defined for the following shader stages: SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL, but immutable sampler that is assigned to it uses only some of these stages: SHADER_TYPE_VERTEX");
}

TEST(PRSCreationFailureTest, InvalidInputAttachmentStages)
{
    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "Invalid input attachment stages";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "g_InputAttachment", 1, SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};

    PRSDesc.UseCombinedTextureSamplers = true;
    PRSDesc.Resources                  = Resources;
    PRSDesc.NumResources               = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Desc.Resources[0].ResourceType (INPUT_ATTACHMENT) is only supported in pixel shader but ShaderStages are SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL");
}

TEST(PRSCreationFailureTest, NoFormattedBuffers)
{
    if (GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Features.FormattedBuffers)
        GTEST_SKIP();

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "No formatted buffers";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "g_Buffer", 1, SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (FORMATTED_BUFFER). The flag can only be used if FormattedBuffers device feature is enabled");
}

TEST(PRSCreationFailureTest, NoShaderResourceRuntimeArrays)
{
    if (GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Features.ShaderResourceRuntimeArrays)
        GTEST_SKIP();

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "No shader resource runtime arrays";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "g_Buffer", 1, SHADER_RESOURCE_TYPE_BUFFER_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].Flags (RUNTIME_ARRAY). The flag can only be used if ShaderResourceRuntimeArrays device feature is enabled");
}

TEST(PRSCreationFailureTest, NoRayTracing)
{
    if (GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Features.RayTracing)
        GTEST_SKIP();

    PipelineResourceSignatureDesc PRSDesc;
    PRSDesc.Name = "No ray tracing";
    PipelineResourceDesc Resources[]{
        {SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_VERTEX, "g_AccelStruct", 1, SHADER_RESOURCE_TYPE_ACCEL_STRUCT, SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
    PRSDesc.Resources    = Resources;
    PRSDesc.NumResources = _countof(Resources);
    TestCreatePRSFailure(PRSDesc, "Incorrect Desc.Resources[1].ResourceType (ACCEL_STRUCT): ray tracing is not supported by device.");
}

} // namespace
