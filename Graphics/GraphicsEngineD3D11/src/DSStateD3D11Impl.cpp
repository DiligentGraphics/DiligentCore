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

#include "pch.h"
#include "DSStateD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"

namespace Diligent
{

D3D11_DEPTH_STENCILOP_DESC StencilOpDesc2D3D11StencilOpDesc(const StencilOpDesc &StOpDesc)
{
    D3D11_DEPTH_STENCILOP_DESC D3D11StOpDesc;
    D3D11StOpDesc.StencilFailOp      = StencilOpToD3D11StencilOp( StOpDesc.StencilFailOp );
    D3D11StOpDesc.StencilDepthFailOp = StencilOpToD3D11StencilOp( StOpDesc.StencilDepthFailOp );
    D3D11StOpDesc.StencilPassOp      = StencilOpToD3D11StencilOp( StOpDesc.StencilPassOp );
    D3D11StOpDesc.StencilFunc        = ComparisonFuncToD3D11ComparisonFunc( StOpDesc.StencilFunc );
    return D3D11StOpDesc;
}

DSStateD3D11Impl::DSStateD3D11Impl(class RenderDeviceD3D11Impl *pRenderDeviceD3D11, const DepthStencilStateDesc& DepthStencilStateDesc) : 
    TDepthStencilStateBase(pRenderDeviceD3D11, DepthStencilStateDesc)
{
    D3D11_DEPTH_STENCIL_DESC D3D11DSSDesc;
    D3D11DSSDesc.DepthEnable       = DepthStencilStateDesc.DepthEnable ? TRUE : FALSE;
    D3D11DSSDesc.DepthWriteMask    = DepthStencilStateDesc.DepthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    D3D11DSSDesc.DepthFunc         = ComparisonFuncToD3D11ComparisonFunc( DepthStencilStateDesc.DepthFunc );
    D3D11DSSDesc.StencilEnable     = DepthStencilStateDesc.StencilEnable ? TRUE : FALSE;
    D3D11DSSDesc.StencilReadMask   = DepthStencilStateDesc.StencilReadMask;
    D3D11DSSDesc.StencilWriteMask  = DepthStencilStateDesc.StencilWriteMask;
    D3D11DSSDesc.FrontFace         = StencilOpDesc2D3D11StencilOpDesc( DepthStencilStateDesc.FrontFace );
    D3D11DSSDesc.BackFace          = StencilOpDesc2D3D11StencilOpDesc( DepthStencilStateDesc.BackFace );

    auto *pDeviceD3D11 = pRenderDeviceD3D11->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateDepthStencilState( &D3D11DSSDesc, &m_pd3d11DepthStencilState ),
                            "Failed to create D3D11 depth stencil state" );
}

DSStateD3D11Impl::~DSStateD3D11Impl()
{

}

IMPLEMENT_QUERY_INTERFACE( DSStateD3D11Impl, IID_DepthStencilStateD3D11, TDepthStencilStateBase )

}
