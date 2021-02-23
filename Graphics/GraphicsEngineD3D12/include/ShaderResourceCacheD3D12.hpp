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

#include <array>
#include <memory>

#include "DescriptorHeap.hpp"
#include "Shader.h"
#include "RootParamsManager.hpp"

namespace Diligent
{

class CommandContext;
class RenderDeviceD3D12Impl;

class ShaderResourceCacheD3D12
{
public:
    enum class CacheContentType : Uint8
    {
        Signature = 0, // The cache is used by the pipeline resource signature to hold static resources.
        SRB       = 1  // The cache is used by SRB to hold resources of all types (static, mutable, dynamic).
    };

    explicit ShaderResourceCacheD3D12(CacheContentType ContentType) noexcept :
        m_ContentType{ContentType}
    {
        for (auto& HeapIndex : m_AllocationIndex)
            HeapIndex.fill(-1);
    }

    // clang-format off
    ShaderResourceCacheD3D12             (const ShaderResourceCacheD3D12&) = delete;
    ShaderResourceCacheD3D12             (ShaderResourceCacheD3D12&&)      = delete;
    ShaderResourceCacheD3D12& operator = (const ShaderResourceCacheD3D12&) = delete;
    ShaderResourceCacheD3D12& operator = (ShaderResourceCacheD3D12&&)      = delete;
    // clang-format on

    ~ShaderResourceCacheD3D12();

    struct MemoryRequirements
    {
        Uint32 NumTables                = 0;
        Uint32 TotalResources           = 0;
        Uint32 NumDescriptorAllocations = 0;
        size_t TotalSize                = 0;
    };
    static MemoryRequirements GetMemoryRequirements(const RootParamsManager& RootParams);

    void Initialize(IMemoryAllocator& MemAllocator,
                    Uint32            NumTables,
                    const Uint32      TableSizes[]);

    void Initialize(IMemoryAllocator&        MemAllocator,
                    RenderDeviceD3D12Impl*   pDevice,
                    const RootParamsManager& RootParams);

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
        RootTable(Uint32 NumResources, Resource* pResources, Uint32 TableStartOffset = InvalidDescriptorOffset) noexcept :
            // clang-format off
            m_NumResources    {NumResources    },
            m_pResources      {pResources      },
            m_TableStartOffset{TableStartOffset}
        // clang-format on
        {}

        const Resource& GetResource(Uint32 OffsetFromTableStart) const
        {
            VERIFY(OffsetFromTableStart < m_NumResources, "Root table is not large enough to store descriptor at offset ", OffsetFromTableStart);
            return m_pResources[OffsetFromTableStart];
        }
        Resource& GetResource(Uint32 OffsetFromTableStart)
        {
            VERIFY(OffsetFromTableStart < m_NumResources, "Root table is not large enough to store descriptor at offset ", OffsetFromTableStart);
            return m_pResources[OffsetFromTableStart];
        }

        Uint32 GetSize() const { return m_NumResources; }
        Uint32 GetStartOffset() const { return m_TableStartOffset; }

    private:
        // Offset from the start of the descriptor heap allocation to the start of the table
        const Uint32 m_TableStartOffset;

        // The total number of resources in the table, accounting for array size
        const Uint32 m_NumResources;

        Resource* const m_pResources;
    };

    RootTable& GetRootTable(Uint32 RootIndex)
    {
        VERIFY_EXPR(RootIndex < m_NumTables);
        return reinterpret_cast<RootTable*>(m_pMemory.get())[RootIndex];
    }
    const RootTable& GetRootTable(Uint32 RootIndex) const
    {
        VERIFY_EXPR(RootIndex < m_NumTables);
        return reinterpret_cast<const RootTable*>(m_pMemory.get())[RootIndex];
    }

    Uint32 GetNumRootTables() const { return m_NumTables; }

