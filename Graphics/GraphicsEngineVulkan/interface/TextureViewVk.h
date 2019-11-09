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
/// Definition of the Diligent::ITextureViewVk interface

#include "../../GraphicsEngine/interface/TextureView.h"

namespace Diligent
{

// {B02AA468-3328-46F3-9777-55E97BF6C86E}
static constexpr INTERFACE_ID IID_TextureViewVk =
{ 0xb02aa468, 0x3328, 0x46f3,{ 0x97, 0x77, 0x55, 0xe9, 0x7b, 0xf6, 0xc8, 0x6e } };

/// Exposes Vulkan-specific functionality of a texture view object.
class ITextureViewVk : public ITextureView
{
public:
    
    /// Returns Vulkan image view handle
    virtual VkImageView GetVulkanImageView()const = 0;
};

}
