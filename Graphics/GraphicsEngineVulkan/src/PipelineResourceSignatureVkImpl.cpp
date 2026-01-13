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

#include "pch.h"

#include "PipelineResourceSignatureVkImpl.hpp"

#include "RenderDeviceVkImpl.hpp"
#include "SamplerVkImpl.hpp"
#include "TextureViewVkImpl.hpp"
#include "BufferVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"

#include "VulkanTypeConversions.hpp"
#include "DynamicLinearAllocator.hpp"
#include "SPIRVShaderResources.hpp"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

namespace
{

DescriptorType GetDescriptorType(const PipelineResourceDesc& Res)
{
    VERIFY((Res.Flags & ~GetValidPipelineResourceFlags(Res.ResourceType)) == 0,
           "Invalid resource flags. This error should've been caught by ValidatePipelineResourceSignatureDesc.");

    const bool WithDynamicOffset = (Res.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0;
    const bool CombinedSampler   = (Res.Flags & PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER) != 0;
    const bool UseTexelBuffer    = (Res.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0;
    const bool GeneralInputAtt   = (Res.Flags & PIPELINE_RESOURCE_FLAG_GENERAL_INPUT_ATTACHMENT) != 0;

    static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update the switch below to handle the new shader resource type");
    switch (Res.ResourceType)
    {
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
            return WithDynamicOffset ? DescriptorType::UniformBufferDynamic : DescriptorType::UniformBuffer;

        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
            return CombinedSampler ? DescriptorType::CombinedImageSampler : DescriptorType::SeparateImage;

        case SHADER_RESOURCE_TYPE_BUFFER_SRV:
            return UseTexelBuffer ? DescriptorType::UniformTexelBuffer :
                                    (WithDynamicOffset ? DescriptorType::StorageBufferDynamic_ReadOnly : DescriptorType::StorageBuffer_ReadOnly);

        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
            return DescriptorType::StorageImage;

        case SHADER_RESOURCE_TYPE_BUFFER_UAV:
            return UseTexelBuffer ? DescriptorType::StorageTexelBuffer :
                                    (WithDynamicOffset ? DescriptorType::StorageBufferDynamic : DescriptorType::StorageBuffer);

        case SHADER_RESOURCE_TYPE_SAMPLER:
            return DescriptorType::Sampler;

        case SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT:
            return GeneralInputAtt ? DescriptorType::InputAttachment_General : DescriptorType::InputAttachment;

        case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
            return DescriptorType::AccelerationStructure;

        default:
            UNEXPECTED("Unknown resource type");
            return DescriptorType::Unknown;
    }
}

Uint32 FindImmutableSamplerVk(const PipelineResourceDesc&          Res,
                              DescriptorType                       DescType,
                              const PipelineResourceSignatureDesc& Desc,
                              const char*                          SamplerSuffix)
{
    if (DescType == DescriptorType::CombinedImageSampler)
    {
        SamplerSuffix = nullptr;
    }
    else if (DescType == DescriptorType::Sampler)
    {
        // Use SamplerSuffix. If HLSL-style combined image samplers are not used,
        // SamplerSuffix will be null and we will be looking for the sampler itself.
    }
    else
    {
        UNEXPECTED("Immutable sampler can only be assigned to a sampled image or separate sampler");
        return InvalidImmutableSamplerIndex;
    }

    return FindImmutableSampler(Desc.ImmutableSamplers, Desc.NumImmutableSamplers, Res.ShaderStages, Res.Name, SamplerSuffix);
}

} // namespace

inline PipelineResourceSignatureVkImpl::CACHE_GROUP PipelineResourceSignatureVkImpl::GetResourceCacheGroup(const PipelineResourceDesc& Res)
{
    // NB: SetId is always 0 for static/mutable variables, and 1 - for dynamic ones.
    //     It is not the actual descriptor set index in the set layout!
    const size_t SetId             = VarTypeToDescriptorSetId(Res.VarType);
    const bool   WithDynamicOffset = (Res.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0;
    const bool   UseTexelBuffer    = (Res.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0;

    if (WithDynamicOffset && !UseTexelBuffer)
    {
        if (Res.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER)
            return static_cast<CACHE_GROUP>(SetId * CACHE_GROUP_COUNT_PER_VAR_TYPE + CACHE_GROUP_DYN_UB);

        if (Res.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV || Res.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV)
            return static_cast<CACHE_GROUP>(SetId * CACHE_GROUP_COUNT_PER_VAR_TYPE + CACHE_GROUP_DYN_SB);
    }
    return static_cast<CACHE_GROUP>(SetId * CACHE_GROUP_COUNT_PER_VAR_TYPE + CACHE_GROUP_OTHER);
}

inline PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID PipelineResourceSignatureVkImpl::VarTypeToDescriptorSetId(SHADER_RESOURCE_VARIABLE_TYPE VarType)
{
    return VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC ? DESCRIPTOR_SET_ID_DYNAMIC : DESCRIPTOR_SET_ID_STATIC_MUTABLE;
}


PipelineResourceSignatureVkImpl::PipelineResourceSignatureVkImpl(IReferenceCounters*                  pRefCounters,
                                                                 RenderDeviceVkImpl*                  pDevice,
                                                                 const PipelineResourceSignatureDesc& Desc,
                                                                 SHADER_TYPE                          ShaderStages,
                                                                 bool                                 bIsDeviceInternal) :
    TPipelineResourceSignatureBase{pRefCounters, pDevice, Desc, ShaderStages, bIsDeviceInternal}
{
    try
    {
        Initialize(
            GetRawAllocator(), Desc, /*CreateImmutableSamplers = */ true,
            [this]() //
            {
                CreateSetLayouts(/*IsSerialized*/ false);
            },
            [this]() //
            {
                return ShaderResourceCacheVk::GetRequiredMemorySize(GetNumDescriptorSets(), m_DescriptorSetSizes.data(), m_TotalInlineConstants);
            });
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

void PipelineResourceSignatureVkImpl::CreateSetLayouts(const bool IsSerialized)
{
    CacheOffsetsType CacheGroupSizes = {}; // Required cache size for each cache group
    BindingCountType BindingCount    = {}; // Binding count in each cache group

    // Count resources
    Uint32 StaticResourceCount = 0; // The total number of static resources in all stages
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const PipelineResourceDesc& ResDesc    = m_Desc.Resources[i];
        const CACHE_GROUP           CacheGroup = GetResourceCacheGroup(ResDesc);

        // For inline constants, GetArraySize() returns 1 (actual resource array size),
        // while ResDesc.ArraySize contains the number of 32-bit constants.
        const Uint32 DescriptorCount = ResDesc.GetArraySize();

        // All resources (including inline constant buffers) use descriptor sets.
        // Push constant selection is deferred to PSO creation time.
        BindingCount[CacheGroup] += 1;
        // Note that we may reserve space for separate immutable samplers, which will never be used, but this is OK.
        CacheGroupSizes[CacheGroup] += DescriptorCount;

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
        {
            StaticResourceCount += DescriptorCount;
        }
    }

    // Initialize static resource cache (now that we know resource and inline constant counts)
    if (StaticResourceCount > 0)
    {
        VERIFY_EXPR(GetNumStaticResStages() > 0);
        m_pStaticResCache->InitializeSets(GetRawAllocator(), 1, &StaticResourceCount, m_TotalStaticInlineConstants);
    }

    // Descriptor set mapping (static/mutable (0) or dynamic (1) -> set index)
    std::array<Uint32, DESCRIPTOR_SET_ID_NUM_SETS> DSMapping = {};
    {
        const Uint32 TotalStaticBindings =
            BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR] +
            BindingCount[CACHE_GROUP_DYN_SB_STAT_VAR] +
            BindingCount[CACHE_GROUP_OTHER_STAT_VAR];
        const Uint32 TotalDynamicBindings =
            BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR] +
            BindingCount[CACHE_GROUP_DYN_SB_DYN_VAR] +
            BindingCount[CACHE_GROUP_OTHER_DYN_VAR];

        Uint32 Idx = 0;

        DSMapping[DESCRIPTOR_SET_ID_STATIC_MUTABLE] = (TotalStaticBindings != 0 ? Idx++ : 0xFF);
        DSMapping[DESCRIPTOR_SET_ID_DYNAMIC]        = (TotalDynamicBindings != 0 ? Idx++ : 0xFF);
        VERIFY_EXPR(Idx <= MAX_DESCRIPTOR_SETS);
    }

    // Resource bindings as well as cache offsets are ordered by CACHE_GROUP in each descriptor set:
    //
    //      static/mutable vars set: |  Dynamic UBs  |  Dynamic SBs  |   The rest    |
    //      dynamic vars set:        |  Dynamic UBs  |  Dynamic SBs  |   The rest    |
    //
    // Note that resources in m_Desc.Resources are sorted by variable type
    CacheOffsetsType CacheGroupOffsets =
        {
            // static/mutable set
            0,
            CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR],
            CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR],
            // dynamic set
            0,
            CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR],
            CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR] //
        };
    BindingCountType BindingIndices =
        {
            // static/mutable set
            0,
            BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR],
            BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR] + BindingCount[CACHE_GROUP_DYN_SB_STAT_VAR],
            // dynamic set
            0,
            BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR],
            BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR] + BindingCount[CACHE_GROUP_DYN_SB_DYN_VAR] //
        };

    // Current offset in the static resource cache
    Uint32 StaticCacheOffset = 0;

    // Current inline constant offset for static resources
    Uint32 StaticInlineConstantOffset = 0;

    // Current inline constant buffer index
    Uint32 InlineConstantBufferIdx = 0;

    std::array<std::vector<VkDescriptorSetLayoutBinding>, DESCRIPTOR_SET_ID_NUM_SETS> vkSetLayoutBindings;

    DynamicLinearAllocator TempAllocator{GetRawAllocator(), 256};

    std::vector<bool> ImmutableSamplerWithResource(m_Desc.NumImmutableSamplers, false);
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const PipelineResourceDesc& ResDesc   = m_Desc.Resources[i];
        const DescriptorType        DescrType = GetDescriptorType(ResDesc);
        // NB: SetId is always 0 for static/mutable variables, and 1 - for dynamic ones.
        //     It is not the actual descriptor set index in the set layout!
        const DESCRIPTOR_SET_ID SetId      = VarTypeToDescriptorSetId(ResDesc.VarType);
        const CACHE_GROUP       CacheGroup = GetResourceCacheGroup(ResDesc);

        const bool IsInlineConst = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS) != 0;
        // For inline constants, GetArraySize() returns 1, while ResDesc.ArraySize contains the number of 32-bit constants.
        const Uint32 ResArraySize = ResDesc.GetArraySize();

        VERIFY(i == 0 || ResDesc.VarType >= m_Desc.Resources[i - 1].VarType, "Resources must be sorted by variable type");

        // If all resources are dynamic, then the signature contains only one descriptor set layout with index 0,
        // so remap SetId to the actual descriptor set index.
        VERIFY_EXPR(DSMapping[SetId] < MAX_DESCRIPTOR_SETS);

        // The sampler may not be yet initialized, but this is OK as all resources are initialized
        // in the same order as in m_Desc.Resources
        const Uint32 AssignedSamplerInd = DescrType == DescriptorType::SeparateImage ?
            FindAssignedSampler(ResDesc, ResourceAttribs::InvalidSamplerInd) :
            ResourceAttribs::InvalidSamplerInd;

        VkSampler* pVkImmutableSamplers = nullptr;
        if (DescrType == DescriptorType::CombinedImageSampler ||
            DescrType == DescriptorType::Sampler)
        {
            // Only search for immutable sampler for combined image samplers and separate samplers.
            // Note that for DescriptorType::SeparateImage with immutable sampler, we will initialize
            // a separate immutable sampler below. It will not be assigned to the image variable.
            const Uint32 SrcImmutableSamplerInd = FindImmutableSamplerVk(ResDesc, DescrType, m_Desc, GetCombinedSamplerSuffix());
            if (SrcImmutableSamplerInd != InvalidImmutableSamplerIndex)
            {
                const RefCntAutoPtr<SamplerVkImpl>& pSamplerVk = m_pImmutableSamplers[SrcImmutableSamplerInd];

                pVkImmutableSamplers = TempAllocator.ConstructArray<VkSampler>(ResDesc.ArraySize, pSamplerVk ? pSamplerVk->GetVkSampler() : VK_NULL_HANDLE);

                ImmutableSamplerWithResource[SrcImmutableSamplerInd] = true;
            }
        }

        ResourceAttribs* const pAttribs = m_pResourceAttribs + i;
        if (!IsSerialized)
        {
            new (pAttribs) ResourceAttribs //
                {
                    BindingIndices[CacheGroup],
                    AssignedSamplerInd,
                    ResArraySize,
                    DescrType,
                    DSMapping[SetId],
                    pVkImmutableSamplers != nullptr,
                    CacheGroupOffsets[CacheGroup],                                                    // SRBCacheOffset
                    ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC ? StaticCacheOffset : ~0u // StaticCacheOffset
                };
        }
        else
        {
            DEV_CHECK_ERR(pAttribs->BindingIndex == BindingIndices[CacheGroup],
                          "Deserialized binding index (", pAttribs->BindingIndex, ") is invalid: ", BindingIndices[CacheGroup], " is expected.");
            DEV_CHECK_ERR(pAttribs->SamplerInd == AssignedSamplerInd,
                          "Deserialized sampler index (", pAttribs->SamplerInd, ") is invalid: ", AssignedSamplerInd, " is expected.");
            DEV_CHECK_ERR(pAttribs->ArraySize == ResArraySize,
                          "Deserialized array size (", pAttribs->ArraySize, ") is invalid: ", ResArraySize, " is expected.");
            DEV_CHECK_ERR(pAttribs->GetDescriptorType() == DescrType, "Deserialized descriptor type is invalid");
            DEV_CHECK_ERR(pAttribs->DescrSet == DSMapping[SetId],
                          "Deserialized descriptor set (", pAttribs->DescrSet, ") is invalid: ", DSMapping[SetId], " is expected.");
            DEV_CHECK_ERR(pAttribs->IsImmutableSamplerAssigned() == (pVkImmutableSamplers != nullptr), "Immutable sampler flag is invalid");
            DEV_CHECK_ERR(pAttribs->SRBCacheOffset == CacheGroupOffsets[CacheGroup],
                          "SRB cache offset (", pAttribs->SRBCacheOffset, ") is invalid: ", CacheGroupOffsets[CacheGroup], " is expected.");
            DEV_CHECK_ERR(pAttribs->StaticCacheOffset == (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC ? StaticCacheOffset : ~0u),
                          "Static cache offset is invalid.");
        }

        // For inline constants, descriptor count is 1 (single uniform buffer)
        const Uint32 DescriptorCount = ResArraySize;

        BindingIndices[CacheGroup] += 1;

        // All resources use descriptor sets - push constant selection is deferred to PSO creation
        CacheGroupOffsets[CacheGroup] += DescriptorCount;

        VkDescriptorSetLayoutBinding vkSetLayoutBinding{};
        vkSetLayoutBinding.binding            = pAttribs->BindingIndex;
        vkSetLayoutBinding.descriptorCount    = DescriptorCount;
        vkSetLayoutBinding.stageFlags         = ShaderTypesToVkShaderStageFlags(ResDesc.ShaderStages);
        vkSetLayoutBinding.pImmutableSamplers = pVkImmutableSamplers;
        vkSetLayoutBinding.descriptorType     = DescriptorTypeToVkDescriptorType(pAttribs->GetDescriptorType());
        vkSetLayoutBindings[SetId].push_back(vkSetLayoutBinding);

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
        {
            VERIFY(pAttribs->DescrSet == 0, "Static resources must always be allocated in descriptor set 0");
            m_pStaticResCache->InitializeResources(pAttribs->DescrSet, StaticCacheOffset, DescriptorCount,
                                                   pAttribs->GetDescriptorType(), pAttribs->IsImmutableSamplerAssigned(),
                                                   IsInlineConst ? StaticInlineConstantOffset : ~0u,
                                                   IsInlineConst ? ResDesc.ArraySize : 0);
            StaticCacheOffset += DescriptorCount;

            if (IsInlineConst)
                StaticInlineConstantOffset += ResDesc.ArraySize; // For inline constants, ArraySize is the number of 32-bit constants
        }

        // Initialize inline constant buffers.
        // All inline constants get descriptor set bindings and emulated buffers.
        if (IsInlineConst)
        {
            VERIFY(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
                   "Only constant buffers can have INLINE_CONSTANTS flag");

            InlineConstantBufferAttribsVk& InlineCBAttribs = m_pInlineConstantBuffers[InlineConstantBufferIdx++];
            InlineCBAttribs.ResIndex                       = i; // Resource index for unique identification
            InlineCBAttribs.DescrSet                       = pAttribs->DescrSet;
            InlineCBAttribs.BindingIndex                   = pAttribs->BindingIndex;
            InlineCBAttribs.NumConstants                   = ResDesc.ArraySize; // For inline constants, ArraySize is the number of 32-bit constants
            InlineCBAttribs.SRBCacheOffset                 = pAttribs->SRBCacheOffset;

            // Create a shared buffer in the Signature for all inline constants.
            // All SRBs will reference this same buffer.
            InlineCBAttribs.pBuffer = CreateInlineConstantBuffer(ResDesc.Name, ResDesc.ArraySize);
        }
    }
    VERIFY_EXPR(InlineConstantBufferIdx == m_NumInlineConstantBuffers);
    VERIFY_EXPR(StaticInlineConstantOffset == m_TotalStaticInlineConstants);

