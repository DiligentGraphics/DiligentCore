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

#include "pch.h"

#include "RenderDeviceWebGPUImpl.hpp"
#include "PipelineResourceSignatureWebGPUImpl.hpp"
#include "DynamicLinearAllocator.hpp"
#include "WebGPUTypeConversions.hpp"

namespace Diligent
{

namespace
{

BindGroupEntryType GetBindGroupEntryType(const PipelineResourceDesc& Res)
{
    VERIFY((Res.Flags & ~GetValidPipelineResourceFlags(Res.ResourceType)) == 0,
           "Invalid resource flags. This error should've been caught by ValidatePipelineResourceSignatureDesc.");

    const bool WithDynamicOffset = (Res.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0;
    const bool IsReadOnly        = false;

    static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update the switch below to handle the new shader resource type");
    switch (Res.ResourceType)
    {
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
            return WithDynamicOffset ? BindGroupEntryType::UniformBufferDynamic : BindGroupEntryType::UniformBuffer;

        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
            return BindGroupEntryType::Texture;

        case SHADER_RESOURCE_TYPE_BUFFER_SRV:
            return WithDynamicOffset ? BindGroupEntryType::StorageBufferDynamic_ReadOnly : BindGroupEntryType::StorageBuffer_ReadOnly;

        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
            return (IsReadOnly ? BindGroupEntryType::StorageBufferDynamic_ReadOnly : BindGroupEntryType::StorageTexture_ReadWrite);

        case SHADER_RESOURCE_TYPE_BUFFER_UAV:
            return WithDynamicOffset ?
                (IsReadOnly ? BindGroupEntryType::StorageBufferDynamic : BindGroupEntryType::StorageBufferDynamic_ReadOnly) :
                (IsReadOnly ? BindGroupEntryType::StorageBuffer : BindGroupEntryType::StorageBuffer_ReadOnly);

        case SHADER_RESOURCE_TYPE_SAMPLER:
            return BindGroupEntryType::Sampler;

        default:
            UNEXPECTED("Unknown resource type");
            return BindGroupEntryType::Count;
    }
}

Uint32 FindImmutableSamplerWGPU(const PipelineResourceDesc&          Res,
                                BindGroupEntryType                   EntryType,
                                const PipelineResourceSignatureDesc& Desc,
                                const char*                          SamplerSuffix)
{
    /*if (EntryType == BindGroupEntryType::CombinedImageSampler)
    {
        SamplerSuffix = nullptr;
    }
    else */
    if (EntryType == BindGroupEntryType::Sampler)
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

WGPUBufferBindingType GetWGPUBufferBindingType(BindGroupEntryType EntryType)
{
    switch (EntryType)
    {
        case BindGroupEntryType::UniformBuffer:
        case BindGroupEntryType::UniformBufferDynamic:
            return WGPUBufferBindingType_Uniform;

        case BindGroupEntryType::StorageBuffer:
        case BindGroupEntryType::StorageBufferDynamic:
            return WGPUBufferBindingType_Storage;

        case BindGroupEntryType::StorageBuffer_ReadOnly:
        case BindGroupEntryType::StorageBufferDynamic_ReadOnly:
            return WGPUBufferBindingType_ReadOnlyStorage;

        default:
            return WGPUBufferBindingType_Undefined;
    }
}

WGPUSamplerBindingType GetWGPUSamplerBindingType(BindGroupEntryType EntryType)
{
    // TODO: handle other sampler types
    return EntryType == BindGroupEntryType::Sampler ?
        WGPUSamplerBindingType_Filtering :
        WGPUSamplerBindingType_Undefined;
}

WGPUTextureSampleType GetWGPUTextureSampleType(BindGroupEntryType EntryType)
{
    // TODO: handle other texture types
    return EntryType == BindGroupEntryType::Texture ?
        WGPUTextureSampleType_Float :
        WGPUTextureSampleType_Undefined;
}

WGPUStorageTextureAccess GetWGPUStorageTextureAccess(BindGroupEntryType EntryType)
{
    switch (EntryType)
    {
        case BindGroupEntryType::StorageTexture_WriteOnly:
            return WGPUStorageTextureAccess_WriteOnly;

        case BindGroupEntryType::StorageTexture_ReadOnly:
            return WGPUStorageTextureAccess_ReadOnly;

        case BindGroupEntryType::StorageTexture_ReadWrite:
            return WGPUStorageTextureAccess_ReadWrite;

        default:
            return WGPUStorageTextureAccess_Undefined;
    }
}

WGPUBindGroupLayoutEntry GetWGPUBindGroupLayoutEntry(const PipelineResourceAttribsWebGPU& Attribs,
                                                     const PipelineResourceDesc&          ResDesc,
                                                     Uint32                               ArrayElement)
{
    WGPUBindGroupLayoutEntry wgpuBGLayoutEntry{};

    wgpuBGLayoutEntry.binding    = Attribs.BindingIndex + ArrayElement;
    wgpuBGLayoutEntry.visibility = ShaderStagesToWGPUShaderStageFlags(ResDesc.ShaderStages);

    const BindGroupEntryType EntryType = Attribs.GetBindGroupEntryType();
    static_assert(static_cast<size_t>(BindGroupEntryType::Count) == 12, "Please update the switch below to handle the new bind group entry type");
    if (WGPUBufferBindingType wgpuBufferBindingType = GetWGPUBufferBindingType(EntryType))
    {
        wgpuBGLayoutEntry.buffer.type = wgpuBufferBindingType;
        wgpuBGLayoutEntry.buffer.hasDynamicOffset =
            (EntryType == BindGroupEntryType::UniformBufferDynamic ||
             EntryType == BindGroupEntryType::StorageBufferDynamic ||
             EntryType == BindGroupEntryType::StorageBufferDynamic_ReadOnly);

        // If this is 0, it is ignored by pipeline creation, and instead draw/dispatch commands validate that each binding
        // in the GPUBindGroup satisfies the minimum buffer binding size of the variable.
        wgpuBGLayoutEntry.buffer.minBindingSize = 0;
    }
    else if (WGPUSamplerBindingType wgpuSamplerBindingType = GetWGPUSamplerBindingType(EntryType))
    {
        wgpuBGLayoutEntry.sampler.type = wgpuSamplerBindingType;
    }
    else if (WGPUTextureSampleType wgpuTextureSampleType = GetWGPUTextureSampleType(EntryType))
    {
        wgpuBGLayoutEntry.texture.sampleType = wgpuTextureSampleType;
        // TODO: handle other view dimensions
        wgpuBGLayoutEntry.texture.viewDimension = WGPUTextureViewDimension_2D;
        // TODO: handle multisampled textures
        wgpuBGLayoutEntry.texture.multisampled = false;
    }
    else if (WGPUStorageTextureAccess wgpuStorageTextureAccess = GetWGPUStorageTextureAccess(EntryType))
    {
        wgpuBGLayoutEntry.storageTexture.access = wgpuStorageTextureAccess;
        // TODO: handle other formats
        wgpuBGLayoutEntry.storageTexture.format = WGPUTextureFormat_RGBA32Float;
        // TODO: handle other view dimensions
        wgpuBGLayoutEntry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    }
    else
    {
        UNEXPECTED("Unexpected bind group entry type");
    }

    return wgpuBGLayoutEntry;
}

} // namespace


inline PipelineResourceSignatureWebGPUImpl::CACHE_GROUP PipelineResourceSignatureWebGPUImpl::GetResourceCacheGroup(const PipelineResourceDesc& Res)
{
    // NB: SetId is always 0 for static/mutable variables, and 1 - for dynamic ones.
    //     It is not the actual descriptor set index in the set layout!
    const size_t SetId             = VarTypeToBindGroupId(Res.VarType);
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

inline PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID PipelineResourceSignatureWebGPUImpl::VarTypeToBindGroupId(SHADER_RESOURCE_VARIABLE_TYPE VarType)
{
    return VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC ? BIND_GROUP_ID_DYNAMIC : BIND_GROUP_ID_STATIC_MUTABLE;
}

PipelineResourceSignatureWebGPUImpl::PipelineResourceSignatureWebGPUImpl(IReferenceCounters*                  pRefCounters,
                                                                         RenderDeviceWebGPUImpl*              pDevice,
                                                                         const PipelineResourceSignatureDesc& Desc,
                                                                         SHADER_TYPE                          ShaderStages,
                                                                         bool                                 bIsDeviceInternal) :
    TPipelineResourceSignatureBase{pRefCounters, pDevice, Desc, ShaderStages, bIsDeviceInternal}
{
    try
    {
        Initialize(
            GetRawAllocator(), Desc, m_ImmutableSamplers,
            [this]() //
            {
                CreateBindGroupLayouts(/*IsSerialized*/ false);
            },
            [this]() //
            {
                return ShaderResourceCacheWebGPU::GetRequiredMemorySize(GetNumBindGroups(), m_BindGroupSizes.data());
            });
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

void PipelineResourceSignatureWebGPUImpl::ImmutableSamplerAttribs::Init(RenderDeviceWebGPUImpl* pDevice, const SamplerDesc& Desc)
{
    VERIFY_EXPR(!Ptr);
    if (pDevice != nullptr)
    {
        pDevice->CreateSampler(Desc, &Ptr);
    }
    else
    {
        Ptr = NEW_RC_OBJ(GetRawAllocator(), "Dummy sampler instance", SamplerWebGPUImpl)(Desc);
    }
}


WGPUSampler PipelineResourceSignatureWebGPUImpl::ImmutableSamplerAttribs::GetWGPUSampler() const
{
    VERIFY_EXPR(Ptr);
    return Ptr.RawPtr<SamplerWebGPUImpl>()->GetWebGPUSampler();
}

void PipelineResourceSignatureWebGPUImpl::CreateBindGroupLayouts(const bool IsSerialized)
{
    // Initialize static resource cache first
    if (auto NumStaticResStages = GetNumStaticResStages())
    {
        Uint32 StaticResourceCount = 0; // The total number of static resources in all stages
                                        // accounting for array sizes.
        for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
        {
            const auto& ResDesc = m_Desc.Resources[i];
            if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                StaticResourceCount += ResDesc.ArraySize;
        }
        m_pStaticResCache->InitializeGroups(GetRawAllocator(), 1, &StaticResourceCount);
    }

    CacheOffsetsType CacheGroupSizes = {}; // Required cache size for each cache group
    BindingCountType BindingCount    = {}; // Binding count in each cache group
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& ResDesc    = m_Desc.Resources[i];
        const auto  CacheGroup = GetResourceCacheGroup(ResDesc);

        BindingCount[CacheGroup] += 1;
        // Note that we may reserve space for separate immutable samplers, which will never be used, but this is OK.
        CacheGroupSizes[CacheGroup] += ResDesc.ArraySize;
    }

    // Bind group mapping (static/mutable (0) or dynamic (1) -> set index)
    std::array<Uint32, BIND_GROUP_ID_NUM_GROUPS> BindGroupMapping = {};
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

        BindGroupMapping[BIND_GROUP_ID_STATIC_MUTABLE] = (TotalStaticBindings != 0 ? Idx++ : 0xFF);
        BindGroupMapping[BIND_GROUP_ID_DYNAMIC]        = (TotalDynamicBindings != 0 ? Idx++ : 0xFF);
        VERIFY_EXPR(Idx <= MAX_BIND_GROUPS);
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

    std::array<std::vector<WGPUBindGroupLayoutEntry>, BIND_GROUP_ID_NUM_GROUPS> wgpuBGLayoutEntries;

    DynamicLinearAllocator TempAllocator{GetRawAllocator(), 256};

    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const PipelineResourceDesc& ResDesc   = m_Desc.Resources[i];
        const BindGroupEntryType    EntryType = GetBindGroupEntryType(ResDesc);
        // NB: GroupId is always 0 for static/mutable variables, and 1 - for dynamic ones.
        //     It is not the actual bind group index in the group layout!
        const BIND_GROUP_ID GroupId    = VarTypeToBindGroupId(ResDesc.VarType);
        const CACHE_GROUP   CacheGroup = GetResourceCacheGroup(ResDesc);

        VERIFY(i == 0 || ResDesc.VarType >= m_Desc.Resources[i - 1].VarType, "Resources must be sorted by variable type");

        // If all resources are dynamic, then the signature contains only one bind group layout with index 0,
        // so remap GroupId to the actual bind group index.
        VERIFY_EXPR(BindGroupMapping[GroupId] < MAX_BIND_GROUPS);

        // The sampler may not be yet initialized, but this is OK as all resources are initialized
        // in the same order as in m_Desc.Resources
        //const auto AssignedSamplerInd = DescrType == DescriptorType::SeparateImage ?
        //    FindAssignedSampler(ResDesc, ResourceAttribs::InvalidSamplerInd) :
        //    ResourceAttribs::InvalidSamplerInd;
        const auto AssignedSamplerInd = FindAssignedSampler(ResDesc, ResourceAttribs::InvalidSamplerInd);

        WGPUSampler* pwgpuImmutableSamplers = nullptr;
        if ( //EntryType == BindGroupEntryType::CombinedImageSampler ||
            EntryType == BindGroupEntryType::Sampler)
        {
            // Only search for immutable sampler for combined image samplers and separate samplers.
            // Note that for DescriptorType::SeparateImage with immutable sampler, we will initialize
            // a separate immutable sampler below. It will not be assigned to the image variable.
            const auto SrcImmutableSamplerInd = FindImmutableSamplerWGPU(ResDesc, EntryType, m_Desc, GetCombinedSamplerSuffix());
            if (SrcImmutableSamplerInd != InvalidImmutableSamplerIndex)
            {
                auto& ImmutableSampler = m_ImmutableSamplers[SrcImmutableSamplerInd];
                if (!ImmutableSampler)
                {
                    const auto& ImmutableSamplerDesc = m_Desc.ImmutableSamplers[SrcImmutableSamplerInd].Desc;
                    // The same immutable sampler may be used by different resources in different shader stages.
                    ImmutableSampler.Init(HasDevice() ? GetDevice() : nullptr, ImmutableSamplerDesc);
                }
                pwgpuImmutableSamplers = TempAllocator.ConstructArray<WGPUSampler>(ResDesc.ArraySize, ImmutableSampler.GetWGPUSampler());
            }
        }

        auto* const pAttribs = m_pResourceAttribs + i;
        if (!IsSerialized)
        {
            new (pAttribs) ResourceAttribs{
                BindingIndices[CacheGroup],
                AssignedSamplerInd,
                ResDesc.ArraySize,
                EntryType,
                BindGroupMapping[GroupId],
                pwgpuImmutableSamplers != nullptr,
                CacheGroupOffsets[CacheGroup],
                ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC ? StaticCacheOffset : ~0u,
            };
        }
        else
        {
            DEV_CHECK_ERR(pAttribs->BindingIndex == BindingIndices[CacheGroup],
                          "Deserialized binding index (", pAttribs->BindingIndex, ") is invalid: ", BindingIndices[CacheGroup], " is expected.");
            DEV_CHECK_ERR(pAttribs->SamplerInd == AssignedSamplerInd,
                          "Deserialized sampler index (", pAttribs->SamplerInd, ") is invalid: ", AssignedSamplerInd, " is expected.");
            DEV_CHECK_ERR(pAttribs->ArraySize == ResDesc.ArraySize,
                          "Deserialized array size (", pAttribs->ArraySize, ") is invalid: ", ResDesc.ArraySize, " is expected.");
            DEV_CHECK_ERR(pAttribs->GetBindGroupEntryType() == EntryType, "Deserialized descriptor type in invalid");
            DEV_CHECK_ERR(pAttribs->BindGroup == BindGroupMapping[GroupId],
                          "Deserialized descriptor set (", pAttribs->BindGroup, ") is invalid: ", BindGroupMapping[GroupId], " is expected.");
            DEV_CHECK_ERR(pAttribs->IsImmutableSamplerAssigned() == (pwgpuImmutableSamplers != nullptr), "Immutable sampler flag is invalid");
            DEV_CHECK_ERR(pAttribs->SRBCacheOffset == CacheGroupOffsets[CacheGroup],
                          "SRB cache offset (", pAttribs->SRBCacheOffset, ") is invalid: ", CacheGroupOffsets[CacheGroup], " is expected.");
            DEV_CHECK_ERR(pAttribs->StaticCacheOffset == (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC ? StaticCacheOffset : ~0u),
                          "Static cache offset is invalid.");
        }

        BindingIndices[CacheGroup] += 1;
        CacheGroupOffsets[CacheGroup] += ResDesc.ArraySize;

        for (Uint32 elem = 0; elem < ResDesc.ArraySize; ++elem)
        {
            WGPUBindGroupLayoutEntry wgpuBGLayoutEntry = GetWGPUBindGroupLayoutEntry(*pAttribs, ResDesc, elem);
            wgpuBGLayoutEntries[GroupId].push_back(wgpuBGLayoutEntry);
        }

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
        {
            VERIFY(pAttribs->BindGroup == 0, "Static resources must always be allocated in bind group 0");
            m_pStaticResCache->InitializeResources(pAttribs->BindGroup, StaticCacheOffset, ResDesc.ArraySize,
                                                   pAttribs->GetBindGroupEntryType(), pAttribs->IsImmutableSamplerAssigned());
            StaticCacheOffset += ResDesc.ArraySize;
        }
    }

#ifdef DILIGENT_DEBUG
    if (m_pStaticResCache != nullptr)
    {
        m_pStaticResCache->DbgVerifyResourceInitialization();
    }
#endif

#if 0
    m_DynamicUniformBufferCount = static_cast<Uint16>(CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR]);
    m_DynamicStorageBufferCount = static_cast<Uint16>(CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR]);
    VERIFY_EXPR(m_DynamicUniformBufferCount == CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR]);
    VERIFY_EXPR(m_DynamicStorageBufferCount == CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR]);
#endif

