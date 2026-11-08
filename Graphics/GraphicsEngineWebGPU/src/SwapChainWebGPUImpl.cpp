/*
 *  Copyright 2023-2026 Diligent Graphics LLC
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

#include "SwapChainWebGPUImpl.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "DeviceContextWebGPUImpl.hpp"
#include "TextureWebGPUImpl.hpp"
#include "WebGPUTypeConversions.hpp"

#if PLATFORM_WIN32
#    include <Windows.h>
#endif

#if PLATFORM_WEB
#    include <emscripten/html5.h>
#endif

namespace Diligent
{

SwapChainWebGPUImpl::SwapChainWebGPUImpl(IReferenceCounters*      pRefCounters,
                                         const SwapChainDesc&     SCDesc,
                                         RenderDeviceWebGPUImpl*  pRenderDevice,
                                         DeviceContextWebGPUImpl* pDeviceContext,
                                         const NativeWindow&      Window) :
    TSwapChainBase{pRefCounters, pRenderDevice, pDeviceContext, SCDesc},
    m_NativeWindow{Window}
{
    if (m_DesiredPreTransform != SURFACE_TRANSFORM_OPTIMAL && m_DesiredPreTransform != SURFACE_TRANSFORM_IDENTITY)
    {
        LOG_WARNING_MESSAGE(GetSurfaceTransformString(m_DesiredPreTransform),
                            " is not an allowed pretransform because WebGPU swap chains only support identity transform. "
                            "Use SURFACE_TRANSFORM_OPTIMAL (recommended) or SURFACE_TRANSFORM_IDENTITY.");
    }
    m_DesiredPreTransform        = SURFACE_TRANSFORM_OPTIMAL;
    m_SwapChainDesc.PreTransform = SURFACE_TRANSFORM_IDENTITY;

    if (!CreateSurface() || !ConfigureSurface() || !CreateDepthBufferView())
        LOG_ERROR_AND_THROW("Failed to initialize WebGPU swap chain");

    // Acquire lazily: a browser canvas texture expires at the end of its rendering frame.
}

SwapChainWebGPUImpl::~SwapChainWebGPUImpl()
{
    ReleaseSwapChainResources();
    if (m_wgpuSurface)
        wgpuSurfaceUnconfigure(m_wgpuSurface);
}

ITextureViewWebGPU* SwapChainWebGPUImpl::GetCurrentBackBufferRTV()
{
    if (m_pBackBufferRTV)
        return m_pBackBufferRTV;

    try
    {
        if ((!m_SurfaceConfigured || m_ReconfigureSurface) && !RecreateSwapChain())
            return nullptr;

        if (!AcquireSurfaceTexture())
            return nullptr;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MESSAGE("Failed to acquire WebGPU swap chain texture: ", e.what());
        return nullptr;
    }

    return m_pBackBufferRTV;
}

void SwapChainWebGPUImpl::Present(Uint32 SyncInterval)
{
    if (SyncInterval != 0 && SyncInterval != 1)
        LOG_WARNING_MESSAGE_ONCE("WebGPU only supports 0 and 1 present intervals");

    RefCntAutoPtr<IDeviceContext> pDeviceContext = m_wpDeviceContext.Lock();
    if (!pDeviceContext)
    {
        LOG_ERROR_MESSAGE("Immediate context has been released");
        return;
    }

    DeviceContextWebGPUImpl* pImmediateCtx = pDeviceContext.RawPtr<DeviceContextWebGPUImpl>();
    pImmediateCtx->Flush();

    if (m_pBackBufferRTV)
    {
        pImmediateCtx->UnbindTextureFromFramebuffer(ClassPtrCast<TextureWebGPUImpl>(m_pBackBufferRTV->GetTexture()), false);
#if !PLATFORM_WEB
        if (wgpuSurfacePresent(m_wgpuSurface) != WGPUStatus_Success)
            m_ReconfigureSurface = true;
#endif
    }

    // The browser presents when control returns to its event loop. Do not acquire the
    // next texture or reconfigure the canvas here, as that would invalidate this frame.
    m_pBackBufferRTV.Release();
    m_wgpuCurrentTexture.Reset();

    if (m_SwapChainDesc.IsPrimary)
    {
        pImmediateCtx->FinishFrame();
        m_pRenderDevice->ReleaseStaleResources();
    }

    const bool EnableVSync = SyncInterval != 0;
    if (m_VSyncEnabled != EnableVSync)
    {
        m_VSyncEnabled       = EnableVSync;
        m_ReconfigureSurface = true;
    }
}

void SwapChainWebGPUImpl::Resize(Uint32            NewWidth,
                                 Uint32            NewHeight,
                                 SURFACE_TRANSFORM NewPreTransform)
{
    if (TSwapChainBase::Resize(NewWidth, NewHeight, NewPreTransform))
        RecreateSwapChain();
}

void SwapChainWebGPUImpl::SetFullscreenMode(const DisplayModeAttribs& DisplayMode)
{
    UNSUPPORTED("WebGPU does not support switching to the fullscreen mode");
}

void SwapChainWebGPUImpl::SetWindowedMode()
{
    UNSUPPORTED("WebGPU does not support switching to the windowed mode");
}

bool SwapChainWebGPUImpl::CreateSurface()
{
    const RenderDeviceWebGPUImpl* pRenderDeviceWebGPU = m_pRenderDevice.RawPtr<RenderDeviceWebGPUImpl>();

#if PLATFORM_WIN32
    WGPUSurfaceSourceWindowsHWND wgpuSurfaceNativeDesc{};
    wgpuSurfaceNativeDesc.chain     = {nullptr, WGPUSType_SurfaceSourceWindowsHWND};
    wgpuSurfaceNativeDesc.hwnd      = m_NativeWindow.hWnd;
    wgpuSurfaceNativeDesc.hinstance = GetModuleHandle(nullptr);
#elif PLATFORM_LINUX
    WGPUSurfaceSourceXCBWindow wgpuSurfaceNativeDesc{};
    wgpuSurfaceNativeDesc.chain      = {nullptr, WGPUSType_SurfaceSourceXCBWindow};
    wgpuSurfaceNativeDesc.connection = m_NativeWindow.pXCBConnection;
    wgpuSurfaceNativeDesc.window     = m_NativeWindow.WindowId;
#elif PLATFORM_MACOS
    WGPUSurfaceSourceMetalLayer wgpuSurfaceNativeDesc{};
    wgpuSurfaceNativeDesc.chain = {nullptr, WGPUSType_SurfaceSourceMetalLayer};
    wgpuSurfaceNativeDesc.layer = m_NativeWindow.GetLayer();
#elif PLATFORM_WEB
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector wgpuSurfaceNativeDesc{};
    wgpuSurfaceNativeDesc.chain    = {nullptr, WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector};
    wgpuSurfaceNativeDesc.selector = GetWGPUStringView(m_NativeWindow.pCanvasId);
#endif

    WGPUSurfaceDescriptor wgpuSurfaceDesc{};
    wgpuSurfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuSurfaceNativeDesc);

    m_wgpuSurface.Reset(wgpuInstanceCreateSurface(pRenderDeviceWebGPU->GetWebGPUInstance(), &wgpuSurfaceDesc));
    if (!m_wgpuSurface)
    {
        LOG_ERROR_MESSAGE("Failed to create OS-specific surface");
        return false;
    }
    return true;
}

bool SwapChainWebGPUImpl::ConfigureSurface()
{
    m_SurfaceConfigured                   = false;
    const RenderDeviceWebGPUImpl* pDevice = m_pRenderDevice.RawPtr<RenderDeviceWebGPUImpl>();

    struct SurfaceCapabilities
    {
        WGPUSurfaceCapabilities wgpuSurfaceCaps{};
        ~SurfaceCapabilities() { wgpuSurfaceCapabilitiesFreeMembers(wgpuSurfaceCaps); }
    } Capabilities;
    WGPUSurfaceCapabilities& wgpuSurfaceCaps = Capabilities.wgpuSurfaceCaps;
    if (wgpuSurfaceGetCapabilities(m_wgpuSurface, pDevice->GetWebGPUAdapter(), &wgpuSurfaceCaps) != WGPUStatus_Success ||
        wgpuSurfaceCaps.formatCount == 0 || wgpuSurfaceCaps.presentModeCount == 0 || wgpuSurfaceCaps.alphaModeCount == 0)
    {
        LOG_ERROR_MESSAGE("Failed to query WebGPU surface capabilities");
        return false;
    }

    const bool        IsSRGB            = IsSRGBFormat(m_SwapChainDesc.ColorBufferFormat);
    WGPUTextureFormat wgpuSurfaceFormat = TextureFormatToWGPUFormat(SRGBFormatToUnorm(m_SwapChainDesc.ColorBufferFormat));
    auto              SupportsFormat    = [&](WGPUTextureFormat wgpuFormat) {
        return std::find(wgpuSurfaceCaps.formats, wgpuSurfaceCaps.formats + wgpuSurfaceCaps.formatCount, wgpuFormat) != wgpuSurfaceCaps.formats + wgpuSurfaceCaps.formatCount;
    };
    if (!SupportsFormat(wgpuSurfaceFormat))
    {
        // Match Vulkan's RGBA/BGRA fallback without changing precision or sRGB encoding.
        WGPUTextureFormat wgpuReplacementFormat = WGPUTextureFormat_Undefined;
        if (wgpuSurfaceFormat == WGPUTextureFormat_RGBA8Unorm)
            wgpuReplacementFormat = WGPUTextureFormat_BGRA8Unorm;
        else if (wgpuSurfaceFormat == WGPUTextureFormat_BGRA8Unorm)
            wgpuReplacementFormat = WGPUTextureFormat_RGBA8Unorm;

        if (wgpuReplacementFormat == WGPUTextureFormat_Undefined || !SupportsFormat(wgpuReplacementFormat))
        {
            LOG_ERROR_MESSAGE("Requested color buffer format ", GetTextureFormatAttribs(m_SwapChainDesc.ColorBufferFormat).Name,
                              " is not supported by the WebGPU surface");
            return false;
        }

        wgpuSurfaceFormat        = wgpuReplacementFormat;
        TEXTURE_FORMAT NewFormat = WGPUFormatToTextureFormat(wgpuReplacementFormat);
        if (IsSRGB)
            NewFormat = UnormFormatToSRGB(NewFormat);
        LOG_INFO_MESSAGE("Requested color buffer format ", GetTextureFormatAttribs(m_SwapChainDesc.ColorBufferFormat).Name,
                         " is not supported by the surface and will be replaced with ", GetTextureFormatAttribs(NewFormat).Name);
        m_SwapChainDesc.ColorBufferFormat = NewFormat;
    }

    static_assert(SWAP_CHAIN_USAGE_LAST == 8, "Please update this function to handle the new swap chain usage");
    if ((m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_RENDER_TARGET) == 0 ||
        (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_INPUT_ATTACHMENT) != 0)
    {
        LOG_ERROR_MESSAGE("WebGPU swap chains require render target usage and do not support input attachments");
        return false;
    }

    WGPUTextureUsage wgpuUsage = WGPUTextureUsage_RenderAttachment;
    if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_SHADER_RESOURCE)
        wgpuUsage |= WGPUTextureUsage_TextureBinding;
    if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_COPY_SOURCE)
        wgpuUsage |= WGPUTextureUsage_CopySrc;
#if !PLATFORM_WEB
    // Emdawnwebgpu does not populate the surface usage capabilities on the Web.
    if ((wgpuUsage & wgpuSurfaceCaps.usages) != wgpuUsage)
    {
        LOG_ERROR_MESSAGE("Requested swap chain usage is not supported by the WebGPU surface");
        return false;
    }
#endif
    // Preserve copy-destination support where available without requiring it to present.
#if !PLATFORM_WEB
    if ((wgpuSurfaceCaps.usages & WGPUTextureUsage_CopyDst) != 0)
#endif
        wgpuUsage |= WGPUTextureUsage_CopyDst;

    WGPUPresentMode       wgpuPresentMode      = WGPUPresentMode_Fifo;
    const WGPUPresentMode wgpuPreferredModes[] = {WGPUPresentMode_Mailbox, WGPUPresentMode_Immediate, WGPUPresentMode_Fifo};
    if (!m_VSyncEnabled)
    {
        for (WGPUPresentMode wgpuMode : wgpuPreferredModes)
        {
            if (std::find(wgpuSurfaceCaps.presentModes, wgpuSurfaceCaps.presentModes + wgpuSurfaceCaps.presentModeCount, wgpuMode) != wgpuSurfaceCaps.presentModes + wgpuSurfaceCaps.presentModeCount)
            {
                wgpuPresentMode = wgpuMode;
                break;
            }
        }
    }

    if (m_SwapChainDesc.Width == 0 || m_SwapChainDesc.Height == 0)
    {
#if PLATFORM_WIN32
        RECT WindowRect{};
        GetClientRect(static_cast<HWND>(m_NativeWindow.hWnd), &WindowRect);
        m_SwapChainDesc.Width  = WindowRect.right - WindowRect.left;
        m_SwapChainDesc.Height = WindowRect.bottom - WindowRect.top;
#elif PLATFORM_MACOS
        m_NativeWindow.GetBackingSize(m_SwapChainDesc.Width, m_SwapChainDesc.Height);
#elif PLATFORM_WEB
        int CanvasWidth  = 0;
        int CanvasHeight = 0;
        emscripten_get_canvas_element_size(m_NativeWindow.pCanvasId, &CanvasWidth, &CanvasHeight);
        m_SwapChainDesc.Width  = static_cast<Uint32>(CanvasWidth);
        m_SwapChainDesc.Height = static_cast<Uint32>(CanvasHeight);
#endif
        m_SwapChainDesc.Width  = (std::max)(m_SwapChainDesc.Width, 1u);
        m_SwapChainDesc.Height = (std::max)(m_SwapChainDesc.Height, 1u);
    }

    WGPUCompositeAlphaMode wgpuAlphaMode = wgpuSurfaceCaps.alphaModes[0];
    if (std::find(wgpuSurfaceCaps.alphaModes, wgpuSurfaceCaps.alphaModes + wgpuSurfaceCaps.alphaModeCount, WGPUCompositeAlphaMode_Opaque) != wgpuSurfaceCaps.alphaModes + wgpuSurfaceCaps.alphaModeCount)
        wgpuAlphaMode = WGPUCompositeAlphaMode_Opaque;

    const WGPUTextureFormat wgpuViewFormat = TextureFormatToWGPUFormat(m_SwapChainDesc.ColorBufferFormat);

    WGPUSurfaceColorManagement wgpuColorManagement{};
    wgpuColorManagement.chain.sType     = WGPUSType_SurfaceColorManagement;
    wgpuColorManagement.chain.next      = nullptr;
    wgpuColorManagement.colorSpace      = WGPUPredefinedColorSpace_DisplayP3;
    wgpuColorManagement.toneMappingMode = WGPUToneMappingMode_Extended;

    WGPUSurfaceConfiguration wgpuSurfaceConfig{};
    wgpuSurfaceConfig.nextInChain     = wgpuSurfaceFormat != WGPUTextureFormat_RGBA16Float ? nullptr : &wgpuColorManagement.chain;
    wgpuSurfaceConfig.device          = pDevice->GetWebGPUDevice();
    wgpuSurfaceConfig.usage           = wgpuUsage;
    wgpuSurfaceConfig.width           = m_SwapChainDesc.Width;
    wgpuSurfaceConfig.height          = m_SwapChainDesc.Height;
    wgpuSurfaceConfig.format          = wgpuSurfaceFormat;
    wgpuSurfaceConfig.presentMode     = wgpuPresentMode;
    wgpuSurfaceConfig.alphaMode       = wgpuAlphaMode;
    wgpuSurfaceConfig.viewFormatCount = IsSRGB ? 1 : 0;
    wgpuSurfaceConfig.viewFormats     = IsSRGB ? &wgpuViewFormat : nullptr;

    wgpuSurfaceConfigure(m_wgpuSurface, &wgpuSurfaceConfig);
    m_SurfaceConfigured = true;
    return true;
}

bool SwapChainWebGPUImpl::AcquireSurfaceTexture()
{
    // Retry once after recovering an outdated or lost surface. A timeout skips this
    // acquisition instead of blocking; the next GetCurrentBackBufferRTV() can retry.
    for (Uint32 Attempt = 0; Attempt < 2; ++Attempt)
    {
        WGPUSurfaceTexture wgpuSurfaceTexture{};
        wgpuSurfaceGetCurrentTexture(m_wgpuSurface, &wgpuSurfaceTexture);
        WebGPUTextureWrapper wgpuTexture{wgpuSurfaceTexture.texture};

        switch (wgpuSurfaceTexture.status)
        {
            case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
            case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
                break;

            case WGPUSurfaceGetCurrentTextureStatus_Timeout:
                return false;

            case WGPUSurfaceGetCurrentTextureStatus_Outdated:
            case WGPUSurfaceGetCurrentTextureStatus_Lost:
                wgpuTexture.Reset();
                m_ReconfigureSurface = true;
                if (Attempt == 0 && RecreateSwapChain())
                    continue;
                return false;

            default:
                LOG_ERROR_MESSAGE("Failed to acquire WebGPU surface texture");
                return false;
        }

        if (!wgpuTexture)
        {
            LOG_ERROR_MESSAGE("WebGPU surface returned a null texture");
            return false;
        }

        TextureDesc Desc;
        Desc.Name      = "Swap chain back buffer";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Width     = wgpuTextureGetWidth(wgpuTexture);
        Desc.Height    = wgpuTextureGetHeight(wgpuTexture);
        Desc.Format    = m_SwapChainDesc.ColorBufferFormat;
        Desc.BindFlags = BIND_RENDER_TARGET;
        if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_SHADER_RESOURCE)
            Desc.BindFlags |= BIND_SHADER_RESOURCE;

        RefCntAutoPtr<ITexture> pBackBuffer;
        m_pRenderDevice.RawPtr<RenderDeviceWebGPUImpl>()->CreateTextureFromWebGPUTexture(wgpuTexture, Desc, RESOURCE_STATE_UNDEFINED, &pBackBuffer);
        if (!pBackBuffer)
            return false;
        RefCntAutoPtr<ITextureViewWebGPU> pRTV{pBackBuffer->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET), IID_TextureViewWebGPU};
        if (!pRTV)
        {
            LOG_ERROR_MESSAGE("Failed to create WebGPU surface render target view");
            return false;
        }

        // Imported textures borrow their native handle. Retain the acquired reference
        // until all rendering for this frame has been submitted and presented.
        m_wgpuCurrentTexture = std::move(wgpuTexture);
        m_pBackBufferRTV     = std::move(pRTV);
        m_ReconfigureSurface = wgpuSurfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;
        return true;
    }
    return false;
}

bool SwapChainWebGPUImpl::CreateDepthBufferView()
{
    if (m_SwapChainDesc.DepthBufferFormat == TEX_FORMAT_UNKNOWN)
        return true;

    TextureDesc Desc;
    Desc.Name                            = "Main depth buffer";
    Desc.Type                            = RESOURCE_DIM_TEX_2D;
    Desc.Width                           = m_SwapChainDesc.Width;
    Desc.Height                          = m_SwapChainDesc.Height;
    Desc.Format                          = m_SwapChainDesc.DepthBufferFormat;
    Desc.BindFlags                       = BIND_DEPTH_STENCIL;
    Desc.ClearValue.Format               = Desc.Format;
    Desc.ClearValue.DepthStencil.Depth   = m_SwapChainDesc.DefaultDepthValue;
    Desc.ClearValue.DepthStencil.Stencil = m_SwapChainDesc.DefaultStencilValue;
    RefCntAutoPtr<ITexture> pDepthBuffer;
    m_pRenderDevice->CreateTexture(Desc, nullptr, &pDepthBuffer);
    if (!pDepthBuffer)
        return false;
    m_pDepthBufferDSV = RefCntAutoPtr<ITextureViewWebGPU>{pDepthBuffer->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL), IID_TextureViewWebGPU};
    return m_pDepthBufferDSV != nullptr;
}

void SwapChainWebGPUImpl::ReleaseSwapChainResources()
{
    if (RefCntAutoPtr<IDeviceContext> pContext = m_wpDeviceContext.Lock())
    {
        // Reconfiguration invalidates acquired textures. Submit any pending clears
        // and draws before releasing them, and remove stale framebuffer bindings.
        DeviceContextWebGPUImpl* pImmediateCtx = pContext.RawPtr<DeviceContextWebGPUImpl>();
        if (m_pBackBufferRTV || m_pDepthBufferDSV)
            pImmediateCtx->Flush();
        if (m_pBackBufferRTV)
            pImmediateCtx->UnbindTextureFromFramebuffer(ClassPtrCast<TextureWebGPUImpl>(m_pBackBufferRTV->GetTexture()), false);
        if (m_pDepthBufferDSV)
            pImmediateCtx->UnbindTextureFromFramebuffer(ClassPtrCast<TextureWebGPUImpl>(m_pDepthBufferDSV->GetTexture()), false);
    }
    m_pBackBufferRTV.Release();
    m_wgpuCurrentTexture.Reset();
    m_pDepthBufferDSV.Release();
}

bool SwapChainWebGPUImpl::RecreateSwapChain()
{
    m_ReconfigureSurface = true;
    try
    {
        ReleaseSwapChainResources();
        if (!ConfigureSurface() || !CreateDepthBufferView())
            return false;
        m_ReconfigureSurface = false;
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MESSAGE("Failed to recreate WebGPU swap chain: ", e.what());
        return false;
    }
}

} // namespace Diligent
