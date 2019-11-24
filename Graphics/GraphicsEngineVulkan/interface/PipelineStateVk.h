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
/// Definition of the Diligent::IPipeplineStateVk interface

#include "../../GraphicsEngine/interface/PipelineState.h"

namespace Diligent
{

// {2FEA0868-0932-412A-9F0A-7CEA7E61B5E0}
static constexpr INTERFACE_ID IID_PipelineStateVk =
    {0x2fea0868, 0x932, 0x412a, {0x9f, 0xa, 0x7c, 0xea, 0x7e, 0x61, 0xb5, 0xe0}};


/// Exposes Vulkan-specific functionality of a pipeline state object.
class IPipelineStateVk : public IPipelineState
{
public:
    /// Returns handle to a vulkan render pass object.
    virtual VkRenderPass GetVkRenderPass() const = 0;

    /// Returns handle to a vulkan pipeline pass object.
    virtual VkPipeline GetVkPipeline() const = 0;
};

} // namespace Diligent