    ID3D12DescriptorHeap* GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, ROOT_PARAMETER_GROUP Group)
    {
        auto AllocationIdx = m_AllocationIndex[HeapType][Group];
        return AllocationIdx >= 0 ? m_DescriptorAllocations[AllocationIdx].GetDescriptorHeap() : nullptr;
    }

    // Returns CPU/GPU descriptor handle of a descriptor heap allocation
    template <typename HandleType>
    HandleType GetDescriptorTableHandle(
        D3D12_DESCRIPTOR_HEAP_TYPE HeapType,
        ROOT_PARAMETER_GROUP       Group,
        Uint32                     RootParamInd,
        Uint32                     OffsetFromTableStart = 0) const
    {
        const auto& RootParam = GetRootTable(RootParamInd);
        VERIFY(RootParam.GetStartOffset() != InvalidDescriptorOffset, "This root parameter is not assigned a valid descriptor table offset");
        VERIFY(OffsetFromTableStart < RootParam.GetSize(), "Offset is out of range");

        const auto AllocationIdx = m_AllocationIndex[HeapType][Group];
        VERIFY(AllocationIdx >= 0, "Descriptor space is not assigned to this table");
        VERIFY_EXPR(AllocationIdx < m_NumDescriptorAllocations);

        return m_DescriptorAllocations[AllocationIdx].GetHandle<HandleType>(RootParam.GetStartOffset() + OffsetFromTableStart);
    }

    DescriptorHeapAllocation& GetDescriptorAllocation(D3D12_DESCRIPTOR_HEAP_TYPE HeapType,
                                                      ROOT_PARAMETER_GROUP       Group)
    {
        const auto AllocationIdx = m_AllocationIndex[HeapType][Group];
        VERIFY(AllocationIdx >= 0, "Descriptor space is not assigned to this combination of heap type and parameter group");
        VERIFY_EXPR(AllocationIdx < m_NumDescriptorAllocations);
        return m_DescriptorAllocations[AllocationIdx];
    }

    enum class StateTransitionMode
    {
        Transition,
        Verify
    };
    void TransitionResourceStates(CommandContext& Ctx, StateTransitionMode Mode);

    Uint32& GetDynamicRootBuffersCounter() { return m_NumDynamicRootBuffers; }

    // Returns the number of dynamic buffers bound as root views in the cache regardless of their variable types
    Uint32 GetNumDynamicRootBuffers() const { return m_NumDynamicRootBuffers; }

    CacheContentType GetContentType() const { return m_ContentType; }

private:
    Resource& GetResource(Uint32 Idx)
    {
        VERIFY_EXPR(Idx < m_TotalResourceCount);
        return reinterpret_cast<Resource*>(reinterpret_cast<RootTable*>(m_pMemory.get()) + m_NumTables)[Idx];
    }

    size_t AllocateMemory(IMemoryAllocator& MemAllocator);

    std::unique_ptr<void, STDDeleter<void, IMemoryAllocator>> m_pMemory;

    // Descriptor heap allocations, indexed by m_AllocationIndex
    DescriptorHeapAllocation* m_DescriptorAllocations = nullptr;

    // The number of the dynamic buffers bound in the resource cache as root views
    // regardless of their variable type
    Uint32 m_NumDynamicRootBuffers = 0;

    // The total number of resources in the cache
    Uint32 m_TotalResourceCount = 0;

    // The number of descriptor tables in the cache
    Uint16 m_NumTables = 0;

    // The number of descriptor heap allocations
    Uint8 m_NumDescriptorAllocations = 0;

    // Indicates what types of resources are stored in the cache
    const CacheContentType m_ContentType;

    // Descriptor allocation index in m_DescriptorAllocations array for every descriptor heap type
    // (CBV_SRV_UAV, SAMPLER) and GPU visibility.
    // -1 indicates no allocation.
    std::array<std::array<Int8, ROOT_PARAMETER_GROUP_COUNT>, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1> m_AllocationIndex{};
};
static_assert(sizeof(ShaderResourceCacheD3D12) == sizeof(void*) * 3 + 16, "Unexpected sizeof(ShaderResourceCacheD3D12) - did you pack the members properly?");

} // namespace Diligent
