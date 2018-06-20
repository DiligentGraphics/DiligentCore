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

#include <thread>
#include "pch.h"
#include "CommandQueueVkImpl.h"

namespace Diligent
{

CommandQueueVkImpl::CommandQueueVkImpl(IReferenceCounters*                                   pRefCounters, 
                                       std::shared_ptr<VulkanUtilities::VulkanLogicalDevice> LogicalDevice,
                                       uint32_t                                              QueueFamilyIndex) :
    TBase(pRefCounters),
    m_LogicalDevice(LogicalDevice),
    m_VkQueue(LogicalDevice->GetQueue(QueueFamilyIndex, 0)),
    m_QueueFamilyIndex(QueueFamilyIndex),
    m_NextFenceValue(1),
    m_FencePool(LogicalDevice)
{
}

CommandQueueVkImpl::~CommandQueueVkImpl()
{
    // Queues are created along with a logical device during vkCreateDevice.
    // All queues associated with a logical device are destroyed when vkDestroyDevice 
    // is called on that device.

    while(!m_PendingFences.empty())
    {
        m_FencePool.DisposeFence(std::move(m_PendingFences.front().second));
        m_PendingFences.pop_front();
    }
}

IMPLEMENT_QUERY_INTERFACE( CommandQueueVkImpl, IID_CommandQueueVk, TBase )


Uint64 CommandQueueVkImpl::ExecuteCommandBuffer(const VkSubmitInfo& SubmitInfo)
{
    std::lock_guard<std::mutex> Lock(m_QueueMutex);
    auto Fence = m_FencePool.GetFence();
    bool SubmitCount = 
        (SubmitInfo.waitSemaphoreCount   != 0 || 
         SubmitInfo.commandBufferCount   != 0 || 
         SubmitInfo.signalSemaphoreCount != 0) ? 
        1 : 0;
    auto err = vkQueueSubmit(m_VkQueue, SubmitCount, &SubmitInfo, Fence);
    VERIFY(err == VK_SUCCESS, "Failed to submit command buffer to the command queue");

    // We must atomically place the (value, fence) pair into the deque
    auto FenceValue = m_NextFenceValue;
    m_PendingFences.emplace_back(FenceValue, std::move(Fence));

    // Increment the value
    Atomics::AtomicIncrement(m_NextFenceValue);
    return FenceValue;
}

Uint64 CommandQueueVkImpl::ExecuteCommandBuffer(VkCommandBuffer cmdBuffer)
{
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = cmdBuffer != VK_NULL_HANDLE ? 1 : 0;
    SubmitInfo.pCommandBuffers = &cmdBuffer;
    SubmitInfo.waitSemaphoreCount = 0; // the number of semaphores upon which to wait before executing the command buffers
    SubmitInfo.pWaitSemaphores = nullptr; // a pointer to an array of semaphores upon which to wait before the command 
                                          // buffers begin execution
    SubmitInfo.pWaitDstStageMask = nullptr; // a pointer to an array of pipeline stages at which each corresponding 
                                            // semaphore wait will occur
    SubmitInfo.signalSemaphoreCount = 0; // the number of semaphores to be signaled once the commands specified in 
                                         // pCommandBuffers have completed execution
    SubmitInfo.pSignalSemaphores = nullptr; // a pointer to an array of semaphores which will be signaled when the 
                                            // command buffers for this batch have completed execution

    return ExecuteCommandBuffer(SubmitInfo);
}

void CommandQueueVkImpl::IdleGPU()
{
    std::lock_guard<std::mutex> Lock(m_QueueMutex);

    // Update last completed fence value to unlock all waiting events
    Uint64 LastCompletedFenceValue = m_NextFenceValue;
    // Increment fence before idling the queue
    Atomics::AtomicIncrement(m_NextFenceValue);
    vkQueueWaitIdle(m_VkQueue);
    if(LastCompletedFenceValue > m_LastCompletedFenceValue)
        m_LastCompletedFenceValue = LastCompletedFenceValue;
    for(auto& val_fence : m_PendingFences)
    {
        // For some reason after idling the queue not all fences are signaled
        while(m_LogicalDevice->GetFenceStatus(val_fence.second) != VK_SUCCESS)
        {
            VkFence FenceToWait = val_fence.second;
            auto res = vkWaitForFences(m_LogicalDevice->GetVkDevice(), 1, &FenceToWait, VK_TRUE, UINT64_MAX);
            VERIFY_EXPR(res == VK_SUCCESS);
        }

        auto status = m_LogicalDevice->GetFenceStatus(val_fence.second);
        VERIFY(status == VK_SUCCESS, "All pending fences must now be complete!");
        m_FencePool.DisposeFence(std::move(val_fence.second));
    }
    m_PendingFences.clear();
}

Uint64 CommandQueueVkImpl::GetCompletedFenceValue()
{
    std::lock_guard<std::mutex> Lock(m_QueueMutex);

    while(!m_PendingFences.empty())
    {
        auto &Value_Fence = m_PendingFences.front();
        auto status = m_LogicalDevice->GetFenceStatus(Value_Fence.second);
        if(status == VK_SUCCESS)
        {
            if(Value_Fence.first > m_LastCompletedFenceValue)
                m_LastCompletedFenceValue = Value_Fence.first;
            m_FencePool.DisposeFence(std::move(Value_Fence.second));
            m_PendingFences.pop_front();
        }
        else
        {
            break;
        }
    }

    return m_LastCompletedFenceValue;
}

}
