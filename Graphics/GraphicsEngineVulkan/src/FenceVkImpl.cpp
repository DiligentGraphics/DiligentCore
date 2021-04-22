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
#include "RenderDeviceVkImpl.hpp"
#include "CommandQueueVkImpl.hpp"
#include "EngineMemory.h"

namespace Diligent
{

FenceVkImpl::FenceVkImpl(IReferenceCounters* pRefCounters,
                         RenderDeviceVkImpl* pRendeDeviceVkImpl,
                         const FenceDesc&    Desc,
                         bool                IsDeviceInternal) :
    // clang-format off
    TFenceBase
    {
        pRefCounters,
        pRendeDeviceVkImpl,
        Desc,
        IsDeviceInternal
    }
// clang-format on
{
    const auto& LogicalDevice = pRendeDeviceVkImpl->GetLogicalDevice();
    if (LogicalDevice.GetEnabledExtFeatures().TimelineSemaphore.timelineSemaphore == VK_TRUE)
    {
        m_TimelineSemaphore = LogicalDevice.CreateTimelineSemaphore(0, m_Desc.Name);
    }
}

FenceVkImpl::~FenceVkImpl()
{
    if (IsTimelineSemaphore())
    {
        VERIFY_EXPR(m_SyncPoints.empty());
    }
    else if (!m_SyncPoints.empty())
    {
        LOG_INFO_MESSAGE("FenceVkImpl::~FenceVkImpl(): waiting for ", m_SyncPoints.size(), " pending Vulkan ",
                         (m_SyncPoints.size() > 1 ? "fences." : "fence."));
        // Vulkan spec states that all queue submission commands that refer to
        // a fence must have completed execution before the fence is destroyed.
        // (https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-vkDestroyFence-fence-01120)
        Wait(UINT64_MAX);
    }
}

Uint64 FenceVkImpl::GetCompletedValue()
{
    if (IsTimelineSemaphore())
    {
        const auto& LogicalDevice    = m_pDevice->GetLogicalDevice();
        Uint64      SemaphoreCounter = ~Uint64{0};
        auto        err              = LogicalDevice.GetSemaphoreCounter(m_TimelineSemaphore, &SemaphoreCounter);
        DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to get timeline semaphore counter");
        return SemaphoreCounter;
    }
    else
    {
        std::lock_guard<std::mutex> Lock{m_SyncPointsGuard};
        return InternalGetCompletedValue();
    }
}

Uint64 FenceVkImpl::InternalGetCompletedValue()
{
    VERIFY_EXPR(!IsTimelineSemaphore());

    const auto& LogicalDevice = m_pDevice->GetLogicalDevice();
    while (!m_SyncPoints.empty())
    {
        auto& Item = m_SyncPoints.front();

        auto status = LogicalDevice.GetFenceStatus(Item.SyncPoint->GetFence());
        if (status == VK_SUCCESS)
        {
            UpdateLastCompletedFenceValue(Item.Value);
            m_SyncPoints.pop_front();
        }
        else
        {
            break;
        }
    }

#ifdef DILIGENT_DEVELOPMENT
    // Fence may sometimes be used for GPU sync, so set the flag only when the fence has pending sync points.
    if (m_SyncPoints.empty())
        m_dvpUsedForGPUSync = false;
#endif

    return m_LastCompletedFenceValue.load();
}

void FenceVkImpl::Reset(Uint64 Value)
{
    if (IsTimelineSemaphore())
    {
        const auto& LogicalDevice = m_pDevice->GetLogicalDevice();

        VkSemaphoreSignalInfo SignalInfo{};
        SignalInfo.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        SignalInfo.pNext     = nullptr;
        SignalInfo.semaphore = m_TimelineSemaphore;
        SignalInfo.value     = Value;

        auto err = LogicalDevice.SignalSemaphore(SignalInfo);
        DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to signal timeline semaphore");
    }
    else
    {
        std::lock_guard<std::mutex> Lock{m_SyncPointsGuard};

        DEV_CHECK_ERR(!m_dvpUsedForGPUSync, "Reseting fence that is used for synchronization between queues is very dangerous and is not allowed as it may cause data race or a deadlock.");
        DEV_CHECK_ERR(Value >= m_LastCompletedFenceValue.load(), "Resetting the fence '", m_Desc.Name, "' to the value (", Value, ") that is smaller than the last completed value (", m_LastCompletedFenceValue, ")");
        UpdateLastCompletedFenceValue(Value);
    }
}

void FenceVkImpl::Wait(Uint64 Value)
{
    if (IsTimelineSemaphore())
    {
        const auto& LogicalDevice = m_pDevice->GetLogicalDevice();

        VkSemaphoreWaitInfo WaitInfo{};
        WaitInfo.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        WaitInfo.pNext          = nullptr;
        WaitInfo.flags          = 0;
        WaitInfo.semaphoreCount = 1;
        WaitInfo.pSemaphores    = &m_TimelineSemaphore;
        WaitInfo.pValues        = &Value;

        auto err = LogicalDevice.WaitSemaphores(WaitInfo, UINT64_MAX);
        DEV_CHECK_ERR(err == VK_SUCCESS, "Timeline Semaphore Unknown Error");
    }
    else
    {
        std::lock_guard<std::mutex> Lock{m_SyncPointsGuard};

        const auto& LogicalDevice = m_pDevice->GetLogicalDevice();
        while (!m_SyncPoints.empty())
        {
            auto& Item = m_SyncPoints.front();
            if (Item.Value > Value)
                break;

            VkFence Fence  = Item.SyncPoint->GetFence();
            auto    status = LogicalDevice.GetFenceStatus(Fence);
            if (status == VK_NOT_READY)
            {
                status = LogicalDevice.WaitForFences(1, &Fence, VK_TRUE, UINT64_MAX);
            }

            DEV_CHECK_ERR(status == VK_SUCCESS, "All pending fences must now be complete!");
            UpdateLastCompletedFenceValue(Item.Value);

            m_SyncPoints.pop_front();
        }

#ifdef DILIGENT_DEVELOPMENT
        // Fence may sometimes be used for GPU sync, so set the flag only when the fence has pending sync points.
        if (m_SyncPoints.empty())
            m_dvpUsedForGPUSync = false;
#endif
    }
}

VulkanUtilities::VulkanRecycledSemaphore FenceVkImpl::ExtractSignalSemaphore(CommandQueueIndex CommandQueueId, Uint64 Value)
{
    if (IsTimelineSemaphore())
    {
        UNEXPECTED("Not supported when timeline semaphore is used");
        return {};
    }

    std::lock_guard<std::mutex> Lock{m_SyncPointsGuard};

    VulkanUtilities::VulkanRecycledSemaphore Result;

#ifdef DILIGENT_DEVELOPMENT
    if (!m_SyncPoints.empty())
    {
        DEV_CHECK_ERR(Value <= m_SyncPoints.back().Value,
                      "Can not wait for value ", Value, " that is greater than the last signaled value (", m_SyncPoints.back().Value,
                      "). This will cause a deadlock. Use timeline semaphore to avoid this.");
    }
    m_dvpUsedForGPUSync = true;
#endif

    // Find the last non-null semaphore
    for (auto Iter = m_SyncPoints.begin(); Iter != m_SyncPoints.end(); ++Iter)
    {
        auto SemaphoreForContext = Iter->SyncPoint->ExtractSemaphore(CommandQueueId);
        if (SemaphoreForContext)
            Result = std::move(SemaphoreForContext);

        if (Iter->Value >= Value)
            break;
    }

    const auto& LogicalDevice = m_pDevice->GetLogicalDevice();

    // If IFence is used only for synchronization between queues, it will accumulate many more sync points.
    // We need to check VkFence and remove already reached sync points.
    while (!m_SyncPoints.empty())
    {
        auto& Item   = m_SyncPoints.front();
        auto  status = LogicalDevice.GetFenceStatus(Item.SyncPoint->GetFence());
        if (status == VK_NOT_READY)
            break;

        DEV_CHECK_ERR(status == VK_SUCCESS, "All pending fences must now be complete!");
        UpdateLastCompletedFenceValue(Item.Value);

        m_SyncPoints.pop_front();
    }

    return Result;
}

void FenceVkImpl::AddPendingSyncPoint(CommandQueueIndex CommandQueueId, Uint64 Value, SyncPointVkPtr SyncPoint)
{
    if (IsTimelineSemaphore())
    {
        UNEXPECTED("Not supported when timeline semaphore is used");
        return;
    }
    if (SyncPoint == nullptr)
    {
        UNEXPECTED("SyncPoint is null");
        return;
    }

    std::lock_guard<std::mutex> Lock{m_SyncPointsGuard};

#ifdef DILIGENT_DEVELOPMENT
    if (!m_SyncPoints.empty())
    {
        DEV_CHECK_ERR(Value > m_SyncPoints.back().Value,
                      "New fence value (", Value, ") must be greater than the previous value (", m_SyncPoints.back().Value, ")");

        DEV_CHECK_ERR(m_SyncPoints.back().SyncPoint->GetCommandQueueId() == CommandQueueId,
                      "Fence enqueued for signal operation in command queue ", CommandQueueId,
                      ", but previous signal operation was in command queue ", m_SyncPoints.back().SyncPoint->GetCommandQueueId(),
                      ". This may cause data race or deadlock. Call Wait() to ensure that all pending signal operation have been completed.");
    }
#endif

    // Remove already completed sync points
    if (m_SyncPoints.size() > RequiredArraySize)
    {
        InternalGetCompletedValue();
    }

    VERIFY(m_SyncPoints.size() < RequiredArraySize * 2, "array of sync points is too large, none of the GetCompletedValue(), Wait() or ExtractSignalSemaphore() have been used.");

    m_SyncPoints.push_back({Value, std::move(SyncPoint)});
}

} // namespace Diligent
