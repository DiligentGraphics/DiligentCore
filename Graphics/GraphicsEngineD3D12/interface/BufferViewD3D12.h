/*     Copyright 2015-2016 Egor Yusov
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
/// Definition of the Diligent::IBufferViewD3D12 interface

#include "BufferView.h"

namespace Diligent
{

// {09643F2F-40D4-4076-B086-9E5CDC2CC4FC}
static const Diligent::INTERFACE_ID IID_BufferViewD3D12 =
{ 0x9643f2f, 0x40d4, 0x4076, { 0xb0, 0x86, 0x9e, 0x5c, 0xdc, 0x2c, 0xc4, 0xfc } };

/// Interface to the buffer view object implemented in D3D12
class IBufferViewD3D12 : public IBufferView
{
public:

    /// Returns CPU descriptor handle of the internal Direct3D12 object.
    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle() = 0;
};

}
