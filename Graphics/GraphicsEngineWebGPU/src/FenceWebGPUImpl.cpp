/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#include "pch.h"

#include "FenceWebGPUImpl.hpp"
#include "GraphicsAccessories.hpp"
#include "QueueSignalPoolWebGPU.hpp"
#include "RenderDeviceWebGPUImpl.hpp"

namespace Diligent
{

FenceWebGPUImpl::FenceWebGPUImpl(IReferenceCounters*     pRefCounters,
                                 RenderDeviceWebGPUImpl* pDevice,
                                 const FenceDesc&        Desc) :
    TFenceBase{pRefCounters, pDevice, Desc}
{
    if (m_Desc.Type != FENCE_TYPE_CPU_WAIT_ONLY)
        LOG_ERROR_AND_THROW("Description of Fence '", m_Desc.Name, "' is invalid: ", GetFenceTypeString(m_Desc.Type), " is not supported in WebGPU.");
}

Uint64 FenceWebGPUImpl::GetCompletedValue()
{
    auto& SignalPoolWebGPU = m_pDevice->GetQueueSignalPool();

    while (!m_PendingSignals.empty())
    {
        const auto& QueryData = m_PendingSignals.front();

        // Timestamp values are implementation defined and may not increase monotonically.
        // The physical device may reset the timestamp counter occasionally,
        // which can result in unexpected values such as negative deltas between timestamps that logically should be monotonically increasing.
        if (SignalPoolWebGPU.GetQueryTimestamp(m_pDevice->GetWebGPUDevice(), QueryData.QueryIdx) == QueryData.LastTimestamp)
        {
            SignalPoolWebGPU.ReleaseQuery(QueryData.QueryIdx);
            UpdateLastCompletedFenceValue(QueryData.Value);
            m_PendingSignals.pop_front();
        }
        else
        {
            break;
        }
    }

    return m_LastCompletedFenceValue.load();
}

void FenceWebGPUImpl::Signal(Uint64 Value)
{
    DEV_ERROR("Signal() is not supported in WebGPU backend");
}

void FenceWebGPUImpl::Wait(Uint64 Value)
{
    auto& SignalPoolWebGPU = m_pDevice->GetQueueSignalPool();

    while (!m_PendingSignals.empty())
    {
        const auto& QueryData = m_PendingSignals.front();
        if (QueryData.Value > Value)
            break;

        while (SignalPoolWebGPU.GetQueryTimestamp(m_pDevice->GetWebGPUDevice(), QueryData.QueryIdx) == QueryData.LastTimestamp)
            std::this_thread::sleep_for(std::chrono::microseconds{1});

        SignalPoolWebGPU.ReleaseQuery(QueryData.QueryIdx);
        UpdateLastCompletedFenceValue(QueryData.Value);
        m_PendingSignals.pop_front();
    }
}

void FenceWebGPUImpl::AddPendingSignal(WGPUCommandEncoder wgpuCmdEncoder, Uint64 Value)
{
    auto&        SignalPoolWebGPU = m_pDevice->GetQueueSignalPool();
    const Uint32 QueryIdx         = SignalPoolWebGPU.AllocateQuery();
    const Uint64 QueryTimestamp   = SignalPoolWebGPU.GetQueryTimestamp(m_pDevice->GetWebGPUDevice(), QueryIdx);
    SignalPoolWebGPU.WriteTimestamp(wgpuCmdEncoder, QueryIdx);
    SignalPoolWebGPU.ResolveQuery(wgpuCmdEncoder, QueryIdx);
    m_PendingSignals.emplace_back(Value, QueryTimestamp, QueryIdx);
    DvpSignal(Value);
}

} // namespace Diligent
