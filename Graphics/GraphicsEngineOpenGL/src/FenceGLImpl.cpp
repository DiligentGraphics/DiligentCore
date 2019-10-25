/*     Copyright 2019 Diligent Graphics LLC
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

#include "FenceGLImpl.h"
#include "EngineMemory.h"

namespace Diligent
{
    
FenceGLImpl :: FenceGLImpl(IReferenceCounters* pRefCounters,
                           RenderDeviceGLImpl* pDevice,
                           const FenceDesc&    Desc) : 
    TFenceBase
    {
        pRefCounters,
        pDevice,
        Desc
    }
{
}

FenceGLImpl :: ~FenceGLImpl()
{
}

Uint64 FenceGLImpl :: GetCompletedValue()
{
    while (!m_PendingFences.empty())
    {
        auto& val_fence = m_PendingFences.front();
        auto res = glClientWaitSync(val_fence.second, 
            0, // Can be SYNC_FLUSH_COMMANDS_BIT
            0  // Timeout in nanoseconds
        );
        if(res == GL_ALREADY_SIGNALED)
        {
            if (val_fence.first > m_LastCompletedFenceValue)
                m_LastCompletedFenceValue = val_fence.first;
            m_PendingFences.pop_front();
        }
        else
        {
            break;
        }
    }

    return m_LastCompletedFenceValue;
}

void FenceGLImpl :: Wait(Uint64 Value, bool FlushCommands)
{
    while (!m_PendingFences.empty())
    {
        auto& val_fence = m_PendingFences.front();  
        if (val_fence.first > Value)
            break;

        auto res = glClientWaitSync(val_fence.second, FlushCommands ? GL_SYNC_FLUSH_COMMANDS_BIT : 0, std::numeric_limits<GLuint64>::max());
        VERIFY_EXPR(res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED); (void)res;

        if (val_fence.first > m_LastCompletedFenceValue)
            m_LastCompletedFenceValue = val_fence.first;
        m_PendingFences.pop_front();
    }
}

void FenceGLImpl :: Reset(Uint64 Value)
{
    DEV_CHECK_ERR(Value >= m_LastCompletedFenceValue, "Resetting fence '", m_Desc.Name, "' to the value (", Value, ") that is smaller than the last completed value (", m_LastCompletedFenceValue, ")");
    if (Value > m_LastCompletedFenceValue)
        m_LastCompletedFenceValue = Value;
}

}
