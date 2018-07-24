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

#include <deque>
#include <memory>
#include "vulkan.h"
#include "VulkanLogicalDevice.h"
#include "VulkanObjectWrappers.h"

namespace VulkanUtilities
{
    class VulkanDescriptorPool
    {
    public:
        VulkanDescriptorPool(std::shared_ptr<const VulkanUtilities::VulkanLogicalDevice> LogicalDevice,
                             const VkDescriptorPoolCreateInfo&                           DescriptorPoolCI);

        VulkanDescriptorPool             (const VulkanDescriptorPool&) = delete;
        VulkanDescriptorPool& operator = (const VulkanDescriptorPool&) = delete;
        VulkanDescriptorPool             (VulkanDescriptorPool&&)      = default;
        VulkanDescriptorPool& operator = (VulkanDescriptorPool&&)      = default;
        ~VulkanDescriptorPool();

        VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout SetLayout, const char* DebugName = "");
        void DisposeDescriptorSet(VkDescriptorSet DescrSet, uint64_t FenceValue);
        void ReleaseDiscardedSets(uint64_t LastCompletedFence);

        DescriptorPoolWrapper&& Release();
        
        size_t GetDiscardedSetCount()const
        {
            return m_DiscardedSets.size();
        }

    private:
        // Shared point to logical device must be defined before the command pool
        std::shared_ptr<const VulkanUtilities::VulkanLogicalDevice> m_LogicalDevice;
        DescriptorPoolWrapper m_Pool;

        // fist    - the fence value associated with the command buffer referencing the set
        //           when it was executed
        // second  - the descriptor set
        typedef std::pair<uint64_t, VkDescriptorSet > QueueElemType;
        std::deque< QueueElemType > m_DiscardedSets;
    };
}