#ifdef DILIGENT_DEBUG
    if (m_pStaticResCache != nullptr)
    {
        m_pStaticResCache->DbgVerifyResourceInitialization();
    }
#endif

    m_DynamicUniformBufferCount = static_cast<Uint16>(CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR]);
    m_DynamicStorageBufferCount = static_cast<Uint16>(CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR]);
    VERIFY_EXPR(m_DynamicUniformBufferCount == CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR]);
    VERIFY_EXPR(m_DynamicStorageBufferCount == CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR]);

    VERIFY_EXPR(m_pStaticResCache == nullptr || const_cast<const ShaderResourceCacheVk*>(m_pStaticResCache)->GetDescriptorSet(0).GetSize() == StaticCacheOffset);
    VERIFY_EXPR(CacheGroupOffsets[CACHE_GROUP_DYN_UB_STAT_VAR] == CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR]);
    VERIFY_EXPR(CacheGroupOffsets[CACHE_GROUP_DYN_SB_STAT_VAR] == CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR]);
    VERIFY_EXPR(CacheGroupOffsets[CACHE_GROUP_OTHER_STAT_VAR] == CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_OTHER_STAT_VAR]);
    VERIFY_EXPR(CacheGroupOffsets[CACHE_GROUP_DYN_UB_DYN_VAR] == CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR]);
    VERIFY_EXPR(CacheGroupOffsets[CACHE_GROUP_DYN_SB_DYN_VAR] == CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR]);
    VERIFY_EXPR(CacheGroupOffsets[CACHE_GROUP_OTHER_DYN_VAR] == CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR] + CacheGroupSizes[CACHE_GROUP_OTHER_DYN_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_DYN_UB_STAT_VAR] == BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_DYN_SB_STAT_VAR] == BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR] + BindingCount[CACHE_GROUP_DYN_SB_STAT_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_OTHER_STAT_VAR] == BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR] + BindingCount[CACHE_GROUP_DYN_SB_STAT_VAR] + BindingCount[CACHE_GROUP_OTHER_STAT_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_DYN_UB_DYN_VAR] == BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_DYN_SB_DYN_VAR] == BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR] + BindingCount[CACHE_GROUP_DYN_SB_DYN_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_OTHER_DYN_VAR] == BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR] + BindingCount[CACHE_GROUP_DYN_SB_DYN_VAR] + BindingCount[CACHE_GROUP_OTHER_DYN_VAR]);

    // Add immutable samplers that do not exist in m_Desc.Resources, as in the example below:
    //
    //  Shader:
    //      Texture2D    g_Texture;
    //      SamplerState g_Texture_sampler;
    //
    //  Host:
    //      PipelineResourceDesc Resources[]         = {{SHADER_TYPE_PIXEL, "g_Texture", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, ...}};
    //      ImmutableSamplerDesc ImmutableSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", SamDesc}};
    //
    //  In the situation above, 'g_Texture_sampler' will not be assigned to separate image
    // 'g_Texture'. Instead, we initialize an immutable sampler with name 'g_Texture'. It will then
    // be retrieved by PSO with PipelineLayoutVk::GetImmutableSamplerInfo() when the PSO initializes
    // 'g_Texture_sampler'.
    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        if (ImmutableSamplerWithResource[i])
        {
            // Immutable sampler has already been initialized as resource
            continue;
        }

        ImmutableSamplerAttribsVk&         ImtblSampAttribs = m_pImmutableSamplerAttribs[i];
        const ImmutableSamplerDesc&        SamplerDesc      = m_Desc.ImmutableSamplers[i];
        const RefCntAutoPtr<SamplerVkImpl> pSamplerVk       = m_pImmutableSamplers[i];

        // If static/mutable descriptor set layout is empty, then add samplers to dynamic set.
        const DESCRIPTOR_SET_ID SetId = (DSMapping[DESCRIPTOR_SET_ID_STATIC_MUTABLE] < MAX_DESCRIPTOR_SETS) ?
            DESCRIPTOR_SET_ID_STATIC_MUTABLE :
            DESCRIPTOR_SET_ID_DYNAMIC;
        DEV_CHECK_ERR(DSMapping[SetId] < MAX_DESCRIPTOR_SETS,
                      "There are no descriptor sets in this signature, which indicates there are no other "
                      "resources besides immutable samplers. This is not currently allowed.");


        Uint32& BindingIndex = BindingIndices[SetId * 3 + CACHE_GROUP_OTHER];
        if (!IsSerialized)
        {
            ImtblSampAttribs.DescrSet     = DSMapping[SetId];
            ImtblSampAttribs.BindingIndex = BindingIndex;
        }
        else
        {
            DEV_CHECK_ERR(ImtblSampAttribs.DescrSet == DSMapping[SetId],
                          "Immutable sampler descriptor set (", ImtblSampAttribs.DescrSet, ") is invalid: ", DSMapping[SetId], " is expected.");
            DEV_CHECK_ERR(ImtblSampAttribs.BindingIndex == BindingIndex,
                          "Immutable sampler bind index (", ImtblSampAttribs.BindingIndex, ") is invalid: ", BindingIndex, " is expected.");
        }
        ++BindingIndex;

        VkDescriptorSetLayoutBinding vkSetLayoutBinding{};
        vkSetLayoutBinding.binding            = ImtblSampAttribs.BindingIndex;
        vkSetLayoutBinding.descriptorCount    = 1;
        vkSetLayoutBinding.stageFlags         = ShaderTypesToVkShaderStageFlags(SamplerDesc.ShaderStages);
        vkSetLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
        vkSetLayoutBinding.pImmutableSamplers = TempAllocator.Construct<VkSampler>(pSamplerVk ? pSamplerVk->GetVkSampler() : VK_NULL_HANDLE);
        vkSetLayoutBindings[SetId].push_back(vkSetLayoutBinding);
    }

    Uint32 NumSets = 0;
    if (DSMapping[DESCRIPTOR_SET_ID_STATIC_MUTABLE] < MAX_DESCRIPTOR_SETS)
    {
        m_DescriptorSetSizes[DSMapping[DESCRIPTOR_SET_ID_STATIC_MUTABLE]] =
            CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] +
            CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR] +
            CacheGroupSizes[CACHE_GROUP_OTHER_STAT_VAR];
        ++NumSets;
    }

    if (DSMapping[DESCRIPTOR_SET_ID_DYNAMIC] < MAX_DESCRIPTOR_SETS)
    {
        m_DescriptorSetSizes[DSMapping[DESCRIPTOR_SET_ID_DYNAMIC]] =
            CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR] +
            CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR] +
            CacheGroupSizes[CACHE_GROUP_OTHER_DYN_VAR];
        ++NumSets;
    }
