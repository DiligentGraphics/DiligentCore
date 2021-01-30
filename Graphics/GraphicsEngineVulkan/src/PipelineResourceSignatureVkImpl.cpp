/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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
#include "ShaderResourceBindingVkImpl.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "VulkanTypeConversions.hpp"
#include "SamplerVkImpl.hpp"
#include "TextureViewVkImpl.hpp"
#include "TopLevelASVkImpl.hpp"
#include "BasicMath.hpp"
#include "DynamicLinearAllocator.hpp"
#include "SPIRVShaderResources.hpp"

namespace Diligent
{

namespace
{

inline bool ResourcesCompatible(const PipelineResourceSignatureVkImpl::ResourceAttribs& lhs,
                                const PipelineResourceSignatureVkImpl::ResourceAttribs& rhs)
{
    // Ignore sampler index and cache offsets.
    // clang-format off
    return lhs.BindingIndex         == rhs.BindingIndex &&
           lhs.ArraySize            == rhs.ArraySize    &&
           lhs.DescrType            == rhs.DescrType    &&
           lhs.DescrSet             == rhs.DescrSet     &&
           lhs.ImtblSamplerAssigned == rhs.ImtblSamplerAssigned;
    // clang-format on
}

inline bool ResourcesCompatible(const PipelineResourceDesc& lhs, const PipelineResourceDesc& rhs)
{
    // Ignore resource names.
    // clang-format off
    return lhs.ShaderStages == rhs.ShaderStages &&
           lhs.ArraySize    == rhs.ArraySize    &&
           lhs.ResourceType == rhs.ResourceType &&
           lhs.VarType      == rhs.VarType      &&
           lhs.Flags        == lhs.Flags;
    // clang-format on
}

inline VkDescriptorType GetVkDescriptorType(DescriptorType Type)
{
    static_assert(static_cast<Uint32>(DescriptorType::Count) == 15, "Please update the switch below to handle the new descriptor type");
    switch (Type)
    {
        // clang-format off
        case DescriptorType::Sampler:                       return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::CombinedImageSampler:          return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::SeparateImage:                 return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::StorageImage:                  return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::UniformTexelBuffer:            return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case DescriptorType::StorageTexelBuffer:            return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case DescriptorType::StorageTexelBuffer_ReadOnly:   return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case DescriptorType::UniformBuffer:                 return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::UniformBufferDynamic:          return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case DescriptorType::StorageBuffer:                 return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::StorageBuffer_ReadOnly:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::StorageBufferDynamic:          return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case DescriptorType::StorageBufferDynamic_ReadOnly: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case DescriptorType::InputAttachment:               return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        case DescriptorType::AccelerationStructure:         return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            // clang-format on
        default:
            UNEXPECTED("Unknown descriptor type");
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

DescriptorType GetDescriptorType(const PipelineResourceDesc& Res)
{
    const bool WithDynamicOffset = (Res.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0;
    const bool CombinedSampler   = (Res.Flags & PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER) != 0;
    const bool UseTexelBuffer    = (Res.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0;

    static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update the switch below to handle the new shader resource type");
    switch (Res.ResourceType)
    {
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
            VERIFY((Res.Flags & ~PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0,
                   "NO_DYNAMIC_BUFFERS is the only valid flag allowed for constant buffers. "
                   "This error should've been caught by ValidatePipelineResourceSignatureDesc.");
            return WithDynamicOffset ? DescriptorType::UniformBufferDynamic : DescriptorType::UniformBuffer;

        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
            VERIFY((Res.Flags & ~PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER) == 0,
                   "COMBINED_SAMPLER is the only valid flag for a texture SRV. "
                   "This error should've been caught by ValidatePipelineResourceSignatureDesc.");
            return CombinedSampler ? DescriptorType::CombinedImageSampler : DescriptorType::SeparateImage;

        case SHADER_RESOURCE_TYPE_BUFFER_SRV:
            VERIFY((Res.Flags & ~(PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS | PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER)) == 0,
                   "NO_DYNAMIC_BUFFERS and FORMATTED_BUFFER are the only valid flags for a buffer SRV. "
                   "This error should've been caught by ValidatePipelineResourceSignatureDesc.");
            return UseTexelBuffer ? DescriptorType::UniformTexelBuffer :
                                    (WithDynamicOffset ? DescriptorType::StorageBufferDynamic_ReadOnly : DescriptorType::StorageBuffer_ReadOnly);

        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
            VERIFY(Res.Flags == PIPELINE_RESOURCE_FLAG_UNKNOWN,
                   "UNKNOWN is the only valid flag for a texture UAV. "
                   "This error should've been caught by ValidatePipelineResourceSignatureDesc.");
            return DescriptorType::StorageImage;

        case SHADER_RESOURCE_TYPE_BUFFER_UAV:
            VERIFY((Res.Flags & ~(PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS | PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER)) == 0,
                   "NO_DYNAMIC_BUFFERS and FORMATTED_BUFFER are the only valid flags for a buffer UAV. "
                   "This should've been caught by ValidatePipelineResourceSignatureDesc.");
            return UseTexelBuffer ? DescriptorType::StorageTexelBuffer :
                                    (WithDynamicOffset ? DescriptorType::StorageBufferDynamic : DescriptorType::StorageBuffer);

        case SHADER_RESOURCE_TYPE_SAMPLER:
            VERIFY(Res.Flags == PIPELINE_RESOURCE_FLAG_UNKNOWN,
                   "UNKNOWN is the only valid flag for a sampler. "
                   "This error should've been caught by ValidatePipelineResourceSignatureDesc.");
            return DescriptorType::Sampler;

        case SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT:
            VERIFY(Res.Flags == PIPELINE_RESOURCE_FLAG_UNKNOWN,
                   "UNKNOWN is the only valid flag for an input attachment. "
                   "This error should've been caught by ValidatePipelineResourceSignatureDesc.");
            return DescriptorType::InputAttachment;

        case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
            VERIFY(Res.Flags == PIPELINE_RESOURCE_FLAG_UNKNOWN,
                   "UNKNOWN is the only valid flag for an acceleration structure. "
                   "This error should've been caught by ValidatePipelineResourceSignatureDesc.");
            return DescriptorType::AccelerationStructure;

        default:
            UNEXPECTED("Unknown resource type");
            return DescriptorType::Unknown;
    }
}

inline BUFFER_VIEW_TYPE DescriptorTypeToBufferView(DescriptorType Type)
{
    static_assert(static_cast<Uint32>(DescriptorType::Count) == 15, "Please update the switch below to handle the new descriptor type");
    switch (Type)
    {
        // clang-format off
        case DescriptorType::UniformTexelBuffer:
        case DescriptorType::StorageTexelBuffer_ReadOnly:
        case DescriptorType::StorageBuffer_ReadOnly:
        case DescriptorType::StorageBufferDynamic_ReadOnly: return BUFFER_VIEW_SHADER_RESOURCE;
        case DescriptorType::StorageTexelBuffer:
        case DescriptorType::StorageBuffer:
        case DescriptorType::StorageBufferDynamic:          return BUFFER_VIEW_UNORDERED_ACCESS;
            // clang-format on
        default:
            UNEXPECTED("Unsupported descriptor type for buffer view");
            return BUFFER_VIEW_UNDEFINED;
    }
}

inline TEXTURE_VIEW_TYPE DescriptorTypeToTextureView(DescriptorType Type)
{
    static_assert(static_cast<Uint32>(DescriptorType::Count) == 15, "Please update the switch below to handle the new descriptor type");
    switch (Type)
    {
        // clang-format off
        case DescriptorType::StorageImage:          return TEXTURE_VIEW_UNORDERED_ACCESS;
        case DescriptorType::CombinedImageSampler:
        case DescriptorType::SeparateImage:
        case DescriptorType::InputAttachment:       return TEXTURE_VIEW_SHADER_RESOURCE;
            // clang-format on
        default:
            UNEXPECTED("Unsupported descriptor type for texture view");
            return TEXTURE_VIEW_UNDEFINED;
    }
}

Int32 FindImmutableSampler(const PipelineResourceDesc&          Res,
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
        return -1;
    }

    for (Uint32 s = 0; s < Desc.NumImmutableSamplers; ++s)
    {
        const auto& ImtblSam = Desc.ImmutableSamplers[s];
        if (((ImtblSam.ShaderStages & Res.ShaderStages) != 0) && StreqSuff(Res.Name, ImtblSam.SamplerOrTextureName, SamplerSuffix))
        {
            DEV_CHECK_ERR((ImtblSam.ShaderStages & Res.ShaderStages) == Res.ShaderStages,
                          "Immutable sampler '", ImtblSam.SamplerOrTextureName,
                          "' is specified for only some of the shader stages that resource '", Res.Name, "' is defined for.");
            return s;
        }
    }

    return -1;
}

} // namespace

inline PipelineResourceSignatureVkImpl::CACHE_GROUP PipelineResourceSignatureVkImpl::GetResourceCacheGroup(const PipelineResourceDesc& Res)
{
    // NB: SetId is always 0 for static/mutable variables, and 1 - for dynamic ones.
    //     It is not the actual descriptor set index in the set layout!
    const auto SetId             = GetDescriptorSetId(Res.VarType);
    const bool WithDynamicOffset = (Res.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0;
    const bool UseTexelBuffer    = (Res.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0;

    if (WithDynamicOffset && !UseTexelBuffer)
    {
        if (Res.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER)
            return static_cast<CACHE_GROUP>(SetId * 3 + CACHE_GROUP_DYN_UB);

        if (Res.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV || Res.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV)
            return static_cast<CACHE_GROUP>(SetId * 3 + CACHE_GROUP_DYN_SB);
    }
    return static_cast<CACHE_GROUP>(SetId * 3 + CACHE_GROUP_OTHER);
}

inline PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID PipelineResourceSignatureVkImpl::GetDescriptorSetId(SHADER_RESOURCE_VARIABLE_TYPE VarType)
{
    return VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC ? DESCRIPTOR_SET_ID_DYNAMIC : DESCRIPTOR_SET_ID_STATIC_MUTABLE;
}


RESOURCE_STATE DescriptorTypeToResourceState(DescriptorType Type)
{
    static_assert(static_cast<Uint32>(DescriptorType::Count) == 15, "Please update the switch below to handle the new descriptor type");
    switch (Type)
    {
        // clang-format off
        case DescriptorType::Sampler:                       return RESOURCE_STATE_UNKNOWN;
        case DescriptorType::CombinedImageSampler:          return RESOURCE_STATE_SHADER_RESOURCE;
        case DescriptorType::SeparateImage:                 return RESOURCE_STATE_SHADER_RESOURCE;
        case DescriptorType::StorageImage:                  return RESOURCE_STATE_UNORDERED_ACCESS;
        case DescriptorType::UniformTexelBuffer:            return RESOURCE_STATE_SHADER_RESOURCE;
        case DescriptorType::StorageTexelBuffer:            return RESOURCE_STATE_UNORDERED_ACCESS;
        case DescriptorType::StorageTexelBuffer_ReadOnly:   return RESOURCE_STATE_SHADER_RESOURCE;
        case DescriptorType::UniformBuffer:                 return RESOURCE_STATE_CONSTANT_BUFFER;
        case DescriptorType::UniformBufferDynamic:          return RESOURCE_STATE_CONSTANT_BUFFER;
        case DescriptorType::StorageBuffer:                 return RESOURCE_STATE_UNORDERED_ACCESS;
        case DescriptorType::StorageBuffer_ReadOnly:        return RESOURCE_STATE_SHADER_RESOURCE;
        case DescriptorType::StorageBufferDynamic:          return RESOURCE_STATE_UNORDERED_ACCESS;
        case DescriptorType::StorageBufferDynamic_ReadOnly: return RESOURCE_STATE_SHADER_RESOURCE;
        case DescriptorType::InputAttachment:               return RESOURCE_STATE_SHADER_RESOURCE;
        case DescriptorType::AccelerationStructure:         return RESOURCE_STATE_SHADER_RESOURCE;
            // clang-format on
        default:
            UNEXPECTED("unknown descriptor type");
            return RESOURCE_STATE_UNKNOWN;
    }
}


PipelineResourceSignatureVkImpl::PipelineResourceSignatureVkImpl(IReferenceCounters*                  pRefCounters,
                                                                 RenderDeviceVkImpl*                  pDevice,
                                                                 const PipelineResourceSignatureDesc& Desc,
                                                                 bool                                 bIsDeviceInternal) :
    TPipelineResourceSignatureBase{pRefCounters, pDevice, Desc, bIsDeviceInternal},
    m_SRBMemAllocator{GetRawAllocator()}
{
    m_StaticVarIndex.fill(-1);

    try
    {
        FixedLinearAllocator MemPool{GetRawAllocator()};

        // Reserve at least 1 element because m_pResourceAttribs must hold a pointer to memory
        MemPool.AddSpace<ResourceAttribs>(std::max(1u, Desc.NumResources));
        MemPool.AddSpace<ImmutableSamplerAttribs>(Desc.NumImmutableSamplers);

        ReserveSpaceForDescription(MemPool, Desc);

        Uint32 StaticResourceCount = 0; // The total number of static resources in all stages
                                        // accounting for array sizes.

        CacheOffsetsType CacheSizes   = {};
        BindingCountType BindingCount = {};

        SHADER_TYPE StaticResStages = SHADER_TYPE_UNKNOWN; // Shader stages that have static resources
        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            const auto& ResDesc    = Desc.Resources[i];
            const auto  CacheGroup = GetResourceCacheGroup(ResDesc);

            m_ShaderStages |= ResDesc.ShaderStages;

            if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                StaticResStages |= ResDesc.ShaderStages;
                StaticResourceCount += ResDesc.ArraySize;
            }

            BindingCount[CacheGroup] += 1;
            CacheSizes[CacheGroup] += ResDesc.ArraySize;
        }

        m_NumShaderStages = static_cast<Uint8>(PlatformMisc::CountOneBits(static_cast<Uint32>(m_ShaderStages)));
        if (m_ShaderStages != SHADER_TYPE_UNKNOWN)
        {
            m_PipelineType = PipelineTypeFromShaderStages(m_ShaderStages);
            DEV_CHECK_ERR(m_PipelineType != PIPELINE_TYPE_INVALID, "Failed to deduce pipeline type from shader stages");
        }

        int StaticVarStageCount = 0; // The number of shader stages that have static variables
        for (; StaticResStages != SHADER_TYPE_UNKNOWN; ++StaticVarStageCount)
        {
            const auto StageBit             = ExtractLSB(StaticResStages);
            const auto ShaderTypeInd        = GetShaderTypePipelineIndex(StageBit, m_PipelineType);
            m_StaticVarIndex[ShaderTypeInd] = static_cast<Int8>(StaticVarStageCount);
        }
        if (StaticVarStageCount > 0)
        {
            MemPool.AddSpace<ShaderResourceCacheVk>(1);
            MemPool.AddSpace<ShaderVariableManagerVk>(StaticVarStageCount);
        }

        MemPool.Reserve();

        m_pResourceAttribs  = MemPool.Allocate<ResourceAttribs>(std::max(1u, m_Desc.NumResources));
        m_ImmutableSamplers = MemPool.ConstructArray<ImmutableSamplerAttribs>(m_Desc.NumImmutableSamplers);

        // The memory is now owned by PipelineResourceSignatureVkImpl and will be freed by Destruct().
        auto* Ptr = MemPool.ReleaseOwnership();
        VERIFY_EXPR(Ptr == m_pResourceAttribs);
        (void)Ptr;

        CopyDescription(MemPool, Desc);

        if (StaticVarStageCount > 0)
        {
            m_pResourceCache = MemPool.Construct<ShaderResourceCacheVk>(CacheContentType::Signature);
            m_StaticVarsMgrs = MemPool.Allocate<ShaderVariableManagerVk>(StaticVarStageCount);

            m_pResourceCache->InitializeSets(GetRawAllocator(), 1, &StaticResourceCount);
        }

        CreateSetLayouts(CacheSizes, BindingCount);

        if (StaticVarStageCount > 0)
        {
            const SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_STATIC};

            for (Uint32 i = 0; i < m_StaticVarIndex.size(); ++i)
            {
                Int8 Idx = m_StaticVarIndex[i];
                if (Idx >= 0)
                {
                    VERIFY_EXPR(Idx < StaticVarStageCount);
                    const auto ShaderType = GetShaderTypeFromPipelineIndex(i, GetPipelineType());
                    new (m_StaticVarsMgrs + Idx) ShaderVariableManagerVk{*this, *m_pResourceCache};
                    m_StaticVarsMgrs[Idx].Initialize(*this, GetRawAllocator(), AllowedVarTypes, _countof(AllowedVarTypes), ShaderType);
                }
            }
        }

        m_Hash = CalculateHash();
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

void PipelineResourceSignatureVkImpl::CreateSetLayouts(const CacheOffsetsType& CacheSizes,
                                                       const BindingCountType& BindingCount)
{
    // Descriptor set mapping (static/mutable (0) or dynamic (1) -> set index)
    std::array<Uint32, DESCRIPTOR_SET_ID_NUM_SETS> DSMapping = {};
    {
        const auto TotalStaticBindings =
            BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR] +
            BindingCount[CACHE_GROUP_DYN_SB_STAT_VAR] +
            BindingCount[CACHE_GROUP_OTHER_STAT_VAR];
        const auto TotalDynamicBindings =
            BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR] +
            BindingCount[CACHE_GROUP_DYN_SB_DYN_VAR] +
            BindingCount[CACHE_GROUP_OTHER_DYN_VAR];

        Uint32 Idx = 0;

        DSMapping[DESCRIPTOR_SET_ID_STATIC_MUTABLE] = (TotalStaticBindings != 0 ? Idx++ : 0xFF);
        DSMapping[DESCRIPTOR_SET_ID_DYNAMIC]        = (TotalDynamicBindings != 0 ? Idx++ : 0xFF);
        VERIFY_EXPR(Idx <= MAX_DESCRIPTOR_SETS);
    }

    CacheOffsetsType CacheOffsets =
        {
            // static/mutable set
            0,
            CacheSizes[CACHE_GROUP_DYN_UB_STAT_VAR],
            CacheSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheSizes[CACHE_GROUP_DYN_SB_STAT_VAR],
            // dynamic set
            0,
            CacheSizes[CACHE_GROUP_DYN_UB_DYN_VAR],
            CacheSizes[CACHE_GROUP_DYN_UB_DYN_VAR] + CacheSizes[CACHE_GROUP_DYN_SB_DYN_VAR] //
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
    Uint32 StaticCacheOffset = 0;

    std::array<std::vector<VkDescriptorSetLayoutBinding>, MAX_DESCRIPTOR_SETS> vkSetLayoutBindings;

    DynamicLinearAllocator TempAllocator{GetRawAllocator()};

    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& ResDesc   = m_Desc.Resources[i];
        const auto  DescrType = GetDescriptorType(ResDesc);
        // NB: SetId is always 0 for static/mutable variables, and 1 - for dynamic ones.
        //     It is not the actual descriptor set index in the set layout!
        const auto SetId      = GetDescriptorSetId(ResDesc.VarType);
        const auto CacheGroup = GetResourceCacheGroup(ResDesc);

        VERIFY(i == 0 || ResDesc.VarType >= m_Desc.Resources[i - 1].VarType, "Resources must be sorted by variable type");

        // If all resources are dynamic, then the signature contains only one descriptor set layout with index 0,
        // so remap SetIdx to the actual descriptor set index.
        VERIFY_EXPR(DSMapping[SetId] < MAX_DESCRIPTOR_SETS);

        // The sampler may not be yet initialized, but this is OK as all resources are initialized
        // in the same order as in m_Desc.Resources
        const auto AssignedSamplerInd = DescrType == DescriptorType::SeparateImage ?
            FindAssignedSampler(ResDesc) :
            ResourceAttribs::InvalidSamplerInd;

        VkSampler* pVkImmutableSamplers = nullptr;
        if (DescrType == DescriptorType::CombinedImageSampler ||
            DescrType == DescriptorType::Sampler)
        {
            // Only search for immutable sampler for combined image samplers and separate samplers
            Int32 SrcImmutableSamplerInd = FindImmutableSampler(ResDesc, DescrType, m_Desc, GetCombinedSamplerSuffix());
            if (SrcImmutableSamplerInd >= 0)
            {
                auto&       ImmutableSampler     = m_ImmutableSamplers[SrcImmutableSamplerInd];
                const auto& ImmutableSamplerDesc = m_Desc.ImmutableSamplers[SrcImmutableSamplerInd].Desc;
                if (!ImmutableSampler.Ptr)
                    GetDevice()->CreateSampler(ImmutableSamplerDesc, &ImmutableSampler.Ptr);

                pVkImmutableSamplers = TempAllocator.ConstructArray<VkSampler>(ResDesc.ArraySize, ImmutableSampler.Ptr.RawPtr<SamplerVkImpl>()->GetVkSampler());
            }
        }

        const auto* const pAttribs = new (m_pResourceAttribs + i) ResourceAttribs //
            {
                BindingIndices[CacheGroup],
                AssignedSamplerInd,
                ResDesc.ArraySize,
                DescrType,
                DSMapping[SetId],
                pVkImmutableSamplers != nullptr,
                CacheOffsets[CacheGroup],
                ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC ? StaticCacheOffset : ~0u //
            };
        BindingIndices[CacheGroup] += 1;
        CacheOffsets[CacheGroup] += ResDesc.ArraySize;

        vkSetLayoutBindings[SetId].emplace_back();
        auto& vkSetLayoutBinding = vkSetLayoutBindings[SetId].back();

        vkSetLayoutBinding.binding            = pAttribs->BindingIndex;
        vkSetLayoutBinding.descriptorCount    = ResDesc.ArraySize;
        vkSetLayoutBinding.stageFlags         = ShaderTypesToVkShaderStageFlags(ResDesc.ShaderStages);
        vkSetLayoutBinding.pImmutableSamplers = pVkImmutableSamplers;
        vkSetLayoutBinding.descriptorType     = GetVkDescriptorType(pAttribs->GetDescriptorType());

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
        {
            m_pResourceCache->InitializeResources(pAttribs->DescrSet, StaticCacheOffset, ResDesc.ArraySize, pAttribs->GetDescriptorType());
            StaticCacheOffset += ResDesc.ArraySize;
        }
    }

    m_DynamicUniformBufferCount = static_cast<Uint16>(CacheSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheSizes[CACHE_GROUP_DYN_UB_DYN_VAR]);
    m_DynamicStorageBufferCount = static_cast<Uint16>(CacheSizes[CACHE_GROUP_DYN_SB_STAT_VAR] + CacheSizes[CACHE_GROUP_DYN_SB_DYN_VAR]);
    VERIFY_EXPR(m_DynamicUniformBufferCount == CacheSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheSizes[CACHE_GROUP_DYN_UB_DYN_VAR]);
    VERIFY_EXPR(m_DynamicStorageBufferCount == CacheSizes[CACHE_GROUP_DYN_SB_STAT_VAR] + CacheSizes[CACHE_GROUP_DYN_SB_DYN_VAR]);

    VERIFY_EXPR(m_pResourceCache == nullptr || m_pResourceCache->GetDescriptorSet(0).GetSize() == StaticCacheOffset);
    VERIFY_EXPR(CacheOffsets[CACHE_GROUP_DYN_UB_STAT_VAR] == CacheSizes[CACHE_GROUP_DYN_UB_STAT_VAR]);
    VERIFY_EXPR(CacheOffsets[CACHE_GROUP_DYN_SB_STAT_VAR] == CacheSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheSizes[CACHE_GROUP_DYN_SB_STAT_VAR]);
    VERIFY_EXPR(CacheOffsets[CACHE_GROUP_OTHER_STAT_VAR] == CacheSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheSizes[CACHE_GROUP_DYN_SB_STAT_VAR] + CacheSizes[CACHE_GROUP_OTHER_STAT_VAR]);
    VERIFY_EXPR(CacheOffsets[CACHE_GROUP_DYN_UB_DYN_VAR] == CacheSizes[CACHE_GROUP_DYN_UB_DYN_VAR]);
    VERIFY_EXPR(CacheOffsets[CACHE_GROUP_DYN_SB_DYN_VAR] == CacheSizes[CACHE_GROUP_DYN_UB_DYN_VAR] + CacheSizes[CACHE_GROUP_DYN_SB_DYN_VAR]);
    VERIFY_EXPR(CacheOffsets[CACHE_GROUP_OTHER_DYN_VAR] == CacheSizes[CACHE_GROUP_DYN_UB_DYN_VAR] + CacheSizes[CACHE_GROUP_DYN_SB_DYN_VAR] + CacheSizes[CACHE_GROUP_OTHER_DYN_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_DYN_UB_STAT_VAR] == BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_DYN_SB_STAT_VAR] == BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR] + BindingCount[CACHE_GROUP_DYN_SB_STAT_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_OTHER_STAT_VAR] == BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR] + BindingCount[CACHE_GROUP_DYN_SB_STAT_VAR] + BindingCount[CACHE_GROUP_OTHER_STAT_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_DYN_UB_DYN_VAR] == BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_DYN_SB_DYN_VAR] == BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR] + BindingCount[CACHE_GROUP_DYN_SB_DYN_VAR]);
    VERIFY_EXPR(BindingIndices[CACHE_GROUP_OTHER_DYN_VAR] == BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR] + BindingCount[CACHE_GROUP_DYN_SB_DYN_VAR] + BindingCount[CACHE_GROUP_OTHER_DYN_VAR]);

    // Add immutable samplers that do not exist in m_Desc.Resources.
    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        auto& ImmutableSampler = m_ImmutableSamplers[i];
        if (ImmutableSampler.Ptr)
            continue;

        const auto& SamplerDesc = m_Desc.ImmutableSamplers[i];
        // If static/mutable descriptor set layout is empty, then add samplers to dynamic set.
        const auto SetId = (DSMapping[DESCRIPTOR_SET_ID_STATIC_MUTABLE] < MAX_DESCRIPTOR_SETS ? DESCRIPTOR_SET_ID_STATIC_MUTABLE : DESCRIPTOR_SET_ID_DYNAMIC);
        DEV_CHECK_ERR(DSMapping[SetId] < MAX_DESCRIPTOR_SETS,
                      "There are no descriptor sets in this singature, which indicates there are no other "
                      "resources besides immutable samplers. This is not currently allowed.");

        auto& BindingIndex = BindingIndices[SetId * 3 + CACHE_GROUP_OTHER];

        GetDevice()->CreateSampler(SamplerDesc.Desc, &ImmutableSampler.Ptr);

        ImmutableSampler.DescrSet     = DSMapping[SetId];
        ImmutableSampler.BindingIndex = BindingIndex;

        VERIFY_EXPR(ImmutableSampler.BindingIndex == BindingIndex);
        ++BindingIndex;

        vkSetLayoutBindings[SetId].emplace_back();
        auto& vkSetLayoutBinding = vkSetLayoutBindings[SetId].back();

        vkSetLayoutBinding.binding            = ImmutableSampler.BindingIndex;
        vkSetLayoutBinding.descriptorCount    = 1;
        vkSetLayoutBinding.stageFlags         = ShaderTypesToVkShaderStageFlags(SamplerDesc.ShaderStages);
        vkSetLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
        vkSetLayoutBinding.pImmutableSamplers = TempAllocator.Construct<VkSampler>(ImmutableSampler.Ptr.RawPtr<SamplerVkImpl>()->GetVkSampler());
    }

    if (m_Desc.SRBAllocationGranularity > 1)
    {
        std::array<size_t, MAX_SHADERS_IN_PIPELINE> ShaderVariableDataSizes = {};
        for (Uint32 s = 0; s < GetNumActiveShaderStages(); ++s)
        {
            const SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};

            Uint32 UnusedNumVars       = 0;
            ShaderVariableDataSizes[s] = ShaderVariableManagerVk::GetRequiredMemorySize(*this, AllowedVarTypes, _countof(AllowedVarTypes), GetActiveShaderStageType(s), UnusedNumVars);
        }

        std::array<Uint32, DESCRIPTOR_SET_ID_NUM_SETS> DescriptorSetSizes;
        DescriptorSetSizes[DESCRIPTOR_SET_ID_STATIC_MUTABLE] =
            CacheSizes[CACHE_GROUP_DYN_UB_STAT_VAR] +
            CacheSizes[CACHE_GROUP_DYN_SB_STAT_VAR] +
            CacheSizes[CACHE_GROUP_OTHER_STAT_VAR];
        DescriptorSetSizes[DESCRIPTOR_SET_ID_DYNAMIC] =
            CacheSizes[CACHE_GROUP_DYN_UB_DYN_VAR] +
            CacheSizes[CACHE_GROUP_DYN_SB_DYN_VAR] +
            CacheSizes[CACHE_GROUP_OTHER_DYN_VAR];
        static_assert(DescriptorSetSizes.size() == MAX_DESCRIPTOR_SETS, "MAX_DESCRIPTOR_SETS was changed, update the code above");

        const Uint32 NumSets = (DescriptorSetSizes[DESCRIPTOR_SET_ID_STATIC_MUTABLE] != 0 ? 1 : 0) + (DescriptorSetSizes[DESCRIPTOR_SET_ID_DYNAMIC] != 0 ? 1 : 0);
        if (DescriptorSetSizes[DESCRIPTOR_SET_ID_STATIC_MUTABLE] == 0)
            DescriptorSetSizes[DESCRIPTOR_SET_ID_STATIC_MUTABLE] = DescriptorSetSizes[DESCRIPTOR_SET_ID_DYNAMIC];

        const size_t CacheMemorySize = ShaderResourceCacheVk::GetRequiredMemorySize(NumSets, DescriptorSetSizes.data());

        m_SRBMemAllocator.Initialize(m_Desc.SRBAllocationGranularity, GetNumActiveShaderStages(), ShaderVariableDataSizes.data(), 1, &CacheMemorySize);
    }

    VkDescriptorSetLayoutCreateInfo SetLayoutCI = {};

    SetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    SetLayoutCI.pNext = nullptr;
    SetLayoutCI.flags = 0;

    const auto& LogicalDevice = GetDevice()->GetLogicalDevice();

    for (size_t i = 0; i < vkSetLayoutBindings.size(); ++i)
    {
        auto& vkSetLayoutBinding = vkSetLayoutBindings[i];
        if (vkSetLayoutBinding.empty())
            continue;

        SetLayoutCI.bindingCount = static_cast<Uint32>(vkSetLayoutBinding.size());
        SetLayoutCI.pBindings    = vkSetLayoutBinding.data();
        m_VkDescrSetLayouts[i]   = LogicalDevice.CreateDescriptorSetLayout(SetLayoutCI);
    }
}

