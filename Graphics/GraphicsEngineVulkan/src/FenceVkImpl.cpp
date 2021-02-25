/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including neVkigence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly neVkigent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "pch.h"

#include "FenceVkImpl.hpp"
#include "EngineMemory.h"
#include "RenderDeviceVkImpl.hpp"

namespace Diligent
{

FenceVkImpl::FenceVkImpl(IReferenceCounters* pRefCounters,
                         RenderDeviceVkImpl* pRendeDeviceVkImpl,
                         const FenceDesc&    Desc,
                         bool                IsDeviceInternal) :   
    TFenceBase { pRefCounters,pRendeDeviceVkImpl, Desc,  IsDeviceInternal },
    m_Semaphore { VK_NULL_HANDLE }
{
    VkSemaphoreTypeCreateInfo SemaphoreTypeCI = {};
    SemaphoreTypeCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    SemaphoreTypeCI.pNext = nullptr;
    SemaphoreTypeCI.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    SemaphoreTypeCI.initialValue = 0;

    VkSemaphoreCreateInfo SemaphoreCI = {};
    SemaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    SemaphoreCI.pNext = &SemaphoreTypeCI;

    m_Semaphore = m_pDevice->GetLogicalDevice().CreateSemaphore(SemaphoreCI, "");
}

FenceVkImpl::~FenceVkImpl()
{
}

Uint64 FenceVkImpl::GetCompletedValue()
{
    const auto& LogicalDevice = m_pDevice->GetLogicalDevice();
    Uint64 SemaphoreCounter;
    auto VkResult = LogicalDevice.GetSemaphoreCounter(m_Semaphore, &SemaphoreCounter);
    DEV_CHECK_ERR(VkResult == VK_SUCCESS, "Timeline Semaphore Unknown Error");
    return SemaphoreCounter;
    
}

VkSemaphore DILIGENT_CALL_TYPE FenceVkImpl::GetVkSemaphore()
{
    return m_Semaphore;
}

void FenceVkImpl::Reset(Uint64 Value)
{
    const auto& LogicalDevice = m_pDevice->GetLogicalDevice();

    VkSemaphoreSignalInfo SignalInfo = {};
    SignalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    SignalInfo.pNext = nullptr;
    SignalInfo.semaphore = m_Semaphore;
    SignalInfo.value = Value;

    auto VkResult = LogicalDevice.SignalSemaphore(m_Semaphore, SignalInfo);
    DEV_CHECK_ERR(VkResult == VK_SUCCESS, "Timeline Semaphore Unknown Error");
}

void FenceVkImpl::WaitForCompletion(Uint64 Value)
{   
    const auto& LogicalDevice = m_pDevice->GetLogicalDevice();

    VkSemaphoreWaitInfo WaitInfo = {};
    WaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    WaitInfo.pNext = nullptr;
    WaitInfo.flags = 0;
    WaitInfo.semaphoreCount = 1;
    WaitInfo.pSemaphores = &m_Semaphore;
    WaitInfo.pValues = &Value;
    
    auto VkResult = LogicalDevice.WaitSemaphores(WaitInfo, UINT64_MAX);
    DEV_CHECK_ERR(VkResult == VK_SUCCESS, "Timeline Semaphore Unknown Error");   
}

} // namespace Diligent