#ifdef DILIGENT_DEBUG
    for (Uint32 i = 0; i < NumSets; ++i)
        VERIFY_EXPR(m_DescriptorSetSizes[i] != ~0U && m_DescriptorSetSizes[i] > 0);
#else
    (void)NumSets;
#endif

    VkDescriptorSetLayoutCreateInfo SetLayoutCI = {};

    SetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    SetLayoutCI.pNext = nullptr;
    SetLayoutCI.flags = 0;

    if (HasDevice())
    {
        const VulkanUtilities::LogicalDevice& LogicalDevice = GetDevice()->GetLogicalDevice();

        for (size_t i = 0; i < vkSetLayoutBindings.size(); ++i)
        {
            auto& vkSetLayoutBinding = vkSetLayoutBindings[i];
            if (vkSetLayoutBinding.empty())
                continue;

            SetLayoutCI.bindingCount = StaticCast<uint32_t>(vkSetLayoutBinding.size());
            SetLayoutCI.pBindings    = vkSetLayoutBinding.data();
            m_VkDescrSetLayouts[i]   = LogicalDevice.CreateDescriptorSetLayout(SetLayoutCI);
        }
        VERIFY_EXPR(NumSets == GetNumDescriptorSets());
    }
}

PipelineResourceSignatureVkImpl::~PipelineResourceSignatureVkImpl()
{
    Destruct();
}

