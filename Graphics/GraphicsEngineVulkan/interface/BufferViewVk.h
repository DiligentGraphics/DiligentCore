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
/// Definition of the Diligent::IBufferViewVk interface

#include "../../GraphicsEngine/interface/BufferView.h"

namespace Diligent
{

// {CB67024A-1E23-4202-A49A-07B6BCEABC06}
static constexpr INTERFACE_ID IID_BufferViewVk =
{ 0xcb67024a, 0x1e23, 0x4202,{ 0xa4, 0x9a, 0x7, 0xb6, 0xbc, 0xea, 0xbc, 0x6 } };

/// Interface to the buffer view object implemented in Vulkan
class IBufferViewVk : public IBufferView
{
public:

    /// Returns CPU descriptor handle of the buffer view.
    //virtual D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle() = 0;
};

}
