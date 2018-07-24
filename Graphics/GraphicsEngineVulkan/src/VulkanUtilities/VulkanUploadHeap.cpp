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
#include "VulkanUtilities/VulkanUploadHeap.h"

namespace VulkanUtilities
{

VkBufferCreateInfo VulkanUploadHeap::GetStagingBufferCI()const
{
    VkBufferCreateInfo StagingBufferCI = {};
    StagingBufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    StagingBufferCI.pNext = nullptr;
    StagingBufferCI.flags = 0; // VK_BUFFER_CREATE_SPARSE_BINDING_BIT, VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT, VK_BUFFER_CREATE_SPARSE_ALIASED_BIT
    StagingBufferCI.size  = m_HostVisiblePageSize;
    StagingBufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    StagingBufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    StagingBufferCI.queueFamilyIndexCount = 0;
    StagingBufferCI.pQueueFamilyIndices = nullptr;
    return StagingBufferCI;
}

VulkanUploadHeap::VulkanUploadHeap(std::string                  MgrName,
                                   const VulkanLogicalDevice&   LogicalDevice, 
                                   const VulkanPhysicalDevice&  PhysicalDevice, 
                                   Diligent::IMemoryAllocator&  Allocator, 
                                   VkDeviceSize                 HostVisiblePageSize,
                                   VkDeviceSize                 HostVisibleReserveSize) :
    VulkanMemoryManager(MgrName, LogicalDevice, PhysicalDevice, Allocator, 0, HostVisiblePageSize, 0, HostVisibleReserveSize)
{
    auto StagingBufferCI = GetStagingBufferCI();
    auto TmpStagingBuffer = LogicalDevice.CreateBuffer(StagingBufferCI);

    VkMemoryRequirements StagingBufferMemReqs = LogicalDevice.GetBufferMemoryRequirements(TmpStagingBuffer);
    m_StagingBufferMemoryTypeIndex = m_PhysicalDevice.GetMemoryTypeIndex(StagingBufferMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VERIFY(m_StagingBufferMemoryTypeIndex != VulkanUtilities::VulkanPhysicalDevice::InvalidMemoryTypeIndex,
           "Vulkan spec requires that for a VkBuffer not created with the VK_BUFFER_CREATE_SPARSE_BINDING_BIT "
           "bit set, or for a VkImage that was created with a VK_IMAGE_TILING_LINEAR value in the tiling member "
           "of the VkImageCreateInfo structure passed to vkCreateImage, the memoryTypeBits member always contains "
           "at least one bit set corresponding to a VkMemoryType with a propertyFlags that has both the "
           "VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT bit AND the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit set. (11.6)");
}

VulkanUploadHeap::~VulkanUploadHeap()
{
    VERIFY_EXPR(m_Pages.size() == m_Buffers.size());
}

VulkanUploadAllocation VulkanUploadHeap::Allocate(size_t SizeInBytes)
{
    VulkanUploadAllocation Allocation;
    Allocation.MemAllocation = VulkanMemoryManager::Allocate(SizeInBytes, 0, m_StagingBufferMemoryTypeIndex, true);

    std::lock_guard<std::mutex> Lock(m_BuffersMtx);
    auto BuffIt = m_Buffers.find(Allocation.MemAllocation.Page);
    VERIFY_EXPR(BuffIt != m_Buffers.end());
    Allocation.vkBuffer = BuffIt->second;

    return Allocation;
}

void VulkanUploadHeap::OnNewPageCreated(VulkanMemoryPage& NewPage)
{
    std::lock_guard<std::mutex> Lock(m_BuffersMtx);
    auto BufferCI = GetStagingBufferCI();
    auto NewBuffer = m_LogicalDevice.CreateBuffer(BufferCI, "Upload buffer");
    auto MemReqs = m_LogicalDevice.GetBufferMemoryRequirements(NewBuffer);
    auto MemoryTypeIndex = m_PhysicalDevice.GetMemoryTypeIndex(MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VERIFY(MemoryTypeIndex == m_StagingBufferMemoryTypeIndex, "Incosistent memory type");
    auto err = m_LogicalDevice.BindBufferMemory(NewBuffer, NewPage.GetVkMemory(), 0);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to bind buffer memory");
    VERIFY(m_Buffers.find(&NewPage) == m_Buffers.end(), "Buffer corresponding to this page already exists");
    m_Buffers.emplace(&NewPage, std::move(NewBuffer));
}

void VulkanUploadHeap::OnPageDestroy(VulkanMemoryPage& Page)
{
    std::lock_guard<std::mutex> Lock(m_BuffersMtx);
    auto ElemsRemoved = m_Buffers.erase(&Page);
    VERIFY_EXPR(ElemsRemoved == 1);
}

}
