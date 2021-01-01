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

#include "DynamicBuffer.hpp"

#include <algorithm>

#include "DebugUtilities.hpp"

namespace Diligent
{

DynamicBuffer::DynamicBuffer(IRenderDevice* pDevice, const BufferDesc& Desc) :
    m_Desc{Desc},
    m_Name{Desc.Name != nullptr ? Desc.Name : "Dynamic buffer"}
{
    m_Desc.Name = m_Name.c_str();
    if (m_Desc.uiSizeInBytes > 0 && pDevice != nullptr)
    {
        pDevice->CreateBuffer(Desc, nullptr, &m_pBuffer);
        VERIFY_EXPR(m_pBuffer);
    }
}

void DynamicBuffer::CommitResize(IRenderDevice*  pDevice,
                                 IDeviceContext* pContext)
{
    if (!m_pBuffer && m_Desc.uiSizeInBytes > 0 && pDevice != nullptr)
    {
        pDevice->CreateBuffer(m_Desc, nullptr, &m_pBuffer);
        VERIFY_EXPR(m_pBuffer);
        ++m_Version;
    }

    if (m_pStaleBuffer && m_pBuffer && pContext != nullptr)
    {
        auto CopySize = std::min(m_Desc.uiSizeInBytes, m_pStaleBuffer->GetDesc().uiSizeInBytes);
        pContext->CopyBuffer(m_pStaleBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                             m_pBuffer, 0, CopySize, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pStaleBuffer.Release();
    }
}

IBuffer* DynamicBuffer::Resize(IRenderDevice*  pDevice,
                               IDeviceContext* pContext,
                               Uint32          NewSize)
{
    if (m_Desc.uiSizeInBytes != NewSize)
    {
        if (!m_pStaleBuffer)
            m_pStaleBuffer = std::move(m_pBuffer);
        else
        {
            DEV_CHECK_ERR(!m_pBuffer || NewSize == 0,
                          "There is a non-null stale buffer. This likely indicates that "
                          "Resize() has been called multiple times with different sizes, "
                          "but copy has not been committed by providing non-null device "
                          "context to either Resize() or GetBuffer()");
        }

        m_Desc.uiSizeInBytes = NewSize;

        if (m_Desc.uiSizeInBytes == 0)
        {
            m_pStaleBuffer.Release();
            m_pBuffer.Release();
        }
    }

    CommitResize(pDevice, pContext);

    return m_pBuffer;
}

IBuffer* DynamicBuffer::GetBuffer(IRenderDevice*  pDevice,
                                  IDeviceContext* pContext)
{
    DEV_CHECK_ERR(m_pBuffer || m_Desc.uiSizeInBytes == 0 || pDevice != nullptr,
                  "A new buffer must be created, but pDevice is null. Use PendingUpdate() to check if the buffer must be updated.");
    DEV_CHECK_ERR(!m_pStaleBuffer || pContext != nullptr,
                  "An existing contents of the buffer must be copied to the new buffer, but pContext is null. "
                  "Use PendingUpdate() to check if the buffer must be updated.");
    CommitResize(pDevice, pContext);

    return m_pBuffer;
}

} // namespace Diligent
