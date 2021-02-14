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
    return lhs.Register               == rhs.Register               &&
           lhs.Space                   == rhs.Space                   &&
           lhs.SRBRootIndex            == rhs.SRBRootIndex            &&
           lhs.SRBOffsetFromTableStart == rhs.SRBOffsetFromTableStart &&
           lhs.ImtblSamplerAssigned    == rhs.ImtblSamplerAssigned;
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
           lhs.Flags        == rhs.Flags;
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

        std::array<Uint32, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1> StaticResCacheTblSizes = {};

        SHADER_TYPE StaticResStages = SHADER_TYPE_UNKNOWN; // Shader stages that have static resources
        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            const auto& ResDesc = Desc.Resources[i];

            m_ShaderStages |= ResDesc.ShaderStages;

            if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                StaticResStages |= ResDesc.ShaderStages;

                // Use artifial root signature:
                // SRVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_SRV (0)
                // UAVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_UAV (1)
                // CBVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_CBV (2)
                // Samplers at root index D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER (3)
                const Uint32 RootIndex = ResourceTypeToD3D12DescriptorRangeType(ResDesc.ResourceType);
                StaticResCacheTblSizes[RootIndex] += ResDesc.ArraySize;
            }
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
            MemPool.AddSpace<ShaderResourceCacheD3D12>(1);
            MemPool.AddSpace<ShaderVariableManagerD3D12>(StaticVarStageCount);
        }

        MemPool.Reserve();

        m_pResourceAttribs  = MemPool.Allocate<ResourceAttribs>(std::max(1u, m_Desc.NumResources));
        m_ImmutableSamplers = MemPool.ConstructArray<ImmutableSamplerAttribs>(m_Desc.NumImmutableSamplers);

        // The memory is now owned by PipelineResourceSignatureD3D12Impl and will be freed by Destruct().
        auto* Ptr = MemPool.ReleaseOwnership();
        VERIFY_EXPR(Ptr == m_pResourceAttribs);
        (void)Ptr;

        CopyDescription(MemPool, Desc);

        if (StaticVarStageCount > 0)
        {
            m_pStaticResCache = MemPool.Construct<ShaderResourceCacheD3D12>(CacheContentType::Signature);
            m_StaticVarsMgrs  = MemPool.Allocate<ShaderVariableManagerD3D12>(StaticVarStageCount);

            m_pStaticResCache->Initialize(GetRawAllocator(), static_cast<Uint32>(StaticResCacheTblSizes.size()), StaticResCacheTblSizes.data());
#ifdef DILIGENT_DEBUG
            m_pStaticResCache->GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_SRV], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);
            m_pStaticResCache->GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_UAV], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);
            m_pStaticResCache->GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_CBV).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_CBV], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);
            m_pStaticResCache->GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, false);
#endif
        }

        CreateLayout();

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
                    new (m_StaticVarsMgrs + Idx) ShaderVariableManagerD3D12{*this, *m_pStaticResCache};
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

