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

#pragma once

/// \file
/// Declaration of Diligent::FenceWebGPUImpl class

#include "EngineWebGPUImplTraits.hpp"
#include "FenceBase.hpp"

namespace Diligent
{

/// Fence object implementation in WebGPU backend.
class FenceWebGPUImpl final : public FenceBase<EngineWebGPUImplTraits>
{
public:
    using TFenceBase = FenceBase<EngineWebGPUImplTraits>;

    FenceWebGPUImpl(IReferenceCounters*     pRefCounters,
                    RenderDeviceWebGPUImpl* pDevice,
                    const FenceDesc&        Desc);

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_FenceWebGPU, TFenceBase)

    /// Implementation of IFence::GetCompletedValue() in WebGPU backend.
    Uint64 DILIGENT_CALL_TYPE GetCompletedValue() override;

    /// Implementation of IFence::Signal() in WebGPU backend.
    void DILIGENT_CALL_TYPE Signal(Uint64 Value) override;

    /// Implementation of IFence::Wait() in WebGPU backend.
    void DILIGENT_CALL_TYPE Wait(Uint64 Value) override;

    void AddPendingSignal(WGPUCommandEncoder wgpuCmdEncoder, Uint64 Value);

private:
    struct PendingFenceData
    {
        const Uint64 Value;
        const Uint64 LastTimestamp;
        const Uint32 QueryIdx;

        PendingFenceData(Uint64 _Value, Uint64 _LastTimestamp, Uint32 _QueryIdx) :
            // clang-format off
            Value         {_Value},
            LastTimestamp {_LastTimestamp},
            QueryIdx      {_QueryIdx}

        // clang-format on
        {}
    };
    std::deque<PendingFenceData> m_PendingSignals;
};

} // namespace Diligent