Uint32 PipelineResourceSignatureVkImpl::FindAssignedSampler(const PipelineResourceDesc& SepImg) const
{
    Uint32 SamplerInd = ResourceAttribs::InvalidSamplerInd;
    if (IsUsingCombinedSamplers())
    {
        const auto IdxRange = GetResourceIndexRange(SepImg.VarType);

        for (Uint32 i = IdxRange.first; i < IdxRange.second; ++i)
        {
            const auto& Res = m_Desc.Resources[i];
            VERIFY_EXPR(SepImg.VarType == Res.VarType);

            if (Res.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER &&
                (SepImg.ShaderStages & Res.ShaderStages) &&
                StreqSuff(Res.Name, SepImg.Name, GetCombinedSamplerSuffix()))
            {
                VERIFY_EXPR((Res.ShaderStages & SepImg.ShaderStages) == SepImg.ShaderStages);
                SamplerInd = i;
                break;
            }
        }
    }
    return SamplerInd;
}

size_t PipelineResourceSignatureVkImpl::CalculateHash() const
{
    size_t Hash = ComputeHash(m_Desc.NumResources, m_Desc.NumImmutableSamplers, m_Desc.BindingIndex);

    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& Res  = m_Desc.Resources[i];
        const auto& Attr = m_pResourceAttribs[i];

        HashCombine(Hash, Res.ArraySize, Res.ShaderStages, Res.VarType, Attr.GetDescriptorType(), Attr.BindingIndex, Attr.DescrSet, Attr.IsImmutableSamplerAssigned());
    }

    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        HashCombine(Hash, m_Desc.ImmutableSamplers[i].ShaderStages, m_Desc.ImmutableSamplers[i].Desc);
    }

    return Hash;
}

