/*     Copyright 2015-2016 Egor Yusov
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

    auto DescriptorHandleOffset = m_FreeBlockManager.Allocate(Count);
    if (DescriptorHandleOffset == VariableSizeGPUAllocationsManager::InvalidOffset)
    {
        return DescriptorHeapAllocation();
    }

    auto CPUHandle = m_FirstCPUHandle;
    CPUHandle.ptr += DescriptorHandleOffset * m_DescriptorSize;

    auto GPUHandle = m_FirstGPUHandle;
    if(m_HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        GPUHandle.ptr += DescriptorHandleOffset * m_DescriptorSize;

    VERIFY(m_ThisManagerId < std::numeric_limits<Uint16>::max(), "ManagerID exceed allowed limit");
	DescriptorHeapAllocation Allocation( m_pParentAllocator, m_pd3d12DescriptorHeap, CPUHandle, GPUHandle, Count, static_cast<Uint16>(m_ThisManagerId));

	return Allocation;
}

void DescriptorHeapAllocationManager::Free(DescriptorHeapAllocation&& Allocation)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);

    VERIFY(Allocation.GetAllocationManagerId() == m_ThisManagerId, "Invalid descriptor heap manager Id")

    auto DescriptorOffset = (Allocation.GetCpuHandle().ptr - m_FirstCPUHandle.ptr) / m_DescriptorSize;
    m_FreeBlockManager.Free(DescriptorOffset, Allocation.GetNumHandles(), m_pDeviceD3D12Impl->GetCurrentFrame());
    Allocation = DescriptorHeapAllocation();
}

void DescriptorHeapAllocationManager::ReleaseStaleAllocations(Uint64 NumCompletedFrames)
{
    m_FreeBlockManager.ReleaseCompletedFrames(NumCompletedFrames);
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
    for (auto HeapPoolIt = m_HeapPool.begin(); HeapPoolIt != m_HeapPool.end(); ++HeapPoolIt)
    {
        VERIFY(HeapPoolIt->GetNumAvailableDescriptors() == m_HeapDesc.NumDescriptors, "Not all descriptors in the descriptor pool are released");
    }

    LOG_INFO_MESSAGE("Max ", GetD3D12DescriptorHeapTypeLiteralName(m_HeapDesc.Type), " CPU heap size: ", m_MaxHeapSize);
}

DescriptorHeapAllocation CPUDescriptorHeap::Allocate( uint32_t Count )
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    DescriptorHeapAllocation Allocation;
    for (auto AvailableHeapIt = m_AvailableHeaps.begin(); AvailableHeapIt != m_AvailableHeaps.end(); ++AvailableHeapIt)
    {
        Allocation = m_HeapPool[*AvailableHeapIt].Allocate(Count);
        if(m_HeapPool[*AvailableHeapIt].GetNumAvailableDescriptors() == 0)
            m_AvailableHeaps.erase(*AvailableHeapIt);

        if(Allocation.GetCpuHandle().ptr != 0)
            break;
    }

    if(Allocation.GetCpuHandle().ptr == 0)
    {
        m_HeapPool.emplace_back(m_MemAllocator, m_pDeviceD3D12Impl, this, m_HeapPool.size(), m_HeapDesc);
        auto NewHeapIt = m_AvailableHeaps.insert(m_HeapPool.size()-1);

        Allocation = m_HeapPool[*NewHeapIt.first].Allocate(Count);
    }

    m_CurrentSize += (Allocation.GetCpuHandle().ptr != 0) ? Count : 0;
    m_MaxHeapSize = std::max(m_MaxHeapSize, m_CurrentSize);

    return Allocation;
}

void CPUDescriptorHeap::Free(DescriptorHeapAllocation&& Allocation)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    auto ManagerId = Allocation.GetAllocationManagerId();
    m_CurrentSize -= static_cast<Uint32>(Allocation.GetNumHandles());
    m_HeapPool[ManagerId].Free(std::move(Allocation));
}

void CPUDescriptorHeap::ReleaseStaleAllocations(Uint64 NumCompletedFrames)
{
    std::lock_guard<std::mutex> LockGuard(m_AllocationMutex);
    for (size_t HeapManagerInd = 0; HeapManagerInd < m_HeapPool.size(); ++HeapManagerInd)
    {
        m_HeapPool[HeapManagerInd].ReleaseStaleAllocations(NumCompletedFrames);
        if(m_HeapPool[HeapManagerInd].GetNumAvailableDescriptors() > 0)
            m_AvailableHeaps.insert(HeapManagerInd);
    }
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
    LOG_INFO_MESSAGE("Max ", GetD3D12DescriptorHeapTypeLiteralName(m_HeapDesc.Type), " GPU heap static/dynamic size: ", m_MaxHeapSize, "/", m_MaxDynamicSize);
}

DescriptorHeapAllocation GPUDescriptorHeap::Allocate(uint32_t Count)
{
    std::lock_guard<std::mutex> LockGuard(m_Mutex);
    DescriptorHeapAllocation Allocation = m_HeapAllocationManager.Allocate(Count);

    m_CurrentSize += (Allocation.GetCpuHandle().ptr != 0) ? Count : 0;
    m_MaxHeapSize = std::max(m_MaxHeapSize, m_CurrentSize);

    return Allocation;
}

DescriptorHeapAllocation GPUDescriptorHeap::AllocateDynamic(uint32_t Count)
{
    std::lock_guard<std::mutex> LockGuard(m_Mutex);
    DescriptorHeapAllocation Allocation = m_DynamicAllocationsManager.Allocate(Count);

    m_CurrentDynamicSize += (Allocation.GetCpuHandle().ptr != 0) ? Count : 0;
    m_MaxDynamicSize = std::max(m_MaxDynamicSize, m_CurrentDynamicSize);

    return Allocation;
}

void GPUDescriptorHeap::Free(DescriptorHeapAllocation&& Allocation)
{
    auto MgrId = Allocation.GetAllocationManagerId();
    VERIFY(MgrId == 0 || MgrId == 1, "Unexpected allocation manager ID");

    std::lock_guard<std::mutex> LockGuard(m_Mutex);

    if(MgrId == 0)
    {
        m_CurrentSize -= static_cast<Uint32>(Allocation.GetNumHandles());
        m_HeapAllocationManager.Free(std::move(Allocation));
    }
    else
    {
        m_CurrentDynamicSize -= static_cast<Uint32>(Allocation.GetNumHandles());
        m_DynamicAllocationsManager.Free(std::move(Allocation));
    }
}

void GPUDescriptorHeap::ReleaseStaleAllocations(Uint64 NumCompletedFrames)
{
    m_HeapAllocationManager.ReleaseStaleAllocations(NumCompletedFrames);
    m_DynamicAllocationsManager.ReleaseStaleAllocations(NumCompletedFrames);
}





DynamicSuballocationsManager::DynamicSuballocationsManager(IMemoryAllocator &Allocator, GPUDescriptorHeap& ParentGPUHeap, Uint32 DynamicChunkSize) :
    m_ParentGPUHeap(ParentGPUHeap),
    m_DynamicChunkSize(DynamicChunkSize),
    m_Suballocations(STD_ALLOCATOR_RAW_MEM(DescriptorHeapAllocation, GetRawAllocator(), "Allocator for vector<DescriptorHeapAllocation>"))
{
}

void DynamicSuballocationsManager::DiscardAllocations(Uint64 FrameNumber)
{
    m_Suballocations.clear();
}

DescriptorHeapAllocation DynamicSuballocationsManager::Allocate(Uint32 Count)
{
    if( m_Suballocations.empty() || 
        m_CurrentSuballocationOffset + Count > m_Suballocations.back().GetNumHandles() )
    {
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
    // Do nothing
    Allocation = DescriptorHeapAllocation();
}

}
