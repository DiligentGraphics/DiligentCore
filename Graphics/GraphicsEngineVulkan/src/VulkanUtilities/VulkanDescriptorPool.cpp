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
#include <sstream>

#include "VulkanUtilities/VulkanDescriptorPool.h"
#include "VulkanUtilities/VulkanDebug.h"
#include "Errors.h"
#include "DebugUtilities.h"
#include "VulkanErrors.h"

namespace VulkanUtilities
{
    VulkanDescriptorPool::VulkanDescriptorPool(std::shared_ptr<const VulkanUtilities::VulkanLogicalDevice> LogicalDevice,
                                               const VkDescriptorPoolCreateInfo &DescriptorPoolCI)noexcept :
        m_LogicalDevice(LogicalDevice)
    {
        VERIFY_EXPR(DescriptorPoolCI.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        m_Pool = m_LogicalDevice->CreateDescriptorPool(DescriptorPoolCI);
        VERIFY_EXPR(m_Pool != VK_NULL_HANDLE);
    }

    VulkanDescriptorPool::~VulkanDescriptorPool()
    {
        m_Pool.Release();
    }

    VkDescriptorSet VulkanDescriptorPool::AllocateDescriptorSet(VkDescriptorSetLayout SetLayout, const char* DebugName)
    {
        VkDescriptorSetAllocateInfo DescrSetAllocInfo = {};
        DescrSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        DescrSetAllocInfo.pNext = nullptr;
        DescrSetAllocInfo.descriptorPool = m_Pool;
        DescrSetAllocInfo.descriptorSetCount = 1;
        DescrSetAllocInfo.pSetLayouts = &SetLayout;
        // Descriptor pools are externally synchronized, meaning that the application must not allocate 
        // and/or free descriptor sets from the same pool in multiple threads simultaneously (13.2.3)
        return m_LogicalDevice->AllocateVkDescriptorSet(DescrSetAllocInfo, DebugName);
    }

    void VulkanDescriptorPool::ReleaseDiscardedSets(uint64_t LastCompletedFence)
    {
        // Pick the oldest descriptor set at the front of the deque.
        // .first is the fence value that was signaled AFTER the command buffer referencing
        // the set has been submitted. If LastCompletedFence is at least this value, the buffer 
        // is now finished, and the set can be safely released
        while (!m_DiscardedSets.empty() && LastCompletedFence >= m_DiscardedSets.front().first )
        {
            m_LogicalDevice->FreeDescriptorSet(m_Pool, m_DiscardedSets.front().second);
            m_DiscardedSets.pop_front();
        }
    }

    void VulkanDescriptorPool::DisposeDescriptorSet(VkDescriptorSet DescrSet, uint64_t FenceValue)
    {
        // FenceValue is the value that was signaled by the command queue after it 
        // executed the command buffer
        m_DiscardedSets.emplace_back(FenceValue, DescrSet);
    }

    DescriptorPoolWrapper&& VulkanDescriptorPool::Release()
    {
        m_LogicalDevice.reset();
        VERIFY(m_DiscardedSets.empty(), "Discarded sets are not released");
        m_DiscardedSets.clear();
        return std::move(m_Pool);
    }
}
