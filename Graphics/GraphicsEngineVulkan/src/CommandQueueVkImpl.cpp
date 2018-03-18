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
#include "CommandQueueVkImpl.h"

namespace Diligent
{

CommandQueueVkImpl::CommandQueueVkImpl(IReferenceCounters *pRefCounters, VkQueue VkNativeCmdQueue, uint32_t QueueFamilyIndex) :
        TBase(pRefCounters),
        m_VkQueue(VkNativeCmdQueue),
        m_QueueFamilyIndex(QueueFamilyIndex)
        /*m_VkFence(pVkFence),
        m_NextFenceValue(1),
        m_WaitForGPUEventHandle( CreateEvent(nullptr, false, false, nullptr) )
        */
{
    //VERIFY_EXPR(m_WaitForGPUEventHandle != INVALID_HANDLE_VALUE);
    //m_VkFence->Signal(0);
}

CommandQueueVkImpl::~CommandQueueVkImpl()
{
    // Queues are created along with a logical device during vkCreateDevice.
    // All queues associated with a logical device are destroyed when vkDestroyDevice 
    // is called on that device.
}

IMPLEMENT_QUERY_INTERFACE( CommandQueueVkImpl, IID_CommandQueueVk, TBase )

#if 0
UINT64 CommandQueueVkImpl::ExecuteCommandList(IVkGraphicsCommandList* commandList)
{
    IVkCommandList *const ppCmdLists[] = {commandList};
	m_pVkCmdQueue->ExecuteCommandLists(1, ppCmdLists);
    auto FenceValue = m_NextFenceValue;
    // Signal the fence
    m_pVkCmdQueue->Signal(m_VkFence, FenceValue);
    // Increment the value
    Atomics::AtomicIncrement(m_NextFenceValue);
    return FenceValue;
}

void CommandQueueVkImpl::IdleGPU()
{
    Uint64 LastSignaledFenceValue = m_NextFenceValue;
    m_pVkCmdQueue->Signal(m_VkFence, LastSignaledFenceValue);
    Atomics::AtomicIncrement(m_NextFenceValue);
    if (GetCompletedFenceValue() < LastSignaledFenceValue)
    {
        m_VkFence->SetEventOnCompletion(LastSignaledFenceValue, m_WaitForGPUEventHandle);
        WaitForSingleObject(m_WaitForGPUEventHandle, INFINITE);
        VERIFY(GetCompletedFenceValue() == LastSignaledFenceValue, "Unexpected signaled fence value");
    }
}

Uint64 CommandQueueVkImpl::GetCompletedFenceValue()
{
    auto CompletedFenceValue = m_VkFence->GetCompletedValue();
    if(CompletedFenceValue > m_LastCompletedFenceValue)
        m_LastCompletedFenceValue = CompletedFenceValue;
    return m_LastCompletedFenceValue;
}
#endif

}
