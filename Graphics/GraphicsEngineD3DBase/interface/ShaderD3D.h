/*     Copyright 2019 Diligent Graphics LLC
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
/// Definition of the Diligent::IShaderD3D interface and related data structures

#include "../../GraphicsEngine/interface/Shader.h"

namespace Diligent
{

// {1EA0898C-1612-457F-B74E-808843D2CBE3}
static constexpr INTERFACE_ID IID_ShaderD3D =
{ 0x1ea0898c, 0x1612, 0x457f, { 0xb7, 0x4e, 0x80, 0x88, 0x43, 0xd2, 0xcb, 0xe3 } };


/// HLSL resource description
struct HLSLShaderResourceDesc : ShaderResourceDesc
{
    Uint32 ShaderRegister = 0;
};

/// Exposes Direct3D-specific functionality of a shader object.
class IShaderD3D : public IShader
{
public:
    /// Returns HLSL shader resource description
    virtual HLSLShaderResourceDesc GetHLSLResource(Uint32 Index)const = 0;
};

}
