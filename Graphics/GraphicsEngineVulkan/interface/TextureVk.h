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
/// Definition of the Diligent::ITextureVk interface

#include "../../GraphicsEngine/interface/Texture.h"

namespace Diligent
{

// {3BB9155F-22C5-4365-927E-8C4049F9B949}
static constexpr INTERFACE_ID IID_TextureVk =
{ 0x3bb9155f, 0x22c5, 0x4365,{ 0x92, 0x7e, 0x8c, 0x40, 0x49, 0xf9, 0xb9, 0x49 } };


/// Interface to the texture object implemented in Vulkan
class ITextureVk : public ITexture
{
public:

    /// Returns Vulkan image handle.
    
    /// The application must not release the returned image
    virtual VkImage GetVkImage()const = 0;

    /// Sets the texture usage state

    /// \param [in] state - D3D12 resource state to be set for this texture
    //virtual void SetD3D12ResourceState(D3D12_RESOURCE_STATES state) = 0;
};

}
