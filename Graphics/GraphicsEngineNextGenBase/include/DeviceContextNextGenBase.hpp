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

#include <atomic>

#include "BasicTypes.h"
#include "ReferenceCounters.h"
#include "RefCntAutoPtr.hpp"
#include "DeviceContextBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "IndexWrapper.hpp"

namespace Diligent
{

/// Base implementation of the device context for next-generation backends.

template <typename EngineImplTraits>
class DeviceContextNextGenBase : public DeviceContextBase<EngineImplTraits>
{
public:
    using TBase             = DeviceContextBase<EngineImplTraits>;
    using DeviceImplType    = typename EngineImplTraits::RenderDeviceImplType;
    using ICommandQueueType = typename EngineImplTraits::CommandQueueInterface;

    DeviceContextNextGenBase(IReferenceCounters* pRefCounters,
                             DeviceImplType*     pRenderDevice,
                             ContextIndex        ContextId,
                             CommandQueueIndex   CommandQueueId,
                             const char*         Name,
                             bool                bIsDeferred) :
        // clang-format off
        TBase{pRefCounters, pRenderDevice, Name, bIsDeferred},
        m_ContextId                   {ContextId         },
        m_SubmittedBuffersCmdQueueMask{bIsDeferred ? 0 : Uint64{1} << Uint64{CommandQueueId}}
    // clang-format on
    {
        this->m_Desc.CommandQueueId = static_cast<Uint8>(CommandQueueId);
        VERIFY(bIsDeferred || ContextId == CommandQueueId,
               "For immediate contexts ContextId must be same as CommandQueueId");
    }

    ~DeviceContextNextGenBase()
    {
    }

    virtual ICommandQueueType* DILIGENT_CALL_TYPE LockCommandQueue() override final
    {
        if (this->IsDeferred())
        {
            LOG_WARNING_MESSAGE("Deferred contexts have no associated command queues");
            return nullptr;
        }
        return this->m_pDevice->LockCommandQueue(GetCommandQueueId());
    }

    virtual void DILIGENT_CALL_TYPE UnlockCommandQueue() override final
    {
        if (this->IsDeferred())
        {
            LOG_WARNING_MESSAGE("Deferred contexts have no associated command queues");
            return;
        }
        this->m_pDevice->UnlockCommandQueue(GetCommandQueueId());
    }

    ContextIndex GetContextId() const { return ContextIndex{m_ContextId}; }

    HardwareQueueId GetHardwareQueueId() const { return HardwareQueueId{this->m_Desc.QueueId}; }

    CommandQueueIndex GetCommandQueueId() const
    {
        VERIFY_EXPR(this->m_Desc.CommandQueueId < MAX_COMMAND_QUEUES);
        return CommandQueueIndex{this->m_Desc.CommandQueueId};
    }

    Uint64 GetSubmittedBuffersCmdQueueMask() const { return m_SubmittedBuffersCmdQueueMask.load(); }

protected:
    // Should be called at the end of FinishFrame()
    void EndFrame()
    {
        if (this->IsDeferred())
        {
            // For deferred context, reset submitted cmd queue mask
            m_SubmittedBuffersCmdQueueMask.store(0);

            m_Desc.QueueId        = MAX_COMMAND_QUEUES;
            m_Desc.CommandQueueId = MAX_COMMAND_QUEUES;
            m_Desc.ContextType    = CONTEXT_TYPE_UNKNOWN;
        }
        else
        {
            this->m_pDevice->FlushStaleResources(GetCommandQueueId());
        }
        TBase::EndFrame();
    }

    void UpdateSubmittedBuffersCmdQueueMask(Uint32 QueueId)
    {
        m_SubmittedBuffersCmdQueueMask.fetch_or(Uint64{1} << QueueId);
    }

private:
    const Uint32 m_ContextId;

    // This mask indicates which command queues command buffers from this context were submitted to.
    // For immediate context, this will always be 1 << GetCommandQueueId().
    // For deferred contexts, this will accumulate bits of the queues to which command buffers
    // were submitted to before FinishFrame() was called. This mask is used to release resources
    // allocated by the context during the frame when FinishFrame() is called.
    std::atomic_uint64_t m_SubmittedBuffersCmdQueueMask{0};
};

} // namespace Diligent
