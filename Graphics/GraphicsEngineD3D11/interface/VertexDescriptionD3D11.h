/*     Copyright 2015 Egor Yusov
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
/// Definition of the Diligent::IVertexDescriptionD3D11 interface

#include "VertexDescription.h"

namespace Diligent
{

// {7BCB2412-60EB-451C-9167-F31B869484C1}
static const Diligent::INTERFACE_ID IID_VertexDescriptionD3D11 =
{ 0x7bcb2412, 0x60eb, 0x451c, { 0x91, 0x67, 0xf3, 0x1b, 0x86, 0x94, 0x84, 0xc1 } };

/// Interface to the vertex description object implemented in D3D11
class IVertexDescriptionD3D11 : public Diligent::IVertexDescription
{
public:

    /// Returns a pointer to the ID3D11InputLayout interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11InputLayout* GetD3D11InputLayout() = 0;
};

}
