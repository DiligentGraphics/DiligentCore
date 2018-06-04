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
#include <array>
#include <unordered_map>
#include <atomic>
#include <string>
#include "MemoryAllocator.h"
#include "VariableSizeAllocationsManager.h"
#include "VulkanUtilities/VulkanPhysicalDevice.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

namespace VulkanUtilities
{

class VulkanMemoryPage;
class VulkanMemoryManager;

struct VulkanMemoryAllocation
{
    VulkanMemoryAllocation()noexcept{}

    VulkanMemoryAllocation            (const VulkanMemoryAllocation&) = delete;
    VulkanMemoryAllocation& operator= (const VulkanMemoryAllocation&) = delete;

	VulkanMemoryAllocation(VulkanMemoryPage* _Page, size_t _UnalignedOffset, size_t _Size)noexcept : 
        Page           (_Page), 
        UnalignedOffset(_UnalignedOffset), 
        Size           (_Size)
    {}
    
    VulkanMemoryAllocation(VulkanMemoryAllocation&& rhs)noexcept :
        Page           (rhs.Page), 
        UnalignedOffset(rhs.UnalignedOffset),
        Size           (rhs.Size)
    {
        rhs.Page            = nullptr;
        rhs.UnalignedOffset = 0;
        rhs.Size            = 0;
    }

    VulkanMemoryAllocation& operator= (VulkanMemoryAllocation&& rhs)noexcept
    {
        Page            = rhs.Page;
        UnalignedOffset = rhs.UnalignedOffset;
        Size            = rhs.Size;

        rhs.Page            = nullptr;
        rhs.UnalignedOffset = 0;
        rhs.Size            = 0;

        return *this;
    }

    // Destructor immediately returns the allocation to the parent page.
    // The allocation must not be in use by the GPU.
    ~VulkanMemoryAllocation();
    
    VulkanMemoryPage* Page             = nullptr;	// Memory page that contains this allocation
	size_t            UnalignedOffset  = 0;         // Unaligned offset from the start of the memory
	size_t            Size             = 0;	        // Reserved size of this allocation
};

class VulkanMemoryPage
{
public:
    VulkanMemoryPage(VulkanMemoryManager& ParentMemoryMgr,
                     VkDeviceSize         PageSize, 
                     uint32_t             MemoryTypeIndex,
                     bool                 MapMemory)noexcept;
    ~VulkanMemoryPage();

    VulkanMemoryPage(VulkanMemoryPage&& rhs)noexcept :
        m_ParentMemoryMgr (rhs.m_ParentMemoryMgr),
        m_AllocationMgr   (std::move(rhs.m_AllocationMgr)),
        m_VkMemory        (std::move(rhs.m_VkMemory)),
        m_CPUMemory       (rhs.m_CPUMemory)
    {
        rhs.m_CPUMemory = nullptr;
    }

    VulkanMemoryPage            (const VulkanMemoryPage&) = delete;
    VulkanMemoryPage& operator= (VulkanMemoryPage&)       = delete;
    VulkanMemoryPage& operator= (VulkanMemoryPage&& rhs)  = delete;

    bool IsEmpty()const{return m_AllocationMgr.IsEmpty();}
    bool IsFull() const{return m_AllocationMgr.IsFull();}
    VkDeviceSize GetPageSize()const{return m_AllocationMgr.GetMaxSize();}
    VkDeviceSize GetUsedSize()const{return m_AllocationMgr.GetUsedSize();}

    VulkanMemoryAllocation Allocate(VkDeviceSize size);

    VkDeviceMemory GetVkMemory()const{return m_VkMemory;}
    void* GetCPUMemory()const{return m_CPUMemory;}
    
private:
    friend struct VulkanMemoryAllocation;
    
    // Memory is reclaimed immediately. The application is responsible to ensure it is not in use by the GPU    
    void Free(VulkanMemoryAllocation& Allocation);

    VulkanMemoryManager&                     m_ParentMemoryMgr;
    std::mutex                               m_Mutex;
    Diligent::VariableSizeAllocationsManager m_AllocationMgr;
    VulkanUtilities::DeviceMemoryWrapper     m_VkMemory;
    void*                                    m_CPUMemory = nullptr;
};

class VulkanMemoryManager
{
public:
	VulkanMemoryManager(std::string                  MgrName,
                        const VulkanLogicalDevice&   LogicalDevice, 
                        const VulkanPhysicalDevice&  PhysicalDevice, 
                        Diligent::IMemoryAllocator&  Allocator, 
                        VkDeviceSize                 DeviceLocalPageSize,
                        VkDeviceSize                 HostVisiblePageSize,
                        VkDeviceSize                 DeviceLocalReserveSize,
                        VkDeviceSize                 HostVisibleReserveSize) : 
        m_MgrName            (std::move(MgrName)),
        m_LogicalDevice      (LogicalDevice),
        m_PhysicalDevice     (PhysicalDevice),
        m_Allocator          (Allocator),
        m_DeviceLocalPageSize(DeviceLocalPageSize),
        m_HostVisiblePageSize(HostVisiblePageSize),
        m_DeviceLocalReserveSize(DeviceLocalReserveSize),
        m_HostVisibleReserveSize(HostVisibleReserveSize)
    {}
    
    ~VulkanMemoryManager();

    VulkanMemoryManager            (const VulkanMemoryManager&) = delete;
    VulkanMemoryManager            (VulkanMemoryManager&&)      = delete;
    VulkanMemoryManager& operator= (const VulkanMemoryManager&) = delete;
    VulkanMemoryManager& operator= (VulkanMemoryManager&&)      = delete;
    
	VulkanMemoryAllocation Allocate(const VkMemoryRequirements& MemReqs, VkMemoryPropertyFlags MemoryProps);
    void ShrinkMemory();

private:
    friend class VulkanMemoryPage;

    const std::string m_MgrName;

    const VulkanLogicalDevice&  m_LogicalDevice;
    const VulkanPhysicalDevice& m_PhysicalDevice;

    Diligent::IMemoryAllocator& m_Allocator;

    std::unordered_multimap<uint32_t, VulkanMemoryPage> m_Pages;
    std::mutex m_Mutex;
    const VkDeviceSize m_DeviceLocalPageSize;
    const VkDeviceSize m_HostVisiblePageSize;
    const VkDeviceSize m_DeviceLocalReserveSize;
    const VkDeviceSize m_HostVisibleReserveSize;
    
    void OnFreeAllocation(VkDeviceSize Size, bool IsHostVisble);

    // 0 == Device local, 1 == Host-visible
    std::array<std::atomic_int64_t, 2> m_CurrUsedSize = {};
    std::array<VkDeviceSize, 2> m_PeakUsedSize = {};
    std::array<VkDeviceSize, 2> m_CurrAllocatedSize = {};
    std::array<VkDeviceSize, 2> m_PeakAllocatedSize = {};
};

}
