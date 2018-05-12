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

// Descriptor heap management utilities. 
// See http://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-descriptor-heaps/ for details

#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include "VulkanUtilities/VulkanDescriptorPool.h"

namespace Diligent
{

class DescriptorPoolAllocation
{
public:
    DescriptorPoolAllocation(VkDescriptorSet _Set,
                             VulkanUtilities::VulkanDescriptorPool& _ParentPool) :
        Set(_Set),
        ParentPool(_ParentPool)
    {}
    DescriptorPoolAllocation(const DescriptorPoolAllocation&) = delete;
    DescriptorPoolAllocation& operator = (const DescriptorPoolAllocation&) = delete;

    DescriptorPoolAllocation(DescriptorPoolAllocation&& rhs) : 
        Set(rhs.Set),
        ParentPool(rhs.ParentPool)
    {
        rhs.Set = VK_NULL_HANDLE;
    }
    DescriptorPoolAllocation& operator = (DescriptorPoolAllocation&&) = delete;

    ~DescriptorPoolAllocation()
    {
        VERIFY(Set == VK_NULL_HANDLE, "Allocation must be explicitly disposed through DescriptorPoolManager::DisposeAllocation");
    }
    
    VkDescriptorSet GetVkDescriptorSet()const {return Set;}

private:
    friend class DescriptorPoolManager;
    VkDescriptorSet Set;
    VulkanUtilities::VulkanDescriptorPool& ParentPool;
};

class DescriptorPoolManager
{
public:
    DescriptorPoolManager(std::shared_ptr<const VulkanUtilities::VulkanLogicalDevice> LogicalDevice,
                          std::vector<VkDescriptorPoolSize> PoolSizes,
                          uint32_t MaxSets,
                          bool IsThreadSafe)noexcept:
        m_LogicalDevice(std::move(LogicalDevice)),
        m_PoolSizes(std::move(PoolSizes)),
        m_MaxSets(MaxSets),
        m_IsThreadSafe(IsThreadSafe)
    {
        CreateNewPool();
    }

    // Move constructor must be noexcept, otherwise vector<DescriptorPoolManager> will fail to compile on MSVC
    // So we have to implement it manually. = default also does not work
    DescriptorPoolManager(DescriptorPoolManager&& rhs)noexcept : 
        m_PoolSizes(std::move(rhs.m_PoolSizes)),
        m_MaxSets(std::move(rhs.m_MaxSets)),
        m_IsThreadSafe(std::move(rhs.m_IsThreadSafe)),
        //m_Mutex(std::move(rhs.m_Mutex)), mutex is not movable
        m_LogicalDevice(std::move(rhs.m_LogicalDevice)),
        m_DescriptorPools(std::move(rhs.m_DescriptorPools)),
        m_ReleasedAllocations(std::move(rhs.m_ReleasedAllocations))
    {
    }

    DescriptorPoolManager(const DescriptorPoolManager&)              = delete;
    DescriptorPoolManager& operator = (const DescriptorPoolManager&) = delete;
    DescriptorPoolManager& operator = (DescriptorPoolManager&&)      = delete;

    DescriptorPoolAllocation Allocate(VkDescriptorSetLayout SetLayout);
    void FreeAllocation(DescriptorPoolAllocation&& Allocation);
    void DisposeAllocations(uint64_t FenceValue);
    void ReleaseStaleAllocations(uint64_t LastCompletedFence);

private:
    void CreateNewPool();

    const std::vector<VkDescriptorPoolSize> m_PoolSizes;
    const uint32_t m_MaxSets;
    const bool m_IsThreadSafe;

    std::mutex m_Mutex;
    std::shared_ptr<const VulkanUtilities::VulkanLogicalDevice> m_LogicalDevice;
    std::deque<VulkanUtilities::VulkanDescriptorPool> m_DescriptorPools;
    std::vector<DescriptorPoolAllocation> m_ReleasedAllocations;
};

}
