/*     Copyright 2015-2019 Egor Yusov
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

TextureVkImpl :: TextureVkImpl(IReferenceCounters*          pRefCounters, 
                               FixedBlockMemoryAllocator&   TexViewObjAllocator,
                               RenderDeviceVkImpl*          pRenderDeviceVk, 
                               const TextureDesc&           TexDesc, 
                               const TextureData*           pInitData /*= nullptr*/) : 
    TTextureBase(pRefCounters, TexViewObjAllocator, pRenderDeviceVk, TexDesc)
{
    if( m_Desc.Usage == USAGE_STATIC && (pInitData == nullptr || pInitData->pSubResources == nullptr) )
        LOG_ERROR_AND_THROW("Static textures must be initialized with data at creation time");
    
    const auto& LogicalDevice = pRenderDeviceVk->GetLogicalDevice();

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
    {
        ImageCI.imageType = VK_IMAGE_TYPE_3D;
        ImageCI.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }
    else
    {
        LOG_ERROR_AND_THROW("Unknown texture type");
    }

    if (FmtAttribs.IsTypeless)
    {
        TEXTURE_VIEW_TYPE DefaultTexView;
        if(m_Desc.BindFlags & BIND_DEPTH_STENCIL)
            DefaultTexView = TEXTURE_VIEW_DEPTH_STENCIL;
        else if (m_Desc.BindFlags & BIND_UNORDERED_ACCESS)
            DefaultTexView = TEXTURE_VIEW_UNORDERED_ACCESS;
        else if (m_Desc.BindFlags & BIND_RENDER_TARGET)
            DefaultTexView = TEXTURE_VIEW_RENDER_TARGET;
        else
            DefaultTexView = TEXTURE_VIEW_SHADER_RESOURCE;
        auto DefaultViewFormat = GetDefaultTextureViewFormat(m_Desc, DefaultTexView);
        ImageCI.format = TexFormatToVkFormat(DefaultViewFormat);
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
    {
        // VK_IMAGE_USAGE_TRANSFER_DST_BIT is required for vkCmdClearColorImage()
        ImageCI.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (m_Desc.BindFlags & BIND_DEPTH_STENCIL)
    {
        // VK_IMAGE_USAGE_TRANSFER_DST_BIT is required for vkCmdClearDepthStencilImage()
        ImageCI.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if ((m_Desc.BindFlags & BIND_UNORDERED_ACCESS) || (m_Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS))
    {
        ImageCI.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (m_Desc.BindFlags & BIND_SHADER_RESOURCE)
    {
        ImageCI.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    ImageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageCI.queueFamilyIndexCount = 0;
    ImageCI.pQueueFamilyIndices = nullptr;

    // initialLayout must be either VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED (11.4)
    // If it is VK_IMAGE_LAYOUT_PREINITIALIZED, then the image data can be preinitialized by the host 
    // while using this layout, and the transition away from this layout will preserve that data. 
    // If it is VK_IMAGE_LAYOUT_UNDEFINED, then the contents of the data are considered to be undefined, 
    // and the transition away from this layout is not guaranteed to preserve that data.
    ImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    bool bInitializeTexture = (pInitData != nullptr && pInitData->pSubResources != nullptr && pInitData->NumSubresources > 0);

    m_VulkanImage = LogicalDevice.CreateImage(ImageCI, m_Desc.Name);

    VkMemoryRequirements MemReqs = LogicalDevice.GetImageMemoryRequirements(m_VulkanImage);
    
    VkMemoryPropertyFlags ImageMemoryFlags = 0;
    if (m_Desc.Usage == USAGE_CPU_ACCESSIBLE)
        ImageMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    else
        ImageMemoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VERIFY( IsPowerOfTwo(MemReqs.alignment), "Alignment is not power of 2!");
    m_MemoryAllocation = pRenderDeviceVk->AllocateMemory(MemReqs, ImageMemoryFlags);
    auto AlignedOffset = Align(m_MemoryAllocation.UnalignedOffset, MemReqs.alignment);
    VERIFY_EXPR(m_MemoryAllocation.Size >= MemReqs.size + (AlignedOffset - m_MemoryAllocation.UnalignedOffset));
    auto Memory = m_MemoryAllocation.Page->GetVkMemory();
    auto err = LogicalDevice.BindImageMemory(m_VulkanImage, Memory, AlignedOffset);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to bind image memory");

    
    // Vulkan validation layers do not like uninitialized memory, so if no initial data
    // is provided, we will clear the memory

    VulkanUtilities::CommandPoolWrapper CmdPool;
    VkCommandBuffer vkCmdBuff;
    pRenderDeviceVk->AllocateTransientCmdPool(CmdPool, vkCmdBuff, "Transient command pool to copy staging data to a device buffer");

    VkImageAspectFlags aspectMask = 0;
    if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH)
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
    {
        if(bInitializeTexture)
        {
            UNSUPPORTED("Initializing depth-stencil texture is not currently supported");
            // Only single aspect bit must be specified when copying texture data
        }
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else
        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    // For either clear or copy command, dst layout must be VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VkImageSubresourceRange SubresRange;
    SubresRange.aspectMask = aspectMask;
    SubresRange.baseArrayLayer = 0;
    SubresRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    SubresRange.baseMipLevel = 0;
    SubresRange.levelCount = VK_REMAINING_MIP_LEVELS;
    auto EnabledGraphicsShaderStages = LogicalDevice.GetEnabledGraphicsShaderStages();
    VulkanUtilities::VulkanCommandBuffer::TransitionImageLayout(vkCmdBuff, m_VulkanImage, ImageCI.initialLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresRange, EnabledGraphicsShaderStages);
    SetState(RESOURCE_STATE_COPY_DEST);
    const auto CurrentLayout = GetLayout();
    VERIFY_EXPR(CurrentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    if(bInitializeTexture)
    {
        Uint32 ExpectedNumSubresources = ImageCI.mipLevels * ImageCI.arrayLayers;
        if (pInitData->NumSubresources != ExpectedNumSubresources )
            LOG_ERROR_AND_THROW("Incorrect number of subresources in init data. ", ExpectedNumSubresources, " expected, while ", pInitData->NumSubresources, " provided");

        std::vector<VkBufferImageCopy> Regions(pInitData->NumSubresources);

        Uint64 uploadBufferSize = 0;
        Uint32 subres = 0;
        for(Uint32 layer = 0; layer < ImageCI.arrayLayers; ++layer)
        {
            for(Uint32 mip = 0; mip < ImageCI.mipLevels; ++mip)
            {
                const auto& SubResData = pInitData->pSubResources[subres]; (void)SubResData;
                auto& CopyRegion = Regions[subres];

                auto MipWidth  = std::max(m_Desc.Width  >> mip, 1u);
                auto MipHeight = std::max(m_Desc.Height >> mip, 1u);
                auto MipDepth  = (m_Desc.Type == RESOURCE_DIM_TEX_3D) ? std::max(m_Desc.Depth >> mip, 1u) : 1u;

                CopyRegion.bufferOffset = uploadBufferSize; // offset in bytes from the start of the buffer object
                // bufferRowLength and bufferImageHeight specify the data in buffer memory as a subregion 
                // of a larger two- or three-dimensional image, and control the addressing calculations of 
                // data in buffer memory. If either of these values is zero, that aspect of the buffer memory 
                // is considered to be tightly packed according to the imageExtent. (18.4)
                CopyRegion.bufferRowLength   = 0;
                CopyRegion.bufferImageHeight = 0;
                // For block-compression formats, all parameters are still specified in texels rather than compressed texel blocks (18.4.1)
                CopyRegion.imageOffset = VkOffset3D{0, 0, 0};
                CopyRegion.imageExtent = VkExtent3D{MipWidth, MipHeight, MipDepth};

                CopyRegion.imageSubresource.aspectMask = aspectMask;
                CopyRegion.imageSubresource.mipLevel = mip;
                CopyRegion.imageSubresource.baseArrayLayer = layer;
                CopyRegion.imageSubresource.layerCount = 1;

                Uint32 RowSize = 0;
                if(FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
                {
                    VERIFY_EXPR(FmtAttribs.BlockWidth > 1 && FmtAttribs.BlockHeight > 1);
                    MipWidth  = (MipWidth  + FmtAttribs.BlockWidth -1) / FmtAttribs.BlockWidth;
                    MipHeight = (MipHeight + FmtAttribs.BlockHeight-1) / FmtAttribs.BlockHeight;
                    RowSize   = MipWidth * Uint32{FmtAttribs.ComponentSize}; // ComponentSize is the block size
                }
                else
                {
                    RowSize = MipWidth * Uint32{FmtAttribs.ComponentSize} * Uint32{FmtAttribs.NumComponents};
                }
                auto MipSize = RowSize * MipHeight * MipDepth;
                VERIFY(SubResData.Stride == 0 || SubResData.Stride >= RowSize, "Stride is too small");
                VERIFY(SubResData.DepthStride == 0 || SubResData.DepthStride >= RowSize * MipHeight, "Depth stride is too small");

                // bufferOffset must be a multiple of 4 (18.4)
                // If the calling command's VkImage parameter is a compressed image, bufferOffset 
                // must be a multiple of the compressed texel block size in bytes (18.4). This
                // is automatically guaranteed as MipWidth and MipHeight are rounded to block size
                uploadBufferSize += (MipSize + 3) & (~3);
                ++subres;
            }
        }
        VERIFY_EXPR(subres == pInitData->NumSubresources);

        VkBufferCreateInfo VkStaginBuffCI = {};
        VkStaginBuffCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        VkStaginBuffCI.pNext = nullptr;
        VkStaginBuffCI.flags = 0;
        VkStaginBuffCI.size = uploadBufferSize;
        VkStaginBuffCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkStaginBuffCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkStaginBuffCI.queueFamilyIndexCount = 0;
        VkStaginBuffCI.pQueueFamilyIndices = nullptr;

        std::string StagingBufferName = "Staging buffer for '";
        StagingBufferName += m_Desc.Name;
        StagingBufferName += '\'';
        VulkanUtilities::BufferWrapper StagingBuffer = LogicalDevice.CreateBuffer(VkStaginBuffCI, StagingBufferName.c_str());

        VkMemoryRequirements StagingBufferMemReqs = LogicalDevice.GetBufferMemoryRequirements(StagingBuffer);
        VERIFY( IsPowerOfTwo(StagingBufferMemReqs.alignment), "Alignment is not power of 2!");
        // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit specifies that the host cache management commands vkFlushMappedMemoryRanges 
        // and vkInvalidateMappedMemoryRanges are NOT needed to flush host writes to the device or make device writes visible
        // to the host (10.2)
        auto StagingMemoryAllocation = pRenderDeviceVk->AllocateMemory(StagingBufferMemReqs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        auto StagingBufferMemory = StagingMemoryAllocation.Page->GetVkMemory();
        auto AlignedStagingMemOffset = Align(StagingMemoryAllocation.UnalignedOffset, StagingBufferMemReqs.alignment);
        VERIFY_EXPR(StagingMemoryAllocation.Size >= StagingBufferMemReqs.size + (AlignedStagingMemOffset - StagingMemoryAllocation.UnalignedOffset));

        auto *StagingData = reinterpret_cast<uint8_t*>(StagingMemoryAllocation.Page->GetCPUMemory());
        VERIFY_EXPR(StagingData != nullptr);
        StagingData += AlignedStagingMemOffset;

        subres = 0;
        for(Uint32 layer = 0; layer < ImageCI.arrayLayers; ++layer)
        {
            for(Uint32 mip = 0; mip < ImageCI.mipLevels; ++mip)
            {
                const auto &SubResData = pInitData->pSubResources[subres];
                const auto &CopyRegion = Regions[subres];

                auto MipWidth  = CopyRegion.imageExtent.width;
                auto MipHeight = CopyRegion.imageExtent.height;
                auto MipDepth  = CopyRegion.imageExtent.depth;
                Uint32 RowSize = 0;
                if(FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
                {
                    VERIFY_EXPR(FmtAttribs.BlockWidth > 1 && FmtAttribs.BlockHeight > 1);
                    MipWidth  = (MipWidth  + FmtAttribs.BlockWidth -1) / FmtAttribs.BlockWidth;
                    MipHeight = (MipHeight + FmtAttribs.BlockHeight-1) / FmtAttribs.BlockHeight;
                    RowSize   = MipWidth * Uint32{FmtAttribs.ComponentSize}; // ComponentSize is the block size
                }
                else
                {
                    RowSize = MipWidth * Uint32{FmtAttribs.ComponentSize} * Uint32{FmtAttribs.NumComponents};
                }
                VERIFY(SubResData.Stride == 0 || SubResData.Stride >= RowSize, "Stride is too small");
                VERIFY(SubResData.DepthStride == 0 || SubResData.DepthStride >= RowSize * MipHeight, "Depth stride is too small");

                for(Uint32 z=0; z < MipDepth; ++z)
                {
                    for(Uint32 y=0; y < MipHeight; ++y)
                    {
                        memcpy(StagingData + CopyRegion.bufferOffset + (y + z * MipHeight) * RowSize,
                               reinterpret_cast<const uint8_t*>(SubResData.pData) + y * SubResData.Stride + z * SubResData.DepthStride,
                               RowSize);
                    }
                }
               
                ++subres;
            }
        }
        VERIFY_EXPR(subres == pInitData->NumSubresources);

        err = LogicalDevice.BindBufferMemory(StagingBuffer, StagingBufferMemory, AlignedStagingMemOffset);
        CHECK_VK_ERROR_AND_THROW(err, "Failed to bind staging bufer memory");

        VulkanUtilities::VulkanCommandBuffer::BufferMemoryBarrier(vkCmdBuff, StagingBuffer, 0, VK_ACCESS_TRANSFER_READ_BIT, EnabledGraphicsShaderStages);

        // Copy commands MUST be recorded outside of a render pass instance. This is OK here
        // as copy will be the only command in the cmd buffer
        vkCmdCopyBufferToImage(vkCmdBuff, StagingBuffer, m_VulkanImage,
            CurrentLayout, // dstImageLayout must be VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL (18.4)
            static_cast<uint32_t>(Regions.size()), Regions.data());

        Uint32 QueueIndex = 0;
	    pRenderDeviceVk->ExecuteAndDisposeTransientCmdBuff(QueueIndex, vkCmdBuff, std::move(CmdPool));

        // After command buffer is submitted, safe-release resources. This strategy
        // is little overconservative as the resources will be released after the first
        // command buffer submitted through the immediate context will be completed
        pRenderDeviceVk->SafeReleaseDeviceObject(std::move(StagingBuffer),           Uint64{1} << Uint64{QueueIndex});
        pRenderDeviceVk->SafeReleaseDeviceObject(std::move(StagingMemoryAllocation), Uint64{1} << Uint64{QueueIndex});
    }
    else
    {
        VkImageSubresourceRange Subresource;
        Subresource.aspectMask     = aspectMask;
        Subresource.baseMipLevel   = 0;
        Subresource.levelCount     = VK_REMAINING_MIP_LEVELS;
        Subresource.baseArrayLayer = 0;
        Subresource.layerCount     = VK_REMAINING_ARRAY_LAYERS;
        if(aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            if(FmtAttribs.ComponentType != COMPONENT_TYPE_COMPRESSED)
            {
                VkClearColorValue ClearColor = {};
                vkCmdClearColorImage(vkCmdBuff, m_VulkanImage,
                                CurrentLayout, // must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                                &ClearColor, 1, &Subresource);
            }
        }
        else if(aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT || 
                aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) )
        {
            VkClearDepthStencilValue ClearValue = {};
            vkCmdClearDepthStencilImage(vkCmdBuff, m_VulkanImage,
                            CurrentLayout, // must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                            &ClearValue, 1, &Subresource);
        }
        else
        {
            UNEXPECTED("Unexpected aspect mask");
        }
        Uint32 QueueIndex = 0;
        pRenderDeviceVk->ExecuteAndDisposeTransientCmdBuff(QueueIndex, vkCmdBuff, std::move(CmdPool));
    }


    if(m_Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS)
    {
        if (m_Desc.Type != RESOURCE_DIM_TEX_2D && m_Desc.Type != RESOURCE_DIM_TEX_2D_ARRAY)
        {
            LOG_ERROR_AND_THROW("Mipmap generation is only supported for 2D textures and texture arrays");
        }

        m_MipLevelUAV.reserve(m_Desc.MipLevels);
        for(Uint32 MipLevel = 0; MipLevel < m_Desc.MipLevels; ++MipLevel)
        {
            // Create mip level UAV
            TextureViewDesc UAVDesc;
            std::stringstream name_ss;
            name_ss << "Mip " << MipLevel << " UAV for texture '" << m_Desc.Name << "'";
            auto name = name_ss.str();
            UAVDesc.Name = name.c_str();
            // Always create texture array UAV
            UAVDesc.TextureDim = RESOURCE_DIM_TEX_2D_ARRAY;
            UAVDesc.ViewType = TEXTURE_VIEW_UNORDERED_ACCESS;
            UAVDesc.FirstArraySlice = 0;
            UAVDesc.NumArraySlices = m_Desc.ArraySize;
            UAVDesc.MostDetailedMip = MipLevel;
            if (m_Desc.Format == TEX_FORMAT_RGBA8_UNORM_SRGB)
                UAVDesc.Format = TEX_FORMAT_RGBA8_UNORM;
            ITextureView* pMipUAV = nullptr;
            CreateViewInternal( UAVDesc, &pMipUAV, true );
            m_MipLevelUAV.emplace_back(ValidatedCast<TextureViewVkImpl>(pMipUAV), STDDeleter<TextureViewVkImpl, FixedBlockMemoryAllocator>(TexViewObjAllocator));
        }
        VERIFY_EXPR(m_MipLevelUAV.size() == m_Desc.MipLevels);

        m_MipLevelSRV.reserve(m_Desc.MipLevels);
        for(Uint32 MipLevel = 0; MipLevel < m_Desc.MipLevels; ++MipLevel)
        {
            // Create mip level SRV
            TextureViewDesc TexArraySRVDesc;
            std::stringstream name_ss;
            name_ss << "Mip " << MipLevel << " SRV for texture '" << m_Desc.Name << "'";
            auto name = name_ss.str();
            TexArraySRVDesc.Name = name.c_str();
            // Alaways create texture array view
            TexArraySRVDesc.TextureDim = RESOURCE_DIM_TEX_2D_ARRAY;
            TexArraySRVDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
            TexArraySRVDesc.FirstArraySlice = 0;
            TexArraySRVDesc.NumArraySlices = m_Desc.ArraySize;
            TexArraySRVDesc.MostDetailedMip = MipLevel;
            TexArraySRVDesc.NumMipLevels = 1;
            ITextureView* pMipLevelSRV = nullptr;
            CreateViewInternal( TexArraySRVDesc, &pMipLevelSRV, true );
            m_MipLevelSRV.emplace_back(ValidatedCast<TextureViewVkImpl>(pMipLevelSRV), STDDeleter<TextureViewVkImpl, FixedBlockMemoryAllocator>(TexViewObjAllocator));
        }
        VERIFY_EXPR(m_MipLevelSRV.size() == m_Desc.MipLevels);
    }

    VERIFY_EXPR(IsInKnownState());
}

TextureVkImpl::TextureVkImpl(IReferenceCounters*         pRefCounters,
                             FixedBlockMemoryAllocator&  TexViewObjAllocator,
                             RenderDeviceVkImpl*         pDeviceVk, 
                             const TextureDesc&          TexDesc,
                             RESOURCE_STATE              InitialState,
                             VkImage                     VkImageHandle) :
    TTextureBase(pRefCounters, TexViewObjAllocator, pDeviceVk, TexDesc),
    m_VulkanImage(VkImageHandle)
{
    SetState(InitialState);
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
        auto &TexViewAllocator = m_pDevice->GetTexViewObjAllocator();
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
    // Vk object can only be destroyed when it is no longer used by the GPU
    // Wrappers for external texture will not be destroyed as they are created with null device pointer
    m_pDevice->SafeReleaseDeviceObject(std::move(m_VulkanImage),      m_Desc.CommandQueueMask);
    m_pDevice->SafeReleaseDeviceObject(std::move(m_MemoryAllocation), m_Desc.CommandQueueMask);
}

VulkanUtilities::ImageViewWrapper TextureVkImpl::CreateImageView(TextureViewDesc& ViewDesc)
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
            if (ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET || ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL)
            {
                ViewDesc.TextureDim = RESOURCE_DIM_TEX_2D_ARRAY;
                ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }
            else
            {
                ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_3D;
                Uint32 MipDepth = std::max(m_Desc.Depth >> ViewDesc.MostDetailedMip, 1U);
                if (ViewDesc.FirstDepthSlice != 0 || ViewDesc.NumDepthSlices != MipDepth)
                {
                    LOG_ERROR("3D texture view '", (ViewDesc.Name ? ViewDesc.Name : ""), "' (most detailed mip: ", ViewDesc.MostDetailedMip,
                              "; mip levels: ", ViewDesc.NumMipLevels, "; first slice: ", ViewDesc.FirstDepthSlice,
                              "; num depth slices: ", ViewDesc.NumDepthSlices, ") of texture '", m_Desc.Name, "' does not references"
                              " all depth slices (", MipDepth, ") in the mip level. 3D texture views in Vulkan must address all depth slices." );
                    ViewDesc.FirstDepthSlice = 0;
                    ViewDesc.NumDepthSlices  = MipDepth;
                }
            }
        break;

        case RESOURCE_DIM_TEX_CUBE:
            ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        break;

        case RESOURCE_DIM_TEX_CUBE_ARRAY:
            ImageViewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        break;

        default: UNEXPECTED("Unexpcted view dimension");
    }
    
    TEXTURE_FORMAT CorrectedViewFormat = ViewDesc.Format;
    if(m_Desc.BindFlags & BIND_DEPTH_STENCIL)
        CorrectedViewFormat = GetDefaultTextureViewFormat(CorrectedViewFormat, TEXTURE_VIEW_DEPTH_STENCIL, m_Desc.BindFlags);
    ImageViewCI.format = TexFormatToVkFormat(CorrectedViewFormat);
    ImageViewCI.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    ImageViewCI.subresourceRange.baseMipLevel   = ViewDesc.MostDetailedMip;
    ImageViewCI.subresourceRange.levelCount     = ViewDesc.NumMipLevels;
    if (ViewDesc.TextureDim == RESOURCE_DIM_TEX_1D_ARRAY || 
        ViewDesc.TextureDim == RESOURCE_DIM_TEX_2D_ARRAY ||
        ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE     || 
        ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE_ARRAY )
    {
        ImageViewCI.subresourceRange.baseArrayLayer = ViewDesc.FirstArraySlice;
        ImageViewCI.subresourceRange.layerCount     = ViewDesc.NumArraySlices;
    }
    else
    {
        ImageViewCI.subresourceRange.baseArrayLayer = 0;
        ImageViewCI.subresourceRange.layerCount     = 1;
    }

    const auto &FmtAttribs = GetTextureFormatAttribs(CorrectedViewFormat);

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
        {
            ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
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
    
    const auto& LogicalDevice = m_pDevice->GetLogicalDevice();

    std::string ViewName = "Image view for \'";
    ViewName += m_Desc.Name;
    ViewName += '\'';
    return LogicalDevice.CreateImageView(ImageViewCI, ViewName.c_str());
}

void TextureVkImpl::SetLayout(VkImageLayout Layout)
{
    SetState(VkImageLayoutToResourceState(Layout));
}

VkImageLayout TextureVkImpl::GetLayout()const
{
    return ResourceStateToVkImageLayout(GetState());
}

}