void PipelineResourceSignatureD3D12Impl::CreateLayout()
{
    const Uint32 FirstSpace = GetBaseRegisterSpace();

    std::array<Uint32, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1> NumResources           = {};
    std::array<Uint32, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1> StaticResCacheTblSizes = {};

    RootParamsBuilder ParamsBuilder;
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& ResDesc = m_Desc.Resources[i];

        VERIFY(i == 0 || ResDesc.VarType >= m_Desc.Resources[i - 1].VarType, "Resources must be sorted by variable type");

        const bool IsRuntimeSizedArray     = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY) != 0;
        const auto DescriptorRangeType     = ResourceTypeToD3D12DescriptorRangeType(ResDesc.ResourceType);
        Uint32     Register                = IsRuntimeSizedArray ? 0 : NumResources[DescriptorRangeType];
        Uint32     Space                   = (IsRuntimeSizedArray ? m_NumSpaces++ : 0);
        Uint32     SRBRootIndex            = ResourceAttribs::InvalidSRBRootIndex;
        Uint32     SRBOffsetFromTableStart = ResourceAttribs::InvalidOffset;
        Uint32     SigRootIndex            = ResourceAttribs::InvalidSigRootIndex;
        Uint32     SigOffsetFromTableStart = ResourceAttribs::InvalidOffset;

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
        {
            // Use artifial root signature:
            // SRVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_SRV (0)
            // UAVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_UAV (1)
            // CBVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_CBV (2)
            // Samplers at root index D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER (3)
            SigRootIndex            = ResourceTypeToD3D12DescriptorRangeType(ResDesc.ResourceType);
            SigOffsetFromTableStart = StaticResCacheTblSizes[SigRootIndex];
            StaticResCacheTblSizes[SigRootIndex] += ResDesc.ArraySize;
        }

        const bool IsBuffer =
            ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER ||
            ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV ||
            ResDesc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV;
        const bool UseDynamicOffset  = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) == 0;
        const bool IsFormattedBuffer = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0;
        const bool IsRootView        = IsBuffer && UseDynamicOffset && !IsFormattedBuffer;

        // runtime sized array must be in separate space
        if (!IsRuntimeSizedArray)
            NumResources[DescriptorRangeType] += ResDesc.ArraySize;

        const Int32 SrcImmutableSamplerInd = ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER ?
            FindImmutableSampler(m_Desc.ImmutableSamplers, m_Desc.NumImmutableSamplers, ResDesc.ShaderStages, ResDesc.Name, GetCombinedSamplerSuffix()) :
            -1;

        const auto AssignedSamplerInd = (SrcImmutableSamplerInd == -1 && ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV) ?
            FindAssignedSampler(ResDesc) :
            ResourceAttribs::InvalidSamplerInd;

        if (SrcImmutableSamplerInd >= 0)
        {
            auto& ImmutableSampler = m_ImmutableSamplers[SrcImmutableSamplerInd];

            if (!ImmutableSampler.IsAssigned())
            {
                ImmutableSampler.ShaderRegister = Register;
                ImmutableSampler.RegisterSpace  = Space;
                ImmutableSampler.ArraySize      = ResDesc.ArraySize;
            }
            else
            {
                Register = ImmutableSampler.ShaderRegister;
                Space    = ImmutableSampler.RegisterSpace;

                VERIFY_EXPR(ResDesc.ArraySize == ImmutableSampler.ArraySize);

                // Use previous bind point and decrease resource counter
                if (!IsRuntimeSizedArray)
                    NumResources[DescriptorRangeType] -= ResDesc.ArraySize;
            }
        }
        else
        {
            ParamsBuilder.AllocateResourceSlot(ResDesc.ShaderStages, ResDesc.VarType,
                                               IsRootView ? D3D12_ROOT_PARAMETER_TYPE_CBV : D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                                               DescriptorRangeType, ResDesc.ArraySize, Register, FirstSpace + Space, SRBRootIndex,
                                               SRBOffsetFromTableStart);
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
                IsRootView //
            };
    }
    ParamsBuilder.InitializeMgr(GetRawAllocator(), m_RootParams);

    // Add immutable samplers that do not exist in m_Desc.Resources
    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        auto& ImmutableSampler = m_ImmutableSamplers[i];
        if (ImmutableSampler.IsAssigned())
            continue;

        const auto DescriptorRangeType = ResourceTypeToD3D12DescriptorRangeType(SHADER_RESOURCE_TYPE_SAMPLER);

        ImmutableSampler.RegisterSpace  = 0;
        ImmutableSampler.ShaderRegister = NumResources[DescriptorRangeType];
        NumResources[DescriptorRangeType] += 1;
    }

    if (m_Desc.SRBAllocationGranularity > 1)
    {
        std::array<size_t, MAX_SHADERS_IN_PIPELINE> ShaderVariableDataSizes = {};
        for (Uint32 s = 0; s < GetNumActiveShaderStages(); ++s)
        {
            const SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};

            Uint32 UnusedNumVars       = 0;
            ShaderVariableDataSizes[s] = ShaderVariableManagerD3D12::GetRequiredMemorySize(*this, AllowedVarTypes, _countof(AllowedVarTypes), GetActiveShaderStageType(s), UnusedNumVars);
        }

        auto CacheTableSizes = GetCacheTableSizes();
        auto CacheMemorySize = ShaderResourceCacheD3D12::GetRequiredMemorySize(static_cast<Uint32>(CacheTableSizes.size()), CacheTableSizes.data());
        m_SRBMemAllocator.Initialize(m_Desc.SRBAllocationGranularity, GetNumActiveShaderStages(), ShaderVariableDataSizes.data(), 1, &CacheMemorySize);
    }
}

Uint32 PipelineResourceSignatureD3D12Impl::FindAssignedSampler(const PipelineResourceDesc& SepImg) const
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


PipelineResourceSignatureD3D12Impl::~PipelineResourceSignatureD3D12Impl()
{
    Destruct();
}