    VERIFY_EXPR(m_pStaticResCache == nullptr || const_cast<const ShaderResourceCacheWebGPU*>(m_pStaticResCache)->GetBindGroup(0).GetSize() == StaticCacheOffset);
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

#if 0
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
    // be retrieved by PSO with PipelineLayoutWebGPU::GetImmutableSamplerInfo() when the PSO initializes
    // 'g_Texture_sampler'.
    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        auto& ImmutableSampler = m_ImmutableSamplers[i];
        if (ImmutableSampler)
        {
            // Immutable sampler has already been initialized as resource
            continue;
        }

        const auto& SamplerDesc = m_Desc.ImmutableSamplers[i];
        // If static/mutable descriptor set layout is empty, then add samplers to dynamic set.
        const auto SetId = (BindGroupMapping[BIND_GROUP_ID_STATIC_MUTABLE] < MAX_DESCRIPTOR_SETS ? BIND_GROUP_ID_STATIC_MUTABLE : BIND_GROUP_ID_DYNAMIC);
        DEV_CHECK_ERR(BindGroupMapping[SetId] < MAX_DESCRIPTOR_SETS,
                      "There are no descriptor sets in this signature, which indicates there are no other "
                      "resources besides immutable samplers. This is not currently allowed.");

        ImmutableSampler.Init(HasDevice() ? GetDevice() : nullptr, SamplerDesc.Desc);

