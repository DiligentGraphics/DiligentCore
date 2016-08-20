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
/// Definition of the Diligent::ISwapChainD3D11 interface

#include "SwapChain.h"

namespace Diligent
{

// {4DAF2E76-9204-4DC4-A53A-B00097412D3A}
static const Diligent::INTERFACE_ID IID_SwapChainD3D11 =
{ 0x4daf2e76, 0x9204, 0x4dc4, { 0xa5, 0x3a, 0xb0, 0x0, 0x97, 0x41, 0x2d, 0x3a } };

/// Interface to the swap chain object implemented in D3D11
class ISwapChainD3D11 : public Diligent::ISwapChain
{
public:

    /// Returns a pointer to the IDXGISwapChain interface of the internal DXGI object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual IDXGISwapChain *GetDXGISwapChain() = 0;
};

}
