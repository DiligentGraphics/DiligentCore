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
/// Implementation of the Diligent::BufferBase template class

#include <memory>

#include "Buffer.h"
#include "GraphicsTypes.h"
#include "DeviceObjectBase.hpp"
#include "GraphicsAccessories.hpp"
#include "STDAllocator.hpp"
#include "FormatString.hpp"

namespace Diligent
{

/// Validates buffer description and throws an exception in case of an error.
void ValidateBufferDesc(const BufferDesc& Desc, const DeviceCaps& deviceCaps) noexcept(false);

/// Validates initial buffer data parameters and throws an exception in case of an error.
void ValidateBufferInitData(const BufferDesc& Desc, const BufferData* pBuffData) noexcept(false);

/// Validates and corrects buffer view description; throws an exception in case of an error.
void ValidateAndCorrectBufferViewDesc(const BufferDesc& BuffDesc, BufferViewDesc& ViewDesc) noexcept(false);


/// Template class implementing base functionality of the buffer object

/// \tparam BaseInterface - Base interface that this class will inheret
///                         (Diligent::IBufferD3D11, Diligent::IBufferD3D12,
///                          Diligent::IBufferGL or Diligent::IBufferVk).
/// \tparam RenderDeviceImplType - Type of the render device implementation
///                                (Diligent::RenderDeviceD3D11Impl, Diligent::RenderDeviceD3D12Impl,
///                                 Diligent::RenderDeviceGLImpl, or Diligent::RenderDeviceVkImpl)
/// \tparam BufferViewImplType - Type of the buffer view implementation
///                              (Diligent::BufferViewD3D11Impl, Diligent::BufferViewD3D12Impl,
///                               Diligent::BufferViewGLImpl or Diligent::BufferViewVkImpl)
/// \tparam TBuffViewObjAllocator - type of the allocator that is used to allocate memory for the buffer view object instances
template <class BaseInterface, class RenderDeviceImplType, class BufferViewImplType, class TBuffViewObjAllocator>
class BufferBase : public DeviceObjectBase<BaseInterface, RenderDeviceImplType, BufferDesc>
{
public:
    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, BufferDesc>;

    /// \param pRefCounters         - Reference counters object that controls the lifetime of this buffer.
    /// \param BuffViewObjAllocator - Allocator that is used to allocate memory for the buffer view instances.
    ///                               This parameter is only used for debug purposes.
    /// \param pDevice              - Pointer to the device.
    /// \param BuffDesc             - Buffer description.
    /// \param bIsDeviceInternal    - Flag indicating if the buffer is an internal device object and
    ///							      must not keep a strong reference to the device.
    BufferBase(IReferenceCounters*    pRefCounters,
               TBuffViewObjAllocator& BuffViewObjAllocator,
               RenderDeviceImplType*  pDevice,
               const BufferDesc&      BuffDesc,
               bool                   bIsDeviceInternal) :
        TDeviceObjectBase{pRefCounters, pDevice, BuffDesc, bIsDeviceInternal},
#ifdef DILIGENT_DEBUG
        m_dbgBuffViewAllocator{BuffViewObjAllocator},
#endif
        m_pDefaultUAV{nullptr, STDDeleter<BufferViewImplType, TBuffViewObjAllocator>(BuffViewObjAllocator)},
        m_pDefaultSRV{nullptr, STDDeleter<BufferViewImplType, TBuffViewObjAllocator>(BuffViewObjAllocator)}
    {
        ValidateBufferDesc(this->m_Desc, pDevice->GetDeviceCaps());

        Uint64 DeviceQueuesMask = pDevice->GetCommandQueueMask();
        DEV_CHECK_ERR((this->m_Desc.CommandQueueMask & DeviceQueuesMask) != 0, "No bits in the command queue mask (0x", std::hex, this->m_Desc.CommandQueueMask, ") correspond to one of ", pDevice->GetCommandQueueCount(), " available device command queues");
        this->m_Desc.CommandQueueMask &= DeviceQueuesMask;
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_Buffer, TDeviceObjectBase)

    /// Implementation of IBuffer::CreateView(); calls CreateViewInternal() virtual function
    /// that creates buffer view for the specific engine implementation.
    virtual void DILIGENT_CALL_TYPE CreateView(const struct BufferViewDesc& ViewDesc, IBufferView** ppView) override
    {
        DEV_CHECK_ERR(ViewDesc.ViewType != BUFFER_VIEW_UNDEFINED, "Buffer view type is not specified");
        if (ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE)
            DEV_CHECK_ERR(this->m_Desc.BindFlags & BIND_SHADER_RESOURCE, "Attempting to create SRV for buffer '", this->m_Desc.Name, "' that was not created with BIND_SHADER_RESOURCE flag");
        else if (ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS)
            DEV_CHECK_ERR(this->m_Desc.BindFlags & BIND_UNORDERED_ACCESS, "Attempting to create UAV for buffer '", this->m_Desc.Name, "' that was not created with BIND_UNORDERED_ACCESS flag");
        else
            UNEXPECTED("Unexpected buffer view type");

        CreateViewInternal(ViewDesc, ppView, false);
    }