        auto& BindingIndex = BindingIndices[SetId * 3 + CACHE_GROUP_OTHER];
        if (!IsSerialized)
        {
            ImmutableSampler.DescrSet     = BindGroupMapping[SetId];
            ImmutableSampler.BindingIndex = BindingIndex;
        }
        else
        {
            DEV_CHECK_ERR(ImmutableSampler.DescrSet == BindGroupMapping[SetId],
                          "Immutable sampler descriptor set (", ImmutableSampler.DescrSet, ") is invalid: ", BindGroupMapping[SetId], " is expected.");
            DEV_CHECK_ERR(ImmutableSampler.BindingIndex == BindingIndex,
                          "Immutable sampler bind index (", ImmutableSampler.BindingIndex, ") is invalid: ", BindingIndex, " is expected.");
        }
        ++BindingIndex;

        WebGPUDescriptorSetLayoutBinding WebGPUSetLayoutBinding{};
        WebGPUSetLayoutBinding.binding            = ImmutableSampler.BindingIndex;
        WebGPUSetLayoutBinding.descriptorCount    = 1;
        WebGPUSetLayoutBinding.stageFlags         = ShaderTypesToWebGPUShaderStageFlags(SamplerDesc.ShaderStages);
        WebGPUSetLayoutBinding.descriptorType     = WebGPU_DESCRIPTOR_TYPE_SAMPLER;
        WebGPUSetLayoutBinding.pImmutableSamplers = TempAllocator.Construct<WebGPUSampler>(ImmutableSampler.GetWebGPUSampler());
        WebGPUSetLayoutBindings[SetId].push_back(WebGPUSetLayoutBinding);
    }
