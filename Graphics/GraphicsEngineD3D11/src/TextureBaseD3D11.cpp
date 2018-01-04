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
#include "TextureBaseD3D11.h"
#include "RenderDeviceD3D11Impl.h"
#include "DeviceContextD3D11Impl.h"
#include "D3D11TypeConversions.h"
#include "TextureViewD3D11Impl.h"
#include "EngineMemory.h"

namespace Diligent
{

TextureBaseD3D11 :: TextureBaseD3D11(IReferenceCounters *pRefCounters, FixedBlockMemoryAllocator &TexViewObjAllocator, RenderDeviceD3D11Impl *pRenderDeviceD3D11, const TextureDesc& TexDesc, const TextureData &InitData /*= TextureData()*/) : 
    TTextureBase(pRefCounters, TexViewObjAllocator, pRenderDeviceD3D11, TexDesc)
{
    if( TexDesc.Usage == USAGE_STATIC && InitData.pSubResources == nullptr )
        LOG_ERROR_AND_THROW("Static Texture must be initialized with data at creation time");
}

IMPLEMENT_QUERY_INTERFACE( TextureBaseD3D11, IID_TextureD3D11, TTextureBase )

void TextureBaseD3D11::CreateViewInternal( const struct TextureViewDesc &ViewDesc, ITextureView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "View pointer address is null" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );
    
    *ppView = nullptr;

    try
    {
        auto UpdatedViewDesc = ViewDesc;
        CorrectTextureViewDesc( UpdatedViewDesc );

        RefCntAutoPtr<ID3D11View> pD3D11View;
        switch( ViewDesc.ViewType )
        {
            case TEXTURE_VIEW_SHADER_RESOURCE:
            {
                VERIFY( m_Desc.BindFlags & BIND_SHADER_RESOURCE, "BIND_SHADER_RESOURCE flag is not set" );
                ID3D11ShaderResourceView *pSRV = nullptr;
                CreateSRV( UpdatedViewDesc, &pSRV );
                pD3D11View.Attach( pSRV );
            }
            break;

            case TEXTURE_VIEW_RENDER_TARGET:
            {
                VERIFY( m_Desc.BindFlags & BIND_RENDER_TARGET, "BIND_RENDER_TARGET flag is not set" );
                ID3D11RenderTargetView *pRTV = nullptr;
                CreateRTV( UpdatedViewDesc, &pRTV );
                pD3D11View.Attach( pRTV );
            }
            break;

            case TEXTURE_VIEW_DEPTH_STENCIL:
            {
                VERIFY( m_Desc.BindFlags & BIND_DEPTH_STENCIL, "BIND_DEPTH_STENCIL is not set" );
                ID3D11DepthStencilView *pDSV = nullptr;
                CreateDSV( UpdatedViewDesc, &pDSV );
                pD3D11View.Attach( pDSV );
            }
            break;

            case TEXTURE_VIEW_UNORDERED_ACCESS:
            {
                VERIFY( m_Desc.BindFlags & BIND_UNORDERED_ACCESS, "BIND_UNORDERED_ACCESS flag is not set" );
                ID3D11UnorderedAccessView *pUAV = nullptr;
                CreateUAV( UpdatedViewDesc, &pUAV );
                pD3D11View.Attach( pUAV );
            }
            break;

            default: UNEXPECTED( "Unknown view type" ); break;
        }

        auto *pDeviceD3D11Impl = ValidatedCast<RenderDeviceD3D11Impl>(GetDevice());
        auto &TexViewAllocator = pDeviceD3D11Impl->GetTexViewObjAllocator();
        VERIFY( &TexViewAllocator == &m_dbgTexViewObjAllocator, "Texture view allocator does not match allocator provided during texture initialization" );

        auto pViewD3D11 = NEW_RC_OBJ(TexViewAllocator, "TextureViewD3D11Impl instance", TextureViewD3D11Impl, bIsDefaultView ? this : nullptr)
                                    (pDeviceD3D11Impl, UpdatedViewDesc, this, pD3D11View, bIsDefaultView );
        VERIFY( pViewD3D11->GetDesc().ViewType == ViewDesc.ViewType, "Incorrect view type" );

        if( bIsDefaultView )
            *ppView = pViewD3D11;
        else
            pViewD3D11->QueryInterface(IID_TextureView, reinterpret_cast<IObject**>(ppView) );
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetTexViewTypeLiteralName(ViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", ViewDesc.Name ? ViewDesc.Name : "", "\" (", ViewTypeName, ") for texture \"", m_Desc.Name ? m_Desc.Name : "", "\"" );
    }
}

void TextureBaseD3D11 :: PrepareD3D11InitData(const TextureData &InitData, Uint32 NumSubresources, 
                                              std::vector<D3D11_SUBRESOURCE_DATA, STDAllocatorRawMem<D3D11_SUBRESOURCE_DATA> >  &D3D11InitData)
{
    if( InitData.pSubResources )
    {
        if( NumSubresources == InitData.NumSubresources )
        {
            D3D11InitData.resize(NumSubresources);
            for(UINT Subres=0; Subres < NumSubresources; ++Subres)
            {
                auto &CurrSubres = InitData.pSubResources[Subres];
                D3D11InitData[Subres].pSysMem = CurrSubres.pData;
                D3D11InitData[Subres].SysMemPitch = CurrSubres.Stride;
                D3D11InitData[Subres].SysMemSlicePitch = CurrSubres.DepthStride;
            }
        }
        else
            UNEXPECTED( "Incorrect number of subrsources" );
    }
}