void PipelineResourceSignatureD3D12Impl::Destruct()
{
    TPipelineResourceSignatureBase::Destruct();

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
                m_StaticVarsMgrs[Idx].Destroy(RawAllocator);
                m_StaticVarsMgrs[Idx].~ShaderVariableManagerD3D12();
            }
        }
        m_StaticVarIndex.fill(-1);
        m_StaticVarsMgrs = nullptr;
    }

    if (m_pStaticResCache != nullptr)
    {
        m_pStaticResCache->~ShaderResourceCacheD3D12();
        m_pStaticResCache = nullptr;
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

bool PipelineResourceSignatureD3D12Impl::IsCompatibleWith(const PipelineResourceSignatureD3D12Impl& Other) const
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

void PipelineResourceSignatureD3D12Impl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                     bool                     InitStaticResources)
{
    auto& SRBAllocator     = m_pDevice->GetSRBAllocator();
    auto  pResBindingD3D12 = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingD3D12Impl instance", ShaderResourceBindingD3D12Impl)(this, false);
    if (InitStaticResources)
        pResBindingD3D12->InitializeStaticResources(nullptr);
    pResBindingD3D12->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
}

Uint32 PipelineResourceSignatureD3D12Impl::GetStaticVariableCount(SHADER_TYPE ShaderType) const
{
    const auto VarMngrInd = GetStaticVariableCountHelper(ShaderType, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return 0;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariableCount();
}

IShaderResourceVariable* PipelineResourceSignatureD3D12Impl::GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name)
{
    const auto VarMngrInd = GetStaticVariableByNameHelper(ShaderType, Name, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariable(Name);
}

IShaderResourceVariable* PipelineResourceSignatureD3D12Impl::GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    const auto VarMngrInd = GetStaticVariableByIndexHelper(ShaderType, Index, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariable(Index);
}

void PipelineResourceSignatureD3D12Impl::BindStaticResources(Uint32            ShaderFlags,
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

size_t PipelineResourceSignatureD3D12Impl::CalculateHash() const
{
    if (m_Desc.NumResources == 0 && m_Desc.NumImmutableSamplers == 0)
        return 0;

    size_t Hash = ComputeHash(m_Desc.NumResources, m_Desc.NumImmutableSamplers, m_Desc.BindingIndex);

    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& Res  = m_Desc.Resources[i];
        const auto& Attr = m_pResourceAttribs[i];

        HashCombine(Hash, Res.ArraySize, Uint32{Res.ShaderStages}, Uint32{Res.VarType}, Uint32{Res.Flags},
                    Attr.Register, Attr.Space, Attr.SRBRootIndex, Attr.SRBOffsetFromTableStart, Attr.IsImmutableSamplerAssigned());
    }

    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        HashCombine(Hash, Uint32{m_Desc.ImmutableSamplers[i].ShaderStages}, m_Desc.ImmutableSamplers[i].Desc);
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
        const auto& RootParam                = m_RootParams.GetRootTable(rt);
        CacheTableSizes[RootParam.RootIndex] = RootParam.GetDescriptorTableSize();
    }

    for (Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        const auto& RootParam                = m_RootParams.GetRootView(rv);
        CacheTableSizes[RootParam.RootIndex] = 1;
    }

    return CacheTableSizes;
}

void PipelineResourceSignatureD3D12Impl::InitSRBResourceCache(ShaderResourceCacheD3D12& ResourceCache,
                                                              IMemoryAllocator&         CacheMemAllocator,
                                                              const char*               DbgPipelineName) const
{
    auto CacheTableSizes = GetCacheTableSizes();

    // Initialize resource cache to hold root tables
    ResourceCache.Initialize(CacheMemAllocator, static_cast<Uint32>(CacheTableSizes.size()), CacheTableSizes.data());

    // Allocate space in GPU-visible descriptor heap for static and mutable variables only
    Uint32 TotalSrvCbvUavDescriptors = m_RootParams.GetTotalSrvCbvUavSlots(ROOT_PARAMETER_GROUP_STATIC_MUTABLE);
    Uint32 TotalSamplerDescriptors   = m_RootParams.GetTotalSamplerSlots(ROOT_PARAMETER_GROUP_STATIC_MUTABLE);

    DescriptorHeapAllocation CbcSrvUavHeapSpace, SamplerHeapSpace;
    if (TotalSrvCbvUavDescriptors)
    {
        CbcSrvUavHeapSpace = GetDevice()->AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, TotalSrvCbvUavDescriptors);
        DEV_CHECK_ERR(!CbcSrvUavHeapSpace.IsNull(),
                      "Failed to allocate ", TotalSrvCbvUavDescriptors, " GPU-visible CBV/SRV/UAV descriptor",
                      (TotalSrvCbvUavDescriptors > 1 ? "s" : ""),
                      ". Consider increasing GPUDescriptorHeapSize[0] in EngineD3D12CreateInfo.");
    }
    VERIFY_EXPR(TotalSrvCbvUavDescriptors == 0 && CbcSrvUavHeapSpace.IsNull() || CbcSrvUavHeapSpace.GetNumHandles() == TotalSrvCbvUavDescriptors);

    if (TotalSamplerDescriptors)
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

        auto TableSize = RootParam.GetDescriptorTableSize();
        VERIFY(TableSize > 0, "Unexpected empty descriptor table");

        auto HeapType = D3D12DescriptorRangeTypeToD3D12HeapType(d3d12RootParam.DescriptorTable.pDescriptorRanges[0].RangeType);

