/*     Copyright 2019 Diligent Graphics LLC
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

#include "SwapChainGL.h"
#include "SwapChainBase.h"
#include "GLObjectWrapper.h"

namespace Diligent
{

class IMemoryAllocator;

/// Swap chain implementation in OpenGL backend.
class SwapChainGLImpl final : public SwapChainBase<ISwapChainGL>
{
public:
    using TSwapChainBase = SwapChainBase<ISwapChainGL>;

    SwapChainGLImpl(IReferenceCounters*         pRefCounters,
                    const EngineGLCreateInfo&   InitAttribs,
                    const SwapChainDesc&        SwapChainDesc, 
                    class RenderDeviceGLImpl*   pRenderDeviceGL,
                    class DeviceContextGLImpl*  pImmediateContextGL);
    ~SwapChainGLImpl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface )override final;

    /// Implementation of ISwapChain::Present() in OpenGL backend.
    virtual void Present(Uint32 SyncInterval)override final;

    /// Implementation of ISwapChain::Resize() in OpenGL backend.
    virtual void Resize( Uint32 NewWidth, Uint32 NewHeight )override final;

    /// Implementation of ISwapChain::SetFullscreenMode() in OpenGL backend.
    virtual void SetFullscreenMode(const DisplayModeAttribs& DisplayMode)override final;

    /// Implementation of ISwapChain::SetWindowedMode() in OpenGL backend.
    virtual void SetWindowedMode()override final;

    /// Implementation of ISwapChainGL::GetDefaultFBO().
    virtual GLuint GetDefaultFBO()const override final{ return 0; }

    /// Implementation of ISwapChain::GetCurrentBackBufferRTV() in OpenGL backend.
    virtual ITextureView* GetCurrentBackBufferRTV()override final{return m_pRenderTargetView;}

    /// Implementation of ISwapChain::GetDepthBufferDSV() in OpenGL backend.
    virtual ITextureView* GetDepthBufferDSV()override final{return m_pDepthStencilView;}

private:
    void CreateDummyBuffers(RenderDeviceGLImpl* pRenderDeviceGL);

    RefCntAutoPtr<TextureViewGLImpl> m_pRenderTargetView;
    RefCntAutoPtr<TextureViewGLImpl> m_pDepthStencilView;
};

}
