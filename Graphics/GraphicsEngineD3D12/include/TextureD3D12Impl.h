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

#pragma once

/// \file
/// Declaration of Diligent::TextureD3D12Impl class

#include "TextureD3D12.h"
#include "RenderDeviceD3D12.h"
#include "TextureBase.h"
#include "TextureViewD3D12Impl.h"
#include "D3D12ResourceBase.h"
#include "RenderDeviceD3D12Impl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Base implementation of the Diligent::ITextureD3D12 interface
class TextureD3D12Impl final : public TextureBase<ITextureD3D12, RenderDeviceD3D12Impl, TextureViewD3D12Impl, FixedBlockMemoryAllocator>, public D3D12ResourceBase
{
public:
    using TTextureBase = TextureBase<ITextureD3D12, RenderDeviceD3D12Impl, TextureViewD3D12Impl, FixedBlockMemoryAllocator>;

    // Creates a new D3D12 resource
    TextureD3D12Impl(IReferenceCounters*            pRefCounters,
                     FixedBlockMemoryAllocator&     TexViewObjAllocator,
                     RenderDeviceD3D12Impl*         pDeviceD3D12, 
                     const TextureDesc&             TexDesc, 
                     const TextureData&             InitData = TextureData());
    // Attaches to an existing D3D12 resource
    TextureD3D12Impl(IReferenceCounters*            pRefCounters,
                     FixedBlockMemoryAllocator&     TexViewObjAllocator,
                     class RenderDeviceD3D12Impl*   pDeviceD3D12, 
                     const TextureDesc&             TexDesc, 
                     ID3D12Resource*                pTexture);
    ~TextureD3D12Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

    virtual void UpdateData( IDeviceContext *pContext, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData )override final;

    //virtual void CopyData(CTexture *pSrcTexture, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size);
    virtual void Map( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags, MappedTextureSubresource &MappedData )override final;
    virtual void Unmap( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags )override final;

    virtual ID3D12Resource* GetD3D12Texture(){ return GetD3D12Resource(); }

    virtual void* GetNativeHandle()override final { return GetD3D12Texture(); }

    virtual void SetD3D12ResourceState(D3D12_RESOURCE_STATES state)override final{ SetState(state); }

    void CopyData(IDeviceContext* pContext, 
                  ITexture*       pSrcTexture, 
                  Uint32          SrcMipLevel,
                  Uint32          SrcSlice,
                  const Box*      pSrcBox,
                  Uint32          DstMipLevel,
                  Uint32          DstSlice,
                  Uint32          DstX,
                  Uint32          DstY,
                  Uint32          DstZ);
    
    D3D12_CPU_DESCRIPTOR_HANDLE GetMipLevelUAV(Uint32 Mip)
    {
        return m_MipUAVs.GetCpuHandle(Mip);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetTexArraySRV()
    {
        return m_TexArraySRV.GetCpuHandle();
    }

protected:
    void CreateViewInternal( const struct TextureViewDesc &ViewDesc, ITextureView **ppView, bool bIsDefaultView )override final;
    //void PrepareD3D12InitData(const TextureData &InitData, Uint32 NumSubresources, std::vector<D3D12_SUBRESOURCE_DATA> &D3D12InitData);

    void CreateSRV( TextureViewDesc &SRVDesc, D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle );
    void CreateRTV( TextureViewDesc &RTVDesc, D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle );
    void CreateDSV( TextureViewDesc &DSVDesc, D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle );
    void CreateUAV( TextureViewDesc &UAVDesc, D3D12_CPU_DESCRIPTOR_HANDLE UAVHandle );

    // UAVs for every mip level to facilitate mipmap generation
    DescriptorHeapAllocation m_MipUAVs;
    // SRV as texture array (even for a non-array texture) required for mipmap generation
    DescriptorHeapAllocation m_TexArraySRV;

    friend class RenderDeviceD3D12Impl;
};

}
