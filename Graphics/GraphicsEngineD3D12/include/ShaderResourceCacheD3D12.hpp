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

#pragma once

/// \file
/// Declaration of Diligent::ShaderResourceCacheD3D12 class


// Shader resource cache stores D3D12 resources in a continuous chunk of memory:
//
//
//                                         __________________________________________________________
//  m_pMemory                             |             m_pResources, m_NumResources == m            |
//  |                                     |                                                          |
//  V                                     |                                                          V
//  |  RootTable[0]  |   ....    |  RootTable[Nrt-1]  |  Res[0]  |  ... |  Res[n-1]  |    ....     | Res[0]  |  ... |  Res[m-1]  |
//       |                                                A \
//       |                                                |  \
//       |________________________________________________|   \RefCntAutoPtr
//                    m_pResources, m_NumResources == n        \_________
//                                                             |  Object |
//                                                              ---------
//
//  Nrt = m_NumTables
//
//
// The cache is also assigned decriptor heap space to store shader visible descriptor handles (for non-dynamic resources).
//
//
//      DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
//  |   DescrptHndl[0]  ...  DescrptHndl[n-1]   |  DescrptHndl[0]  ...  DescrptHndl[m-1] |
//          A                                           A
//          |                                           |
//          | TableStartOffset                          | TableStartOffset
//          |                                           |
//   |    RootTable[0]    |    RootTable[1]    |    RootTable[2]    |     ....      |   RootTable[Nrt]   |
//                              |                                                           |
//                              | TableStartOffset                                          | InvalidDescriptorOffset
//                              |                                                           |
//                              V                                                           V
//                      |   DescrptHndl[0]  ...  DescrptHndl[n-1]   |                       X
//                       DESCRIPTOR_HEAP_TYPE_SAMPLER
//
//
//
// The allocation is inexed by the offset from the beginning of the root table.
// Each root table is assigned the space to store exactly m_NumResources resources.
// Dynamic resources are not assigned space in the descriptor heap allocation.
//
//
//
//   |      RootTable[i]       |       Res[0]      ...       Res[n-1]      |
//                      \
//       TableStartOffset\____
//                            \
//                             V
//                 .....       |   DescrptHndl[0]  ...  DescrptHndl[n-1]   |    ....
//

#include "DescriptorHeap.hpp"
#include "Shader.h"

namespace Diligent
{

class CommandContext;

class ShaderResourceCacheD3D12
{
public:
    enum class CacheContentType : Uint8
    {
        Signature = 0, // The cache is used by the pipeline resource signature to hold static resources.
        SRB       = 1  // The cache is used by SRB to hold resources of all types (static, mutable, dynamic).
    };

    explicit ShaderResourceCacheD3D12(CacheContentType ContentType) noexcept :
        m_NumTables{0},
        m_ContentType{ContentType}
    {
    }

    // clang-format off
    ShaderResourceCacheD3D12             (const ShaderResourceCacheD3D12&) = delete;
    ShaderResourceCacheD3D12             (ShaderResourceCacheD3D12&&)      = delete;
    ShaderResourceCacheD3D12& operator = (const ShaderResourceCacheD3D12&) = delete;
    ShaderResourceCacheD3D12& operator = (ShaderResourceCacheD3D12&&)      = delete;
    // clang-format on

    ~ShaderResourceCacheD3D12();

    static size_t GetRequiredMemorySize(Uint32       NumTables,
                                        const Uint32 TableSizes[]);

    void Initialize(IMemoryAllocator& MemAllocator,
                    Uint32            NumTables,
                    const Uint32      TableSizes[]);

    static constexpr Uint32 InvalidDescriptorOffset = ~0u;

    struct Resource
    {
        Resource() noexcept {}

        SHADER_RESOURCE_TYPE Type = SHADER_RESOURCE_TYPE_UNKNOWN;
        // CPU descriptor handle of a cached resource in CPU-only descriptor heap.
        // Note that for dynamic resources, this is the only available CPU descriptor handle.
        D3D12_CPU_DESCRIPTOR_HANDLE  CPUDescriptorHandle = {0};
        RefCntAutoPtr<IDeviceObject> pObject;

        bool IsNull() const { return pObject == nullptr; }

