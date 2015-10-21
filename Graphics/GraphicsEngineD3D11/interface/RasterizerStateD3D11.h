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
/// Definition of the Diligent::IRasterizerStateD3D11 interface

#include "RasterizerState.h"

namespace Diligent
{

// {10E96D54-1A17-4AB4-8A87-CD39211B348D}
static const Diligent::INTERFACE_ID IID_RasterizerStateD3D11 =
{ 0x10e96d54, 0x1a17, 0x4ab4, { 0x8a, 0x87, 0xcd, 0x39, 0x21, 0x1b, 0x34, 0x8d } };

/// Interface to the rasterizer state object implemented in D3D11
class IRasterizerStateD3D11 : public Diligent::IRasterizerState
{
public:

    /// Returns a pointer to the ID3D11RasterizerState interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11RasterizerState *GetD3D11RasterizerState() = 0;
};

}
