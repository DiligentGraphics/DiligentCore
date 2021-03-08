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

#include "PipelineResourceSignatureD3D12Impl.hpp"

#include <unordered_map>

#include "RenderDeviceD3D12Impl.hpp"
#include "BufferD3D12Impl.hpp"
#include "BufferViewD3D12Impl.hpp"
#include "SamplerD3D12Impl.hpp"
#include "TextureD3D12Impl.hpp"
#include "TextureViewD3D12Impl.hpp"
#include "TopLevelASD3D12Impl.hpp"
#include "D3D12TypeConversions.hpp"
#include "ShaderVariableD3D.hpp"

namespace Diligent
{

namespace
{

inline bool ResourcesCompatible(const PipelineResourceSignatureD3D12Impl::ResourceAttribs& lhs,
                                const PipelineResourceSignatureD3D12Impl::ResourceAttribs& rhs)
{
    // Ignore sampler index, signature root index & offset.
    // clang-format off
    return lhs.Register                == rhs.Register                &&
           lhs.Space                   == rhs.Space                   &&
           lhs.SRBRootIndex            == rhs.SRBRootIndex            &&
           lhs.SRBOffsetFromTableStart == rhs.SRBOffsetFromTableStart &&
           lhs.ImtblSamplerAssigned    == rhs.ImtblSamplerAssigned    &&
           lhs.RootParamType           == rhs.RootParamType;
    // clang-format on
}

void ValidatePipelineResourceSignatureDescD3D12(const PipelineResourceSignatureDesc& Desc) noexcept(false)
{
    {
        std::unordered_multimap<HashMapStringKey, SHADER_TYPE, HashMapStringKey::Hasher> ResNameToShaderStages;
        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            const auto& Res = Desc.Resources[i];

            ResNameToShaderStages.emplace(Res.Name, Res.ShaderStages);
            auto range          = ResNameToShaderStages.equal_range(Res.Name);
            auto multi_stage_it = ResNameToShaderStages.end();
            for (auto it = range.first; it != range.second; ++it)
            {
                if (!IsPowerOfTwo(it->second))
                {
                    if (multi_stage_it == ResNameToShaderStages.end())
                        multi_stage_it = it;
                    else
                    {
                        LOG_ERROR_AND_THROW("Pipeline resource signature '", (Desc.Name != nullptr ? Desc.Name : ""),
                                            "' defines separate resources with the name '", Res.Name, "' in shader stages ",
                                            GetShaderStagesString(multi_stage_it->second), " and ",
                                            GetShaderStagesString(it->second),
                                            ". In Direct3D12 backend, only one resource in the group of resources with the same name can be shared between more than "
                                            "one shader stages. To solve this problem, use single shader stage for all but one resource with the same name.");
                    }
                }
            }
        }
    }

    {
        std::unordered_multimap<HashMapStringKey, SHADER_TYPE, HashMapStringKey::Hasher> SamNameToShaderStages;
        for (Uint32 i = 0; i < Desc.NumImmutableSamplers; ++i)
        {
            const auto& Sam = Desc.ImmutableSamplers[i];

            const auto* Name = Sam.SamplerOrTextureName;
            SamNameToShaderStages.emplace(Name, Sam.ShaderStages);
            auto range          = SamNameToShaderStages.equal_range(Name);
            auto multi_stage_it = SamNameToShaderStages.end();
            for (auto it = range.first; it != range.second; ++it)
            {
                if (!IsPowerOfTwo(it->second))
                {
                    if (multi_stage_it == SamNameToShaderStages.end())
                        multi_stage_it = it;
                    else
                    {
                        LOG_ERROR_AND_THROW("Pipeline resource signature '", (Desc.Name != nullptr ? Desc.Name : ""),
                                            "' defines separate immutable samplers with the name '", Name, "' in shader stages ",
                                            GetShaderStagesString(multi_stage_it->second), " and ",
                                            GetShaderStagesString(it->second),
                                            ". In Direct3D12 backend, only one immutable sampler in the group of samplers with the same name can be shared between more than "
                                            "one shader stages. To solve this problem, use single shader stage for all but one immutable sampler with the same name.");
                    }
                }
            }
        }
    }
}

} // namespace


PipelineResourceSignatureD3D12Impl::PipelineResourceSignatureD3D12Impl(IReferenceCounters*                  pRefCounters,
                                                                       RenderDeviceD3D12Impl*               pDevice,
                                                                       const PipelineResourceSignatureDesc& Desc,
                                                                       bool                                 bIsDeviceInternal) :
    TPipelineResourceSignatureBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
{
    try
    {
        ValidatePipelineResourceSignatureDescD3D12(Desc);

        auto& RawAllocator{GetRawAllocator()};
        auto  MemPool = AllocateInternalObjects(RawAllocator, Desc,
                                               [&Desc](FixedLinearAllocator& MemPool) //
                                               {
                                                   MemPool.AddSpace<ResourceAttribs>(Desc.NumResources);
                                                   MemPool.AddSpace<ImmutableSamplerAttribs>(Desc.NumImmutableSamplers);
                                               });

        static_assert(std::is_trivially_destructible<ResourceAttribs>::value,
                      "ResourceAttribs objects must be constructed to be properly destructed in case an excpetion is thrown");
        m_pResourceAttribs  = MemPool.Allocate<ResourceAttribs>(m_Desc.NumResources);
        m_ImmutableSamplers = MemPool.ConstructArray<ImmutableSamplerAttribs>(m_Desc.NumImmutableSamplers);

        StaticResCacheTblSizesArrayType StaticResCacheTblSizes = {};
        AllocateRootParameters(StaticResCacheTblSizes);

        const auto NumStaticResStages = GetNumStaticResStages();
        if (NumStaticResStages > 0)
        {
            m_pStaticResCache->Initialize(RawAllocator, static_cast<Uint32>(StaticResCacheTblSizes.size()), StaticResCacheTblSizes.data());

            constexpr SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_STATIC};
            for (Uint32 i = 0; i < m_StaticResStageIndex.size(); ++i)
            {
                auto Idx = m_StaticResStageIndex[i];
                if (Idx >= 0)
                {
                    VERIFY_EXPR(static_cast<Uint32>(Idx) < NumStaticResStages);
                    const auto ShaderType = GetShaderTypeFromPipelineIndex(i, GetPipelineType());
                    m_StaticVarsMgrs[Idx].Initialize(*this, RawAllocator, AllowedVarTypes, _countof(AllowedVarTypes), ShaderType);
                }
            }
        }
        else
        {
#ifdef DILIGENT_DEBUG
            for (auto TblSize : StaticResCacheTblSizes)
                VERIFY(TblSize == 0, "The size of every static resource cache table must be zero because there are no static resources in the PRS.");
#endif
        }

        if (m_Desc.SRBAllocationGranularity > 1)
        {
            std::array<size_t, MAX_SHADERS_IN_PIPELINE> ShaderVariableDataSizes = {};
            for (Uint32 s = 0; s < GetNumActiveShaderStages(); ++s)
            {
                constexpr SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};

                Uint32 UnusedNumVars       = 0;
                ShaderVariableDataSizes[s] = ShaderVariableManagerD3D12::GetRequiredMemorySize(*this, AllowedVarTypes, _countof(AllowedVarTypes), GetActiveShaderStageType(s), UnusedNumVars);
            }

            auto CacheMemorySize = ShaderResourceCacheD3D12::GetMemoryRequirements(m_RootParams);
            m_SRBMemAllocator.Initialize(m_Desc.SRBAllocationGranularity, GetNumActiveShaderStages(), ShaderVariableDataSizes.data(), 1, &CacheMemorySize.TotalSize);
        }

        m_Hash = CalculateHash();
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

