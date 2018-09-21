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
#include "DescriptorPoolManager.h"
#include "RenderDeviceVkImpl.h"

namespace Diligent
{

void DescriptorPoolAllocation::Release()
{
    if (Set != VK_NULL_HANDLE)
    {
        VERIFY_EXPR(ParentPoolMgr != nullptr && ParentPool != nullptr);
        ParentPoolMgr->FreeAllocation(Set, *ParentPool);

        Set           = VK_NULL_HANDLE;
        ParentPoolMgr = nullptr;
        ParentPool    = nullptr;
    }
}

void DescriptorPoolManager::CreateNewPool()
{
    VkDescriptorPoolCreateInfo PoolCI = {};
    PoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    PoolCI.pNext = nullptr;
    // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT specifies that descriptor sets can 
    // return their individual allocations to the pool, i.e. all of vkAllocateDescriptorSets, 
    // vkFreeDescriptorSets, and vkResetDescriptorPool are allowed. (13.2.3)
    PoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    PoolCI.maxSets = m_MaxSets;
    PoolCI.poolSizeCount = static_cast<uint32_t>(m_PoolSizes.size());
    PoolCI.pPoolSizes = m_PoolSizes.data();
    m_DescriptorPools.emplace_front( new VulkanUtilities::VulkanDescriptorPool(m_LogicalDevice, PoolCI) );
}

DescriptorPoolAllocation DescriptorPoolManager::Allocate(Uint64 CommandQueueMask, VkDescriptorSetLayout SetLayout)
{
    // Descriptor pools are externally synchronized, meaning that the application must not allocate 
    // and/or free descriptor sets from the same pool in multiple threads simultaneously (13.2.3)
    std::lock_guard<std::mutex> Lock(m_Mutex);

    // Try all pools starting from the frontmost
    for(auto it = m_DescriptorPools.begin(); it != m_DescriptorPools.end(); ++it)
    {
        auto& Pool = *(*it);
        auto Set = Pool.AllocateDescriptorSet(SetLayout);
        if(Set != VK_NULL_HANDLE)
        {
            // Move the pool to the front
            if(it != m_DescriptorPools.begin())
            {
                std::swap(*it, m_DescriptorPools.front());
            }
            return {Set, CommandQueueMask, Pool, *this};
        }
    }

    // Failed to allocate descriptor from existing pools -> create a new one
    CreateNewPool();
    LOG_INFO_MESSAGE("Allocated new descriptor pool");

    auto &NewPool = *m_DescriptorPools.front();
    auto Set = NewPool.AllocateDescriptorSet(SetLayout);
    VERIFY(Set != VK_NULL_HANDLE, "Failed to allocate descriptor set");

    return {Set, CommandQueueMask, NewPool, *this };
}

void DescriptorPoolManager::FreeAllocation(VkDescriptorSet Set, VulkanUtilities::VulkanDescriptorPool& Pool)
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    m_ReleasedAllocations.emplace_back(std::make_pair(Set, &Pool));
}

void DescriptorPoolManager::DisposeAllocations(uint64_t FenceValue)
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    for(auto &Allocation : m_ReleasedAllocations)
    {
        Allocation.second->DisposeDescriptorSet(Allocation.first, FenceValue);
    }
    m_ReleasedAllocations.clear();
}

void DescriptorPoolManager::ReleaseStaleAllocations(uint64_t LastCompletedFence)
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    for(auto &Pool : m_DescriptorPools)
        Pool->ReleaseDiscardedSets(LastCompletedFence);
}

size_t DescriptorPoolManager::GetPendingReleaseAllocationCount()
{
    size_t count = 0;
    std::lock_guard<std::mutex> Lock(m_Mutex);
    for(auto &Pool : m_DescriptorPools)
        count += Pool->GetDiscardedSetCount();
    return count;
}


}
