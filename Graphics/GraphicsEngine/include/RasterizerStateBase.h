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
/// Implementation of the Diligent::RasterizerStateBase template class

#include "RasterizerState.h"
#include "DeviceObjectBase.h"

namespace Diligent
{

/// Template class implementing base functionality for a rasterizer object.

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::IRasterizerStateD3D11 or Diligent::IRasterizerStateGL).
/// \tparam RenderDeviceBaseInterface - base interface for the render device
///                                     (Diligent::IRenderDeviceD3D11, Diligent::IRenderDeviceGL,
///                                      or Diligent::IRenderDeviceGLES).
template<class BaseInterface = IRasterizerState, class RenderDeviceBaseInterface = IRenderDevice>
class RasterizerStateBase : public DeviceObjectBase<BaseInterface, RasterizerStateDesc>
{
public:
    typedef DeviceObjectBase<BaseInterface, RasterizerStateDesc> TDeviceObjectBase;
    typedef RenderDeviceBase < RenderDeviceBaseInterface > TRenderDeviceBase;

	/// \param pDevice - pointer to the device.
	/// \param RSDesc - rasterizer state description.
	/// \param bIsDeviceInternal - flag indicating if the state is an internal device object and 
	///							   must not keep a strong reference to the device.
    RasterizerStateBase( IRenderDevice *pDevice, const RasterizerStateDesc& RSDesc, bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( pDevice, RSDesc, nullptr, bIsDeviceInternal )
    {
    }

    ~RasterizerStateBase()
    {
        /// \note Destructor cannot directly remove the object from the registry as this may cause a 
        ///       deadlock. See BlendStateBase::~BlendStateBase() for details.
        auto &RasterizerStateRegistry = static_cast<TRenderDeviceBase*>(this->GetDevice())->GetRSRegistry();
        RasterizerStateRegistry.ReportDeletedObject();
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_RasterizerState, TDeviceObjectBase )
};

}