void PipelineResourceSignatureVkImpl::Destruct()
{
    for (VulkanUtilities::DescriptorSetLayoutWrapper& Layout : m_VkDescrSetLayouts)
    {
        if (Layout)
            GetDevice()->SafeReleaseDeviceObject(std::move(Layout), ~0ull);
    }

    TPipelineResourceSignatureBase::Destruct();
}

void PipelineResourceSignatureVkImpl::InitSRBResourceCache(ShaderResourceCacheVk& ResourceCache)
{
    const Uint32 NumSets = GetNumDescriptorSets();
#ifdef DILIGENT_DEBUG
    for (Uint32 i = 0; i < NumSets; ++i)
        VERIFY_EXPR(m_DescriptorSetSizes[i] != ~0U);
#endif

    IMemoryAllocator& CacheMemAllocator = m_SRBMemAllocator.GetResourceCacheDataAllocator(0);
    ResourceCache.InitializeSets(CacheMemAllocator, NumSets, m_DescriptorSetSizes.data(), m_TotalInlineConstants);

    const Uint32                   TotalResources = GetTotalResourceCount();
    const ResourceCacheContentType CacheType      = ResourceCache.GetContentType();

    Uint32 InlineConstantOffset = 0;
    for (Uint32 r = 0; r < TotalResources; ++r)
    {
        const PipelineResourceDesc& ResDesc = GetResourceDesc(r);
        const ResourceAttribs&      Attr    = GetResourceAttribs(r);

        // For inline constants, GetArraySize() returns 1 (actual array size),
        // while ArraySize contains the number of 32-bit constants
        const Uint32 ResArraySize  = ResDesc.GetArraySize();
        const bool   IsInlineConst = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS) != 0;

        ResourceCache.InitializeResources(Attr.DescrSet, Attr.CacheOffset(CacheType), ResArraySize,
                                          Attr.GetDescriptorType(), Attr.IsImmutableSamplerAssigned(),
                                          IsInlineConst ? InlineConstantOffset : ~0u,
                                          IsInlineConst ? ResDesc.ArraySize : 0);
        if (IsInlineConst)
            InlineConstantOffset += ResDesc.ArraySize;
    }
    VERIFY_EXPR(InlineConstantOffset == m_TotalInlineConstants);

#ifdef DILIGENT_DEBUG
    ResourceCache.DbgVerifyResourceInitialization();
