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
/// Declaration of Diligent::TextureBaseD3D11 class

#include "TextureD3D11.h"
#include "RenderDeviceD3D11.h"
#include "TextureBase.h"
#include "TextureViewD3D11Impl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

enum class D3D11TextureState
{
    Undefined       = 0x0,
    ShaderResource  = 0x1,
    RenderTarget    = 0x2,
    DepthStencil    = 0x4,
    UnorderedAccess = 0x8,
    Output = RenderTarget | DepthStencil | UnorderedAccess
};

/// Base implementation of the Diligent::ITextureD3D11 interface
class TextureBaseD3D11 : public TextureBase<ITextureD3D11, TextureViewD3D11Impl, FixedBlockMemoryAllocator, FixedBlockMemoryAllocator>
{
public:
    typedef TextureBase<ITextureD3D11, TextureViewD3D11Impl, FixedBlockMemoryAllocator, FixedBlockMemoryAllocator> TTextureBase;

    TextureBaseD3D11(FixedBlockMemoryAllocator &TexObjAllocator, 
                     FixedBlockMemoryAllocator &TexViewObjAllocator, 
                     class RenderDeviceD3D11Impl *pDeviceD3D11, 
                     const TextureDesc& TexDesc, 
                     const TextureData &InitData = TextureData());
    ~TextureBaseD3D11();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

    virtual void UpdateData( IDeviceContext *pContext, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData )override final;

    //virtual void CopyData(CTexture *pSrcTexture, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size);
    virtual void Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )override final;
    virtual void Unmap( IDeviceContext *pContext, MAP_TYPE MapType )override final;

    virtual ID3D11Resource* GetD3D11Texture()override final{ return m_pd3d11Texture; }

    void CopyData(IDeviceContext *pContext, 
                          ITexture *pSrcTexture, 
                          Uint32 SrcMipLevel,
                          Uint32 SrcSlice,
                          const Box *pSrcBox,
                          Uint32 DstMipLevel,
                          Uint32 DstSlice,
                          Uint32 DstX,
                          Uint32 DstY,
                          Uint32 DstZ);

    void ResetState(D3D11TextureState State){m_State = static_cast<Uint32>(State);}
    void AddState(D3D11TextureState State){m_State |= static_cast<Uint32>(State);}
    void ClearState(D3D11TextureState State){m_State &= ~static_cast<Uint32>(State);}
    bool CheckState(D3D11TextureState State){return (m_State & static_cast<Uint32>(State)) ? true : false;}

protected:
    void CreateViewInternal( const struct TextureViewDesc &ViewDesc, ITextureView **ppView, bool bIsDefaultView )override final;
    void PrepareD3D11InitData(const TextureData &InitData, Uint32 NumSubresources, 
                              std::vector<D3D11_SUBRESOURCE_DATA, STDAllocatorRawMem<D3D11_SUBRESOURCE_DATA> > &D3D11InitData);

    virtual void CreateSRV( TextureViewDesc &SRVDesc, ID3D11ShaderResourceView  **ppD3D11SRV ) = 0;
    virtual void CreateRTV( TextureViewDesc &RTVDesc, ID3D11RenderTargetView    **ppD3D11RTV ) = 0;
    virtual void CreateDSV( TextureViewDesc &DSVDesc, ID3D11DepthStencilView    **ppD3D11DSV ) = 0;
    virtual void CreateUAV( TextureViewDesc &UAVDesc, ID3D11UnorderedAccessView **ppD3D11UAV ) = 0;

    friend class RenderDeviceD3D11Impl;
    /// D3D11 texture
    CComPtr<ID3D11Resource> m_pd3d11Texture;

    Uint32 m_State = static_cast<Uint32>(D3D11TextureState::Undefined);
};

}