void PipelineResourceSignatureD3D12Impl::AllocateRootParameters(StaticResCacheTblSizesArrayType& StaticResCacheTblSizes)
{
    // Index of the assigned sampler, for every texture SRV in m_Desc.Resources, or InvalidSamplerInd.
    std::vector<Uint32> TextureSrvToAssignedSamplerInd(m_Desc.NumResources, ResourceAttribs::InvalidSamplerInd);
    // Index of the immutable sampler for every sampler in m_Desc.Resources, or -1.
    std::vector<Int32> ResourceToImmutableSamplerInd(m_Desc.NumResources, -1);
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& ResDesc = m_Desc.Resources[i];

        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER)
        {
            // We only need to search for immutable samplers for SHADER_RESOURCE_TYPE_SAMPLER.
            // For SHADER_RESOURCE_TYPE_TEXTURE_SRV, we will look for the assigned sampler and check if it is immutable.

            // Note that FindImmutableSampler() below will work properly both when combined texture samplers are used and when not:
            //  - When combined texture samplers are used, sampler suffix will not be null,
            //    and we will be looking for the 'Texture_sampler' name.
            //  - When combined texture samplers are not used, sampler suffix will be null,
            //    and we will be looking for the sampler name itself.
            const auto SrcImmutableSamplerInd = FindImmutableSampler(ResDesc.ShaderStages, ResDesc.Name);
            if (SrcImmutableSamplerInd != InvalidImmutableSamplerIndex)
            {
                ResourceToImmutableSamplerInd[i] = SrcImmutableSamplerInd;
                // Set the immutable sampler array size to match the resource array size
                auto& DstImtblSampAttribs = m_ImmutableSamplers[SrcImmutableSamplerInd];
                // One immutable sampler may be used by different arrays in different shader stages - use the maximum array size
                DstImtblSampAttribs.ArraySize = std::max(DstImtblSampAttribs.ArraySize, ResDesc.ArraySize);
            }
        }

        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV)
        {
            TextureSrvToAssignedSamplerInd[i] = FindAssignedSampler(ResDesc, ResourceAttribs::InvalidSamplerInd);
        }
    }

    // The total number of resources (counting array size), for every descriptor range type
    std::array<Uint32, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1> NumResources = {};
    StaticResCacheTblSizes.fill(0);

    // Allocate registers for immutable samplers first
    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        auto& ImmutableSampler = m_ImmutableSamplers[i];

        constexpr auto DescriptorRangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

        ImmutableSampler.RegisterSpace  = 0;
        ImmutableSampler.ShaderRegister = NumResources[DescriptorRangeType];
        NumResources[DescriptorRangeType] += ImmutableSampler.ArraySize;
    }


    RootParamsBuilder ParamsBuilder;

    Uint32 NextRTSizedArraySpace = 1;
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& ResDesc = m_Desc.Resources[i];
        VERIFY(i == 0 || ResDesc.VarType >= m_Desc.Resources[i - 1].VarType, "Resources must be sorted by variable type");

        auto AssignedSamplerInd     = TextureSrvToAssignedSamplerInd[i];
        auto SrcImmutableSamplerInd = ResourceToImmutableSamplerInd[i];
        if (AssignedSamplerInd != ResourceAttribs::InvalidSamplerInd)
        {
            VERIFY_EXPR(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV);
            SrcImmutableSamplerInd = ResourceToImmutableSamplerInd[AssignedSamplerInd];
        }

        const auto d3d12DescriptorRangeType = ResourceTypeToD3D12DescriptorRangeType(ResDesc.ResourceType);
        const bool IsRTSizedArray           = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY) != 0;
        Uint32     Register                 = 0;
        Uint32     Space                    = 0;
        Uint32     SRBRootIndex             = ResourceAttribs::InvalidSRBRootIndex;
        Uint32     SRBOffsetFromTableStart  = ResourceAttribs::InvalidOffset;
        Uint32     SigRootIndex             = ResourceAttribs::InvalidSigRootIndex;
        Uint32     SigOffsetFromTableStart  = ResourceAttribs::InvalidOffset;

        auto d3d12RootParamType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(D3D12_ROOT_PARAMETER_TYPE_UAV + 1);
        // Do not allocate resource slot for immutable samplers that are also defined as resource
        if (!(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER && SrcImmutableSamplerInd >= 0))
        {
            if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                // Use artifial root signature:
                // SRVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_SRV (0)
                // UAVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_UAV (1)
                // CBVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_CBV (2)
                // Samplers at root index D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER (3)
                SigRootIndex            = d3d12DescriptorRangeType;
                SigOffsetFromTableStart = StaticResCacheTblSizes[SigRootIndex];
                StaticResCacheTblSizes[SigRootIndex] += ResDesc.ArraySize;
            }

            if (IsRTSizedArray)
            {
                // All run-time sized arrays are allocated in separate spaces.
                Space    = NextRTSizedArraySpace++;
                Register = 0;
            }
            else
            {
                // Normal resources go into space 0.
                Space    = 0;
                Register = NumResources[d3d12DescriptorRangeType];
                NumResources[d3d12DescriptorRangeType] += ResDesc.ArraySize;
            }

            const auto dbgValidResourceFlags = GetValidPipelineResourceFlags(ResDesc.ResourceType);
            VERIFY((ResDesc.Flags & ~dbgValidResourceFlags) == 0, "Invalid resource flags. This error should've been caught by ValidatePipelineResourceSignatureDesc.");

            const auto UseDynamicOffset  = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0;
            const auto IsFormattedBuffer = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0;
            const auto IsArray           = ResDesc.ArraySize != 1;

            d3d12RootParamType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update the switch below to handle the new shader resource type");
            switch (ResDesc.ResourceType)
            {
                case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
                    VERIFY(!IsFormattedBuffer, "Constant buffers can't be labeled as formatted. This error should've been caught by ValidatePipelineResourceSignatureDesc().");
                    d3d12RootParamType = UseDynamicOffset && !IsArray ? D3D12_ROOT_PARAMETER_TYPE_CBV : D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    break;

                case SHADER_RESOURCE_TYPE_BUFFER_SRV:
                    d3d12RootParamType = UseDynamicOffset && !IsFormattedBuffer && !IsArray ? D3D12_ROOT_PARAMETER_TYPE_SRV : D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    break;

                case SHADER_RESOURCE_TYPE_BUFFER_UAV:
                    // Always allocate buffer UAVs in descriptor tables
                    d3d12RootParamType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    break;

                default:
                    d3d12RootParamType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            }

            ParamsBuilder.AllocateResourceSlot(ResDesc.ShaderStages, ResDesc.VarType, d3d12RootParamType,
                                               d3d12DescriptorRangeType, ResDesc.ArraySize, Register, Space,
                                               SRBRootIndex, SRBOffsetFromTableStart);
        }
        else
        {
            const auto& ImtblSamAttribs = GetImmutableSamplerAttribs(SrcImmutableSamplerInd);
            VERIFY_EXPR(ImtblSamAttribs.IsValid());
            // Initialize space and register, which are required for register remapping
            Space    = ImtblSamAttribs.RegisterSpace;
            Register = ImtblSamAttribs.ShaderRegister;
        }

        new (m_pResourceAttribs + i) ResourceAttribs //
            {
                Register,
                Space,
                AssignedSamplerInd,
                SRBRootIndex,
                SRBOffsetFromTableStart,
                SigRootIndex,
                SigOffsetFromTableStart,
                SrcImmutableSamplerInd >= 0,
                d3d12RootParamType //
            };
    }
    ParamsBuilder.InitializeMgr(GetRawAllocator(), m_RootParams);
}

