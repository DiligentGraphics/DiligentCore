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
#ifdef DILIGENT_DEBUG
            m_pStaticResCache->GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_SRV], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);
            m_pStaticResCache->GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_UAV], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);
            m_pStaticResCache->GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_CBV).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_CBV], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);
            m_pStaticResCache->GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, false);
#endif

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

            auto CacheTableSizes = GetCacheTableSizes();
            auto CacheMemorySize = ShaderResourceCacheD3D12::GetRequiredMemorySize(static_cast<Uint32>(CacheTableSizes.size()), CacheTableSizes.data());
            m_SRBMemAllocator.Initialize(m_Desc.SRBAllocationGranularity, GetNumActiveShaderStages(), ShaderVariableDataSizes.data(), 1, &CacheMemorySize);
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
            const auto SrcImmutableSamplerInd = FindImmutableSampler(m_Desc.ImmutableSamplers, m_Desc.NumImmutableSamplers, ResDesc.ShaderStages,
                                                                     ResDesc.Name, GetCombinedSamplerSuffix());
            if (SrcImmutableSamplerInd >= 0)
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

std::vector<Uint32, STDAllocatorRawMem<Uint32>> PipelineResourceSignatureD3D12Impl::GetCacheTableSizes() const
{
    // Get root table size for every root index
    // m_RootParams keeps root tables sorted by the array index, not the root index
    // Root views are treated as one-descriptor tables
    std::vector<Uint32, STDAllocatorRawMem<Uint32>> CacheTableSizes(m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews(), 0, STD_ALLOCATOR_RAW_MEM(Uint32, GetRawAllocator(), "Allocator for vector<Uint32>"));
    for (Uint32 rt = 0; rt < m_RootParams.GetNumRootTables(); ++rt)
    {
        const auto& RootParam = m_RootParams.GetRootTable(rt);
        VERIFY(CacheTableSizes[RootParam.RootIndex] == 0, "Cache table at index ", RootParam.RootIndex,
               " has already been initialized. This is a bug as each root index must be used only once.");
        CacheTableSizes[RootParam.RootIndex] = RootParam.GetDescriptorTableSize();
    }

    for (Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        const auto& RootParam = m_RootParams.GetRootView(rv);
        VERIFY(CacheTableSizes[RootParam.RootIndex] == 0, "Cache table at index ", RootParam.RootIndex,
               " has already been initialized. This is a bug as each root index must be used only once.");
        CacheTableSizes[RootParam.RootIndex] = 1;
    }

    return CacheTableSizes;
}

