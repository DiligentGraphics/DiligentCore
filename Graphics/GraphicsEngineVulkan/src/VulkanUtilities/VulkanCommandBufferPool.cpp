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

#include "VulkanUtilities/VulkanCommandBufferPool.h"
#include "VulkanUtilities/VulkanDebug.h"
#include "Errors.h"
#include "DebugUtilities.h"
#include "VulkanErrors.h"

namespace VulkanUtilities
{
    VulkanCommandBufferPool::VulkanCommandBufferPool(std::shared_ptr<const VulkanUtilities::VulkanLogicalDevice> LogicalDevice,
                                                     uint32_t queueFamilyIndex, 
                                                     VkCommandPoolCreateFlags flags) :
        m_LogicalDevice(LogicalDevice)
    {
        VkCommandPoolCreateInfo CmdPoolCI = {};
        CmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        CmdPoolCI.pNext = nullptr;
        CmdPoolCI.queueFamilyIndex = queueFamilyIndex;
        CmdPoolCI.flags = flags;
        m_CmdPool = m_LogicalDevice->CreateCommandPool(CmdPoolCI);
        VERIFY_EXPR(m_CmdPool != VK_NULL_HANDLE);
    }

    VulkanCommandBufferPool::~VulkanCommandBufferPool()
    {
        m_CmdPool.Release();
    }

    VkCommandBuffer VulkanCommandBufferPool::GetCommandBuffer(uint64_t LastCompletedFence, const char* DebugName)
    {
        VkCommandBuffer CmdBuffer = VK_NULL_HANDLE;

        if (!m_DiscardedCmdBuffers.empty())
        {
            // Pick the oldest cmd buffer at the front of the deque
            // If this buffer is not yet available, there is no point in
            // looking at other buffers since they were released even
            // later
            auto& OldestBuff = m_DiscardedCmdBuffers.front();

            // Note that LastCompletedFence only grows. So if after we queried
            // the value, the actual value is increased in other thread, this will not
            // be an issue as the only consequence is that potentially available  
            // cmd buffer may not be used.

            // OldestBuff.first is the fence value that was signaled AFTER the
            // command buffer has been submitted. If LastCompletedFence is at least
            // this value, the buffer can be safely reused
            if (LastCompletedFence >= OldestBuff.first)
            {
                CmdBuffer = OldestBuff.second;
                auto err = vkResetCommandBuffer(CmdBuffer, 
                    0 // VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT -  specifies that most or all memory resources currently owned by the command buffer should be returned to the parent command pool.
                );
                VERIFY(err == VK_SUCCESS, "Failed to reset command buffer");
                m_DiscardedCmdBuffers.pop_front();
            }
        }

        // If no allocators were ready to be reused, create a new one
        if (CmdBuffer == VK_NULL_HANDLE)
        {
            VkCommandBufferAllocateInfo BuffAllocInfo = {};
            BuffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            BuffAllocInfo.pNext = nullptr;
            BuffAllocInfo.commandPool = m_CmdPool;
            BuffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            BuffAllocInfo.commandBufferCount = 1;

            CmdBuffer = m_LogicalDevice->AllocateVkCommandBuffer(BuffAllocInfo);
        }

        VkCommandBufferBeginInfo CmdBuffBeginInfo = {};
        CmdBuffBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        CmdBuffBeginInfo.pNext = nullptr;
        CmdBuffBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Each recording of the command buffer will only be 
                                                                              // submitted once, and the command buffer will be reset 
                                                                              // and recorded again between each submission.
        CmdBuffBeginInfo.pInheritanceInfo = nullptr; // Ignored for a primary command buffer
        auto err = vkBeginCommandBuffer(CmdBuffer, &CmdBuffBeginInfo);
        VERIFY(err == VK_SUCCESS, "Failed to begin command buffer");

        return CmdBuffer;
    }

    void VulkanCommandBufferPool::DisposeCommandBuffer(VkCommandBuffer CmdBuffer, uint64_t FenceValue)
    {
        // FenceValue is the value that was signaled by the command queue after it 
        // executed the command buffer
        m_DiscardedCmdBuffers.emplace_back(FenceValue, CmdBuffer);
    }

    CommandPoolWrapper&& VulkanCommandBufferPool::Release()
    {
        m_LogicalDevice.reset();
        m_DiscardedCmdBuffers.clear();
        return std::move(m_CmdPool);
    }
}
