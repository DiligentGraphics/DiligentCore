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

void DescriptorSetAllocation::Release()
{
    if (Set != VK_NULL_HANDLE)
    {
        VERIFY_EXPR(DescrSetAllocator != nullptr && Pool != nullptr);
        DescrSetAllocator->FreeDescriptorSet(Set, Pool, CmdQueueMask);

        Reset();
    }
}

VulkanUtilities::DescriptorPoolWrapper DescriptorPoolManager::CreateDescriptorPool(const char* DebugName)
{
    VkDescriptorPoolCreateInfo PoolCI = {};
    PoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    PoolCI.pNext = nullptr;
    // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT specifies that descriptor sets can 
    // return their individual allocations to the pool, i.e. all of vkAllocateDescriptorSets, 
    // vkFreeDescriptorSets, and vkResetDescriptorPool are allowed. (13.2.3)
    PoolCI.flags = m_AllowFreeing ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0;
    PoolCI.maxSets = m_MaxSets;
    PoolCI.poolSizeCount = static_cast<uint32_t>(m_PoolSizes.size());
    PoolCI.pPoolSizes = m_PoolSizes.data();
    return m_DeviceVkImpl.GetLogicalDevice().CreateDescriptorPool(PoolCI, DebugName);
}

DescriptorPoolManager::~DescriptorPoolManager()
{
    DEV_CHECK_ERR(m_AllocatedPoolCounter == 0, "Not all allocated descriptor pools are returned to the pool manager");
    LOG_INFO_MESSAGE(m_PoolName, " stats: allocated ", m_Pools.size(), " pool(s)");
}

VulkanUtilities::DescriptorPoolWrapper DescriptorPoolManager::GetPool(const char* DebugName)
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
#ifdef DEVELOPMENT
    ++m_AllocatedPoolCounter;
#endif
    if (m_Pools.empty())
        return CreateDescriptorPool(DebugName);
    else
    {
        auto& LogicalDevice = m_DeviceVkImpl.GetLogicalDevice();
        auto Pool = std::move(m_Pools.front());
        VulkanUtilities::SetDescriptorPoolName(LogicalDevice.GetVkDevice(), Pool, DebugName);
        m_Pools.pop_front();
        return Pool;
    }
}

void DescriptorPoolManager::FreePool(VulkanUtilities::DescriptorPoolWrapper&& Pool)
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    m_DeviceVkImpl.GetLogicalDevice().ResetDescriptorPool(Pool);
    m_Pools.emplace_back(std::move(Pool));
#ifdef DEVELOPMENT
    --m_AllocatedPoolCounter;
#endif
}


static VkDescriptorSet AllocateDescriptorSet(const VulkanUtilities::VulkanLogicalDevice& LogicalDevice,
                                             VkDescriptorPool                            Pool,
                                             VkDescriptorSetLayout                       SetLayout,
                                             const char*                                 DebugName)
{
    VkDescriptorSetAllocateInfo DescrSetAllocInfo = {};
    DescrSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescrSetAllocInfo.pNext = nullptr;
    DescrSetAllocInfo.descriptorPool = Pool;
    DescrSetAllocInfo.descriptorSetCount = 1;
    DescrSetAllocInfo.pSetLayouts = &SetLayout;
    // Descriptor pools are externally synchronized, meaning that the application must not allocate 
    // and/or free descriptor sets from the same pool in multiple threads simultaneously (13.2.3)
    return LogicalDevice.AllocateVkDescriptorSet(DescrSetAllocInfo, DebugName);
}


DescriptorSetAllocation DescriptorSetAllocator::Allocate(Uint64 CommandQueueMask, VkDescriptorSetLayout SetLayout)
{
    // Descriptor pools are externally synchronized, meaning that the application must not allocate 
    // and/or free descriptor sets from the same pool in multiple threads simultaneously (13.2.3)
    std::lock_guard<std::mutex> Lock(m_Mutex);

    const auto& LogicalDevice = m_DeviceVkImpl.GetLogicalDevice();
    // Try all pools starting from the frontmost
    for(auto it = m_Pools.begin(); it != m_Pools.end(); ++it)
    {
        auto& Pool = *it;
        auto Set = AllocateDescriptorSet(LogicalDevice, Pool, SetLayout, "Descriptor set");
        if (Set != VK_NULL_HANDLE)
        {
            // Move the pool to the front
            if (it != m_Pools.begin())
            {
                std::swap(*it, m_Pools.front());
            }
            return {Set, Pool, CommandQueueMask, *this};
        }
    }

    // Failed to allocate descriptor from existing pools -> create a new one
    LOG_INFO_MESSAGE("Allocated new descriptor pool");
    m_Pools.emplace_front(CreateDescriptorPool("Descriptor pool"));

    auto& NewPool = m_Pools.front();
    auto Set = AllocateDescriptorSet(LogicalDevice, NewPool, SetLayout, "");
    VERIFY(Set != VK_NULL_HANDLE, "Failed to allocate descriptor set");

    return {Set, NewPool, CommandQueueMask, *this };
}

