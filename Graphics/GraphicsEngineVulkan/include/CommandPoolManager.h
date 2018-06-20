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
#include <mutex>
#include "STDAllocator.h"
#include "VulkanUtilities/VulkanDescriptorPool.h"

namespace Diligent
{

class CommandPoolManager
{
public:
    CommandPoolManager(const VulkanUtilities::VulkanLogicalDevice& LogicalDevice, 
                       uint32_t                                    queueFamilyIndex, 
                       VkCommandPoolCreateFlags                    flags)noexcept;
    
    CommandPoolManager             (const CommandPoolManager&) = delete;
    CommandPoolManager             (CommandPoolManager&&)      = delete;
    CommandPoolManager& operator = (const CommandPoolManager&) = delete;
    CommandPoolManager& operator = (CommandPoolManager&&)      = delete;

    ~CommandPoolManager();

    // Allocates Vulkan command pool.
    // The method first tries to find previously disposed command pool that can be safely reused
    // (i.e., whose FenceValue <= CompletedFenceValue). If no buffer can be reused, the method creates
    // a new one
    VulkanUtilities::CommandPoolWrapper AllocateCommandPool(uint64_t CompletedFenceValue, const char *DebugName = nullptr);
    
    // Disposes command pool. The buffer allocated from this pool MUST have already been submitted to the queue,
    // and FenceValue must be the value associated with this command buffer
    void DisposeCommandPool(VulkanUtilities::CommandPoolWrapper&& CmdPool, uint64_t FenceValue);

    void DestroyPools(uint64_t CompletedFenceValue);

private:
    const VulkanUtilities::VulkanLogicalDevice& m_LogicalDevice;
    const uint32_t                              m_QueueFamilyIndex;
    const VkCommandPoolCreateFlags              m_CmdPoolFlags;

    std::mutex m_Mutex;
    using CmdPoolQueueElemType = std::pair<uint64_t, VulkanUtilities::CommandPoolWrapper>;
    std::deque< CmdPoolQueueElemType, STDAllocatorRawMem<CmdPoolQueueElemType> > m_CmdPools;
};

}
