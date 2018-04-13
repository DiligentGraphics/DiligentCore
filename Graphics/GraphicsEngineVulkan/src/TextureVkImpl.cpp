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
    TTextureBase(pRefCounters, TexViewObjAllocator, pRenderDeviceVk, TexDesc)
{
    if( m_Desc.Usage == USAGE_STATIC && InitData.pSubResources == nullptr )
        LOG_ERROR_AND_THROW("Static Texture must be initialized with data at creation time");
    
    const auto& LogicalDevice = pRenderDeviceVk->GetLogicalDevice();
    const auto& PhysicalDevice = pRenderDeviceVk->GetPhysicalDevice();

    VkImageCreateInfo ImageCI = {};
    ImageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageCI.pNext = nullptr;
    ImageCI.flags = 0;
    if(m_Desc.Type == RESOURCE_DIM_TEX_CUBE || m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
        ImageCI.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    const auto &FmtAttribs = GetTextureFormatAttribs(TexDesc.Format);
    if(FmtAttribs.IsTypeless)
        ImageCI.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT; // Specifies that the image can be used to create a 
                                                             // VkImageView with a different format from the image.

    if (m_Desc.Type == RESOURCE_DIM_TEX_1D || m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY)
        ImageCI.imageType = VK_IMAGE_TYPE_1D;
    else if (m_Desc.Type == RESOURCE_DIM_TEX_2D || m_Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY || m_Desc.Type == RESOURCE_DIM_TEX_CUBE || m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
        ImageCI.imageType = VK_IMAGE_TYPE_2D;
    else if (m_Desc.Type == RESOURCE_DIM_TEX_3D)
        ImageCI.imageType = VK_IMAGE_TYPE_3D;
    else
    {
        LOG_ERROR_AND_THROW("Unknown texture type");
    }

    if (FmtAttribs.IsTypeless)
    {
        // Use SRV format to create the texture
        auto SRVFormat = GetDefaultTextureViewFormat(m_Desc, TEXTURE_VIEW_SHADER_RESOURCE);
        ImageCI.format = TexFormatToVkFormat(SRVFormat);
    }
    else
    {
        ImageCI.format = TexFormatToVkFormat(m_Desc.Format);
    }

    ImageCI.extent.width = m_Desc.Width;
    ImageCI.extent.height = (m_Desc.Type == RESOURCE_DIM_TEX_1D || m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY) ? 1 : m_Desc.Height;
    ImageCI.extent.depth = (m_Desc.Type == RESOURCE_DIM_TEX_3D) ? m_Desc.Depth : 1;
    
    ImageCI.mipLevels = m_Desc.MipLevels;
    if (m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY || 
        m_Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY || 
        m_Desc.Type == RESOURCE_DIM_TEX_CUBE || 
        m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
        ImageCI.arrayLayers = m_Desc.ArraySize;
    else
        ImageCI.arrayLayers = 1;

    ImageCI.samples = static_cast<VkSampleCountFlagBits>(1 << (m_Desc.SampleCount-1));
    ImageCI.tiling = VK_IMAGE_TILING_OPTIMAL;

    ImageCI.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (m_Desc.BindFlags & BIND_RENDER_TARGET)
        ImageCI.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (m_Desc.BindFlags & BIND_DEPTH_STENCIL)
        ImageCI.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((m_Desc.BindFlags & BIND_UNORDERED_ACCESS) || (m_Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS))
        ImageCI.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (m_Desc.BindFlags & BIND_SHADER_RESOURCE)
        ImageCI.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    ImageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageCI.queueFamilyIndexCount = 0;
    ImageCI.pQueueFamilyIndices = nullptr;

    ImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

#if 0
    Desc.Flags = Vk_RESOURCE_FLAG_NONE;

    auto Format = TexFormatToDXGI_Format(m_Desc.Format, m_Desc.BindFlags);
    if (Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB && (Desc.Flags & Vk_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        Desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    else
        Desc.Format = Format;

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
#endif

    bool bInitializeTexture = (InitData.pSubResources != nullptr && InitData.NumSubresources > 0);
    //if(bInitializeTexture)
    //    m_UsageState = Vk_RESOURCE_STATE_COPY_DEST;

    m_VulkanImage = LogicalDevice.CreateImage(ImageCI, m_Desc.Name);

    VkMemoryRequirements MemReqs = LogicalDevice.GetImageMemoryRequirements(m_VulkanImage);

    VkMemoryAllocateInfo MemAlloc = {};
    MemAlloc.pNext = nullptr;
    MemAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemAlloc.allocationSize = MemReqs.size;

    VkMemoryPropertyFlags ImageMemoryFlags = 0;
    if (m_Desc.Usage == USAGE_CPU_ACCESSIBLE)
        ImageMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    else
        ImageMemoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // memoryTypeBits is a bitmask and contains one bit set for every supported memory type for the resource. 
    // Bit i is set if and only if the memory type i in the VkPhysicalDeviceMemoryProperties structure for the 
    // physical device is supported for the resource.
    MemAlloc.memoryTypeIndex = PhysicalDevice.GetMemoryTypeIndex(MemReqs.memoryTypeBits, ImageMemoryFlags);
    if (ImageMemoryFlags == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        // There must be at least one memory type with the DEVICE_LOCAL_BIT bit set
        VERIFY(MemAlloc.memoryTypeIndex != VulkanUtilities::VulkanPhysicalDevice::InvalidMemoryTypeIndex,
               "Vulkan spec requires that memoryTypeBits member always contains "
               "at least one bit set corresponding to a VkMemoryType with a propertyFlags that has the "
               "VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT bit set (11.6)");
    }          
    else if (MemAlloc.memoryTypeIndex != VulkanUtilities::VulkanPhysicalDevice::InvalidMemoryTypeIndex)
    {
        LOG_ERROR_AND_THROW("Failed to find suitable device memory type for an image");
    }

    std::string MemoryName = "Device memory for \'";
    MemoryName += m_Desc.Name;
    MemoryName += '\'';
    m_ImageMemory = LogicalDevice.AllocateDeviceMemory(MemAlloc, MemoryName.c_str());

    auto err = LogicalDevice.BindImageMemory(m_VulkanImage, m_ImageMemory, 0 /*offset*/);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to bind image memory");

    if(bInitializeTexture)
    {
        auto StagingImageCI = ImageCI;
        StagingImageCI.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        std::string StagingBufferName = "Staging buffer for \'";
        StagingBufferName += m_Desc.Name;
        StagingBufferName += '\'';


        VkMemoryRequirements StagingMemReqs = LogicalDevice.GetImageMemoryRequirements(m_VulkanImage);

        std::string StaginMemoryName = "Staging memory for \'";
        StaginMemoryName += m_Desc.Name;
        StaginMemoryName += '\'';


#if 0
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
#endif
    }

#if 0
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
                             VkImage&& VkImageHandle) :
    TTextureBase(pRefCounters, TexViewObjAllocator, pDeviceVk, InitTexDescFromVkImage(VkImageHandle, TexDesc)),
    m_VulkanImage(nullptr, std::move(VkImageHandle))
{
}
IMPLEMENT_QUERY_INTERFACE( TextureVkImpl, IID_TextureVk, TTextureBase )

void TextureVkImpl::CreateViewInternal( const struct TextureViewDesc &ViewDesc, ITextureView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "View pointer address is null" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );
    
    *ppView = nullptr;

    try
    {
        auto *pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(GetDevice());
        auto &TexViewAllocator = pDeviceVkImpl->GetTexViewObjAllocator();
        VERIFY( &TexViewAllocator == &m_dbgTexViewObjAllocator, "Texture view allocator does not match allocator provided during texture initialization" );

        auto UpdatedViewDesc = ViewDesc;
        CorrectTextureViewDesc( UpdatedViewDesc );

        VulkanUtilities::ImageViewWrapper ImgView = CreateImageView(UpdatedViewDesc);

        auto pViewVk = NEW_RC_OBJ(TexViewAllocator, "TextureViewVkImpl instance", TextureViewVkImpl, bIsDefaultView ? this : nullptr)
                                    (GetDevice(), UpdatedViewDesc, this, std::move(ImgView), bIsDefaultView );
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
}

TextureVkImpl :: ~TextureVkImpl()
{
    auto *pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(GetDevice());
    // Vk object can only be destroyed when it is no longer used by the GPU
    // Wrappers for external texture will not be destroyed as they are created with null device pointer
    pDeviceVkImpl->SafeReleaseVkObject(std::move(m_VulkanImage));
    pDeviceVkImpl->SafeReleaseVkObject(std::move(m_ImageMemory));
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
    UNEXPECTED("TextureVkImpl::Map() is not implemented");
}

void TextureVkImpl::Unmap( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags )
{
    TTextureBase::Unmap( pContext, Subresource, MapType, MapFlags );
    UNEXPECTED("TextureVkImpl::Unmap() is not implemented");
}

VulkanUtilities::ImageViewWrapper TextureVkImpl::CreateImageView(TextureViewDesc &ViewDesc)
{
    VERIFY(ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE ||
           ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET ||
           ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL ||
           ViewDesc.ViewType == TEXTURE_VIEW_UNORDERED_ACCESS, "Unexpected view type");
    if (ViewDesc.Format == TEX_FORMAT_UNKNOWN)
    {
        ViewDesc.Format = m_Desc.Format;
    }

    VkImageViewCreateInfo ImageViewCI = {};
    ImageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ImageViewCI.pNext = nullptr;
    ImageViewCI.flags = 0; // reserved for future use.
    ImageViewCI.image = m_VulkanImage;

    switch(ViewDesc.TextureDim)
    {
        case RESOURCE_DIM_TEX_1D:
            ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_1D;
        break;

        case RESOURCE_DIM_TEX_1D_ARRAY:
            ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        break;

        case RESOURCE_DIM_TEX_2D:
            ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        break;

        case RESOURCE_DIM_TEX_2D_ARRAY:
            ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        break;

        case RESOURCE_DIM_TEX_3D:
            ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_3D;
        break;

        case RESOURCE_DIM_TEX_CUBE:
            ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        break;

        case RESOURCE_DIM_TEX_CUBE_ARRAY:
            ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        break;

        default: UNEXPECTED("Unexpcted view dimension");
    }
    
    ImageViewCI.format = TexFormatToVkFormat(ViewDesc.Format);
    ImageViewCI.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    ImageViewCI.subresourceRange.baseMipLevel = ViewDesc.MostDetailedMip;
    ImageViewCI.subresourceRange.levelCount = ViewDesc.NumMipLevels;
    ImageViewCI.subresourceRange.baseArrayLayer = ViewDesc.FirstArraySlice;
    ImageViewCI.subresourceRange.layerCount = ViewDesc.NumArraySlices;

    const auto &FmtAttribs = GetTextureFormatAttribs(ViewDesc.Format);

    if(ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL)
    {
        // When an imageView of a depth/stencil image is used as a depth/stencil framebuffer attachment, 
        // the aspectMask is ignored and both depth and stencil image subresources are used. (11.5)
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH)
            ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
            ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        else
            UNEXPECTED("Unexpected component type for a depth-stencil view format");
    }
    else
    {
        // aspectMask must be only VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT or VK_IMAGE_ASPECT_STENCIL_BIT 
        // if format is a color, depth-only or stencil-only format, respectively.  (11.5)
        if(FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH )
            ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
        {
            if(ViewDesc.Format == TEX_FORMAT_D32_FLOAT_S8X24_UINT || 
               ViewDesc.Format == TEX_FORMAT_D24_UNORM_S8_UINT)
            {
                ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            else if (ViewDesc.Format == TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
                     ViewDesc.Format == TEX_FORMAT_R24_UNORM_X8_TYPELESS)
            {
                ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            }
            else if (ViewDesc.Format == TEX_FORMAT_X32_TYPELESS_G8X24_UINT || 
                     ViewDesc.Format == TEX_FORMAT_X24_TYPELESS_G8_UINT)
            {
                ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            else
                UNEXPECTED("Unexpected depth-stencil texture format");
        }
        else
            ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    auto *pRenderDeviceVk = static_cast<RenderDeviceVkImpl*>(GetDevice());
    const auto& LogicalDevice = pRenderDeviceVk->GetLogicalDevice();

    std::string ViewName = "Image view for \'";
    ViewName += m_Desc.Name;
    ViewName += '\'';
    return LogicalDevice.CreateImageView(ImageViewCI, ViewName.c_str());
}

}
