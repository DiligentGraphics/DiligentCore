/*     Copyright 2015-2018 Egor Yusov
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
#include "D3D12TypeConversions.h"
#include "DXGITypeConversions.h"

#include "D3D12TypeDefinitions.h"
#include "D3DTypeConversionImpl.h"
#include "D3DViewDescConversionImpl.h"

namespace Diligent
{

D3D12_COMPARISON_FUNC ComparisonFuncToD3D12ComparisonFunc(COMPARISON_FUNCTION Func)
{
    return ComparisonFuncToD3DComparisonFunc<D3D12_COMPARISON_FUNC>(Func);
}

D3D12_FILTER FilterTypeToD3D12Filter(FILTER_TYPE MinFilter, FILTER_TYPE MagFilter, FILTER_TYPE MipFilter)
{
    return FilterTypeToD3DFilter<D3D12_FILTER>(MinFilter, MagFilter, MipFilter);
}

D3D12_TEXTURE_ADDRESS_MODE TexAddressModeToD3D12AddressMode(TEXTURE_ADDRESS_MODE Mode)
{
    return TexAddressModeToD3DAddressMode<D3D12_TEXTURE_ADDRESS_MODE>(Mode);
}

void DepthStencilStateDesc_To_D3D12_DEPTH_STENCIL_DESC(const DepthStencilStateDesc& DepthStencilDesc, D3D12_DEPTH_STENCIL_DESC& d3d12DSSDesc)
{
    DepthStencilStateDesc_To_D3D_DEPTH_STENCIL_DESC<D3D12_DEPTH_STENCIL_DESC, D3D12_DEPTH_STENCILOP_DESC, D3D12_STENCIL_OP, D3D12_COMPARISON_FUNC>(DepthStencilDesc, d3d12DSSDesc);
}

void RasterizerStateDesc_To_D3D12_RASTERIZER_DESC(const RasterizerStateDesc& RasterizerDesc, D3D12_RASTERIZER_DESC& d3d12RSDesc)
{
    RasterizerStateDesc_To_D3D_RASTERIZER_DESC<D3D12_RASTERIZER_DESC, D3D12_FILL_MODE, D3D12_CULL_MODE>(RasterizerDesc, d3d12RSDesc);

    // The sample count that is forced while UAV rendering or rasterizing. 
    // Valid values are 0, 1, 2, 4, 8, and optionally 16. 0 indicates that 
    // the sample count is not forced.
    d3d12RSDesc.ForcedSampleCount     = 0;

    d3d12RSDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
}



D3D12_LOGIC_OP LogicOperationToD3D12LogicOp( LOGIC_OPERATION lo )
{
    // Note that this code is safe for multithreaded environments since
    // bIsInit is set to true only AFTER the entire map is initialized.
    static bool bIsInit = false;
    static D3D12_LOGIC_OP D3D12LogicOp[LOGIC_OP_NUM_OPERATIONS] = {};
    if( !bIsInit )
    {
        // In a multithreaded environment, several threads can potentially enter
        // this block. This is not a problem since they will just initialize the 
        // memory with the same values more than once
        D3D12LogicOp[ D3D12_LOGIC_OP_CLEAR		    ]  = D3D12_LOGIC_OP_CLEAR;
        D3D12LogicOp[ D3D12_LOGIC_OP_SET			]  = D3D12_LOGIC_OP_SET;
        D3D12LogicOp[ D3D12_LOGIC_OP_COPY			]  = D3D12_LOGIC_OP_COPY;
        D3D12LogicOp[ D3D12_LOGIC_OP_COPY_INVERTED  ]  = D3D12_LOGIC_OP_COPY_INVERTED;
        D3D12LogicOp[ D3D12_LOGIC_OP_NOOP			]  = D3D12_LOGIC_OP_NOOP;
        D3D12LogicOp[ D3D12_LOGIC_OP_INVERT		    ]  = D3D12_LOGIC_OP_INVERT;
        D3D12LogicOp[ D3D12_LOGIC_OP_AND			]  = D3D12_LOGIC_OP_AND;
        D3D12LogicOp[ D3D12_LOGIC_OP_NAND			]  = D3D12_LOGIC_OP_NAND;
        D3D12LogicOp[ D3D12_LOGIC_OP_OR			    ]  = D3D12_LOGIC_OP_OR;
        D3D12LogicOp[ D3D12_LOGIC_OP_NOR			]  = D3D12_LOGIC_OP_NOR;
        D3D12LogicOp[ D3D12_LOGIC_OP_XOR			]  = D3D12_LOGIC_OP_XOR;
        D3D12LogicOp[ D3D12_LOGIC_OP_EQUIV		    ]  = D3D12_LOGIC_OP_EQUIV;
        D3D12LogicOp[ D3D12_LOGIC_OP_AND_REVERSE	]  = D3D12_LOGIC_OP_AND_REVERSE;
        D3D12LogicOp[ D3D12_LOGIC_OP_AND_INVERTED	]  = D3D12_LOGIC_OP_AND_INVERTED;
        D3D12LogicOp[ D3D12_LOGIC_OP_OR_REVERSE	    ]  = D3D12_LOGIC_OP_OR_REVERSE;
        D3D12LogicOp[ D3D12_LOGIC_OP_OR_INVERTED	]  = D3D12_LOGIC_OP_OR_INVERTED;
        
        bIsInit = true;
    }
    if( lo >= LOGIC_OP_CLEAR && lo < LOGIC_OP_NUM_OPERATIONS )
    {
        auto d3dlo = D3D12LogicOp[lo];
        return d3dlo;
    }
    else
    {
        UNEXPECTED("Incorrect blend factor (", lo, ")" );
        return static_cast<D3D12_LOGIC_OP>( 0 );
    }
}


void BlendStateDesc_To_D3D12_BLEND_DESC(const BlendStateDesc& BSDesc, D3D12_BLEND_DESC& d3d12BlendDesc)
{
    BlendStateDescToD3DBlendDesc<D3D12_BLEND_DESC, D3D12_BLEND, D3D12_BLEND_OP>(BSDesc, d3d12BlendDesc);

    for( int i = 0; i < 8; ++i )
    {
        const auto& SrcRTDesc = BSDesc.RenderTargets[i];
        auto &DstRTDesc = d3d12BlendDesc.RenderTarget[i];

        // The following members only present in D3D_RENDER_TARGET_BLEND_DESC
        DstRTDesc.LogicOpEnable = SrcRTDesc.LogicOperationEnable ? TRUE : FALSE;
        DstRTDesc.LogicOp = LogicOperationToD3D12LogicOp(SrcRTDesc.LogicOp);
    }
}

void LayoutElements_To_D3D12_INPUT_ELEMENT_DESCs(const std::vector<LayoutElement, STDAllocatorRawMem<LayoutElement>> &LayoutElements, 
                                                 std::vector<D3D12_INPUT_ELEMENT_DESC, STDAllocatorRawMem<D3D12_INPUT_ELEMENT_DESC> > &d3d12InputElements)
{
    LayoutElements_To_D3D_INPUT_ELEMENT_DESCs<D3D12_INPUT_ELEMENT_DESC>(LayoutElements, d3d12InputElements);
}

D3D12_PRIMITIVE_TOPOLOGY TopologyToD3D12Topology(PRIMITIVE_TOPOLOGY Topology)
{
    return TopologyToD3DTopology<D3D12_PRIMITIVE_TOPOLOGY>(Topology);
}



void TextureViewDesc_to_D3D12_SRV_DESC(const TextureViewDesc& SRVDesc, D3D12_SHADER_RESOURCE_VIEW_DESC &D3D12SRVDesc, Uint32 SampleCount)
{
    TextureViewDesc_to_D3D_SRV_DESC(SRVDesc, D3D12SRVDesc, SampleCount);
    D3D12SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    switch (SRVDesc.TextureDim)
    {
        case RESOURCE_DIM_TEX_1D:
            D3D12SRVDesc.Texture1D.ResourceMinLODClamp = 0;
        break;

        case RESOURCE_DIM_TEX_1D_ARRAY:
            D3D12SRVDesc.Texture1DArray.ResourceMinLODClamp = 0;
        break;

        case RESOURCE_DIM_TEX_2D:
            if( SampleCount > 1 )
            {
            }
            else
            {
                D3D12SRVDesc.Texture2D.PlaneSlice = 0;
                D3D12SRVDesc.Texture2D.ResourceMinLODClamp = 0;
            }
        break;

        case RESOURCE_DIM_TEX_2D_ARRAY:
            if( SampleCount > 1 )
            {
            }
            else
            {
                D3D12SRVDesc.Texture2DArray.PlaneSlice = 0;
                D3D12SRVDesc.Texture2DArray.ResourceMinLODClamp = 0;
            }
        break;

        case RESOURCE_DIM_TEX_3D:
            D3D12SRVDesc.Texture3D.ResourceMinLODClamp = 0;
        break;

        case RESOURCE_DIM_TEX_CUBE:
            D3D12SRVDesc.TextureCube.ResourceMinLODClamp = 0;
        break;

        case RESOURCE_DIM_TEX_CUBE_ARRAY:
            D3D12SRVDesc.TextureCubeArray.ResourceMinLODClamp = 0;
        break;

        default:
            UNEXPECTED( "Unexpected view type" );
    }
}

void TextureViewDesc_to_D3D12_RTV_DESC(const TextureViewDesc& RTVDesc, D3D12_RENDER_TARGET_VIEW_DESC& D3D12RTVDesc, Uint32 SampleCount)
{
    TextureViewDesc_to_D3D_RTV_DESC(RTVDesc, D3D12RTVDesc, SampleCount);
    switch (RTVDesc.TextureDim)
    {
        case RESOURCE_DIM_TEX_1D:
        break;

        case RESOURCE_DIM_TEX_1D_ARRAY:
        break;

        case RESOURCE_DIM_TEX_2D:
            if( SampleCount > 1 )
            {
            }
            else
            {
                D3D12RTVDesc.Texture2D.PlaneSlice = 0;
            }
        break;

        case RESOURCE_DIM_TEX_2D_ARRAY:
            if( SampleCount > 1 )
            {
            }
            else
            {
                D3D12RTVDesc.Texture2DArray.PlaneSlice = 0;
            }
        break;

        case RESOURCE_DIM_TEX_3D:
        break;

        default:
            UNEXPECTED( "Unexpected view type" );
    }
}

void TextureViewDesc_to_D3D12_DSV_DESC(const TextureViewDesc& DSVDesc, D3D12_DEPTH_STENCIL_VIEW_DESC& D3D12DSVDesc, Uint32 SampleCount)
{
    TextureViewDesc_to_D3D_DSV_DESC(DSVDesc, D3D12DSVDesc, SampleCount);
    D3D12DSVDesc.Flags = D3D12_DSV_FLAG_NONE;
}

void TextureViewDesc_to_D3D12_UAV_DESC(const TextureViewDesc& UAVDesc, D3D12_UNORDERED_ACCESS_VIEW_DESC& D3D12UAVDesc)
{
    TextureViewDesc_to_D3D_UAV_DESC(UAVDesc, D3D12UAVDesc);
    switch (UAVDesc.TextureDim)
    {
        case RESOURCE_DIM_TEX_1D:
        break;

        case RESOURCE_DIM_TEX_1D_ARRAY:
        break;

        case RESOURCE_DIM_TEX_2D:
            D3D12UAVDesc.Texture2D.PlaneSlice = 0;
        break;

        case RESOURCE_DIM_TEX_2D_ARRAY:
            D3D12UAVDesc.Texture2DArray.PlaneSlice = 0;
        break;

        case RESOURCE_DIM_TEX_3D:
        break;

        default:
            UNEXPECTED( "Unexpected view type" );
    }
}


void BufferViewDesc_to_D3D12_SRV_DESC(const BufferDesc &BuffDesc, const BufferViewDesc& SRVDesc, D3D12_SHADER_RESOURCE_VIEW_DESC& D3D12SRVDesc)
{
    BufferViewDesc_to_D3D_SRV_DESC(BuffDesc, SRVDesc, D3D12SRVDesc);
    D3D12SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    D3D12SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    VERIFY_EXPR(BuffDesc.BindFlags & BIND_SHADER_RESOURCE);
    if (BuffDesc.Mode == BUFFER_MODE_STRUCTURED)
        D3D12SRVDesc.Buffer.StructureByteStride = BuffDesc.ElementByteStride; 
}

void BufferViewDesc_to_D3D12_UAV_DESC(const BufferDesc& BuffDesc, const BufferViewDesc& UAVDesc, D3D12_UNORDERED_ACCESS_VIEW_DESC& D3D12UAVDesc)
{
    BufferViewDesc_to_D3D_UAV_DESC(BuffDesc, UAVDesc, D3D12UAVDesc);
    D3D12UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    VERIFY_EXPR(BuffDesc.BindFlags & BIND_UNORDERED_ACCESS);
    if (BuffDesc.Mode == BUFFER_MODE_STRUCTURED)
        D3D12UAVDesc.Buffer.StructureByteStride = BuffDesc.ElementByteStride; 
}

D3D12_STATIC_BORDER_COLOR BorderColorToD3D12StaticBorderColor(const Float32 BorderColor[])
{
    D3D12_STATIC_BORDER_COLOR StaticBorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    if(BorderColor[0] == 0 && BorderColor[1] == 0 && BorderColor[2] == 0 && BorderColor[3] == 0)
        StaticBorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    else if(BorderColor[0] == 0 && BorderColor[1] == 0 && BorderColor[2] == 0 && BorderColor[3] == 1)
        StaticBorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    else if(BorderColor[0] == 1 && BorderColor[1] == 1 && BorderColor[2] == 1 && BorderColor[3] == 1)
        StaticBorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    else
    {
        LOG_ERROR_MESSAGE("Static samplers only allow transparent black (0,0,0,0), opaque black (0,0,0,1) or opaque white (1,1,1,1) as border colors.");
    }
    return StaticBorderColor;
}

}