PipelineResourceSignatureVkImpl::~PipelineResourceSignatureVkImpl()
{
    Destruct();
}

void PipelineResourceSignatureVkImpl::Destruct()
{
    TPipelineResourceSignatureBase::Destruct();

    for (auto& Layout : m_VkDescrSetLayouts)
    {
        if (Layout)
            m_pDevice->SafeReleaseDeviceObject(std::move(Layout), ~0ull);
    }

    if (m_pResourceAttribs == nullptr)
        return; // memory is not allocated

    auto& RawAllocator = GetRawAllocator();

    if (m_StaticVarsMgrs != nullptr)
    {
        for (size_t i = 0; i < m_StaticVarIndex.size(); ++i)
        {
            auto Idx = m_StaticVarIndex[i];
            if (Idx >= 0)
            {
                m_StaticVarsMgrs[Idx].DestroyVariables(RawAllocator);
                m_StaticVarsMgrs[Idx].~ShaderVariableManagerVk();
            }
        }
        m_StaticVarIndex.fill(-1);
        m_StaticVarsMgrs = nullptr;
    }

    if (m_pResourceCache != nullptr)
    {
        m_pResourceCache->~ShaderResourceCacheVk();
        m_pResourceCache = nullptr;
    }

    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        m_ImmutableSamplers[i].~ImmutableSamplerAttribs();
    }
    m_ImmutableSamplers = nullptr;

    if (void* pRawMem = m_pResourceAttribs)
    {
        RawAllocator.Free(pRawMem);
        m_pResourceAttribs = nullptr;
    }
}