void DescriptorSetAllocator::FreeDescriptorSet(VkDescriptorSet Set, VkDescriptorPool Pool, Uint64 QueueMask)
{
    class DescriptorSetDeleter
    {
    public:
        DescriptorSetDeleter(DescriptorSetAllocator& _Allocator,
                             VkDescriptorSet         _Set,
                             VkDescriptorPool        _Pool) : 
            Allocator (&_Allocator),
            Set       (_Set),
            Pool      (_Pool)
        {}

        DescriptorSetDeleter             (const DescriptorSetDeleter&) = delete;
        DescriptorSetDeleter& operator = (const DescriptorSetDeleter&) = delete;
        DescriptorSetDeleter& operator = (      DescriptorSetDeleter&&)= delete;

        DescriptorSetDeleter(DescriptorSetDeleter&& rhs)noexcept : 
            Allocator (rhs.Allocator),
            Set       (rhs.Set),
            Pool      (rhs.Pool)
        {
            rhs.Allocator = nullptr;
            rhs.Set       = nullptr;
            rhs.Pool      = nullptr;
        }

        ~DescriptorSetDeleter()
        {
            if (Allocator!=nullptr)
            {
                std::lock_guard<std::mutex> Lock(Allocator->m_Mutex);
                Allocator->m_DeviceVkImpl.GetLogicalDevice().FreeDescriptorSet(Pool, Set);
            }
        }

    private:
        DescriptorSetAllocator* Allocator;
        VkDescriptorSet         Set;
        VkDescriptorPool        Pool;
    };
    m_DeviceVkImpl.SafeReleaseDeviceObject(DescriptorSetDeleter{*this, Set, Pool}, QueueMask);
}


VkDescriptorSet DynamicDescriptorSetAllocator::Allocate(VkDescriptorSetLayout SetLayout, const char* DebugName)
{
    VkDescriptorSet set = VK_NULL_HANDLE;
    const auto& LogicalDevice = m_PoolMgr.m_DeviceVkImpl.GetLogicalDevice();
    if (!m_AllocatedPools.empty())
    {
        set = AllocateDescriptorSet(LogicalDevice, m_AllocatedPools.back(), SetLayout, DebugName);
    }

    if (set == VK_NULL_HANDLE)
    {
        m_AllocatedPools.emplace_back(m_PoolMgr.GetPool("Dynamic Descriptor Pool"));
        set = AllocateDescriptorSet(LogicalDevice, m_AllocatedPools.back(), SetLayout, DebugName);
    }
    
    return set;
}

void DynamicDescriptorSetAllocator::ReleasePools(Uint64 QueueMask)
{
    class DescriptorPoolDeleter
    {
    public:
        DescriptorPoolDeleter(DescriptorPoolManager&                   _PoolMgr,
                              VulkanUtilities::DescriptorPoolWrapper&& _Pool) noexcept : 
            PoolMgr (&_PoolMgr),
            Pool    (std::move(_Pool))
        {}

        DescriptorPoolDeleter             (const DescriptorPoolDeleter&) = delete;
        DescriptorPoolDeleter& operator = (const DescriptorPoolDeleter&) = delete;
        DescriptorPoolDeleter& operator = (      DescriptorPoolDeleter&&)= delete;

        DescriptorPoolDeleter(DescriptorPoolDeleter&& rhs)noexcept : 
            PoolMgr (rhs.PoolMgr),
            Pool    (std::move(rhs.Pool))
        {
            rhs.PoolMgr = nullptr;
        }

        ~DescriptorPoolDeleter()
        {
            if (PoolMgr!=nullptr)
            {
                PoolMgr->FreePool(std::move(Pool));
            }
        }

    private:
        DescriptorPoolManager*                 PoolMgr;
        VulkanUtilities::DescriptorPoolWrapper Pool;
    };

    for(auto& Pool : m_AllocatedPools)
    {
        m_PoolMgr.m_DeviceVkImpl.SafeReleaseDeviceObject(DescriptorPoolDeleter{m_PoolMgr, std::move(Pool)}, QueueMask);
    }
    m_PeakPoolCount = std::max(m_PeakPoolCount, m_AllocatedPools.size());
    m_AllocatedPools.clear();
}

DynamicDescriptorSetAllocator::~DynamicDescriptorSetAllocator()
{
    DEV_CHECK_ERR(m_AllocatedPools.empty(), "All allocated pools must be returned to the parent descriptor pool manager");
    LOG_INFO_MESSAGE(m_Name, " peak descriptor pool count: ", m_PeakPoolCount);
}

}