PipelineResourceSignatureD3D12Impl::~PipelineResourceSignatureD3D12Impl()
{
    Destruct();
}

void PipelineResourceSignatureD3D12Impl::Destruct()
{
    if (m_ImmutableSamplers != nullptr)
    {
        for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
        {
            m_ImmutableSamplers[i].~ImmutableSamplerAttribs();
        }
        m_ImmutableSamplers = nullptr;
    }

    m_pResourceAttribs = nullptr;

    TPipelineResourceSignatureBase::Destruct();
}

bool PipelineResourceSignatureD3D12Impl::IsCompatibleWith(const PipelineResourceSignatureD3D12Impl& Other) const
{
    if (this == &Other)
        return true;

    if (GetHash() != Other.GetHash())
        return false;

    if (!PipelineResourceSignaturesCompatible(GetDesc(), Other.GetDesc()))
        return false;

    const auto ResCount = GetTotalResourceCount();
    VERIFY_EXPR(ResCount == Other.GetTotalResourceCount());
    for (Uint32 r = 0; r < ResCount; ++r)
    {
        if (!ResourcesCompatible(GetResourceAttribs(r), Other.GetResourceAttribs(r)))
            return false;
    }

    return true;
}

size_t PipelineResourceSignatureD3D12Impl::CalculateHash() const
{
    if (m_Desc.NumResources == 0 && m_Desc.NumImmutableSamplers == 0)
        return 0;

    auto Hash = CalculatePipelineResourceSignatureDescHash(m_Desc);
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& Attr = m_pResourceAttribs[i];
        HashCombine(Hash, Attr.Register, Attr.Space, Attr.SRBRootIndex, Attr.SRBOffsetFromTableStart,
                    Attr.RootParamType, Attr.IsImmutableSamplerAssigned());
    }

    return Hash;
}

void PipelineResourceSignatureD3D12Impl::InitSRBResourceCache(ShaderResourceCacheD3D12& ResourceCache)
{
    ResourceCache.Initialize(m_SRBMemAllocator.GetResourceCacheDataAllocator(0), m_pDevice, m_RootParams);
}

void PipelineResourceSignatureD3D12Impl::CopyStaticResources(ShaderResourceCacheD3D12& DstResourceCache) const
{
    if (m_pStaticResCache == nullptr)
        return;

    // SrcResourceCache contains only static resources.
    // DstResourceCache contains static, mutable and dynamic resources.
    const auto& SrcResourceCache = *m_pStaticResCache;
    const auto  ResIdxRange      = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    auto* const d3d12Device      = GetDevice()->GetD3D12Device();
    const auto  SrcCacheType     = SrcResourceCache.GetContentType();
    const auto  DstCacheType     = DstResourceCache.GetContentType();
    VERIFY_EXPR(SrcCacheType == ResourceCacheContentType::Signature);
    VERIFY_EXPR(DstCacheType == ResourceCacheContentType::SRB);

    for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
    {
        const auto& ResDesc   = GetResourceDesc(r);
        const auto& Attr      = GetResourceAttribs(r);
        const bool  IsSampler = (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);
        VERIFY_EXPR(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        if (IsSampler && Attr.IsImmutableSamplerAssigned())
        {
            // Immutable samplers should not be assigned cache space
            VERIFY_EXPR(Attr.RootIndex(ResourceCacheContentType::Signature) == ResourceAttribs::InvalidSigRootIndex);
            VERIFY_EXPR(Attr.RootIndex(ResourceCacheContentType::SRB) == ResourceAttribs::InvalidSRBRootIndex);
            VERIFY_EXPR(Attr.SigOffsetFromTableStart == ResourceAttribs::InvalidOffset);
            VERIFY_EXPR(Attr.SRBOffsetFromTableStart == ResourceAttribs::InvalidOffset);
            continue;
        }

        const auto  HeapType     = IsSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        const auto  DstRootIndex = Attr.RootIndex(DstCacheType);
        const auto  SrcRootIndex = Attr.RootIndex(SrcCacheType);
        const auto& SrcRootTable = SrcResourceCache.GetRootTable(SrcRootIndex);
        const auto& DstRootTable = const_cast<const ShaderResourceCacheD3D12&>(DstResourceCache).GetRootTable(DstRootIndex);

        auto SrcCacheOffset = Attr.OffsetFromTableStart(SrcCacheType);
        auto DstCacheOffset = Attr.OffsetFromTableStart(DstCacheType);
        for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd, ++SrcCacheOffset, ++DstCacheOffset)
        {
            const auto& SrcRes = SrcRootTable.GetResource(SrcCacheOffset);
            if (!SrcRes.pObject)
                LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

            const auto& DstRes = DstRootTable.GetResource(DstCacheOffset);
            if (DstRes.pObject != SrcRes.pObject)
            {
                DEV_CHECK_ERR(DstRes.pObject == nullptr, "Static resource has already been initialized, and the new resource does not match previously assigned resource.");

                DstResourceCache.CopyResource(DstRootIndex, DstCacheOffset, SrcRes);

                if (!Attr.IsRootView())
                {
                    auto DstDescrHandle = DstResourceCache.GetDescriptorTableHandle<D3D12_CPU_DESCRIPTOR_HANDLE>(
                        HeapType, ROOT_PARAMETER_GROUP_STATIC_MUTABLE, DstRootIndex, DstCacheOffset);
                    VERIFY_EXPR(DstDescrHandle.ptr != 0);
                    d3d12Device->CopyDescriptorsSimple(1, DstDescrHandle, SrcRes.CPUDescriptorHandle, HeapType);
                }
            }
            else
            {
                VERIFY_EXPR(DstRes.pObject == SrcRes.pObject);
                VERIFY_EXPR(DstRes.Type == SrcRes.Type);
                VERIFY_EXPR(DstRes.CPUDescriptorHandle.ptr == SrcRes.CPUDescriptorHandle.ptr);
            }
        }
    }
}

