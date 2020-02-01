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

#include "pch.h"
#include "DeviceContextGLImpl.hpp"
#include "RenderDeviceGLImpl.hpp"
#include "SwapChainGLImpl.hpp"

namespace Diligent
{
SwapChainGLImpl::SwapChainGLImpl(IReferenceCounters*       pRefCounters,
                                 const EngineGLCreateInfo& InitAttribs,
                                 const SwapChainDesc&      SCDesc,
                                 RenderDeviceGLImpl*       pRenderDeviceGL,
                                 DeviceContextGLImpl*      pImmediateContextGL) :
    // clang-format off
    TSwapChainGLBase
    {
        pRefCounters,
        pRenderDeviceGL,
        pImmediateContextGL,
        SCDesc
    }
// clang-format on
{
#if PLATFORM_WIN32
    HWND hWnd = reinterpret_cast<HWND>(InitAttribs.Window.hWnd);
    RECT rc;
    GetClientRect(hWnd, &rc);
    m_SwapChainDesc.Width  = rc.right - rc.left;
    m_SwapChainDesc.Height = rc.bottom - rc.top;
#elif PLATFORM_LINUX
    auto wnd     = static_cast<Window>(reinterpret_cast<size_t>(InitAttribs.Window.pWindow));
    auto display = reinterpret_cast<Display*>(InitAttribs.Window.pDisplay);

    XWindowAttributes XWndAttribs;
    XGetWindowAttributes(display, wnd, &XWndAttribs);

    m_SwapChainDesc.Width  = XWndAttribs.width;
    m_SwapChainDesc.Height = XWndAttribs.height;
#elif PLATFORM_ANDROID
    auto& GLContext        = pRenderDeviceGL->m_GLContext;
    m_SwapChainDesc.Width  = GLContext.GetScreenWidth();
    m_SwapChainDesc.Height = GLContext.GetScreenHeight();
#elif PLATFORM_MACOS
    //Set dummy width and height until resize is called by the app
    m_SwapChainDesc.Width  = 1024;
    m_SwapChainDesc.Height = 768;
#else
#    error Unsupported platform
#endif

    CreateDummyBuffers(pRenderDeviceGL);
}

SwapChainGLImpl::~SwapChainGLImpl()
{
}

IMPLEMENT_QUERY_INTERFACE(SwapChainGLImpl, IID_SwapChainGL, TSwapChainGLBase)

void SwapChainGLImpl::Present(Uint32 SyncInterval)
{
#if PLATFORM_WIN32 || PLATFORM_LINUX || PLATFORM_ANDROID
    auto* pDeviceGL = m_pRenderDevice.RawPtr<RenderDeviceGLImpl>();
    auto& GLContext = pDeviceGL->m_GLContext;
    GLContext.SwapBuffers();
#elif PLATFORM_MACOS
    LOG_ERROR("Swap buffers operation must be performed by the app on MacOS");
#else
#    error Unsupported platform
#endif

    // Unbind back buffer from device context to be consistent with other backends
    if (auto pDeviceContext = m_wpDeviceContext.Lock())
    {
        auto* pDeviceCtxGl = pDeviceContext.RawPtr<DeviceContextGLImpl>();
        auto* pBackBuffer  = ValidatedCast<TextureBaseGL>(m_pRenderTargetView->GetTexture());
        pDeviceCtxGl->UnbindTextureFromFramebuffer(pBackBuffer, false);
    }
}

void SwapChainGLImpl::Resize(Uint32 NewWidth, Uint32 NewHeight)
{
#if PLATFORM_ANDROID
    auto* pDeviceGL = m_pRenderDevice.RawPtr<RenderDeviceGLImpl>();
    auto& GLContext = pDeviceGL->m_GLContext;
    GLContext.UpdateScreenSize();
    NewWidth  = GLContext.GetScreenWidth();
    NewHeight = GLContext.GetScreenHeight();
#endif

    TSwapChainGLBase::Resize(NewWidth, NewHeight, 0);
}

void SwapChainGLImpl::SetFullscreenMode(const DisplayModeAttribs& DisplayMode)
{
    UNSUPPORTED("OpenGL does not support switching to the fullscreen mode");
}

void SwapChainGLImpl::SetWindowedMode()
{
    UNSUPPORTED("OpenGL does not support switching to the windowed mode");
}

} // namespace Diligent
