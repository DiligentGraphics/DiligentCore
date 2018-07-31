/*     Copyright 2015-2018 Egor Yusov
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

#pragma once

#include <mutex>
#include "Vulkan.h"
#include "RingBuffer.h"
#include "VulkanUtilities/VulkanMemoryManager.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

namespace Diligent
{

// Vulkand dynamic heap implementation consists of a number of dynamic heaps, one per context.
// Every dynamic heap suballocates chunk of memory from the global memory manager. Within
// every chunk, memory is allocated in a simple lock-free linear fashion. All used allocations are discarded
// when FinishFrame() is called

class RenderDeviceVkImpl;
class VulkanRingBuffer;
class VulkanDynamicMemoryManager;

// sizeof(VulkanDynamicAllocation) must be at least 16 to avoid false cache line sharing problems
struct VulkanDynamicAllocation
{
    VulkanDynamicAllocation()noexcept{}

    VulkanDynamicAllocation(VulkanDynamicMemoryManager& _DynamicMemMgr, size_t _Offset, size_t _Size)noexcept :
        pDynamicMemMgr(&_DynamicMemMgr),
        Offset        (_Offset), 
        Size          (_Size)
    {}

    VulkanDynamicAllocation             (const VulkanDynamicAllocation&) = delete;
    VulkanDynamicAllocation& operator = (const VulkanDynamicAllocation&) = delete;
    VulkanDynamicAllocation             (VulkanDynamicAllocation&& rhs)noexcept :
        pDynamicMemMgr(rhs.pDynamicMemMgr),
        Offset        (rhs.Offset),
        Size          (rhs.Size)
#ifdef DEVELOPMENT
        , dvpFrameNumber(rhs.dvpFrameNumber)
#endif
    {
        rhs.pDynamicMemMgr = nullptr;
        rhs.Offset = 0;
        rhs.Size = 0;
#ifdef DEVELOPMENT
        rhs.dvpFrameNumber = 0;
#endif
    }

    VulkanDynamicAllocation& operator = (VulkanDynamicAllocation&& rhs)noexcept // Must be noexcept on MSVC, so can't use = default
    {
        pDynamicMemMgr = rhs.pDynamicMemMgr;
        Offset         = rhs.Offset;
        Size           = rhs.Size;
        rhs.pDynamicMemMgr = nullptr;
        rhs.Offset         = 0;
        rhs.Size           = 0;
#ifdef DEVELOPMENT
        dvpFrameNumber = rhs.dvpFrameNumber;
        rhs.dvpFrameNumber = 0;
#endif
        return *this;
    }

    VulkanDynamicMemoryManager* pDynamicMemMgr = nullptr;
    size_t                      Offset         = 0;		// Offset from the start of the buffer resource
    size_t                      Size           = 0;	    // Reserved size of this allocation
#ifdef DEVELOPMENT
    Int64                       dvpFrameNumber = 0;
#endif
};


// Having global ring buffer shared between all contexts is inconvinient because all contexts
// must share the same frame. Having individual ring bufer per context may result in a lot of unused
// memory. As a result, ring buffer is not currently used for dynamic memory management.
// Instead, every dynamic heap allocates pages from the global dynamic memory manager.
class RingBufferAllocationStrategy
{
public:
    using OffsetType    = RingBuffer::OffsetType;
    static constexpr const OffsetType InvalidOffset = RingBuffer::InvalidOffset;

    RingBufferAllocationStrategy(IMemoryAllocator& Allocator, 
                                 Uint32            Size) : 
        m_RingBuffer(Size, Allocator)
    {}

    RingBufferAllocationStrategy            (const RingBufferAllocationStrategy&) = delete;
    RingBufferAllocationStrategy            (RingBufferAllocationStrategy&&)      = delete;
    RingBufferAllocationStrategy& operator= (const RingBufferAllocationStrategy&) = delete;
    RingBufferAllocationStrategy& operator= (RingBufferAllocationStrategy&&)      = delete;

    void DiscardAllocations(const std::vector<std::pair<OffsetType, OffsetType>>& Allocations, Uint64 FenceValue)
    {
        std::lock_guard<std::mutex> Lock(m_RingBufferMtx);
        m_RingBuffer.FinishCurrentFrame(FenceValue);
    }

    void ReleaseStaleAllocations(Uint64 LastCompletedFenceValue)
    {
        std::lock_guard<std::mutex> Lock(m_RingBufferMtx);
        m_RingBuffer.ReleaseCompletedFrames(LastCompletedFenceValue);
    }

    OffsetType Allocate(OffsetType SizeInBytes)
    {
        std::lock_guard<std::mutex> Lock(m_RingBufferMtx);
        return m_RingBuffer.Allocate(SizeInBytes);
    }

    OffsetType GetSize()    const{return m_RingBuffer.GetMaxSize();}
    OffsetType GetUsedSize()const{return m_RingBuffer.GetUsedSize();}
private:
    std::mutex m_RingBufferMtx;
    RingBuffer m_RingBuffer;
};


class ListBasedAllocationStrategy
{
public:
    using OffsetType    = VariableSizeAllocationsManager::OffsetType;
    static constexpr const OffsetType InvalidOffset = VariableSizeAllocationsManager::InvalidOffset;

    ListBasedAllocationStrategy(IMemoryAllocator& Allocator, 
                                Uint32            Size) : 
        m_AllocationsMgr(Size, Allocator)
    {}

    ListBasedAllocationStrategy            (const ListBasedAllocationStrategy&) = delete;
    ListBasedAllocationStrategy            (ListBasedAllocationStrategy&&)      = delete;
    ListBasedAllocationStrategy& operator= (const ListBasedAllocationStrategy&) = delete;
    ListBasedAllocationStrategy& operator= (ListBasedAllocationStrategy&&)      = delete;

    void DiscardAllocations(const std::vector<std::pair<OffsetType, OffsetType>>& Allocations, Uint64 FenceValue)
    {
        std::lock_guard<std::mutex> Lock(m_ReleaseQueueMtx);
        for(const auto& Allocation : Allocations)
            m_ReleaseQueue.emplace_back(Allocation.first, Allocation.second, FenceValue);
    }

    void ReleaseStaleAllocations(Uint64 LastCompletedFenceValue)
    {
        std::lock_guard<std::mutex> MgrLock(m_AllocationsMgrMtx);
        std::lock_guard<std::mutex> QueueLock(m_ReleaseQueueMtx);
        while (!m_ReleaseQueue.empty())
        {
            auto &FirstAllocation = m_ReleaseQueue.front();
            if (FirstAllocation.FenceValue <= LastCompletedFenceValue)
            {
                m_AllocationsMgr.Free(FirstAllocation.Offset, FirstAllocation.Size);
                m_ReleaseQueue.pop_front();
            }
            else
                break;
        }
    }

    OffsetType Allocate(OffsetType SizeInBytes)
    {
        std::lock_guard<std::mutex> Lock(m_AllocationsMgrMtx);
        return m_AllocationsMgr.Allocate(SizeInBytes);
    }
    OffsetType GetSize()    const{return m_AllocationsMgr.GetMaxSize();}
    OffsetType GetUsedSize()const{return m_AllocationsMgr.GetUsedSize();}

private:
    std::mutex                      m_AllocationsMgrMtx;
    VariableSizeAllocationsManager  m_AllocationsMgr;

    struct StaleAllocationInfo
    {
        const OffsetType Offset;
        const OffsetType Size;
        const Uint64     FenceValue;
        StaleAllocationInfo(OffsetType _Offset,
                            OffsetType _Size,
                            Uint64     _FenceValue) :
            Offset    (_Offset),
            Size      (_Size),
            FenceValue(_FenceValue)
        {}
    };
    std::deque< StaleAllocationInfo > m_ReleaseQueue;
    std::mutex                        m_ReleaseQueueMtx;
};

// We cannot use global memory manager for dynamic resources because they
// need to use the same Vulkan buffer
class VulkanDynamicMemoryManager
{
public:
    using AllocationStategy = ListBasedAllocationStrategy; // or RingBufferAllocationStrategy
    using OffsetType = AllocationStategy::OffsetType;

    VulkanDynamicMemoryManager(IMemoryAllocator&         Allocator, 
                               class RenderDeviceVkImpl& DeviceVk, 
                               Uint32                    Size);
    ~VulkanDynamicMemoryManager();

    VulkanDynamicMemoryManager            (const VulkanDynamicMemoryManager&) = delete;
    VulkanDynamicMemoryManager            (VulkanDynamicMemoryManager&&)      = delete;
    VulkanDynamicMemoryManager& operator= (const VulkanDynamicMemoryManager&) = delete;
    VulkanDynamicMemoryManager& operator= (VulkanDynamicMemoryManager&&)      = delete;

    void ReleaseStaleAllocations(Uint64 LastCompletedFenceValue)
    {
        m_AllocationStrategy.ReleaseStaleAllocations(LastCompletedFenceValue);
    }

    void DiscardAllocations(const std::vector<std::pair<OffsetType, OffsetType>>& Allocations, Uint64 FenceValue)
    {
        m_AllocationStrategy.DiscardAllocations(Allocations, FenceValue);
    }

    VkBuffer GetVkBuffer()  const{return m_VkBuffer;}
    Uint8*   GetCPUAddress()const{return m_CPUAddress;}

    void Destroy();

    static constexpr const Uint32 MinAlignment = 1024;

private:
    friend class VulkanDynamicHeap;
    OffsetType Allocate(OffsetType SizeInBytes);

    RenderDeviceVkImpl&                  m_DeviceVk;
    VulkanUtilities::BufferWrapper       m_VkBuffer;
    VulkanUtilities::DeviceMemoryWrapper m_BufferMemory;
    Uint8*                               m_CPUAddress;
    const VkDeviceSize                   m_DefaultAlignment;

    AllocationStategy                    m_AllocationStrategy;

    OffsetType  m_TotalPeakSize    = 0;
};


class VulkanDynamicHeap
{
public:
    VulkanDynamicHeap(VulkanDynamicMemoryManager& DynamicMemMgr, std::string HeapName, Uint32 PageSize) :
        m_DynamicMemMgr(DynamicMemMgr),
        m_HeapName(std::move(HeapName)),
        m_PagSize(PageSize)
    {}

    VulkanDynamicHeap            (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap            (VulkanDynamicHeap&&)      = delete;
    VulkanDynamicHeap& operator= (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap& operator= (VulkanDynamicHeap&&)      = delete;
    
    ~VulkanDynamicHeap();

    VulkanDynamicAllocation Allocate(Uint32 SizeInBytes, Uint32 Alignment);
    void FinishFrame(Uint64 FenceValue);

    using OffsetType = VulkanDynamicMemoryManager::AllocationStategy::OffsetType;
    static constexpr OffsetType InvalidOffset = VulkanDynamicMemoryManager::AllocationStategy::InvalidOffset;

private:
    VulkanDynamicMemoryManager& m_DynamicMemMgr;
    const std::string m_HeapName;

    std::vector<std::pair<OffsetType, OffsetType>> m_Allocations;

    OffsetType m_CurrOffset = InvalidOffset;
    const Uint32 m_PagSize;
    Uint32 m_AvailableSize     = 0;

    Uint32 m_CurrAllocatedSize = 0;
    Uint32 m_CurrUsedSize      = 0;
    Uint32 m_PeakAllocatedSize = 0;
    Uint32 m_PeakUsedSize      = 0;
};

}