bool PipelineResourceSignatureVkImpl::IsCompatibleWith(const PipelineResourceSignatureVkImpl& Other) const
{
    if (this == &Other)
        return true;

    if (GetHash() != Other.GetHash())
        return false;

    if (GetDesc().BindingIndex != Other.GetDesc().BindingIndex)
        return false;

    const Uint32 LResCount = GetTotalResourceCount();
    const Uint32 RResCount = Other.GetTotalResourceCount();

    if (LResCount != RResCount)
        return false;

    for (Uint32 r = 0; r < LResCount; ++r)
    {
        if (!ResourcesCompatible(GetResourceAttribs(r), Other.GetResourceAttribs(r)) ||
            !ResourcesCompatible(GetResourceDesc(r), Other.GetResourceDesc(r)))
            return false;
    }

    const Uint32 LSampCount = GetDesc().NumImmutableSamplers;
    const Uint32 RSampCount = Other.GetDesc().NumImmutableSamplers;

    if (LSampCount != RSampCount)
        return false;

    for (Uint32 s = 0; s < LSampCount; ++s)
    {
        const auto& LSamp = GetDesc().ImmutableSamplers[s];
        const auto& RSamp = Other.GetDesc().ImmutableSamplers[s];

        if (LSamp.ShaderStages != RSamp.ShaderStages ||
            !(LSamp.Desc == RSamp.Desc))
            return false;
    }

    return true;
}

void PipelineResourceSignatureVkImpl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                  bool                     InitStaticResources)
{
    auto& SRBAllocator  = m_pDevice->GetSRBAllocator();
    auto  pResBindingVk = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingVkImpl instance", ShaderResourceBindingVkImpl)(this, false);
    if (InitStaticResources)
        pResBindingVk->InitializeStaticResources(nullptr);
    pResBindingVk->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
}

