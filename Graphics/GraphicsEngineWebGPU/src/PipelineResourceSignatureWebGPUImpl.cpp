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
#include "WebGPUTypeConversions.hpp"
#include "BufferWebGPUImpl.hpp"
#include "BufferViewWebGPUImpl.hpp"
#include "TextureViewWebGPUImpl.hpp"

namespace Diligent
{

namespace
{

constexpr SHADER_TYPE WEB_GPU_SUPPORTED_SHADER_STAGES =
    SHADER_TYPE_VERTEX |
    SHADER_TYPE_PIXEL |
    SHADER_TYPE_COMPUTE;


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

WGPUSamplerBindingType GetWGPUSamplerBindingType(BindGroupEntryType EntryType, WEB_GPU_BINDING_TYPE BindingType)
{
    if (EntryType == BindGroupEntryType::Sampler)
    {
        switch (BindingType)
        {
            case WEB_GPU_BINDING_TYPE_DEFAULT:
            case WEB_GPU_BINDING_TYPE_FILTERING_SAMPLER:
                return WGPUSamplerBindingType_Filtering;

            case WEB_GPU_BINDING_TYPE_NON_FILTERING_SAMPLER:
                return WGPUSamplerBindingType_NonFiltering;

            case WEB_GPU_BINDING_TYPE_COMPARISON_SAMPLER:
                return WGPUSamplerBindingType_Comparison;

            default:
                UNEXPECTED("Invalid sampler binding type");
                return WGPUSamplerBindingType_Filtering;
        }
    }
    else
    {
        return WGPUSamplerBindingType_Undefined;
    }
}

WGPUSamplerBindingType SamplerDescToWGPUSamplerBindingType(const SamplerDesc& Desc)
{
    if (IsComparisonFilter(Desc.MinFilter))
    {
        VERIFY(IsComparisonFilter(Desc.MagFilter), "Inconsistent min/mag filters");
        return WGPUSamplerBindingType_Comparison;
    }
    else
    {
        VERIFY(!IsComparisonFilter(Desc.MagFilter), "Inconsistent min/mag filters");
        return WGPUSamplerBindingType_Filtering;
    }
}

WGPUTextureSampleType GetWGPUTextureSampleType(BindGroupEntryType EntryType, WEB_GPU_BINDING_TYPE BindingType)
{
    if (EntryType == BindGroupEntryType::Texture)
    {
        switch (BindingType)
        {
            case WEB_GPU_BINDING_TYPE_DEFAULT:
            case WEB_GPU_BINDING_TYPE_FLOAT_TEXTURE:
            case WEB_GPU_BINDING_TYPE_FLOAT_TEXTURE_MS:
                return WGPUTextureSampleType_Float;

            case WEB_GPU_BINDING_TYPE_UNFILTERABLE_FLOAT_TEXTURE:
            case WEB_GPU_BINDING_TYPE_UNFILTERABLE_FLOAT_TEXTURE_MS:
                return WGPUTextureSampleType_UnfilterableFloat;

            case WEB_GPU_BINDING_TYPE_DEPTH_TEXTURE:
            case WEB_GPU_BINDING_TYPE_DEPTH_TEXTURE_MS:
                return WGPUTextureSampleType_Depth;

            case WEB_GPU_BINDING_TYPE_SINT_TEXTURE:
            case WEB_GPU_BINDING_TYPE_SINT_TEXTURE_MS:
                return WGPUTextureSampleType_Sint;

            case WEB_GPU_BINDING_TYPE_UINT_TEXTURE:
            case WEB_GPU_BINDING_TYPE_UINT_TEXTURE_MS:
                return WGPUTextureSampleType_Uint;

            default:
                UNEXPECTED("Invalid texture binding type");
                return WGPUTextureSampleType_Float;
        }
    }
    else
    {
        return WGPUTextureSampleType_Undefined;
    }
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

bool IsMSTextureGPUBinding(WEB_GPU_BINDING_TYPE BindingType)
{
    return (BindingType == WEB_GPU_BINDING_TYPE_FLOAT_TEXTURE_MS ||
            BindingType == WEB_GPU_BINDING_TYPE_UNFILTERABLE_FLOAT_TEXTURE_MS ||
            BindingType == WEB_GPU_BINDING_TYPE_DEPTH_TEXTURE_MS ||
            BindingType == WEB_GPU_BINDING_TYPE_SINT_TEXTURE_MS ||
            BindingType == WEB_GPU_BINDING_TYPE_UINT_TEXTURE_MS);
}

WGPUBindGroupLayoutEntry GetWGPUBindGroupLayoutEntry(const PipelineResourceAttribsWebGPU& Attribs,
                                                     const PipelineResourceDesc&          ResDesc,
                                                     Uint32                               ArrayElement)
{
    WGPUBindGroupLayoutEntry wgpuBGLayoutEntry{};

    wgpuBGLayoutEntry.binding    = Attribs.BindingIndex + ArrayElement;
    wgpuBGLayoutEntry.visibility = ShaderStagesToWGPUShaderStageFlags(ResDesc.ShaderStages & WEB_GPU_SUPPORTED_SHADER_STAGES);

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
    else if (WGPUSamplerBindingType wgpuSamplerBindingType = GetWGPUSamplerBindingType(EntryType, ResDesc.WebGPUAttribs.BindingType))
    {
        wgpuBGLayoutEntry.sampler.type = wgpuSamplerBindingType;
    }
    else if (WGPUTextureSampleType wgpuTextureSampleType = GetWGPUTextureSampleType(EntryType, ResDesc.WebGPUAttribs.BindingType))
    {
        wgpuBGLayoutEntry.texture.sampleType = wgpuTextureSampleType;

        wgpuBGLayoutEntry.texture.viewDimension = ResDesc.WebGPUAttribs.TextureViewDim != RESOURCE_DIM_UNDEFINED ?
            ResourceDimensionToWGPUTextureViewDimension(ResDesc.WebGPUAttribs.TextureViewDim) :
            WGPUTextureViewDimension_2D;

        wgpuBGLayoutEntry.texture.multisampled = IsMSTextureGPUBinding(ResDesc.WebGPUAttribs.BindingType);
    }
    else if (WGPUStorageTextureAccess wgpuStorageTextureAccess = GetWGPUStorageTextureAccess(EntryType))
    {
        wgpuBGLayoutEntry.storageTexture.access = wgpuStorageTextureAccess;

        wgpuBGLayoutEntry.storageTexture.format = ResDesc.WebGPUAttribs.UAVTextureFormat != TEX_FORMAT_UNKNOWN ?
            TextureFormatToWGPUFormat(ResDesc.WebGPUAttribs.UAVTextureFormat) :
            WGPUTextureFormat_RGBA32Float;

        wgpuBGLayoutEntry.storageTexture.viewDimension = ResDesc.WebGPUAttribs.TextureViewDim != RESOURCE_DIM_UNDEFINED ?
            ResourceDimensionToWGPUTextureViewDimension(ResDesc.WebGPUAttribs.TextureViewDim) :
            WGPUTextureViewDimension_2D;
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
    // NB: GroupId is always 0 for static/mutable variables, and 1 - for dynamic ones.
    //     It is not the actual bind group index in the group layout!
    const size_t GroupId           = VarTypeToBindGroupId(Res.VarType);
    const bool   WithDynamicOffset = (Res.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0;
    const bool   UseTexelBuffer    = (Res.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0;

    if (WithDynamicOffset && !UseTexelBuffer)
    {
        if (Res.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER)
            return static_cast<CACHE_GROUP>(GroupId * CACHE_GROUP_COUNT_PER_VAR_TYPE + CACHE_GROUP_DYN_UB);

        if (Res.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV || Res.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV)
            return static_cast<CACHE_GROUP>(GroupId * CACHE_GROUP_COUNT_PER_VAR_TYPE + CACHE_GROUP_DYN_SB);
    }
    return static_cast<CACHE_GROUP>(GroupId * CACHE_GROUP_COUNT_PER_VAR_TYPE + CACHE_GROUP_OTHER);
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
        UpdateStaticResStages(Desc);
        Initialize(
            GetRawAllocator(), DecoupleCombinedSamplers(Desc), m_ImmutableSamplers,
            [this]() //
            {
                CreateBindGroupLayouts(/*IsSerialized*/ false);
            },
            [this]() //
            {
                return ShaderResourceCacheWebGPU::GetRequiredMemorySize(GetNumBindGroups(), m_BindGroupSizes.data(), m_DynamicOffsetCounts.data());
            });
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

void PipelineResourceSignatureWebGPUImpl::UpdateStaticResStages(const PipelineResourceSignatureDesc& Desc)
{
    // Immutable samplers are allocated in the static resource cache.
    // Make sure that the cache is initialized even if there are no samplers in m_Desc.Resources.
    for (Uint32 i = 0; i < Desc.NumImmutableSamplers; ++i)
    {
        m_StaticResShaderStages |= Desc.ImmutableSamplers[i].ShaderStages;
    }
}

void PipelineResourceSignatureWebGPUImpl::CreateBindGroupLayouts(const bool IsSerialized)
{
    // Binding count in each cache group.
    BindingCountType BindingCount = {};
    // Required cache size for each cache group
    CacheOffsetsType CacheGroupSizes = {};

    // NB: since WebGPU does not support resource arrays, binding count is the same as the cache group size.

    // The total number of static resources in all stages accounting for array sizes.
    Uint32 StaticResourceCount = 0;

    // Index of the immutable sampler for every sampler in m_Desc.Resources, or InvalidImmutableSamplerIndex.
    std::vector<Uint32> ResourceToImmutableSamplerInd(m_Desc.NumResources, InvalidImmutableSamplerIndex);
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const PipelineResourceDesc& ResDesc    = m_Desc.Resources[i];
        const CACHE_GROUP           CacheGroup = GetResourceCacheGroup(ResDesc);

        BindingCount[CacheGroup] += ResDesc.ArraySize;
        CacheGroupSizes[CacheGroup] += ResDesc.ArraySize;

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            StaticResourceCount += ResDesc.ArraySize;

        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER)
        {
            // We only need to search for immutable samplers for SHADER_RESOURCE_TYPE_SAMPLER.
            // For SHADER_RESOURCE_TYPE_TEXTURE_SRV, we will look for the assigned sampler and check if it is immutable.

            // Note that FindImmutableSampler() below will work properly both when combined texture samplers are used and when not.
            const Uint32 SrcImmutableSamplerInd = FindImmutableSampler(ResDesc.ShaderStages, ResDesc.Name);
            if (SrcImmutableSamplerInd != InvalidImmutableSamplerIndex)
            {
                ResourceToImmutableSamplerInd[i] = SrcImmutableSamplerInd;
                // Set the immutable sampler array size to match the resource array size
                ImmutableSamplerAttribs& DstImtblSampAttribs = m_ImmutableSamplers[SrcImmutableSamplerInd];
                // One immutable sampler may be used by different arrays in different shader stages - use the maximum array size
                DstImtblSampAttribs.ArraySize  = std::max(DstImtblSampAttribs.ArraySize, ResDesc.ArraySize);
                DstImtblSampAttribs.SamplerInd = i;
            }
        }
    }

    // Reserve space for immutable samplers not defined as resources, e.g.:
    //
    //      PipelineResourceDesc Resources[] = {{SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_TYPE_TEXTURE_SRV, ...}, ... }
    //      ImmutableSamplerDesc ImtblSams[] = {{SHADER_TYPE_PIXEL, "g_Texture", ...}, ... }
    //
    static constexpr CACHE_GROUP ImmutableSamplerCacheGroup = CACHE_GROUP_OTHER_STAT_VAR;
    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        const ImmutableSamplerAttribs& ImtblSampAttribs = m_ImmutableSamplers[i];
        if (ImtblSampAttribs.SamplerInd == ResourceAttribs::InvalidSamplerInd)
        {
            BindingCount[ImmutableSamplerCacheGroup] += ImtblSampAttribs.ArraySize;
            CacheGroupSizes[ImmutableSamplerCacheGroup] += ImtblSampAttribs.ArraySize;
            // Immutable samplers are stored in the static resource cache
            StaticResourceCount += ImtblSampAttribs.ArraySize;
        }
    }

    // Initialize static resource cache
    if (StaticResourceCount != 0)
    {
        VERIFY_EXPR(m_pStaticResCache != nullptr);
        const Uint32 DynamicOffsetCount = BindingCount[CACHE_GROUP_DYN_UB_STAT_VAR] + BindingCount[CACHE_GROUP_DYN_SB_STAT_VAR];
        m_pStaticResCache->InitializeGroups(GetRawAllocator(), 1, &StaticResourceCount, &DynamicOffsetCount);
    }

    // Bind group mapping (static/mutable (0) or dynamic (1) -> bind group index)
    std::array<Uint32, BIND_GROUP_ID_NUM_GROUPS> BindGroupMapping = {};
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

        BindGroupMapping[BIND_GROUP_ID_STATIC_MUTABLE] = (TotalStaticBindings != 0 ? Idx++ : 0xFF);
        BindGroupMapping[BIND_GROUP_ID_DYNAMIC]        = (TotalDynamicBindings != 0 ? Idx++ : 0xFF);
        VERIFY_EXPR(Idx <= MAX_BIND_GROUPS);
    }

    // Resource bindings as well as cache offsets are ordered by CACHE_GROUP in each bind group:
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
            CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR] + CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR],
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
            BindingCount[CACHE_GROUP_DYN_UB_DYN_VAR] + BindingCount[CACHE_GROUP_DYN_SB_DYN_VAR],
        };

