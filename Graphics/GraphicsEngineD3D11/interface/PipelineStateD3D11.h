/*     Copyright 2015-2019 Egor Yusov
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
/// Definition of the Diligent::IPipeplineStateD3D11 interface

#include "../../GraphicsEngine/interface/PipelineState.h"

namespace Diligent
{

// {3EA6E3F4-9966-47FC-8CE8-0EB3E2273061}
static constexpr INTERFACE_ID IID_PipelineStateD3D11 = 
{ 0x3ea6e3f4, 0x9966, 0x47fc, { 0x8c, 0xe8, 0xe, 0xb3, 0xe2, 0x27, 0x30, 0x61 } };

/// Interface to the blend state object implemented in D3D11
class IPipelineStateD3D11 : public IPipelineState
{
public:

    /// Returns a pointer to the ID3D11BlendState interface of the internal Direct3D11 object.
    
    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11BlendState* GetD3D11BlendState() = 0;

    
    /// Returns a pointer to the ID3D11RasterizerState interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11RasterizerState* GetD3D11RasterizerState() = 0;


    /// Returns a pointer to the ID3D11DepthStencilState interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11DepthStencilState* GetD3D11DepthStencilState() = 0;

    /// Returns a pointer to the ID3D11InputLayout interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11InputLayout* GetD3D11InputLayout() = 0;

    /// Returns a pointer to the ID3D11VertexShader interface of the internal vertex shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11VertexShader* GetD3D11VertexShader() = 0;

    /// Returns a pointer to the ID3D11PixelShader interface of the internal pixel shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11PixelShader* GetD3D11PixelShader() = 0;


    /// Returns a pointer to the ID3D11GeometryShader interface of the internal geometry shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11GeometryShader* GetD3D11GeometryShader() = 0;
    
    /// Returns a pointer to the ID3D11DomainShader interface of the internal domain shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11DomainShader* GetD3D11DomainShader() = 0;

    /// Returns a pointer to the ID3D11HullShader interface of the internal hull shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11HullShader* GetD3D11HullShader() = 0;

    /// Returns a pointer to the ID3D11ComputeShader interface of the internal compute shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11ComputeShader* GetD3D11ComputeShader() = 0;
};

}