Uint32 PipelineResourceSignatureVkImpl::GetStaticVariableCount(SHADER_TYPE ShaderType) const
{
    const auto VarMngrInd = GetStaticVariableCountHelper(ShaderType, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return 0;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariableCount();
}

IShaderResourceVariable* PipelineResourceSignatureVkImpl::GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name)
{
    const auto VarMngrInd = GetStaticVariableByNameHelper(ShaderType, Name, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariable(Name);
}

IShaderResourceVariable* PipelineResourceSignatureVkImpl::GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    const auto VarMngrInd = GetStaticVariableByIndexHelper(ShaderType, Index, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    const auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariable(Index);
}

void PipelineResourceSignatureVkImpl::BindStaticResources(Uint32            ShaderFlags,
                                                          IResourceMapping* pResMapping,
                                                          Uint32            Flags)
{
    const auto PipelineType = GetPipelineType();
    for (Uint32 ShaderInd = 0; ShaderInd < m_StaticVarIndex.size(); ++ShaderInd)
    {
        const auto VarMngrInd = m_StaticVarIndex[ShaderInd];
        if (VarMngrInd >= 0)
        {
            // ShaderInd is the shader type pipeline index here
            const auto ShaderType = GetShaderTypeFromPipelineIndex(ShaderInd, PipelineType);
            if (ShaderFlags & ShaderType)
            {
                m_StaticVarsMgrs[VarMngrInd].BindResources(pResMapping, Flags);
            }
        }
    }
}

void PipelineResourceSignatureVkImpl::InitResourceCache(ShaderResourceCacheVk& ResourceCache,
                                                        IMemoryAllocator&      CacheMemAllocator,
                                                        const char*            DbgPipelineName) const
{
    std::array<Uint32, MAX_DESCRIPTOR_SETS> VarCount = {};

    Uint32 NumSets = static_cast<Uint32>(VarCount.size());

    for (Uint32 r = 0; r < m_Desc.NumResources; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& Attr    = GetResourceAttribs(r);
        VarCount[Attr.DescrSet] += ResDesc.ArraySize;
    }

    if (VarCount[1] == 0)
        --NumSets;

    VERIFY_EXPR(NumSets > 0 && VarCount[0] > 0);

    // This call only initializes descriptor sets (ShaderResourceCacheVk::DescriptorSet) in the resource cache
    // Resources are initialized by source layout when shader resource binding objects are created
    ResourceCache.InitializeSets(CacheMemAllocator, NumSets, VarCount.data());

    if (auto vkLayout = GetVkDescriptorSetLayout(DESCRIPTOR_SET_ID_STATIC_MUTABLE))
    {
        const char* DescrSetName = "Static/Mutable Descriptor Set";
#ifdef DILIGENT_DEVELOPMENT
        std::string _DescrSetName(DbgPipelineName);
        _DescrSetName.append(" - static/mutable set");
        DescrSetName = _DescrSetName.c_str();
#endif
        DescriptorSetAllocation SetAllocation = GetDevice()->AllocateDescriptorSet(~Uint64{0}, vkLayout, DescrSetName);
        ResourceCache.GetDescriptorSet(GetDescriptorSetIndex<DESCRIPTOR_SET_ID_STATIC_MUTABLE>()).AssignDescriptorSetAllocation(std::move(SetAllocation));
    }
}

SHADER_TYPE PipelineResourceSignatureVkImpl::GetActiveShaderStageType(Uint32 StageIndex) const
{
    VERIFY_EXPR(StageIndex < m_NumShaderStages);

    SHADER_TYPE Stages = m_ShaderStages;
    for (Uint32 Index = 0; Stages != SHADER_TYPE_UNKNOWN; ++Index)
    {
        auto StageBit = ExtractLSB(Stages);

        if (Index == StageIndex)
            return StageBit;
    }

    UNEXPECTED("Index is out of range");
    return SHADER_TYPE_UNKNOWN;
}

void PipelineResourceSignatureVkImpl::InitializeResourceMemoryInCache(ShaderResourceCacheVk& ResourceCache) const
{
    const auto TotalResources = GetTotalResourceCount();
    const auto CacheType      = ResourceCache.GetContentType();
    for (Uint32 r = 0; r < TotalResources; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& Attr    = GetResourceAttribs(r);
        ResourceCache.InitializeResources(Attr.DescrSet, Attr.CacheOffset(CacheType), ResDesc.ArraySize, Attr.GetDescriptorType());
    }
}

void PipelineResourceSignatureVkImpl::InitializeStaticSRBResources(ShaderResourceCacheVk& DstResourceCache) const
{
    if (!HasDescriptorSet(DESCRIPTOR_SET_ID_STATIC_MUTABLE) || m_pResourceCache == nullptr)
        return;

    // SrcResourceCache contains only static resources.
    // DstResourceCache contains static and mutable resources.
    const auto& SrcResourceCache = *m_pResourceCache;
    const auto  StaticSetIdx     = GetDescriptorSetIndex<DESCRIPTOR_SET_ID_STATIC_MUTABLE>();
    const auto& SrcDescrSet      = SrcResourceCache.GetDescriptorSet(StaticSetIdx);
    auto&       DstDescrSet      = DstResourceCache.GetDescriptorSet(StaticSetIdx);
    const auto  ResIdxRange      = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    const auto  SrcCacheType     = SrcResourceCache.GetContentType();
    const auto  DstCacheType     = DstResourceCache.GetContentType();

    for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& Attr    = GetResourceAttribs(r);
        VERIFY_EXPR(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER && Attr.IsImmutableSamplerAssigned())
            continue; // Skip immutable separate samplers

        for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
        {
            auto           SrcCacheOffset = Attr.CacheOffset(SrcCacheType) + ArrInd;
            const auto&    SrcCachedRes   = SrcDescrSet.GetResource(SrcCacheOffset);
            IDeviceObject* pObject        = SrcCachedRes.pObject.RawPtr<IDeviceObject>();
            if (!pObject)
                LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

            auto           DstCacheOffset  = Attr.CacheOffset(DstCacheType) + ArrInd;
            IDeviceObject* pCachedResource = DstDescrSet.GetResource(DstCacheOffset).pObject;
            if (pCachedResource != pObject)
            {
                VERIFY(pCachedResource == nullptr, "Static resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");
                BindResource(pObject, ArrInd, r, DstResourceCache);
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
    VERIFY(HasDescriptorSet(DESCRIPTOR_SET_ID_DYNAMIC), "This shader resource layout does not contain dynamic resources");
    VERIFY_EXPR(vkDynamicDescriptorSet != VK_NULL_HANDLE);
    VERIFY_EXPR(ResourceCache.GetContentType() == CacheContentType::SRB);

#ifdef DILIGENT_DEBUG
    static constexpr size_t ImgUpdateBatchSize          = 4;
    static constexpr size_t BuffUpdateBatchSize         = 2;
    static constexpr size_t TexelBuffUpdateBatchSize    = 2;
    static constexpr size_t AccelStructBatchSize        = 2;
    static constexpr size_t WriteDescriptorSetBatchSize = 2;
#else
    static constexpr size_t ImgUpdateBatchSize          = 128;
    static constexpr size_t BuffUpdateBatchSize         = 64;
    static constexpr size_t TexelBuffUpdateBatchSize    = 32;
    static constexpr size_t AccelStructBatchSize        = 32;
    static constexpr size_t WriteDescriptorSetBatchSize = 32;
#endif

    // Do not zero-initiaize arrays!
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

    const auto  DynamicSetIdx = GetDescriptorSetIndex<DESCRIPTOR_SET_ID_DYNAMIC>();
    const auto& SetResources  = ResourceCache.GetDescriptorSet(DynamicSetIdx);
    const auto& LogicalDevice = GetDevice()->GetLogicalDevice();
    const auto  ResIdxRange   = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

    constexpr auto CacheType = CacheContentType::SRB;

    for (Uint32 ResNum = ResIdxRange.first, ArrElem = 0; ResNum < ResIdxRange.second;)
    {
        const auto& Attr        = GetResourceAttribs(ResNum);
        const auto  CacheOffset = Attr.CacheOffset(CacheType);
        const auto  ArraySize   = Attr.ArraySize;
        const auto  DescrType   = Attr.GetDescriptorType();

#ifdef DILIGENT_DEBUG
        {
            const auto& Res = GetResourceDesc(ResNum);
            VERIFY_EXPR(ArraySize == GetResourceDesc(ResNum).ArraySize);
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
        WriteDescrSetIt->descriptorType = GetVkDescriptorType(DescrType);

        // For every resource type, try to batch as many descriptor updates as we can
        static_assert(static_cast<Uint32>(DescriptorType::Count) == 15, "Please update the switch below to handle the new descriptor type");
        switch (DescrType)
        {
            case DescriptorType::UniformBuffer:
            case DescriptorType::UniformBufferDynamic:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                while (ArrElem < ArraySize && DescrBuffIt != DescrBuffInfoArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(CacheOffset + ArrElem);
                    *DescrBuffIt          = CachedRes.GetUniformBufferDescriptorWriteInfo();
                    ++DescrBuffIt;
                    ++ArrElem;
                }
                break;

            case DescriptorType::StorageBuffer:
            case DescriptorType::StorageBufferDynamic:
            case DescriptorType::StorageBuffer_ReadOnly:
            case DescriptorType::StorageBufferDynamic_ReadOnly:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                while (ArrElem < ArraySize && DescrBuffIt != DescrBuffInfoArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(CacheOffset + ArrElem);
                    *DescrBuffIt          = CachedRes.GetStorageBufferDescriptorWriteInfo();
                    ++DescrBuffIt;
                    ++ArrElem;
                }
                break;

            case DescriptorType::UniformTexelBuffer:
            case DescriptorType::StorageTexelBuffer:
            case DescriptorType::StorageTexelBuffer_ReadOnly:
                WriteDescrSetIt->pTexelBufferView = &(*BuffViewIt);
                while (ArrElem < ArraySize && BuffViewIt != DescrBuffViewArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(CacheOffset + ArrElem);
                    *BuffViewIt           = CachedRes.GetBufferViewWriteInfo();
                    ++BuffViewIt;
                    ++ArrElem;
                }
                break;

            case DescriptorType::CombinedImageSampler:
            case DescriptorType::SeparateImage:
            case DescriptorType::StorageImage:
            case DescriptorType::InputAttachment:
                WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                while (ArrElem < ArraySize && DescrImgIt != DescrImgInfoArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(CacheOffset + ArrElem);
                    *DescrImgIt           = CachedRes.GetImageDescriptorWriteInfo(Attr.IsImmutableSamplerAssigned());
                    ++DescrImgIt;
                    ++ArrElem;
                }
                break;

            case DescriptorType::Sampler:
                // Immutable samplers are permanently bound into the set layout; later binding a sampler
                // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
                if (!Attr.IsImmutableSamplerAssigned())
                {
                    WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                    while (ArrElem < ArraySize && DescrImgIt != DescrImgInfoArr.end())
                    {
                        const auto& CachedRes = SetResources.GetResource(CacheOffset + ArrElem);
                        *DescrImgIt           = CachedRes.GetSamplerDescriptorWriteInfo();
                        ++DescrImgIt;
                        ++ArrElem;
                    }
                }
                else
                {
                    ArrElem                          = ArraySize;
                    WriteDescrSetIt->dstArrayElement = ArraySize;
                }
                break;

            case DescriptorType::AccelerationStructure:
                WriteDescrSetIt->pNext = &(*AccelStructIt);
                while (ArrElem < ArraySize && AccelStructIt != DescrAccelStructArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(CacheOffset + ArrElem);
                    *AccelStructIt        = CachedRes.GetAccelerationStructureWriteInfo();
                    ++AccelStructIt;
                    ++ArrElem;
                }
                break;

            default:
                UNEXPECTED("Unexpected resource type");
        }

        WriteDescrSetIt->descriptorCount = ArrElem - WriteDescrSetIt->dstArrayElement;
        if (ArrElem == ArraySize)
        {
            ArrElem = 0;
            ++ResNum;
        }
        // descriptorCount == 0 for immutable separate samplers
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
            auto DescrWriteCount = static_cast<Uint32>(std::distance(WriteDescrSetArr.begin(), WriteDescrSetIt));
            if (DescrWriteCount > 0)
                LogicalDevice.UpdateDescriptorSets(DescrWriteCount, WriteDescrSetArr.data(), 0, nullptr);

            DescrImgIt      = DescrImgInfoArr.begin();
            DescrBuffIt     = DescrBuffInfoArr.begin();
            BuffViewIt      = DescrBuffViewArr.begin();
            AccelStructIt   = DescrAccelStructArr.begin();
            WriteDescrSetIt = WriteDescrSetArr.begin();
        }
    }

    auto DescrWriteCount = static_cast<Uint32>(std::distance(WriteDescrSetArr.begin(), WriteDescrSetIt));
    if (DescrWriteCount > 0)
        LogicalDevice.UpdateDescriptorSets(DescrWriteCount, WriteDescrSetArr.data(), 0, nullptr);
}

namespace
{

struct BindResourceHelper
{
    ShaderResourceCacheVk::Resource&                        DstRes;
    const PipelineResourceDesc&                             ResDesc;
    const PipelineResourceSignatureVkImpl::ResourceAttribs& Attribs;
    const Uint32                                            ArrayIndex;
    const VkDescriptorSet                                   vkDescrSet;
    PipelineResourceSignatureVkImpl const&                  Signature;
    ShaderResourceCacheVk&                                  ResourceCache;

    void BindResource(IDeviceObject* pObj) const;

private:
    void CacheUniformBuffer(IDeviceObject* pBuffer,
                            Uint16&        DynamicBuffersCounter) const;

    void CacheStorageBuffer(IDeviceObject* pBufferView,
                            Uint16&        DynamicBuffersCounter) const;

    void CacheTexelBuffer(IDeviceObject* pBufferView,
                          Uint16&        DynamicBuffersCounter) const;

    void CacheImage(IDeviceObject* pTexView) const;

    void CacheSeparateSampler(IDeviceObject* pSampler) const;

    void CacheInputAttachment(IDeviceObject* pTexView) const;

    void CacheAccelerationStructure(IDeviceObject* pTLAS) const;

    template <typename ObjectType, typename TPreUpdateObject>
    bool UpdateCachedResource(RefCntAutoPtr<ObjectType>&& pObject,
                              TPreUpdateObject            PreUpdateObject) const;

    bool IsImmutableSamplerAssigned() const { return Attribs.IsImmutableSamplerAssigned(); }

    // Updates resource descriptor in the descriptor set
    inline void UpdateDescriptorHandle(const VkDescriptorImageInfo*                        pImageInfo,
                                       const VkDescriptorBufferInfo*                       pBufferInfo,
                                       const VkBufferView*                                 pTexelBufferView,
                                       const VkWriteDescriptorSetAccelerationStructureKHR* pAccelStructInfo = nullptr) const;
};

void BindResourceHelper::BindResource(IDeviceObject* pObj) const
{
#ifdef DILIGENT_DEBUG
    if (ResourceCache.GetContentType() == PipelineResourceSignatureVkImpl::CacheContentType::SRB)
    {
        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC || ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
        {
            VERIFY(vkDescrSet != VK_NULL_HANDLE, "Static and mutable variables must have valid vulkan descriptor set assigned");
            // Dynamic variables do not have vulkan descriptor set only until they are assigned one the first time
        }
    }
    else if (ResourceCache.GetContentType() == PipelineResourceSignatureVkImpl::CacheContentType::Signature)
    {
        VERIFY(vkDescrSet == VK_NULL_HANDLE, "Static shader resource cache should not have vulkan descriptor set allocation");
    }
    else
    {
        UNEXPECTED("Unexpected shader resource cache content type");
    }
#endif

    if (pObj)
    {
        static_assert(static_cast<Uint32>(DescriptorType::Count) == 15, "Please update the switch below to handle the new descriptor type");
        switch (DstRes.Type)
        {
            case DescriptorType::UniformBuffer:
            case DescriptorType::UniformBufferDynamic:
                CacheUniformBuffer(pObj, ResourceCache.GetDynamicBuffersCounter());
                break;

            case DescriptorType::StorageBuffer:
            case DescriptorType::StorageBuffer_ReadOnly:
            case DescriptorType::StorageBufferDynamic:
            case DescriptorType::StorageBufferDynamic_ReadOnly:
                CacheStorageBuffer(pObj, ResourceCache.GetDynamicBuffersCounter());
                break;

            case DescriptorType::UniformTexelBuffer:
            case DescriptorType::StorageTexelBuffer:
            case DescriptorType::StorageTexelBuffer_ReadOnly:
                CacheTexelBuffer(pObj, ResourceCache.GetDynamicBuffersCounter());
                break;

            case DescriptorType::StorageImage:
            case DescriptorType::SeparateImage:
            case DescriptorType::CombinedImageSampler:
                CacheImage(pObj);
                break;

            case DescriptorType::Sampler:
                if (!IsImmutableSamplerAssigned())
                {
                    CacheSeparateSampler(pObj);
                }
                else
                {
                    // Immutable samplers are permanently bound into the set layout; later binding a sampler
                    // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
                    LOG_ERROR_MESSAGE("Attempting to assign a sampler to an immutable sampler '", ResDesc.Name, '\'');
                }
                break;

            case DescriptorType::InputAttachment:
                CacheInputAttachment(pObj);
                break;

            case DescriptorType::AccelerationStructure:
                CacheAccelerationStructure(pObj);
                break;

            default: UNEXPECTED("Unknown resource type ", static_cast<Int32>(DstRes.Type));
        }
    }
    else
    {
        if (DstRes.pObject && ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            LOG_ERROR_MESSAGE("Shader variable '", ResDesc.Name, "' is not dynamic but being unbound. This is an error and may cause unpredicted behavior. ",
                              "Use another shader resource binding instance or label shader variable as dynamic if you need to bind another resource.");
        }

        DstRes.pObject.Release();
    }
}

template <typename ObjectType, typename TPreUpdateObject>
bool BindResourceHelper::UpdateCachedResource(RefCntAutoPtr<ObjectType>&& pObject,
                                              TPreUpdateObject            PreUpdateObject) const
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    if (pObject)
    {
        if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as writing descriptors while they are used by the GPU is an undefined behavior
            return false;
        }

        PreUpdateObject(DstRes.pObject.template RawPtr<const ObjectType>(), pObject.template RawPtr<const ObjectType>());
        DstRes.pObject.Attach(pObject.Detach());
        return true;
    }
    else
    {
        return false;
    }
}

void BindResourceHelper::CacheUniformBuffer(IDeviceObject* pBuffer,
                                            Uint16&        DynamicBuffersCounter) const
{
    // clang-format off
    VERIFY(DstRes.Type == DescriptorType::UniformBuffer ||
           DstRes.Type == DescriptorType::UniformBufferDynamic,
           "Uniform buffer resource is expected");
    // clang-format on
    RefCntAutoPtr<BufferVkImpl> pBufferVk{pBuffer, IID_BufferVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyConstantBufferBinding(ResDesc.Name, ResDesc.ArraySize, ResDesc.VarType, ResDesc.Flags, ArrayIndex,
                                pBuffer, pBufferVk.RawPtr(), DstRes.pObject.RawPtr());
#endif

    auto UpdateDynamicBuffersCounter = [&DynamicBuffersCounter](const BufferVkImpl* pOldBuffer, const BufferVkImpl* pNewBuffer) {
        if (pOldBuffer != nullptr && pOldBuffer->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(DynamicBuffersCounter > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
            --DynamicBuffersCounter;
        }
        if (pNewBuffer != nullptr && pNewBuffer->GetDesc().Usage == USAGE_DYNAMIC)
            ++DynamicBuffersCounter;
    };
    if (UpdateCachedResource(std::move(pBufferVk), UpdateDynamicBuffersCounter))
    {
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC descriptor type require
        // buffer to be created with VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT

        // Do not update descriptor for a dynamic uniform buffer. All dynamic resource
        // descriptors are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorBufferInfo DescrBuffInfo = DstRes.GetUniformBufferDescriptorWriteInfo();
            UpdateDescriptorHandle(nullptr, &DescrBuffInfo, nullptr);
        }
    }
}

void BindResourceHelper::CacheStorageBuffer(IDeviceObject* pBufferView,
                                            Uint16&        DynamicBuffersCounter) const
{
    // clang-format off
    VERIFY(DstRes.Type == DescriptorType::StorageBuffer ||
           DstRes.Type == DescriptorType::StorageBuffer_ReadOnly ||
           DstRes.Type == DescriptorType::StorageBufferDynamic ||
           DstRes.Type == DescriptorType::StorageBufferDynamic_ReadOnly,
           "Storage buffer resource is expected");
    // clang-format on

    RefCntAutoPtr<BufferViewVkImpl> pBufferViewVk{pBufferView, IID_BufferViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = DescriptorTypeToBufferView(DstRes.Type);
        VerifyResourceViewBinding(ResDesc.Name, ResDesc.ArraySize, ResDesc.VarType, ArrayIndex,
                                  pBufferView, pBufferViewVk.RawPtr(),
                                  {RequiredViewType}, RESOURCE_DIM_BUFFER,
                                  false, // IsMultisample
                                  DstRes.pObject.RawPtr());
        if (pBufferViewVk != nullptr)
        {
            const auto& ViewDesc = pBufferViewVk->GetDesc();
            const auto& BuffDesc = pBufferViewVk->GetBuffer()->GetDesc();
            if (BuffDesc.Mode != BUFFER_MODE_STRUCTURED && BuffDesc.Mode != BUFFER_MODE_RAW)
            {
                LOG_ERROR_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                  ResDesc.Name, "': structured buffer view is expected.");
            }
        }
    }
#endif

    auto UpdateDynamicBuffersCounter = [&DynamicBuffersCounter](const BufferViewVkImpl* pOldBufferView, const BufferViewVkImpl* pNewBufferView) {
        if (pOldBufferView != nullptr && pOldBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(DynamicBuffersCounter > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
            --DynamicBuffersCounter;
        }
        if (pNewBufferView != nullptr && pNewBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
            ++DynamicBuffersCounter;
    };

    if (UpdateCachedResource(std::move(pBufferViewVk), UpdateDynamicBuffersCounter))
    {
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC descriptor type
        // require buffer to be created with VK_BUFFER_USAGE_STORAGE_BUFFER_BIT (13.2.4)

        // Do not update descriptor for a dynamic storage buffer. All dynamic resource
        // descriptors are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorBufferInfo DescrBuffInfo = DstRes.GetStorageBufferDescriptorWriteInfo();
            UpdateDescriptorHandle(nullptr, &DescrBuffInfo, nullptr);
        }
    }
}

void BindResourceHelper::CacheTexelBuffer(IDeviceObject* pBufferView,
                                          Uint16&        DynamicBuffersCounter) const
{
    // clang-format off
    VERIFY(DstRes.Type == DescriptorType::UniformTexelBuffer || 
           DstRes.Type == DescriptorType::StorageTexelBuffer ||
           DstRes.Type == DescriptorType::StorageTexelBuffer_ReadOnly,
           "Uniform or storage buffer resource is expected");
    // clang-format on

    RefCntAutoPtr<BufferViewVkImpl> pBufferViewVk{pBufferView, IID_BufferViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = DescriptorTypeToBufferView(DstRes.Type);
        VerifyResourceViewBinding(ResDesc.Name, ResDesc.ArraySize, ResDesc.VarType, ArrayIndex,
                                  pBufferView, pBufferViewVk.RawPtr(),
                                  {RequiredViewType}, RESOURCE_DIM_BUFFER,
                                  false, // IsMultisample
                                  DstRes.pObject.RawPtr());
        if (pBufferViewVk != nullptr)
        {
            const auto& ViewDesc = pBufferViewVk->GetDesc();
            const auto& BuffDesc = pBufferViewVk->GetBuffer()->GetDesc();
            if (!((BuffDesc.Mode == BUFFER_MODE_FORMATTED && ViewDesc.Format.ValueType != VT_UNDEFINED) || BuffDesc.Mode == BUFFER_MODE_RAW))
            {
                LOG_ERROR_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                  ResDesc.Name, "': formatted buffer view is expected.");
            }
        }
    }
#endif

    auto UpdateDynamicBuffersCounter = [&DynamicBuffersCounter](const BufferViewVkImpl* pOldBufferView, const BufferViewVkImpl* pNewBufferView) {
        if (pOldBufferView != nullptr && pOldBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(DynamicBuffersCounter > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
            --DynamicBuffersCounter;
        }
        if (pNewBufferView != nullptr && pNewBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
            ++DynamicBuffersCounter;
    };

    if (UpdateCachedResource(std::move(pBufferViewVk), UpdateDynamicBuffersCounter))
    {
        // The following bits must have been set at buffer creation time:
        //  * VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER  ->  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
        //  * VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER  ->  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT

        // Do not update descriptor for a dynamic texel buffer. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkBufferView BuffView = DstRes.pObject.RawPtr<BufferViewVkImpl>()->GetVkBufferView();
            UpdateDescriptorHandle(nullptr, nullptr, &BuffView);
        }
    }
}

void BindResourceHelper::CacheImage(IDeviceObject* pTexView) const
{
    // clang-format off
    VERIFY(DstRes.Type == DescriptorType::StorageImage ||
           DstRes.Type == DescriptorType::SeparateImage ||
           DstRes.Type == DescriptorType::CombinedImageSampler,
           "Storage image, separate image or sampled image resource is expected");
    // clang-format on
    RefCntAutoPtr<TextureViewVkImpl> pTexViewVk0{pTexView, IID_TextureViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = DescriptorTypeToTextureView(DstRes.Type);
        VerifyResourceViewBinding(ResDesc.Name, ResDesc.ArraySize, ResDesc.VarType, ArrayIndex,
                                  pTexView, pTexViewVk0.RawPtr(),
                                  {RequiredViewType},
                                  RESOURCE_DIM_UNDEFINED, // Required resource dimension is not known
                                  false,                  // IsMultisample
                                  DstRes.pObject.RawPtr());
    }
#endif
    if (UpdateCachedResource(std::move(pTexViewVk0), [](const TextureViewVkImpl*, const TextureViewVkImpl*) {}))
    {
        // We can do RawPtr here safely since UpdateCachedResource() returned true
        auto* pTexViewVk = DstRes.pObject.RawPtr<TextureViewVkImpl>();
#ifdef DILIGENT_DEVELOPMENT
        if (DstRes.Type == DescriptorType::CombinedImageSampler && !IsImmutableSamplerAssigned())
        {
            if (pTexViewVk->GetSampler() == nullptr)
            {
                LOG_ERROR_MESSAGE("Error binding texture view '", pTexViewVk->GetDesc().Name, "' to variable '", GetShaderResourcePrintName(ResDesc, ArrayIndex),
                                  "'. No sampler is assigned to the view");
            }
        }
#endif

        // Do not update descriptor for a dynamic image. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = DstRes.GetImageDescriptorWriteInfo(IsImmutableSamplerAssigned());
            UpdateDescriptorHandle(&DescrImgInfo, nullptr, nullptr);
        }

        if (Attribs.SamplerInd != PipelineResourceSignatureVkImpl::ResourceAttribs::InvalidSamplerInd)
        {
            VERIFY(DstRes.Type == DescriptorType::SeparateImage,
                   "Only separate images can be assigned separate samplers when using HLSL-style combined samplers.");
            VERIFY(!IsImmutableSamplerAssigned(), "Separate image can't be assigned an immutable sampler.");

            auto& SamplerResDesc = Signature.GetResourceDesc(Attribs.SamplerInd);
            auto& SamplerAttribs = Signature.GetResourceAttribs(Attribs.SamplerInd);
            VERIFY_EXPR(SamplerResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);

            if (!SamplerAttribs.IsImmutableSamplerAssigned())
            {
                auto* pSampler = pTexViewVk->GetSampler();
                if (pSampler != nullptr)
                {
                    VERIFY_EXPR(DstRes.Type == DescriptorType::SeparateImage);
                    DEV_CHECK_ERR(SamplerResDesc.ArraySize == 1 || SamplerResDesc.ArraySize == ResDesc.ArraySize,
                                  "Array size (", SamplerResDesc.ArraySize,
                                  ") of separate sampler variable '",
                                  SamplerResDesc.Name,
                                  "' must be one or the same as the array size (", ResDesc.ArraySize,
                                  ") of separate image variable '", ResDesc.Name, "' it is assigned to");

                    const auto CacheType     = ResourceCache.GetContentType();
                    auto&      DstDescrSet   = ResourceCache.GetDescriptorSet(SamplerAttribs.DescrSet);
                    const auto SamplerArrInd = SamplerResDesc.ArraySize == 1 ? 0 : ArrayIndex;
                    auto&      SampleDstRes  = DstDescrSet.GetResource(SamplerAttribs.CacheOffset(CacheType) + SamplerArrInd);

                    BindResourceHelper SeparateSampler{
                        SampleDstRes,
                        SamplerResDesc,
                        SamplerAttribs,
                        SamplerArrInd,
                        DstDescrSet.GetVkDescriptorSet(),
                        Signature,
                        ResourceCache};
                    SeparateSampler.BindResource(pSampler);
                }
                else
                {
                    LOG_ERROR_MESSAGE("Failed to bind sampler to sampler variable '", SamplerResDesc.Name,
                                      "' assigned to separate image '", GetShaderResourcePrintName(ResDesc, ArrayIndex),
                                      "': no sampler is set in texture view '", pTexViewVk->GetDesc().Name, '\'');
                }
            }
        }
    }
}

void BindResourceHelper::CacheSeparateSampler(IDeviceObject* pSampler) const
{
    VERIFY(DstRes.Type == DescriptorType::Sampler, "Separate sampler resource is expected");
    VERIFY(!IsImmutableSamplerAssigned(), "This separate sampler is assigned an immutable sampler");

    RefCntAutoPtr<SamplerVkImpl> pSamplerVk{pSampler, IID_Sampler};
#ifdef DILIGENT_DEVELOPMENT
    if (pSampler != nullptr && pSamplerVk == nullptr)
    {
        LOG_ERROR_MESSAGE("Failed to bind object '", pSampler->GetDesc().Name, "' to variable '", GetShaderResourcePrintName(ResDesc, ArrayIndex),
                          "'. Unexpected object type: sampler is expected");
    }
    if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr && DstRes.pObject != pSamplerVk)
    {
        auto VarTypeStr = GetShaderVariableTypeLiteralName(ResDesc.VarType);
        LOG_ERROR_MESSAGE("Non-null sampler is already bound to ", VarTypeStr, " shader variable '", GetShaderResourcePrintName(ResDesc, ArrayIndex),
                          "'. Attempting to bind another sampler or null is an error and may "
                          "cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic.");
    }
#endif
    if (UpdateCachedResource(std::move(pSamplerVk), [](const SamplerVkImpl*, const SamplerVkImpl*) {}))
    {
        // Do not update descriptor for a dynamic sampler. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = DstRes.GetSamplerDescriptorWriteInfo();
            UpdateDescriptorHandle(&DescrImgInfo, nullptr, nullptr);
        }
    }
}

