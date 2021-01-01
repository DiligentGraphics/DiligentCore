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
/// Declaration of a DynamicBuffer class

#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../GraphicsEngine/interface/Buffer.h"
#include "../../../Common/interface/RefCntAutoPtr.hpp"

namespace Diligent
{

/// Dynamically resizable buffer
class DynamicBuffer
{
public:
    /// Initialies the dynamic buffer.

    /// \param[in] pDevice - Render device that will be used to create the buffer.
    ///                      This parameter may be null (see remarks).
    /// \param[in] Desc    - Buffer description.
    ///
    /// \remarks            If pDevice is null, internal buffer creation will be postponed
    ///                     until GetBuffer() or Resize() is called.
    DynamicBuffer(IRenderDevice* pDevice, const BufferDesc& Desc);

    // clang-format off
    DynamicBuffer           (const DynamicBuffer&)  = delete;
    DynamicBuffer& operator=(const DynamicBuffer&)  = delete;
    DynamicBuffer           (      DynamicBuffer&&) = delete;
    DynamicBuffer& operator=(      DynamicBuffer&&) = delete;
    // clang-format on


    /// Resizes the buffer to the new size.

    /// \param[in] pDevice  - Render device that will be used create a new internal buffer.
    ///                       This parameter may be null (see remarks).
    /// \param[in] pContext - Device context that will be used to copy existing contents
    ///                       to the new buffer. This parameter may be null (see remarks).
    /// \param[in] NewSize  - New buffer size. Can be zero.
    /// \return               Pointer to the new buffer.
    ///
    /// \remarks    The method operation depends on which of pDevice and pContext parameters
    ///             are not null:
    ///             - Both pDevice and pContext are not null: internal buffer is created
    ///               and existing contents is copied. GetBuffer() may be called with
    ///               both pDevice and pContext being null.
    ///             - pDevice is not null, pContext is null: internal buffer is created,
    ///               but existing contents is not copied. An application must provide non-null
    ///               device context when calling GetBuffer().
    ///             - Both pDevice and pContext are null: internal buffer is not created.
    ///               An application must provide non-null device and device context when calling
    ///               GetBuffer().
    ///
    ///             Typically pDevice and pContext should be null when the method is called from a worker thread.
    ///
    ///             If NewSize is zero, internal buffer will be released.
    IBuffer* Resize(IRenderDevice*  pDevice,
                    IDeviceContext* pContext,
                    Uint32          NewSize);


    /// Returns the pointer to the buffer object, initializing it if necessary.

    /// \param[in] pDevice  - Render device that will be used to create the new buffer,
    ///                       if necessary (see remarks).
    /// \param[in] pContext - Device context that will be used to copy existing
    ///                       buffer contents, if necessary (see remarks).
    /// \return               The pointer to the buffer object.
    ///
    /// \remarks    If the buffer has been resized, but internal buffer object has not been
    ///             initialized, pDevice and pContext must not be null.
    ///
    ///             If buffer does not need to be updated (PendingUpdate() returns false),
    ///             both pDevice and pContext may be null.
    IBuffer* GetBuffer(IRenderDevice*  pDevice,
                       IDeviceContext* pContext);


    /// Returns true if the buffer buffer must be updated before use (e.g. it has been resized,
    /// but internal buffer has not been initialized or updated).
    /// When update is not pending, GetBuffer() may be called with null device and context.
    bool PendingUpdate() const
    {
        return (m_Desc.uiSizeInBytes > 0) && (!m_pBuffer || m_pStaleBuffer);
    }


    /// Returns the buffer description.
    const BufferDesc& GetDesc()
    {
        return m_Desc;
    }


    /// Returns dynamic buffer version.
    /// The version is incremented every time a new internal buffer is created.
    Uint32 GetVersion() const
    {
        return m_Version;
    }

private:
    void CommitResize(IRenderDevice*  pDevice,
                      IDeviceContext* pContext);

    BufferDesc        m_Desc;
    const std::string m_Name;
    Uint32            m_Version = 0;

    RefCntAutoPtr<IBuffer> m_pBuffer;
    RefCntAutoPtr<IBuffer> m_pStaleBuffer;
};

} // namespace Diligent