#ifdef DILIGENT_DEBUG
        RootTableCache.SetDebugAttribs(TableSize, HeapType, IsDynamic);
#endif

        // Space for dynamic variables is allocated at every draw call
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
            // AZ TODO: optimization: break on first dynamic resource

            VERIFY_EXPR(RootTableCache.m_TableStartOffset == ShaderResourceCacheD3D12::InvalidDescriptorOffset);
        }
    }

#ifdef DILIGENT_DEBUG
    for (Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        const auto& RootParam      = m_RootParams.GetRootView(rv);
        const auto& d3d12RootParam = RootParam.d3d12RootParam;
        auto&       RootTableCache = ResourceCache.GetRootTable(RootParam.RootIndex);
        const bool  IsDynamic      = RootParam.Group == ROOT_PARAMETER_GROUP_DYNAMIC;

        // Root views are not assigned valid table start offset
        VERIFY_EXPR(RootTableCache.m_TableStartOffset == ShaderResourceCacheD3D12::InvalidDescriptorOffset);

        VERIFY_EXPR(d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV);
        RootTableCache.SetDebugAttribs(1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, IsDynamic);
    }
#endif

    VERIFY_EXPR(SrvCbvUavTblStartOffset == TotalSrvCbvUavDescriptors);
    VERIFY_EXPR(SamplerTblStartOffset == TotalSamplerDescriptors);

    ResourceCache.SetDescriptorHeapSpace(std::move(CbcSrvUavHeapSpace), std::move(SamplerHeapSpace));
}