    // Current offset in the static resource cache
    Uint32 StaticCacheOffset = 0;

    std::array<std::vector<WGPUBindGroupLayoutEntry>, BIND_GROUP_ID_NUM_GROUPS> wgpuBGLayoutEntries;
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const PipelineResourceDesc& ResDesc = m_Desc.Resources[i];
        VERIFY(i == 0 || ResDesc.VarType >= m_Desc.Resources[i - 1].VarType, "Resources must be sorted by variable type");

        const BindGroupEntryType EntryType = GetBindGroupEntryType(ResDesc);

        // NB: GroupId is always 0 for static/mutable variables, and 1 - for dynamic ones.
        //     It is not the actual bind group index in the group layout!
        const BIND_GROUP_ID GroupId = VarTypeToBindGroupId(ResDesc.VarType);
        // If all resources are dynamic, then the signature contains only one bind group layout with index 0,
        // so remap GroupId to the actual bind group index.
        const Uint32 BindGroupIndex = BindGroupMapping[GroupId];
        VERIFY_EXPR(BindGroupIndex < MAX_BIND_GROUPS);
        const CACHE_GROUP CacheGroup = GetResourceCacheGroup(ResDesc);

        Uint32 AssignedSamplerInd     = ResourceAttribs::InvalidSamplerInd;
        Uint32 SrcImmutableSamplerInd = ResourceToImmutableSamplerInd[i];
        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV)
        {
            VERIFY_EXPR(SrcImmutableSamplerInd == InvalidImmutableSamplerIndex);
            AssignedSamplerInd = FindAssignedSampler(ResDesc, ResourceAttribs::InvalidSamplerInd);
            if (AssignedSamplerInd != ResourceAttribs::InvalidSamplerInd)
            {
                SrcImmutableSamplerInd = ResourceToImmutableSamplerInd[AssignedSamplerInd];
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
                BindGroupIndex,
                SrcImmutableSamplerInd != InvalidImmutableSamplerIndex,
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
            DEV_CHECK_ERR(pAttribs->GetBindGroupEntryType() == EntryType, "Deserialized bind group is invalid");
            DEV_CHECK_ERR(pAttribs->BindGroup == BindGroupMapping[GroupId],
                          "Deserialized bind group (", pAttribs->BindGroup, ") is invalid: ", BindGroupMapping[GroupId], " is expected.");
            DEV_CHECK_ERR(pAttribs->IsImmutableSamplerAssigned() == (SrcImmutableSamplerInd != InvalidImmutableSamplerIndex), "Immutable sampler flag is invalid");
            DEV_CHECK_ERR(pAttribs->SRBCacheOffset == CacheGroupOffsets[CacheGroup],
                          "SRB cache offset (", pAttribs->SRBCacheOffset, ") is invalid: ", CacheGroupOffsets[CacheGroup], " is expected.");
            DEV_CHECK_ERR(pAttribs->StaticCacheOffset == (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC ? StaticCacheOffset : ~0u),
                          "Static cache offset is invalid.");
        }

        BindingIndices[CacheGroup] += ResDesc.ArraySize;
        CacheGroupOffsets[CacheGroup] += ResDesc.ArraySize;

        for (Uint32 elem = 0; elem < ResDesc.ArraySize; ++elem)
        {
            WGPUBindGroupLayoutEntry wgpuBGLayoutEntry = GetWGPUBindGroupLayoutEntry(*pAttribs, ResDesc, elem);
            wgpuBGLayoutEntries[BindGroupIndex].push_back(wgpuBGLayoutEntry);
        }

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
        {
            VERIFY(pAttribs->BindGroup == 0, "Static resources must always be allocated in bind group 0");
            m_pStaticResCache->InitializeResources(pAttribs->BindGroup, StaticCacheOffset, ResDesc.ArraySize,
                                                   pAttribs->GetBindGroupEntryType(), pAttribs->IsImmutableSamplerAssigned());
            StaticCacheOffset += ResDesc.ArraySize;
        }
    }

    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        const ImmutableSamplerDesc& ImtblSamp        = GetImmutableSamplerDesc(i);
        ImmutableSamplerAttribs&    ImtblSampAttribs = m_ImmutableSamplers[i];
        RefCntAutoPtr<ISampler>     pSampler;
        if (HasDevice())
            GetDevice()->CreateSampler(ImtblSamp.Desc, &pSampler);

        if (ImtblSampAttribs.SamplerInd == ResourceAttribs::InvalidSamplerInd)
        {
            // There is no corresponding resource for this immutable sampler, so we need
            // to new bind group layout entries.
            ImtblSampAttribs.BindGroup         = static_cast<Uint16>(BindGroupMapping[BIND_GROUP_ID_STATIC_MUTABLE]);
            ImtblSampAttribs.BindingIndex      = static_cast<Uint16>(BindingIndices[ImmutableSamplerCacheGroup]);
            ImtblSampAttribs.SRBCacheOffset    = CacheGroupOffsets[ImmutableSamplerCacheGroup];
            ImtblSampAttribs.StaticCacheOffset = StaticCacheOffset;
            BindingIndices[ImmutableSamplerCacheGroup] += ImtblSampAttribs.ArraySize;
            CacheGroupOffsets[ImmutableSamplerCacheGroup] += ImtblSampAttribs.ArraySize;
            StaticCacheOffset += ImtblSampAttribs.ArraySize;

            for (Uint32 elem = 0; elem < ImtblSampAttribs.ArraySize; ++elem)
            {
                WGPUBindGroupLayoutEntry wgpuBGLayoutEntry{};
                wgpuBGLayoutEntry.binding      = ImtblSampAttribs.BindingIndex + elem;
                wgpuBGLayoutEntry.visibility   = ShaderStagesToWGPUShaderStageFlags(ImtblSamp.ShaderStages & WEB_GPU_SUPPORTED_SHADER_STAGES);
                wgpuBGLayoutEntry.sampler.type = SamplerDescToWGPUSamplerBindingType(ImtblSamp.Desc);
                wgpuBGLayoutEntries[ImtblSampAttribs.BindGroup].push_back(wgpuBGLayoutEntry);
            }
        }
        else
        {
            // There is a corresponding resource for this immutable sampler. Use its
            // bind group and binding index.
            ResourceAttribs& SamplerAttribs = m_pResourceAttribs[ImtblSampAttribs.SamplerInd];
            VERIFY_EXPR(SamplerAttribs.GetBindGroupEntryType() == BindGroupEntryType::Sampler);
            ImtblSampAttribs.BindGroup      = static_cast<Uint16>(SamplerAttribs.BindGroup);
            ImtblSampAttribs.BindingIndex   = static_cast<Uint16>(SamplerAttribs.BindingIndex);
            ImtblSampAttribs.SRBCacheOffset = SamplerAttribs.SRBCacheOffset;
        }

        // Initialize immutable samplers in the static resource cache
        m_pStaticResCache->InitializeResources(ImtblSampAttribs.BindGroup, ImtblSampAttribs.StaticCacheOffset, ImtblSampAttribs.ArraySize,
                                               BindGroupEntryType::Sampler, /*HasImmutableSampler = */ true);
        if (pSampler)
        {
            for (Uint32 elem = 0; elem < ImtblSampAttribs.ArraySize; ++elem)
            {
                m_pStaticResCache->SetResource(ImtblSampAttribs.BindGroup, ImtblSampAttribs.StaticCacheOffset + elem, pSampler);
            }
        }
    }

    VERIFY_EXPR(StaticCacheOffset == StaticResourceCount);