    /// Implementation of IBuffer::GetDefaultView().
    virtual IBufferView* DILIGENT_CALL_TYPE GetDefaultView(BUFFER_VIEW_TYPE ViewType) override
    {
        switch (ViewType)
        {
            case BUFFER_VIEW_SHADER_RESOURCE: return m_pDefaultSRV.get();
            case BUFFER_VIEW_UNORDERED_ACCESS: return m_pDefaultUAV.get();
            default: UNEXPECTED("Unknown view type"); return nullptr;
        }
    }


    /// Creates default buffer views.

    ///
    /// - Creates default shader resource view addressing the entire buffer if Diligent::BIND_SHADER_RESOURCE flag is set
    /// - Creates default unordered access view addressing the entire buffer if Diligent::BIND_UNORDERED_ACCESS flag is set
    ///
    /// The function calls CreateViewInternal().
    void CreateDefaultViews()
    {
        // Create default views for structured and raw buffers. For formatted buffers we do not know the view format, so
        // cannot create views.

        auto CreateDefaultView = [&](BUFFER_VIEW_TYPE ViewType) //
        {
            BufferViewDesc ViewDesc;
            ViewDesc.ViewType = ViewType;

            std::string ViewName;
            switch (ViewType)
            {
                case BUFFER_VIEW_UNORDERED_ACCESS:
                    ViewName = "Default UAV of buffer '";
                    break;

                case BUFFER_VIEW_SHADER_RESOURCE:
                    ViewName = "Default SRV of buffer '";
                    break;

                default:
                    UNEXPECTED("Unexpected buffer view type");
            }

            ViewName += this->m_Desc.Name;
            ViewName += '\'';
            ViewDesc.Name = ViewName.c_str();

            IBufferView* pView = nullptr;
            CreateViewInternal(ViewDesc, &pView, true);
            VERIFY(pView != nullptr, "Failed to create default view for buffer '", this->m_Desc.Name, "'");
            VERIFY(pView->GetDesc().ViewType == ViewType, "Unexpected view type");

            return static_cast<BufferViewImplType*>(pView);
        };

        if (this->m_Desc.BindFlags & BIND_UNORDERED_ACCESS && (this->m_Desc.Mode == BUFFER_MODE_STRUCTURED || this->m_Desc.Mode == BUFFER_MODE_RAW))
        {
            m_pDefaultUAV.reset(CreateDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
        }

        if (this->m_Desc.BindFlags & BIND_SHADER_RESOURCE && (this->m_Desc.Mode == BUFFER_MODE_STRUCTURED || this->m_Desc.Mode == BUFFER_MODE_RAW))
        {
            m_pDefaultSRV.reset(CreateDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
        }
    }

    virtual void DILIGENT_CALL_TYPE SetState(RESOURCE_STATE State) override final
    {
        this->m_State = State;
    }

    virtual RESOURCE_STATE DILIGENT_CALL_TYPE GetState() const override final
    {
        return this->m_State;
    }

    bool IsInKnownState() const
    {
        return this->m_State != RESOURCE_STATE_UNKNOWN;
    }

    bool CheckState(RESOURCE_STATE State) const
    {
        DEV_CHECK_ERR((State & (State - 1)) == 0, "Single state is expected");
        DEV_CHECK_ERR(IsInKnownState(), "Buffer state is unknown");
        return (this->m_State & State) == State;
    }

protected:
    /// Pure virtual function that creates buffer view for the specific engine implementation.
    virtual void CreateViewInternal(const struct BufferViewDesc& ViewDesc, IBufferView** ppView, bool bIsDefaultView) = 0;

#ifdef DILIGENT_DEBUG
    TBuffViewObjAllocator& m_dbgBuffViewAllocator;
#endif

    RESOURCE_STATE m_State = RESOURCE_STATE_UNKNOWN;

    /// Default UAV addressing the entire buffer
    std::unique_ptr<BufferViewImplType, STDDeleter<BufferViewImplType, TBuffViewObjAllocator>> m_pDefaultUAV;

    /// Default SRV addressing the entire buffer
    std::unique_ptr<BufferViewImplType, STDDeleter<BufferViewImplType, TBuffViewObjAllocator>> m_pDefaultSRV;
};

} // namespace Diligent