#endif

    if (VkDescriptorSetLayout vkLayout = GetVkDescriptorSetLayout(DESCRIPTOR_SET_ID_STATIC_MUTABLE))
    {
        const char* DescrSetName = "Static/Mutable Descriptor Set";
#ifdef DILIGENT_DEVELOPMENT
        std::string _DescrSetName{m_Desc.Name};
        _DescrSetName.append(" - static/mutable set");
        DescrSetName = _DescrSetName.c_str();
#endif
        DescriptorSetAllocation SetAllocation = GetDevice()->AllocateDescriptorSet(~Uint64{0}, vkLayout, DescrSetName);
        ResourceCache.AssignDescriptorSetAllocation(GetDescriptorSetIndex<DESCRIPTOR_SET_ID_STATIC_MUTABLE>(), std::move(SetAllocation));
    }

    // Bind shared inline constant buffers to the resource cache.
    // This must be done after descriptor set allocation so that descriptor writes work correctly
    // The buffers are created in CreateSetLayouts() and shared by all SRBs.
    // Push constant selection is deferred to PSO creation - all inline constants get buffers bound here.
    for (Uint32 i = 0; i < m_NumInlineConstantBuffers; ++i)
    {
        const InlineConstantBufferAttribsVk& InlineCBAttr = GetInlineConstantBufferAttribs(i);
        VERIFY_EXPR(InlineCBAttr.pBuffer);

        // Use ResIndex to access the resource attributes
        const ResourceAttribs& Attr        = GetResourceAttribs(InlineCBAttr.ResIndex);
        const Uint32           CacheOffset = Attr.CacheOffset(CacheType);

        // Bind the shared uniform buffer to the resource cache.
        // Note that since we use SetResource, the buffer will count towards the number of
        // dynamic buffers in the cache.
        ResourceCache.SetResource(
            &GetDevice()->GetLogicalDevice(),
            Attr.DescrSet,
            CacheOffset,
            {
                Attr.BindingIndex,
                0, // ArrayIndex
                RefCntAutoPtr<IDeviceObject>{InlineCBAttr.pBuffer},
                0,                                         // BufferBaseOffset
                InlineCBAttr.NumConstants * sizeof(Uint32) // BufferRangeSize
            });
    }
}

void PipelineResourceSignatureVkImpl::CopyStaticResources(ShaderResourceCacheVk& DstResourceCache) const
{
    if (!HasDescriptorSet(DESCRIPTOR_SET_ID_STATIC_MUTABLE) || m_pStaticResCache == nullptr)
        return;

    // SrcResourceCache contains only static resources.
    // In case of SRB, DstResourceCache contains static, mutable and dynamic resources.
    // In case of Signature, DstResourceCache contains only static resources.
    const ShaderResourceCacheVk&                SrcResourceCache = *m_pStaticResCache;
    const Uint32                                StaticSetIdx     = GetDescriptorSetIndex<DESCRIPTOR_SET_ID_STATIC_MUTABLE>();
    const ShaderResourceCacheVk::DescriptorSet& SrcDescrSet      = SrcResourceCache.GetDescriptorSet(StaticSetIdx);
    const ShaderResourceCacheVk::DescriptorSet& DstDescrSet      = const_cast<const ShaderResourceCacheVk&>(DstResourceCache).GetDescriptorSet(StaticSetIdx);
    const auto                                  ResIdxRange      = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    const ResourceCacheContentType              SrcCacheType     = SrcResourceCache.GetContentType();
    const ResourceCacheContentType              DstCacheType     = DstResourceCache.GetContentType();

    for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
    {
        const PipelineResourceDesc& ResDesc = GetResourceDesc(r);
        const ResourceAttribs&      Attr    = GetResourceAttribs(r);
        VERIFY_EXPR(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER && Attr.IsImmutableSamplerAssigned())
            continue; // Skip immutable separate samplers

        if (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS)
        {
            // Copy inline constant staging data from static cache to the SRB cache
            const Uint32                           SrcCacheOffset = Attr.CacheOffset(SrcCacheType);
            const ShaderResourceCacheVk::Resource& SrcCachedRes   = SrcDescrSet.GetResource(SrcCacheOffset);
            const Uint32                           DstCacheOffset = Attr.CacheOffset(DstCacheType);
            const ShaderResourceCacheVk::Resource& DstCachedRes   = DstDescrSet.GetResource(DstCacheOffset);

            VERIFY_EXPR(SrcCachedRes.pInlineConstantData != nullptr && DstCachedRes.pInlineConstantData != nullptr);
            // ArraySize contains the number of 32-bit constants for inline constants
            memcpy(DstCachedRes.pInlineConstantData, SrcCachedRes.pInlineConstantData, ResDesc.ArraySize * sizeof(Uint32));
        }
        else
        {
            // For regular resources (not inline constants), copy each array element
            for (Uint32 ArrInd = 0; ArrInd < ResDesc.GetArraySize(); ++ArrInd)
            {
                const Uint32                           SrcCacheOffset = Attr.CacheOffset(SrcCacheType) + ArrInd;
                const ShaderResourceCacheVk::Resource& SrcCachedRes   = SrcDescrSet.GetResource(SrcCacheOffset);
                IDeviceObject*                         pObject        = SrcCachedRes.pObject;
                if (pObject == nullptr)
                {
                    if (DstCacheType == ResourceCacheContentType::SRB)
                        LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");
                    continue;
                }

                const Uint32                           DstCacheOffset = Attr.CacheOffset(DstCacheType) + ArrInd;
                const ShaderResourceCacheVk::Resource& DstCachedRes   = DstDescrSet.GetResource(DstCacheOffset);
                VERIFY_EXPR(SrcCachedRes.Type == DstCachedRes.Type);

                const IDeviceObject* pCachedResource = DstCachedRes.pObject;
                if (pCachedResource != pObject)
                {
                    DEV_CHECK_ERR(pCachedResource == nullptr, "Static resource has already been initialized, and the new resource does not match previously assigned resource");
                    DstResourceCache.SetResource(&GetDevice()->GetLogicalDevice(),
                                                 StaticSetIdx,
                                                 DstCacheOffset,
                                                 {
                                                     Attr.BindingIndex,
                                                     ArrInd,
                                                     RefCntAutoPtr<IDeviceObject>{SrcCachedRes.pObject},
                                                     SrcCachedRes.BufferBaseOffset,
                                                     SrcCachedRes.BufferRangeSize //
                                                 });
                }
            }
        }
    }

#ifdef DILIGENT_DEBUG
    DstResourceCache.DbgVerifyDynamicBuffersCounter();
#endif
}

template <>
Uint32 PipelineResourceSignatureVkImpl::GetDescriptorSetIndex<PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_STATIC_MUTABLE>() const
{
    VERIFY(HasDescriptorSet(DESCRIPTOR_SET_ID_STATIC_MUTABLE), "This signature does not have static/mutable descriptor set");
    return 0;
}

template <>
Uint32 PipelineResourceSignatureVkImpl::GetDescriptorSetIndex<PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_DYNAMIC>() const
{
    VERIFY(HasDescriptorSet(DESCRIPTOR_SET_ID_DYNAMIC), "This signature does not have dynamic descriptor set");
    return HasDescriptorSet(DESCRIPTOR_SET_ID_STATIC_MUTABLE) ? 1 : 0;
}

