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
/// Definition of the Diligent::IBufferD3D11 interface

#include "Buffer.h"

namespace Diligent
{

// {4A696D2E-44BB-4C4B-9DE2-3AF7C94DCFC0}
static const Diligent::INTERFACE_ID IID_BufferD3D11 =
{ 0x4a696d2e, 0x44bb, 0x4c4b, { 0x9d, 0xe2, 0x3a, 0xf7, 0xc9, 0x4d, 0xcf, 0xc0 } };

/// Interface to the buffer object implemented in D3D11
class IBufferD3D11 : public Diligent::IBuffer
{
public:

    /// Returns a pointer to the ID3D11Buffer interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11Buffer *GetD3D11Buffer() = 0;
};

}