void PipelineResourceSignatureD3D12Impl::InitializeStaticSRBResources(ShaderResourceCacheD3D12& DstResourceCache) const
{
    if (m_pStaticResCache == nullptr)
        return;

    // SrcResourceCache contains only static resources.
    // DstResourceCache contains static, mutable and dynamic resources.
    const auto& SrcResourceCache = *m_pStaticResCache;
    const auto  ResIdxRange      = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    auto*       d3d12Device      = GetDevice()->GetD3D12Device();
    const auto  SrcCacheType     = SrcResourceCache.GetContentType();
    const auto  DstCacheType     = DstResourceCache.GetContentType();

    auto& DstBoundDynamicCBsCounter = DstResourceCache.GetBoundDynamicCBsCounter();

    for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& Attr    = GetResourceAttribs(r);
        VERIFY_EXPR(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        if (Attr.IsImmutableSamplerAssigned())
            continue;

        const bool  IsSampler    = (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);
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
                DEV_CHECK_ERR(DstRes.pObject == nullptr, "Static resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");

                if (SrcRes.Type == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER)
                {
                    if (DstRes.pObject && DstRes.pObject.RawPtr<const BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC)
                    {
                        VERIFY_EXPR(DstBoundDynamicCBsCounter > 0);
                        --DstBoundDynamicCBsCounter;
                    }
                    if (SrcRes.pObject && SrcRes.pObject.RawPtr<const BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC)
                    {
                        ++DstBoundDynamicCBsCounter;
                    }
                }

                DstRes.pObject             = SrcRes.pObject;
                DstRes.Type                = SrcRes.Type;
                DstRes.CPUDescriptorHandle = SrcRes.CPUDescriptorHandle;

                if (IsSampler)
                {
                    auto ShdrVisibleSamplerHeapCPUDescriptorHandle = DstResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(DstRootIndex, DstCacheOffset);
                    VERIFY_EXPR(ShdrVisibleSamplerHeapCPUDescriptorHandle.ptr != 0);

                    if (ShdrVisibleSamplerHeapCPUDescriptorHandle.ptr != 0)
                    {
                        VERIFY_EXPR(SrcRes.CPUDescriptorHandle.ptr != 0);
                        d3d12Device->CopyDescriptorsSimple(1, ShdrVisibleSamplerHeapCPUDescriptorHandle, SrcRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                    }
                }
                else
                {
                    auto ShdrVisibleHeapCPUDescriptorHandle = DstResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(DstRootIndex, DstCacheOffset);
                    VERIFY_EXPR(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0 || DstRes.Type == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER);

                    // Root views are not assigned space in the GPU-visible descriptor heap allocation
                    if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0 && SrcRes.CPUDescriptorHandle.ptr != 0)
                    {
                        VERIFY_EXPR(SrcRes.CPUDescriptorHandle.ptr != 0);
                        d3d12Device->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, SrcRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    }
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


namespace
{

template <class TOperation>
__forceinline void ProcessCachedTableResources(Uint32                      RootInd,
                                               const D3D12_ROOT_PARAMETER& D3D12Param,
                                               ShaderResourceCacheD3D12&   ResourceCache,
                                               D3D12_DESCRIPTOR_HEAP_TYPE  dbgHeapType,
                                               TOperation                  Operation)
{
    for (UINT r = 0; r < D3D12Param.DescriptorTable.NumDescriptorRanges; ++r)
    {
        const auto& range = D3D12Param.DescriptorTable.pDescriptorRanges[r];
        for (UINT d = 0; d < range.NumDescriptors; ++d)
        {
            VERIFY(dbgHeapType == D3D12DescriptorRangeTypeToD3D12HeapType(range.RangeType), "Mistmatch between descriptor heap type and descriptor range type");

            auto  OffsetFromTableStart = range.OffsetInDescriptorsFromTableStart + d;
            auto& Res                  = ResourceCache.GetRootTable(RootInd).GetResource(OffsetFromTableStart, dbgHeapType);

            Operation(OffsetFromTableStart, range, Res);
        }
    }
}

__forceinline void TransitionResource(CommandContext&                     Ctx,
                                      ShaderResourceCacheD3D12::Resource& Res,
                                      D3D12_DESCRIPTOR_RANGE_TYPE         RangeType)
{
    static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update this function to handle the new resource type");
    switch (Res.Type)
    {
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV, "Unexpected descriptor range type");
            // Not using QueryInterface() for the sake of efficiency
            auto* pBuffToTransition = Res.pObject.RawPtr<BufferD3D12Impl>();
            if (pBuffToTransition->IsInKnownState() && !pBuffToTransition->CheckState(RESOURCE_STATE_CONSTANT_BUFFER))
                Ctx.TransitionResource(pBuffToTransition, RESOURCE_STATE_CONSTANT_BUFFER);
        }
        break;

        case SHADER_RESOURCE_TYPE_BUFFER_SRV:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            auto* pBuffViewD3D12    = Res.pObject.RawPtr<BufferViewD3D12Impl>();
            auto* pBuffToTransition = pBuffViewD3D12->GetBuffer<BufferD3D12Impl>();
            if (pBuffToTransition->IsInKnownState() && !pBuffToTransition->CheckState(RESOURCE_STATE_SHADER_RESOURCE))
                Ctx.TransitionResource(pBuffToTransition, RESOURCE_STATE_SHADER_RESOURCE);
        }
        break;

        case SHADER_RESOURCE_TYPE_BUFFER_UAV:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
            auto* pBuffViewD3D12    = Res.pObject.RawPtr<BufferViewD3D12Impl>();
            auto* pBuffToTransition = pBuffViewD3D12->GetBuffer<BufferD3D12Impl>();
            if (pBuffToTransition->IsInKnownState())
            {
                // We must always call TransitionResource() even when the state is already
                // RESOURCE_STATE_UNORDERED_ACCESS as in this case UAV barrier must be executed
                Ctx.TransitionResource(pBuffToTransition, RESOURCE_STATE_UNORDERED_ACCESS);
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            auto* pTexViewD3D12    = Res.pObject.RawPtr<TextureViewD3D12Impl>();
            auto* pTexToTransition = pTexViewD3D12->GetTexture<TextureD3D12Impl>();
            if (pTexToTransition->IsInKnownState() && !pTexToTransition->CheckAnyState(RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INPUT_ATTACHMENT))
                Ctx.TransitionResource(pTexToTransition, RESOURCE_STATE_SHADER_RESOURCE);
        }
        break;

        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
            auto* pTexViewD3D12    = Res.pObject.RawPtr<TextureViewD3D12Impl>();
            auto* pTexToTransition = pTexViewD3D12->GetTexture<TextureD3D12Impl>();
            if (pTexToTransition->IsInKnownState())
            {
                // We must always call TransitionResource() even when the state is already
                // RESOURCE_STATE_UNORDERED_ACCESS as in this case UAV barrier must be executed
                Ctx.TransitionResource(pTexToTransition, RESOURCE_STATE_UNORDERED_ACCESS);
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_SAMPLER:
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, "Unexpected descriptor range type");
            break;

        case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            auto* pTLASD3D12 = Res.pObject.RawPtr<TopLevelASD3D12Impl>();
            if (pTLASD3D12->IsInKnownState())
                Ctx.TransitionResource(pTLASD3D12, RESOURCE_STATE_RAY_TRACING);
        }
        break;

        default:
            // Resource not bound
            VERIFY(Res.Type == SHADER_RESOURCE_TYPE_UNKNOWN, "Unexpected resource type");
            VERIFY(Res.pObject == nullptr && Res.CPUDescriptorHandle.ptr == 0, "Bound resource is unexpected");
    }
}


