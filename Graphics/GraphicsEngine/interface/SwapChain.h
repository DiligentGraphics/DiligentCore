/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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
/// Definition of the Diligent::ISwapChain interface and related data structures

#include "../../../Primitives/interface/Object.h"
#include "TextureView.h"
#include "GraphicsTypes.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)


// {1C703B77-6607-4EEC-B1FE-15C82D3B4130}
static const struct INTERFACE_ID IID_SwapChain =
    {0x1c703b77, 0x6607, 0x4eec, {0xb1, 0xfe, 0x15, 0xc8, 0x2d, 0x3b, 0x41, 0x30}};


#if DILIGENT_CPP_INTERFACE

/// Swap chain interface

/// The swap chain is created by a platform-dependent function
class ISwapChain : public IObject
{
public:
    /// Presents a rendered image to the user
    virtual void Present(Uint32 SyncInterval = 1) = 0;

    /// Returns the swap chain desctription
    virtual const SwapChainDesc& GetDesc() const = 0;

    /// Changes the swap chain's back buffer size

    /// \param [in] NewWidth - New swap chain width, in pixels
    /// \param [in] NewHeight - New swap chain height, in pixels
    ///
    /// \note When resizing non-primary swap chains, the engine unbinds the
    ///       swap chain buffers from the output.
    virtual void Resize(Uint32 NewWidth, Uint32 NewHeight) = 0;

    /// Sets fullscreen mode (only supported on Win32 platform)
    virtual void SetFullscreenMode(const DisplayModeAttribs& DisplayMode) = 0;

    /// Sets windowed mode (only supported on Win32 platform)
    virtual void SetWindowedMode() = 0;

    /// Returns render target view of the current back buffer in the swap chain

    /// \note For Direct3D12 and Vulkan backends, the function returns
    /// different pointer for every offscreen buffer in the swap chain
    /// (flipped by every call to ISwapChain::Present()). For Direct3D11
    /// backend it always returns the same pointer. For OpenGL/GLES backends
    /// the method returns null.
    ///
    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ITextureView* GetCurrentBackBufferRTV() = 0;

    /// Returns depth-stencil view of the depth buffer

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ITextureView* GetDepthBufferDSV() = 0;
};

#else

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
