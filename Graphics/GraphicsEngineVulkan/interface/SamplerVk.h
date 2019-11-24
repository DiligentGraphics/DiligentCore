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
/// Definition of the Diligent::ISamplerVk interface

#include "../../GraphicsEngine/interface/Sampler.h"

namespace Diligent
{

// {87C21E88-8A9F-4AD2-9A1E-D5EC140415EA}
static constexpr INTERFACE_ID IID_SamplerVk =
    {0x87c21e88, 0x8a9f, 0x4ad2, {0x9a, 0x1e, 0xd5, 0xec, 0x14, 0x4, 0x15, 0xea}};

/// Exposes Vulkan-specific functionality of a sampler object.
class ISamplerVk : public ISampler
{
public:
    /// Returns a vulkan sampler object handle
    virtual VkSampler GetVkSampler() const = 0;
};

} // namespace Diligent