void PipelineResourceSignatureVkImpl::CommitDynamicResources(const ShaderResourceCacheVk& ResourceCache,
                                                             VkDescriptorSet              vkDynamicDescriptorSet) const
{
    VERIFY(HasDescriptorSet(DESCRIPTOR_SET_ID_DYNAMIC), "This signature does not contain dynamic resources");
    VERIFY_EXPR(vkDynamicDescriptorSet != VK_NULL_HANDLE);
    VERIFY_EXPR(ResourceCache.GetContentType() == ResourceCacheContentType::SRB);

#ifdef DILIGENT_DEBUG
    static constexpr size_t ImgUpdateBatchSize          = 4;
    static constexpr size_t BuffUpdateBatchSize         = 2;
    static constexpr size_t TexelBuffUpdateBatchSize    = 2;
    static constexpr size_t AccelStructBatchSize        = 2;
    static constexpr size_t WriteDescriptorSetBatchSize = 2;
#else
    static constexpr size_t ImgUpdateBatchSize          = 64;
    static constexpr size_t BuffUpdateBatchSize         = 32;
    static constexpr size_t TexelBuffUpdateBatchSize    = 16;
    static constexpr size_t AccelStructBatchSize        = 16;
    static constexpr size_t WriteDescriptorSetBatchSize = 32;
#endif

    // Do not zero-initialize arrays!
    std::array<VkDescriptorImageInfo, ImgUpdateBatchSize>                          DescrImgInfoArr;
    std::array<VkDescriptorBufferInfo, BuffUpdateBatchSize>                        DescrBuffInfoArr;
    std::array<VkBufferView, TexelBuffUpdateBatchSize>                             DescrBuffViewArr;
    std::array<VkWriteDescriptorSetAccelerationStructureKHR, AccelStructBatchSize> DescrAccelStructArr;
    std::array<VkWriteDescriptorSet, WriteDescriptorSetBatchSize>                  WriteDescrSetArr;

    auto DescrImgIt      = DescrImgInfoArr.begin();
    auto DescrBuffIt     = DescrBuffInfoArr.begin();
    auto BuffViewIt      = DescrBuffViewArr.begin();
    auto AccelStructIt   = DescrAccelStructArr.begin();
    auto WriteDescrSetIt = WriteDescrSetArr.begin();

    const Uint32                                DynamicSetIdx  = GetDescriptorSetIndex<DESCRIPTOR_SET_ID_DYNAMIC>();
    const ShaderResourceCacheVk::DescriptorSet& SetResources   = ResourceCache.GetDescriptorSet(DynamicSetIdx);
    const VulkanUtilities::LogicalDevice&       LogicalDevice  = GetDevice()->GetLogicalDevice();
    const std::pair<Uint32, Uint32>             DynResIdxRange = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

    constexpr ResourceCacheContentType CacheType = ResourceCacheContentType::SRB;

    for (Uint32 ResIdx = DynResIdxRange.first, ArrElem = 0; ResIdx < DynResIdxRange.second;)
    {
        const PipelineResourceAttribsType& Attr        = GetResourceAttribs(ResIdx);
        const Uint32                       CacheOffset = Attr.CacheOffset(CacheType);
        const Uint32                       ArraySize   = Attr.ArraySize;
        const DescriptorType               DescrType   = Attr.GetDescriptorType();

#ifdef DILIGENT_DEBUG
        {
            const PipelineResourceDesc& Res = GetResourceDesc(ResIdx);
            VERIFY_EXPR(ArraySize == GetResourceDesc(ResIdx).GetArraySize());
            VERIFY_EXPR(Res.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
        }
#endif

        WriteDescrSetIt->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        WriteDescrSetIt->pNext = nullptr;
        VERIFY(SetResources.GetVkDescriptorSet() == VK_NULL_HANDLE, "Dynamic descriptor set must not be assigned to the resource cache");
        WriteDescrSetIt->dstSet = vkDynamicDescriptorSet;
        VERIFY(WriteDescrSetIt->dstSet != VK_NULL_HANDLE, "Vulkan descriptor set must not be null");
        WriteDescrSetIt->dstBinding      = Attr.BindingIndex;
        WriteDescrSetIt->dstArrayElement = ArrElem;
        // descriptorType must be the same type as that specified in VkDescriptorSetLayoutBinding for dstSet at dstBinding.
        // The type of the descriptor also controls which array the descriptors are taken from. (13.2.4)
        WriteDescrSetIt->descriptorType  = DescriptorTypeToVkDescriptorType(DescrType);
        WriteDescrSetIt->descriptorCount = 0;
        // Zero-initialize array pointers as some implementations (e.g. Android Emulator) still check them even
        // if they are not used.
        WriteDescrSetIt->pImageInfo       = nullptr;
        WriteDescrSetIt->pBufferInfo      = nullptr;
        WriteDescrSetIt->pTexelBufferView = nullptr;

        auto WriteArrayElements = [&](auto DescrType, auto& DescrIt, const auto& DescrArr) //
        {
            while (ArrElem < ArraySize && DescrIt != DescrArr.end())
            {
                if (const ShaderResourceCacheVk::Resource& CachedRes = SetResources.GetResource(CacheOffset + (ArrElem++)))
                {
                    *DescrIt = CachedRes.GetDescriptorWriteInfo<DescrType>();
                    ++DescrIt;
                    ++WriteDescrSetIt->descriptorCount;
                }
                else
                {
                    if (WriteDescrSetIt->descriptorCount == 0)
                        WriteDescrSetIt->dstArrayElement = ArrElem; // No elements have been written yet
                    else
                        break; // We need to use a new VkWriteDescriptorSet since we skipped an array element
                }
            }
        };

        // For every resource type, try to batch as many descriptor updates as we can
        static_assert(static_cast<Uint32>(DescriptorType::Count) == 16, "Please update the switch below to handle the new descriptor type");
        switch (DescrType)
        {
            case DescriptorType::UniformBuffer:
            case DescriptorType::UniformBufferDynamic:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                WriteArrayElements(std::integral_constant<DescriptorType, DescriptorType::UniformBuffer>{}, DescrBuffIt, DescrBuffInfoArr);
                break;

            case DescriptorType::StorageBuffer:
            case DescriptorType::StorageBufferDynamic:
            case DescriptorType::StorageBuffer_ReadOnly:
            case DescriptorType::StorageBufferDynamic_ReadOnly:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                WriteArrayElements(std::integral_constant<DescriptorType, DescriptorType::StorageBuffer>{}, DescrBuffIt, DescrBuffInfoArr);
                break;

            case DescriptorType::UniformTexelBuffer:
            case DescriptorType::StorageTexelBuffer:
            case DescriptorType::StorageTexelBuffer_ReadOnly:
                WriteDescrSetIt->pTexelBufferView = &(*BuffViewIt);
                WriteArrayElements(std::integral_constant<DescriptorType, DescriptorType::UniformTexelBuffer>{}, BuffViewIt, DescrBuffViewArr);
                break;

            case DescriptorType::CombinedImageSampler:
            case DescriptorType::SeparateImage:
            case DescriptorType::StorageImage:
                WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                WriteArrayElements(std::integral_constant<DescriptorType, DescriptorType::SeparateImage>{}, DescrImgIt, DescrImgInfoArr);
                break;

            case DescriptorType::InputAttachment:
            case DescriptorType::InputAttachment_General:
                WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                WriteArrayElements(std::integral_constant<DescriptorType, DescriptorType::InputAttachment>{}, DescrImgIt, DescrImgInfoArr);
                break;

            case DescriptorType::Sampler:
                // Immutable samplers are permanently bound into the set layout; later binding a sampler
                // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
                if (!Attr.IsImmutableSamplerAssigned())
                {
                    WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                    WriteArrayElements(std::integral_constant<DescriptorType, DescriptorType::Sampler>{}, DescrImgIt, DescrImgInfoArr);
                }
                else
                {
                    // Go to the next resource
                    ArrElem                          = ArraySize;
                    WriteDescrSetIt->dstArrayElement = ArraySize;
                }
                break;

            case DescriptorType::AccelerationStructure:
                WriteDescrSetIt->pNext = &(*AccelStructIt);
                WriteArrayElements(std::integral_constant<DescriptorType, DescriptorType::AccelerationStructure>{}, AccelStructIt, DescrAccelStructArr);
                break;

            default:
                UNEXPECTED("Unexpected resource type");
        }

        if (ArrElem == ArraySize)
        {
            ArrElem = 0;
            ++ResIdx;
        }

        // descriptorCount == 0 for immutable separate samplers or null resources
        if (WriteDescrSetIt->descriptorCount > 0)
            ++WriteDescrSetIt;

        // If we ran out of space in any of the arrays or if we processed all resources,
        // flush pending updates and reset iterators
        if (DescrImgIt == DescrImgInfoArr.end() ||
            DescrBuffIt == DescrBuffInfoArr.end() ||
            BuffViewIt == DescrBuffViewArr.end() ||
            AccelStructIt == DescrAccelStructArr.end() ||
            WriteDescrSetIt == WriteDescrSetArr.end())
        {
            Uint32 DescrWriteCount = static_cast<Uint32>(std::distance(WriteDescrSetArr.begin(), WriteDescrSetIt));
            if (DescrWriteCount > 0)
                LogicalDevice.UpdateDescriptorSets(DescrWriteCount, WriteDescrSetArr.data(), 0, nullptr);

            DescrImgIt      = DescrImgInfoArr.begin();
            DescrBuffIt     = DescrBuffInfoArr.begin();
            BuffViewIt      = DescrBuffViewArr.begin();
            AccelStructIt   = DescrAccelStructArr.begin();
            WriteDescrSetIt = WriteDescrSetArr.begin();
        }
    }

    Uint32 DescrWriteCount = static_cast<Uint32>(std::distance(WriteDescrSetArr.begin(), WriteDescrSetIt));
    if (DescrWriteCount > 0)
        LogicalDevice.UpdateDescriptorSets(DescrWriteCount, WriteDescrSetArr.data(), 0, nullptr);
}


#ifdef DILIGENT_DEVELOPMENT
bool PipelineResourceSignatureVkImpl::DvpValidateCommittedResource(const DeviceContextVkImpl*        pDeviceCtx,
                                                                   const SPIRVShaderResourceAttribs& SPIRVAttribs,
                                                                   Uint32                            ResIndex,
                                                                   const ShaderResourceCacheVk&      ResourceCache,
                                                                   const char*                       ShaderName,
                                                                   const char*                       PSOName) const
{
    VERIFY_EXPR(ResIndex < m_Desc.NumResources);
    const PipelineResourceDesc& ResDesc    = m_Desc.Resources[ResIndex];
    const ResourceAttribs&      ResAttribs = m_pResourceAttribs[ResIndex];
    VERIFY(strcmp(ResDesc.Name, SPIRVAttribs.Name) == 0, "Inconsistent resource names");

    if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER && ResAttribs.IsImmutableSamplerAssigned())
        return true; // Skip immutable separate samplers

    const ShaderResourceCacheVk::DescriptorSet& DescrSetResources = ResourceCache.GetDescriptorSet(ResAttribs.DescrSet);
    const ResourceCacheContentType              CacheType         = ResourceCache.GetContentType();
    const Uint32                                CacheOffset       = ResAttribs.CacheOffset(CacheType);

    VERIFY_EXPR(SPIRVAttribs.ArraySize <= ResAttribs.ArraySize);

    bool BindingsOK = true;
    for (Uint32 ArrIndex = 0; ArrIndex < SPIRVAttribs.ArraySize; ++ArrIndex)
    {
        const ShaderResourceCacheVk::Resource& Res = DescrSetResources.GetResource(CacheOffset + ArrIndex);
        if (Res.IsNull())
        {
            LOG_ERROR_MESSAGE("No resource is bound to variable '", GetShaderResourcePrintName(SPIRVAttribs, ArrIndex),
                              "' in shader '", ShaderName, "' of PSO '", PSOName, "'");
            BindingsOK = false;
            continue;
        }

        if (ResAttribs.IsCombinedWithSampler())
        {
            const PipelineResourceDesc& SamplerResDesc = GetResourceDesc(ResAttribs.SamplerInd);
            const ResourceAttribs&      SamplerAttribs = GetResourceAttribs(ResAttribs.SamplerInd);
            VERIFY_EXPR(SamplerResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);
            VERIFY_EXPR(SamplerResDesc.ArraySize == 1 || SamplerResDesc.ArraySize == ResDesc.ArraySize);
            if (!SamplerAttribs.IsImmutableSamplerAssigned())
            {
                if (ArrIndex < SamplerResDesc.ArraySize)
                {
                    const ShaderResourceCacheVk::DescriptorSet& SamDescrSetResources = ResourceCache.GetDescriptorSet(SamplerAttribs.DescrSet);
                    const Uint32                                SamCacheOffset       = SamplerAttribs.CacheOffset(CacheType);
                    const ShaderResourceCacheVk::Resource&      Sam                  = SamDescrSetResources.GetResource(SamCacheOffset + ArrIndex);
                    if (Sam.IsNull())
                    {
                        LOG_ERROR_MESSAGE("No sampler is bound to sampler variable '", GetShaderResourcePrintName(SamplerResDesc, ArrIndex),
                                          "' combined with texture '", SPIRVAttribs.Name, "' in shader '", ShaderName, "' of PSO '", PSOName, "'.");
                        BindingsOK = false;
                    }
                }
            }
        }

        switch (ResAttribs.GetDescriptorType())
        {
            case DescriptorType::UniformBuffer:
            case DescriptorType::UniformBufferDynamic:
                VERIFY_EXPR(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER);
                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (const BufferVkImpl* pBufferVk = Res.pObject.RawPtr<BufferVkImpl>())
                {
                    // Skip dynamic allocation verification for inline constant buffers.
                    // These are internal buffers managed by the signature and are updated
                    // via UpdateInlineConstantBuffers() before each draw/dispatch.
                    if ((ResDesc.Flags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS) == 0)
                    {
                        pDeviceCtx->DvpVerifyDynamicAllocation(pBufferVk);
                    }

                    if ((pBufferVk->GetDesc().Size < SPIRVAttribs.BufferStaticSize) &&
                        (GetDevice()->GetValidationFlags() & VALIDATION_FLAG_CHECK_SHADER_BUFFER_SIZE) != 0)
                    {
                        // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
                        LOG_WARNING_MESSAGE("The size of uniform buffer '",
                                            pBufferVk->GetDesc().Name, "' bound to shader variable '",
                                            GetShaderResourcePrintName(SPIRVAttribs, ArrIndex), "' is ", pBufferVk->GetDesc().Size,
                                            " bytes, but the shader expects at least ", SPIRVAttribs.BufferStaticSize,
                                            " bytes.");
                    }
                }
                break;

            case DescriptorType::StorageBuffer:
            case DescriptorType::StorageBuffer_ReadOnly:
            case DescriptorType::StorageBufferDynamic:
            case DescriptorType::StorageBufferDynamic_ReadOnly:
                VERIFY_EXPR(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV || ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV);
                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (BufferViewVkImpl* pBufferViewVk = Res.pObject.RawPtr<BufferViewVkImpl>())
                {
                    const BufferVkImpl*   pBufferVk = ClassPtrCast<BufferVkImpl>(pBufferViewVk->GetBuffer());
                    const BufferViewDesc& ViewDesc  = pBufferViewVk->GetDesc();
                    const BufferDesc&     BuffDesc  = pBufferVk->GetDesc();

                    pDeviceCtx->DvpVerifyDynamicAllocation(pBufferVk);

                    if (BuffDesc.ElementByteStride == 0)
                    {
                        if ((ViewDesc.ByteWidth < SPIRVAttribs.BufferStaticSize) &&
                            (GetDevice()->GetValidationFlags() & VALIDATION_FLAG_CHECK_SHADER_BUFFER_SIZE) != 0)
                        {
                            // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
                            LOG_WARNING_MESSAGE("The size of buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' bound to shader variable '",
                                                GetShaderResourcePrintName(SPIRVAttribs, ArrIndex), "' is ", ViewDesc.ByteWidth, " bytes, but the shader expects at least ",
                                                SPIRVAttribs.BufferStaticSize, " bytes.");
                        }
                    }
                    else
                    {
                        if ((ViewDesc.ByteWidth < SPIRVAttribs.BufferStaticSize || (ViewDesc.ByteWidth - SPIRVAttribs.BufferStaticSize) % BuffDesc.ElementByteStride != 0) &&
                            (GetDevice()->GetValidationFlags() & VALIDATION_FLAG_CHECK_SHADER_BUFFER_SIZE) != 0)
                        {
                            // For buffers with dynamic arrays we know only static part size and array element stride.
                            // Element stride in the shader may be differ than in the code. Here we check that the buffer size is exactly the same as the array with N elements.
                            LOG_WARNING_MESSAGE("The size (", ViewDesc.ByteWidth, ") and stride (", BuffDesc.ElementByteStride, ") of buffer view '",
                                                ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' bound to shader variable '",
                                                GetShaderResourcePrintName(SPIRVAttribs, ArrIndex), "' are incompatible with what the shader expects. This may be the result of the array element size mismatch.");
                        }
                    }
                }
                break;

            case DescriptorType::StorageImage:
            case DescriptorType::SeparateImage:
            case DescriptorType::CombinedImageSampler:
                VERIFY_EXPR(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV || ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV);
                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (const TextureViewVkImpl* pTexViewVk = Res.pObject.RawPtr<TextureViewVkImpl>())
                {
                    if (!ValidateResourceViewDimension(SPIRVAttribs.Name, SPIRVAttribs.ArraySize, ArrIndex, pTexViewVk, SPIRVAttribs.GetResourceDimension(), SPIRVAttribs.IsMultisample()))
                        BindingsOK = false;
                }
                break;

            default:
                break;
                // Nothing to do
        }
    }

    return BindingsOK;
}
#endif

