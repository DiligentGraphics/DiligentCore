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
/// Declaration of Diligent::TextureVkImpl class

#include "TextureVk.h"
#include "RenderDeviceVk.h"
#include "TextureBase.h"
#include "TextureViewVkImpl.h"
#include "VulkanUtilities/VulkanMemoryManager.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Base implementation of the Diligent::ITextureVk interface
class TextureVkImpl : public TextureBase<ITextureVk, TextureViewVkImpl, FixedBlockMemoryAllocator>
{
public:
    typedef TextureBase<ITextureVk, TextureViewVkImpl, FixedBlockMemoryAllocator> TTextureBase;

    // Creates a new Vk resource
    TextureVkImpl(IReferenceCounters*        pRefCounters,
                  FixedBlockMemoryAllocator& TexViewObjAllocator,
                  class RenderDeviceVkImpl*  pDeviceVk, 
                  const TextureDesc&         TexDesc, 
                  const TextureData&         InitData = TextureData());
    
    // Attaches to an existing Vk resource
    TextureVkImpl(IReferenceCounters*         pRefCounters,
                  FixedBlockMemoryAllocator&  TexViewObjAllocator,
                  class RenderDeviceVkImpl*   pDeviceVk, 
                  const TextureDesc&          TexDesc, 
                  VkImage                     VkImageHandle);

    ~TextureVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID& IID, IObject** ppInterface )override;

    virtual void UpdateData( IDeviceContext* pContext, Uint32 MipLevel, Uint32 Slice, const Box& DstBox, const TextureSubResData& SubresData )override;

    //virtual void CopyData(CTexture* pSrcTexture, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size);
    virtual void Map( IDeviceContext* pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags, MappedTextureSubresource& MappedData )override;
    virtual void Unmap( IDeviceContext* pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags )override;

    virtual VkImage GetVkImage()const override final{ return m_VulkanImage; }
    virtual void* GetNativeHandle()override final { return GetVkImage(); }
/*
    virtual void SetVkResourceState(Vk_RESOURCE_STATES state)override final{ SetState(state); }
    */

    void CopyData(IDeviceContext* pContext, 
                  ITexture* pSrcTexture, 
                  Uint32 SrcMipLevel,
                  Uint32 SrcSlice,
                  const Box* pSrcBox,
                  Uint32 DstMipLevel,
                  Uint32 DstSlice,
                  Uint32 DstX,
                  Uint32 DstY,
                  Uint32 DstZ);

    ITextureView* GetMipLevelSRV(Uint32 MipLevel)
    {
        return m_MipLevelSRV[MipLevel].get();
    }

    ITextureView* GetMipLevelUAV(Uint32 MipLevel)
    {
        return m_MipLevelUAV[MipLevel].get();
    }

    void SetLayout(VkImageLayout NewLayout){ m_CurrentLayout = NewLayout;}
    VkImageLayout GetLayout()const{return m_CurrentLayout;}

protected:

    void CreateViewInternal( const struct TextureViewDesc& ViewDesc, ITextureView** ppView, bool bIsDefaultView )override;
    //void PrepareVkInitData(const TextureData &InitData, Uint32 NumSubresources, std::vector<Vk_SUBRESOURCE_DATA> &VkInitData);
    
    VulkanUtilities::ImageViewWrapper CreateImageView(TextureViewDesc &ViewDesc);

    VulkanUtilities::ImageWrapper m_VulkanImage;
    VulkanUtilities::VulkanMemoryAllocation m_MemoryAllocation;
    VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Texture views needed for mipmap generation
    std::vector<std::unique_ptr<TextureViewVkImpl, STDDeleter<TextureViewVkImpl, FixedBlockMemoryAllocator> > > m_MipLevelSRV;
    std::vector<std::unique_ptr<TextureViewVkImpl, STDDeleter<TextureViewVkImpl, FixedBlockMemoryAllocator> > > m_MipLevelUAV;
};

}
