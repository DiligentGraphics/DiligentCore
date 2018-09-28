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
#include "RenderDeviceVkImpl.h"

namespace Diligent
{

CommandPoolManager::CommandPoolManager(RenderDeviceVkImpl&      DeviceVkImpl, 
                                       std::string              Name,
                                       uint32_t                 queueFamilyIndex, 
                                       VkCommandPoolCreateFlags flags)noexcept:
    m_DeviceVkImpl    (DeviceVkImpl),
    m_Name            (std::move(Name)),
    m_QueueFamilyIndex(queueFamilyIndex),
    m_CmdPoolFlags    (flags),
    m_CmdPools        (STD_ALLOCATOR_RAW_MEM(VulkanUtilities::CommandPoolWrapper, GetRawAllocator(), "Allocator for deque<VulkanUtilities::CommandPoolWrapper>"))
{
}

VulkanUtilities::CommandPoolWrapper CommandPoolManager::AllocateCommandPool(const char* DebugName)
{
    std::lock_guard<std::mutex> LockGuard(m_Mutex);

    VulkanUtilities::CommandPoolWrapper CmdPool;
    if(!m_CmdPools.empty())
    {
        CmdPool = std::move(m_CmdPools.front());
        m_CmdPools.pop_front();
    }

    auto& LogicalDevice = m_DeviceVkImpl.GetLogicalDevice();
    if(CmdPool == VK_NULL_HANDLE)
    {
        VkCommandPoolCreateInfo CmdPoolCI = {};
        CmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        CmdPoolCI.pNext = nullptr;
        CmdPoolCI.queueFamilyIndex = m_QueueFamilyIndex;
        CmdPoolCI.flags = m_CmdPoolFlags;
        CmdPool = LogicalDevice.CreateCommandPool(CmdPoolCI);
        VERIFY_EXPR(CmdPool != VK_NULL_HANDLE);
    }

    LogicalDevice.ResetCommandPool(CmdPool);

#ifdef DEVELOPMENT
    ++m_AllocatedPoolCounter;
#endif
    return std::move(CmdPool);
}

void CommandPoolManager::FreeCommandPool(VulkanUtilities::CommandPoolWrapper&& CmdPool)
{
    std::lock_guard<std::mutex> LockGuard(m_Mutex);
#ifdef DEVELOPMENT
    --m_AllocatedPoolCounter;
#endif
    m_CmdPools.emplace_back(std::move(CmdPool));
}

void CommandPoolManager::DestroyPools()
{
    std::lock_guard<std::mutex> LockGuard(m_Mutex);
    DEV_CHECK_ERR(m_AllocatedPoolCounter == 0, m_AllocatedPoolCounter, " command pools have not been returned to the manager.");
    LOG_INFO_MESSAGE(m_Name, " allocated descriptor pool count: ", m_CmdPools.size() );
    m_CmdPools.clear();
}

CommandPoolManager::~CommandPoolManager()
{
    VERIFY(m_CmdPools.empty(), "Command pools have not been destroyed");
}

}
