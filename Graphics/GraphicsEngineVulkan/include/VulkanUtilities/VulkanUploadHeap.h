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

#include <unordered_map>
#include "VulkanUtilities/VulkanMemoryManager.h"

namespace VulkanUtilities
{

struct VulkanUploadAllocation
{
    VulkanUploadAllocation(){}
    VulkanUploadAllocation(VulkanMemoryAllocation&& _MemAllocation, VkBuffer _vkBuffer) :
        MemAllocation(std::move(_MemAllocation)),
        vkBuffer     (_vkBuffer)
    {}
    VulkanUploadAllocation             (const VulkanUploadAllocation&) = delete;
    VulkanUploadAllocation& operator = (const VulkanUploadAllocation&) = delete;
    VulkanUploadAllocation             (VulkanUploadAllocation&&) = default;
    VulkanUploadAllocation& operator = (VulkanUploadAllocation&&) = default;

    VulkanMemoryAllocation MemAllocation;
    VkBuffer vkBuffer = VK_NULL_HANDLE;	    // Vulkan buffer associated with this memory.
};

class VulkanUploadHeap : public VulkanMemoryManager
{
public:
    VulkanUploadHeap(std::string                  MgrName,
                     const VulkanLogicalDevice&   LogicalDevice, 
                     const VulkanPhysicalDevice&  PhysicalDevice, 
                     Diligent::IMemoryAllocator&  Allocator, 
                     VkDeviceSize                 HostVisiblePageSize,
                     VkDeviceSize                 HostVisibleReserveSize);
    
    VulkanUploadHeap(VulkanUploadHeap&& rhs)noexcept :
        VulkanMemoryManager(std::move(rhs)),
        m_StagingBufferMemoryTypeIndex(rhs.m_StagingBufferMemoryTypeIndex),
        m_Buffers(std::move(rhs.m_Buffers))
    {
    }
    VulkanUploadHeap            (const VulkanUploadHeap&) = delete;
    VulkanUploadHeap& operator= (VulkanUploadHeap&)       = delete;
    VulkanUploadHeap& operator= (VulkanUploadHeap&& rhs)  = delete;

    ~VulkanUploadHeap();

    VulkanUploadAllocation Allocate(size_t SizeInBytes);

private:
    virtual void OnNewPageCreated(VulkanMemoryPage& NewPage);
    virtual void OnPageDestroy(VulkanMemoryPage& Page);
    VkBufferCreateInfo GetStagingBufferCI()const;
    
    uint32_t m_StagingBufferMemoryTypeIndex = 0;
    std::mutex m_BuffersMtx;
    std::unordered_map<VulkanMemoryPage*, VulkanUtilities::BufferWrapper> m_Buffers;

    // If adding new member, do not forget to update move ctor
};

}
