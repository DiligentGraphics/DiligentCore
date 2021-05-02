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
/// Declaration of Diligent::FenceVkImpl class

#include <deque>
#include <atomic>

#include "EngineVkImplTraits.hpp"
#include "FenceBase.hpp"
#include "VulkanUtilities/VulkanObjectWrappers.hpp"
#include "VulkanUtilities/VulkanSyncObjectManager.hpp"

namespace Diligent
{
using SyncPointVkPtr = std::shared_ptr<class SyncPointVk>;


/// Fence implementation in Vulkan backend.
class FenceVkImpl final : public FenceBase<EngineVkImplTraits>
{
public:
    using TFenceBase = FenceBase<EngineVkImplTraits>;

    FenceVkImpl(IReferenceCounters* pRefCounters,
                RenderDeviceVkImpl* pRendeDeviceVkImpl,
                const FenceDesc&    Desc,
                bool                IsDeviceInternal = false);
    ~FenceVkImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_FenceVk, TFenceBase)

    /// Implementation of IFence::GetCompletedValue() in Vulkan backend.
    /// Note that this method is not thread-safe. The reason is that VulkanFencePool is not thread
    /// safe, and DeviceContextVkImpl::SignalFence() adds the fence to the pending fences list that
    /// are signaled later by the command context when it submits the command list. So there is no
    /// guarantee that the fence pool is not accessed simultaneously by multiple threads even if the
    /// fence object itself is protected by mutex.
    virtual Uint64 DILIGENT_CALL_TYPE GetCompletedValue() override final;

    /// Implementation of IFence::Reset() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE Reset(Uint64 Value) override final;

    /// Implementation of IFence::Wait() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE Wait(Uint64 Value) override final;

    /// Implementation of IFence::GetVkSemaphore().
    virtual VkSemaphore DILIGENT_CALL_TYPE GetVkSemaphore() override final;

    VulkanUtilities::VulkanRecycledSemaphore ExtractSignalSemaphore(CommandQueueIndex CommandQueueId, Uint64 Value);

    void AddPendingSyncPoint(CommandQueueIndex CommandQueueId, Uint64 Value, SyncPointVkPtr SyncPoint);

private:
    Uint64 InternalGetCompletedValue();

    VulkanUtilities::SemaphoreWrapper m_Semaphore;

    static constexpr Uint32 RequiredArraySize = 8;

    struct SyncPointData
    {
        Uint64         Value;
        SyncPointVkPtr SyncPoint;
    };

    std::mutex                m_Guard;      // Protects access to the m_SyncPoints
    std::deque<SyncPointData> m_SyncPoints; // TODO: use ring buffer
};

} // namespace Diligent
