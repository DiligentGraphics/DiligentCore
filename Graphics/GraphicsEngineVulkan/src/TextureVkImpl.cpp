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
#include "TextureVkImpl.h"
#include "RenderDeviceVkImpl.h"
#include "DeviceContextVkImpl.h"
#include "VulkanTypeConversions.h"
#include "TextureViewVkImpl.h"
#include "VulkanTypeConversions.h"
#include "EngineMemory.h"
#include "StringTools.h"

using namespace Diligent;

namespace Diligent
{

#if 0
DXGI_FORMAT GetClearFormat(DXGI_FORMAT Fmt, Vk_RESOURCE_FLAGS Flags)
{
    if( Flags & Vk_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL )
    {
        switch (Fmt)
        {
            case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_D32_FLOAT;
            case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_D16_UNORM;
            case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case DXGI_FORMAT_R32G8X24_TYPELESS: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        }
    }
    else if (Flags & Vk_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {

    }
    return Fmt;
}
#endif

TextureVkImpl :: TextureVkImpl(IReferenceCounters *pRefCounters, 
                                     FixedBlockMemoryAllocator &TexViewObjAllocator,
                                     RenderDeviceVkImpl *pRenderDeviceVk, 
                                     const TextureDesc& TexDesc, 
                                     const TextureData &InitData /*= TextureData()*/) : 
    TTextureBase(pRefCounters, TexViewObjAllocator, pRenderDeviceVk, TexDesc),
    m_IsExternalHandle(false)
{
    if( m_Desc.Usage == USAGE_STATIC && InitData.pSubResources == nullptr )
        LOG_ERROR_AND_THROW("Static Texture must be initialized with data at creation time");
#if 0
	Vk_RESOURCE_DESC Desc = {};
	Desc.Alignment = 0;
    if(m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY || m_Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY || m_Desc.Type == RESOURCE_DIM_TEX_CUBE || m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
	    Desc.DepthOrArraySize = (UINT16)m_Desc.ArraySize;
    else if(m_Desc.Type == RESOURCE_DIM_TEX_3D )
        Desc.DepthOrArraySize = (UINT16)m_Desc.Depth;
    else
        Desc.DepthOrArraySize = 1;

    if( m_Desc.Type == RESOURCE_DIM_TEX_1D || m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY )
	    Desc.Dimension = Vk_RESOURCE_DIMENSION_TEXTURE1D;
    else if( m_Desc.Type == RESOURCE_DIM_TEX_2D || m_Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY || m_Desc.Type == RESOURCE_DIM_TEX_CUBE || m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
	    Desc.Dimension = Vk_RESOURCE_DIMENSION_TEXTURE2D;
    else if( m_Desc.Type == RESOURCE_DIM_TEX_3D )
        Desc.Dimension = Vk_RESOURCE_DIMENSION_TEXTURE3D;
    else
    {
        LOG_ERROR_AND_THROW("Unknown texture type");
    }


    Desc.Flags = Vk_RESOURCE_FLAG_NONE;
    if( m_Desc.BindFlags & BIND_RENDER_TARGET )
        Desc.Flags |= Vk_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if( m_Desc.BindFlags & BIND_DEPTH_STENCIL )
        Desc.Flags |= Vk_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if( (m_Desc.BindFlags & BIND_UNORDERED_ACCESS) || (m_Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS) )
        Desc.Flags |= Vk_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if( (m_Desc.BindFlags & BIND_SHADER_RESOURCE) == 0 )
        Desc.Flags |= Vk_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

    auto Format = TexFormatToDXGI_Format(m_Desc.Format, m_Desc.BindFlags);
    if (Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB && (Desc.Flags & Vk_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        Desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    else
        Desc.Format = Format;
	Desc.Height = (UINT)m_Desc.Height;
	Desc.Layout = Vk_TEXTURE_LAYOUT_UNKNOWN;
	Desc.MipLevels = static_cast<Uint16>(m_Desc.MipLevels);
	Desc.SampleDesc.Count = m_Desc.SampleCount;
	Desc.SampleDesc.Quality = 0;
	Desc.Width = (UINT64)m_Desc.Width;


	Vk_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = Vk_HEAP_TYPE_DEFAULT;
	HeapProps.CPUPageProperty = Vk_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = Vk_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

    auto *pVkDevice = pRenderDeviceVk->GetVkDevice();
    Vk_CLEAR_VALUE ClearValue;
    Vk_CLEAR_VALUE *pClearValue = nullptr;
    if( Desc.Flags & (Vk_RESOURCE_FLAG_ALLOW_RENDER_TARGET | Vk_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) )
    {
        if(m_Desc.ClearValue.Format != TEX_FORMAT_UNKNOWN)
            ClearValue.Format = TexFormatToDXGI_Format(m_Desc.ClearValue.Format);
        else
            ClearValue.Format = GetClearFormat(Format, Desc.Flags);

        if (Desc.Flags & Vk_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        {
            for(int i=0; i < 4; ++i)
                ClearValue.Color[i] = m_Desc.ClearValue.Color[i];
        }
        else if(Desc.Flags & Vk_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        {
            ClearValue.DepthStencil.Depth = m_Desc.ClearValue.DepthStencil.Depth;
            ClearValue.DepthStencil.Stencil = m_Desc.ClearValue.DepthStencil.Stencil;
        }
        pClearValue = &ClearValue;
    }

    bool bInitializeTexture = (InitData.pSubResources != nullptr && InitData.NumSubresources > 0);
    if(bInitializeTexture)
        m_UsageState = Vk_RESOURCE_STATE_COPY_DEST;

    auto hr = pVkDevice->CreateCommittedResource( &HeapProps, Vk_HEAP_FLAG_NONE,
		&Desc, m_UsageState, pClearValue, __uuidof(m_pVkResource), reinterpret_cast<void**>(static_cast<IVkResource**>(&m_pVkResource)) );
    if(FAILED(hr))
        LOG_ERROR_AND_THROW("Failed to create Vk texture");

    if( *m_Desc.Name != 0)
        m_pVkResource->SetName(WidenString(m_Desc.Name).c_str());

    if(bInitializeTexture)
    {
        Uint32 ExpectedNumSubresources = static_cast<Uint32>(Desc.MipLevels * (Desc.Dimension == Vk_RESOURCE_DIMENSION_TEXTURE3D ? 1 : Desc.DepthOrArraySize) );
        if( InitData.NumSubresources != ExpectedNumSubresources )
            LOG_ERROR_AND_THROW("Incorrect number of subresources in init data. ", ExpectedNumSubresources, " expected, while ", InitData.NumSubresources, " provided");

	    UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_pVkResource, 0, InitData.NumSubresources);

        Vk_HEAP_PROPERTIES UploadHeapProps;
	    UploadHeapProps.Type = Vk_HEAP_TYPE_UPLOAD;
	    UploadHeapProps.CPUPageProperty = Vk_CPU_PAGE_PROPERTY_UNKNOWN;
	    UploadHeapProps.MemoryPoolPreference = Vk_MEMORY_POOL_UNKNOWN;
	    UploadHeapProps.CreationNodeMask = 1;
	    UploadHeapProps.VisibleNodeMask = 1;

	    Vk_RESOURCE_DESC BufferDesc;
	    BufferDesc.Dimension = Vk_RESOURCE_DIMENSION_BUFFER;
	    BufferDesc.Alignment = 0;
	    BufferDesc.Width = uploadBufferSize;
	    BufferDesc.Height = 1;
	    BufferDesc.DepthOrArraySize = 1;
	    BufferDesc.MipLevels = 1;
	    BufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	    BufferDesc.SampleDesc.Count = 1;
	    BufferDesc.SampleDesc.Quality = 0;
	    BufferDesc.Layout = Vk_TEXTURE_LAYOUT_ROW_MAJOR;
	    BufferDesc.Flags = Vk_RESOURCE_FLAG_NONE;

        CComPtr<IVkResource> UploadBuffer;
	    hr = pVkDevice->CreateCommittedResource( &UploadHeapProps, Vk_HEAP_FLAG_NONE,
		    &BufferDesc, Vk_RESOURCE_STATE_GENERIC_READ,
		    nullptr,  __uuidof(UploadBuffer), reinterpret_cast<void**>(static_cast<IVkResource**>(&UploadBuffer)));
        if(FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create committed resource in an upload heap");

        auto *pInitContext = pRenderDeviceVk->AllocateCommandContext();
	    // copy data to the intermediate upload heap and then schedule a copy from the upload heap to the default texture
        VERIFY_EXPR(m_UsageState == Vk_RESOURCE_STATE_COPY_DEST);
        std::vector<Vk_SUBRESOURCE_DATA, STDAllocatorRawMem<Vk_SUBRESOURCE_DATA> > VkSubResData(InitData.NumSubresources, Vk_SUBRESOURCE_DATA(), STD_ALLOCATOR_RAW_MEM(Vk_SUBRESOURCE_DATA, GetRawAllocator(), "Allocator for vector<Vk_SUBRESOURCE_DATA>") );
        for(size_t subres=0; subres < VkSubResData.size(); ++subres)
        {
            VkSubResData[subres].pData = InitData.pSubResources[subres].pData;
            VkSubResData[subres].RowPitch = InitData.pSubResources[subres].Stride;
            VkSubResData[subres].SlicePitch = InitData.pSubResources[subres].DepthStride;
        }
	    auto UploadedSize = UpdateSubresources(pInitContext->GetCommandList(), m_pVkResource, UploadBuffer, 0, 0, InitData.NumSubresources, VkSubResData.data());
        VERIFY(UploadedSize == uploadBufferSize, "Incorrect uploaded data size (", UploadedSize, "). ", uploadBufferSize, " is expected");

        // Command list fence should only be signaled when submitting cmd list
        // from the immediate context, otherwise the basic requirement will be violated
        // as in the scenario below
        // See http://diligentgraphics.com/diligent-engine/architecture/Vk/managing-resource-lifetimes/
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
	    pRenderDeviceVk->CloseAndExecuteCommandContext(pInitContext, false);

        // We MUST NOT call TransitionResource() from here, because
        // it will call AddRef() and potentially Release(), while 
        // the object is not constructed yet
        // Add reference to the object to the release queue to keep it alive
        // until copy operation is complete.  This must be done after
        // submitting command list for execution!
        pRenderDeviceVk->SafeReleaseVkObject(UploadBuffer);
    }

    if(m_Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS)
    {
        if (m_Desc.Type != RESOURCE_DIM_TEX_2D && m_Desc.Type != RESOURCE_DIM_TEX_2D_ARRAY)
        {
            LOG_ERROR_AND_THROW("Mipmap generation is only supported for 2D textures and texture arrays");
        }

        m_MipUAVs = pRenderDeviceVk->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_Desc.MipLevels);
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
            m_TexArraySRV = pRenderDeviceVk->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
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
#endif
}

static TextureDesc InitTexDescFromVkImage(VkImage vkImg, const TextureDesc& SrcTexDesc)
{
    // There is no way to query any image attribute in Vulkan
    return SrcTexDesc;
}


TextureVkImpl::TextureVkImpl(IReferenceCounters *pRefCounters,
                                   FixedBlockMemoryAllocator &TexViewObjAllocator,
                                   RenderDeviceVkImpl *pDeviceVk, 
                                   const TextureDesc& TexDesc,
                                   VkImage VkImageHandle) :
    TTextureBase(pRefCounters, TexViewObjAllocator, pDeviceVk, InitTexDescFromVkImage(VkImageHandle, TexDesc)),
    m_VkImage(VkImageHandle),
    m_IsExternalHandle(true)
{
}
IMPLEMENT_QUERY_INTERFACE( TextureVkImpl, IID_TextureVk, TTextureBase )

void TextureVkImpl::CreateViewInternal( const struct TextureViewDesc &ViewDesc, ITextureView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "View pointer address is null" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );
    
    *ppView = nullptr;
#if 0
    try
    {
        auto *pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(GetDevice());
        auto &TexViewAllocator = pDeviceVkImpl->GetTexViewObjAllocator();
        VERIFY( &TexViewAllocator == &m_dbgTexViewObjAllocator, "Texture view allocator does not match allocator provided during texture initialization" );

        auto UpdatedViewDesc = ViewDesc;
        CorrectTextureViewDesc( UpdatedViewDesc );

        DescriptorHeapAllocation ViewHandleAlloc;
        switch( ViewDesc.ViewType )
        {
            case TEXTURE_VIEW_SHADER_RESOURCE:
            {
                VERIFY( m_Desc.BindFlags & BIND_SHADER_RESOURCE, "BIND_SHADER_RESOURCE flag is not set" );
                ViewHandleAlloc = pDeviceVkImpl->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                CreateSRV( UpdatedViewDesc, ViewHandleAlloc.GetCpuHandle() );
            }
            break;

            case TEXTURE_VIEW_RENDER_TARGET:
            {
                VERIFY( m_Desc.BindFlags & BIND_RENDER_TARGET, "BIND_RENDER_TARGET flag is not set" );
                ViewHandleAlloc = pDeviceVkImpl->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_RTV);
                CreateRTV( UpdatedViewDesc, ViewHandleAlloc.GetCpuHandle() );
            }
            break;

            case TEXTURE_VIEW_DEPTH_STENCIL:
            {
                VERIFY( m_Desc.BindFlags & BIND_DEPTH_STENCIL, "BIND_DEPTH_STENCIL is not set" );
                ViewHandleAlloc = pDeviceVkImpl->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_DSV);
                CreateDSV( UpdatedViewDesc, ViewHandleAlloc.GetCpuHandle() );
            }
            break;

            case TEXTURE_VIEW_UNORDERED_ACCESS:
            {
                VERIFY( m_Desc.BindFlags & BIND_UNORDERED_ACCESS, "BIND_UNORDERED_ACCESS flag is not set" );
                ViewHandleAlloc = pDeviceVkImpl->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                CreateUAV( UpdatedViewDesc, ViewHandleAlloc.GetCpuHandle() );
            }
            break;

            default: UNEXPECTED( "Unknown view type" ); break;
        }

        auto pViewVk = NEW_RC_OBJ(TexViewAllocator, "TextureViewVkImpl instance", TextureViewVkImpl, bIsDefaultView ? this : nullptr)
                                    (GetDevice(), UpdatedViewDesc, this, std::move(ViewHandleAlloc), bIsDefaultView );
        VERIFY( pViewVk->GetDesc().ViewType == ViewDesc.ViewType, "Incorrect view type" );

        if( bIsDefaultView )
            *ppView = pViewVk;
        else
            pViewVk->QueryInterface(IID_TextureView, reinterpret_cast<IObject**>(ppView) );
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetTexViewTypeLiteralName(ViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", ViewDesc.Name ? ViewDesc.Name : "", "\" (", ViewTypeName, ") for texture \"", m_Desc.Name ? m_Desc.Name : "", "\"" );
    }
#endif
}

TextureVkImpl :: ~TextureVkImpl()
{
    if(!m_IsExternalHandle)
    {

    }
#if 0
    // Vk object can only be destroyed when it is no longer used by the GPU
    auto *pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(GetDevice());
    pDeviceVkImpl->SafeReleaseVkObject(m_pVkResource);
#endif
}

void TextureVkImpl::UpdateData( IDeviceContext *pContext, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData )
{
    TTextureBase::UpdateData( pContext, MipLevel, Slice, DstBox, SubresData );
    if (SubresData.pSrcBuffer == nullptr)
    {
        LOG_ERROR("Vk does not allow updating texture subresource from CPU memory");
        return;
    }

    VERIFY( m_Desc.Usage == USAGE_DEFAULT, "Only default usage resiurces can be updated with UpdateData()" );

    auto *pCtxVk = ValidatedCast<DeviceContextVkImpl>(pContext);
#if 0
    auto DstSubResIndex = VkCalcSubresource(MipLevel, Slice, 0, m_Desc.MipLevels, m_Desc.ArraySize);
 
    pCtxVk->CopyTextureRegion(SubresData.pSrcBuffer, SubresData.Stride, SubresData.DepthStride, this, DstSubResIndex, DstBox);
#endif
}

void TextureVkImpl ::  CopyData(IDeviceContext *pContext, 
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

    auto *pCtxVk = ValidatedCast<DeviceContextVkImpl>(pContext);
    auto *pSrcTexVk = ValidatedCast<TextureVkImpl>( pSrcTexture );

#if 0
    Vk_BOX VkSrcBox, *pVkSrcBox = nullptr;
    if( pSrcBox )
    {
        VkSrcBox.left    = pSrcBox->MinX;
        VkSrcBox.right   = pSrcBox->MaxX;
        VkSrcBox.top     = pSrcBox->MinY;
        VkSrcBox.bottom  = pSrcBox->MaxY;
        VkSrcBox.front   = pSrcBox->MinZ;
        VkSrcBox.back    = pSrcBox->MaxZ;
        pVkSrcBox = &VkSrcBox;
    }

    auto DstSubResIndex = VkCalcSubresource(DstMipLevel, DstSlice, 0, m_Desc.MipLevels, m_Desc.ArraySize);
    auto SrcSubResIndex = VkCalcSubresource(SrcMipLevel, SrcSlice, 0, pSrcTexVk->m_Desc.MipLevels, pSrcTexVk->m_Desc.ArraySize);
    pCtxVk->CopyTextureRegion(pSrcTexVk, SrcSubResIndex, pVkSrcBox, this, DstSubResIndex, DstX, DstY, DstZ);
#endif
}

void TextureVkImpl :: Map(IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags, MappedTextureSubresource &MappedData)
{
    TTextureBase::Map( pContext, Subresource, MapType, MapFlags, MappedData );
    LOG_ERROR_ONCE("TextureVkImpl::Map() is not implemented");

    static char TmpDummyBuffer[1024*1024*64];
    MappedData.pData = TmpDummyBuffer;
}

void TextureVkImpl::Unmap( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags )
{
    TTextureBase::Unmap( pContext, Subresource, MapType, MapFlags );
    LOG_ERROR_ONCE("TextureVkImpl::Unmap() is not implemented");
}

#if 0
void TextureVkImpl::CreateSRV( TextureViewDesc &SRVDesc, Vk_CPU_DESCRIPTOR_HANDLE SRVHandle )
{
    VERIFY( SRVDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE, "Incorrect view type: shader resource is expected" );
    
    if( SRVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        SRVDesc.Format = m_Desc.Format;
    }
    Vk_SHADER_RESOURCE_VIEW_DESC Vk_SRVDesc;
    TextureViewDesc_to_Vk_SRV_DESC(SRVDesc, Vk_SRVDesc, m_Desc.SampleCount);

    auto *pDeviceVk = static_cast<RenderDeviceVkImpl*>(GetDevice())->GetVkDevice();
    pDeviceVk->CreateShaderResourceView(m_pVkResource, &Vk_SRVDesc, SRVHandle);
}

void TextureVkImpl::CreateRTV( TextureViewDesc &RTVDesc, Vk_CPU_DESCRIPTOR_HANDLE RTVHandle )
{
    VERIFY( RTVDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET, "Incorrect view type: render target is expected" );

    if( RTVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        RTVDesc.Format = m_Desc.Format;
    }

    Vk_RENDER_TARGET_VIEW_DESC Vk_RTVDesc;
    TextureViewDesc_to_Vk_RTV_DESC(RTVDesc, Vk_RTVDesc, m_Desc.SampleCount);

    auto *pDeviceVk = static_cast<RenderDeviceVkImpl*>(GetDevice())->GetVkDevice();
    pDeviceVk->CreateRenderTargetView( m_pVkResource, &Vk_RTVDesc, RTVHandle );
}

void TextureVkImpl::CreateDSV( TextureViewDesc &DSVDesc, Vk_CPU_DESCRIPTOR_HANDLE DSVHandle )
{
    VERIFY( DSVDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Incorrect view type: depth stencil is expected" );

    if( DSVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        DSVDesc.Format = m_Desc.Format;
    }

    Vk_DEPTH_STENCIL_VIEW_DESC Vk_DSVDesc;
    TextureViewDesc_to_Vk_DSV_DESC(DSVDesc, Vk_DSVDesc, m_Desc.SampleCount);

    auto *pDeviceVk = static_cast<RenderDeviceVkImpl*>(GetDevice())->GetVkDevice();
    pDeviceVk->CreateDepthStencilView( m_pVkResource, &Vk_DSVDesc, DSVHandle );
}

void TextureVkImpl::CreateUAV( TextureViewDesc &UAVDesc, Vk_CPU_DESCRIPTOR_HANDLE UAVHandle )
{
    VERIFY( UAVDesc.ViewType == TEXTURE_VIEW_UNORDERED_ACCESS, "Incorrect view type: unordered access is expected" );
    
    if( UAVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        UAVDesc.Format = m_Desc.Format;
    }

    Vk_UNORDERED_ACCESS_VIEW_DESC Vk_UAVDesc;
    TextureViewDesc_to_Vk_UAV_DESC(UAVDesc, Vk_UAVDesc);

    auto *pDeviceVk = static_cast<RenderDeviceVkImpl*>(GetDevice())->GetVkDevice();
    pDeviceVk->CreateUnorderedAccessView( m_pVkResource, nullptr, &Vk_UAVDesc, UAVHandle );
}
#endif
}