void PipelineResourceSignatureD3D12Impl::CommitRootViews(const CommitCacheResourcesAttribs& CommitAttribs,
                                                         Uint64                             BuffersMask) const
{
    while (BuffersMask != 0)
    {
        const auto  BufferBit = ExtractLSB(BuffersMask);
        const auto  RootInd   = PlatformMisc::GetLSB(BufferBit);
        const auto& CacheTbl  = CommitAttribs.ResourceCache.GetRootTable(RootInd);
        VERIFY_EXPR(CacheTbl.IsRootView());
        const auto& BaseRootIndex = CommitAttribs.BaseRootIndex;

        VERIFY_EXPR(CacheTbl.GetSize() == 1);
        const auto& Res = CacheTbl.GetResource(0);
        if (Res.IsNull())
        {
            LOG_ERROR_MESSAGE("Failed to bind root view at index ", BaseRootIndex + RootInd, ": no resource is bound in the cache.");
            continue;
        }

        BufferD3D12Impl* pBuffer = nullptr;
        if (Res.Type == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER)
        {
            // No need to QueryInterface() - the type is verified when a resource is bound
            pBuffer = Res.pObject.RawPtr<BufferD3D12Impl>();
        }
        else if (Res.Type == SHADER_RESOURCE_TYPE_BUFFER_SRV ||
                 Res.Type == SHADER_RESOURCE_TYPE_BUFFER_UAV)
        {
            auto* pBuffView = Res.pObject.RawPtr<BufferViewD3D12Impl>();
            pBuffer         = pBuffView->GetBuffer<BufferD3D12Impl>();
        }
        else
        {
            UNEXPECTED("Unexpected root view resource type");
        }
        VERIFY_EXPR(pBuffer != nullptr);

        const auto BufferGPUAddress = pBuffer->GetGPUAddress(CommitAttribs.DeviceCtxId, CommitAttribs.pDeviceCtx);
        VERIFY_EXPR(BufferGPUAddress != 0);

        auto* const pd3d12CmdList = CommitAttribs.Ctx.GetCommandList();
        static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update the switch below to handle the new shader resource type");
        switch (Res.Type)
        {
            case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
                if (CommitAttribs.IsCompute)
                    pd3d12CmdList->SetComputeRootConstantBufferView(BaseRootIndex + RootInd, BufferGPUAddress);
                else
                    pd3d12CmdList->SetGraphicsRootConstantBufferView(BaseRootIndex + RootInd, BufferGPUAddress);
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_SRV:
                if (CommitAttribs.IsCompute)
                    pd3d12CmdList->SetComputeRootShaderResourceView(BaseRootIndex + RootInd, BufferGPUAddress);
                else
                    pd3d12CmdList->SetGraphicsRootShaderResourceView(BaseRootIndex + RootInd, BufferGPUAddress);
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_UAV:
                if (CommitAttribs.IsCompute)
                    pd3d12CmdList->SetComputeRootUnorderedAccessView(BaseRootIndex + RootInd, BufferGPUAddress);
                else
                    pd3d12CmdList->SetGraphicsRootUnorderedAccessView(BaseRootIndex + RootInd, BufferGPUAddress);
                break;

            default:
                UNEXPECTED("Unexpected root view resource type");
        }
    }
}

void PipelineResourceSignatureD3D12Impl::CommitRootTables(const CommitCacheResourcesAttribs& CommitAttribs) const
{
    const auto& ResourceCache = CommitAttribs.ResourceCache;
    const auto& BaseRootIndex = CommitAttribs.BaseRootIndex;
    auto&       CmdCtx        = CommitAttribs.Ctx;

    auto* const pd3d12Device = GetDevice()->GetD3D12Device();

    // Having an array of actual DescriptorHeapAllocation objects introduces unncessary overhead when
    // there are no dynamic variables as constructors and desctructors are always called. To avoid this
    // overhead we will construct DescriptorHeapAllocation in-place only when they are really needed.
    std::array<DescriptorHeapAllocation*, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1> pDynamicDescriptorAllocations{};

    // Reserve space for DescriptorHeapAllocation objects (do NOT zero-out!)
    alignas(DescriptorHeapAllocation) uint8_t DynamicDescriptorAllocationsRawMem[sizeof(DescriptorHeapAllocation) * (D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1)];

    for (Uint32 heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; heap_type < D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1; ++heap_type)
    {
        const auto d3d12HeapType = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(heap_type);

        auto NumDynamicDescriptors = m_RootParams.GetParameterGroupSize(d3d12HeapType, ROOT_PARAMETER_GROUP_DYNAMIC);
        if (NumDynamicDescriptors > 0)
        {
            auto& pAllocation = pDynamicDescriptorAllocations[d3d12HeapType];

            // Create new DescriptorHeapAllocation in-place
            pAllocation = new (&DynamicDescriptorAllocationsRawMem[sizeof(DescriptorHeapAllocation) * heap_type])
                DescriptorHeapAllocation{CmdCtx.AllocateDynamicGPUVisibleDescriptor(d3d12HeapType, NumDynamicDescriptors)};

            DEV_CHECK_ERR(!pAllocation->IsNull(),
                          "Failed to allocate ", NumDynamicDescriptors, " dynamic GPU-visible ",
                          (d3d12HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? "CBV/SRV/UAV" : "Sampler"),
                          " descriptor(s). Consider increasing GPUDescriptorHeapDynamicSize[", heap_type,
                          "] in EngineD3D12CreateInfo or optimizing dynamic resource utilization by using static "
                          "or mutable shader resource variables instead.");

            // Copy all dynamic descriptors from the CPU-only cache allocation
            const auto& SrcDynamicAllocation = ResourceCache.GetDescriptorAllocation(d3d12HeapType, ROOT_PARAMETER_GROUP_DYNAMIC);
            VERIFY_EXPR(SrcDynamicAllocation.GetNumHandles() == NumDynamicDescriptors);
            pd3d12Device->CopyDescriptorsSimple(NumDynamicDescriptors, pAllocation->GetCpuHandle(), SrcDynamicAllocation.GetCpuHandle(), d3d12HeapType);
        }
    }

    auto* const pSrvCbvUavDynamicAllocation = pDynamicDescriptorAllocations[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    auto* const pSamplerDynamicAllocation   = pDynamicDescriptorAllocations[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];

    CommandContext::ShaderDescriptorHeaps Heaps{
        ResourceCache.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, ROOT_PARAMETER_GROUP_STATIC_MUTABLE),
        ResourceCache.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, ROOT_PARAMETER_GROUP_STATIC_MUTABLE),
    };
    if (Heaps.pSrvCbvUavHeap == nullptr && pSrvCbvUavDynamicAllocation != nullptr)
        Heaps.pSrvCbvUavHeap = pSrvCbvUavDynamicAllocation->GetDescriptorHeap();
    if (Heaps.pSamplerHeap == nullptr && pSamplerDynamicAllocation != nullptr)
        Heaps.pSamplerHeap = pSamplerDynamicAllocation->GetDescriptorHeap();

    VERIFY(pSrvCbvUavDynamicAllocation == nullptr || pSrvCbvUavDynamicAllocation->GetDescriptorHeap() == Heaps.pSrvCbvUavHeap,
           "Inconsistent CBV/SRV/UAV descriptor heaps");
    VERIFY(pSamplerDynamicAllocation == nullptr || pSamplerDynamicAllocation->GetDescriptorHeap() == Heaps.pSamplerHeap,
           "Inconsistent Sampler descriptor heaps");

    if (Heaps)
        CmdCtx.SetDescriptorHeaps(Heaps);

    const auto NumRootTables = m_RootParams.GetNumRootTables();
    for (Uint32 rt = 0; rt < NumRootTables; ++rt)
    {
        const auto& RootTable = m_RootParams.GetRootTable(rt);

        const auto TableOffsetInGroupAllocation = RootTable.TableOffsetInGroupAllocation;
        VERIFY_EXPR(TableOffsetInGroupAllocation != RootParameter::InvalidTableOffsetInGroupAllocation);

        const auto& d3d12Param = RootTable.d3d12RootParam;
        VERIFY_EXPR(d3d12Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
        const auto& d3d12Table = d3d12Param.DescriptorTable;

        const auto d3d12HeapType = d3d12Table.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER ?
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER :
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        D3D12_GPU_DESCRIPTOR_HANDLE RootTableGPUDescriptorHandle{};
        if (RootTable.Group == ROOT_PARAMETER_GROUP_DYNAMIC)
        {
            auto& DynamicAllocation      = *pDynamicDescriptorAllocations[d3d12HeapType];
            RootTableGPUDescriptorHandle = DynamicAllocation.GetGpuHandle(TableOffsetInGroupAllocation);
        }
        else
        {
            RootTableGPUDescriptorHandle = ResourceCache.GetDescriptorTableHandle<D3D12_GPU_DESCRIPTOR_HANDLE>(
                d3d12HeapType, ROOT_PARAMETER_GROUP_STATIC_MUTABLE, RootTable.RootIndex);
            VERIFY(RootTableGPUDescriptorHandle.ptr != 0, "Unexpected null GPU descriptor handle");
        }

        if (CommitAttribs.IsCompute)
            CmdCtx.GetCommandList()->SetComputeRootDescriptorTable(BaseRootIndex + RootTable.RootIndex, RootTableGPUDescriptorHandle);
        else
            CmdCtx.GetCommandList()->SetGraphicsRootDescriptorTable(BaseRootIndex + RootTable.RootIndex, RootTableGPUDescriptorHandle);
    }

    // Commit non-dynamic root buffer views
    if (auto NonDynamicBuffersMask = ResourceCache.GetNonDynamicRootBuffersMask())
    {
        CommitRootViews(CommitAttribs, NonDynamicBuffersMask);
    }

    // Manually destroy DescriptorHeapAllocation objects we created.
    for (auto* pAllocation : pDynamicDescriptorAllocations)
    {
        if (pAllocation != nullptr)
            pAllocation->~DescriptorHeapAllocation();
    }
}


