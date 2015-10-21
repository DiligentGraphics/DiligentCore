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
/// Implementation of the Diligent::ShaderBase template class

#include "Shader.h"
#include "DeviceObjectBase.h"

namespace Diligent
{

/// Base implementation of a shader variable
struct ShaderVariableBase : public ObjectBase<IShaderVariable>
{
    ShaderVariableBase(IShader *pShader) : 
        // Shader variables are always created as part of the shader,
        // so we must provide owner pointer to the base class constructor
        ObjectBase<IShaderVariable>(pShader),
        m_pShader(pShader)
    {}

    virtual IShader* GetShader()override
    {
        return m_pShader;
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_ShaderVariable, ObjectBase<IShaderVariable> )

protected:
    IShader *m_pShader;
};

/// Implementation of a dummy shader variable that silently ignores all operations
struct DummyShaderVariable : ShaderVariableBase
{
    DummyShaderVariable(IShader *pShader) :
        ShaderVariableBase(pShader)
    {}

    virtual void Set( IDeviceObject *pObject )override
    {
        // Ignore operation
        // Probably output warning
    }
};

/// Template class implementing base functionality for a shader object
template<class BaseInterface = IShader, class RenderDeviceBaseInterface = IRenderDevice>
class ShaderBase : public DeviceObjectBase<BaseInterface, ShaderDesc>
{
public:
    typedef DeviceObjectBase<BaseInterface, ShaderDesc> TDeviceObjectBase;

	/// \param pDevice - pointer to the device.
	/// \param ShdrDesc - shader description.
	/// \param bIsDeviceInternal - flag indicating if the shader is an internal device object and 
	///							   must not keep a strong reference to the device.
    ShaderBase( IRenderDevice *pDevice, const ShaderDesc& ShdrDesc, bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( pDevice, ShdrDesc, nullptr, bIsDeviceInternal ),
        m_DummyShaderVar(this)
    {}

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_Shader, TDeviceObjectBase )
    
protected:
    DummyShaderVariable m_DummyShaderVar; ///< Dummy shader variable
};

}