#ifdef DILIGENT_DEVELOPMENT
void DvpVerifyResourceState(const ShaderResourceCacheD3D12::Resource& Res, D3D12_DESCRIPTOR_RANGE_TYPE RangeType)
{
    static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update this function to handle the new resource type");
    switch (Res.Type)
    {
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV, "Unexpected descriptor range type");
            // Not using QueryInterface() for the sake of efficiency
            const auto* pBufferD3D12 = Res.pObject.RawPtr<const BufferD3D12Impl>();
            if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_CONSTANT_BUFFER))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBufferD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_CONSTANT_BUFFER state. Actual state: ",
                                  GetResourceStateString(pBufferD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the buffer state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_BUFFER_SRV:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            const auto* pBuffViewD3D12 = Res.pObject.RawPtr<const BufferViewD3D12Impl>();
            const auto* pBufferD3D12   = pBuffViewD3D12->GetBuffer<const BufferD3D12Impl>();
            if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_SHADER_RESOURCE))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBufferD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_SHADER_RESOURCE state.  Actual state: ",
                                  GetResourceStateString(pBufferD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the buffer state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_BUFFER_UAV:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
            const auto* pBuffViewD3D12 = Res.pObject.RawPtr<const BufferViewD3D12Impl>();
            const auto* pBufferD3D12   = pBuffViewD3D12->GetBuffer<const BufferD3D12Impl>();
            if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBufferD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_UNORDERED_ACCESS state. Actual state: ",
                                  GetResourceStateString(pBufferD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the buffer state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            const auto* pTexViewD3D12 = Res.pObject.RawPtr<const TextureViewD3D12Impl>();
            const auto* pTexD3D12     = pTexViewD3D12->GetTexture<TextureD3D12Impl>();
            if (pTexD3D12->IsInKnownState() && !pTexD3D12->CheckAnyState(RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INPUT_ATTACHMENT))
            {
                LOG_ERROR_MESSAGE("Texture '", pTexD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_SHADER_RESOURCE state. Actual state: ",
                                  GetResourceStateString(pTexD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the texture state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
            const auto* pTexViewD3D12 = Res.pObject.RawPtr<const TextureViewD3D12Impl>();
            const auto* pTexD3D12     = pTexViewD3D12->GetTexture<const TextureD3D12Impl>();
            if (pTexD3D12->IsInKnownState() && !pTexD3D12->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                LOG_ERROR_MESSAGE("Texture '", pTexD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_UNORDERED_ACCESS state. Actual state: ",
                                  GetResourceStateString(pTexD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the texture state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_SAMPLER:
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, "Unexpected descriptor range type");
            break;

        case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
        {
            VERIFY(RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            const auto* pTLASD3D12 = Res.pObject.RawPtr<const TopLevelASD3D12Impl>();
            if (pTLASD3D12->IsInKnownState() && !pTLASD3D12->CheckState(RESOURCE_STATE_RAY_TRACING))
            {
                LOG_ERROR_MESSAGE("TLAS '", pTLASD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_RAY_TRACING state.  Actual state: ",
                                  GetResourceStateString(pTLASD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the TLAS state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        default:
            // Resource not bound
            VERIFY(Res.Type == SHADER_RESOURCE_TYPE_UNKNOWN, "Unexpected resource type");
            VERIFY(Res.pObject == nullptr && Res.CPUDescriptorHandle.ptr == 0, "Bound resource is unexpected");
    }
}
#endif // DILIGENT_DEVELOPMENT

} // namespace

void PipelineResourceSignatureD3D12Impl::TransitionResources(ShaderResourceCacheD3D12& ResourceCache,
                                                             CommandContext&           Ctx,
                                                             bool                      PerformResourceTransitions,
                                                             bool                      ValidateStates) const
{
    m_RootParams.ProcessRootTables(
        [&](Uint32                      RootInd,
            const RootParameter&        RootTable,
            const D3D12_ROOT_PARAMETER& D3D12Param,
            bool                        IsResourceTable,
            D3D12_DESCRIPTOR_HEAP_TYPE  dbgHeapType) //
        {
            ProcessCachedTableResources(
                RootInd, D3D12Param, ResourceCache, dbgHeapType,
                [&](UINT                                OffsetFromTableStart,
                    const D3D12_DESCRIPTOR_RANGE&       range,
                    ShaderResourceCacheD3D12::Resource& Res) //
                {
                    // AZ TODO: optimize
                    if (PerformResourceTransitions)
                    {
                        TransitionResource(Ctx, Res, range.RangeType);
                    }
#ifdef DILIGENT_DEVELOPMENT
                    else if (ValidateStates)
                    {
                        DvpVerifyResourceState(Res, range.RangeType);
                    }
#endif
                } //
            );
        } //
    );
}

void PipelineResourceSignatureD3D12Impl::CommitRootTables(ShaderResourceCacheD3D12& ResourceCache,
                                                          CommandContext&           CmdCtx,
                                                          DeviceContextD3D12Impl*   pDeviceCtx,
                                                          Uint32                    DeviceCtxId,
                                                          bool                      IsCompute,
                                                          Uint32                    FirstRootIndex)
{
    auto* pd3d12Device = GetDevice()->GetD3D12Device();

    Uint32 NumDynamicCbvSrvUavDescriptors = m_RootParams.GetTotalSrvCbvUavSlots(ROOT_PARAMETER_GROUP_DYNAMIC);
    Uint32 NumDynamicSamplerDescriptors   = m_RootParams.GetTotalSamplerSlots(ROOT_PARAMETER_GROUP_DYNAMIC);
    //VERIFY_EXPR(NumDynamicCbvSrvUavDescriptors > 0 || NumDynamicSamplerDescriptors > 0);

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

    CommandContext::ShaderDescriptorHeaps Heaps(ResourceCache.GetSrvCbvUavDescriptorHeap(), ResourceCache.GetSamplerDescriptorHeap());
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
            const D3D12_ROOT_PARAMETER& D3D12Param,
            bool                        IsResourceTable,
            D3D12_DESCRIPTOR_HEAP_TYPE  dbgHeapType) //
        {
            D3D12_GPU_DESCRIPTOR_HANDLE RootTableGPUDescriptorHandle;

            bool IsDynamicTable = RootTable.Group == ROOT_PARAMETER_GROUP_DYNAMIC;
            if (IsDynamicTable)
            {
                if (IsResourceTable)
                    RootTableGPUDescriptorHandle = DynamicCbvSrvUavDescriptors.GetGpuHandle(DynamicCbvSrvUavTblOffset);
                else
                    RootTableGPUDescriptorHandle = DynamicSamplerDescriptors.GetGpuHandle(DynamicSamplerTblOffset);
            }
            else
            {
                RootTableGPUDescriptorHandle = IsResourceTable ?
                    ResourceCache.GetShaderVisibleTableGPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(RootInd) :
                    ResourceCache.GetShaderVisibleTableGPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(RootInd);
                VERIFY(RootTableGPUDescriptorHandle.ptr != 0, "Unexpected null GPU descriptor handle");
            }

            if (IsCompute)
                CmdCtx.GetCommandList()->SetComputeRootDescriptorTable(FirstRootIndex + RootInd, RootTableGPUDescriptorHandle);
            else
                CmdCtx.GetCommandList()->SetGraphicsRootDescriptorTable(FirstRootIndex + RootInd, RootTableGPUDescriptorHandle);

            if (IsDynamicTable)
            {
                ProcessCachedTableResources(
                    RootInd, D3D12Param, ResourceCache, dbgHeapType,
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

    for (Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        auto& RootView = m_RootParams.GetRootView(rv);
        auto  RootInd  = RootView.RootIndex;

        auto& Res = ResourceCache.GetRootTable(RootInd).GetResource(0, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        if (auto* pBuffToTransition = Res.pObject.RawPtr<BufferD3D12Impl>())
        {
            bool IsDynamic = pBuffToTransition->GetDesc().Usage == USAGE_DYNAMIC;
            if (!IsDynamic)
            {
                D3D12_GPU_VIRTUAL_ADDRESS CBVAddress = pBuffToTransition->GetGPUAddress(DeviceCtxId, pDeviceCtx);
                if (IsCompute)
                    CmdCtx.GetCommandList()->SetComputeRootConstantBufferView(FirstRootIndex + RootInd, CBVAddress);
                else
                    CmdCtx.GetCommandList()->SetGraphicsRootConstantBufferView(FirstRootIndex + RootInd, CBVAddress);
            }
        }
    }
}

void PipelineResourceSignatureD3D12Impl::CommitRootViews(ShaderResourceCacheD3D12& ResourceCache,
                                                         CommandContext&           CmdCtx,
                                                         DeviceContextD3D12Impl*   pDeviceCtx,
                                                         Uint32                    DeviceCtxId,
                                                         bool                      IsCompute,
                                                         Uint32                    FirstRootIndex)
{
    for (Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        auto& RootView = m_RootParams.GetRootView(rv);
        auto  RootInd  = RootView.RootIndex;

        auto& Res = ResourceCache.GetRootTable(RootInd).GetResource(0, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        if (auto* pBuffToTransition = Res.pObject.RawPtr<BufferD3D12Impl>())
        {
            bool IsDynamic = pBuffToTransition->GetDesc().Usage == USAGE_DYNAMIC;
            if (IsDynamic)
            {
                D3D12_GPU_VIRTUAL_ADDRESS CBVAddress = pBuffToTransition->GetGPUAddress(DeviceCtxId, pDeviceCtx);
                if (IsCompute)
                    CmdCtx.GetCommandList()->SetComputeRootConstantBufferView(FirstRootIndex + RootInd, CBVAddress);
                else
                    CmdCtx.GetCommandList()->SetGraphicsRootConstantBufferView(FirstRootIndex + RootInd, CBVAddress);
            }
        }
    }
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
    bool dbgIsDynamic  = false;
    bool dbgIsRootView = false;
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
        else
        {
            VERIFY(dbgIsRootView || dbgIsDynamic, "Descriptor in root table can be used only in dynamic tables.");
        }

        auto& BoundDynamicCBsCounter = ResourceCache.GetBoundDynamicCBsCounter();
        if (DstRes.pObject != nullptr && DstRes.pObject.RawPtr<const BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(BoundDynamicCBsCounter > 0, "There is a dynamic CB bound in the resource cache, but the dynamic CB counter is zero");
            --BoundDynamicCBsCounter;
        }
        if (pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC)
            ++BoundDynamicCBsCounter;
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
struct ResourceViewTraits<ITextureViewD3D12>
{
    static const INTERFACE_ID& IID;

    //static bool VerifyView(ITextureViewD3D12* pViewD3D12, const D3DShaderResourceAttribs& Attribs)
    //{
    //    return true;
    //}
};
const INTERFACE_ID& ResourceViewTraits<ITextureViewD3D12>::IID = IID_TextureViewD3D12;

template <>
struct ResourceViewTraits<IBufferViewD3D12>
{
    static const INTERFACE_ID& IID;

    //static bool VerifyView(IBufferViewD3D12* pViewD3D12, const D3DShaderResourceAttribs& Attribs)
    //{
    //    return VerifyBufferViewModeD3D(pViewD3D12, Attribs);
    //}
};
const INTERFACE_ID& ResourceViewTraits<IBufferViewD3D12>::IID = IID_BufferViewD3D12;


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
    //ResourceViewTraits<TResourceViewType>::VerifyView(pViewD3D12, Attribs);
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
        VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Static shader resources of a shader should not be assigned shader visible descriptor space");
    }
    else if (ResourceCache.GetContentType() == CacheContentType::SRB)
    {
        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER && ResDesc.ArraySize == 1)
        {
            VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Non-array constant buffers are bound as root views and should not be assigned shader visible descriptor space");
        }
        else
        {
            if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Dynamic resources of a shader resource binding should be assigned shader visible descriptor space at every draw call");
            else
                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0, "Non-dynamics resources of a shader resource binding must be assigned shader visible descriptor space");
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
                CacheResourceView<ITextureViewD3D12>(
                    pObj, TEXTURE_VIEW_SHADER_RESOURCE,
                    [&](ITextureViewD3D12* pTexView) //
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
                CacheResourceView<ITextureViewD3D12>(pObj, TEXTURE_VIEW_UNORDERED_ACCESS, [](ITextureViewD3D12*) {});
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_SRV:
                CacheResourceView<IBufferViewD3D12>(pObj, BUFFER_VIEW_SHADER_RESOURCE, [](IBufferViewD3D12*) {});
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_UAV:
                CacheResourceView<IBufferViewD3D12>(pObj, BUFFER_VIEW_UNORDERED_ACCESS, [](IBufferViewD3D12*) {});
                break;

            case SHADER_RESOURCE_TYPE_SAMPLER:
                //DEV_CHECK_ERR(Signature.IsUsingSeparateSamplers(), "Samplers should not be set directly when using combined texture samplers");
                if (Signature.IsUsingSeparateSamplers())
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
    Helper.dbgIsDynamic  = RootTable.IsDynamic();
    Helper.dbgIsRootView = Attribs.IsRootView != 0;
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
