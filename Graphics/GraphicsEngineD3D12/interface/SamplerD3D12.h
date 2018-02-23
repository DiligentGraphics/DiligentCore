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
/// Definition of the Diligent::ISamplerD3D12 interface

#include "../../GraphicsEngine/interface/Sampler.h"

namespace Diligent
{

// {31A3BFAF-738E-4D8C-AD18-B021C5D948DD}
static constexpr INTERFACE_ID IID_SamplerD3D12 =
{ 0x31a3bfaf, 0x738e, 0x4d8c, { 0xad, 0x18, 0xb0, 0x21, 0xc5, 0xd9, 0x48, 0xdd } };

/// Interface to the sampler object implemented in D3D12
class ISamplerD3D12 : public ISampler
{
public:

    /// Returns a CPU descriptor handle of the D3D12 sampler object
    
    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle() = 0;
};

}