#ifdef DILIGENT_DEBUG
    if (m_pStaticResCache != nullptr)
    {
        m_pStaticResCache->DbgVerifyResourceInitialization();
    }
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

    Uint32 NumGroups = 0;
    if (BindGroupMapping[BIND_GROUP_ID_STATIC_MUTABLE] < MAX_BIND_GROUPS)
    {
        const Uint32 BindGroupIndex = BindGroupMapping[BIND_GROUP_ID_STATIC_MUTABLE];
        m_BindGroupSizes[BindGroupIndex] =
            CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] +
            CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR] +
            CacheGroupSizes[CACHE_GROUP_OTHER_STAT_VAR];
        m_DynamicOffsetCounts[BindGroupIndex] =
            CacheGroupSizes[CACHE_GROUP_DYN_UB_STAT_VAR] +
            CacheGroupSizes[CACHE_GROUP_DYN_SB_STAT_VAR];
        ++NumGroups;
    }

    if (BindGroupMapping[BIND_GROUP_ID_DYNAMIC] < MAX_BIND_GROUPS)
    {
        const Uint32 BindGroupIndex = BindGroupMapping[BIND_GROUP_ID_DYNAMIC];
        m_BindGroupSizes[BindGroupIndex] =
            CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR] +
            CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR] +
            CacheGroupSizes[CACHE_GROUP_OTHER_DYN_VAR];
        m_DynamicOffsetCounts[BindGroupIndex] =
            CacheGroupSizes[CACHE_GROUP_DYN_UB_DYN_VAR] +
            CacheGroupSizes[CACHE_GROUP_DYN_SB_DYN_VAR];
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
    ResourceCache.InitializeGroups(CacheMemAllocator, NumGroups, m_BindGroupSizes.data(), m_DynamicOffsetCounts.data());

    const Uint32                   TotalResources = GetTotalResourceCount();
    const ResourceCacheContentType CacheType      = ResourceCache.GetContentType();
    for (Uint32 r = 0; r < TotalResources; ++r)
    {
        const PipelineResourceDesc& ResDesc = GetResourceDesc(r);
        const ResourceAttribs&      Attr    = GetResourceAttribs(r);
        ResourceCache.InitializeResources(Attr.BindGroup, Attr.CacheOffset(CacheType), ResDesc.ArraySize,
                                          Attr.GetBindGroupEntryType(), Attr.IsImmutableSamplerAssigned());
    }

    const ShaderResourceCacheWebGPU& StaticResCache = *m_pStaticResCache;
    // Initialize immutable samplers
    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        const PipelineResourceImmutableSamplerAttribsWebGPU& ImtblSampAttr = m_ImmutableSamplers[i];
        VERIFY_EXPR(ImtblSampAttr.IsAllocated());
        VERIFY_EXPR(ImtblSampAttr.ArraySize > 0);
        // Initialize immutable samplers that are not defined as resources
        if (ImtblSampAttr.SamplerInd == ResourceAttribs::InvalidSamplerInd)
        {
            ResourceCache.InitializeResources(ImtblSampAttr.BindGroup, ImtblSampAttr.SRBCacheOffset, ImtblSampAttr.ArraySize,
                                              BindGroupEntryType::Sampler, /*HasImmutableSampler = */ true);
        }

        const ShaderResourceCacheWebGPU::BindGroup& SrcBindGroup = StaticResCache.GetBindGroup(0);
        for (Uint32 elem = 0; elem < ImtblSampAttr.ArraySize; ++elem)
        {
            const ShaderResourceCacheWebGPU::Resource& SrcSamplerRes = SrcBindGroup.GetResource(ImtblSampAttr.StaticCacheOffset + elem);
            if (SrcSamplerRes.pObject)
            {
                ResourceCache.SetResource(ImtblSampAttr.BindGroup, ImtblSampAttr.SRBCacheOffset + elem, SrcSamplerRes.pObject);
            }
        }
    }

