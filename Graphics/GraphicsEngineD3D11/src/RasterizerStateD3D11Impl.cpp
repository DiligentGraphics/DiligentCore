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
#include "RasterizerStateD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"

namespace Diligent
{

D3D11_FILL_MODE FillMode2D3D11FillMode(FILL_MODE FillMode)
{
    static bool bIsInit = false;
    static D3D11_FILL_MODE D3D11FillModes[FILL_MODE_NUM_MODES] = {};
    if( !bIsInit )
    {
        D3D11FillModes[ FILL_MODE_WIREFRAME ] = D3D11_FILL_WIREFRAME;
        D3D11FillModes[ FILL_MODE_SOLID ]     = D3D11_FILL_SOLID;

        bIsInit = true;
    }
    if( FILL_MODE_UNDEFINED < FillMode && FillMode < FILL_MODE_NUM_MODES )
    {
        auto D3D11FillMode = D3D11FillModes[FillMode];
        VERIFY( D3D11FillMode != 0, "Incorrect fill mode" );
        return D3D11FillModes[FillMode];
    }
    else
    {
        UNEXPECTED( "Incorrect fill mode (", FillMode, ")" )
        return static_cast<D3D11_FILL_MODE>(0);
    }
}

D3D11_CULL_MODE CullMode2D3D11CullMode( CULL_MODE CullMode )
{
    static bool bIsInit = false;
    static D3D11_CULL_MODE D3D11CullModes[CULL_MODE_NUM_MODES] = {};
    if( !bIsInit )
    {
        D3D11CullModes[ CULL_MODE_NONE  ] = D3D11_CULL_NONE;
        D3D11CullModes[ CULL_MODE_FRONT ] = D3D11_CULL_FRONT;
        D3D11CullModes[ CULL_MODE_BACK  ] = D3D11_CULL_BACK;

        bIsInit = true;
    }

    if( CULL_MODE_UNDEFINED < CullMode && CullMode < CULL_MODE_NUM_MODES )
    {
        auto D3D11CullMode = D3D11CullModes[CullMode];
        VERIFY( D3D11CullMode != 0, "Incorrect cull mode" );
        return D3D11CullMode;
    }
    else
    {
        UNEXPECTED( "Incorrect cull mode (", CullMode, ")" )
        return static_cast<D3D11_CULL_MODE>(0);
    }
}

RasterizerStateD3D11Impl::RasterizerStateD3D11Impl(class RenderDeviceD3D11Impl *pRenderDeviceD3D11, 
                                                     const RasterizerStateDesc& RasterizerStateDesc) : 
    TRasterizerStateBase(pRenderDeviceD3D11, RasterizerStateDesc)
{
    D3D11_RASTERIZER_DESC D3D11RSDesc;

    D3D11RSDesc.FillMode = FillMode2D3D11FillMode(RasterizerStateDesc.FillMode);
    D3D11RSDesc.CullMode = CullMode2D3D11CullMode(RasterizerStateDesc.CullMode);
    D3D11RSDesc.FrontCounterClockwise = RasterizerStateDesc.FrontCounterClockwise ? TRUE : FALSE;
    D3D11RSDesc.DepthBias             = RasterizerStateDesc.DepthBias;
    D3D11RSDesc.DepthBiasClamp        = RasterizerStateDesc.DepthBiasClamp;  
    D3D11RSDesc.SlopeScaledDepthBias  = RasterizerStateDesc.SlopeScaledDepthBias;
    D3D11RSDesc.DepthClipEnable       = RasterizerStateDesc.DepthClipEnable       ? TRUE : FALSE;
    D3D11RSDesc.ScissorEnable         = RasterizerStateDesc.ScissorEnable         ? TRUE : FALSE;    
    D3D11RSDesc.AntialiasedLineEnable = RasterizerStateDesc.AntialiasedLineEnable ? TRUE : FALSE;
    D3D11RSDesc.MultisampleEnable     = D3D11RSDesc.AntialiasedLineEnable;

    auto *pDeviceD3D11 = pRenderDeviceD3D11->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateRasterizerState( &D3D11RSDesc, &m_pd3d11RasterizerState ),
                            "Failed to create D3D11 rasterizer state" );
}

RasterizerStateD3D11Impl::~RasterizerStateD3D11Impl()
{

}

IMPLEMENT_QUERY_INTERFACE( RasterizerStateD3D11Impl, IID_RasterizerStateD3D11, TRasterizerStateBase )

}