TextureBaseD3D11 :: ~TextureBaseD3D11()
{
}

void TextureBaseD3D11::UpdateData( IDeviceContext *pContext, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData )
{
    TTextureBase::UpdateData( pContext, MipLevel, Slice, DstBox, SubresData );
    if (SubresData.pSrcBuffer != nullptr)
    {
        LOG_ERROR("D3D11 does not support updating texture subresource from a GPU buffer");
        return;
    }

    VERIFY( m_Desc.Usage == USAGE_DEFAULT, "Only default usage resiurces can be updated with UpdateData()" );
    
    auto *pd3d11DeviceContext = static_cast<DeviceContextD3D11Impl*>(pContext)->GetD3D11DeviceContext();

    D3D11_BOX D3D11Box;
    D3D11Box.left = DstBox.MinX;
    D3D11Box.right = DstBox.MaxX;
    D3D11Box.top = DstBox.MinY;
    D3D11Box.bottom = DstBox.MaxY;
    D3D11Box.front = DstBox.MinZ;
    D3D11Box.back = DstBox.MaxZ;
    auto SubresIndex = D3D11CalcSubresource(MipLevel, Slice, m_Desc.MipLevels);
    pd3d11DeviceContext->UpdateSubresource(m_pd3d11Texture, SubresIndex, &D3D11Box, SubresData.pData, SubresData.Stride, SubresData.DepthStride);
}

void TextureBaseD3D11 ::  CopyData(IDeviceContext *pContext, 
                                    ITexture *pSrcTexture, 
                                    Uint32 SrcMipLevel,
                                    Uint32 SrcSlice,
                                    const Box *pSrcBox,
                                    Uint32 DstMipLevel,
                                    Uint32 DstSlice,
                                    Uint32 DstX,
                                    Uint32 DstY,
                                    Uint32 DstZ)
{
    TTextureBase::CopyData( pContext, pSrcTexture, SrcMipLevel, SrcSlice, pSrcBox,
                            DstMipLevel, DstSlice, DstX, DstY, DstZ );

    auto *pd3d11DeviceContext = ValidatedCast<DeviceContextD3D11Impl>(pContext )->GetD3D11DeviceContext();
    auto *pSrTextureBaseD3D11 = ValidatedCast<TextureBaseD3D11>( pSrcTexture );
    
    D3D11_BOX D3D11SrcBox, *pD3D11SrcBox = nullptr;
    if( pSrcBox )
    {
        D3D11SrcBox.left    = pSrcBox->MinX;
        D3D11SrcBox.right   = pSrcBox->MaxX;
        D3D11SrcBox.top     = pSrcBox->MinY;
        D3D11SrcBox.bottom  = pSrcBox->MaxY;
        D3D11SrcBox.front   = pSrcBox->MinZ;
        D3D11SrcBox.back    = pSrcBox->MaxZ;
        pD3D11SrcBox = &D3D11SrcBox;
    }
    auto SrcSubRes = D3D11CalcSubresource(SrcMipLevel, SrcSlice, pSrTextureBaseD3D11->GetDesc().MipLevels);
    auto DstSubRes = D3D11CalcSubresource(DstMipLevel, DstSlice, m_Desc.MipLevels);
    pd3d11DeviceContext->CopySubresourceRegion(m_pd3d11Texture, DstSubRes, DstX, DstY, DstZ, pSrTextureBaseD3D11->GetD3D11Texture(), SrcSubRes, pD3D11SrcBox);
}

void TextureBaseD3D11 :: Map( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags, MappedTextureSubresource &MappedData )
{
    TTextureBase::Map( pContext, Subresource, MapType, MapFlags, MappedData );

    auto *pd3d11DeviceContext = static_cast<DeviceContextD3D11Impl*>(pContext)->GetD3D11DeviceContext();
    D3D11_MAP d3d11MapType = static_cast<D3D11_MAP>(0);
    UINT d3d11MapFlags = 0;
    MapParamsToD3D11MapParams(MapType, MapFlags, d3d11MapType, d3d11MapFlags);

    D3D11_MAPPED_SUBRESOURCE MappedTex;
    auto hr = pd3d11DeviceContext->Map(m_pd3d11Texture, Subresource, d3d11MapType, d3d11MapFlags, &MappedTex);
    if( FAILED(hr) )
    {
        VERIFY_EXPR( hr == DXGI_ERROR_WAS_STILL_DRAWING  );
        MappedData = MappedTextureSubresource();
    }
    else
    {
        MappedData.pData = MappedTex.pData;
        MappedData.Stride = MappedTex.RowPitch;
        MappedData.DepthStride = MappedTex.DepthPitch;
    }
}

void TextureBaseD3D11::Unmap( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags )
{
    TTextureBase::Unmap( pContext, Subresource, MapType, MapFlags );

    auto *pd3d11DeviceContext = static_cast<DeviceContextD3D11Impl*>(pContext)->GetD3D11DeviceContext();
    pd3d11DeviceContext->Unmap(m_pd3d11Texture, Subresource);
}

}