#endif

    Uint32 NumGroups = 0;
    if (BindGroupMapping[BIND_GROUP_ID_STATIC_MUTABLE] < MAX_BIND_GROUPS)
    {
        m_BindGroupSizes[BindGroupMapping[BIND_GROUP_ID_STATIC_MUTABLE]] =
            CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] +
            CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR] +
            CacheGroupSizes[CACHE_GROUP_OTHER_STAT_VAR];
        ++NumGroups;
    }

    if (BindGroupMapping[BIND_GROUP_ID_DYNAMIC] < MAX_BIND_GROUPS)
    {
        m_BindGroupSizes[BindGroupMapping[BIND_GROUP_ID_DYNAMIC]] =
            CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR] +
            CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR] +
            CacheGroupSizes[CACHE_GROUP_OTHER_DYN_VAR];
        ++NumGroups;
    }
#ifdef DILIGENT_DEBUG
    for (Uint32 i = 0; i < NumGroups; ++i)
        VERIFY_EXPR(m_BindGroupSizes[i] != ~0U && m_BindGroupSizes[i] > 0);
#else
    (void)NumGroups;
#endif

    if (HasDevice())
    {
        WGPUDevice wgpuDevice = GetDevice()->GetWebGPUDevice();

        for (size_t i = 0; i < wgpuBGLayoutEntries.size(); ++i)
        {
            const auto& wgpuEntries = wgpuBGLayoutEntries[i];
            if (wgpuEntries.empty())
                continue;

            const std::string Label = std::string{m_Desc.Name} + " - bind group " + std::to_string(i);

            WGPUBindGroupLayoutDescriptor BGLayoutDesc{};
            BGLayoutDesc.label      = Label.c_str();
            BGLayoutDesc.entryCount = wgpuEntries.size();
            BGLayoutDesc.entries    = wgpuEntries.data();

            m_wgpuBindGroupLayouts[i].Reset(wgpuDeviceCreateBindGroupLayout(wgpuDevice, &BGLayoutDesc));
            VERIFY_EXPR(m_wgpuBindGroupLayouts[i]);
        }
        VERIFY_EXPR(NumGroups == GetNumBindGroups());
    }
}

