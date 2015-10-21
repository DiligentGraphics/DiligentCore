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
/// Implementation of the Diligent::BlendStateBase template class

#include "BlendState.h"
#include "DeviceObjectBase.h"

namespace Diligent
{

/// Template class implementing base functionality for a blend state object.

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::IBlendStateD3D11 or Diligent::IBlendStateGL).
/// \tparam RenderDeviceBaseInterface - base interface for the render device
///                                     (Diligent::IRenderDeviceD3D11, Diligent::IRenderDeviceGL,
///                                      or Diligent::IRenderDeviceGLES).
template<class BaseInterface = IBlendState, class RenderDeviceBaseInterface = IRenderDevice>
class BlendStateBase : public DeviceObjectBase<BaseInterface, BlendStateDesc>
{
public:
    typedef DeviceObjectBase<BaseInterface, BlendStateDesc> TDeviceObjectBase;
    typedef RenderDeviceBase < RenderDeviceBaseInterface > TRenderDeviceBase;

	/// \param pDevice - pointer to the device.
	/// \param BSDesc - blend state description.
	/// \param bIsDeviceInternal - flag indicating if the blend state is an internal device object and 
	///							   must not keep a strong reference to the device.
    BlendStateBase( IRenderDevice *pDevice, const BlendStateDesc& BSDesc, bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( pDevice, BSDesc, nullptr, bIsDeviceInternal )
    {
    }

    ~BlendStateBase()
    {
        /// \note Destructor cannot directly remove the object from the registry as this may cause a  
        ///       deadlock at the point where StateObjectsRegistry::Find() locks the weak pointer: if we
        ///       are in dtor, the object is locked by Diligent::RefCountedObject::Release() and 
        ///       StateObjectsRegistry::Find() will wait for that lock to be released.
        ///       A the same time this thread will be waiting for the other thread to unlock the registry.\n
        ///       Thus destructor only notifies the registry that there is a deleted object.
        ///       The reference to the object will be removed later.
        auto &BlendStateRegistry = static_cast<TRenderDeviceBase*>(this->GetDevice())->GetBSRegistry();
        // StateObjectsRegistry::ReportDeletedObject() does not lock the registry, but only 
        // atomically increments the outstanding deleted objects counter.
        BlendStateRegistry.ReportDeletedObject();
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_BlendState, TDeviceObjectBase )
};

}
