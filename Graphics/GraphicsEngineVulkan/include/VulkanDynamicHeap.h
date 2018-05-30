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

#include "RingBuffer.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

namespace Diligent
{

// Constant blocks must be multiples of 16 constants @ 16 bytes each
#define DEFAULT_ALIGN 256

class RenderDeviceVkImpl;

struct VulkanDynamicAllocation
{
	VulkanDynamicAllocation(VkBuffer _Buff, size_t _Offset, size_t _Size, void *_CPUAddress)
		: vkBuffer(_Buff), Offset(_Offset), Size(_Size), CPUAddress(_CPUAddress)
    {}

    VkBuffer vkBuffer = VK_NULL_HANDLE;	    // Vulkan buffer associated with this memory.
	size_t Offset = 0;			            // Offset from the start of the buffer resource
	size_t Size = 0;			            // Reserved size of this allocation
    void *CPUAddress = nullptr;
#ifdef _DEBUG
    Uint64 FrameNum = static_cast<Uint64>(-1);
#endif
};

class VulkanRingBuffer : public RingBuffer
{
public:
    VulkanRingBuffer(size_t MaxSize, IMemoryAllocator &Allocator, RenderDeviceVkImpl* pDeviceVk);
    
    VulkanRingBuffer(VulkanRingBuffer&& rhs)noexcept : 
        RingBuffer(std::move(rhs)),
        m_pDeviceVk(rhs.m_pDeviceVk),
        m_VkBuffer(std::move(rhs.m_VkBuffer)),
        m_CPUAddress(rhs.m_CPUAddress)
    {
        rhs.m_CPUAddress = nullptr;
    }

    VulkanRingBuffer            (const VulkanRingBuffer&) = delete;
    VulkanRingBuffer& operator= (VulkanRingBuffer&)       = delete;
    VulkanRingBuffer& operator= (VulkanRingBuffer&& rhs)
    {
        Destroy();

        static_cast<RingBuffer&>(*this) = std::move(rhs);
        m_pDeviceVk = rhs.m_pDeviceVk;
        m_VkBuffer = std::move(rhs.m_VkBuffer);
        m_CPUAddress = rhs.m_CPUAddress;
        rhs.m_CPUAddress = nullptr;
        
        return *this;
    }

    ~VulkanRingBuffer();

    VulkanDynamicAllocation Allocate(size_t SizeInBytes)
    {
        auto Offset = RingBuffer::Allocate(SizeInBytes);
        if (Offset != RingBuffer::InvalidOffset)
        {
            return VulkanDynamicAllocation {m_VkBuffer, Offset, SizeInBytes, m_CPUAddress + Offset};
        }
        else
        {
            return VulkanDynamicAllocation {nullptr, 0, 0, nullptr};
        }
    }

private:
    void Destroy();

    RenderDeviceVkImpl* m_pDeviceVk;
    VulkanUtilities::BufferWrapper m_VkBuffer;
    VulkanUtilities::DeviceMemoryWrapper m_BufferMemory;
    Uint8* m_CPUAddress;
};

class VulkanDynamicHeap
{
public:
	VulkanDynamicHeap(IMemoryAllocator &Allocator, class RenderDeviceVkImpl* pDeviceVk, size_t InitialSize);
    
    VulkanDynamicHeap            (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap            (VulkanDynamicHeap&&)      = delete;
    VulkanDynamicHeap& operator= (const VulkanDynamicHeap&) = delete;
    VulkanDynamicHeap& operator= (VulkanDynamicHeap&&)      = delete;

	VulkanDynamicAllocation Allocate( size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN );

    void FinishFrame(Uint64 FenceValue, Uint64 LastCompletedFenceValue);

private:
    // When a chunk of dynamic memory is requested, the heap first tries to allocate the memory in the largest GPU buffer. 
    // If allocation fails, a new ring buffer is created that provides enough space and requests memory from that buffer.
    // Only the largest buffer is used for allocation and all other buffers are released when GPU is done with corresponding frames
    std::vector<VulkanRingBuffer, STDAllocatorRawMem<VulkanRingBuffer> > m_RingBuffers;
    IMemoryAllocator &m_Allocator;
    RenderDeviceVkImpl* m_pDeviceVk = nullptr;
    //std::mutex m_Mutex;
};

}