void BindResourceHelper::CacheInputAttachment(IDeviceObject* pTexView) const
{
    VERIFY(DstRes.Type == DescriptorType::InputAttachment, "Input attachment resource is expected");
    RefCntAutoPtr<TextureViewVkImpl> pTexViewVk0{pTexView, IID_TextureViewVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyResourceViewBinding(ResDesc.Name, ResDesc.ArraySize, ResDesc.VarType, ArrayIndex,
                              pTexView, pTexViewVk0.RawPtr(),
                              {TEXTURE_VIEW_SHADER_RESOURCE},
                              RESOURCE_DIM_UNDEFINED,
                              false, // IsMultisample
                              DstRes.pObject.RawPtr());
#endif
    if (UpdateCachedResource(std::move(pTexViewVk0), [](const TextureViewVkImpl*, const TextureViewVkImpl*) {}))
    {
        // Do not update descriptor for a dynamic image. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = DstRes.GetInputAttachmentDescriptorWriteInfo();
            UpdateDescriptorHandle(&DescrImgInfo, nullptr, nullptr);
        }
        //
    }
}

void BindResourceHelper::CacheAccelerationStructure(IDeviceObject* pTLAS) const
{
    VERIFY(DstRes.Type == DescriptorType::AccelerationStructure, "Acceleration Structure resource is expected");
    RefCntAutoPtr<TopLevelASVkImpl> pTLASVk{pTLAS, IID_TopLevelASVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyTLASResourceBinding(ResDesc.Name, ResDesc.ArraySize, ResDesc.VarType, ArrayIndex, pTLAS, pTLASVk.RawPtr(), DstRes.pObject.RawPtr());
#endif
    if (UpdateCachedResource(std::move(pTLASVk), [](const TopLevelASVkImpl*, const TopLevelASVkImpl*) {}))
    {
        // Do not update descriptor for a dynamic TLAS. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkWriteDescriptorSetAccelerationStructureKHR DescrASInfo = DstRes.GetAccelerationStructureWriteInfo();
            UpdateDescriptorHandle(nullptr, nullptr, nullptr, &DescrASInfo);
        }
        //
    }
}

