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
}

FenceVkImpl::~FenceVkImpl()
{
    if (!m_SyncPoints.empty())
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
    std::lock_guard<std::mutex> Lock{m_Guard};
    return InternalGetCompletedValue();
}

Uint64 FenceVkImpl::InternalGetCompletedValue()
{
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

    return m_LastCompletedFenceValue.load();
}

void FenceVkImpl::Reset(Uint64 Value)
{
    DEV_CHECK_ERR(Value >= m_LastCompletedFenceValue.load(), "Resetting fence '", m_Desc.Name, "' to the value (", Value, ") that is smaller than the last completed value (", m_LastCompletedFenceValue, ")");
    UpdateLastCompletedFenceValue(Value);
}


void FenceVkImpl::Wait(Uint64 Value)
{
    std::lock_guard<std::mutex> Lock{m_Guard};

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
}

VulkanUtilities::VulkanRecycledSemaphore FenceVkImpl::ExtractSignalSemaphore(CommandQueueIndex CommandQueueId, Uint64 Value)
{
    std::lock_guard<std::mutex> Lock{m_Guard};

    VulkanUtilities::VulkanRecycledSemaphore Result;

    // Find last non-null semaphore
    for (auto Iter = m_SyncPoints.begin(); Iter != m_SyncPoints.end(); ++Iter)
    {
        auto SemaphoreForContext = Iter->SyncPoint->ExtractSemaphore(CommandQueueId);
        if (SemaphoreForContext)
            Result = std::move(SemaphoreForContext);

        if (Iter->Value >= Value)
            break;
    }

    const auto& LogicalDevice = m_pDevice->GetLogicalDevice();

    // If IFence is used only for synchronization between queues it will accumulate much more sync points.
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
    std::lock_guard<std::mutex> Lock{m_Guard};

#ifdef DILIGENT_DEVELOPMENT
    if (!m_SyncPoints.empty())
    {
        DEV_CHECK_ERR(Value > m_SyncPoints.back().Value,
                      "New value for fence (", Value, ") must be greater than previous value (", m_SyncPoints.back().Value, ")");

        DEV_CHECK_ERR(m_SyncPoints.back().SyncPoint->GetCommandQueueId() == CommandQueueId,
                      "Fence enqueued for signal operation in command queue (", CommandQueueId,
                      ") but previous signal operation was in command queue (", m_SyncPoints.back().SyncPoint->GetCommandQueueId(),
                      ") this may cause the data race or deadlock. Call Wait() before to unsure that all pending signal operation have been completed.");
    }
#endif

    // Remove already completed sync points
    if (m_SyncPoints.size() > RequiredArraySize)
    {
        (void)(InternalGetCompletedValue());
    }

    VERIFY(m_SyncPoints.size() < RequiredArraySize * 2, "array of sync points is too big, none of the GetCompletedValue(), Wait() or ExtractSignalSemaphore() are used");

    m_SyncPoints.push_back({Value, std::move(SyncPoint)});
}

} // namespace Diligent
