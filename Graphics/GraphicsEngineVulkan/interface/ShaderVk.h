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
/// Definition of the Diligent::IShaderVk interface

#include "../../GraphicsEngine/interface/Shader.h"

namespace Diligent
{

// {8B0C91B4-B1D8-4E03-9250-A70E131A59FA}
static constexpr INTERFACE_ID IID_ShaderVk =
{ 0x8b0c91b4, 0xb1d8, 0x4e03,{ 0x92, 0x50, 0xa7, 0xe, 0x13, 0x1a, 0x59, 0xfa } };

/// Interface to the shader object implemented in Vulkan
class IShaderVk : public IShader
{
public:

    /// Returns Vulkan shader module handle
    virtual VkShaderModule GetVkShaderModule() = 0;
};

}
