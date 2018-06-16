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
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

namespace Diligent
{

// Vulkand dynamic heap implementation consists of a single ring buffer and a number of dynamic heaps,
// one per context. Every dynamic heap suballocates chunk of memory from the global ring buffer. Within
// every chunk, memory is allocated in simple lock-free linear fashion:
//   
//  | <----------------------frame 0---------------------------->|<-----------------frame 1-------------->
//  | Ctx0-f0-Chunk0  |  Ctx1-f0-Chunk0 | Ctx0-f0-Chunk1 |  ...  |  Ctx0-f1-Chunk0 | Ctx1-f1-Chunk0 |....
//

class RenderDeviceVkImpl;
class VulkanRingBuffer;

// sizeof(VulkanDynamicAllocation) must be at least 16 to avoid false cache line sharing problems
struct VulkanDynamicAllocation
{
    VulkanDynamicAllocation(){}

    VulkanDynamicAllocation(VulkanRingBuffer& _ParentHeap, size_t _Offset, size_t _Size) :
        pParentDynamicHeap(&_ParentHeap),
        Offset           (_Offset), 
        Size             (_Size)
    {}

    VulkanDynamicAllocation             (const VulkanDynamicAllocation&) = delete;
    VulkanDynamicAllocation& operator = (const VulkanDynamicAllocation&) = delete;
    VulkanDynamicAllocation             (VulkanDynamicAllocation&& rhs)noexcept :
        pParentDynamicHeap(rhs.pParentDynamicHeap),
        Offset            (rhs.Offset),
        Size              (rhs.Size)
#ifdef _DEBUG
        , dbgFrameNumber(rhs.dbgFrameNumber)
#endif
    {
        rhs.pParentDynamicHeap = nullptr;
        rhs.Offset = 0;
        rhs.Size = 0;
#ifdef _DEBUG
        rhs.dbgFrameNumber = 0;
#endif
    }

    VulkanDynamicAllocation& operator = (VulkanDynamicAllocation&& rhs)noexcept // Must be noexcept on MSVC, so can't use = default
    {
        pParentDynamicHeap = rhs.pParentDynamicHeap;
        Offset             = rhs.Offset;
        Size               = rhs.Size;
        rhs.pParentDynamicHeap = nullptr;
        rhs.Offset             = 0;
        rhs.Size               = 0;
#ifdef _DEBUG
        dbgFrameNumber = rhs.dbgFrameNumber;
        rhs.dbgFrameNumber = 0;
#endif
        return *this;
    }

    VulkanRingBuffer* pParentDynamicHeap = nullptr;
    size_t             Offset             = 0;		// Offset from the start of the buffer resource
    size_t             Size               = 0;	    // Reserved size of this allocation
#ifdef _DEBUG
    Uint64             dbgFrameNumber     = 0;
#endif
};

class VulkanRingBuffer
{
public:
    VulkanRingBuffer(IMemoryAllocator&         Allocator, 
                     class RenderDeviceVkImpl* pDeviceVk, 
                     Uint32                    Size);
    ~VulkanRingBuffer();

    VulkanRingBuffer            (const VulkanRingBuffer&) = delete;
    VulkanRingBuffer            (VulkanRingBuffer&&)      = delete;
    VulkanRingBuffer& operator= (const VulkanRingBuffer&) = delete;
    VulkanRingBuffer& operator= (VulkanRingBuffer&&)      = delete;

    void FinishFrame(Uint64 FenceValue, Uint64 LastCompletedFenceValue);
    void Destroy();

    VkBuffer GetVkBuffer()  const{return m_VkBuffer;}
    Uint8*   GetCPUAddress()const{return m_CPUAddress;}

private:
    friend class VulkanDynamicHeap;

    static constexpr const Uint32 MinAlignment = 1024;
    RingBuffer::OffsetType Allocate(size_t SizeInBytes);

    std::mutex                  m_RingBuffMtx;
    RingBuffer                  m_RingBuffer;
    RenderDeviceVkImpl* const   m_pDeviceVk;

    VulkanUtilities::BufferWrapper       m_VkBuffer;
    VulkanUtilities::DeviceMemoryWrapper m_BufferMemory;
    Uint8*                               m_CPUAddress;
    const VkDeviceSize                   m_DefaultAlignment;
    RingBuffer::OffsetType               m_TotalPeakSize    = 0;
    RingBuffer::OffsetType               m_CurrentFrameSize = 0;
    RingBuffer::OffsetType               m_FramePeakSize    = 0;
};


class VulkanDynamicHeap
{
public:
    VulkanDynamicHeap(VulkanRingBuffer& ParentRingBuffer, std::string HeapName, Uint32 PageSize) :
        m_ParentRingBuffer(ParentRingBuffer),
        m_HeapName(std::move(HeapName)),
        m_PagSize(PageSize)
    {}

    VulkanDynamicHeap            (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap            (VulkanDynamicHeap&&)      = delete;
    VulkanDynamicHeap& operator= (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap& operator= (VulkanDynamicHeap&&)      = delete;
    
    ~VulkanDynamicHeap();

    VulkanDynamicAllocation Allocate(Uint32 SizeInBytes, Uint32 Alignment);

    void Reset()
    {
        m_CurrOffset = RingBuffer::InvalidOffset;
        m_AvailableSize     = 0;

        m_CurrAllocatedSize = 0;
        m_CurrUsedSize      = 0;
    }

private:
    VulkanRingBuffer& m_ParentRingBuffer;
    const std::string m_HeapName;

    RingBuffer::OffsetType m_CurrOffset = RingBuffer::InvalidOffset;
    const Uint32 m_PagSize;
    Uint32 m_AvailableSize     = 0;

    Uint32 m_CurrAllocatedSize = 0;
    Uint32 m_CurrUsedSize      = 0;
    Uint32 m_PeakAllocatedSize = 0;
    Uint32 m_PeakUsedSize      = 0;
};

}