void PipelineResourceSignatureD3D12Impl::InitSRBResourceCache(ShaderResourceCacheD3D12& ResourceCache,
                                                              IMemoryAllocator&         CacheMemAllocator,
                                                              const char*               DbgPipelineName) const
{
    const auto CacheTableSizes = GetCacheTableSizes();

    // Initialize resource cache to hold root tables
    ResourceCache.Initialize(CacheMemAllocator, static_cast<Uint32>(CacheTableSizes.size()), CacheTableSizes.data());

    // Allocate space in GPU-visible descriptor heap for static and mutable variables only
    Uint32 TotalSrvCbvUavDescriptors = m_RootParams.GetTotalSrvCbvUavSlots(ROOT_PARAMETER_GROUP_STATIC_MUTABLE);
    Uint32 TotalSamplerDescriptors   = m_RootParams.GetTotalSamplerSlots(ROOT_PARAMETER_GROUP_STATIC_MUTABLE);

    DescriptorHeapAllocation CbcSrvUavHeapSpace, SamplerHeapSpace;
    if (TotalSrvCbvUavDescriptors > 0)
    {
        CbcSrvUavHeapSpace = GetDevice()->AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, TotalSrvCbvUavDescriptors);
        DEV_CHECK_ERR(!CbcSrvUavHeapSpace.IsNull(),
                      "Failed to allocate ", TotalSrvCbvUavDescriptors, " GPU-visible CBV/SRV/UAV descriptor",
                      (TotalSrvCbvUavDescriptors > 1 ? "s" : ""),
                      ". Consider increasing GPUDescriptorHeapSize[0] in EngineD3D12CreateInfo.");
    }
    VERIFY_EXPR(TotalSrvCbvUavDescriptors == 0 && CbcSrvUavHeapSpace.IsNull() || CbcSrvUavHeapSpace.GetNumHandles() == TotalSrvCbvUavDescriptors);

    if (TotalSamplerDescriptors > 0)
    {
        SamplerHeapSpace = GetDevice()->AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, TotalSamplerDescriptors);
        DEV_CHECK_ERR(!SamplerHeapSpace.IsNull(),
                      "Failed to allocate ", TotalSamplerDescriptors, " GPU-visible Sampler descriptor",
                      (TotalSamplerDescriptors > 1 ? "s" : ""),
                      ". Consider using immutable samplers in the Pipeline State Object or "
                      "increasing GPUDescriptorHeapSize[1] in EngineD3D12CreateInfo.");
    }
    VERIFY_EXPR(TotalSamplerDescriptors == 0 && SamplerHeapSpace.IsNull() || SamplerHeapSpace.GetNumHandles() == TotalSamplerDescriptors);

    // Iterate through all root static/mutable tables and assign start offsets. The tables are tightly packed, so
    // start offset of table N+1 is start offset of table N plus the size of table N.
    // Root tables with dynamic resources as well as root views are not assigned space in GPU-visible allocation
    // (root views are simply not processed)
    Uint32 SrvCbvUavTblStartOffset = 0;
    Uint32 SamplerTblStartOffset   = 0;
    for (Uint32 rt = 0; rt < m_RootParams.GetNumRootTables(); ++rt)
    {
        const auto& RootParam      = m_RootParams.GetRootTable(rt);
        const auto& d3d12RootParam = RootParam.d3d12RootParam;
        auto&       RootTableCache = ResourceCache.GetRootTable(RootParam.RootIndex);
        const bool  IsDynamic      = RootParam.Group == ROOT_PARAMETER_GROUP_DYNAMIC;

        VERIFY_EXPR(d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);

        const auto TableSize = RootParam.GetDescriptorTableSize();
        VERIFY(TableSize > 0, "Unexpected empty descriptor table");

        const auto HeapType = D3D12DescriptorRangeTypeToD3D12HeapType(d3d12RootParam.DescriptorTable.pDescriptorRanges[0].RangeType);

#ifdef DILIGENT_DEBUG
        RootTableCache.SetDebugAttribs(TableSize, HeapType, IsDynamic);
#endif

        if (!IsDynamic)
        {
            if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
            {
                RootTableCache.m_TableStartOffset = SrvCbvUavTblStartOffset;
                SrvCbvUavTblStartOffset += TableSize;
            }
            else
            {
                RootTableCache.m_TableStartOffset = SamplerTblStartOffset;
                SamplerTblStartOffset += TableSize;
            }
        }
        else
        {
            // Space for dynamic variables is allocated at every commit
            VERIFY_EXPR(RootTableCache.m_TableStartOffset == ShaderResourceCacheD3D12::InvalidDescriptorOffset);
        }
    }
    VERIFY_EXPR(SrvCbvUavTblStartOffset == TotalSrvCbvUavDescriptors);
    VERIFY_EXPR(SamplerTblStartOffset == TotalSamplerDescriptors);

#ifdef DILIGENT_DEBUG
    for (Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        const auto& RootParam      = m_RootParams.GetRootView(rv);
        const auto& d3d12RootParam = RootParam.d3d12RootParam;
        auto&       RootTableCache = ResourceCache.GetRootTable(RootParam.RootIndex);
        const bool  IsDynamic      = RootParam.Group == ROOT_PARAMETER_GROUP_DYNAMIC;

        // Root views are not assigned valid table start offset
        VERIFY_EXPR(RootTableCache.m_TableStartOffset == ShaderResourceCacheD3D12::InvalidDescriptorOffset);

        VERIFY_EXPR((d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ||
                     d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ||
                     d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV));
        RootTableCache.SetDebugAttribs(1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, IsDynamic);
    }
