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
#include <sstream>
#include <iomanip>
#include "VulkanUtilities/VulkanMemoryManager.h"

namespace VulkanUtilities
{
    
VulkanMemoryAllocation::~VulkanMemoryAllocation()
{
    if(Page != nullptr)
    {
        Page->Free(*this);
    }
}

VulkanMemoryPage::VulkanMemoryPage(VulkanMemoryManager& ParentMemoryMgr,
                                   VkDeviceSize         PageSize, 
                                   uint32_t             MemoryTypeIndex,
                                   bool                 MapMemory)noexcept : 
    m_ParentMemoryMgr(ParentMemoryMgr),
    m_AllocationMgr(PageSize, ParentMemoryMgr.m_Allocator)
{
    VkMemoryAllocateInfo MemAlloc = {};
    MemAlloc.pNext = nullptr;
    MemAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemAlloc.allocationSize = PageSize;
    MemAlloc.memoryTypeIndex = MemoryTypeIndex;

    std::stringstream ss;
    ss << "Device memory page. Size: " << (PageSize >> 10) << " Kb, type: " << MemoryTypeIndex;
    m_VkMemory = ParentMemoryMgr.m_LogicalDevice.AllocateDeviceMemory(MemAlloc, ss.str().c_str());

    if(MapMemory)
    {
        auto err = ParentMemoryMgr.m_LogicalDevice.MapMemory(m_VkMemory, 
            0, // offset
            PageSize,
            0, // flags, reserved for future use
            &m_CPUMemory);
        CHECK_VK_ERROR_AND_THROW(err, "Failed to map staging memory");
    }
}

VulkanMemoryPage::~VulkanMemoryPage()
{
    if(m_CPUMemory != nullptr)
    {
        // Unmapping memory is not necessary, byt anyway
        m_ParentMemoryMgr.m_LogicalDevice.UnmapMemory(m_VkMemory);
    }

    VERIFY(IsEmpty(), "Destroying a page with not all allocations released");
}

VulkanMemoryAllocation VulkanMemoryPage::Allocate(VkDeviceSize size)
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    auto Offset = m_AllocationMgr.Allocate(size);
    if(Offset != Diligent::VariableSizeAllocationsManager::InvalidOffset)
    {
        return VulkanMemoryAllocation{this, Offset, size};
    }
    else
    {
        return VulkanMemoryAllocation{};
    }
}

void VulkanMemoryPage::Free(VulkanMemoryAllocation& Allocation)
{
    m_ParentMemoryMgr.OnFreeAllocation(Allocation.Size, m_CPUMemory != nullptr);
    std::lock_guard<std::mutex> Lock(m_Mutex);
    m_AllocationMgr.Free(Allocation.UnalignedOffset, Allocation.Size);
}