PipelineResourceSignatureVkImpl::PipelineResourceSignatureVkImpl(IReferenceCounters*                            pRefCounters,
                                                                 RenderDeviceVkImpl*                            pDevice,
                                                                 const PipelineResourceSignatureDesc&           Desc,
                                                                 const PipelineResourceSignatureInternalDataVk& InternalData) :
    TPipelineResourceSignatureBase{pRefCounters, pDevice, Desc, InternalData}
{
    try
    {
        Deserialize(
            GetRawAllocator(), Desc, InternalData, /*CreateImmutableSamplers = */ true,
            [this]() //
            {
                CreateSetLayouts(/*IsSerialized*/ true);
            },
            [this]() //
            {
                return ShaderResourceCacheVk::GetRequiredMemorySize(GetNumDescriptorSets(), m_DescriptorSetSizes.data(), m_TotalInlineConstants);
            });
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

PipelineResourceSignatureInternalDataVk PipelineResourceSignatureVkImpl::GetInternalData() const
{
    PipelineResourceSignatureInternalDataVk InternalData = TPipelineResourceSignatureBase::GetInternalData();

    InternalData.DynamicStorageBufferCount = m_DynamicStorageBufferCount;
    InternalData.DynamicUniformBufferCount = m_DynamicUniformBufferCount;

    return InternalData;
}

void PipelineResourceSignatureVkImpl::CommitInlineConstants(const CommitInlineConstantsAttribs& Attribs) const
{
    const ShaderResourceCacheVk& ResourceCache = *Attribs.pResourceCache;
    VERIFY(ResourceCache.GetContentType() == ResourceCacheContentType::SRB,
           "Inline constants can only be committed from SRB resource cache");
    for (Uint32 i = 0; i < m_NumInlineConstantBuffers; ++i)
    {
        const InlineConstantBufferAttribsVk& InlineCBAttr        = GetInlineConstantBufferAttribs(i);
        const Uint32                         DataSize            = InlineCBAttr.NumConstants * sizeof(Uint32);
        const void*                          pInlineConstantData = ResourceCache.GetInlineConstantData(InlineCBAttr.DescrSet, InlineCBAttr.SRBCacheOffset);
        VERIFY_EXPR(pInlineConstantData != nullptr);

        if (InlineCBAttr.ResIndex == Attribs.PushConstantResIndex)
        {
            VulkanUtilities::CommandBuffer& CmdBuffer = Attribs.Ctx.GetCommandBuffer();
            VERIFY(Attribs.vkPushConstRange.size == DataSize,
                   "Push constant range size (", Attribs.vkPushConstRange.size,
                   ") does not match the inline constant buffer data size (", DataSize, ")");
            CmdBuffer.PushConstants(Attribs.vkPipelineLayout, Attribs.vkPushConstRange, pInlineConstantData);
        }
        else
        {
            // Get the buffer from the SRB cache (not from the signature's InlineCBAttr.pBuffer).
            // This ensures we update the same buffer that was committed by DeviceContextVkImpl::CommitDescriptorSets.
            // When an SRB created from a compatible but different signature is used (e.g., via PSO serialization),
            // the SRB's cache contains inline constant buffers from another signature, not from this one.
            const ShaderResourceCacheVk::DescriptorSet& DescrSet  = ResourceCache.GetDescriptorSet(InlineCBAttr.DescrSet);
            const ShaderResourceCacheVk::Resource&      CachedRes = DescrSet.GetResource(InlineCBAttr.SRBCacheOffset);
            BufferVkImpl*                               pBuffer   = CachedRes.pObject.RawPtr<BufferVkImpl>();
            VERIFY(pBuffer != nullptr, "Inline constant buffer is null in SRB cache");

            // Map the buffer from SRB cache and copy the data
            void* pMappedData = nullptr;
            Attribs.Ctx.MapBuffer(pBuffer, MAP_WRITE, MAP_FLAG_DISCARD, pMappedData);
            memcpy(pMappedData, pInlineConstantData, DataSize);
            Attribs.Ctx.UnmapBuffer(pBuffer, MAP_WRITE);
        }
    }
}

} // namespace Diligent