#endif

    ResourceCache.SetDescriptorHeapSpace(std::move(CbcSrvUavHeapSpace), std::move(SamplerHeapSpace));
}

inline void UpdateDynamicBuffersCounter(const BufferD3D12Impl*    pOldBuff,
                                        const BufferD3D12Impl*    pNewBuff,
                                        Uint32&                   BuffCounter,
                                        D3D12_ROOT_PARAMETER_TYPE d3d12RootParamType)
{
    if (pOldBuff != nullptr && pOldBuff->GetDesc().Usage == USAGE_DYNAMIC)
    {
        VERIFY_EXPR(d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_CBV || pOldBuff->GetD3D12Resource() != nullptr);
        VERIFY(BuffCounter > 0, "There is a dynamic root buffer in the resource cache, but dynamic buffers counter is zero");
        --BuffCounter;
    }
    if (pNewBuff != nullptr && pNewBuff->GetDesc().Usage == USAGE_DYNAMIC)
    {
        DEV_CHECK_ERR(d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_CBV || pNewBuff->GetD3D12Resource() != nullptr,
                      "Dynamic constant buffers that don't have backing d3d12 resource must be bound as root views");
        ++BuffCounter;
    }
}

inline void UpdateDynamicBuffersCounter(const BufferViewD3D12Impl* pOldBuffView,
                                        const BufferViewD3D12Impl* pNewBuffView,
                                        Uint32&                    BuffCounter,
                                        D3D12_ROOT_PARAMETER_TYPE  d3d12RootParamType)
{
    if (pOldBuffView != nullptr)
    {
        auto* const pOldBuff = pOldBuffView->GetBuffer<BufferD3D12Impl>();
        if (pOldBuff->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY_EXPR(d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_SRV || d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_UAV || pOldBuff->GetD3D12Resource() != nullptr);
            VERIFY(BuffCounter > 0, "There is a dynamic root buffer in the resource cache, but dynamic buffers counter is zero");
            --BuffCounter;
        }
    }
    if (pNewBuffView != nullptr)
    {
        auto* const pNewBuffer = pNewBuffView->GetBuffer<BufferD3D12Impl>();
        if (pNewBuffer->GetDesc().Usage == USAGE_DYNAMIC)
        {
            DEV_CHECK_ERR(d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_SRV || d3d12RootParamType == D3D12_ROOT_PARAMETER_TYPE_UAV || pNewBuffer->GetD3D12Resource() != nullptr,
                          "Dynamic buffers that don't have backing d3d12 resource must be bound as root views");
            ++BuffCounter;
        }
    }
}

