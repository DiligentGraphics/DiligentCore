/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#pragma once

/// \file
/// Declaration of Diligent::PipelineResourceSignatureWebGPUImpl class

#include "EngineWebGPUImplTraits.hpp"
#include "PipelineResourceSignatureBase.hpp"

// ShaderResourceCacheWebGPU, ShaderVariableManagerWebGPU, and ShaderResourceBindingWebGPUImpl
// are required by PipelineResourceSignatureBase
#include "ShaderResourceCacheWebGPU.hpp"
#include "ShaderVariableManagerWebGPU.hpp"
#include "ShaderResourceBindingWebGPUImpl.hpp"

#include "PipelineResourceAttribsWebGPU.hpp"
#include "WebGPUObjectWrappers.hpp"
#include "SamplerWebGPUImpl.hpp"

namespace Diligent
{

struct WGSLShaderResourceAttribs;

struct PipelineResourceImmutableSamplerAttribsWebGPU
{
public:
    Uint16 BindGroup    = 0xFFFFu;
    Uint16 BindingIndex = 0;

    Uint32 ArraySize         = 1;
    Uint32 SRBCacheOffset    = 0; // Offset in the SRB resource cache
    Uint32 StaticCacheOffset = 0; // Offset in the static resource cache

    // Index of the sampler resource in m_Desc.Resources, e.g.:
    //
    //      PipelineResourceDesc Resources[] = {{SHADER_TYPE_PIXEL, "g_Sampler", SHADER_RESOURCE_TYPE_SAMPLER, ...}, ... }
    //      ImmutableSamplerDesc ImtblSams[] = {{SHADER_TYPE_PIXEL, "g_Sampler", ...}, ... }
    //
    Uint32 SamplerInd = PipelineResourceAttribsWebGPU::InvalidSamplerInd;

    PipelineResourceImmutableSamplerAttribsWebGPU() noexcept {}

    bool IsAllocated() const { return BindGroup != 0xFFFFu; }
};
ASSERT_SIZEOF(PipelineResourceImmutableSamplerAttribsWebGPU, 20, "The struct is used in serialization and must be tightly packed");

struct PipelineResourceSignatureInternalDataWebGPU : PipelineResourceSignatureInternalData
{
    const PipelineResourceAttribsWebGPU*                 pResourceAttribs     = nullptr; // [NumResources]
    Uint32                                               NumResources         = 0;
    const PipelineResourceImmutableSamplerAttribsWebGPU* pImmutableSamplers   = nullptr; // [NumImmutableSamplers]
    Uint32                                               NumImmutableSamplers = 0;

    PipelineResourceSignatureInternalDataWebGPU() noexcept = default;

    explicit PipelineResourceSignatureInternalDataWebGPU(const PipelineResourceSignatureInternalData& InternalData) noexcept :
        PipelineResourceSignatureInternalData{InternalData}
    {}
};

/// Implementation of the Diligent::PipelineResourceSignatureWebGPUImpl class
class PipelineResourceSignatureWebGPUImpl final : public PipelineResourceSignatureBase<EngineWebGPUImplTraits>
{
public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<EngineWebGPUImplTraits>;

    using ResourceAttribs = TPipelineResourceSignatureBase::PipelineResourceAttribsType;

    // Bind group identifier (this is not the bind group set index in the layout!)
    enum BIND_GROUP_ID : size_t
    {
        // Static/mutable variables bind group id
        BIND_GROUP_ID_STATIC_MUTABLE = 0,

        // Dynamic variables bind group id
        BIND_GROUP_ID_DYNAMIC,

        BIND_GROUP_ID_NUM_GROUPS
    };

    // Static/mutable and dynamic bind groups
    static constexpr Uint32 MAX_BIND_GROUPS = BIND_GROUP_ID_NUM_GROUPS;

    static_assert(ResourceAttribs::MaxBindGroups >= MAX_BIND_GROUPS, "Not enough bits to store bind group index");

    PipelineResourceSignatureWebGPUImpl(IReferenceCounters*                  pRefCounters,
                                        RenderDeviceWebGPUImpl*              pDevice,
                                        const PipelineResourceSignatureDesc& Desc,
                                        SHADER_TYPE                          ShaderStages      = SHADER_TYPE_UNKNOWN,
                                        bool                                 bIsDeviceInternal = false);

    PipelineResourceSignatureWebGPUImpl(IReferenceCounters*                                pRefCounters,
                                        RenderDeviceWebGPUImpl*                            pDevice,
                                        const PipelineResourceSignatureDesc&               Desc,
                                        const PipelineResourceSignatureInternalDataWebGPU& InternalData);

    ~PipelineResourceSignatureWebGPUImpl();

    Uint32 GetNumBindGroups() const
    {
        static_assert(BIND_GROUP_ID_NUM_GROUPS == 2, "Please update this method with new bind group id");
        return (HasBindGroup(BIND_GROUP_ID_STATIC_MUTABLE) ? 1 : 0) + (HasBindGroup(BIND_GROUP_ID_DYNAMIC) ? 1 : 0);
    }

