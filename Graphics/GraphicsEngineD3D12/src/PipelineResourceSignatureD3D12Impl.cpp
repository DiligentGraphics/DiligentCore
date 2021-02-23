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
#include "ShaderResourceCacheD3D12.hpp"
#include "ShaderVariableD3D12.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "ShaderResourceBindingD3D12Impl.hpp"
#include "BufferD3D12Impl.hpp"
#include "BufferViewD3D12Impl.hpp"
#include "SamplerD3D12Impl.hpp"
#include "TextureD3D12Impl.hpp"
#include "TextureViewD3D12Impl.hpp"
#include "TopLevelASD3D12Impl.hpp"
#include "D3D12TypeConversions.hpp"

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

} // namespace


PipelineResourceSignatureD3D12Impl::PipelineResourceSignatureD3D12Impl(IReferenceCounters*                  pRefCounters,
                                                                       RenderDeviceD3D12Impl*               pDevice,
                                                                       const PipelineResourceSignatureDesc& Desc,
                                                                       bool                                 bIsDeviceInternal) :
    TPipelineResourceSignatureBase{pRefCounters, pDevice, Desc, bIsDeviceInternal},
    m_SRBMemAllocator{GetRawAllocator()}
{
    try
    {
        FixedLinearAllocator MemPool{GetRawAllocator()};

        // Reserve at least 1 element because m_pResourceAttribs must hold a pointer to memory
        MemPool.AddSpace<ResourceAttribs>(std::max(1u, Desc.NumResources));
        MemPool.AddSpace<ImmutableSamplerAttribs>(m_Desc.NumImmutableSamplers);

        ReserveSpaceForDescription(MemPool, Desc);

        const auto NumStaticResStages = GetNumStaticResStages();
        if (NumStaticResStages > 0)
        {
            MemPool.AddSpace<ShaderResourceCacheD3D12>(1);
            MemPool.AddSpace<ShaderVariableManagerD3D12>(NumStaticResStages);
        }

        MemPool.Reserve();

        static_assert(std::is_trivially_destructible<ResourceAttribs>::value,
                      "ResourceAttribs objects must be constructed to be properly destructed in case an excpetion is thrown");
        m_pResourceAttribs  = MemPool.Allocate<ResourceAttribs>(std::max(1u, m_Desc.NumResources));
        m_ImmutableSamplers = MemPool.ConstructArray<ImmutableSamplerAttribs>(m_Desc.NumImmutableSamplers);

        // The memory is now owned by PipelineResourceSignatureD3D12Impl and will be freed by Destruct().
        auto* Ptr = MemPool.ReleaseOwnership();
        VERIFY_EXPR(Ptr == m_pResourceAttribs);
        (void)Ptr;

        CopyDescription(MemPool, Desc);

        StaticResCacheTblSizesArrayType StaticResCacheTblSizes = {};
        AllocateRootParameters(StaticResCacheTblSizes);

        if (NumStaticResStages > 0)
        {
            m_pStaticResCache = MemPool.Construct<ShaderResourceCacheD3D12>(CacheContentType::Signature);
            // Constructor of ShaderVariableManagerD3D12 is noexcept, so we can safely construct all manager objects.
            // Moreover, all objects must be constructed if an exception is thrown for Destruct() method to work properly.
            m_StaticVarsMgrs = MemPool.ConstructArray<ShaderVariableManagerD3D12>(NumStaticResStages, std::ref(*this), std::ref(*m_pStaticResCache));

            m_pStaticResCache->Initialize(GetRawAllocator(), static_cast<Uint32>(StaticResCacheTblSizes.size()), StaticResCacheTblSizes.data());

            constexpr SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_STATIC};
            for (Uint32 i = 0; i < m_StaticResStageIndex.size(); ++i)
            {
                auto Idx = m_StaticResStageIndex[i];
                if (Idx >= 0)
                {
                    VERIFY_EXPR(static_cast<Uint32>(Idx) < NumStaticResStages);
                    const auto ShaderType = GetShaderTypeFromPipelineIndex(i, GetPipelineType());
                    m_StaticVarsMgrs[Idx].Initialize(*this, GetRawAllocator(), AllowedVarTypes, _countof(AllowedVarTypes), ShaderType);
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

        auto d3d12RootParamType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(D3D12_ROOT_PARAMETER_TYPE_UAV + 1);
        // Do not allocate resource slot for immutable samplers that are also defined as resource
        if (!(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER && SrcImmutableSamplerInd >= 0))
        {
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

            const auto UseDynamicOffset  = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0;
            const auto IsFormattedBuffer = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0;
            const auto IsArray           = ResDesc.ArraySize != 1;

            d3d12RootParamType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            switch (ResDesc.ResourceType)
            {
                case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
                    VERIFY(!IsFormattedBuffer, "Constant buffers can't be labeled as formatted. This error should've been cuaght by ValidatePipelineResourceSignatureDesc().");
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
    auto& RawAllocator = GetRawAllocator();

    if (m_StaticVarsMgrs != nullptr)
    {
        for (auto Idx : m_StaticResStageIndex)
        {
            if (Idx >= 0)
            {
                m_StaticVarsMgrs[Idx].Destroy(RawAllocator);
                m_StaticVarsMgrs[Idx].~ShaderVariableManagerD3D12();
            }
        }
        m_StaticVarsMgrs = nullptr;
    }

    if (m_pStaticResCache != nullptr)
    {
        m_pStaticResCache->~ShaderResourceCacheD3D12();
        m_pStaticResCache = nullptr;
    }

    if (m_ImmutableSamplers != nullptr)
    {
        for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
        {
            m_ImmutableSamplers[i].~ImmutableSamplerAttribs();
        }
        m_ImmutableSamplers = nullptr;
    }

    if (void* pRawMem = m_pResourceAttribs)
    {
        RawAllocator.Free(pRawMem);
        m_pResourceAttribs = nullptr;
    }

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

void PipelineResourceSignatureD3D12Impl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                     bool                     InitStaticResources)
{
    auto& SRBAllocator     = m_pDevice->GetSRBAllocator();
    auto* pResBindingD3D12 = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingD3D12Impl instance", ShaderResourceBindingD3D12Impl)(this, false);
    if (InitStaticResources)
        pResBindingD3D12->InitializeStaticResources(nullptr);
    pResBindingD3D12->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
}

Uint32 PipelineResourceSignatureD3D12Impl::GetStaticVariableCount(SHADER_TYPE ShaderType) const
{
    return GetStaticVariableCountImpl(ShaderType, m_StaticVarsMgrs);
}

IShaderResourceVariable* PipelineResourceSignatureD3D12Impl::GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name)
{
    return GetStaticVariableByNameImpl(ShaderType, Name, m_StaticVarsMgrs);
}

IShaderResourceVariable* PipelineResourceSignatureD3D12Impl::GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    return GetStaticVariableByIndexImpl(ShaderType, Index, m_StaticVarsMgrs);
}

void PipelineResourceSignatureD3D12Impl::BindStaticResources(Uint32            ShaderFlags,
                                                             IResourceMapping* pResMapping,
                                                             Uint32            Flags)
{
    BindStaticResourcesImpl(ShaderFlags, pResMapping, Flags, m_StaticVarsMgrs);
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

void PipelineResourceSignatureD3D12Impl::InitSRBResourceCache(ShaderResourceCacheD3D12& ResourceCache,
                                                              IMemoryAllocator&         CacheMemAllocator,
                                                              const char*               DbgPipelineName) const
{
    ResourceCache.Initialize(CacheMemAllocator, m_pDevice, m_RootParams);
}

inline void UpdateDynamicRootBuffersCounter(const BufferD3D12Impl*    pOldBuff,
                                            const BufferD3D12Impl*    pNewBuff,
                                            Uint32&                   BuffCounter,
                                            D3D12_ROOT_PARAMETER_TYPE d3d12RootParamType)
{
    if (pOldBuff != nullptr && pOldBuff->GetDesc().Usage == USAGE_DYNAMIC)
    {
        if (d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_CBV)
        {
            // Only count dynamic buffers bound as root views
            VERIFY(BuffCounter > 0, "There is a dynamic root buffer in the resource cache, but dynamic buffers counter is zero");
            --BuffCounter;
        }
        else
        {
            VERIFY_EXPR(pOldBuff->GetD3D12Resource() != nullptr);
        }
    }
    if (pNewBuff != nullptr && pNewBuff->GetDesc().Usage == USAGE_DYNAMIC)
    {
        if (d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_CBV)
        {
            // Only count dynamic buffers bound as root views
            ++BuffCounter;
        }
        else
        {
            DEV_CHECK_ERR(pNewBuff->GetD3D12Resource() != nullptr,
                          "Dynamic constant buffers that don't have backing d3d12 resource must be bound as root views");
        }
    }
}

inline void UpdateDynamicRootBuffersCounter(const BufferViewD3D12Impl* pOldBuffView,
                                            const BufferViewD3D12Impl* pNewBuffView,
                                            Uint32&                    BuffCounter,
                                            D3D12_ROOT_PARAMETER_TYPE  d3d12RootParamType)
{
    if (pOldBuffView != nullptr)
    {
        auto* const pOldBuff = pOldBuffView->GetBuffer<BufferD3D12Impl>();
        if (pOldBuff->GetDesc().Usage == USAGE_DYNAMIC)
        {
            if (d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_SRV || d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_UAV)
            {
                // Only count dynamic buffers bound as root views
                VERIFY(BuffCounter > 0, "There is a dynamic root buffer in the resource cache, but dynamic buffers counter is zero");
                --BuffCounter;
            }
            else
            {
                VERIFY_EXPR(pOldBuff->GetD3D12Resource() != nullptr);
            }
        }
    }
    if (pNewBuffView != nullptr)
    {
        auto* const pNewBuffer = pNewBuffView->GetBuffer<BufferD3D12Impl>();
        if (pNewBuffer->GetDesc().Usage == USAGE_DYNAMIC)
        {
            if (d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_SRV || d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_UAV)
            {
                // Only count dynamic buffers bound as root views
                ++BuffCounter;
            }
            else
            {
                DEV_CHECK_ERR(pNewBuffer->GetD3D12Resource() != nullptr,
                              "Dynamic buffers that don't have backing d3d12 resource must be bound as root views");
            }
        }
    }
}

inline void UpdateDynamicRootBuffersCounter(const TextureViewD3D12Impl*,
                                            const TextureViewD3D12Impl*,
                                            Uint32&,
                                            D3D12_ROOT_PARAMETER_TYPE)
{
}


void PipelineResourceSignatureD3D12Impl::InitializeStaticSRBResources(ShaderResourceCacheD3D12& DstResourceCache) const
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
    VERIFY_EXPR(SrcCacheType == ShaderResourceCacheD3D12::CacheContentType::Signature);
    VERIFY_EXPR(DstCacheType == ShaderResourceCacheD3D12::CacheContentType::SRB);

    auto& DynamicRootBuffersCounter = DstResourceCache.GetDynamicRootBuffersCounter();

    for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
    {
        const auto& ResDesc   = GetResourceDesc(r);
        const auto& Attr      = GetResourceAttribs(r);
        const bool  IsSampler = (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);
        VERIFY_EXPR(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        if (IsSampler && Attr.IsImmutableSamplerAssigned())
            continue;

        const auto  HeapType     = IsSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        const auto  DstRootIndex = Attr.RootIndex(DstCacheType);
        const auto  SrcRootIndex = Attr.RootIndex(SrcCacheType);
        const auto& SrcRootTable = SrcResourceCache.GetRootTable(SrcRootIndex);
        auto&       DstRootTable = DstResourceCache.GetRootTable(DstRootIndex);

        auto SrcCacheOffset = Attr.OffsetFromTableStart(SrcCacheType);
        auto DstCacheOffset = Attr.OffsetFromTableStart(DstCacheType);
        for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd, ++SrcCacheOffset, ++DstCacheOffset)
        {
            const auto& SrcRes = SrcRootTable.GetResource(SrcCacheOffset);
            if (!SrcRes.pObject)
                LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

            auto& DstRes = DstRootTable.GetResource(DstCacheOffset);
            if (DstRes.pObject != SrcRes.pObject)
            {
                DEV_CHECK_ERR(DstRes.pObject == nullptr, "Static resource has already been initialized, and the new resource does not match previously assigned resource.");

                if (SrcRes.Type == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER)
                {
                    UpdateDynamicRootBuffersCounter(DstRes.pObject.RawPtr<const BufferD3D12Impl>(),
                                                    SrcRes.pObject.RawPtr<const BufferD3D12Impl>(),
                                                    DynamicRootBuffersCounter,
                                                    Attr.GetD3D12RootParamType());
                }
                else if (SrcRes.Type == SHADER_RESOURCE_TYPE_BUFFER_SRV || SrcRes.Type == SHADER_RESOURCE_TYPE_BUFFER_UAV)
                {
                    UpdateDynamicRootBuffersCounter(DstRes.pObject.RawPtr<const BufferViewD3D12Impl>(),
                                                    SrcRes.pObject.RawPtr<const BufferViewD3D12Impl>(),
                                                    DynamicRootBuffersCounter,
                                                    Attr.GetD3D12RootParamType());
                }

                DstRes.pObject             = SrcRes.pObject;
                DstRes.Type                = SrcRes.Type;
                DstRes.CPUDescriptorHandle = SrcRes.CPUDescriptorHandle;

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

void PipelineResourceSignatureD3D12Impl::CommitRootViews(ShaderResourceCacheD3D12& ResourceCache,
                                                         CommandContext&           CmdCtx,
                                                         DeviceContextD3D12Impl*   pDeviceCtx,
                                                         Uint32                    DeviceCtxId,
                                                         Uint32                    BaseRootIndex,
                                                         bool                      IsCompute,
                                                         bool                      CommitDynamicBuffers) const
{
    for (Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        const auto& RootView = m_RootParams.GetRootView(rv);
        const auto  RootInd  = RootView.RootIndex;

        auto& Res = ResourceCache.GetRootTable(RootInd).GetResource(0);
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

        auto IsDynamic = pBuffer->GetDesc().Usage == USAGE_DYNAMIC;
        if (IsDynamic != CommitDynamicBuffers)
            continue;

        auto BufferGPUAddress = pBuffer->GetGPUAddress(DeviceCtxId, pDeviceCtx);
        VERIFY_EXPR(BufferGPUAddress != 0);

        auto* pd3d12CmdList = CmdCtx.GetCommandList();
        switch (Res.Type)
        {
            case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
                VERIFY_EXPR(RootView.d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV);
                if (IsCompute)
                    pd3d12CmdList->SetComputeRootConstantBufferView(BaseRootIndex + RootInd, BufferGPUAddress);
                else
                    pd3d12CmdList->SetGraphicsRootConstantBufferView(BaseRootIndex + RootInd, BufferGPUAddress);
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_SRV:
                VERIFY_EXPR(RootView.d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV);
                if (IsCompute)
                    pd3d12CmdList->SetComputeRootShaderResourceView(BaseRootIndex + RootInd, BufferGPUAddress);
                else
                    pd3d12CmdList->SetGraphicsRootShaderResourceView(BaseRootIndex + RootInd, BufferGPUAddress);
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_UAV:
                VERIFY_EXPR(RootView.d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV);
                if (IsCompute)
                    pd3d12CmdList->SetComputeRootUnorderedAccessView(BaseRootIndex + RootInd, BufferGPUAddress);
                else
                    pd3d12CmdList->SetGraphicsRootUnorderedAccessView(BaseRootIndex + RootInd, BufferGPUAddress);
                break;

            default:
                UNEXPECTED("Unexpected root view resource type");
        }
    }
}

void PipelineResourceSignatureD3D12Impl::CommitRootTables(ShaderResourceCacheD3D12& ResourceCache,
                                                          CommandContext&           CmdCtx,
                                                          DeviceContextD3D12Impl*   pDeviceCtx,
                                                          Uint32                    DeviceCtxId,
                                                          bool                      IsCompute,
                                                          Uint32                    BaseRootIndex) const
{
    auto* const pd3d12Device = GetDevice()->GetD3D12Device();

    std::array<DescriptorHeapAllocation, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1> DynamicDescriptorAllocations;
    for (Uint32 heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; heap_type < D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1; ++heap_type)
    {
        const auto d3d12HeapType = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(heap_type);

        auto NumDynamicDescriptors = m_RootParams.GetTotalTableSlots(d3d12HeapType, ROOT_PARAMETER_GROUP_DYNAMIC);
        if (NumDynamicDescriptors > 0)
        {
            auto& Allocation = DynamicDescriptorAllocations[d3d12HeapType];
            Allocation       = CmdCtx.AllocateDynamicGPUVisibleDescriptor(d3d12HeapType, NumDynamicDescriptors);

            DEV_CHECK_ERR(!Allocation.IsNull(),
                          "Failed to allocate ", NumDynamicDescriptors, " dynamic GPU-visible ",
                          (d3d12HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? "CBV/SRV/UAV" : "Sampler"),
                          " descriptor(s). Consider increasing GPUDescriptorHeapDynamicSize[", heap_type,
                          "] in EngineD3D12CreateInfo or optimizing dynamic resource utilization by using static "
                          "or mutable shader resource variables instead.");

            // Copy all all dynamic descriptors from the CPU-only cache allocation
            auto& SrcDynamicAllocation = ResourceCache.GetDescriptorAllocation(d3d12HeapType, ROOT_PARAMETER_GROUP_DYNAMIC);
            VERIFY_EXPR(SrcDynamicAllocation.GetNumHandles() == NumDynamicDescriptors);
            pd3d12Device->CopyDescriptorsSimple(NumDynamicDescriptors, Allocation.GetCpuHandle(), SrcDynamicAllocation.GetCpuHandle(), d3d12HeapType);
        }
    }

    CommandContext::ShaderDescriptorHeaps Heaps{
        ResourceCache.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, ROOT_PARAMETER_GROUP_STATIC_MUTABLE),
        ResourceCache.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, ROOT_PARAMETER_GROUP_STATIC_MUTABLE),
    };
    if (Heaps.pSamplerHeap == nullptr)
        Heaps.pSamplerHeap = DynamicDescriptorAllocations[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetDescriptorHeap();

    if (Heaps.pSrvCbvUavHeap == nullptr)
        Heaps.pSrvCbvUavHeap = DynamicDescriptorAllocations[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].GetDescriptorHeap();

    VERIFY((DynamicDescriptorAllocations[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].IsNull() ||
            DynamicDescriptorAllocations[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].GetDescriptorHeap() == Heaps.pSrvCbvUavHeap),
           "Inconsistent CBV/SRV/UAV descriptor heaps");
    VERIFY((DynamicDescriptorAllocations[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].IsNull() ||
            DynamicDescriptorAllocations[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetDescriptorHeap() == Heaps.pSamplerHeap),
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
            auto& DynamicAllocation      = DynamicDescriptorAllocations[d3d12HeapType];
            RootTableGPUDescriptorHandle = DynamicAllocation.GetGpuHandle(TableOffsetInGroupAllocation);
        }
        else
        {
            RootTableGPUDescriptorHandle = ResourceCache.GetDescriptorTableHandle<D3D12_GPU_DESCRIPTOR_HANDLE>(
                d3d12HeapType, ROOT_PARAMETER_GROUP_STATIC_MUTABLE, RootTable.RootIndex);
            VERIFY(RootTableGPUDescriptorHandle.ptr != 0, "Unexpected null GPU descriptor handle");
        }

        if (IsCompute)
            CmdCtx.GetCommandList()->SetComputeRootDescriptorTable(BaseRootIndex + RootTable.RootIndex, RootTableGPUDescriptorHandle);
        else
            CmdCtx.GetCommandList()->SetGraphicsRootDescriptorTable(BaseRootIndex + RootTable.RootIndex, RootTableGPUDescriptorHandle);
    }

    // Commit non-dynamic root buffer views
    constexpr auto CommitDynamicBuffers = false;
    CommitRootViews(ResourceCache, CmdCtx, pDeviceCtx,
                    DeviceCtxId, BaseRootIndex, IsCompute, CommitDynamicBuffers);
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
                    ResDesc.ArraySize //
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
                    SampAttr.ArraySize //
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

struct BindResourceHelper
{
    ShaderResourceCacheD3D12::Resource&                        DstRes;
    const PipelineResourceDesc&                                ResDesc;
    const PipelineResourceSignatureD3D12Impl::ResourceAttribs& Attribs;
    const Uint32                                               ArrayIndex;
    D3D12_CPU_DESCRIPTOR_HANDLE                                DstTableCPUDescriptorHandle;
    PipelineResourceSignatureD3D12Impl const&                  Signature;
    ShaderResourceCacheD3D12&                                  ResourceCache;

    void BindResource(IDeviceObject* pObj) const;

private:
    void CacheCB(IDeviceObject* pBuffer) const;
    void CacheSampler(IDeviceObject* pBuffer) const;
    void CacheAccelStruct(IDeviceObject* pBuffer) const;

    template <typename TResourceViewType,    ///< ResType of the view (ITextureViewD3D12 or IBufferViewD3D12)
              typename TViewTypeEnum,        ///< ResType of the expected view type enum (TEXTURE_VIEW_TYPE or BUFFER_VIEW_TYPE)
              typename TBindSamplerProcType> ///< ResType of the procedure to set sampler
    void CacheResourceView(IDeviceObject*       pBufferView,
                           TViewTypeEnum        dbgExpectedViewType,
                           TBindSamplerProcType BindSamplerProc) const;

    ID3D12Device* GetD3D12Device() const { return Signature.GetDevice()->GetD3D12Device(); }
};


void BindResourceHelper::CacheCB(IDeviceObject* pBuffer) const
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferD3D12Impl> pBuffD3D12{pBuffer, IID_BufferD3D12};
#ifdef DILIGENT_DEVELOPMENT
    VerifyConstantBufferBinding(ResDesc.Name, ResDesc.ArraySize, ResDesc.VarType, ResDesc.Flags, ArrayIndex,
                                pBuffer, pBuffD3D12.RawPtr(), DstRes.pObject.RawPtr());
    if (ResDesc.ArraySize != 1 && pBuffD3D12 && pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC && pBuffD3D12->GetD3D12Resource() == nullptr)
    {
        LOG_ERROR_MESSAGE("Attempting to bind dynamic buffer '", pBuffD3D12->GetDesc().Name, "' that doesn't have backing d3d12 resource to array variable '", ResDesc.Name,
                          "[", ResDesc.ArraySize, "]', which is currently not supported in Direct3D12 backend. Either use non-array variable, or bind non-dynamic buffer.");
    }
#endif
    if (pBuffD3D12)
    {
        if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        DstRes.Type                = ResDesc.ResourceType;
        DstRes.CPUDescriptorHandle = pBuffD3D12->GetCBVHandle();
        VERIFY(DstRes.CPUDescriptorHandle.ptr != 0 || pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC, "No relevant CBV CPU descriptor handle");

        if (DstTableCPUDescriptorHandle.ptr != 0)
        {
            VERIFY(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC || DstRes.pObject == nullptr,
                   "Static and mutable resource descriptors must be copied only once");
            VERIFY_EXPR(DstRes.CPUDescriptorHandle.ptr != 0);

            GetD3D12Device()->CopyDescriptorsSimple(1, DstTableCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        UpdateDynamicRootBuffersCounter(DstRes.pObject.RawPtr<const BufferD3D12Impl>(), pBuffD3D12, ResourceCache.GetDynamicRootBuffersCounter(), Attribs.GetD3D12RootParamType());

        DstRes.pObject = std::move(pBuffD3D12);
    }
}

void BindResourceHelper::CacheSampler(IDeviceObject* pSampler) const
{
    RefCntAutoPtr<ISamplerD3D12> pSamplerD3D12{pSampler, IID_SamplerD3D12};
    if (pSamplerD3D12)
    {
        if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            if (DstRes.pObject != pSampler)
            {
                auto VarTypeStr = GetShaderVariableTypeLiteralName(ResDesc.VarType);
                LOG_ERROR_MESSAGE("Non-null sampler is already bound to ", VarTypeStr, " shader variable '", GetShaderResourcePrintName(ResDesc, ArrayIndex),
                                  "'. Attempting to bind another sampler is an error and will be ignored. ",
                                  "Use another shader resource binding instance or label the variable as dynamic.");
            }

            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        DstRes.Type = SHADER_RESOURCE_TYPE_SAMPLER;

        DstRes.CPUDescriptorHandle = pSamplerD3D12->GetCPUDescriptorHandle();
        VERIFY(DstRes.CPUDescriptorHandle.ptr != 0, "No relevant D3D12 sampler descriptor handle");

        if (DstTableCPUDescriptorHandle.ptr != 0)
        {
            VERIFY(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC || DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");
            GetD3D12Device()->CopyDescriptorsSimple(1, DstTableCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        }
        else
        {
            VERIFY(ResourceCache.GetContentType() == ShaderResourceCacheD3D12::CacheContentType::Signature,
                   "Samplers must always be allocated in root tables and thus assigned descriptor");
        }

        DstRes.pObject = std::move(pSamplerD3D12);
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to bind object '", pSampler->GetDesc().Name, "' to variable '", GetShaderResourcePrintName(ResDesc, ArrayIndex), "'."
                                                                                                                                                   "Incorect object type: sampler is expected.");
    }
}

void BindResourceHelper::CacheAccelStruct(IDeviceObject* pTLAS) const
{
    RefCntAutoPtr<ITopLevelASD3D12> pTLASD3D12{pTLAS, IID_TopLevelASD3D12};
    if (pTLASD3D12)
    {
        if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        DstRes.Type                = SHADER_RESOURCE_TYPE_ACCEL_STRUCT;
        DstRes.CPUDescriptorHandle = pTLASD3D12->GetCPUDescriptorHandle();

        VERIFY(DstRes.CPUDescriptorHandle.ptr != 0, "No relevant D3D12 resource");
        if (DstTableCPUDescriptorHandle.ptr != 0)
        {
            VERIFY(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC || DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");
            GetD3D12Device()->CopyDescriptorsSimple(1, DstTableCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
        else
        {
            VERIFY(ResourceCache.GetContentType() == ShaderResourceCacheD3D12::CacheContentType::Signature,
                   "Acceleration structures are always allocated in root tables and thus must have a descriptor");
        }

        DstRes.pObject = std::move(pTLASD3D12);
    }
}

template <typename TResourceViewType>
struct ResourceViewTraits
{};

template <>
struct ResourceViewTraits<TextureViewD3D12Impl>
{
    static const INTERFACE_ID& IID;

    static bool VerifyView(TextureViewD3D12Impl* pViewD3D12, const PipelineResourceDesc& ResDesc)
    {
        return true;
    }
};
const INTERFACE_ID& ResourceViewTraits<TextureViewD3D12Impl>::IID = IID_TextureViewD3D12;

template <>
struct ResourceViewTraits<BufferViewD3D12Impl>
{
    static const INTERFACE_ID& IID;

    static bool VerifyView(BufferViewD3D12Impl* pViewD3D12, const PipelineResourceDesc& ResDesc)
    {
        if (pViewD3D12 != nullptr)
        {
            auto* pBuffer = pViewD3D12->GetBuffer<BufferD3D12Impl>();
            if (ResDesc.ArraySize != 1 && pBuffer->GetDesc().Usage == USAGE_DYNAMIC && pBuffer->GetD3D12Resource() == nullptr)
            {
                LOG_ERROR_MESSAGE("Attempting to bind dynamic buffer '", pBuffer->GetDesc().Name, "' that doesn't have backing d3d12 resource to array variable '", ResDesc.Name,
                                  "[", ResDesc.ArraySize, "]', which is currently not supported in Direct3D12 backend. Either use non-array variable, or bind non-dynamic buffer.");
                return false;
            }
        }

        return true;
        //return VerifyBufferViewModeD3D(pViewD3D12, Attribs);
    }
};
const INTERFACE_ID& ResourceViewTraits<BufferViewD3D12Impl>::IID = IID_BufferViewD3D12;


template <typename TResourceViewType,    ///< ResType of the view (ITextureViewD3D12 or IBufferViewD3D12)
          typename TViewTypeEnum,        ///< ResType of the expected view type enum (TEXTURE_VIEW_TYPE or BUFFER_VIEW_TYPE)
          typename TBindSamplerProcType> ///< ResType of the procedure to set sampler
void BindResourceHelper::CacheResourceView(IDeviceObject*       pView,
                                           TViewTypeEnum        dbgExpectedViewType,
                                           TBindSamplerProcType BindSamplerProc) const
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<TResourceViewType> pViewD3D12{pView, ResourceViewTraits<TResourceViewType>::IID};
#ifdef DILIGENT_DEVELOPMENT
    VerifyResourceViewBinding(ResDesc.Name, ResDesc.ArraySize, ResDesc.VarType, ArrayIndex,
                              pView, pViewD3D12.RawPtr(),
                              {dbgExpectedViewType}, RESOURCE_DIM_UNDEFINED,
                              false, // IsMultisample
                              DstRes.pObject.RawPtr());
    ResourceViewTraits<TResourceViewType>::VerifyView(pViewD3D12, ResDesc);
#endif
    if (pViewD3D12)
    {
        if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        DstRes.Type                = ResDesc.ResourceType;
        DstRes.CPUDescriptorHandle = pViewD3D12->GetCPUDescriptorHandle();
        VERIFY(DstRes.CPUDescriptorHandle.ptr != 0, "No relevant D3D12 view");

        if (DstTableCPUDescriptorHandle.ptr != 0)
        {
            VERIFY(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC || DstRes.pObject == nullptr,
                   "Static and mutable resource descriptors must be copied only once");

            GetD3D12Device()->CopyDescriptorsSimple(1, DstTableCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        UpdateDynamicRootBuffersCounter(DstRes.pObject.RawPtr<const TResourceViewType>(), pViewD3D12, ResourceCache.GetDynamicRootBuffersCounter(), Attribs.GetD3D12RootParamType());

        BindSamplerProc(pViewD3D12);

        DstRes.pObject = std::move(pViewD3D12);
    }
}

void BindResourceHelper::BindResource(IDeviceObject* pObj) const
{
    VERIFY_EXPR(ArrayIndex < ResDesc.ArraySize);

#ifdef DILIGENT_DEBUG
    using CacheContentType = PipelineResourceSignatureD3D12Impl::CacheContentType;

    if (ResourceCache.GetContentType() == CacheContentType::Signature)
    {
        VERIFY(DstTableCPUDescriptorHandle.ptr == 0, "Static shader resources should never be assigned descriptor space.");
    }
    else if (ResourceCache.GetContentType() == CacheContentType::SRB)
    {
        if (Attribs.GetD3D12RootParamType() == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            VERIFY(DstTableCPUDescriptorHandle.ptr != 0, "Shader resources allocated in descriptor tables must be assigned descriptor space.");
        }
        else
        {
            VERIFY((ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER ||
                    ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV ||
                    ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV),
                   "Only constant buffers and dynamic buffers views can be allocated as root views");
            VERIFY(DstTableCPUDescriptorHandle.ptr == 0, "Resources allocated as root views should never be assigned descriptor space.");
        }
    }
    else
    {
        UNEXPECTED("Unknown content type");
    }
#endif

    if (pObj)
    {
        static_assert(SHADER_RESOURCE_TYPE_LAST == 8, "Please update this function to handle the new resource type");
        switch (ResDesc.ResourceType)
        {
            case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
                CacheCB(pObj);
                break;

            case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
                CacheResourceView<TextureViewD3D12Impl>(
                    pObj, TEXTURE_VIEW_SHADER_RESOURCE,
                    [&](TextureViewD3D12Impl* pTexView) //
                    {
                        if (Attribs.IsCombinedWithSampler())
                        {
                            auto& SamplerResDesc = Signature.GetResourceDesc(Attribs.SamplerInd);
                            auto& SamplerAttribs = Signature.GetResourceAttribs(Attribs.SamplerInd);
                            VERIFY_EXPR(SamplerResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);

                            if (!SamplerAttribs.IsImmutableSamplerAssigned())
                            {
                                auto* pSampler = pTexView->GetSampler();
                                if (pSampler)
                                {
                                    VERIFY_EXPR(ResDesc.ArraySize == SamplerResDesc.ArraySize || SamplerResDesc.ArraySize == 1);
                                    const auto CacheType            = ResourceCache.GetContentType();
                                    const auto SamplerArrInd        = SamplerResDesc.ArraySize > 1 ? ArrayIndex : 0;
                                    const auto RootIndex            = SamplerAttribs.RootIndex(CacheType);
                                    const auto OffsetFromTableStart = SamplerAttribs.OffsetFromTableStart(CacheType) + SamplerArrInd;
                                    auto&      SampleDstRes         = ResourceCache.GetRootTable(RootIndex).GetResource(OffsetFromTableStart);

                                    D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle{};
                                    if (CacheType != ShaderResourceCacheD3D12::CacheContentType::Signature)
                                    {
                                        const auto RootParamGroup = VariableTypeToRootParameterGroup(SamplerResDesc.VarType);
                                        // Static/mutable resources are allocated in GPU-visible descriptor heap, while dynamic resources - in CPU-only heap.
                                        CPUDescriptorHandle = ResourceCache.GetDescriptorTableHandle<D3D12_CPU_DESCRIPTOR_HANDLE>(
                                            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, RootParamGroup, RootIndex, OffsetFromTableStart);
                                    }

                                    BindResourceHelper SeparateSampler{
                                        SampleDstRes,
                                        SamplerResDesc,
                                        SamplerAttribs,
                                        SamplerArrInd,
                                        CPUDescriptorHandle,
                                        Signature,
                                        ResourceCache};
                                    SeparateSampler.BindResource(pSampler);
                                }
                                else
                                {
                                    LOG_ERROR_MESSAGE("Failed to bind sampler to variable '", SamplerResDesc.Name, ". Sampler is not set in the texture view '", pTexView->GetDesc().Name, '\'');
                                }
                            }
                        }
                    });
                break;

            case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
                CacheResourceView<TextureViewD3D12Impl>(pObj, TEXTURE_VIEW_UNORDERED_ACCESS, [](TextureViewD3D12Impl*) {});
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_SRV:
                CacheResourceView<BufferViewD3D12Impl>(pObj, BUFFER_VIEW_SHADER_RESOURCE, [](BufferViewD3D12Impl*) {});
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_UAV:
                CacheResourceView<BufferViewD3D12Impl>(pObj, BUFFER_VIEW_UNORDERED_ACCESS, [](BufferViewD3D12Impl*) {});
                break;

            case SHADER_RESOURCE_TYPE_SAMPLER:
                //DEV_CHECK_ERR(Signature.IsUsingSeparateSamplers(), "Samplers should not be set directly when using combined texture samplers");
                CacheSampler(pObj);
                break;

            case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
                CacheAccelStruct(pObj);
                break;

            default: UNEXPECTED("Unknown resource type ", static_cast<Int32>(ResDesc.ResourceType));
        }
    }
    else
    {
        if (DstRes.pObject != nullptr && ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            LOG_ERROR_MESSAGE("Shader variable '", ResDesc.Name, "' is not dynamic but is being reset to null. This is an error and may cause unpredicted behavior. ",
                              "Use another shader resource binding instance or label the variable as dynamic if you need to bind another resource.");

        DstRes = ShaderResourceCacheD3D12::Resource{};
        if (Attribs.IsCombinedWithSampler())
        {
            auto& SamplerResDesc = Signature.GetResourceDesc(Attribs.SamplerInd);
            auto& SamplerAttribs = Signature.GetResourceAttribs(Attribs.SamplerInd);
            VERIFY_EXPR(SamplerResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);

            if (!SamplerAttribs.IsImmutableSamplerAssigned())
            {
                const auto CacheType            = ResourceCache.GetContentType();
                auto       SamplerArrInd        = SamplerResDesc.ArraySize > 1 ? ArrayIndex : 0;
                const auto RootIndex            = SamplerAttribs.RootIndex(CacheType);
                const auto OffsetFromTableStart = SamplerAttribs.OffsetFromTableStart(CacheType) + SamplerArrInd;
                auto&      DstSam               = ResourceCache.GetRootTable(RootIndex).GetResource(OffsetFromTableStart);

                if (DstSam.pObject != nullptr && SamplerResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                    LOG_ERROR_MESSAGE("Sampler variable '", SamplerResDesc.Name, "' is not dynamic but is being reset to null. This is an error and may cause unpredicted behavior. ",
                                      "Use another shader resource binding instance or label the variable as dynamic if you need to bind another sampler.");

                DstSam = ShaderResourceCacheD3D12::Resource{};
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
    const auto& ResDesc              = GetResourceDesc(ResIndex);
    const auto& Attribs              = GetResourceAttribs(ResIndex);
    const bool  IsSampler            = (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);
    const auto  CacheType            = ResourceCache.GetContentType();
    const auto  RootIndex            = Attribs.RootIndex(CacheType);
    const auto  OffsetFromTableStart = Attribs.OffsetFromTableStart(CacheType) + ArrayIndex;

    VERIFY_EXPR(ArrayIndex < ResDesc.ArraySize);

    auto& RootTable = ResourceCache.GetRootTable(RootIndex);
    auto& DstRes    = RootTable.GetResource(OffsetFromTableStart);

    D3D12_CPU_DESCRIPTOR_HANDLE DstTableCPUDescriptorHandle{};
    if (CacheType != ShaderResourceCacheD3D12::CacheContentType::Signature && !Attribs.IsRootView())
    {
        const auto RootParamGroup = VariableTypeToRootParameterGroup(ResDesc.VarType);
        // Static/mutable resources are allocated in GPU-visible descriptor heap, while dynamic resources - in CPU-only heap.
        DstTableCPUDescriptorHandle =
            ResourceCache.GetDescriptorTableHandle<D3D12_CPU_DESCRIPTOR_HANDLE>(
                IsSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                RootParamGroup, RootIndex, OffsetFromTableStart);
    }

    BindResourceHelper Helper{
        DstRes,
        ResDesc,
        Attribs,
        ArrayIndex,
        DstTableCPUDescriptorHandle,
        *this,
        ResourceCache};

    Helper.BindResource(pObj);
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
                                                                      const char*                     PSOName,
                                                                      Uint32&                         DynamicRootBuffersCounter) const
{
    const auto& ResDesc    = GetResourceDesc(ResIndex);
    const auto& ResAttribs = GetResourceAttribs(ResIndex);
    VERIFY_EXPR(strcmp(ResDesc.Name, D3DAttribs.Name) == 0);
    VERIFY_EXPR(D3DAttribs.BindCount <= ResDesc.ArraySize);

    if ((ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER) && ResAttribs.IsImmutableSamplerAssigned())
        return true;

    const auto  CacheType            = ResourceCache.GetContentType();
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
                            ++DynamicRootBuffersCounter;
                    }
                }
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_SRV:
            case SHADER_RESOURCE_TYPE_BUFFER_UAV:
                if (ResAttribs.GetD3D12RootParamType() == D3D12_ROOT_PARAMETER_TYPE_SRV ||
                    ResAttribs.GetD3D12RootParamType() == D3D12_ROOT_PARAMETER_TYPE_UAV)
                {
                    if (const auto* pBuffViewD3D12 = CachedRes.pObject.RawPtr<BufferViewD3D12Impl>())
                    {
                        const auto* pBuffD3D12 = pBuffViewD3D12->GetBuffer<BufferD3D12Impl>();
                        if (pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC)
                            ++DynamicRootBuffersCounter;
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
