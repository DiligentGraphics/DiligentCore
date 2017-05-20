/*     Copyright 2015-2017 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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
#include "DescriptorHeap.h"
#include "RenderDeviceD3D12Impl.h"
#include "D3D12Utils.h"

namespace Diligent
{

// Creates a new descriptor heap and reference the entire heap
DescriptorHeapAllocationManager::DescriptorHeapAllocationManager(IMemoryAllocator &Allocator, 
                                                                 RenderDeviceD3D12Impl *pDeviceD3D12Impl,
                                                                 IDescriptorAllocator *pParentAllocator,
                                                                 size_t ThisManagerId,
                                                                 const D3D12_DESCRIPTOR_HEAP_DESC &HeapDesc) : 
    m_FreeBlockManager(HeapDesc.NumDescriptors, Allocator),
    m_NumDescriptorsInAllocation(HeapDesc.NumDescriptors),
    m_HeapDesc(HeapDesc),
    m_pDeviceD3D12Impl(pDeviceD3D12Impl),
    m_pParentAllocator(pParentAllocator),
    m_ThisManagerId(ThisManagerId)
{
    auto pDevice = pDeviceD3D12Impl->GetD3D12Device();

    m_FirstCPUHandle.ptr = 0;
    m_FirstGPUHandle.ptr = 0;
    m_DescriptorSize  = pDevice->GetDescriptorHandleIncrementSize(HeapDesc.Type);

	pDevice->CreateDescriptorHeap(&m_HeapDesc, __uuidof(m_pd3d12DescriptorHeap), reinterpret_cast<void**>(static_cast<ID3D12DescriptorHeap**>(&m_pd3d12DescriptorHeap)));
	m_FirstCPUHandle = m_pd3d12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if(m_HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        m_FirstGPUHandle = m_pd3d12DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

// Uses subrange of descriptors in the existing D3D12 descriptor heap
// that starts at offset FirstDescriptor and uses NumDescriptors descriptors
DescriptorHeapAllocationManager::DescriptorHeapAllocationManager(IMemoryAllocator &Allocator, 
                                                                 RenderDeviceD3D12Impl *pDeviceD3D12Impl,
                                                                 IDescriptorAllocator *pParentAllocator,
                                                                 size_t ThisManagerId,
                                                                 ID3D12DescriptorHeap *pd3d12DescriptorHeap,
                                                                 Uint32 FirstDescriptor,
                                                                 Uint32 NumDescriptors): 
    m_FreeBlockManager(NumDescriptors, Allocator),
    m_NumDescriptorsInAllocation(NumDescriptors),
    m_pDeviceD3D12Impl(pDeviceD3D12Impl),
    m_pParentAllocator(pParentAllocator),
    m_ThisManagerId(ThisManagerId),
    m_pd3d12DescriptorHeap(pd3d12DescriptorHeap)
{
    m_HeapDesc = m_pd3d12DescriptorHeap->GetDesc();
    m_DescriptorSize = pDeviceD3D12Impl->GetD3D12Device()->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);
        
	m_FirstCPUHandle = pd3d12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_FirstCPUHandle.ptr += m_DescriptorSize * FirstDescriptor;

    if (m_HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        m_FirstGPUHandle = pd3d12DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_FirstGPUHandle.ptr += m_DescriptorSize * FirstDescriptor;
    }
}


DescriptorHeapAllocationManager::~DescriptorHeapAllocationManager()
{
    VERIFY(m_FreeBlockManager.GetFreeSize() == m_NumDescriptorsInAllocation, "Not all descriptors were released");
}

DescriptorHeapAllocation DescriptorHeapAllocationManager::Allocate(uint32_t Count)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    // Methods of VariableSizeGPUAllocationsManager class are not thread safe!

    // Use variable-size GPU allocations manager to allocate the requested number of descriptors
    auto DescriptorHandleOffset = m_FreeBlockManager.Allocate(Count);
    if (DescriptorHandleOffset == VariableSizeGPUAllocationsManager::InvalidOffset)
    {
        return DescriptorHeapAllocation();
    }

    // Compute the first CPU and GPU descriptor handles in the allocation by
    // offseting the first CPU and GPU descriptor handle in the range
    auto CPUHandle = m_FirstCPUHandle;
    CPUHandle.ptr += DescriptorHandleOffset * m_DescriptorSize;

    auto GPUHandle = m_FirstGPUHandle; // Will be null if the heap is not GPU-visible
    if(m_HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        GPUHandle.ptr += DescriptorHandleOffset * m_DescriptorSize;

    VERIFY(m_ThisManagerId < std::numeric_limits<Uint16>::max(), "ManagerID exceeds 16-bit range");
	return DescriptorHeapAllocation( m_pParentAllocator, m_pd3d12DescriptorHeap, CPUHandle, GPUHandle, Count, static_cast<Uint16>(m_ThisManagerId));
}

void DescriptorHeapAllocationManager::Free(DescriptorHeapAllocation&& Allocation)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    // Methods of VariableSizeGPUAllocationsManager class are not thread safe!

    VERIFY(Allocation.GetAllocationManagerId() == m_ThisManagerId, "Invalid descriptor heap manager Id")

    auto DescriptorOffset = (Allocation.GetCpuHandle().ptr - m_FirstCPUHandle.ptr) / m_DescriptorSize;
    
	// Note that the allocation is not released immediately, but added to the release queue in the allocations manager
	
    // The following basic requirement guarantees correctness of resource deallocation:
    //
    //        A resource is never released before the last draw command referencing it is invoked on the immediate context
    //
    // See http://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-resource-lifetimes/
    //
    // If basic requirement is met, GetNextCmdListNumber() will never return a number that is less than the last
    // command list number that references descriptors from the allocation
    m_FreeBlockManager.Free(DescriptorOffset, Allocation.GetNumHandles(), m_pDeviceD3D12Impl->GetNextCmdListNumber());
    
	// Clear the allocation
    Allocation = DescriptorHeapAllocation();
}

void DescriptorHeapAllocationManager::ReleaseStaleAllocations(Uint64 NumCompletedCmdLists)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    // Methods of VariableSizeGPUAllocationsManager class are not thread safe!

    m_FreeBlockManager.ReleaseStaleAllocations(NumCompletedCmdLists);
}




//
// CPUDescriptorHeap implementation
//
CPUDescriptorHeap::CPUDescriptorHeap(IMemoryAllocator &Allocator, RenderDeviceD3D12Impl *pDeviceD3D12Impl, Uint32 NumDescriptorsInHeap, D3D12_DESCRIPTOR_HEAP_TYPE Type, D3D12_DESCRIPTOR_HEAP_FLAGS Flags) : 
    m_pDeviceD3D12Impl(pDeviceD3D12Impl), 
    m_MemAllocator(Allocator),
    m_HeapPool(STD_ALLOCATOR_RAW_MEM(DescriptorHeapAllocationManager, GetRawAllocator(), "Allocator for vector<DescriptorHeapAllocationManager>")),
    m_AvailableHeaps(STD_ALLOCATOR_RAW_MEM(size_t, GetRawAllocator(), "Allocator for set<size_t>"))
{
    m_HeapDesc.Type = Type;
    m_HeapDesc.NodeMask = 1;
    m_HeapDesc.NumDescriptors = NumDescriptorsInHeap;
    m_HeapDesc.Flags = Flags;

    m_DescriptorSize  = m_pDeviceD3D12Impl->GetD3D12Device()->GetDescriptorHandleIncrementSize(Type);
}

CPUDescriptorHeap::~CPUDescriptorHeap()
{
    VERIFY(m_CurrentSize == 0, "Not all allocations released" );
    
    VERIFY(m_AvailableHeaps.size() == m_HeapPool.size(), "Not all descriptor heap pools are released");
	Uint32 TotalDescriptors = 0;
    for (auto HeapPoolIt = m_HeapPool.begin(); HeapPoolIt != m_HeapPool.end(); ++HeapPoolIt)
    {
        VERIFY(HeapPoolIt->GetNumAvailableDescriptors() == m_HeapDesc.NumDescriptors, "Not all descriptors in the descriptor pool are released");
		TotalDescriptors += HeapPoolIt->GetMaxDescriptors();
	}

    LOG_INFO_MESSAGE(GetD3D12DescriptorHeapTypeLiteralName(m_HeapDesc.Type), " CPU heap max size: ", m_MaxHeapSize, " (", m_MaxHeapSize*100/ TotalDescriptors, "%) "
					 ". Max stale size: ", m_MaxStaleSize, " (", m_MaxStaleSize * 100 / TotalDescriptors, "%)");
}

DescriptorHeapAllocation CPUDescriptorHeap::Allocate( uint32_t Count )
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    // Note that every DescriptorHeapAllocationManager object instance is itslef
    // thread-safe. Nested mutexes cannot cause a deadlock

    DescriptorHeapAllocation Allocation;
    // Go through all descriptor heap managers that have free descriptors
    for (auto AvailableHeapIt = m_AvailableHeaps.begin(); AvailableHeapIt != m_AvailableHeaps.end(); ++AvailableHeapIt)
    {
        // Try to allocate descriptor using the current descriptor heap manager
        Allocation = m_HeapPool[*AvailableHeapIt].Allocate(Count);
        // Remove the manager from the pool if it has no more available descriptors
        if(m_HeapPool[*AvailableHeapIt].GetNumAvailableDescriptors() == 0)
            m_AvailableHeaps.erase(*AvailableHeapIt);

        // Terminate the loop if descriptor was successfully allocated, otherwise
        // go to the next manager
        if(Allocation.GetCpuHandle().ptr != 0)
            break;
    }

    // If there were no available descriptor heap managers or no manager was able 
    // to suffice the allocation request, create a new manager
    if(Allocation.GetCpuHandle().ptr == 0)
    {
        // Make sure the heap is large enough to accomodate the requested number of descriptors
        if(Count > m_HeapDesc.NumDescriptors)
        {
            LOG_WARNING_MESSAGE("Number of requested descriptors exceeds the descriptor heap size. Increasing the number of descriptors in the heap")
        }
        m_HeapDesc.NumDescriptors = std::max(m_HeapDesc.NumDescriptors, static_cast<UINT>(Count));
        // Create a new descriptor heap manager. Note that this constructor creates a new D3D12 descriptor
        // heap and references the entire heap. Pool index is used as manager ID
        m_HeapPool.emplace_back(m_MemAllocator, m_pDeviceD3D12Impl, this, m_HeapPool.size(), m_HeapDesc);
        auto NewHeapIt = m_AvailableHeaps.insert(m_HeapPool.size()-1);

        // Use the new manager to allocate descriptor handles
        Allocation = m_HeapPool[*NewHeapIt.first].Allocate(Count);
    }

    m_CurrentSize += (Allocation.GetCpuHandle().ptr != 0) ? Count : 0;
    m_MaxHeapSize = std::max(m_MaxHeapSize, m_CurrentSize);

    return Allocation;
}

void CPUDescriptorHeap::Free(DescriptorHeapAllocation&& Allocation)
{
    // Method is called from ~DescriptorHeapAllocation()
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    auto ManagerId = Allocation.GetAllocationManagerId();
    m_CurrentSize -= static_cast<Uint32>(Allocation.GetNumHandles());
    m_HeapPool[ManagerId].Free(std::move(Allocation));
}

void CPUDescriptorHeap::ReleaseStaleAllocations(Uint64 NumCompletedCmdLists)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
	size_t StaleSize = 0;
    for (size_t HeapManagerInd = 0; HeapManagerInd < m_HeapPool.size(); ++HeapManagerInd)
    {
		// Update size before releasing stale allocations	
		StaleSize += m_HeapPool[HeapManagerInd].GetNumStaleDescriptors();

        m_HeapPool[HeapManagerInd].ReleaseStaleAllocations(NumCompletedCmdLists);
        // Return the manager to the pool of available managers if it has available descriptors
        if(m_HeapPool[HeapManagerInd].GetNumAvailableDescriptors() > 0)
            m_AvailableHeaps.insert(HeapManagerInd);
    }
	m_MaxStaleSize = std::max(m_MaxStaleSize, static_cast<Uint32>(StaleSize));
}




GPUDescriptorHeap::GPUDescriptorHeap(IMemoryAllocator &Allocator, 
                                     RenderDeviceD3D12Impl *pDevice, 
                                     Uint32 NumDescriptorsInHeap, 
                                     Uint32 NumDynamicDescriptors,
                                     D3D12_DESCRIPTOR_HEAP_TYPE Type, 
                                     D3D12_DESCRIPTOR_HEAP_FLAGS Flags) :
    m_pDeviceD3D12(pDevice),
    m_HeapDesc
    {
        Type,
        NumDescriptorsInHeap + NumDynamicDescriptors,
        Flags,
        1 // UINT NodeMask;
    },
    m_pd3d12DescriptorHeap([&]{
              CComPtr<ID3D12DescriptorHeap> pHeap;
              pDevice->GetD3D12Device()->CreateDescriptorHeap(&m_HeapDesc, __uuidof(pHeap), reinterpret_cast<void**>(&pHeap));
              return pHeap;
              }()),
    m_DescriptorSize( pDevice->GetD3D12Device()->GetDescriptorHandleIncrementSize(Type) ),
    m_HeapAllocationManager(Allocator, pDevice, this, 0, m_pd3d12DescriptorHeap, 0, NumDescriptorsInHeap),
    m_DynamicAllocationsManager(Allocator, pDevice, this, 1, m_pd3d12DescriptorHeap, NumDescriptorsInHeap, NumDynamicDescriptors )
{
}

GPUDescriptorHeap::~GPUDescriptorHeap()
{
	auto StaticSize = m_HeapAllocationManager.GetMaxDescriptors();
	auto DynamicSize = m_DynamicAllocationsManager.GetMaxDescriptors();
    LOG_INFO_MESSAGE(GetD3D12DescriptorHeapTypeLiteralName(m_HeapDesc.Type), " GPU heap max allocated size (static|dynamic): ", 
		            m_MaxHeapSize, " (", m_MaxHeapSize * 100 / StaticSize,"%) | ",
					m_MaxDynamicSize, " (", m_MaxDynamicSize * 100 / DynamicSize, "%). Max stale size (static|dynamic): ", 
		            m_MaxStaleSize, " (", m_MaxStaleSize * 100 / StaticSize, "%) | ", 
					m_MaxDynamicStaleSize, " (", m_MaxDynamicStaleSize * 100 / DynamicSize, "%)");
}

DescriptorHeapAllocation GPUDescriptorHeap::Allocate(uint32_t Count)
{
    // Note: this mutex may be redundant as DescriptorHeapAllocationManager::Allocate() is itself thread-safe
    std::lock_guard<std::mutex> LockGuard(m_AllocMutex);
    DescriptorHeapAllocation Allocation = m_HeapAllocationManager.Allocate(Count);

    m_CurrentSize += (Allocation.GetCpuHandle().ptr != 0) ? Count : 0;
    m_MaxHeapSize = std::max(m_MaxHeapSize, m_CurrentSize);

    return Allocation;
}

DescriptorHeapAllocation GPUDescriptorHeap::AllocateDynamic(uint32_t Count)
{
    // Note: this mutex may be redundant as DescriptorHeapAllocationManager::Allocate() is itself thread-safe
    std::lock_guard<std::mutex> LockGuard(m_DynAllocMutex);
    DescriptorHeapAllocation Allocation = m_DynamicAllocationsManager.Allocate(Count);

    m_CurrentDynamicSize += (Allocation.GetCpuHandle().ptr != 0) ? Count : 0;
    m_MaxDynamicSize = std::max(m_MaxDynamicSize, m_CurrentDynamicSize);

    return Allocation;
}

void GPUDescriptorHeap::Free(DescriptorHeapAllocation&& Allocation)
{
    auto MgrId = Allocation.GetAllocationManagerId();
    VERIFY(MgrId == 0 || MgrId == 1, "Unexpected allocation manager ID");

    // Note: mutexex may be redundant as DescriptorHeapAllocationManager::Free() is itself thread-safe
    if(MgrId == 0)
    {
        std::lock_guard<std::mutex> LockGuard(m_AllocMutex);
        m_CurrentSize -= static_cast<Uint32>(Allocation.GetNumHandles());
        m_HeapAllocationManager.Free(std::move(Allocation));
    }
    else
    {
        std::lock_guard<std::mutex> LockGuard(m_DynAllocMutex);
        m_CurrentDynamicSize -= static_cast<Uint32>(Allocation.GetNumHandles());
        m_DynamicAllocationsManager.Free(std::move(Allocation));
    }
}

void GPUDescriptorHeap::ReleaseStaleAllocations(Uint64 NumCompletedCmdLists)
{
    {
        std::lock_guard<std::mutex> LockGuard(m_AllocMutex);
		m_MaxStaleSize = std::max(m_MaxStaleSize, static_cast<Uint32>(m_HeapAllocationManager.GetNumStaleDescriptors()));
        m_HeapAllocationManager.ReleaseStaleAllocations(NumCompletedCmdLists);
    }

    {
        std::lock_guard<std::mutex> LockGuard(m_DynAllocMutex);
		m_MaxDynamicStaleSize = std::max(m_MaxDynamicStaleSize, static_cast<Uint32>(m_DynamicAllocationsManager.GetNumStaleDescriptors()));
        m_DynamicAllocationsManager.ReleaseStaleAllocations(NumCompletedCmdLists);
    }
}





DynamicSuballocationsManager::DynamicSuballocationsManager(IMemoryAllocator &Allocator, GPUDescriptorHeap& ParentGPUHeap, Uint32 DynamicChunkSize) :
    m_ParentGPUHeap(ParentGPUHeap),
    m_DynamicChunkSize(DynamicChunkSize),
    m_Suballocations(STD_ALLOCATOR_RAW_MEM(DescriptorHeapAllocation, GetRawAllocator(), "Allocator for vector<DescriptorHeapAllocation>"))
{
}

void DynamicSuballocationsManager::DiscardAllocations(Uint64 FrameNumber)
{
    // Clear the list and dispose all allocated chunks of GPU descriptor heap.
    // The chunks will be added to the release queue in the allocations manager
    m_Suballocations.clear();
}

DescriptorHeapAllocation DynamicSuballocationsManager::Allocate(Uint32 Count)
{
    // This method is intentionally lock-free as it is expected to
    // be called through device context from single thread only

    // Check if there are no chunks or the last chunk does not have enough space
    if( m_Suballocations.empty() || 
        m_CurrentSuballocationOffset + Count > m_Suballocations.back().GetNumHandles() )
    {
        // Request a new chunk from the parent GPU descriptor heap
        auto SuballocationSize = std::max(m_DynamicChunkSize, Count);
        auto NewDynamicSubAllocation = m_ParentGPUHeap.AllocateDynamic(SuballocationSize);
        if (NewDynamicSubAllocation.GetCpuHandle().ptr == 0)
        {
            LOG_ERROR_MESSAGE("Failed to suballocate region for dynamic descriptors")
            return DescriptorHeapAllocation();
        }
        m_Suballocations.emplace_back(std::move(NewDynamicSubAllocation));
        m_CurrentSuballocationOffset = 0;
    }

    // Perform suballocation from the last chunk
    auto &CurrentSuballocation = m_Suballocations.back();
    
    auto ManagerId = CurrentSuballocation.GetAllocationManagerId();
    VERIFY(ManagerId < std::numeric_limits<Uint16>::max(), "ManagerID exceed allowed limit");
    DescriptorHeapAllocation Allocation( this, 
                                         CurrentSuballocation.GetDescriptorHeap(), 
                                         CurrentSuballocation.GetCpuHandle(m_CurrentSuballocationOffset), 
                                         CurrentSuballocation.GetGpuHandle(m_CurrentSuballocationOffset), 
                                         Count, 
                                         static_cast<Uint16>(ManagerId) );
	m_CurrentSuballocationOffset += Count;

    return Allocation;
}

void DynamicSuballocationsManager::Free(DescriptorHeapAllocation&& Allocation)
{
    // Do nothing. Dynamic allocations are not disposed individually, but as whole chunks at the end of the frame
    Allocation = DescriptorHeapAllocation();
}

}
