/*     Copyright 2015-2017 Egor Yusov
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
/// Implementation of the Diligent::CommandListBase template class

#include "CommandList.h"
#include "DeviceObjectBase.h"
#include "RenderDeviceBase.h"

namespace Diligent
{

struct CommandListDesc : public DeviceObjectAttribs
{
};

/// Template class implementing base functionality for a command list object.

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::ICommandListD3D11 or Diligent::ICommandListD3D12).
/// \tparam CommandListObjAllocator - allocator that is used to allocate memory for command list object instances
template<class BaseInterface, class CommandListObjAllocator>
class CommandListBase : public DeviceObjectBase<BaseInterface, CommandListDesc, CommandListObjAllocator>
{
public:
    typedef DeviceObjectBase<BaseInterface, CommandListDesc, CommandListObjAllocator> TDeviceObjectBase;

    /// \param ObjAllocator - Allocator that was used to allocate memory for this instance of the command list object
	/// \param pDevice - pointer to the device.
	/// \param bIsDeviceInternal - flag indicating if the CommandList is an internal device object and 
	///							   must not keep a strong reference to the device.
    CommandListBase( CommandListObjAllocator &ObjAllocator, IRenderDevice *pDevice, bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( ObjAllocator, pDevice, CommandListDesc(), nullptr, bIsDeviceInternal )
    {}

    ~CommandListBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_CommandList, TDeviceObjectBase )
};

}
