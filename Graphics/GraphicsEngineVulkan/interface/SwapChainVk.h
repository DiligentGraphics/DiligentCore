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
/// Definition of the Diligent::ISwapChainVk interface

#include "vulkan.h"

#include "../../GraphicsEngine/interface/SwapChain.h"
#include "TextureViewVk.h"

namespace Diligent
{

// {22A39881-5EC5-4A9C-8395-90215F04A5CC}
static constexpr INTERFACE_ID IID_SwapChainVk =
{ 0x22a39881, 0x5ec5, 0x4a9c,{ 0x83, 0x95, 0x90, 0x21, 0x5f, 0x4, 0xa5, 0xcc } };

/// Interface to the swap chain object implemented in Vulkan
class ISwapChainVk : public ISwapChain
{
public:

    /// Returns a pointer to the IDXGISwapChain interface of the internal DXGI object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    //virtual IDXGISwapChain *GetDXGISwapChain() = 0;

    /// Returns a pointer to the render target view of the current back buffer in the swap chain

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    //virtual ITextureViewD3D12* GetCurrentBackBufferRTV() = 0;

    /// Returns a pointer to the depth-stencil view of the depth buffer

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    //virtual ITextureViewD3D12* GetDepthBufferDSV() = 0;
};

}