PipelineResourceSignatureWebGPUImpl::~PipelineResourceSignatureWebGPUImpl()
{
    Destruct();
}

void PipelineResourceSignatureWebGPUImpl::Destruct()
{
    //for (auto& Layout : m_WebGPUDescrSetLayouts)
    //{
    //    if (Layout)
    //        GetDevice()->SafeReleaseDeviceObject(std::move(Layout), ~0ull);
    //}

    if (m_ImmutableSamplers != nullptr)
    {
        for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
        {
            m_ImmutableSamplers[i].~ImmutableSamplerAttribs();
        }
        m_ImmutableSamplers = nullptr;
    }

    TPipelineResourceSignatureBase::Destruct();
}

void PipelineResourceSignatureWebGPUImpl::InitSRBResourceCache(ShaderResourceCacheWebGPU& ResourceCache)
{
    const Uint32 NumGroups = GetNumBindGroups();
#ifdef DILIGENT_DEBUG
    for (Uint32 i = 0; i < NumGroups; ++i)
        VERIFY_EXPR(m_BindGroupSizes[i] != ~0U);
#endif

    auto& CacheMemAllocator = m_SRBMemAllocator.GetResourceCacheDataAllocator(0);
    ResourceCache.InitializeGroups(CacheMemAllocator, NumGroups, m_BindGroupSizes.data());

    const Uint32 TotalResources = GetTotalResourceCount();
    const auto   CacheType      = ResourceCache.GetContentType();
    for (Uint32 r = 0; r < TotalResources; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& Attr    = GetResourceAttribs(r);
        ResourceCache.InitializeResources(Attr.BindGroup, Attr.CacheOffset(CacheType), ResDesc.ArraySize,
                                          Attr.GetBindGroupEntryType(), Attr.IsImmutableSamplerAssigned());
    }

#ifdef DILIGENT_DEBUG
    ResourceCache.DbgVerifyResourceInitialization();
#endif

    //    if (auto WebGPULayout = GetWGPUBindGroupLayout(BIND_GROUP_ID_STATIC_MUTABLE))
    //    {
    //        const char* DescrSetName = "Static/Mutable Descriptor Set";
    //#ifdef DILIGENT_DEVELOPMENT
    //        std::string _DescrSetName{m_Desc.Name};
    //        _DescrSetName.append(" - static/mutable set");
    //        DescrSetName = _DescrSetName.c_str();
    //#endif
    //        DescriptorSetAllocation SetAllocation = GetDevice()->AllocateDescriptorSet(~Uint64{0}, WebGPULayout, DescrSetName);
    //        ResourceCache.AssignDescriptorSetAllocation(GetDescriptorSetIndex<BIND_GROUP_ID_STATIC_MUTABLE>(), std::move(SetAllocation));
    //    }
}

