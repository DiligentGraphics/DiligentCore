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
/// Declaration of Diligent::VertexDescD3D11Impl class

#include "VertexDescriptionD3D11.h"
#include "RenderDeviceD3D11.h"
#include "VertexDescriptionBase.h"

namespace Diligent
{

/// Implementation of the Diligent::IVertexDescriptionD3D11 interface
class VertexDescD3D11Impl : public VertexDescriptionBase<IVertexDescriptionD3D11>
{
public:
    typedef VertexDescriptionBase<IVertexDescriptionD3D11> TVertexDescriptionBase;

    VertexDescD3D11Impl( class RenderDeviceD3D11Impl *pRenderDeviceD3D11, const LayoutDesc &LayoutDesc, IShader *pVertexShader );
    ~VertexDescD3D11Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface );

    virtual ID3D11InputLayout* GetD3D11InputLayout(){ return m_pd3d11InputLayout; }

private:
    friend class DeviceContextD3D11Impl;
    /// D3D11 input layout
    Diligent::CComPtr<ID3D11InputLayout> m_pd3d11InputLayout;
};

}
