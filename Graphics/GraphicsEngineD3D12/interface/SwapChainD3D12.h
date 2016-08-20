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
/// Definition of the Diligent::ISwapChainD3D12 interface

#include "SwapChain.h"

namespace Diligent
{

// {C9F8384D-A45E-4970-8447-394177E5B0EE}
static const Diligent::INTERFACE_ID IID_SwapChainD3D12 =
{ 0xc9f8384d, 0xa45e, 0x4970, { 0x84, 0x47, 0x39, 0x41, 0x77, 0xe5, 0xb0, 0xee } };

/// Interface to the swap chain object implemented in D3D12
class ISwapChainD3D12 : public Diligent::ISwapChain
{
public:

    /// Returns a pointer to the IDXGISwapChain interface of the internal DXGI object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual IDXGISwapChain *GetDXGISwapChain() = 0;
};

}
