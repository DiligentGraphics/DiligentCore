/*     Copyright 2015 Egor Yusov
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

#pragma once

/// \file
/// Implementation of the Diligent::BufferViewBase template class

#include "BufferView.h"
#include "DeviceObjectBase.h"
#include "GraphicsTypes.h"
#include "RefCntAutoPtr.h"

namespace Diligent
{

/// Template class implementing base functionality for a buffer view object

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::IBufferViewD3D11 or Diligent::IBufferViewGL).
template<class BaseInterface = IBufferView>
class BufferViewBase : public DeviceObjectBase<BaseInterface, BufferViewDesc>
{
public:
    typedef DeviceObjectBase<BaseInterface, BufferViewDesc> TDeviceObjectBase;

	/// \param pDevice - pointer to the render device.
	/// \param ViewDesc - buffer view description.
	/// \param pBuffer - pointer to the buffer that the view is to be created for.
	/// \param bIsDefaultView - flag indicating if the view is default view, and is thus
	///						    part of the buffer object. In this case the view will attach 
	///							to the buffer's reference counters.
    BufferViewBase( class IRenderDevice *pDevice,
                    const BufferViewDesc& ViewDesc, 
                    class IBuffer *pBuffer,
                    bool bIsDefaultView ) :
        // Default views are created as part of the buffer, so we cannot not keep strong 
        // reference to the buffer to avoid cyclic links. Instead, we will attach to the 
        // reference counters of the buffer.
        TDeviceObjectBase( pDevice, ViewDesc, bIsDefaultView ? pBuffer : nullptr ),
        m_pBuffer( pBuffer ),
        // For non-default view, we will keep strong reference to buffer
        m_spBuffer(bIsDefaultView ? nullptr : pBuffer)
    {}

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_BufferView, TDeviceObjectBase )

    virtual IBuffer* GetBuffer()
    {
        return m_pBuffer;
    }

protected:

    /// Reference to the buffer
    IBuffer* m_pBuffer;

    /// Strong reference to the buffer. Used for non-default views
    /// to keep the buffer alive
    Diligent::RefCntAutoPtr<IBuffer> m_spBuffer;
};

}
