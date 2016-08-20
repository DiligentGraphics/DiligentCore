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
/// Definition of the Diligent::IShaderD3D11 interface

#include "Shader.h"

namespace Diligent
{

// {C513E83E-B037-405B-8B49-BF8F5C220DEE}
static const Diligent::INTERFACE_ID IID_ShaderD3D11 =
{ 0xc513e83e, 0xb037, 0x405b, { 0x8b, 0x49, 0xbf, 0x8f, 0x5c, 0x22, 0xd, 0xee } };

/// Interface to the shader object implemented in D3D11
class IShaderD3D11 : public Diligent::IShader
{
public:

    /// Returns a pointer to the ID3D11DeviceChild interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11DeviceChild* GetD3D11Shader() = 0;
};

}