    using ImmutableSamplerAttribs = PipelineResourceImmutableSamplerAttribsWebGPU;

    WGPUBindGroupLayout GetWGPUBindGroupLayout(BIND_GROUP_ID GroupId) const { return m_wgpuBindGroupLayouts[GroupId]; }

    bool   HasBindGroup(BIND_GROUP_ID GroupId) const { return m_wgpuBindGroupLayouts[GroupId]; }
    Uint32 GetBindGroupSize(BIND_GROUP_ID GroupId) const { return m_BindGroupSizes[GroupId]; }

    void InitSRBResourceCache(ShaderResourceCacheWebGPU& ResourceCache);

    void CopyStaticResources(ShaderResourceCacheWebGPU& ResourceCache) const;
    // Make the base class method visible
    using TPipelineResourceSignatureBase::CopyStaticResources;

    // Returns the bind group index in the resource cache
    template <BIND_GROUP_ID GroupId>
    Uint32 GetBindGroupIndex() const;

    const ImmutableSamplerAttribs& GetImmutableSamplerAttribs(Uint32 SampIndex) const
    {
        VERIFY_EXPR(SampIndex < m_Desc.NumImmutableSamplers);
        return m_ImmutableSamplers[SampIndex];
    }

#ifdef DILIGENT_DEVELOPMENT
    /// Verifies committed resource using the WGSL resource attributes from the PSO.
    bool DvpValidateCommittedResource(const WGSLShaderResourceAttribs& WGSLAttribs,
                                      Uint32                           ResIndex,
                                      const ShaderResourceCacheWebGPU& ResourceCache,
                                      const char*                      ShaderName,
                                      const char*                      PSOName) const;
#endif

private:
    void UpdateStaticResStages(const PipelineResourceSignatureDesc& Desc);
    void CreateBindGroupLayouts(bool IsSerialized);
    void Destruct();

    // Resource cache group identifier
    enum CACHE_GROUP : size_t
    {
        CACHE_GROUP_DYN_UB = 0,         // Uniform buffer with dynamic offset
        CACHE_GROUP_DYN_SB,             // Storage buffer with dynamic offset
        CACHE_GROUP_OTHER,              // Other resource type
        CACHE_GROUP_COUNT_PER_VAR_TYPE, // Cache group count per shader variable type

        CACHE_GROUP_DYN_UB_STAT_VAR = CACHE_GROUP_DYN_UB, // Uniform buffer with dynamic offset, static variable
        CACHE_GROUP_DYN_SB_STAT_VAR = CACHE_GROUP_DYN_SB, // Storage buffer with dynamic offset, static variable
        CACHE_GROUP_OTHER_STAT_VAR  = CACHE_GROUP_OTHER,  // Other resource type, static variable

        CACHE_GROUP_DYN_UB_DYN_VAR, // Uniform buffer with dynamic offset, dynamic variable
        CACHE_GROUP_DYN_SB_DYN_VAR, // Storage buffer with dynamic offset, dynamic variable
        CACHE_GROUP_OTHER_DYN_VAR,  // Other resource type, dynamic variable

        CACHE_GROUP_COUNT
    };
    static_assert(CACHE_GROUP_COUNT == CACHE_GROUP_COUNT_PER_VAR_TYPE * MAX_BIND_GROUPS, "Inconsistent cache group count");

    using CacheOffsetsType = std::array<Uint32, CACHE_GROUP_COUNT>; // [dynamic uniform buffers, dynamic storage buffers, other] x [bind group] including ArraySize
    using BindingCountType = std::array<Uint32, CACHE_GROUP_COUNT>; // [dynamic uniform buffers, dynamic storage buffers, other] x [bind group] not counting ArraySize

    static inline CACHE_GROUP   GetResourceCacheGroup(const PipelineResourceDesc& Res);
    static inline BIND_GROUP_ID VarTypeToBindGroupId(SHADER_RESOURCE_VARIABLE_TYPE VarType);

private:
    std::array<WebGPUBindGroupLayoutWrapper, BIND_GROUP_ID_NUM_GROUPS> m_wgpuBindGroupLayouts;

    // Bind group sizes indexed by the group index in the layout (not BIND_GROUP_ID!)
    std::array<Uint32, MAX_BIND_GROUPS> m_BindGroupSizes = {~0U, ~0U};

    // The total number of uniform buffers with dynamic offsets in both bind groups,
    // accounting for array size.
    Uint16 m_DynamicUniformBufferCount = 0;
    // The total number storage buffers with dynamic offsets in both bind groups,
    // accounting for array size.
    Uint16 m_DynamicStorageBufferCount = 0;

    ImmutableSamplerAttribs* m_ImmutableSamplers = nullptr; // [m_Desc.NumImmutableSamplers]
};

template <> Uint32 PipelineResourceSignatureWebGPUImpl::GetBindGroupIndex<PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_STATIC_MUTABLE>() const;
template <> Uint32 PipelineResourceSignatureWebGPUImpl::GetBindGroupIndex<PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_DYNAMIC>() const;

} // namespace Diligent
