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
#include "BlendStateD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"

namespace Diligent
{
 
D3D11_BLEND BlendFactorToD3D11Blend( BLEND_FACTOR bf )
{
    // Note that this code is safe for multithreaded environments since
    // bIsInit is set to true only AFTER the entire map is initialized.
    static bool bIsInit = false;
    static D3D11_BLEND D3D11Blend[BLEND_FACTOR_NUM_FACTORS] = {};
    if( !bIsInit )
    {
        // In a multithreaded environment, several threads can potentially enter
        // this block. This is not a problem since they will just initialize the 
        // memory with the same values more than once
        D3D11Blend[ BLEND_FACTOR_ZERO             ] =  D3D11_BLEND_ZERO;       
        D3D11Blend[ BLEND_FACTOR_ONE              ] =  D3D11_BLEND_ONE;           
        D3D11Blend[ BLEND_FACTOR_SRC_COLOR        ] =  D3D11_BLEND_SRC_COLOR;
        D3D11Blend[ BLEND_FACTOR_INV_SRC_COLOR    ] =  D3D11_BLEND_INV_SRC_COLOR;
        D3D11Blend[ BLEND_FACTOR_SRC_ALPHA        ] =  D3D11_BLEND_SRC_ALPHA;
        D3D11Blend[ BLEND_FACTOR_INV_SRC_ALPHA    ] =  D3D11_BLEND_INV_SRC_ALPHA;
        D3D11Blend[ BLEND_FACTOR_DEST_ALPHA       ] =  D3D11_BLEND_DEST_ALPHA;
        D3D11Blend[ BLEND_FACTOR_INV_DEST_ALPHA   ] =  D3D11_BLEND_INV_DEST_ALPHA;
        D3D11Blend[ BLEND_FACTOR_DEST_COLOR       ] =  D3D11_BLEND_DEST_COLOR;
        D3D11Blend[ BLEND_FACTOR_INV_DEST_COLOR   ] =  D3D11_BLEND_INV_DEST_COLOR;
        D3D11Blend[ BLEND_FACTOR_SRC_ALPHA_SAT    ] =  D3D11_BLEND_SRC_ALPHA_SAT;
        D3D11Blend[ BLEND_FACTOR_BLEND_FACTOR     ] =  D3D11_BLEND_BLEND_FACTOR;
        D3D11Blend[ BLEND_FACTOR_INV_BLEND_FACTOR ] =  D3D11_BLEND_INV_BLEND_FACTOR;
        D3D11Blend[ BLEND_FACTOR_SRC1_COLOR       ] =  D3D11_BLEND_SRC1_COLOR;
        D3D11Blend[ BLEND_FACTOR_INV_SRC1_COLOR   ] =  D3D11_BLEND_INV_SRC1_COLOR;
        D3D11Blend[ BLEND_FACTOR_SRC1_ALPHA       ] =  D3D11_BLEND_SRC1_ALPHA;
        D3D11Blend[ BLEND_FACTOR_INV_SRC1_ALPHA   ] =  D3D11_BLEND_INV_SRC1_ALPHA;

        bIsInit = true;
    }
    if( bf > BLEND_FACTOR_UNDEFINED && bf < BLEND_FACTOR_NUM_FACTORS )
    {
        auto d3dbf = D3D11Blend[bf];
        VERIFY( d3dbf != 0, "Incorrect blend factor" );
        return d3dbf;
    }
    else
    {
        UNEXPECTED("Incorrect blend factor (", bf, ")" )
        return static_cast<D3D11_BLEND>( 0 );
    }
}

D3D11_BLEND_OP BlendOperation2D3D11BlendOp( BLEND_OPERATION BlendOp )
{
    static bool bIsInit = false;
    static D3D11_BLEND_OP D3D11BlendOp[BLEND_OPERATION_NUM_OPERATIONS] = {};
    if( !bIsInit )
    {
        D3D11BlendOp[ BLEND_OPERATION_ADD          ] = D3D11_BLEND_OP_ADD;
        D3D11BlendOp[ BLEND_OPERATION_SUBTRACT     ] = D3D11_BLEND_OP_SUBTRACT;
        D3D11BlendOp[ BLEND_OPERATION_REV_SUBTRACT ] = D3D11_BLEND_OP_REV_SUBTRACT;
        D3D11BlendOp[ BLEND_OPERATION_MIN          ] = D3D11_BLEND_OP_MIN;
        D3D11BlendOp[ BLEND_OPERATION_MAX          ] = D3D11_BLEND_OP_MAX;

        bIsInit = true;
    }

    if( BlendOp > BLEND_OPERATION_UNDEFINED && BlendOp < BLEND_OPERATION_NUM_OPERATIONS )
    {
        auto d3dbop = D3D11BlendOp[BlendOp];
        VERIFY( d3dbop != 0, "Incorrect blend operation" );
        return d3dbop;
    }
    else
    {
        UNEXPECTED( "Incorrect blend operation (", BlendOp, ")" )
        return static_cast<D3D11_BLEND_OP>(0);
    }
}

BlendStateD3D11Impl::BlendStateD3D11Impl(class RenderDeviceD3D11Impl *pRenderDeviceD3D11, 
                                           const BlendStateDesc& BlendStateDesc) : 
    TBlendStateBase(pRenderDeviceD3D11, BlendStateDesc)
{
    D3D11_BLEND_DESC D3D11BSDesc;
    D3D11BSDesc.AlphaToCoverageEnable  = BlendStateDesc.AlphaToCoverageEnable ? TRUE : FALSE;
    D3D11BSDesc.IndependentBlendEnable = BlendStateDesc.IndependentBlendEnable ? TRUE : FALSE;
    VERIFY( BlendStateDesc.MaxRenderTargets >= 8, "Number of render targets is expected to be at least 8" );
    for( int i = 0; i < 8; ++i )
    {
        const auto& SrcRTDesc = BlendStateDesc.RenderTargets[i];
        auto &DstRTDesc = D3D11BSDesc.RenderTarget[i];
        DstRTDesc.BlendEnable = SrcRTDesc.BlendEnable ? TRUE : FALSE;

        DstRTDesc.SrcBlend  = BlendFactorToD3D11Blend(SrcRTDesc.SrcBlend);
        DstRTDesc.DestBlend = BlendFactorToD3D11Blend(SrcRTDesc.DestBlend);
        DstRTDesc.BlendOp   = BlendOperation2D3D11BlendOp(SrcRTDesc.BlendOp);

        DstRTDesc.SrcBlendAlpha  = BlendFactorToD3D11Blend(SrcRTDesc.SrcBlendAlpha);
        DstRTDesc.DestBlendAlpha = BlendFactorToD3D11Blend(SrcRTDesc.DestBlendAlpha);
        DstRTDesc.BlendOpAlpha   = BlendOperation2D3D11BlendOp(SrcRTDesc.BlendOpAlpha);

        DstRTDesc.RenderTargetWriteMask = 
            ((SrcRTDesc.RenderTargetWriteMask & COLOR_MASK_RED)   ? D3D11_COLOR_WRITE_ENABLE_RED   : 0) | 
            ((SrcRTDesc.RenderTargetWriteMask & COLOR_MASK_GREEN) ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0) |
            ((SrcRTDesc.RenderTargetWriteMask & COLOR_MASK_BLUE)  ? D3D11_COLOR_WRITE_ENABLE_BLUE  : 0) |
            ((SrcRTDesc.RenderTargetWriteMask & COLOR_MASK_ALPHA) ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0);
    }
    
    auto *pDeviceD3D11 = pRenderDeviceD3D11->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateBlendState( &D3D11BSDesc, &m_pd3d11BlendState ), 
                            "Failed to create D3D11 blend state object" );
}

BlendStateD3D11Impl::~BlendStateD3D11Impl()
{

}

IMPLEMENT_QUERY_INTERFACE( BlendStateD3D11Impl, IID_BlendStateD3D11, TBlendStateBase )

}