        __forceinline void TransitionResource(CommandContext& Ctx);
#ifdef DILIGENT_DEVELOPMENT
        void DvpVerifyResourceState();
#endif
    };

    class RootTable
    {
    public:
        RootTable(Uint32 NumResources, Resource* pResources) noexcept :
            // clang-format off
            m_NumResources{NumResources},
            m_pResources  {pResources  }
        // clang-format on
        {}

        inline const Resource& GetResource(Uint32 OffsetFromTableStart) const
        {
            VERIFY(OffsetFromTableStart < m_NumResources, "Root table is not large enough to store descriptor at offset ", OffsetFromTableStart);
            return m_pResources[OffsetFromTableStart];
        }

        inline const Resource& GetResource(Uint32                           OffsetFromTableStart,
                                           const D3D12_DESCRIPTOR_HEAP_TYPE dbgDescriptorHeapType) const
        {
            VERIFY(m_dbgHeapType == dbgDescriptorHeapType, "Incosistent descriptor heap type");
            VERIFY(OffsetFromTableStart < m_NumResources, "Root table is not large enough to store descriptor at offset ", OffsetFromTableStart);
            return m_pResources[OffsetFromTableStart];
        }
        inline Resource& GetResource(Uint32                           OffsetFromTableStart,
                                     const D3D12_DESCRIPTOR_HEAP_TYPE dbgDescriptorHeapType)
        {
            return const_cast<Resource&>(const_cast<const RootTable*>(this)->GetResource(OffsetFromTableStart, dbgDescriptorHeapType));
        }

        inline Uint32 GetSize() const { return m_NumResources; }

        // Offset from the start of the descriptor heap allocation to the start of the table
        Uint32 m_TableStartOffset = InvalidDescriptorOffset;

#ifdef DILIGENT_DEBUG
        void SetDebugAttribs(Uint32                           MaxOffset,
                             const D3D12_DESCRIPTOR_HEAP_TYPE dbgDescriptorHeapType,
                             bool                             isDynamic)
        {
            VERIFY_EXPR(m_NumResources == MaxOffset);
            m_dbgHeapType  = dbgDescriptorHeapType;
            m_dbgIsDynamic = isDynamic;
        }

        D3D12_DESCRIPTOR_HEAP_TYPE DbgGetHeapType() const { return m_dbgHeapType; }
        bool                       DbgIsDynamic() const { return m_dbgIsDynamic; }
#endif

        // The total number of resources in the table, accounting for array size
        const Uint32 m_NumResources;

    private:
#ifdef DILIGENT_DEBUG
        D3D12_DESCRIPTOR_HEAP_TYPE m_dbgHeapType  = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
        bool                       m_dbgIsDynamic = false;
#endif

        Resource* const m_pResources;
    };

    inline RootTable& GetRootTable(Uint32 RootIndex)
    {
        VERIFY_EXPR(RootIndex < m_NumTables);
        return reinterpret_cast<RootTable*>(m_pMemory)[RootIndex];
    }
    inline const RootTable& GetRootTable(Uint32 RootIndex) const
    {
        VERIFY_EXPR(RootIndex < m_NumTables);
        return reinterpret_cast<const RootTable*>(m_pMemory)[RootIndex];
    }

    inline Uint32 GetNumRootTables() const { return m_NumTables; }

