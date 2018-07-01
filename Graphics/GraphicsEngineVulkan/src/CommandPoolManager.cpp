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
#include "CommandPoolManager.h"

namespace Diligent
{

CommandPoolManager::CommandPoolManager(const VulkanUtilities::VulkanLogicalDevice& LogicalDevice, 
                                       uint32_t                                    queueFamilyIndex, 
                                       VkCommandPoolCreateFlags                    flags)noexcept:
    m_LogicalDevice   (LogicalDevice),
    m_QueueFamilyIndex(queueFamilyIndex),
    m_CmdPoolFlags    (flags),
    m_CmdPools(STD_ALLOCATOR_RAW_MEM(CmdPoolQueueElemType, GetRawAllocator(), "Allocator for deque< std::pair<uint64_t, CommandPoolWrapper > >"))
{
}

VulkanUtilities::CommandPoolWrapper CommandPoolManager::AllocateCommandPool(uint64_t CompletedFenceValue, const char* DebugName)
{
    std::lock_guard<std::mutex> LockGuard(m_Mutex);

    VulkanUtilities::CommandPoolWrapper CmdPool;
    if(!m_CmdPools.empty() && m_CmdPools.front().first <= CompletedFenceValue)
    {
        CmdPool = std::move(m_CmdPools.front().second);
        m_CmdPools.pop_front();
    }

    if(CmdPool == VK_NULL_HANDLE)
    {
        VkCommandPoolCreateInfo CmdPoolCI = {};
        CmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        CmdPoolCI.pNext = nullptr;
        CmdPoolCI.queueFamilyIndex = m_QueueFamilyIndex;
        CmdPoolCI.flags = m_CmdPoolFlags;
        CmdPool = m_LogicalDevice.CreateCommandPool(CmdPoolCI);
        VERIFY_EXPR(CmdPool != VK_NULL_HANDLE);
    }

    m_LogicalDevice.ResetCommandPool(CmdPool);

    return std::move(CmdPool);
}

void CommandPoolManager::DisposeCommandPool(VulkanUtilities::CommandPoolWrapper&& CmdPool, uint64_t FenceValue)
{
    std::lock_guard<std::mutex> LockGuard(m_Mutex);
    // Command pools must be disposed after the corresponding command list has been submitted to the queue.
    // At this point the fence value has been incremented, so the pool can be added to the queue.
    // There is no need to go through stale objects queue as FenceValue is guaranteed to be signaled
    // afer the command buffer submission
    m_CmdPools.emplace_back(FenceValue, std::move(CmdPool));
}

void CommandPoolManager::DestroyPools(uint64_t CompletedFenceValue)
{
    std::lock_guard<std::mutex> LockGuard(m_Mutex);
    while(!m_CmdPools.empty() && m_CmdPools.front().first <= CompletedFenceValue)
        m_CmdPools.pop_front();
}

CommandPoolManager::~CommandPoolManager()
{
    VERIFY(m_CmdPools.empty(), "Command pools have not been destroyed");
}

}
