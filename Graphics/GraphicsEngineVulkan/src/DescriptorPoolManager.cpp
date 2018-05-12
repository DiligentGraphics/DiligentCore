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
    m_DescriptorPools.emplace_front(m_LogicalDevice, PoolCI);
}

DescriptorPoolAllocation DescriptorPoolManager::Allocate(VkDescriptorSetLayout SetLayout)
{
    std::unique_lock<std::mutex> Lock(m_Mutex, std::defer_lock);
    if (m_IsThreadSafe)
        Lock.lock();

    // Try all pools starting from the frontmost
    for(auto it = m_DescriptorPools.begin(); it != m_DescriptorPools.end(); ++it)
    {
        auto Set = it->AllocateDescriptorSet(SetLayout);
        if(Set != VK_NULL_HANDLE)
        {
            // Move the pool to the front
            if(it != m_DescriptorPools.begin())
            {
                std::swap(*it, m_DescriptorPools.front());
            }
            return {Set, *it};
        }
    }

    // Failed to allocate descriptor from existing pools -> create a new one
    CreateNewPool();
    LOG_INFO_MESSAGE("Allocated new descriptor pool");

    auto Set = m_DescriptorPools.front().AllocateDescriptorSet(SetLayout);
    VERIFY(Set != VK_NULL_HANDLE, "Failed to allocate descriptor set");

    return {Set, m_DescriptorPools.front() };
}

void DescriptorPoolManager::FreeAllocation(DescriptorPoolAllocation&& Allocation)
{
    std::unique_lock<std::mutex> Lock(m_Mutex, std::defer_lock);
    if (m_IsThreadSafe)
        Lock.lock();

    m_ReleasedAllocations.emplace_back(std::move(Allocation));
}

void DescriptorPoolManager::DisposeAllocations(uint64_t FenceValue)
{
    std::unique_lock<std::mutex> Lock(m_Mutex, std::defer_lock);
    if (m_IsThreadSafe)
        Lock.lock();

    for(auto &Allocation : m_ReleasedAllocations)
    {
        Allocation.ParentPool.DisposeDescriptorSet(Allocation.Set, FenceValue);
        Allocation.Set = VK_NULL_HANDLE;
    }
    m_ReleasedAllocations.clear();
}

void DescriptorPoolManager::ReleaseStaleAllocations(uint64_t LastCompletedFence)
{
    std::unique_lock<std::mutex> Lock(m_Mutex, std::defer_lock);
    if (m_IsThreadSafe)
        Lock.lock();

    for(auto &Pool : m_DescriptorPools)
        Pool.ReleaseDiscardedSets(LastCompletedFence);
}

}
