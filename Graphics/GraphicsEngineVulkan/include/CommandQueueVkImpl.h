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

/// \file
/// Declaration of Diligent::CommandQueueVkImpl class

#include <mutex>
#include <deque>
#include "vulkan.h"
#include "CommandQueueVk.h"
#include "ObjectBase.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "FenceVkImpl.h"

namespace Diligent
{

/// Implementation of the Diligent::ICommandQueueVk interface
class CommandQueueVkImpl : public ObjectBase<ICommandQueueVk>
{
public:
    using TBase = ObjectBase<ICommandQueueVk>;

    CommandQueueVkImpl(IReferenceCounters*                                   pRefCounters, 
                       std::shared_ptr<VulkanUtilities::VulkanLogicalDevice> LogicalDevice, 
                       uint32_t                                              QueueFamilyIndex);
    ~CommandQueueVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID& IID, IObject** ppInterface )override;

	// Returns the fence value that will be signaled next time
    virtual UINT64 GetNextFenceValue()override final { return m_NextFenceValue; }

	// Executes a given command buffer
	virtual Uint64 ExecuteCommandBuffer(VkCommandBuffer cmdBuffer)override final;

    virtual Uint64 ExecuteCommandBuffer(const VkSubmitInfo& SubmitInfo)override final;

    virtual VkQueue GetVkQueue()override final{return m_VkQueue;}

    virtual uint32_t GetQueueFamilyIndex()override final { return m_QueueFamilyIndex; }
    
    virtual void IdleGPU()override final;

    virtual Uint64 GetCompletedFenceValue()override final;

    virtual void SignalFence(VkFence vkFence)override final;

    void SetFence(RefCntAutoPtr<FenceVkImpl> pFence){m_pFence = std::move(pFence);}

private:
    // A value that will be signaled by the command queue next
    Atomics::AtomicInt64 m_NextFenceValue;

    std::shared_ptr<VulkanUtilities::VulkanLogicalDevice> m_LogicalDevice;
   
    const VkQueue m_VkQueue;
    const uint32_t m_QueueFamilyIndex;
    // Fence is signaled right after a command buffer has been 
    // submitted to the command queue for execution.
    // All command buffers with fence value less than or equal to the signaled value
    // are guaranteed to be finished by the GPU
    RefCntAutoPtr<FenceVkImpl> m_pFence;

    std::mutex m_QueueMutex;
};

}
