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

#pragma once

/// \file
/// Declaration of Diligent::TextureVkImpl class

#include "TextureVk.h"
#include "RenderDeviceVk.h"
#include "TextureBase.h"
#include "TextureViewVkImpl.h"
#include "VulkanUtilities/VulkanMemoryManager.h"
#include "RenderDeviceVkImpl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

struct MipLevelProperties
{
    Uint32 Width   = 0;
    Uint32 Height  = 0;
    Uint32 Depth   = 1;
    Uint32 RowSize = 0;
    Uint32 MipSize = 0;
};

MipLevelProperties GetMipLevelProperties(const TextureDesc& TexDesc, Uint32 MipLevel);
Uint32 GetStagingDataOffset(const TextureDesc& TexDesc, Uint32 ArraySlice, Uint32 MipLevel);

/// Base implementation of the Diligent::ITextureVk interface
class TextureVkImpl final : public TextureBase<ITextureVk, RenderDeviceVkImpl, TextureViewVkImpl, FixedBlockMemoryAllocator>
{
public:
    using TTextureBase = TextureBase<ITextureVk, RenderDeviceVkImpl, TextureViewVkImpl, FixedBlockMemoryAllocator>;
    using ViewImplType = TextureViewVkImpl;

    // Creates a new Vk resource
    TextureVkImpl(IReferenceCounters*        pRefCounters,
                  FixedBlockMemoryAllocator& TexViewObjAllocator,
                  RenderDeviceVkImpl*        pDeviceVk, 
                  const TextureDesc&         TexDesc, 
                  const TextureData*         pInitData = nullptr);
    
    // Attaches to an existing Vk resource
    TextureVkImpl(IReferenceCounters*         pRefCounters,
                  FixedBlockMemoryAllocator&  TexViewObjAllocator,
                  class RenderDeviceVkImpl*   pDeviceVk, 
                  const TextureDesc&          TexDesc, 
                  RESOURCE_STATE              InitialState,
                  VkImage                     VkImageHandle);

    ~TextureVkImpl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override final;

    virtual VkImage GetVkImage()const override final{ return m_VulkanImage; }
    virtual void* GetNativeHandle()override final
    {
        auto vkImage = GetVkImage();
        return vkImage;
    }

    ITextureView* GetMipLevelSRV(Uint32 MipLevel)
    {
        return m_MipLevelSRV[MipLevel].get();
    }

    ITextureView* GetMipLevelUAV(Uint32 MipLevel)
    {
        return m_MipLevelUAV[MipLevel].get();
    }

    
    void SetLayout(VkImageLayout Layout)override final;
    VkImageLayout GetLayout()const override final;

    VkBuffer GetVkStagingBuffer()const
    {
        return m_StagingBuffer;
    }

    uint8_t* GetStagingDataCPUAddress()const
    {
        auto* StagingDataCPUAddress = reinterpret_cast<uint8_t*>(m_MemoryAllocation.Page->GetCPUMemory());
        VERIFY_EXPR(StagingDataCPUAddress != nullptr);
        StagingDataCPUAddress += m_StagingDataAlignedOffset;
        return StagingDataCPUAddress;
    }

    void InvalidateStagingRange(VkDeviceSize Offset, VkDeviceSize Size);

protected:
    void CreateViewInternal( const struct TextureViewDesc& ViewDesc, ITextureView** ppView, bool bIsDefaultView )override;
    //void PrepareVkInitData(const TextureData &InitData, Uint32 NumSubresources, std::vector<Vk_SUBRESOURCE_DATA> &VkInitData);
    
    VulkanUtilities::ImageViewWrapper CreateImageView(TextureViewDesc &ViewDesc);

    VulkanUtilities::ImageWrapper           m_VulkanImage;
    VulkanUtilities::BufferWrapper          m_StagingBuffer;
    VulkanUtilities::VulkanMemoryAllocation m_MemoryAllocation;
    VkDeviceSize                            m_StagingDataAlignedOffset;

    // Texture views needed for mipmap generation
    std::vector<std::unique_ptr<TextureViewVkImpl, STDDeleter<TextureViewVkImpl, FixedBlockMemoryAllocator> > > m_MipLevelSRV;
    std::vector<std::unique_ptr<TextureViewVkImpl, STDDeleter<TextureViewVkImpl, FixedBlockMemoryAllocator> > > m_MipLevelUAV;
};

}