inline void UpdateDynamicBuffersCounter(const TextureViewD3D12Impl*,
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

    auto& DstBoundDynamicCBsCounter = DstResourceCache.GetDynamicRootBuffersCounter();

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
        const auto& SrcRootTable = SrcResourceCache.GetRootTable(Attr.RootIndex(SrcCacheType));
        auto&       DstRootTable = DstResourceCache.GetRootTable(DstRootIndex);

        for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
        {
            const auto SrcCacheOffset = Attr.OffsetFromTableStart(SrcCacheType) + ArrInd;
            const auto DstCacheOffset = Attr.OffsetFromTableStart(DstCacheType) + ArrInd;

            const auto& SrcRes = SrcRootTable.GetResource(SrcCacheOffset, HeapType);
            if (!SrcRes.pObject)
                LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

            auto& DstRes = DstRootTable.GetResource(DstCacheOffset, HeapType);
            if (DstRes.pObject != SrcRes.pObject)
            {
                DEV_CHECK_ERR(DstRes.pObject == nullptr, "Static resource has already been initialized, and the new resource does not match previously assigned resource.");

                if (SrcRes.Type == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER)
                {
                    UpdateDynamicBuffersCounter(DstRes.pObject.RawPtr<const BufferD3D12Impl>(),
                                                SrcRes.pObject.RawPtr<const BufferD3D12Impl>(),
                                                DstBoundDynamicCBsCounter,
                                                Attr.GetD3D12RootParamType());
                }
                else if (SrcRes.Type == SHADER_RESOURCE_TYPE_BUFFER_SRV || SrcRes.Type == SHADER_RESOURCE_TYPE_BUFFER_UAV)
                {
                    UpdateDynamicBuffersCounter(DstRes.pObject.RawPtr<const BufferViewD3D12Impl>(),
                                                SrcRes.pObject.RawPtr<const BufferViewD3D12Impl>(),
                                                DstBoundDynamicCBsCounter,
                                                Attr.GetD3D12RootParamType());
                }

                DstRes.pObject             = SrcRes.pObject;
                DstRes.Type                = SrcRes.Type;
                DstRes.CPUDescriptorHandle = SrcRes.CPUDescriptorHandle;

                if (IsSampler)
                {
                    auto DstHandle = DstResourceCache.CopyDescriptors<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(d3d12Device, SrcRes.CPUDescriptorHandle, 1, DstRootIndex, DstCacheOffset);
                    VERIFY_EXPR(DstHandle.ptr != 0);
                }
                else
                {
                    auto DstHandle = DstResourceCache.CopyDescriptors<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(d3d12Device, SrcRes.CPUDescriptorHandle, 1, DstRootIndex, DstCacheOffset);
                    VERIFY_EXPR(DstHandle.ptr != 0 || Attr.IsRootView());
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
        auto& RootView = m_RootParams.GetRootView(rv);
        auto  RootInd  = RootView.RootIndex;

        auto& Res = ResourceCache.GetRootTable(RootInd).GetResource(0, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        if (Res.Type == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER)
        {
            if (auto* pBuff = Res.pObject.RawPtr<BufferD3D12Impl>())
            {
                bool IsDynamic = pBuff->GetDesc().Usage == USAGE_DYNAMIC;
                if (IsDynamic == CommitDynamicBuffers)
                {
                    auto CBVAddress = pBuff->GetGPUAddress(DeviceCtxId, pDeviceCtx);
                    if (IsCompute)
                        CmdCtx.GetCommandList()->SetComputeRootConstantBufferView(BaseRootIndex + RootInd, CBVAddress);
                    else
                        CmdCtx.GetCommandList()->SetGraphicsRootConstantBufferView(BaseRootIndex + RootInd, CBVAddress);
                }
            }
        }
        else if (Res.Type == SHADER_RESOURCE_TYPE_BUFFER_SRV ||
                 Res.Type == SHADER_RESOURCE_TYPE_BUFFER_UAV)
        {
            if (auto* pBuffView = Res.pObject.RawPtr<BufferViewD3D12Impl>())
            {
                auto* pBuffer   = pBuffView->GetBuffer<BufferD3D12Impl>();
                bool  IsDynamic = pBuffer->GetDesc().Usage == USAGE_DYNAMIC;
                if (IsDynamic == CommitDynamicBuffers)
                {
                    auto SRVAddress = pBuffer->GetGPUAddress(DeviceCtxId, pDeviceCtx);
                    if (IsCompute)
                        CmdCtx.GetCommandList()->SetComputeRootShaderResourceView(BaseRootIndex + RootInd, SRVAddress);
                    else
                        CmdCtx.GetCommandList()->SetGraphicsRootShaderResourceView(BaseRootIndex + RootInd, SRVAddress);
                }
            }
        }
        else
        {
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
    auto* pd3d12Device = GetDevice()->GetD3D12Device();

    Uint32 NumDynamicCbvSrvUavDescriptors = m_RootParams.GetTotalSrvCbvUavSlots(ROOT_PARAMETER_GROUP_DYNAMIC);
    Uint32 NumDynamicSamplerDescriptors   = m_RootParams.GetTotalSamplerSlots(ROOT_PARAMETER_GROUP_DYNAMIC);

    DescriptorHeapAllocation DynamicCbvSrvUavDescriptors, DynamicSamplerDescriptors;
    if (NumDynamicCbvSrvUavDescriptors > 0)
    {
        DynamicCbvSrvUavDescriptors = CmdCtx.AllocateDynamicGPUVisibleDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NumDynamicCbvSrvUavDescriptors);
        DEV_CHECK_ERR(DynamicCbvSrvUavDescriptors.GetDescriptorHeap() != nullptr,
                      "Failed to allocate ", NumDynamicCbvSrvUavDescriptors, " dynamic GPU-visible CBV/SRV/UAV descriptor",
                      (NumDynamicCbvSrvUavDescriptors > 1 ? "s" : ""),
                      ". Consider increasing GPUDescriptorHeapDynamicSize[0] in EngineD3D12CreateInfo "
                      "or optimizing dynamic resource utilization by using static or mutable shader resource variables instead.");
    }

    if (NumDynamicSamplerDescriptors > 0)
    {
        DynamicSamplerDescriptors = CmdCtx.AllocateDynamicGPUVisibleDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, NumDynamicSamplerDescriptors);
        DEV_CHECK_ERR(DynamicSamplerDescriptors.GetDescriptorHeap() != nullptr,
                      "Failed to allocate ", NumDynamicSamplerDescriptors, " dynamic GPU-visible Sampler descriptor",
                      (NumDynamicSamplerDescriptors > 1 ? "s" : ""),
                      ". Consider using immutable samplers in the Pipeline State Object, increasing GPUDescriptorHeapDynamicSize[1] in "
                      "EngineD3D12CreateInfo, or optimizing dynamic resource utilization by using static or mutable shader resource variables instead.");
    }

    CommandContext::ShaderDescriptorHeaps Heaps{ResourceCache.GetSrvCbvUavDescriptorHeap(), ResourceCache.GetSamplerDescriptorHeap()};
    if (Heaps.pSamplerHeap == nullptr)
        Heaps.pSamplerHeap = DynamicSamplerDescriptors.GetDescriptorHeap();

    if (Heaps.pSrvCbvUavHeap == nullptr)
        Heaps.pSrvCbvUavHeap = DynamicCbvSrvUavDescriptors.GetDescriptorHeap();

    if (NumDynamicCbvSrvUavDescriptors > 0)
        VERIFY(DynamicCbvSrvUavDescriptors.GetDescriptorHeap() == Heaps.pSrvCbvUavHeap, "Inconsistent CbvSrvUav descriptor heaps");
    if (NumDynamicSamplerDescriptors > 0)
        VERIFY(DynamicSamplerDescriptors.GetDescriptorHeap() == Heaps.pSamplerHeap, "Inconsistent Sampler descriptor heaps");

    if (Heaps)
        CmdCtx.SetDescriptorHeaps(Heaps);

    // Offset to the beginning of the current dynamic CBV_SRV_UAV/SAMPLER table from
    // the start of the allocation
    Uint32 DynamicCbvSrvUavTblOffset = 0;
    Uint32 DynamicSamplerTblOffset   = 0;

    m_RootParams.ProcessRootTables(
        [&](Uint32                      RootInd,
            const RootParameter&        RootTable,
            const D3D12_ROOT_PARAMETER& d3d12Param,
            bool                        IsResourceTable,
            D3D12_DESCRIPTOR_HEAP_TYPE  dbgHeapType) //
        {
            D3D12_GPU_DESCRIPTOR_HANDLE RootTableGPUDescriptorHandle;

            bool IsDynamicTable = RootTable.Group == ROOT_PARAMETER_GROUP_DYNAMIC;
            if (IsDynamicTable)
            {
                RootTableGPUDescriptorHandle = IsResourceTable ?
                    DynamicCbvSrvUavDescriptors.GetGpuHandle(DynamicCbvSrvUavTblOffset) :
                    DynamicSamplerDescriptors.GetGpuHandle(DynamicSamplerTblOffset);
            }
            else
            {
                RootTableGPUDescriptorHandle = IsResourceTable ?
                    ResourceCache.GetShaderVisibleTableGPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(RootInd) :
                    ResourceCache.GetShaderVisibleTableGPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(RootInd);
                VERIFY(RootTableGPUDescriptorHandle.ptr != 0, "Unexpected null GPU descriptor handle");
            }

            if (IsCompute)
                CmdCtx.GetCommandList()->SetComputeRootDescriptorTable(BaseRootIndex + RootInd, RootTableGPUDescriptorHandle);
            else
                CmdCtx.GetCommandList()->SetGraphicsRootDescriptorTable(BaseRootIndex + RootInd, RootTableGPUDescriptorHandle);

            if (IsDynamicTable)
            {
                ResourceCache.ProcessTableResources(
                    RootInd, d3d12Param, dbgHeapType,
                    [&](UINT                                OffsetFromTableStart,
                        const D3D12_DESCRIPTOR_RANGE&       range,
                        ShaderResourceCacheD3D12::Resource& Res) //
                    {
                        if (IsResourceTable)
                        {
                            VERIFY(DynamicCbvSrvUavTblOffset < NumDynamicCbvSrvUavDescriptors, "Not enough space in the descriptor heap allocation");

                            if (Res.CPUDescriptorHandle.ptr != 0)
                            {
                                pd3d12Device->CopyDescriptorsSimple(1, DynamicCbvSrvUavDescriptors.GetCpuHandle(DynamicCbvSrvUavTblOffset), Res.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                            }
#ifdef DILIGENT_DEVELOPMENT
                            else
                            {
                                LOG_ERROR_MESSAGE("No valid CbvSrvUav descriptor handle found for root parameter ", RootInd, ", descriptor slot ", OffsetFromTableStart);
                            }
#endif

                            ++DynamicCbvSrvUavTblOffset;
                        }
                        else
                        {
                            VERIFY(DynamicSamplerTblOffset < NumDynamicSamplerDescriptors, "Not enough space in the descriptor heap allocation");

                            if (Res.CPUDescriptorHandle.ptr != 0)
                            {
                                pd3d12Device->CopyDescriptorsSimple(1, DynamicSamplerDescriptors.GetCpuHandle(DynamicSamplerTblOffset), Res.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                            }
#ifdef DILIGENT_DEVELOPMENT
                            else
                            {
                                LOG_ERROR_MESSAGE("No valid sampler descriptor handle found for root parameter ", RootInd, ", descriptor slot ", OffsetFromTableStart);
                            }
#endif

                            ++DynamicSamplerTblOffset;
                        }
                    }); //
            }
        } //
    );

    VERIFY_EXPR(DynamicCbvSrvUavTblOffset == NumDynamicCbvSrvUavDescriptors);
    VERIFY_EXPR(DynamicSamplerTblOffset == NumDynamicSamplerDescriptors);

    // Commit non-dynamic root buffer views
    constexpr auto CommitDynamicBuffers = false;
    CommitRootViews(ResourceCache, CmdCtx, pDeviceCtx,
                    DeviceCtxId, BaseRootIndex, IsCompute, CommitDynamicBuffers);
}


namespace
{

struct BindResourceHelper
{
    ShaderResourceCacheD3D12::Resource&                        DstRes;
    const PipelineResourceDesc&                                ResDesc;
    const PipelineResourceSignatureD3D12Impl::ResourceAttribs& Attribs;
    const Uint32                                               ArrayIndex;
    D3D12_CPU_DESCRIPTOR_HANDLE                                ShdrVisibleHeapCPUDescriptorHandle;
    PipelineResourceSignatureD3D12Impl const&                  Signature;
    ShaderResourceCacheD3D12&                                  ResourceCache;

#ifdef DILIGENT_DEBUG
    bool                      dbgIsDynamic = false;
    D3D12_ROOT_PARAMETER_TYPE dbgParamType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
#endif

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
    // http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-cache#Binding-Objects-to-Shader-Variables

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

        if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
        {
            // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
            // the descriptor is copied by the RootSignature when resources are committed
            VERIFY(DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");
            VERIFY_EXPR(DstRes.CPUDescriptorHandle.ptr != 0);

            GetD3D12Device()->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        UpdateDynamicBuffersCounter(DstRes.pObject.RawPtr<const BufferD3D12Impl>(), pBuffD3D12, ResourceCache.GetDynamicRootBuffersCounter(), Attribs.GetD3D12RootParamType());

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

        if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
        {
            // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
            // the descriptor is copied by the RootSignature when resources are committed
            VERIFY(DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

            GetD3D12Device()->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
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

        if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
        {
            // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
            // the descriptor is copied by the RootSignature when resources are committed
            VERIFY(DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

            GetD3D12Device()->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

        if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
        {
            // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
            // the descriptor is copied by the RootSignature when resources are committed
            VERIFY(DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

            GetD3D12Device()->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        UpdateDynamicBuffersCounter(DstRes.pObject.RawPtr<const TResourceViewType>(), pViewD3D12, ResourceCache.GetDynamicRootBuffersCounter(), Attribs.GetD3D12RootParamType());

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
        VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Static shader resources should never be assigned shader visible descriptor space.");
    }
    else if (ResourceCache.GetContentType() == CacheContentType::SRB)
    {
        if (Attribs.GetD3D12RootParamType() == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0, "Shader resources allocated in non-dynamic descriptor tables must be assigned shader-visible descriptor space");
            else
                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Shader resources allocated in dynamic descriptor tables should never be assigned shader-visible descriptor space");
        }
        else
        {
            VERIFY((ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER ||
                    ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV ||
                    ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV),
                   "Only constant buffers and dynamic buffers views can be allocated as root views");
            VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Resourcesa allocated as root views should never be assigned shader-visible descriptor space");
        }
    }
    else
    {
        UNEXPECTED("Unknown content type");
    }
#endif

    if (pObj)
    {
        static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update this function to handle the new resource type");
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
                                    auto&      SampleDstRes         = ResourceCache.GetRootTable(RootIndex).GetResource(OffsetFromTableStart, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

                                    BindResourceHelper SeparateSampler{
                                        SampleDstRes,
                                        SamplerResDesc,
                                        SamplerAttribs,
                                        SamplerArrInd,
                                        ResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(RootIndex, OffsetFromTableStart),
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

            const auto CacheType            = ResourceCache.GetContentType();
            auto       SamplerArrInd        = SamplerResDesc.ArraySize > 1 ? ArrayIndex : 0;
            const auto RootIndex            = SamplerAttribs.RootIndex(CacheType);
            const auto OffsetFromTableStart = SamplerAttribs.OffsetFromTableStart(CacheType) + SamplerArrInd;
            auto&      DstSam               = ResourceCache.GetRootTable(RootIndex).GetResource(OffsetFromTableStart, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

            if (DstSam.pObject != nullptr && SamplerResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                LOG_ERROR_MESSAGE("Sampler variable '", SamplerResDesc.Name, "' is not dynamic but is being reset to null. This is an error and may cause unpredicted behavior. ",
                                  "Use another shader resource binding instance or label the variable as dynamic if you need to bind another sampler.");

            DstSam = ShaderResourceCacheD3D12::Resource{};
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
    auto& DstRes    = RootTable.GetResource(OffsetFromTableStart, IsSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    auto ShdrVisibleHeapCPUDescriptorHandle = IsSampler ?
        ResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(RootIndex, OffsetFromTableStart) :
        ResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(RootIndex, OffsetFromTableStart);

    BindResourceHelper Helper{
        DstRes,
        ResDesc,
        Attribs,
        ArrayIndex,
        ShdrVisibleHeapCPUDescriptorHandle,
        *this,
        ResourceCache};

#ifdef DILIGENT_DEBUG
    Helper.dbgIsDynamic = RootTable.DbgIsDynamic();
    Helper.dbgParamType = Attribs.GetD3D12RootParamType();
#endif

    Helper.BindResource(pObj);
}

bool PipelineResourceSignatureD3D12Impl::IsBound(Uint32                    ArrayIndex,
                                                 Uint32                    ResIndex,
                                                 ShaderResourceCacheD3D12& ResourceCache) const
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
        if (OffsetFromTableStart + ArrayIndex < RootTable.GetSize())
        {
            const auto& CachedRes =
                RootTable.GetResource(OffsetFromTableStart + ArrayIndex,
                                      ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            if (CachedRes.pObject != nullptr)
            {
                VERIFY(CachedRes.CPUDescriptorHandle.ptr != 0 || CachedRes.pObject.RawPtr<BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC, "No relevant descriptor handle");
                return true;
            }
        }
    }

    return false;
}

} // namespace Diligent