void BindResourceHelper::UpdateDescriptorHandle(const VkDescriptorImageInfo*                        pImageInfo,
                                                const VkDescriptorBufferInfo*                       pBufferInfo,
                                                const VkBufferView*                                 pTexelBufferView,
                                                const VkWriteDescriptorSetAccelerationStructureKHR* pAccelStructInfo) const
{
    VERIFY_EXPR(vkDescrSet != VK_NULL_HANDLE);

    VkWriteDescriptorSet WriteDescrSet;
    WriteDescrSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    WriteDescrSet.pNext           = pAccelStructInfo;
    WriteDescrSet.dstSet          = vkDescrSet;
    WriteDescrSet.dstBinding      = Attribs.BindingIndex;
    WriteDescrSet.dstArrayElement = ArrayIndex;
    WriteDescrSet.descriptorCount = 1;
    // descriptorType must be the same type as that specified in VkDescriptorSetLayoutBinding for dstSet at dstBinding.
    // The type of the descriptor also controls which array the descriptors are taken from. (13.2.4)
    WriteDescrSet.descriptorType   = GetVkDescriptorType(DstRes.Type);
    WriteDescrSet.pImageInfo       = pImageInfo;
    WriteDescrSet.pBufferInfo      = pBufferInfo;
    WriteDescrSet.pTexelBufferView = pTexelBufferView;

    Signature.GetDevice()->GetLogicalDevice().UpdateDescriptorSets(1, &WriteDescrSet, 0, nullptr);
}

} // namespace

