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

#include "pch.h"
#include "VulkanDynamicHeap.h"
#include "RenderDeviceVkImpl.h"

namespace Diligent
{
VulkanRingBuffer::VulkanRingBuffer(size_t MaxSize, IMemoryAllocator &Allocator, RenderDeviceVkImpl* pDeviceVk) :
    RingBuffer(MaxSize, Allocator),
    m_pDeviceVk(pDeviceVk)
{
    VkBufferCreateInfo VkBuffCI = {};
    VkBuffCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    VkBuffCI.pNext = nullptr;
    VkBuffCI.flags = 0; // VK_BUFFER_CREATE_SPARSE_BINDING_BIT, VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT, VK_BUFFER_CREATE_SPARSE_ALIASED_BIT
    VkBuffCI.size = MaxSize;
    VkBuffCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkBuffCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffCI.queueFamilyIndexCount = 0;
    VkBuffCI.pQueueFamilyIndices = nullptr;

    const auto& LogicalDevice = pDeviceVk->GetLogicalDevice();
    m_VkBuffer = LogicalDevice.CreateBuffer(VkBuffCI, "Upload buffer");
    VkMemoryRequirements MemReqs = LogicalDevice.GetBufferMemoryRequirements(m_VkBuffer);

    const auto& PhysicalDevice = pDeviceVk->GetPhysicalDevice();
       
    VkMemoryAllocateInfo MemAlloc = {};
    MemAlloc.pNext = nullptr;
    MemAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemAlloc.allocationSize = MemReqs.size;
            
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit specifies that the host cache management commands vkFlushMappedMemoryRanges 
    // and vkInvalidateMappedMemoryRanges are NOT needed to flush host writes to the device or make device writes visible
    // to the host (10.2)
    MemAlloc.memoryTypeIndex = PhysicalDevice.GetMemoryTypeIndex(MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VERIFY(MemAlloc.memoryTypeIndex != VulkanUtilities::VulkanPhysicalDevice::InvalidMemoryTypeIndex,
            "Vulkan spec requires that for a VkBuffer not created with the "
            "VK_BUFFER_CREATE_SPARSE_BINDING_BIT bit set, the memoryTypeBits member always contains at least one bit set "
            "corresponding to a VkMemoryType with a propertyFlags that has both the VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT bit "
            "and the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit set(11.6)");

    m_BufferMemory = LogicalDevice.AllocateDeviceMemory(MemAlloc, "Host-visible memory for upload buffer");
        
    void *Data = nullptr;
    auto err = LogicalDevice.MapMemory(m_BufferMemory, 
        0, // offset
        MemAlloc.allocationSize,
        0, // flags, reserved for future use
        &Data);
    m_CPUAddress = reinterpret_cast<Uint8*>(Data);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to map  memory");

    err = LogicalDevice.BindBufferMemory(m_VkBuffer, m_BufferMemory, 0 /*offset*/);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to bind  bufer memory");

    LOG_INFO_MESSAGE("GPU ring buffer created. Size: ", MaxSize);
}

void VulkanRingBuffer::Destroy()
{
    if(m_VkBuffer)
    {
        LOG_INFO_MESSAGE("Destroying GPU ring buffer. Size: ", GetMaxSize());
        m_pDeviceVk->GetLogicalDevice().UnmapMemory(m_BufferMemory);
        m_pDeviceVk->SafeReleaseVkObject(std::move(m_VkBuffer));
        m_pDeviceVk->SafeReleaseVkObject(std::move(m_BufferMemory));
    }
    m_CPUAddress = nullptr;
}

VulkanRingBuffer::~VulkanRingBuffer()
{
    Destroy();
}


VulkanDynamicHeap::VulkanDynamicHeap(IMemoryAllocator &Allocator, RenderDeviceVkImpl* pDevice, size_t InitialSize) :
    m_Allocator(Allocator),
    m_pDeviceVk(pDevice),
    m_RingBuffers(STD_ALLOCATOR_RAW_MEM(VulkanRingBuffer, GetRawAllocator(), "Allocator for vector<VulkanRingBuffer>"))
{
    m_RingBuffers.emplace_back(InitialSize, Allocator, pDevice);
}


VulkanDynamicAllocation VulkanDynamicHeap::Allocate(size_t SizeInBytes, size_t Alignment /*= DEFAULT_ALIGN*/)
{
    if(Alignment == 0)
        Alignment = DEFAULT_ALIGN;
    // Every device context has its own upload heap, so there is no need to lock

    //std::lock_guard<std::mutex> Lock(m_Mutex);

    //
    //      Deferred contexts must not update resources or map dynamic buffers
    //      across several frames!
    //

	const size_t AlignmentMask = Alignment - 1;
	// Assert that it's a power of two.
	VERIFY_EXPR((AlignmentMask & Alignment) == 0);
	// Align the allocation
	const size_t AlignedSize = (SizeInBytes + AlignmentMask) & ~AlignmentMask;
    auto DynAlloc = m_RingBuffers.back().Allocate(AlignedSize);
    if (DynAlloc.vkBuffer == VK_NULL_HANDLE)
    {
        auto NewMaxSize = m_RingBuffers.back().GetMaxSize() * 2;
        while(NewMaxSize < AlignedSize)NewMaxSize*=2;
        m_RingBuffers.emplace_back(NewMaxSize, m_Allocator, m_pDeviceVk);
        DynAlloc = m_RingBuffers.back().Allocate(AlignedSize);
    }
#ifdef _DEBUG
	DynAlloc.FrameNum = m_pDeviceVk->GetCurrentFrameNumber();
#endif
    return DynAlloc;
}

void VulkanDynamicHeap::FinishFrame(Uint64 FenceValue, Uint64 LastCompletedFenceValue)
{
    // Every device context has its own upload heap, so there is no need to lock
    //std::lock_guard<std::mutex> Lock(m_Mutex);

    //
    //      Deferred contexts must not update resources or map dynamic buffers
    //      across several frames!
    //

    size_t NumBuffsToDelete = 0;
    for(size_t Ind = 0; Ind < m_RingBuffers.size(); ++Ind)
    {
        auto &RingBuff = m_RingBuffers[Ind];
        RingBuff.FinishCurrentFrame(FenceValue);
        RingBuff.ReleaseCompletedFrames(LastCompletedFenceValue);
        if ( NumBuffsToDelete == Ind && Ind < m_RingBuffers.size()-1 && RingBuff.IsEmpty())
        {
            ++NumBuffsToDelete;
        }
    }
        
    if(NumBuffsToDelete)
        m_RingBuffers.erase(m_RingBuffers.begin(), m_RingBuffers.begin()+NumBuffsToDelete);
}

}