void PipelineResourceSignatureWebGPUImpl::CopyStaticResources(ShaderResourceCacheWebGPU& DstResourceCache) const
{
    if (!HasBindGroup(BIND_GROUP_ID_STATIC_MUTABLE) || m_pStaticResCache == nullptr)
        return;

    // SrcResourceCache contains only static resources.
    // In case of SRB, DstResourceCache contains static, mutable and dynamic resources.
    // In case of Signature, DstResourceCache contains only static resources.
    const auto& SrcResourceCache = *m_pStaticResCache;
    const auto  StaticGroupIdx   = GetBindGroupIndex<BIND_GROUP_ID_STATIC_MUTABLE>();
    const auto& SrcBindGroup     = SrcResourceCache.GetBindGroup(StaticGroupIdx);
    const auto& DstBindGroup     = const_cast<const ShaderResourceCacheWebGPU&>(DstResourceCache).GetBindGroup(StaticGroupIdx);
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
            const auto     SrcCacheOffset = Attr.CacheOffset(SrcCacheType) + ArrInd;
            const auto&    SrcCachedRes   = SrcBindGroup.GetResource(SrcCacheOffset);
            IDeviceObject* pObject        = SrcCachedRes.pObject;
            if (pObject == nullptr)
            {
                if (DstCacheType == ResourceCacheContentType::SRB)
                    LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");
                continue;
            }

            const auto  DstCacheOffset = Attr.CacheOffset(DstCacheType) + ArrInd;
            const auto& DstCachedRes   = DstBindGroup.GetResource(DstCacheOffset);
            VERIFY_EXPR(SrcCachedRes.Type == DstCachedRes.Type);

            const IDeviceObject* pCachedResource = DstCachedRes.pObject;
            if (pCachedResource != pObject)
            {
                DEV_CHECK_ERR(pCachedResource == nullptr, "Static resource has already been initialized, and the new resource does not match previously assigned resource");
                DstResourceCache.SetResource(StaticGroupIdx,
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

#ifdef DILIGENT_DEBUG
    //DstResourceCache.DbgVerifyDynamicBuffersCounter();
#endif
}

template <>
Uint32 PipelineResourceSignatureWebGPUImpl::GetBindGroupIndex<PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_STATIC_MUTABLE>() const
{
    VERIFY(HasBindGroup(BIND_GROUP_ID_STATIC_MUTABLE), "This signature does not have static/mutable descriptor set");
    return 0;
}

template <>
Uint32 PipelineResourceSignatureWebGPUImpl::GetBindGroupIndex<PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_DYNAMIC>() const
{
    VERIFY(HasBindGroup(BIND_GROUP_ID_DYNAMIC), "This signature does not have dynamic descriptor set");
    return HasBindGroup(BIND_GROUP_ID_STATIC_MUTABLE) ? 1 : 0;
}

#if 0

void PipelineResourceSignatureWebGPUImpl::CommitDynamicResources(const ShaderResourceCacheWebGPU& ResourceCache,
                                                                 WebGPUDescriptorSet              WebGPUDynamicDescriptorSet) const
{
    VERIFY(HasDescriptorSet(BIND_GROUP_ID_DYNAMIC), "This signature does not contain dynamic resources");
    VERIFY_EXPR(WebGPUDynamicDescriptorSet != WebGPU_NULL_HANDLE);
    VERIFY_EXPR(ResourceCache.GetContentType() == ResourceCacheContentType::SRB);

#    ifdef DILIGENT_DEBUG
    static constexpr size_t ImgUpdateBatchSize          = 4;
    static constexpr size_t BuffUpdateBatchSize         = 2;
    static constexpr size_t TexelBuffUpdateBatchSize    = 2;
    static constexpr size_t AccelStructBatchSize        = 2;
    static constexpr size_t WriteDescriptorSetBatchSize = 2;
#    else
    static constexpr size_t ImgUpdateBatchSize          = 64;
    static constexpr size_t BuffUpdateBatchSize         = 32;
    static constexpr size_t TexelBuffUpdateBatchSize    = 16;
    static constexpr size_t AccelStructBatchSize        = 16;
    static constexpr size_t WriteDescriptorSetBatchSize = 32;
#    endif

    // Do not zero-initialize arrays!
    std::array<WebGPUDescriptorImageInfo, ImgUpdateBatchSize>                          DescrImgInfoArr;
    std::array<WebGPUDescriptorBufferInfo, BuffUpdateBatchSize>                        DescrBuffInfoArr;
    std::array<WebGPUBufferView, TexelBuffUpdateBatchSize>                             DescrBuffViewArr;
    std::array<WebGPUWriteDescriptorSetAccelerationStructureKHR, AccelStructBatchSize> DescrAccelStructArr;
    std::array<WebGPUWriteDescriptorSet, WriteDescriptorSetBatchSize>                  WriteDescrSetArr;

    auto DescrImgIt      = DescrImgInfoArr.begin();
    auto DescrBuffIt     = DescrBuffInfoArr.begin();
    auto BuffViewIt      = DescrBuffViewArr.begin();
    auto AccelStructIt   = DescrAccelStructArr.begin();
    auto WriteDescrSetIt = WriteDescrSetArr.begin();

    const auto  DynamicSetIdx  = GetDescriptorSetIndex<BIND_GROUP_ID_DYNAMIC>();
    const auto& SetResources   = ResourceCache.GetDescriptorSet(DynamicSetIdx);
    const auto& LogicalDevice  = GetDevice()->GetLogicalDevice();
    const auto  DynResIdxRange = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

    constexpr auto CacheType = ResourceCacheContentType::SRB;

    for (Uint32 ResIdx = DynResIdxRange.first, ArrElem = 0; ResIdx < DynResIdxRange.second;)
    {
        const auto& Attr        = GetResourceAttribs(ResIdx);
        const auto  CacheOffset = Attr.CacheOffset(CacheType);
        const auto  ArraySize   = Attr.ArraySize;
        const auto  DescrType   = Attr.GetDescriptorType();

#    ifdef DILIGENT_DEBUG
        {
            const auto& Res = GetResourceDesc(ResIdx);
            VERIFY_EXPR(ArraySize == GetResourceDesc(ResIdx).ArraySize);
            VERIFY_EXPR(Res.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
        }
#    endif

        WriteDescrSetIt->sType = WebGPU_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        WriteDescrSetIt->pNext = nullptr;
        VERIFY(SetResources.GetWebGPUDescriptorSet() == WebGPU_NULL_HANDLE, "Dynamic descriptor set must not be assigned to the resource cache");
        WriteDescrSetIt->dstSet = WebGPUDynamicDescriptorSet;
        VERIFY(WriteDescrSetIt->dstSet != WebGPU_NULL_HANDLE, "Vulkan descriptor set must not be null");
        WriteDescrSetIt->dstBinding      = Attr.BindingIndex;
        WriteDescrSetIt->dstArrayElement = ArrElem;
        // descriptorType must be the same type as that specified in WebGPUDescriptorSetLayoutBinding for dstSet at dstBinding.
        // The type of the descriptor also controls which array the descriptors are taken from. (13.2.4)
        WriteDescrSetIt->descriptorType  = DescriptorTypeToWebGPUDescriptorType(DescrType);
        WriteDescrSetIt->descriptorCount = 0;

        auto WriteArrayElements = [&](auto DescrType, auto& DescrIt, const auto& DescrArr) //
        {
            while (ArrElem < ArraySize && DescrIt != DescrArr.end())
            {
                if (const auto& CachedRes = SetResources.GetResource(CacheOffset + (ArrElem++)))
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
                        break; // We need to use a new WebGPUWriteDescriptorSet since we skipped an array element
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


#    ifdef DILIGENT_DEVELOPMENT
bool PipelineResourceSignatureWebGPUImpl::DvpValidateCommittedResource(const DeviceContextWebGPUImpl*    pDeviceCtx,
                                                                       const SPIRVShaderResourceAttribs& SPIRVAttribs,
                                                                       Uint32                            ResIndex,
                                                                       const ShaderResourceCacheWebGPU&  ResourceCache,
                                                                       const char*                       ShaderName,
                                                                       const char*                       PSOName) const
{
    VERIFY_EXPR(ResIndex < m_Desc.NumResources);
    const auto& ResDesc    = m_Desc.Resources[ResIndex];
    const auto& ResAttribs = m_pResourceAttribs[ResIndex];
    VERIFY(strcmp(ResDesc.Name, SPIRVAttribs.Name) == 0, "Inconsistent resource names");

    if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER && ResAttribs.IsImmutableSamplerAssigned())
        return true; // Skip immutable separate samplers

    const auto& DescrSetResources = ResourceCache.GetDescriptorSet(ResAttribs.DescrSet);
    const auto  CacheType         = ResourceCache.GetContentType();
    const auto  CacheOffset       = ResAttribs.CacheOffset(CacheType);

    VERIFY_EXPR(SPIRVAttribs.ArraySize <= ResAttribs.ArraySize);

    bool BindingsOK = true;
    for (Uint32 ArrIndex = 0; ArrIndex < SPIRVAttribs.ArraySize; ++ArrIndex)
    {
        const auto& Res = DescrSetResources.GetResource(CacheOffset + ArrIndex);
        if (Res.IsNull())
        {
            LOG_ERROR_MESSAGE("No resource is bound to variable '", GetShaderResourcePrintName(SPIRVAttribs, ArrIndex),
                              "' in shader '", ShaderName, "' of PSO '", PSOName, "'");
            BindingsOK = false;
            continue;
        }

        if (ResAttribs.IsCombinedWithSampler())
        {
            const auto& SamplerResDesc = GetResourceDesc(ResAttribs.SamplerInd);
            const auto& SamplerAttribs = GetResourceAttribs(ResAttribs.SamplerInd);
            VERIFY_EXPR(SamplerResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);
            VERIFY_EXPR(SamplerResDesc.ArraySize == 1 || SamplerResDesc.ArraySize == ResDesc.ArraySize);
            if (!SamplerAttribs.IsImmutableSamplerAssigned())
            {
                if (ArrIndex < SamplerResDesc.ArraySize)
                {
                    const auto& SamDescrSetResources = ResourceCache.GetDescriptorSet(SamplerAttribs.DescrSet);
                    const auto  SamCacheOffset       = SamplerAttribs.CacheOffset(CacheType);
                    const auto& Sam                  = SamDescrSetResources.GetResource(SamCacheOffset + ArrIndex);
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
                if (const auto* pBufferWebGPU = Res.pObject.RawPtr<BufferWebGPUImpl>())
                {
                    pBufferWebGPU->DvpVerifyDynamicAllocation(pDeviceCtx);

                    if ((pBufferWebGPU->GetDesc().Size < SPIRVAttribs.BufferStaticSize) &&
                        (GetDevice()->GetValidationFlags() & VALIDATION_FLAG_CHECK_SHADER_BUFFER_SIZE) != 0)
                    {
                        // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
                        LOG_WARNING_MESSAGE("The size of uniform buffer '",
                                            pBufferWebGPU->GetDesc().Name, "' bound to shader variable '",
                                            GetShaderResourcePrintName(SPIRVAttribs, ArrIndex), "' is ", pBufferWebGPU->GetDesc().Size,
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
                if (auto* pBufferViewWebGPU = Res.pObject.RawPtr<BufferViewWebGPUImpl>())
                {
                    const auto* pBufferWebGPU = ClassPtrCast<BufferWebGPUImpl>(pBufferViewWebGPU->GetBuffer());
                    const auto& ViewDesc      = pBufferViewWebGPU->GetDesc();
                    const auto& BuffDesc      = pBufferWebGPU->GetDesc();

                    pBufferWebGPU->DvpVerifyDynamicAllocation(pDeviceCtx);

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
                if (const auto* pTexViewWebGPU = Res.pObject.RawPtr<TextureViewWebGPUImpl>())
                {
                    if (!ValidateResourceViewDimension(SPIRVAttribs.Name, SPIRVAttribs.ArraySize, ArrIndex, pTexViewWebGPU, SPIRVAttribs.GetResourceDimension(), SPIRVAttribs.IsMultisample()))
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
#    endif
#endif

PipelineResourceSignatureWebGPUImpl::PipelineResourceSignatureWebGPUImpl(IReferenceCounters*                                pRefCounters,
                                                                         RenderDeviceWebGPUImpl*                            pDevice,
                                                                         const PipelineResourceSignatureDesc&               Desc,
                                                                         const PipelineResourceSignatureInternalDataWebGPU& InternalData) :
    TPipelineResourceSignatureBase{pRefCounters, pDevice, Desc, InternalData}
//m_DynamicUniformBufferCount{Serialized.DynamicUniformBufferCount}
//m_DynamicStorageBufferCount{Serialized.DynamicStorageBufferCount}
{
    try
    {
        Deserialize(
            GetRawAllocator(), Desc, InternalData, m_ImmutableSamplers,
            [this]() //
            {
                CreateBindGroupLayouts(/*IsSerialized*/ true);
                //VERIFY_EXPR(m_DynamicUniformBufferCount == Serialized.DynamicUniformBufferCount);
                //VERIFY_EXPR(m_DynamicStorageBufferCount == Serialized.DynamicStorageBufferCount);
            },
            [this]() //
            {
                return ShaderResourceCacheWebGPU::GetRequiredMemorySize(GetNumBindGroups(), m_BindGroupSizes.data());
            });
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

#if 0
PipelineResourceSignatureInternalDataWebGPU PipelineResourceSignatureWebGPUImpl::GetInternalData() const
{
    PipelineResourceSignatureInternalDataWebGPU InternalData;

    TPipelineResourceSignatureBase::GetInternalData(InternalData);

    const auto NumImmutableSamplers = GetDesc().NumImmutableSamplers;
    if (NumImmutableSamplers > 0)
    {
        VERIFY_EXPR(m_ImmutableSamplers != nullptr);
        InternalData.m_pImmutableSamplers = std::make_unique<PipelineResourceImmutableSamplerAttribsWebGPU[]>(NumImmutableSamplers);

        for (Uint32 i = 0; i < NumImmutableSamplers; ++i)
            InternalData.m_pImmutableSamplers[i] = m_ImmutableSamplers[i];
    }

    InternalData.pResourceAttribs          = m_pResourceAttribs;
    InternalData.NumResources              = GetDesc().NumResources;
    InternalData.pImmutableSamplers        = InternalData.m_pImmutableSamplers.get();
    InternalData.NumImmutableSamplers      = NumImmutableSamplers;
    InternalData.DynamicStorageBufferCount = m_DynamicStorageBufferCount;
    InternalData.DynamicUniformBufferCount = m_DynamicUniformBufferCount;

    return InternalData;
}

#endif

} // namespace Diligent
