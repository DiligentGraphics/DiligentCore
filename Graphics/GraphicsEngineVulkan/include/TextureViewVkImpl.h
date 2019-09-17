/*     Copyright 2019 Diligent Graphics LLC
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
/// Declaration of Diligent::TextureViewVkImpl class

#include "TextureViewVk.h"
#include "RenderDeviceVk.h"
#include "TextureViewBase.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"
#include "RenderDeviceVkImpl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::ITextureViewVk interface
class TextureViewVkImpl final : public TextureViewBase<ITextureViewVk, RenderDeviceVkImpl>
{
public:
    using TTextureViewBase = TextureViewBase<ITextureViewVk, RenderDeviceVkImpl>;

    TextureViewVkImpl( IReferenceCounters*                 pRefCounters,
                       RenderDeviceVkImpl*                 pDevice, 
                       const TextureViewDesc&              ViewDesc, 
                       class ITexture*                     pTexture,
                       VulkanUtilities::ImageViewWrapper&& ImgView,
                       bool                                bIsDefaultView);
    ~TextureViewVkImpl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override final;

    VkImageView GetVulkanImageView()const override final{return m_ImageView;}

    bool HasMipLevelViews() const
    {
        return m_MipLevelViews != nullptr;
    }

    TextureViewVkImpl* GetMipLevelSRV(Uint32 MipLevel)
    {
        VERIFY_EXPR(m_MipLevelViews != nullptr && MipLevel < m_Desc.NumMipLevels);
        return m_MipLevelViews[MipLevel*2].get();
    }

    TextureViewVkImpl* GetMipLevelUAV(Uint32 MipLevel)
    {
        VERIFY_EXPR(m_MipLevelViews != nullptr && MipLevel < m_Desc.NumMipLevels);
        return m_MipLevelViews[MipLevel*2 + 1].get();
    }

    using MipLevelViewAutoPtrType = std::unique_ptr<TextureViewVkImpl, STDDeleter<TextureViewVkImpl, FixedBlockMemoryAllocator> >;

    void AssignMipLevelViews(MipLevelViewAutoPtrType* MipLevelViews)
    {
        m_MipLevelViews = MipLevelViews;
    }

protected:
    /// Vulkan image view descriptor handle
    VulkanUtilities::ImageViewWrapper       m_ImageView;

    /// Individual mip level views used for mipmap generation
    MipLevelViewAutoPtrType*                m_MipLevelViews = nullptr;
};

}