void PipelineResourceSignatureD3D12Impl::UpdateShaderResourceBindingMap(ResourceBinding::TMap& ResourceMap, SHADER_TYPE ShaderStage, Uint32 BaseRegisterSpace) const
{
    VERIFY(ShaderStage != SHADER_TYPE_UNKNOWN && IsPowerOfTwo(ShaderStage), "Only single shader stage must be provided.");

    for (Uint32 r = 0, ResCount = GetTotalResourceCount(); r < ResCount; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& Attribs = GetResourceAttribs(r);

        if ((ResDesc.ShaderStages & ShaderStage) != 0)
        {
            ResourceBinding::BindInfo BindInfo //
                {
                    Attribs.Register,
                    Attribs.Space + BaseRegisterSpace,
                    ResDesc.ArraySize,
                    ResDesc.ResourceType //
                };
            auto IsUnique = ResourceMap.emplace(HashMapStringKey{ResDesc.Name}, BindInfo).second;
            VERIFY(IsUnique, "Shader resource '", ResDesc.Name,
                   "' already present in the binding map. Every shader resource in PSO must be unambiguously defined by "
                   "only one resource signature. This error should've been caught by ValidatePipelineResourceSignatures().");
        }
    }

    for (Uint32 samp = 0, SampCount = GetImmutableSamplerCount(); samp < SampCount; ++samp)
    {
        const auto& ImtblSam = GetImmutableSamplerDesc(samp);
        const auto& SampAttr = GetImmutableSamplerAttribs(samp);

        if ((ImtblSam.ShaderStages & ShaderStage) != 0)
        {
            String SampName{ImtblSam.SamplerOrTextureName};
            if (IsUsingCombinedSamplers())
                SampName += GetCombinedSamplerSuffix();

            ResourceBinding::BindInfo BindInfo //
                {
                    SampAttr.ShaderRegister,
                    SampAttr.RegisterSpace + BaseRegisterSpace,
                    SampAttr.ArraySize,
                    SHADER_RESOURCE_TYPE_SAMPLER //
                };

            auto it_inserted = ResourceMap.emplace(HashMapStringKey{SampName}, BindInfo);
#ifdef DILIGENT_DEBUG
            if (!it_inserted.second)
            {
                const auto& ExistingBindInfo = it_inserted.first->second;
                VERIFY(ExistingBindInfo.BindPoint == BindInfo.BindPoint,
                       "Bind point defined by the immutable sampler attribs is inconsistent with the bind point defined by the sampler resource.");
                VERIFY(ExistingBindInfo.Space == BindInfo.Space,
                       "Register space defined by the immutable sampler attribs is inconsistent with the bind point defined by the sampler resource.");
            }
#endif
        }
    }
}

bool PipelineResourceSignatureD3D12Impl::HasImmutableSamplerArray(SHADER_TYPE ShaderStage) const
{
    for (Uint32 s = 0; s < GetImmutableSamplerCount(); ++s)
    {
        const auto& ImtblSam = GetImmutableSamplerDesc(s);
        const auto& SampAttr = GetImmutableSamplerAttribs(s);
        if ((ImtblSam.ShaderStages & ShaderStage) != 0 && SampAttr.ArraySize > 1)
            return true;
    }
    return false;
}


namespace
{

class BindResourceHelper
{
public:
    BindResourceHelper(const PipelineResourceSignatureD3D12Impl& Signature,
                       ShaderResourceCacheD3D12&                 ResourceCache,
                       Uint32                                    ResIndex,
                       Uint32                                    ArrayIndex);

    void operator()(IDeviceObject* pObj) const;

private:
    void CacheCB(IDeviceObject* pBuffer) const;
    void CacheSampler(IDeviceObject* pBuffer) const;
    void CacheAccelStruct(IDeviceObject* pBuffer) const;
    void BindCombinedSampler(TextureViewD3D12Impl* pTexView) const;
    void BindCombinedSampler(BufferViewD3D12Impl* pTexView) const {}

    template <typename TResourceViewType, ///< The type of the view (TextureViewD3D12Impl or BufferViewD3D12Impl)
              typename TViewTypeEnum      ///< The type of the expected view type enum (TEXTURE_VIEW_TYPE or BUFFER_VIEW_TYPE)
              >
    void CacheResourceView(IDeviceObject* pBufferView,
                           TViewTypeEnum  dbgExpectedViewType) const;

    ID3D12Device* GetD3D12Device() const { return m_Signature.GetDevice()->GetD3D12Device(); }

    void SetResource(D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle, RefCntAutoPtr<IDeviceObject>&& pObject) const
    {
        if (m_DstTableCPUDescriptorHandle.ptr != 0)
        {
            VERIFY(CPUDescriptorHandle.ptr != 0, "CPU descriptor handle must not be null for resources allocated in descriptor tables");
            DEV_CHECK_ERR(m_ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC || m_DstRes.pObject == nullptr, "Static and mutable resource descriptors should only be copied once");
            const auto d3d12HeapType = m_ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER ?
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER :
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            GetD3D12Device()->CopyDescriptorsSimple(1, m_DstTableCPUDescriptorHandle, CPUDescriptorHandle, d3d12HeapType);
        }

        m_ResourceCache.SetResource(m_RootIndex, m_OffsetFromTableStart, m_ResDesc.ResourceType, CPUDescriptorHandle, std::move(pObject));
    }

private:
    using ResourceAttribs = PipelineResourceSignatureD3D12Impl::ResourceAttribs;

    const PipelineResourceSignatureD3D12Impl& m_Signature;
    ShaderResourceCacheD3D12&                 m_ResourceCache;

    const PipelineResourceDesc& m_ResDesc;
    const ResourceAttribs&      m_Attribs; // Must go before m_RootIndex, m_OffsetFromTableStart

    const ResourceCacheContentType m_CacheType; // Must go before m_RootIndex, m_OffsetFromTableStart
    const Uint32                   m_RootIndex; // Must go before m_DstRes
    const Uint32                   m_ArrayIndex;
    const Uint32                   m_OffsetFromTableStart; // Must go before m_DstRes