    void SetDescriptorHeapSpace(DescriptorHeapAllocation&& CbcSrvUavHeapSpace, DescriptorHeapAllocation&& SamplerHeapSpace)
    {
        VERIFY(m_SamplerHeapSpace.GetCpuHandle().ptr == 0 && m_CbvSrvUavHeapSpace.GetCpuHandle().ptr == 0, "Space has already been allocated in GPU descriptor heaps");
#ifdef DILIGENT_DEBUG
        Uint32 NumSamplerDescriptors = 0, NumSrvCbvUavDescriptors = 0;
        for (Uint32 rt = 0; rt < m_NumTables; ++rt)
        {
            const auto& Tbl = GetRootTable(rt);
            if (Tbl.m_TableStartOffset != InvalidDescriptorOffset)
            {
                if (Tbl.DbgGetHeapType() == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
                {
                    VERIFY(Tbl.m_TableStartOffset == NumSrvCbvUavDescriptors, "Descriptor space allocation is not continuous");
                    NumSrvCbvUavDescriptors = std::max(NumSrvCbvUavDescriptors, Tbl.m_TableStartOffset + Tbl.GetSize());
                }
                else
                {
                    VERIFY(Tbl.m_TableStartOffset == NumSamplerDescriptors, "Descriptor space allocation is not continuous");
                    NumSamplerDescriptors = std::max(NumSamplerDescriptors, Tbl.m_TableStartOffset + Tbl.GetSize());
                }
            }
        }
        VERIFY(NumSrvCbvUavDescriptors == CbcSrvUavHeapSpace.GetNumHandles() || NumSrvCbvUavDescriptors == 0 && CbcSrvUavHeapSpace.IsNull(), "Unexpected descriptor heap allocation size");
        VERIFY(NumSamplerDescriptors == SamplerHeapSpace.GetNumHandles() || NumSamplerDescriptors == 0 && SamplerHeapSpace.IsNull(), "Unexpected descriptor heap allocation size");
#endif

        m_CbvSrvUavHeapSpace = std::move(CbcSrvUavHeapSpace);
        m_SamplerHeapSpace   = std::move(SamplerHeapSpace);
    }

    ID3D12DescriptorHeap* GetSrvCbvUavDescriptorHeap() { return m_CbvSrvUavHeapSpace.GetDescriptorHeap(); }
    ID3D12DescriptorHeap* GetSamplerDescriptorHeap() { return m_SamplerHeapSpace.GetDescriptorHeap(); }

    // Returns CPU descriptor handle of a shader visible descriptor heap allocation
    template <D3D12_DESCRIPTOR_HEAP_TYPE HeapType>
    D3D12_CPU_DESCRIPTOR_HANDLE GetShaderVisibleTableCPUDescriptorHandle(Uint32 RootParamInd, Uint32 OffsetFromTableStart = 0) const
    {
        const auto& RootParam = GetRootTable(RootParamInd);
        VERIFY(HeapType == RootParam.DbgGetHeapType(), "Invalid descriptor heap type");

        D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle = {0};
        // Descriptor heap allocation is not assigned for dynamic resources or
        // in a special case when resource cache is used to store static
        // variable assignments for a shader. It is also not assigned for root views.
        if (RootParam.m_TableStartOffset != InvalidDescriptorOffset)
        {
            VERIFY(OffsetFromTableStart < RootParam.m_NumResources, "Offset is out of range");
            if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
            {
                VERIFY_EXPR(!m_SamplerHeapSpace.IsNull());
                CPUDescriptorHandle = m_SamplerHeapSpace.GetCpuHandle(RootParam.m_TableStartOffset + OffsetFromTableStart);
            }
            else if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
            {
                VERIFY_EXPR(!m_CbvSrvUavHeapSpace.IsNull());
                CPUDescriptorHandle = m_CbvSrvUavHeapSpace.GetCpuHandle(RootParam.m_TableStartOffset + OffsetFromTableStart);
            }
            else
            {
                UNEXPECTED("Unexpected descriptor heap type");
            }
        }

        return CPUDescriptorHandle;
    }

    // Returns GPU descriptor handle of a shader visible descriptor table
    template <D3D12_DESCRIPTOR_HEAP_TYPE HeapType>
    D3D12_GPU_DESCRIPTOR_HANDLE GetShaderVisibleTableGPUDescriptorHandle(Uint32 RootParamInd, Uint32 OffsetFromTableStart = 0) const
    {
        const auto& RootParam = GetRootTable(RootParamInd);
        VERIFY(RootParam.m_TableStartOffset != InvalidDescriptorOffset, "GPU descriptor handle must never be requested for dynamic resources");
        VERIFY(OffsetFromTableStart < RootParam.m_NumResources, "Offset is out of range");

        D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHandle = {0};
        VERIFY(HeapType == RootParam.DbgGetHeapType(), "Invalid descriptor heap type");
        if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
        {
            VERIFY_EXPR(!m_SamplerHeapSpace.IsNull());
            GPUDescriptorHandle = m_SamplerHeapSpace.GetGpuHandle(RootParam.m_TableStartOffset + OffsetFromTableStart);
        }
        else if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
        {
            VERIFY_EXPR(!m_CbvSrvUavHeapSpace.IsNull());
            GPUDescriptorHandle = m_CbvSrvUavHeapSpace.GetGpuHandle(RootParam.m_TableStartOffset + OffsetFromTableStart);
        }
        else
        {
            UNEXPECTED("Unexpected descriptor heap type");
        }

        return GPUDescriptorHandle;
    }

    template <D3D12_DESCRIPTOR_HEAP_TYPE HeapType>
    D3D12_CPU_DESCRIPTOR_HANDLE CopyDescriptors(ID3D12Device*               pd3d12Device,
                                                D3D12_CPU_DESCRIPTOR_HANDLE SrcDescrHandle,
                                                Uint32                      NumDescriptors,
                                                Uint32                      RootParamInd,
                                                Uint32                      OffsetFromTableStart)
    {
        auto DstDescrHandle = GetShaderVisibleTableCPUDescriptorHandle<HeapType>(RootParamInd, OffsetFromTableStart);
        if (DstDescrHandle.ptr != 0)
        {
            VERIFY_EXPR(SrcDescrHandle.ptr != 0);
            pd3d12Device->CopyDescriptorsSimple(NumDescriptors, DstDescrHandle, SrcDescrHandle, HeapType);
        }
        return DstDescrHandle;
    }

    template <class TOperation>
    __forceinline void ProcessTableResources(Uint32                      RootInd,
                                             const D3D12_ROOT_PARAMETER& d3d12Param,
                                             D3D12_DESCRIPTOR_HEAP_TYPE  dbgHeapType,
                                             TOperation                  Operation)
    {
        auto& TableResources = GetRootTable(RootInd);
        for (UINT r = 0; r < d3d12Param.DescriptorTable.NumDescriptorRanges; ++r)
        {
            const auto& range = d3d12Param.DescriptorTable.pDescriptorRanges[r];
            VERIFY(dbgHeapType == D3D12DescriptorRangeTypeToD3D12HeapType(range.RangeType), "Mistmatch between descriptor heap type and descriptor range type");
            for (UINT d = 0; d < range.NumDescriptors; ++d)
            {
                const auto Offset = range.OffsetInDescriptorsFromTableStart + d;
                auto&      Res    = TableResources.GetResource(Offset, dbgHeapType);
                Operation(Offset, range, Res);
            }
        }
    }

    void TransitionResources(CommandContext& Ctx,
                             bool            PerformTransitions,
                             bool            ValidateStates);

    Uint32& GetDynamicRootBuffersCounter() { return m_NumDynamicRootBuffers; }

    // Returns the number of dynamic buffers bound as root views in the cache regardless of their variable types
    Uint32 GetNumDynamicRootBuffers() const { return m_NumDynamicRootBuffers; }

    CacheContentType GetContentType() const { return m_ContentType; }

#ifdef DILIGENT_DEBUG
    //void DbgVerifyBoundDynamicCBsCounter() const;
#endif

private:
    Resource& GetResource(Uint32 Idx)
    {
        VERIFY_EXPR(Idx < m_TotalResourceCount);
        return reinterpret_cast<Resource*>(reinterpret_cast<RootTable*>(m_pMemory) + m_NumTables)[Idx];
    }

    // Allocation in a GPU-visible sampler descriptor heap
    DescriptorHeapAllocation m_SamplerHeapSpace;

    // Allocation in a GPU-visible CBV/SRV/UAV descriptor heap
    DescriptorHeapAllocation m_CbvSrvUavHeapSpace;

    IMemoryAllocator* m_pAllocator = nullptr;
    void*             m_pMemory    = nullptr;

    // The number of the dynamic buffers bound in the resource cache regardless of their variable type
    Uint32 m_NumDynamicRootBuffers = 0;

    // The number of descriptor tables in the cache
    Uint32 m_NumTables = 0;

    // The total number of resources in the cache
    Uint32 m_TotalResourceCount = 0;

    // Indicates what types of resources are stored in the cache
    const CacheContentType m_ContentType;
};

} // namespace Diligent