VulkanMemoryAllocation VulkanMemoryManager::Allocate(const VkMemoryRequirements& MemReqs, VkMemoryPropertyFlags MemoryProps)
{
    // memoryTypeBits is a bitmask and contains one bit set for every supported memory type for the resource. 
    // Bit i is set if and only if the memory type i in the VkPhysicalDeviceMemoryProperties structure for the 
    // physical device is supported for the resource.
    auto MemoryTypeIndex = m_PhysicalDevice.GetMemoryTypeIndex(MemReqs.memoryTypeBits, MemoryProps);
    if(MemoryProps == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        // There must be at least one memory type with the DEVICE_LOCAL_BIT bit set
        VERIFY(MemoryTypeIndex != VulkanUtilities::VulkanPhysicalDevice::InvalidMemoryTypeIndex,
               "Vulkan spec requires that memoryTypeBits member always contains "
               "at least one bit set corresponding to a VkMemoryType with a propertyFlags that has the "
               "VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT bit set (11.6)");
    }
    else if( (MemoryProps & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        VERIFY(MemoryTypeIndex != VulkanUtilities::VulkanPhysicalDevice::InvalidMemoryTypeIndex,
               "Vulkan spec requires that for a VkBuffer not created with the VK_BUFFER_CREATE_SPARSE_BINDING_BIT "
               "bit set, or for a VkImage that was created with a VK_IMAGE_TILING_LINEAR value in the tiling member "
               "of the VkImageCreateInfo structure passed to vkCreateImage, the memoryTypeBits member always contains "
               "at least one bit set corresponding to a VkMemoryType with a propertyFlags that has both the "
               "VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT bit AND the VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit set. (11.6)");
    }
    else if(MemoryTypeIndex == VulkanUtilities::VulkanPhysicalDevice::InvalidMemoryTypeIndex)
    {
        LOG_ERROR_AND_THROW("Failed to find suitable device memory type for a buffer");
    }

    auto Size = MemReqs.size + MemReqs.alignment;
    VulkanMemoryAllocation Allocation;

    std::lock_guard<std::mutex> Lock(m_Mutex);
    auto range = m_Pages.equal_range(MemoryTypeIndex);
    for(auto page_it = range.first; page_it != range.second; ++page_it)
    {
        Allocation = page_it->second.Allocate(Size);
        if(Allocation.Page != nullptr)
            break;
    }

    bool HostVisible = (MemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    size_t stat_ind = HostVisible ? 1 : 0;
    if(Allocation.Page == nullptr)
    {
        auto PageSize = HostVisible ? m_HostVisiblePageSize : m_DeviceLocalPageSize;
        while(PageSize < Size)
            PageSize *= 2;

        m_CurrAllocatedSize[stat_ind] += PageSize;
        m_PeakAllocatedSize[stat_ind] = std::max(m_PeakAllocatedSize[stat_ind], m_CurrAllocatedSize[stat_ind]);

        auto it = m_Pages.emplace(MemoryTypeIndex, VulkanMemoryPage{*this, PageSize, MemoryTypeIndex, HostVisible});
        LOG_INFO_MESSAGE("VulkanMemoryManager '", m_MgrName, "': created new ", (HostVisible ? "host-visible" : "device-local"), " page. Size: ", 
                         std::fixed, std::setprecision(2), PageSize / double{1 << 20}, " MB. Memory type: ", MemoryTypeIndex, 
                         ". Current allocated size: ", std::fixed, std::setprecision(2), m_CurrAllocatedSize[stat_ind] / double{1 << 20});
        Allocation = it->second.Allocate(Size);
        VERIFY(Allocation.Page != nullptr, "Failed to allocate new memory page");
    }

    m_CurrUsedSize[stat_ind].fetch_add(Size);
    m_PeakUsedSize[stat_ind] = std::max(m_PeakUsedSize[stat_ind], static_cast<VkDeviceSize>(m_CurrUsedSize[stat_ind].load()));

    return Allocation;
}

void VulkanMemoryManager::ShrinkMemory()
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    auto it = m_Pages.begin();
    while(it != m_Pages.end())
    {
        auto curr_it = it;
        ++it;
        auto& Page = curr_it->second;
        if(Page.IsEmpty())
        {
            auto PageSize = Page.GetPageSize();
            bool IsHostVisible = Page.GetCPUMemory() != nullptr;
            m_CurrAllocatedSize[IsHostVisible ? 1 : 0] -= PageSize;
            LOG_INFO_MESSAGE("VulkanMemoryManager '", m_MgrName, "': destroying ", (IsHostVisible ? "host-visible" : "device-local"), " page. Size: ", 
                             std::fixed, std::setprecision(2), PageSize / double{1 << 20}, 
                             ". Current allocated size: ", std::fixed, std::setprecision(2), m_CurrAllocatedSize[IsHostVisible ? 1 : 0] / double{1 << 20});
            m_Pages.erase(curr_it);
        }
    }
}

void VulkanMemoryManager::OnFreeAllocation(VkDeviceSize Size, bool IsHostVisble)
{
    m_CurrUsedSize[IsHostVisble ? 1 : 0].fetch_add( -static_cast<int64_t>(Size) );
}

VulkanMemoryManager::~VulkanMemoryManager()
{
    LOG_INFO_MESSAGE("VulkanMemoryManager '", m_MgrName, "' stats:\n"
                     "    Peak used/peak allocated device-local memory size: ", 
                     std::fixed, std::setprecision(2), m_PeakUsedSize[0] / double{1 << 20}, "/",
                     std::fixed, std::setprecision(2), m_PeakAllocatedSize[0] / double{1 << 20}, " MB. "
                     "\n    Peak used/peak allocated host-visible memory size: ", 
                     std::fixed, std::setprecision(2), m_PeakUsedSize[1] / double{1 << 20}, "/",
                     std::fixed, std::setprecision(2), m_PeakAllocatedSize[1] / double{1 << 20}, " MB.");
    VERIFY(m_Pages.empty(), "Not all pages have been released");
    VERIFY_EXPR(m_CurrAllocatedSize[0] == 0 && m_CurrAllocatedSize[1] == 0 && m_CurrUsedSize[0] == 0 && m_CurrUsedSize[1] == 0);
}

}