#ifdef DILIGENT_DEBUG
    ResourceCache.DbgVerifyResourceInitialization();
#endif
}

void PipelineResourceSignatureWebGPUImpl::CopyStaticResources(ShaderResourceCacheWebGPU& DstResourceCache) const
{
    if (!HasBindGroup(BIND_GROUP_ID_STATIC_MUTABLE) || m_pStaticResCache == nullptr)
        return;

    // SrcResourceCache contains only static resources.
    // In case of SRB, DstResourceCache contains static, mutable and dynamic resources.
    // In case of Signature, DstResourceCache contains only static resources.
    const ShaderResourceCacheWebGPU&            SrcResourceCache = *m_pStaticResCache;
    const Uint32                                StaticGroupIdx   = GetBindGroupIndex<BIND_GROUP_ID_STATIC_MUTABLE>();
    const ShaderResourceCacheWebGPU::BindGroup& SrcBindGroup     = SrcResourceCache.GetBindGroup(StaticGroupIdx);
    const ShaderResourceCacheWebGPU::BindGroup& DstBindGroup     = const_cast<const ShaderResourceCacheWebGPU&>(DstResourceCache).GetBindGroup(StaticGroupIdx);
    const std::pair<Uint32, Uint32>             ResIdxRange      = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    const ResourceCacheContentType              SrcCacheType     = SrcResourceCache.GetContentType();
    const ResourceCacheContentType              DstCacheType     = DstResourceCache.GetContentType();

    for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
    {
        const PipelineResourceDesc& ResDesc = GetResourceDesc(r);
        const ResourceAttribs&      Attr    = GetResourceAttribs(r);
        VERIFY_EXPR(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER && Attr.IsImmutableSamplerAssigned())
        {
            // Skip immutable samplers as they are initialized in InitSRBResourceCache()
            continue;
        }

        for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
        {
            const Uint32                               SrcCacheOffset = Attr.CacheOffset(SrcCacheType) + ArrInd;
            const ShaderResourceCacheWebGPU::Resource& SrcCachedRes   = SrcBindGroup.GetResource(SrcCacheOffset);
            IDeviceObject*                             pObject        = SrcCachedRes.pObject;
            if (pObject == nullptr)
            {
                if (DstCacheType == ResourceCacheContentType::SRB)
                    LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");
                continue;
            }

            const Uint32                               DstCacheOffset = Attr.CacheOffset(DstCacheType) + ArrInd;
            const ShaderResourceCacheWebGPU::Resource& DstCachedRes   = DstBindGroup.GetResource(DstCacheOffset);
            VERIFY_EXPR(SrcCachedRes.Type == DstCachedRes.Type);

            const IDeviceObject* pCachedResource = DstCachedRes.pObject;
            if (pCachedResource != pObject)
            {
                DEV_CHECK_ERR(pCachedResource == nullptr, "Static resource has already been initialized, and the new resource does not match previously assigned resource");
                DstResourceCache.SetResource(StaticGroupIdx,
                                             DstCacheOffset + ArrInd,
                                             SrcCachedRes.pObject,
                                             SrcCachedRes.BufferBaseOffset,
                                             SrcCachedRes.BufferRangeSize);
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
        const auto  DescrType   = Attr.GetBindGroupEntryType();

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
        // BindGroupEntryType must be the same type as that specified in WebGPUDescriptorSetLayoutBinding for dstSet at dstBinding.
        // The type of the descriptor also controls which array the descriptors are taken from. (13.2.4)
        WriteDescrSetIt->BindGroupEntryType  = BindGroupEntryTypeToWebGPUBindGroupEntryType(DescrType);
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
        static_assert(static_cast<Uint32>(BindGroupEntryType::Count) == 16, "Please update the switch below to handle the new descriptor type");
        switch (DescrType)
        {
            case BindGroupEntryType::UniformBuffer:
            case BindGroupEntryType::UniformBufferDynamic:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                WriteArrayElements(std::integral_constant<BindGroupEntryType, BindGroupEntryType::UniformBuffer>{}, DescrBuffIt, DescrBuffInfoArr);
                break;

            case BindGroupEntryType::StorageBuffer:
            case BindGroupEntryType::StorageBufferDynamic:
            case BindGroupEntryType::StorageBuffer_ReadOnly:
            case BindGroupEntryType::StorageBufferDynamic_ReadOnly:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                WriteArrayElements(std::integral_constant<BindGroupEntryType, BindGroupEntryType::StorageBuffer>{}, DescrBuffIt, DescrBuffInfoArr);
                break;

            case BindGroupEntryType::UniformTexelBuffer:
            case BindGroupEntryType::StorageTexelBuffer:
            case BindGroupEntryType::StorageTexelBuffer_ReadOnly:
                WriteDescrSetIt->pTexelBufferView = &(*BuffViewIt);
                WriteArrayElements(std::integral_constant<BindGroupEntryType, BindGroupEntryType::UniformTexelBuffer>{}, BuffViewIt, DescrBuffViewArr);
                break;

            case BindGroupEntryType::CombinedImageSampler:
            case BindGroupEntryType::SeparateImage:
            case BindGroupEntryType::StorageImage:
                WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                WriteArrayElements(std::integral_constant<BindGroupEntryType, BindGroupEntryType::SeparateImage>{}, DescrImgIt, DescrImgInfoArr);
                break;

            case BindGroupEntryType::InputAttachment:
            case BindGroupEntryType::InputAttachment_General:
                WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                WriteArrayElements(std::integral_constant<BindGroupEntryType, BindGroupEntryType::InputAttachment>{}, DescrImgIt, DescrImgInfoArr);
                break;

            case BindGroupEntryType::Sampler:
                // Immutable samplers are permanently bound into the set layout; later binding a sampler
                // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
                if (!Attr.IsImmutableSamplerAssigned())
                {
                    WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                    WriteArrayElements(std::integral_constant<BindGroupEntryType, BindGroupEntryType::Sampler>{}, DescrImgIt, DescrImgInfoArr);
                }
                else
                {
                    // Go to the next resource
                    ArrElem                          = ArraySize;
                    WriteDescrSetIt->dstArrayElement = ArraySize;
                }
                break;

            case BindGroupEntryType::AccelerationStructure:
                WriteDescrSetIt->pNext = &(*AccelStructIt);
                WriteArrayElements(std::integral_constant<BindGroupEntryType, BindGroupEntryType::AccelerationStructure>{}, AccelStructIt, DescrAccelStructArr);
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
        UpdateStaticResStages(Desc);
        Deserialize(
            GetRawAllocator(), DecoupleCombinedSamplers(Desc), InternalData, m_ImmutableSamplers,
            [this]() //
            {
                CreateBindGroupLayouts(/*IsSerialized*/ true);
                //VERIFY_EXPR(m_DynamicUniformBufferCount == Serialized.DynamicUniformBufferCount);
                //VERIFY_EXPR(m_DynamicStorageBufferCount == Serialized.DynamicStorageBufferCount);
            },
            [this]() //
            {
                return ShaderResourceCacheWebGPU::GetRequiredMemorySize(GetNumBindGroups(), m_BindGroupSizes.data(), m_DynamicOffsetCounts.data());
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


#ifdef DILIGENT_DEVELOPMENT
bool PipelineResourceSignatureWebGPUImpl::DvpValidateCommittedResource(const WGSLShaderResourceAttribs& WGSLAttribs,
                                                                       Uint32                           ResIndex,
                                                                       const ShaderResourceCacheWebGPU& ResourceCache,
                                                                       const char*                      ShaderName,
                                                                       const char*                      PSOName) const
{
    VERIFY_EXPR(ResIndex < m_Desc.NumResources);
    const PipelineResourceDesc& ResDesc    = m_Desc.Resources[ResIndex];
    const ResourceAttribs&      ResAttribs = m_pResourceAttribs[ResIndex];
    VERIFY(strcmp(ResDesc.Name, WGSLAttribs.Name) == 0, "Inconsistent resource names");

    const ShaderResourceCacheWebGPU::BindGroup& BindGroupResources = ResourceCache.GetBindGroup(ResAttribs.BindGroup);
    const ResourceCacheContentType              CacheType          = ResourceCache.GetContentType();
    const Uint32                                CacheOffset        = ResAttribs.CacheOffset(CacheType);

    VERIFY_EXPR(WGSLAttribs.ArraySize <= ResAttribs.ArraySize);

    bool BindingsOK = true;
    for (Uint32 ArrIndex = 0; ArrIndex < WGSLAttribs.ArraySize; ++ArrIndex)
    {
        const ShaderResourceCacheWebGPU::Resource& Res = BindGroupResources.GetResource(CacheOffset + ArrIndex);
        if (!Res)
        {
            LOG_ERROR_MESSAGE("No resource is bound to variable '", GetShaderResourcePrintName(WGSLAttribs, ArrIndex),
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
                    const ShaderResourceCacheWebGPU::BindGroup& SamBindGroupResources = ResourceCache.GetBindGroup(SamplerAttribs.BindGroup);
                    const Uint32                                SamCacheOffset        = SamplerAttribs.CacheOffset(CacheType);
                    const ShaderResourceCacheWebGPU::Resource&  Sam                   = SamBindGroupResources.GetResource(SamCacheOffset + ArrIndex);
                    if (!Sam)
                    {
                        LOG_ERROR_MESSAGE("No sampler is bound to sampler variable '", GetShaderResourcePrintName(SamplerResDesc, ArrIndex),
                                          "' combined with texture '", WGSLAttribs.Name, "' in shader '", ShaderName, "' of PSO '", PSOName, "'.");
                        BindingsOK = false;
                    }
                }
            }
        }

        static_assert(static_cast<size_t>(BindGroupEntryType::Count) == 12, "Please update the switch below to handle the new bind group entry type");
        switch (ResAttribs.GetBindGroupEntryType())
        {
            case BindGroupEntryType::UniformBuffer:
            case BindGroupEntryType::UniformBufferDynamic:
                VERIFY_EXPR(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER);
                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (const BufferWebGPUImpl* pBufferWebGPU = Res.pObject.RawPtr<BufferWebGPUImpl>())
                {
                    //pBufferWebGPU->DvpVerifyDynamicAllocation(pDeviceCtx);

                    if ((WGSLAttribs.BufferStaticSize != 0) &&
                        (GetDevice()->GetValidationFlags() & VALIDATION_FLAG_CHECK_SHADER_BUFFER_SIZE) != 0 &&
                        (pBufferWebGPU->GetDesc().Size < WGSLAttribs.BufferStaticSize))
                    {
                        // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
                        LOG_WARNING_MESSAGE("The size of uniform buffer '",
                                            pBufferWebGPU->GetDesc().Name, "' bound to shader variable '",
                                            GetShaderResourcePrintName(WGSLAttribs, ArrIndex), "' is ", pBufferWebGPU->GetDesc().Size,
                                            " bytes, but the shader expects at least ", WGSLAttribs.BufferStaticSize,
                                            " bytes.");
                    }
                }
                break;

            case BindGroupEntryType::StorageBuffer:
            case BindGroupEntryType::StorageBuffer_ReadOnly:
            case BindGroupEntryType::StorageBufferDynamic:
            case BindGroupEntryType::StorageBufferDynamic_ReadOnly:
                VERIFY_EXPR(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV || ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV);
                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (const BufferViewWebGPUImpl* pBufferViewWebGPU = Res.pObject.RawPtr<BufferViewWebGPUImpl>())
                {
                    const BufferWebGPUImpl* pBufferWebGPU = ClassPtrCast<BufferWebGPUImpl>(pBufferViewWebGPU->GetBuffer());
                    const BufferViewDesc&   ViewDesc      = pBufferViewWebGPU->GetDesc();
                    const BufferDesc&       BuffDesc      = pBufferWebGPU->GetDesc();

                    //pBufferWebGPU->DvpVerifyDynamicAllocation(pDeviceCtx);

                    if (BuffDesc.ElementByteStride == 0)
                    {
                        if ((ViewDesc.ByteWidth < WGSLAttribs.BufferStaticSize) &&
                            (GetDevice()->GetValidationFlags() & VALIDATION_FLAG_CHECK_SHADER_BUFFER_SIZE) != 0)
                        {
                            // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
                            LOG_WARNING_MESSAGE("The size of buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' bound to shader variable '",
                                                GetShaderResourcePrintName(WGSLAttribs, ArrIndex), "' is ", ViewDesc.ByteWidth, " bytes, but the shader expects at least ",
                                                WGSLAttribs.BufferStaticSize, " bytes.");
                        }
                    }
                    else
                    {
                        if ((ViewDesc.ByteWidth < WGSLAttribs.BufferStaticSize || (ViewDesc.ByteWidth - WGSLAttribs.BufferStaticSize) % BuffDesc.ElementByteStride != 0) &&
                            (GetDevice()->GetValidationFlags() & VALIDATION_FLAG_CHECK_SHADER_BUFFER_SIZE) != 0)
                        {
                            // For buffers with dynamic arrays we know only static part size and array element stride.
                            // Element stride in the shader may be differ than in the code. Here we check that the buffer size is exactly the same as the array with N elements.
                            LOG_WARNING_MESSAGE("The size (", ViewDesc.ByteWidth, ") and stride (", BuffDesc.ElementByteStride, ") of buffer view '",
                                                ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' bound to shader variable '",
                                                GetShaderResourcePrintName(WGSLAttribs, ArrIndex), "' are incompatible with what the shader expects. This may be the result of the array element size mismatch.");
                        }
                    }
                }
                break;

            case BindGroupEntryType::Texture:
            case BindGroupEntryType::StorageTexture_WriteOnly:
            case BindGroupEntryType::StorageTexture_ReadOnly:
            case BindGroupEntryType::StorageTexture_ReadWrite:
                VERIFY_EXPR(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV || ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV);
                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (const TextureViewWebGPUImpl* pTexViewWebGPU = Res.pObject.RawPtr<TextureViewWebGPUImpl>())
                {
                    if (!ValidateResourceViewDimension(WGSLAttribs.Name, WGSLAttribs.ArraySize, ArrIndex, pTexViewWebGPU, WGSLAttribs.GetResourceDimension(), WGSLAttribs.IsMultisample()))
                        BindingsOK = false;
                }
                break;

            case BindGroupEntryType::Sampler:
                // Nothing else to check
                break;

            case BindGroupEntryType::ExternalTexture:
                break;

            default:
                break;
                // Nothing to do
        }
    }

    return BindingsOK;
}

bool PipelineResourceSignatureWebGPUImpl::DvpValidateImmutableSampler(const WGSLShaderResourceAttribs& WGSLAttribs,
                                                                      Uint32                           ImtblSamIndex,
                                                                      const ShaderResourceCacheWebGPU& ResourceCache,
                                                                      const char*                      ShaderName,
                                                                      const char*                      PSOName) const
{
    VERIFY_EXPR(ImtblSamIndex < m_Desc.NumImmutableSamplers);
    const ImmutableSamplerAttribs& ImtblSamAttribs = m_ImmutableSamplers[ImtblSamIndex];

    const ShaderResourceCacheWebGPU::BindGroup& BindGroupResources = ResourceCache.GetBindGroup(ImtblSamAttribs.BindGroup);
    VERIFY_EXPR(WGSLAttribs.ArraySize <= ImtblSamAttribs.ArraySize);

    bool BindingsOK = true;
    for (Uint32 ArrIndex = 0; ArrIndex < WGSLAttribs.ArraySize; ++ArrIndex)
    {
        const ShaderResourceCacheWebGPU::Resource& Res = BindGroupResources.GetResource(ImtblSamAttribs.SRBCacheOffset + ArrIndex);
        DEV_CHECK_ERR(Res.HasImmutableSampler, "Resource must have immutable sampler assigned");
        if (!Res)
        {
            DEV_ERROR("Immutable sampler is not initialized for variable '", GetShaderResourcePrintName(WGSLAttribs, ArrIndex),
                      "' in shader '", ShaderName, "' of PSO '", PSOName, "'. This might be a bug as immutable samplers should be initialized by InitSRBResourceCache.");
            BindingsOK = false;
        }
    }
    return BindingsOK;
}
#endif

} // namespace Diligent
