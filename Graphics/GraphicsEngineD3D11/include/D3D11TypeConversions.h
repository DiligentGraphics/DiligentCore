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
/// Type conversion routines

#include "GraphicsTypes.h"
#include "DXGITypeConversions.h"

namespace Diligent
{

inline UINT BindFlagsToD3D11BindFlags(Uint32 BindFlags)
{
    UINT D3D11BindFlags = 0;
    D3D11BindFlags = D3D11BindFlags | ((BindFlags & BIND_VERTEX_BUFFER)     ? D3D11_BIND_VERTEX_BUFFER    : 0);
    D3D11BindFlags = D3D11BindFlags | ((BindFlags & BIND_INDEX_BUFFER)      ? D3D11_BIND_INDEX_BUFFER     : 0);
    D3D11BindFlags = D3D11BindFlags | ((BindFlags & BIND_UNIFORM_BUFFER)    ? D3D11_BIND_CONSTANT_BUFFER  : 0);
    D3D11BindFlags = D3D11BindFlags | ((BindFlags & BIND_SHADER_RESOURCE)   ? D3D11_BIND_SHADER_RESOURCE  : 0);
    D3D11BindFlags = D3D11BindFlags | ((BindFlags & BIND_STREAM_OUTPUT)     ? D3D11_BIND_STREAM_OUTPUT    : 0);
    D3D11BindFlags = D3D11BindFlags | ((BindFlags & BIND_RENDER_TARGET)     ? D3D11_BIND_RENDER_TARGET    : 0);
    D3D11BindFlags = D3D11BindFlags | ((BindFlags & BIND_DEPTH_STENCIL)     ? D3D11_BIND_DEPTH_STENCIL    : 0);
    D3D11BindFlags = D3D11BindFlags | ((BindFlags & BIND_UNORDERED_ACCESS)  ? D3D11_BIND_UNORDERED_ACCESS : 0);
    return D3D11BindFlags;
}

D3D11_PRIMITIVE_TOPOLOGY TopologyToD3D11Topology(PRIMITIVE_TOPOLOGY Topology);

inline D3D11_USAGE UsageToD3D11Usage(USAGE Usage)
{
    switch(Usage)
    {
        case USAGE_STATIC:       return D3D11_USAGE_IMMUTABLE;
        case USAGE_DEFAULT:      return D3D11_USAGE_DEFAULT;
        case USAGE_DYNAMIC:      return D3D11_USAGE_DYNAMIC;
        case USAGE_CPU_ACCESSIBLE: return D3D11_USAGE_STAGING;
        default: UNEXPECTED("Unknow usage" ); return D3D11_USAGE_DEFAULT;
    }
}

inline D3D11_MAP MapTypeToD3D11MapType(MAP_TYPE MapType)
{ 
    switch(MapType)
    {
        case MAP_READ:              return D3D11_MAP_READ;
        case MAP_WRITE:             return D3D11_MAP_WRITE;
        case MAP_READ_WRITE:        return D3D11_MAP_READ_WRITE;
        case MAP_WRITE_DISCARD:     return D3D11_MAP_WRITE_DISCARD;
        case MAP_WRITE_NO_OVERWRITE:return D3D11_MAP_WRITE_NO_OVERWRITE;
        default: UNEXPECTED( "Unknown map type" ); return D3D11_MAP_READ;
    }
}

inline UINT MapFlagsToD3D11MapFlags(Uint32 MapFlags)
{
    UINT D3D11MapFlags = 0;
    D3D11MapFlags |= (MapFlags & MAP_FLAG_DO_NOT_WAIT) ? D3D11_MAP_FLAG_DO_NOT_WAIT : 0;
    return D3D11MapFlags;
}

inline UINT CPUAccessFlagsToD3D11CPUAccessFlags(Uint32 Flags)
{
    UINT D3D11CPUAccessFlags = 0;
    D3D11CPUAccessFlags |= (Flags & CPU_ACCESS_READ) ? D3D11_CPU_ACCESS_READ : 0;
    D3D11CPUAccessFlags |= (Flags & CPU_ACCESS_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0;
    return D3D11CPUAccessFlags;
}

inline UINT MiscTextureFlagsToD3D11Flags(Uint32 Flags)
{
    UINT D3D11MiscFlags = 0;
    D3D11MiscFlags |= (Flags & MISC_TEXTURE_FLAG_GENERATE_MIPS) ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
    return D3D11MiscFlags;
}

D3D11_FILTER FilterTypeToD3D11Filter(FILTER_TYPE MinFilter, FILTER_TYPE MagFilter, FILTER_TYPE MipFilter);
D3D11_TEXTURE_ADDRESS_MODE TexAddressModeToD3D11AddressMode(TEXTURE_ADDRESS_MODE Mode);

D3D11_COMPARISON_FUNC ComparisonFuncToD3D11ComparisonFunc(COMPARISON_FUNCTION Func);
void DepthStencilStateDesc_To_D3D11_DEPTH_STENCIL_DESC(const DepthStencilStateDesc &DepthStencilDesc, D3D11_DEPTH_STENCIL_DESC &d3d11DSSDesc);
void RasterizerStateDesc_To_D3D11_RASTERIZER_DESC(const RasterizerStateDesc &RasterizerDesc, D3D11_RASTERIZER_DESC &d3d11RSDesc);
void BlendStateDesc_To_D3D11_BLEND_DESC(const BlendStateDesc &BSDesc, D3D11_BLEND_DESC &D3D11BSDesc);
void LayoutElements_To_D3D11_INPUT_ELEMENT_DESCs(const std::vector<LayoutElement, STDAllocatorRawMem<LayoutElement> > &LayoutElements, 
                                                 std::vector<D3D11_INPUT_ELEMENT_DESC, STDAllocatorRawMem<D3D11_INPUT_ELEMENT_DESC> > &D3D11InputElements);

void TextureViewDesc_to_D3D11_SRV_DESC(const TextureViewDesc& TexViewDesc, D3D11_SHADER_RESOURCE_VIEW_DESC &D3D11SRVDesc,  Uint32 SampleCount);
void TextureViewDesc_to_D3D11_RTV_DESC(const TextureViewDesc& TexViewDesc, D3D11_RENDER_TARGET_VIEW_DESC &D3D11RTVDesc,    Uint32 SampleCount);
void TextureViewDesc_to_D3D11_DSV_DESC(const TextureViewDesc& TexViewDesc, D3D11_DEPTH_STENCIL_VIEW_DESC &D3D11DSVDesc,    Uint32 SampleCount);
void TextureViewDesc_to_D3D11_UAV_DESC(const TextureViewDesc& TexViewDesc, D3D11_UNORDERED_ACCESS_VIEW_DESC &D3D11UAVDesc);

void BufferViewDesc_to_D3D11_SRV_DESC(const BufferDesc &BuffDesc, const BufferViewDesc& SRVDesc, D3D11_SHADER_RESOURCE_VIEW_DESC &D3D11SRVDesc);
void BufferViewDesc_to_D3D11_UAV_DESC(const BufferDesc &BuffDesc, const BufferViewDesc& UAVDesc, D3D11_UNORDERED_ACCESS_VIEW_DESC &D3D11UAVDesc);

}