void PipelineResourceSignatureVkImpl::BindResource(IDeviceObject*         pObj,
                                                   Uint32                 ArrayIndex,
                                                   Uint32                 ResIndex,
                                                   ShaderResourceCacheVk& ResourceCache) const
{
    auto&      ResDesc     = GetResourceDesc(ResIndex);
    auto&      Attribs     = GetResourceAttribs(ResIndex);
    const auto CacheType   = ResourceCache.GetContentType();
    auto&      DstDescrSet = ResourceCache.GetDescriptorSet(Attribs.DescrSet);
    auto&      DstRes      = DstDescrSet.GetResource(Attribs.CacheOffset(CacheType) + ArrayIndex);

    VERIFY_EXPR(ArrayIndex < ResDesc.ArraySize);
    VERIFY(DstRes.Type == Attribs.GetDescriptorType(), "Inconsistent types");

    BindResourceHelper Helper{
        DstRes,
        ResDesc,
        Attribs,
        ArrayIndex,
        DstDescrSet.GetVkDescriptorSet(),
        *this,
        ResourceCache};

    Helper.BindResource(pObj);
}

#ifdef DILIGENT_DEVELOPMENT
bool PipelineResourceSignatureVkImpl::DvpValidateCommittedResource(const SPIRVShaderResourceAttribs& SPIRVAttribs,
                                                                   Uint32                            ResIndex,
                                                                   ShaderResourceCacheVk&            ResourceCache) const
{
    VERIFY_EXPR(ResIndex < m_Desc.NumResources);
    const auto& ResInfo    = m_Desc.Resources[ResIndex];
    const auto& ResAttribs = m_pResourceAttribs[ResIndex];
    VERIFY(strcmp(ResInfo.Name, SPIRVAttribs.Name) == 0, "Inconsistent resource names");

    bool BindingsOK = true;

    const auto& DescrSetResources = ResourceCache.GetDescriptorSet(ResAttribs.DescrSet);
    const auto  CacheType         = ResourceCache.GetContentType();
    const auto  CacheOffset       = ResAttribs.CacheOffset(CacheType);

    VERIFY_EXPR(SPIRVAttribs.ArraySize <= ResAttribs.ArraySize);

    switch (ResAttribs.GetDescriptorType())
    {
        case DescriptorType::UniformBuffer:
        case DescriptorType::UniformBufferDynamic:
        {
            VERIFY_EXPR(ResInfo.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER);
            for (Uint32 i = 0; i < SPIRVAttribs.ArraySize; ++i)
            {
                const auto& Res = DescrSetResources.GetResource(CacheOffset + i);

                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (const auto* pBufferVk = Res.pObject.RawPtr<BufferVkImpl>())
                {
                    if (pBufferVk->GetDesc().uiSizeInBytes < SPIRVAttribs.BufferStaticSize)
                    {
                        // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
                        LOG_WARNING_MESSAGE("The size of uniform buffer '",
                                            pBufferVk->GetDesc().Name, "' bound to shader variable '",
                                            GetShaderResourcePrintName(ResInfo, i), "' is ", pBufferVk->GetDesc().uiSizeInBytes,
                                            " bytes, but the shader expects at least ", SPIRVAttribs.BufferStaticSize,
                                            " bytes.");
                    }
                }
                else
                {
                    // Missing resource error is logged by BindResourceHelper::CacheUniformBuffer
                }
            }
        }
        break;

        case DescriptorType::StorageBuffer:
        case DescriptorType::StorageBuffer_ReadOnly:
        case DescriptorType::StorageBufferDynamic:
        case DescriptorType::StorageBufferDynamic_ReadOnly:
        {
            VERIFY_EXPR(ResInfo.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV || ResInfo.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV);
            for (Uint32 i = 0; i < SPIRVAttribs.ArraySize; ++i)
            {
                const auto& Res = DescrSetResources.GetResource(CacheOffset + i);

                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (auto* pBufferViewVk = Res.pObject.RawPtr<BufferViewVkImpl>())
                {
                    const auto* pBufferVk = ValidatedCast<BufferVkImpl>(pBufferViewVk->GetBuffer());
                    const auto& ViewDesc  = pBufferViewVk->GetDesc();
                    const auto& BuffDesc  = pBufferVk->GetDesc();

                    if (BuffDesc.ElementByteStride == 0)
                    {
                        if (ViewDesc.ByteWidth < SPIRVAttribs.BufferStaticSize)
                        {
                            // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
                            LOG_WARNING_MESSAGE("The size of buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' bound to shader variable '",
                                                GetShaderResourcePrintName(ResInfo, i), "' is ", ViewDesc.ByteWidth, " bytes, but the shader expects at least ",
                                                SPIRVAttribs.BufferStaticSize, " bytes.");
                        }
                    }
                    else
                    {
                        if (ViewDesc.ByteWidth < SPIRVAttribs.BufferStaticSize || (ViewDesc.ByteWidth - SPIRVAttribs.BufferStaticSize) % BuffDesc.ElementByteStride != 0)
                        {
                            // For buffers with dynamic arrays we know only static part size and array element stride.
                            // Element stride in the shader may be differ than in the code. Here we check that the buffer size is exactly the same as the array with N elements.
                            LOG_WARNING_MESSAGE("The size (", ViewDesc.ByteWidth, ") and stride (", BuffDesc.ElementByteStride, ") of buffer view '",
                                                ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' bound to shader variable '",
                                                GetShaderResourcePrintName(ResInfo, i), "' are incompatible with what the shader expects. This may be the result of the array element size mismatch.");
                        }
                    }
                }
                else
                {
                    // Missing resource error is logged by BindResourceHelper::CacheStorageBuffer
                }
            }
        }
        break;

        case DescriptorType::StorageImage:
        case DescriptorType::SeparateImage:
        case DescriptorType::CombinedImageSampler:
        {
            VERIFY_EXPR(ResInfo.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV || ResInfo.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV);
            for (Uint32 i = 0; i < SPIRVAttribs.ArraySize; ++i)
            {
                const auto& Res = DescrSetResources.GetResource(CacheOffset + i);
                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (const auto* pTexViewVk = Res.pObject.RawPtr<TextureViewVkImpl>())
                {
                    if (!ValidateResourceViewDimension(ResInfo.Name, ResInfo.ArraySize, i, pTexViewVk, SPIRVAttribs.GetResourceDimension(), SPIRVAttribs.IsMultisample()))
                        BindingsOK = false;
                }
                else
                {
                    // Missing resource error is logged by BindResourceHelper::CacheImage
                }
            }
        }
        break;

        default:
            break;
            // Nothing to do
    }

    return BindingsOK;
}
#endif

} // namespace Diligent
