/*     Copyright 2015-2016 Egor Yusov
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
/// Implementation of the Diligent::SamplerBase template class

#include "Sampler.h"
#include "DeviceObjectBase.h"
#include "RenderDeviceBase.h"

namespace Diligent
{

/// Template class implementing base functionality for a sampler object.

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::ISamplerD3D11, Diligent::ISamplerD3D12 or Diligent::ISamplerGL).
/// \tparam RenderDeviceBaseInterface - base interface for the render device
///                                     (Diligent::IRenderDeviceD3D11, Diligent::IRenderDeviceD3D12, Diligent::IRenderDeviceGL, or Diligent::IRenderDeviceGLES).
/// \tparam SamplerObjAllocator - type of the allocator that is used to allocate memory for the sampler object instances
template<class BaseInterface, class RenderDeviceBaseInterface, class SamplerObjAllocator>
class SamplerBase : public DeviceObjectBase<BaseInterface, SamplerDesc, SamplerObjAllocator>
{
public:
    typedef DeviceObjectBase<BaseInterface, SamplerDesc, SamplerObjAllocator> TDeviceObjectBase;
    typedef RenderDeviceBase<RenderDeviceBaseInterface> TRenderDeviceBase;

    /// \param ObjAllocator - allocator that was used to allocate memory for this instance of the sampler object
	/// \param pDevice - pointer to the device.
	/// \param SamDesc - sampler description.
	/// \param bIsDeviceInternal - flag indicating if the sampler is an internal device object and 
	///							   must not keep a strong reference to the device.
    SamplerBase( SamplerObjAllocator &ObjAllocator, IRenderDevice *pDevice, const SamplerDesc& SamDesc, bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( ObjAllocator, pDevice, SamDesc, nullptr, bIsDeviceInternal )
    {}

    ~SamplerBase()
    {
        /// \note Destructor cannot directly remove the object from the registry as this may cause a 
        ///       deadlock.
        auto &SamplerRegistry = static_cast<TRenderDeviceBase *>(this->GetDevice())->GetSamplerRegistry();
        SamplerRegistry.ReportDeletedObject();
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_Sampler, TDeviceObjectBase )
};

}