    const ShaderResourceCacheD3D12::Resource& m_DstRes;

    D3D12_CPU_DESCRIPTOR_HANDLE m_DstTableCPUDescriptorHandle{};
};

BindResourceHelper::BindResourceHelper(const PipelineResourceSignatureD3D12Impl& Signature,
                                       ShaderResourceCacheD3D12&                 ResourceCache,
                                       Uint32                                    ResIndex,
                                       Uint32                                    ArrayIndex) :
    // clang-format off
    m_Signature     {Signature},
    m_ResourceCache {ResourceCache},
    m_ResDesc       {Signature.GetResourceDesc(ResIndex)},
    m_Attribs       {Signature.GetResourceAttribs(ResIndex)},
    m_CacheType     {ResourceCache.GetContentType()},
    m_RootIndex     {m_Attribs.RootIndex(m_CacheType)},
    m_ArrayIndex    {ArrayIndex},
    m_OffsetFromTableStart{m_Attribs.OffsetFromTableStart(m_CacheType) + ArrayIndex},
    m_DstRes        {const_cast<const ShaderResourceCacheD3D12&>(ResourceCache).GetRootTable(m_RootIndex).GetResource(m_OffsetFromTableStart)}
// clang-format on
{
    VERIFY(ArrayIndex < m_ResDesc.ArraySize, "Array index is out of range");

    if (m_CacheType != ResourceCacheContentType::Signature && !m_Attribs.IsRootView())
    {
        const auto IsSampler      = (m_ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);
        const auto RootParamGroup = VariableTypeToRootParameterGroup(m_ResDesc.VarType);
        // Static/mutable resources are allocated in GPU-visible descriptor heap, while dynamic resources - in CPU-only heap.
        m_DstTableCPUDescriptorHandle =
            ResourceCache.GetDescriptorTableHandle<D3D12_CPU_DESCRIPTOR_HANDLE>(
                IsSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                RootParamGroup, m_RootIndex, m_OffsetFromTableStart);
    }

#ifdef DILIGENT_DEBUG
    if (m_CacheType == ResourceCacheContentType::Signature)
    {
        VERIFY(m_DstTableCPUDescriptorHandle.ptr == 0, "Static shader resource cache should never be assigned descriptor space.");
    }
    else if (m_CacheType == ResourceCacheContentType::SRB)
    {
        if (m_Attribs.GetD3D12RootParamType() == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            VERIFY(m_DstTableCPUDescriptorHandle.ptr != 0, "Shader resources allocated in descriptor tables must be assigned descriptor space.");
        }
        else
        {
            VERIFY_EXPR(m_Attribs.IsRootView());
            VERIFY((m_ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER ||
                    m_ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV ||
                    m_ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV),
                   "Only constant buffers and dynamic buffer views can be allocated as root views");
            VERIFY(m_DstTableCPUDescriptorHandle.ptr == 0, "Resources allocated as root views should never be assigned descriptor space.");
        }
    }
    else
    {
        UNEXPECTED("Unknown content type");
    }
#endif
}

void BindResourceHelper::CacheCB(IDeviceObject* pBuffer) const
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferD3D12Impl> pBuffD3D12{pBuffer, IID_BufferD3D12};
#ifdef DILIGENT_DEVELOPMENT
    VerifyConstantBufferBinding(m_ResDesc.Name, m_ResDesc.ArraySize, m_ResDesc.VarType, m_ResDesc.Flags, m_ArrayIndex,
                                pBuffer, pBuffD3D12.RawPtr(), m_DstRes.pObject.RawPtr());
    if (m_ResDesc.ArraySize != 1 && pBuffD3D12 && pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC && pBuffD3D12->GetD3D12Resource() == nullptr)
    {
        LOG_ERROR_MESSAGE("Attempting to bind dynamic buffer '", pBuffD3D12->GetDesc().Name, "' that doesn't have backing d3d12 resource to array variable '", m_ResDesc.Name,
                          "[", m_ResDesc.ArraySize, "]', which is currently not supported in Direct3D12 backend. Either use non-array variable, or bind non-dynamic buffer.");
    }
#endif
    if (pBuffD3D12)
    {
        if (m_ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && m_DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        const auto CPUDescriptorHandle = pBuffD3D12->GetCBVHandle();
        VERIFY(CPUDescriptorHandle.ptr != 0 || pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC,
               "Only dynamic constant buffers may have null CPU descriptor");

        SetResource(CPUDescriptorHandle, std::move(pBuffD3D12));
    }
}


void BindResourceHelper::CacheSampler(IDeviceObject* pSampler) const
{
    RefCntAutoPtr<ISamplerD3D12> pSamplerD3D12{pSampler, IID_SamplerD3D12};
    if (pSamplerD3D12)
    {
        if (m_ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && m_DstRes.pObject != nullptr)
        {
            if (m_DstRes.pObject != pSampler)
            {
                auto VarTypeStr = GetShaderVariableTypeLiteralName(m_ResDesc.VarType);
                LOG_ERROR_MESSAGE("Non-null sampler is already bound to ", VarTypeStr, " shader variable '", GetShaderResourcePrintName(m_ResDesc, m_ArrayIndex),
                                  "'. Attempting to bind another sampler is an error and will be ignored. ",
                                  "Use another shader resource binding instance or label the variable as dynamic.");
            }

            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        const auto CPUDescriptorHandle = pSamplerD3D12->GetCPUDescriptorHandle();
        VERIFY(CPUDescriptorHandle.ptr != 0, "Samplers must always have valid CPU descriptors");
        VERIFY(m_CacheType == ResourceCacheContentType::Signature || m_DstTableCPUDescriptorHandle.ptr != 0,
               "Samplers in SRB cache must always be allocated in root tables and thus assigned descriptor in the table");

        SetResource(CPUDescriptorHandle, std::move(pSamplerD3D12));
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to bind object '", pSampler->GetDesc().Name, "' to variable '",
                          GetShaderResourcePrintName(m_ResDesc, m_ArrayIndex), "'. Incorect object type: sampler is expected.");
    }
}


void BindResourceHelper::CacheAccelStruct(IDeviceObject* pTLAS) const
{
    RefCntAutoPtr<ITopLevelASD3D12> pTLASD3D12{pTLAS, IID_TopLevelASD3D12};
    if (pTLASD3D12)
    {
        if (m_ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && m_DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        const auto CPUDescriptorHandle = pTLASD3D12->GetCPUDescriptorHandle();
        VERIFY(CPUDescriptorHandle.ptr != 0, "Acceleration structures must always have valid CPU descriptor handles");
        VERIFY(m_CacheType == ResourceCacheContentType::Signature || m_DstTableCPUDescriptorHandle.ptr != 0,
               "Acceleration structures in SRB cache are always allocated in root tables and thus must have a descriptor");

        SetResource(CPUDescriptorHandle, std::move(pTLASD3D12));
    }
}


template <typename TResourceViewType>
struct ResourceViewTraits
{};

template <>
struct ResourceViewTraits<TextureViewD3D12Impl>
{
    static const INTERFACE_ID& IID;

    static bool VerifyView(const TextureViewD3D12Impl* pViewD3D12, const PipelineResourceDesc& ResDesc)
    {
        return true;
    }
};
const INTERFACE_ID& ResourceViewTraits<TextureViewD3D12Impl>::IID = IID_TextureViewD3D12;

template <>
struct ResourceViewTraits<BufferViewD3D12Impl>
{
    static const INTERFACE_ID& IID;

    static bool VerifyView(const BufferViewD3D12Impl* pViewD3D12, const PipelineResourceDesc& ResDesc)
    {
        if (pViewD3D12 != nullptr)
        {
            const auto* const pBuffer = pViewD3D12->GetBuffer<BufferD3D12Impl>();
            if (ResDesc.ArraySize != 1 && pBuffer->GetDesc().Usage == USAGE_DYNAMIC && pBuffer->GetD3D12Resource() == nullptr)
            {
                LOG_ERROR_MESSAGE("Attempting to bind dynamic buffer '", pBuffer->GetDesc().Name, "' that doesn't have backing d3d12 resource to array variable '", ResDesc.Name,
                                  "[", ResDesc.ArraySize, "]', which is currently not supported in Direct3D12 backend. Either use non-array variable, or bind non-dynamic buffer.");
                return false;
            }
        }

        return true;
    }
};
const INTERFACE_ID& ResourceViewTraits<BufferViewD3D12Impl>::IID = IID_BufferViewD3D12;


template <typename TResourceViewType,
          typename TViewTypeEnum>
void BindResourceHelper::CacheResourceView(IDeviceObject* pView,
                                           TViewTypeEnum  dbgExpectedViewType) const
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<TResourceViewType> pViewD3D12{pView, ResourceViewTraits<TResourceViewType>::IID};
#ifdef DILIGENT_DEVELOPMENT
    VerifyResourceViewBinding(m_ResDesc.Name, m_ResDesc.ArraySize, m_ResDesc.VarType, m_ArrayIndex,
                              pView, pViewD3D12.RawPtr(),
                              {dbgExpectedViewType}, RESOURCE_DIM_UNDEFINED,
                              false, // IsMultisample
                              m_DstRes.pObject.RawPtr());
    ResourceViewTraits<TResourceViewType>::VerifyView(pViewD3D12, m_ResDesc);
#endif
    if (pViewD3D12)
    {
        if (m_ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && m_DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        const auto CPUDescriptorHandle = pViewD3D12->GetCPUDescriptorHandle();
        // Note that for dynamic structured buffers we still create SRV even though we don't really use it.
        VERIFY(CPUDescriptorHandle.ptr != 0, "Texture/buffer views should always have valid CPU descriptor handles");

        BindCombinedSampler(pViewD3D12);

        SetResource(CPUDescriptorHandle, std::move(pViewD3D12));
    }
}


void BindResourceHelper::BindCombinedSampler(TextureViewD3D12Impl* pTexView) const
{
    if (m_ResDesc.ResourceType != SHADER_RESOURCE_TYPE_TEXTURE_SRV)
    {
        VERIFY(!m_Attribs.IsCombinedWithSampler(), "Only texture SRVs can be combined with sampler");
        return;
    }

    if (!m_Attribs.IsCombinedWithSampler())
        return;

    const auto& SamplerResDesc = m_Signature.GetResourceDesc(m_Attribs.SamplerInd);
    const auto& SamplerAttribs = m_Signature.GetResourceAttribs(m_Attribs.SamplerInd);
    VERIFY_EXPR(SamplerResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);

    if (SamplerAttribs.IsImmutableSamplerAssigned())
    {
        // Immutable samplers should not be assigned cache space
        VERIFY_EXPR(SamplerAttribs.RootIndex(ResourceCacheContentType::Signature) == ResourceAttribs::InvalidSigRootIndex);
        VERIFY_EXPR(SamplerAttribs.RootIndex(ResourceCacheContentType::SRB) == ResourceAttribs::InvalidSRBRootIndex);
        VERIFY_EXPR(SamplerAttribs.SigOffsetFromTableStart == ResourceAttribs::InvalidOffset);
        VERIFY_EXPR(SamplerAttribs.SRBOffsetFromTableStart == ResourceAttribs::InvalidOffset);
        return;
    }

    auto* const pSampler = pTexView->GetSampler();
    if (pSampler == nullptr)
    {
        LOG_ERROR_MESSAGE("Failed to bind sampler to variable '", SamplerResDesc.Name, ". Sampler is not set in the texture view '", pTexView->GetDesc().Name, '\'');
        return;
    }

    VERIFY_EXPR(m_ResDesc.ArraySize == SamplerResDesc.ArraySize || SamplerResDesc.ArraySize == 1);
    const auto SamplerArrInd = SamplerResDesc.ArraySize > 1 ? m_ArrayIndex : 0;

    BindResourceHelper BindSampler{m_Signature, m_ResourceCache, m_Attribs.SamplerInd, SamplerArrInd};
    BindSampler(pSampler);
}

void BindResourceHelper::operator()(IDeviceObject* pObj) const
{
    if (pObj)
    {
        static_assert(SHADER_RESOURCE_TYPE_LAST == 8, "Please update this function to handle the new resource type");
        switch (m_ResDesc.ResourceType)
        {
            case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
                CacheCB(pObj);
                break;

            case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
            case SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT:
                CacheResourceView<TextureViewD3D12Impl>(pObj, TEXTURE_VIEW_SHADER_RESOURCE);
                break;

            case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
                CacheResourceView<TextureViewD3D12Impl>(pObj, TEXTURE_VIEW_UNORDERED_ACCESS);
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_SRV:
                CacheResourceView<BufferViewD3D12Impl>(pObj, BUFFER_VIEW_SHADER_RESOURCE);
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_UAV:
                CacheResourceView<BufferViewD3D12Impl>(pObj, BUFFER_VIEW_UNORDERED_ACCESS);
                break;

            case SHADER_RESOURCE_TYPE_SAMPLER:
                CacheSampler(pObj);
                break;

            case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
                CacheAccelStruct(pObj);
                break;

            default: UNEXPECTED("Unknown resource type ", static_cast<Int32>(m_ResDesc.ResourceType));
        }
    }
    else
    {
        if (m_DstRes.pObject != nullptr && m_ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            LOG_ERROR_MESSAGE("Shader variable '", m_ResDesc.Name, "' is not dynamic but is being reset to null. This is an error and may cause unpredicted behavior. ",
                              "Use another shader resource binding instance or label the variable as dynamic if you need to bind another resource.");
        }

        m_ResourceCache.ResetResource(m_RootIndex, m_OffsetFromTableStart);
        if (m_Attribs.IsCombinedWithSampler())
        {
            auto& SamplerResDesc = m_Signature.GetResourceDesc(m_Attribs.SamplerInd);
            auto& SamplerAttribs = m_Signature.GetResourceAttribs(m_Attribs.SamplerInd);
            VERIFY_EXPR(SamplerResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);

            if (!SamplerAttribs.IsImmutableSamplerAssigned())
            {
                const auto  SamplerArrInd           = SamplerResDesc.ArraySize > 1 ? m_ArrayIndex : 0;
                const auto  SamRootIndex            = SamplerAttribs.RootIndex(m_CacheType);
                const auto  SamOffsetFromTableStart = SamplerAttribs.OffsetFromTableStart(m_CacheType) + SamplerArrInd;
                const auto& DstSam                  = const_cast<const ShaderResourceCacheD3D12&>(m_ResourceCache).GetRootTable(SamRootIndex).GetResource(SamOffsetFromTableStart);

                if (DstSam.pObject != nullptr && SamplerResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                {
                    LOG_ERROR_MESSAGE("Sampler variable '", SamplerResDesc.Name, "' is not dynamic but is being reset to null. This is an error and may cause unpredicted behavior. ",
                                      "Use another shader resource binding instance or label the variable as dynamic if you need to bind another sampler.");
                }

                m_ResourceCache.ResetResource(SamRootIndex, SamOffsetFromTableStart);
            }
        }
    }
}

} // namespace


void PipelineResourceSignatureD3D12Impl::BindResource(IDeviceObject*            pObj,
                                                      Uint32                    ArrayIndex,
                                                      Uint32                    ResIndex,
                                                      ShaderResourceCacheD3D12& ResourceCache) const
{
    VERIFY(IsUsingSeparateSamplers() || GetResourceDesc(ResIndex).ResourceType != SHADER_RESOURCE_TYPE_SAMPLER,
           "Samplers should not be set directly when using combined texture samplers");
    BindResourceHelper BindResHelper{*this, ResourceCache, ResIndex, ArrayIndex};
    BindResHelper(pObj);
}

bool PipelineResourceSignatureD3D12Impl::IsBound(Uint32                          ArrayIndex,
                                                 Uint32                          ResIndex,
                                                 const ShaderResourceCacheD3D12& ResourceCache) const
{
    const auto& ResDesc              = GetResourceDesc(ResIndex);
    const auto& Attribs              = GetResourceAttribs(ResIndex);
    const auto  CacheType            = ResourceCache.GetContentType();
    const auto  RootIndex            = Attribs.RootIndex(CacheType);
    const auto  OffsetFromTableStart = Attribs.OffsetFromTableStart(CacheType) + ArrayIndex;

    VERIFY_EXPR(ArrayIndex < ResDesc.ArraySize);

    if (RootIndex < ResourceCache.GetNumRootTables())
    {
        const auto& RootTable = ResourceCache.GetRootTable(RootIndex);
        if (OffsetFromTableStart < RootTable.GetSize())
        {
            const auto& CachedRes = RootTable.GetResource(OffsetFromTableStart);
            return !CachedRes.IsNull();
        }
    }

    return false;
}


#ifdef DILIGENT_DEVELOPMENT
bool PipelineResourceSignatureD3D12Impl::DvpValidateCommittedResource(const D3DShaderResourceAttribs& D3DAttribs,
                                                                      Uint32                          ResIndex,
                                                                      const ShaderResourceCacheD3D12& ResourceCache,
                                                                      const char*                     ShaderName,
                                                                      const char*                     PSOName) const
{
    const auto& ResDesc    = GetResourceDesc(ResIndex);
    const auto& ResAttribs = GetResourceAttribs(ResIndex);
    VERIFY_EXPR(strcmp(ResDesc.Name, D3DAttribs.Name) == 0);
    VERIFY_EXPR(D3DAttribs.BindCount <= ResDesc.ArraySize);

    if ((ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER) && ResAttribs.IsImmutableSamplerAssigned())
        return true;

    const auto CacheType = ResourceCache.GetContentType();
    VERIFY(CacheType == ResourceCacheContentType::SRB, "Only SRB resource cache can be committed");
    const auto  RootIndex            = ResAttribs.RootIndex(CacheType);
    const auto  OffsetFromTableStart = ResAttribs.OffsetFromTableStart(CacheType);
    const auto& RootTable            = ResourceCache.GetRootTable(RootIndex);

    bool BindingsOK = true;
    for (Uint32 ArrIndex = 0; ArrIndex < D3DAttribs.BindCount; ++ArrIndex)
    {
        if (!IsBound(ArrIndex, ResIndex, ResourceCache))
        {
            LOG_ERROR_MESSAGE("No resource is bound to variable '", GetShaderResourcePrintName(D3DAttribs.Name, D3DAttribs.BindCount, ArrIndex),
                              "' in shader '", ShaderName, "' of PSO '", PSOName, "'.");
            BindingsOK = false;
            continue;
        }

        if (ResAttribs.IsCombinedWithSampler())
        {
            auto& SamplerResDesc = GetResourceDesc(ResAttribs.SamplerInd);
            auto& SamplerAttribs = GetResourceAttribs(ResAttribs.SamplerInd);
            VERIFY_EXPR(SamplerResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);
            VERIFY_EXPR(SamplerResDesc.ArraySize == 1 || SamplerResDesc.ArraySize == ResDesc.ArraySize);
            if (!SamplerAttribs.IsImmutableSamplerAssigned())
            {
                if (ArrIndex < SamplerResDesc.ArraySize)
                {
                    if (!IsBound(ArrIndex, ResAttribs.SamplerInd, ResourceCache))
                    {
                        LOG_ERROR_MESSAGE("No sampler is bound to sampler variable '", GetShaderResourcePrintName(SamplerResDesc, ArrIndex),
                                          "' combined with texture '", D3DAttribs.Name, "' in shader '", ShaderName, "' of PSO '", PSOName, "'.");
                        BindingsOK = false;
                    }
                }
            }
        }

        const auto& CachedRes = RootTable.GetResource(OffsetFromTableStart);

        switch (ResDesc.ResourceType)
        {
            case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
            case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
                // When can use raw cast here because the dynamic type is verified when the resource
                // is bound. It will be null if the type is incorrect.
                if (const auto* pTexViewD3D12 = CachedRes.pObject.RawPtr<TextureViewD3D12Impl>())
                {
                    if (!ValidateResourceViewDimension(D3DAttribs.Name, D3DAttribs.BindCount, ArrIndex, pTexViewD3D12, D3DAttribs.GetResourceDimension(), D3DAttribs.IsMultisample()))
                        BindingsOK = false;
                }
                break;

            case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
                if (ResAttribs.GetD3D12RootParamType() == D3D12_ROOT_PARAMETER_TYPE_CBV)
                {
                    if (const auto* pBuffD3D12 = CachedRes.pObject.RawPtr<BufferD3D12Impl>())
                    {
                        if (pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC)
                            VERIFY_EXPR((ResourceCache.GetDynamicRootBuffersMask() & (Uint64{1} << RootIndex)) != 0);
                        else
                            VERIFY_EXPR((ResourceCache.GetNonDynamicRootBuffersMask() & (Uint64{1} << RootIndex)) != 0);
                    }
                }
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_SRV:
            case SHADER_RESOURCE_TYPE_BUFFER_UAV:
                if (const auto* pBuffViewD3D12 = CachedRes.pObject.RawPtr<BufferViewD3D12Impl>())
                {
                    if (!VerifyBufferViewModeD3D(pBuffViewD3D12, D3DAttribs, ShaderName))
                        BindingsOK = false;

                    if (ResAttribs.GetD3D12RootParamType() == D3D12_ROOT_PARAMETER_TYPE_SRV ||
                        ResAttribs.GetD3D12RootParamType() == D3D12_ROOT_PARAMETER_TYPE_UAV)
                    {
                        const auto* pBuffD3D12 = pBuffViewD3D12->GetBuffer<BufferD3D12Impl>();
                        VERIFY_EXPR(pBuffD3D12 != nullptr);
                        if (pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC)
                            VERIFY_EXPR((ResourceCache.GetDynamicRootBuffersMask() & (Uint64{1} << RootIndex)) != 0);
                        else
                            VERIFY_EXPR((ResourceCache.GetNonDynamicRootBuffersMask() & (Uint64{1} << RootIndex)) != 0);
                    }
                }
                break;

            default:
                //Do nothing
                break;
        }
    }
    return BindingsOK;
}
#endif

} // namespace Diligent
