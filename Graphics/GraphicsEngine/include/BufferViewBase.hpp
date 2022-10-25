/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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
/// Implementation of the Diligent::BufferViewBase template class

#include "BufferView.h"
#include "DeviceObjectBase.hpp"
#include "GraphicsTypes.h"
#include "RefCntAutoPtr.hpp"

namespace Diligent
{

/// Template class implementing base functionality of the buffer view object

/// \tparam EngineImplTraits - Engine implementation type traits.
template <typename EngineImplTraits>
class BufferViewBase : public DeviceObjectBase<typename EngineImplTraits::BufferViewInterface, typename EngineImplTraits::RenderDeviceImplType, BufferViewDesc>
{
public:
    // Base interface that this class inherits (IBufferViewD3D12, IBufferViewVk, etc.).
    using BaseInterface = typename EngineImplTraits::BufferViewInterface;

    // Render device implementation type (RenderDeviceD3D12Impl, RenderDeviceVkImpl, etc.).
    using RenderDeviceImplType = typename EngineImplTraits::RenderDeviceImplType;

    // Buffer implementation type (BufferD3D12Impl, BufferVkImpl, etc.).
    using BufferImplType = typename EngineImplTraits::BufferImplType;

    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, BufferViewDesc>;

    /// \param pRefCounters   - Reference counters object that controls the lifetime of this buffer view.
    /// \param pDevice        - Pointer to the render device.
    /// \param ViewDesc       - Buffer view description.
    /// \param pBuffer        - Pointer to the buffer that the view is to be created for.
    /// \param bIsDefaultView - Flag indicating if the view is a default view, and is thus
    ///						    part of the buffer object. In this case the view will attach
    ///							to the buffer's reference counters.
    BufferViewBase(IReferenceCounters*   pRefCounters,
                   RenderDeviceImplType* pDevice,
                   const BufferViewDesc& ViewDesc,
                   IBuffer*              pBuffer,
                   bool                  bIsDefaultView) :
        // Default views are created as part of the buffer, so we cannot not keep strong
        // reference to the buffer to avoid cyclic links. Instead, we will attach to the
        // reference counters of the buffer.
        TDeviceObjectBase(pRefCounters, pDevice, ViewDesc),
        m_pBuffer{pBuffer},
        // For non-default view, we will keep strong reference to buffer
        m_spBuffer{bIsDefaultView ? nullptr : pBuffer}
    {}

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_IBufferView, TDeviceObjectBase)

    /// Implementation of IBufferView::GetBuffer()
    virtual IBuffer* DILIGENT_CALL_TYPE GetBuffer() const override final
    {
        return m_pBuffer;
    }

    template <typename BufferType>
    BufferType* GetBuffer()
    {
        return ClassPtrCast<BufferType>(m_pBuffer);
    }

    template <typename BufferType>
    BufferType* GetBuffer() const
    {
        return ClassPtrCast<BufferType>(m_pBuffer);
    }

protected:
    /// Pointer to the buffer
    IBuffer* const m_pBuffer;

    /// Strong reference to the buffer. Used for non-default views
    /// to keep the buffer alive
    RefCntAutoPtr<IBuffer> m_spBuffer;
};

} // namespace Diligent
