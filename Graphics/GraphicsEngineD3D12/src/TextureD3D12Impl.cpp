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
#include "TextureD3D12Impl.h"
#include "RenderDeviceD3D12Impl.h"
#include "DeviceContextD3D12Impl.h"
#include "D3D12TypeConversions.h"
#include "TextureViewD3D12Impl.h"
#include "DXGITypeConversions.h"
#include "d3dx12_win.h"
#include "EngineMemory.h"
#include "StringTools.h"

using namespace Diligent;

namespace Diligent
{

DXGI_FORMAT GetClearFormat(DXGI_FORMAT Fmt, D3D12_RESOURCE_FLAGS Flags)
{
    if( Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL )
    {
        switch (Fmt)
        {
            case DXGI_FORMAT_R32_TYPELESS:      return DXGI_FORMAT_D32_FLOAT;
            case DXGI_FORMAT_R16_TYPELESS:      return DXGI_FORMAT_D16_UNORM;
            case DXGI_FORMAT_R24G8_TYPELESS:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case DXGI_FORMAT_R32G8X24_TYPELESS: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        }
    }
    else if (Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        switch (Fmt)
        {
            case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case DXGI_FORMAT_R32G32B32_TYPELESS:    return DXGI_FORMAT_R32G32B32_FLOAT;  
            case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case DXGI_FORMAT_R32G32_TYPELESS:       return DXGI_FORMAT_R32G32_FLOAT;     
            case DXGI_FORMAT_R10G10B10A2_TYPELESS:  return DXGI_FORMAT_R10G10B10A2_UNORM;
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:     return DXGI_FORMAT_R8G8B8A8_UNORM;   
            case DXGI_FORMAT_R16G16_TYPELESS:       return DXGI_FORMAT_R16G16_FLOAT;     
            case DXGI_FORMAT_R32_TYPELESS:          return DXGI_FORMAT_R32_FLOAT;        
            case DXGI_FORMAT_R8G8_TYPELESS:         return DXGI_FORMAT_R8G8_UNORM;       
            case DXGI_FORMAT_R16_TYPELESS:          return DXGI_FORMAT_R16_FLOAT;        
            case DXGI_FORMAT_R8_TYPELESS:           return DXGI_FORMAT_R8_UNORM;         
            case DXGI_FORMAT_B8G8R8A8_TYPELESS:     return DXGI_FORMAT_B8G8R8A8_UNORM;   
            case DXGI_FORMAT_B8G8R8X8_TYPELESS:     return DXGI_FORMAT_B8G8R8X8_UNORM;   
        }
    }
    return Fmt;
}

TextureD3D12Impl :: TextureD3D12Impl(IReferenceCounters*        pRefCounters, 
                                     FixedBlockMemoryAllocator& TexViewObjAllocator,
                                     RenderDeviceD3D12Impl*     pRenderDeviceD3D12, 
                                     const TextureDesc&         TexDesc, 
                                     const TextureData&         InitData /*= TextureData()*/) : 
    TTextureBase(pRefCounters, TexViewObjAllocator, pRenderDeviceD3D12, TexDesc)
{
    if( m_Desc.Usage == USAGE_STATIC && InitData.pSubResources == nullptr )
        LOG_ERROR_AND_THROW("Static Texture must be initialized with data at creation time");

	D3D12_RESOURCE_DESC Desc = {};
	Desc.Alignment = 0;
    if(m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY || m_Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY || m_Desc.Type == RESOURCE_DIM_TEX_CUBE || m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
	    Desc.DepthOrArraySize = (UINT16)m_Desc.ArraySize;
    else if(m_Desc.Type == RESOURCE_DIM_TEX_3D )
        Desc.DepthOrArraySize = (UINT16)m_Desc.Depth;
    else
        Desc.DepthOrArraySize = 1;

    if( m_Desc.Type == RESOURCE_DIM_TEX_1D || m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY )
	    Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    else if( m_Desc.Type == RESOURCE_DIM_TEX_2D || m_Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY || m_Desc.Type == RESOURCE_DIM_TEX_CUBE || m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
	    Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    else if( m_Desc.Type == RESOURCE_DIM_TEX_3D )
        Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    else
    {
        LOG_ERROR_AND_THROW("Unknown texture type");
    }


    Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    if( m_Desc.BindFlags & BIND_RENDER_TARGET )
        Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if( m_Desc.BindFlags & BIND_DEPTH_STENCIL )
        Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if( (m_Desc.BindFlags & BIND_UNORDERED_ACCESS) || (m_Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS) )
        Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if( (m_Desc.BindFlags & BIND_SHADER_RESOURCE) == 0 )
        Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

    auto Format = TexFormatToDXGI_Format(m_Desc.Format, m_Desc.BindFlags);
    if (Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB && (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        Desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    else
        Desc.Format = Format;
	Desc.Height = (UINT)m_Desc.Height;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	Desc.MipLevels = static_cast<Uint16>(m_Desc.MipLevels);
	Desc.SampleDesc.Count = m_Desc.SampleCount;
	Desc.SampleDesc.Quality = 0;
	Desc.Width = (UINT64)m_Desc.Width;


	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

    auto *pd3d12Device = pRenderDeviceD3D12->GetD3D12Device();
    D3D12_CLEAR_VALUE ClearValue;
    D3D12_CLEAR_VALUE *pClearValue = nullptr;
    if( Desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) )
    {
        if(m_Desc.ClearValue.Format != TEX_FORMAT_UNKNOWN)
            ClearValue.Format = TexFormatToDXGI_Format(m_Desc.ClearValue.Format);
        else
            ClearValue.Format = GetClearFormat(Format, Desc.Flags);

        if (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        {
            for(int i=0; i < 4; ++i)
                ClearValue.Color[i] = m_Desc.ClearValue.Color[i];
        }
        else if(Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        {
            ClearValue.DepthStencil.Depth = m_Desc.ClearValue.DepthStencil.Depth;
            ClearValue.DepthStencil.Stencil = m_Desc.ClearValue.DepthStencil.Stencil;
        }
        pClearValue = &ClearValue;
    }

    bool bInitializeTexture = (InitData.pSubResources != nullptr && InitData.NumSubresources > 0);
    auto InitialState = bInitializeTexture ? RESOURCE_STATE_COPY_DEST : RESOURCE_STATE_UNDEFINED;
    SetState(InitialState);
    auto D3D12State = ResourceStateFlagsToD3D12ResourceStates(InitialState);
    auto hr = pd3d12Device->CreateCommittedResource( &HeapProps, D3D12_HEAP_FLAG_NONE,
		&Desc, D3D12State, pClearValue, __uuidof(m_pd3d12Resource), reinterpret_cast<void**>(static_cast<ID3D12Resource**>(&m_pd3d12Resource)) );
    if(FAILED(hr))
        LOG_ERROR_AND_THROW("Failed to create D3D12 texture");

    if( *m_Desc.Name != 0)
        m_pd3d12Resource->SetName(WidenString(m_Desc.Name).c_str());

    if(bInitializeTexture)
    {
        Uint32 ExpectedNumSubresources = static_cast<Uint32>(Desc.MipLevels * (Desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : Desc.DepthOrArraySize) );
        if( InitData.NumSubresources != ExpectedNumSubresources )
            LOG_ERROR_AND_THROW("Incorrect number of subresources in init data. ", ExpectedNumSubresources, " expected, while ", InitData.NumSubresources, " provided");

	    UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_pd3d12Resource, 0, InitData.NumSubresources);

        D3D12_HEAP_PROPERTIES UploadHeapProps;
	    UploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	    UploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	    UploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	    UploadHeapProps.CreationNodeMask = 1;
	    UploadHeapProps.VisibleNodeMask = 1;

	    D3D12_RESOURCE_DESC BufferDesc;
	    BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	    BufferDesc.Alignment = 0;
	    BufferDesc.Width = uploadBufferSize;
	    BufferDesc.Height = 1;
	    BufferDesc.DepthOrArraySize = 1;
	    BufferDesc.MipLevels = 1;
	    BufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	    BufferDesc.SampleDesc.Count = 1;
	    BufferDesc.SampleDesc.Quality = 0;
	    BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	    BufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        CComPtr<ID3D12Resource> UploadBuffer;
	    hr = pd3d12Device->CreateCommittedResource( &UploadHeapProps, D3D12_HEAP_FLAG_NONE,
		    &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		    nullptr,  __uuidof(UploadBuffer), reinterpret_cast<void**>(static_cast<ID3D12Resource**>(&UploadBuffer)));
        if(FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create committed resource in an upload heap");

        auto InitContext = pRenderDeviceD3D12->AllocateCommandContext();
	    // copy data to the intermediate upload heap and then schedule a copy from the upload heap to the default texture
        VERIFY_EXPR(CheckState(RESOURCE_STATE_COPY_DEST));
        std::vector<D3D12_SUBRESOURCE_DATA, STDAllocatorRawMem<D3D12_SUBRESOURCE_DATA> > D3D12SubResData(InitData.NumSubresources, D3D12_SUBRESOURCE_DATA(), STD_ALLOCATOR_RAW_MEM(D3D12_SUBRESOURCE_DATA, GetRawAllocator(), "Allocator for vector<D3D12_SUBRESOURCE_DATA>") );
        for(size_t subres=0; subres < D3D12SubResData.size(); ++subres)
        {
            D3D12SubResData[subres].pData = InitData.pSubResources[subres].pData;
            D3D12SubResData[subres].RowPitch = InitData.pSubResources[subres].Stride;
            D3D12SubResData[subres].SlicePitch = InitData.pSubResources[subres].DepthStride;
        }
	    auto UploadedSize = UpdateSubresources(InitContext->GetCommandList(), m_pd3d12Resource, UploadBuffer, 0, 0, InitData.NumSubresources, D3D12SubResData.data());
        VERIFY(UploadedSize == uploadBufferSize, "Incorrect uploaded data size (", UploadedSize, "). ", uploadBufferSize, " is expected");

        // Command list fence should only be signaled when submitting cmd list
        // from the immediate context, otherwise the basic requirement will be violated
        // as in the scenario below
        // See http://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-resource-lifetimes/
        //                                                           
        //  Signaled Fence  |        Immediate Context               |            InitContext            |
        //                  |                                        |                                   |
        //    N             |  Draw(ResourceX)                       |                                   |
        //                  |  Release(ResourceX)                    |                                   |
        //                  |   - (ResourceX, N) -> Release Queue    |                                   |
        //                  |                                        | CopyResource()                    |
        //   N+1            |                                        | CloseAndExecuteCommandContext()   |
        //                  |                                        |                                   |
        //   N+2            |  CloseAndExecuteCommandContext()       |                                   |
        //                  |   - Cmd list is submitted with number  |                                   |
        //                  |     N+1, but resource it references    |                                   |
        //                  |     was added to the delete queue      |                                   |
        //                  |     with value N                       |                                   |
        Uint32 QueueIndex = 0;
	    pRenderDeviceD3D12->CloseAndExecuteTransientCommandContext(QueueIndex, std::move(InitContext));

        // We MUST NOT call TransitionResource() from here, because
        // it will call AddRef() and potentially Release(), while 
        // the object is not constructed yet
        // Add reference to the object to the release queue to keep it alive
        // until copy operation is complete.  This must be done after
        // submitting command list for execution!
        pRenderDeviceD3D12->SafeReleaseDeviceObject(std::move(UploadBuffer), Uint64{1} << QueueIndex);
    }

    if(m_Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS)
    {
        if (m_Desc.Type != RESOURCE_DIM_TEX_2D && m_Desc.Type != RESOURCE_DIM_TEX_2D_ARRAY)
        {
            LOG_ERROR_AND_THROW("Mipmap generation is only supported for 2D textures and texture arrays");
        }

        m_MipUAVs = pRenderDeviceD3D12->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_Desc.MipLevels);
        for(Uint32 MipLevel = 0; MipLevel < m_Desc.MipLevels; ++MipLevel)
        {
            TextureViewDesc UAVDesc;
            // Always create texture array UAV
            UAVDesc.TextureDim = RESOURCE_DIM_TEX_2D_ARRAY;
            UAVDesc.ViewType = TEXTURE_VIEW_UNORDERED_ACCESS;
            UAVDesc.FirstArraySlice = 0;
            UAVDesc.NumArraySlices = m_Desc.ArraySize;
            UAVDesc.MostDetailedMip = MipLevel;
            if (m_Desc.Format == TEX_FORMAT_RGBA8_UNORM_SRGB)
                UAVDesc.Format = TEX_FORMAT_RGBA8_UNORM;
            CreateUAV( UAVDesc, m_MipUAVs.GetCpuHandle(MipLevel) );
        }

        {
            m_TexArraySRV = pRenderDeviceD3D12->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
            TextureViewDesc TexArraySRVDesc;
            // Create texture array SRV
            TexArraySRVDesc.TextureDim = RESOURCE_DIM_TEX_2D_ARRAY;
            TexArraySRVDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
            TexArraySRVDesc.FirstArraySlice = 0;
            TexArraySRVDesc.NumArraySlices = m_Desc.ArraySize;
            TexArraySRVDesc.MostDetailedMip = 0;
            TexArraySRVDesc.NumMipLevels = m_Desc.MipLevels;
            CreateSRV( TexArraySRVDesc, m_TexArraySRV.GetCpuHandle() );
        }
    }
}


static TextureDesc InitTexDescFromD3D12Resource(ID3D12Resource* pTexture, const TextureDesc& SrcTexDesc)
{
    auto ResourceDesc = pTexture->GetDesc();

    TextureDesc TexDesc = SrcTexDesc;
    if (TexDesc.Format == TEX_FORMAT_UNKNOWN)
        TexDesc.Format = DXGI_FormatToTexFormat(ResourceDesc.Format);
    auto RefDXGIFormat = TexFormatToDXGI_Format(TexDesc.Format);
    if( RefDXGIFormat != TexDesc.Format)
        LOG_ERROR_AND_THROW("Incorrect texture format (", GetTextureFormatAttribs(TexDesc.Format).Name, ")");

    TexDesc.Width = static_cast<Uint32>( ResourceDesc.Width );
    TexDesc.Height = Uint32{ ResourceDesc.Height };
    TexDesc.ArraySize = Uint32{ ResourceDesc.DepthOrArraySize };
    TexDesc.MipLevels = Uint32{ ResourceDesc.MipLevels };
    switch(ResourceDesc.Dimension)
    {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D: TexDesc.Type = TexDesc.ArraySize == 1 ? RESOURCE_DIM_TEX_1D : RESOURCE_DIM_TEX_1D_ARRAY; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D: TexDesc.Type = TexDesc.ArraySize == 1 ? RESOURCE_DIM_TEX_2D : RESOURCE_DIM_TEX_2D_ARRAY; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D: TexDesc.Type = RESOURCE_DIM_TEX_3D; break;
    }
         
    TexDesc.SampleCount = ResourceDesc.SampleDesc.Count;
    
    TexDesc.Usage = USAGE_DEFAULT;
    TexDesc.BindFlags = 0;
    if( (ResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 )
        TexDesc.BindFlags |= BIND_RENDER_TARGET;
    if( (ResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 )
        TexDesc.BindFlags |= BIND_DEPTH_STENCIL;
    if( (ResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0 )
        TexDesc.BindFlags |= BIND_UNORDERED_ACCESS;
    if ((ResourceDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0)
    {
        auto FormatAttribs = GetTextureFormatAttribs(TexDesc.Format);
        if (FormatAttribs.IsTypeless ||
            (FormatAttribs.ComponentType != COMPONENT_TYPE_DEPTH &&
             FormatAttribs.ComponentType != COMPONENT_TYPE_DEPTH_STENCIL) )
        {
            TexDesc.BindFlags |= BIND_SHADER_RESOURCE;
        }
    }

    return TexDesc;
}

TextureD3D12Impl::TextureD3D12Impl(IReferenceCounters*        pRefCounters,
                                   FixedBlockMemoryAllocator& TexViewObjAllocator,
                                   RenderDeviceD3D12Impl*     pDeviceD3D12, 
                                   const TextureDesc&         TexDesc, 
                                   RESOURCE_STATE             InitialState,
                                   ID3D12Resource*            pTexture) : 
    TTextureBase(pRefCounters, TexViewObjAllocator, pDeviceD3D12, InitTexDescFromD3D12Resource(pTexture, TexDesc))
{
    m_pd3d12Resource = pTexture;
    SetState(InitialState);
}
IMPLEMENT_QUERY_INTERFACE( TextureD3D12Impl, IID_TextureD3D12, TTextureBase )

void TextureD3D12Impl::CreateViewInternal( const struct TextureViewDesc &ViewDesc, ITextureView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "View pointer address is null" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );
    
    *ppView = nullptr;

    try
    {
        auto *pDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>(GetDevice());
        auto &TexViewAllocator = pDeviceD3D12Impl->GetTexViewObjAllocator();
        VERIFY( &TexViewAllocator == &m_dbgTexViewObjAllocator, "Texture view allocator does not match allocator provided during texture initialization" );

        auto UpdatedViewDesc = ViewDesc;
        CorrectTextureViewDesc( UpdatedViewDesc );

        DescriptorHeapAllocation ViewHandleAlloc;
        switch( ViewDesc.ViewType )
        {
            case TEXTURE_VIEW_SHADER_RESOURCE:
            {
                VERIFY( m_Desc.BindFlags & BIND_SHADER_RESOURCE, "BIND_SHADER_RESOURCE flag is not set" );
                ViewHandleAlloc = pDeviceD3D12Impl->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                CreateSRV( UpdatedViewDesc, ViewHandleAlloc.GetCpuHandle() );
            }
            break;

            case TEXTURE_VIEW_RENDER_TARGET:
            {
                VERIFY( m_Desc.BindFlags & BIND_RENDER_TARGET, "BIND_RENDER_TARGET flag is not set" );
                ViewHandleAlloc = pDeviceD3D12Impl->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                CreateRTV( UpdatedViewDesc, ViewHandleAlloc.GetCpuHandle() );
            }
            break;

            case TEXTURE_VIEW_DEPTH_STENCIL:
            {
                VERIFY( m_Desc.BindFlags & BIND_DEPTH_STENCIL, "BIND_DEPTH_STENCIL is not set" );
                ViewHandleAlloc = pDeviceD3D12Impl->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
                CreateDSV( UpdatedViewDesc, ViewHandleAlloc.GetCpuHandle() );
            }
            break;

            case TEXTURE_VIEW_UNORDERED_ACCESS:
            {
                VERIFY( m_Desc.BindFlags & BIND_UNORDERED_ACCESS, "BIND_UNORDERED_ACCESS flag is not set" );
                ViewHandleAlloc = pDeviceD3D12Impl->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                CreateUAV( UpdatedViewDesc, ViewHandleAlloc.GetCpuHandle() );
            }
            break;

            default: UNEXPECTED( "Unknown view type" ); break;
        }

        auto pViewD3D12 = NEW_RC_OBJ(TexViewAllocator, "TextureViewD3D12Impl instance", TextureViewD3D12Impl, bIsDefaultView ? this : nullptr)
                                    (GetDevice(), UpdatedViewDesc, this, std::move(ViewHandleAlloc), bIsDefaultView );
        VERIFY( pViewD3D12->GetDesc().ViewType == ViewDesc.ViewType, "Incorrect view type" );

        if( bIsDefaultView )
            *ppView = pViewD3D12;
        else
            pViewD3D12->QueryInterface(IID_TextureView, reinterpret_cast<IObject**>(ppView) );
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetTexViewTypeLiteralName(ViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", ViewDesc.Name ? ViewDesc.Name : "", "\" (", ViewTypeName, ") for texture \"", m_Desc.Name ? m_Desc.Name : "", "\"" );
    }
}

TextureD3D12Impl :: ~TextureD3D12Impl()
{
    // D3D12 object can only be destroyed when it is no longer used by the GPU
    auto *pDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>(GetDevice());
    pDeviceD3D12Impl->SafeReleaseDeviceObject(std::move(m_pd3d12Resource), m_Desc.CommandQueueMask);
}

void TextureD3D12Impl::CreateSRV( TextureViewDesc& SRVDesc, D3D12_CPU_DESCRIPTOR_HANDLE SRVHandle )
{
    VERIFY( SRVDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE, "Incorrect view type: shader resource is expected" );
    
    if( SRVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        SRVDesc.Format = m_Desc.Format;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC D3D12_SRVDesc;
    TextureViewDesc_to_D3D12_SRV_DESC(SRVDesc, D3D12_SRVDesc, m_Desc.SampleCount);

    auto *pDeviceD3D12 = static_cast<RenderDeviceD3D12Impl*>(GetDevice())->GetD3D12Device();
    pDeviceD3D12->CreateShaderResourceView(m_pd3d12Resource, &D3D12_SRVDesc, SRVHandle);
}

void TextureD3D12Impl::CreateRTV( TextureViewDesc& RTVDesc, D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle )
{
    VERIFY( RTVDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET, "Incorrect view type: render target is expected" );

    if( RTVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        RTVDesc.Format = m_Desc.Format;
    }

    D3D12_RENDER_TARGET_VIEW_DESC D3D12_RTVDesc;
    TextureViewDesc_to_D3D12_RTV_DESC(RTVDesc, D3D12_RTVDesc, m_Desc.SampleCount);

    auto *pDeviceD3D12 = static_cast<RenderDeviceD3D12Impl*>(GetDevice())->GetD3D12Device();
    pDeviceD3D12->CreateRenderTargetView( m_pd3d12Resource, &D3D12_RTVDesc, RTVHandle );
}

void TextureD3D12Impl::CreateDSV( TextureViewDesc& DSVDesc, D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle )
{
    VERIFY( DSVDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Incorrect view type: depth stencil is expected" );

    if( DSVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        DSVDesc.Format = m_Desc.Format;
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC D3D12_DSVDesc;
    TextureViewDesc_to_D3D12_DSV_DESC(DSVDesc, D3D12_DSVDesc, m_Desc.SampleCount);

    auto *pDeviceD3D12 = static_cast<RenderDeviceD3D12Impl*>(GetDevice())->GetD3D12Device();
    pDeviceD3D12->CreateDepthStencilView( m_pd3d12Resource, &D3D12_DSVDesc, DSVHandle );
}

void TextureD3D12Impl::CreateUAV( TextureViewDesc& UAVDesc, D3D12_CPU_DESCRIPTOR_HANDLE UAVHandle )
{
    VERIFY( UAVDesc.ViewType == TEXTURE_VIEW_UNORDERED_ACCESS, "Incorrect view type: unordered access is expected" );
    
    if( UAVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        UAVDesc.Format = m_Desc.Format;
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC D3D12_UAVDesc;
    TextureViewDesc_to_D3D12_UAV_DESC(UAVDesc, D3D12_UAVDesc);

    auto *pDeviceD3D12 = static_cast<RenderDeviceD3D12Impl*>(GetDevice())->GetD3D12Device();
    pDeviceD3D12->CreateUnorderedAccessView( m_pd3d12Resource, nullptr, &D3D12_UAVDesc, UAVHandle );
}

void TextureD3D12Impl::SetD3D12ResourceState(D3D12_RESOURCE_STATES state)
{
    SetState(D3D12ResourceStatesToResourceStateFlags(state));
}

}
