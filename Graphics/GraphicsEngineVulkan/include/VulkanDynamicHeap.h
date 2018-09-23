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
#include "VulkanUtilities/VulkanMemoryManager.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"
#include "DynamicHeap.h"

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

    VulkanDynamicAllocation(VulkanDynamicMemoryManager& _DynamicMemMgr, size_t _AlignedOffset, size_t _Size)noexcept :
        pDynamicMemMgr(&_DynamicMemMgr),
        AlignedOffset (_AlignedOffset), 
        Size          (_Size)
    {}

    VulkanDynamicAllocation             (const VulkanDynamicAllocation&) = delete;
    VulkanDynamicAllocation& operator = (const VulkanDynamicAllocation&) = delete;
    VulkanDynamicAllocation             (VulkanDynamicAllocation&& rhs)noexcept :
        pDynamicMemMgr(rhs.pDynamicMemMgr),
        AlignedOffset (rhs.AlignedOffset),
        Size          (rhs.Size)
#ifdef DEVELOPMENT
        , dvpFrameNumber(rhs.dvpFrameNumber)
#endif
    {
        rhs.pDynamicMemMgr = nullptr;
        rhs.AlignedOffset  = 0;
        rhs.Size           = 0;
#ifdef DEVELOPMENT
        rhs.dvpFrameNumber = 0;
#endif
    }

    VulkanDynamicAllocation& operator = (VulkanDynamicAllocation&& rhs)noexcept // Must be noexcept on MSVC, so can't use = default
    {
        pDynamicMemMgr = rhs.pDynamicMemMgr;
        AlignedOffset  = rhs.AlignedOffset;
        Size           = rhs.Size;
        rhs.pDynamicMemMgr = nullptr;
        rhs.AlignedOffset  = 0;
        rhs.Size           = 0;
#ifdef DEVELOPMENT
        dvpFrameNumber     = rhs.dvpFrameNumber;
        rhs.dvpFrameNumber = 0;
#endif
        return *this;
    }

    VulkanDynamicMemoryManager* pDynamicMemMgr = nullptr;
    size_t                      AlignedOffset  = 0;		// Offset from the start of the buffer
    size_t                      Size           = 0;	    // Reserved size of this allocation
#ifdef DEVELOPMENT
    Int64                       dvpFrameNumber = 0;
#endif
};



// We cannot use global memory manager for dynamic resources because they
// need to use the same Vulkan buffer
class VulkanDynamicMemoryManager : public DynamicHeap::MasterBlockListBasedManager // or MasterBlockRingBufferBasedManager
{
public:
    using TBase = DynamicHeap::MasterBlockListBasedManager;
    using OffsetType = TBase::OffsetType;
    using TBase::InvalidOffset;
    using MasterBlock = TBase::MasterBlock;

    VulkanDynamicMemoryManager(IMemoryAllocator&         Allocator, 
                               class RenderDeviceVkImpl& DeviceVk, 
                               Uint32                    Size,
                               Uint64                    CommandQueueMask);
    ~VulkanDynamicMemoryManager();

    VulkanDynamicMemoryManager            (const VulkanDynamicMemoryManager&)  = delete;
    VulkanDynamicMemoryManager            (      VulkanDynamicMemoryManager&&) = delete;
    VulkanDynamicMemoryManager& operator= (const VulkanDynamicMemoryManager&)  = delete;
    VulkanDynamicMemoryManager& operator= (      VulkanDynamicMemoryManager&&) = delete;

    VkBuffer GetVkBuffer()  const{return m_VkBuffer;}
    Uint8*   GetCPUAddress()const{return m_CPUAddress;}

    void Destroy();

    static constexpr const Uint32 MasterBlockAlignment = 1024;
    MasterBlock AllocateMasterBlock(OffsetType SizeInBytes, OffsetType Alignment);

private:

    RenderDeviceVkImpl&                  m_DeviceVk;
    VulkanUtilities::BufferWrapper       m_VkBuffer;
    VulkanUtilities::DeviceMemoryWrapper m_BufferMemory;
    Uint8*                               m_CPUAddress;
    const VkDeviceSize                   m_DefaultAlignment;
    const Uint64                         m_CommandQueueMask;
    OffsetType  m_TotalPeakSize    = 0;
};


class VulkanDynamicHeap
{
public:
    VulkanDynamicHeap(VulkanDynamicMemoryManager& DynamicMemMgr, std::string HeapName, Uint32 PageSize) :
        m_DynamicMemMgr(DynamicMemMgr),
        m_HeapName(std::move(HeapName)),
        m_MasterBlockSize(PageSize)
    {}

    VulkanDynamicHeap            (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap            (VulkanDynamicHeap&&)      = delete;
    VulkanDynamicHeap& operator= (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap& operator= (VulkanDynamicHeap&&)      = delete;
    
    ~VulkanDynamicHeap();

    VulkanDynamicAllocation Allocate(Uint32 SizeInBytes, Uint32 Alignment);
    // CmdQueueMask indicates which command queues the allocations from this heap were used
    // with during the last frame
    void FinishFrame(RenderDeviceVkImpl& DeviceVkImpl, Uint64 CmdQueueMask);

    using OffsetType  = VulkanDynamicMemoryManager::OffsetType;
    using MasterBlock = VulkanDynamicMemoryManager::MasterBlock;
    static constexpr OffsetType InvalidOffset = VulkanDynamicMemoryManager::InvalidOffset;

private:
    VulkanDynamicMemoryManager& m_DynamicMemMgr;
    const std::string m_HeapName;

    std::vector<MasterBlock> m_MasterBlocks;

    OffsetType m_CurrOffset = InvalidOffset;
    const Uint32 m_MasterBlockSize;
    Uint32 m_AvailableSize     = 0;

    Uint32 m_CurrAllocatedSize = 0;
    Uint32 m_CurrUsedSize      = 0;
    Uint32 m_PeakAllocatedSize = 0;
    Uint32 m_PeakUsedSize      = 0;
};

}
