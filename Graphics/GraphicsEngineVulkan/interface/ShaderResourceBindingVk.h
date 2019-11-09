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
/// Definition of the Diligent::IShaderResourceBindingVk interface and related data structures

#include "../../GraphicsEngine/interface/ShaderResourceBinding.h"

namespace Diligent
{

// {1E8C82DC-5B3A-47D5-8AE9-197CAE8DB71F}
static constexpr INTERFACE_ID IID_ShaderResourceBindingVk =
{ 0x1e8c82dc, 0x5b3a, 0x47d5,{ 0x8a, 0xe9, 0x19, 0x7c, 0xae, 0x8d, 0xb7, 0x1f } };

/// Exposes Vulkan-specific functionality of a shader resource binding object.
class IShaderResourceBindingVk : public IShaderResourceBinding
{
public:

};

}
