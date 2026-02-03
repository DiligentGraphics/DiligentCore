/*
 *  Copyright 2023-2025 Diligent Graphics LLC
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
#include "TextureViewWebGPU.h"
#include "WebGPUTypeConversions.hpp"
#include "WebGPUStubs.hpp"
#include "webgpu/webgpu.h"
#include <cstdint>

#ifdef PLATFORM_WIN32
#    include <Windows.h>
#endif

#if PLATFORM_WEB
#    include <emscripten/html5.h>
#endif

namespace Diligent
{

namespace
{

WGPUTextureFormat WGPUConvertUnormToSRGB(WGPUTextureFormat Format)
{
    switch (Format)
    {
        case WGPUTextureFormat_RGBA8Unorm:
            return WGPUTextureFormat_RGBA8UnormSrgb;
        case WGPUTextureFormat_BGRA8Unorm:
            return WGPUTextureFormat_BGRA8UnormSrgb;
        default:
            UNEXPECTED("Unexpected texture format");
            return Format;
    }
}

} // namespace



SwapChainWebGPUImpl::SwapChainWebGPUImpl(IReferenceCounters*      pRefCounters,
                                         const SwapChainDesc&     SCDesc,
                                         RenderDeviceWebGPUImpl*  pRenderDevice,
                                         DeviceContextWebGPUImpl* pDeviceContext,
                                         const NativeWindow&      Window) :
    // clang-format off
    TSwapChainBase
    {
        pRefCounters,
        pRenderDevice,
        pDeviceContext,
        SCDesc
    },
    m_NativeWindow{Window}
// clang-format on
{
    if (m_DesiredPreTransform != SURFACE_TRANSFORM_OPTIMAL && m_DesiredPreTransform != SURFACE_TRANSFORM_IDENTITY)
    {
        LOG_WARNING_MESSAGE(GetSurfaceTransformString(m_DesiredPreTransform),
                            " is not an allowed pretransform because WebGPU swap chains only support identity transform. "
                            "Use SURFACE_TRANSFORM_OPTIMAL (recommended) or SURFACE_TRANSFORM_IDENTITY.");
    }
    m_DesiredPreTransform        = SURFACE_TRANSFORM_OPTIMAL;
    m_SwapChainDesc.PreTransform = SURFACE_TRANSFORM_IDENTITY;

    CreateSurface();
    ConfigureSurface();
    CreateDepthBufferView();

    WGPUSurfaceGetCurrentTextureStatus wgpuStatus = AcquireSurfaceTexture();
    if (wgpuStatus != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        wgpuStatus != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    {
        LOG_ERROR_MESSAGE("Failed to acquire the initial swap chain surface texture");
    }
}

SwapChainWebGPUImpl::~SwapChainWebGPUImpl() = default;

void SwapChainWebGPUImpl::Present(Uint32 SyncInterval)
{
    m_RequestedSyncInterval = SyncInterval;
    if (SyncInterval != 0 && SyncInterval != 1)
        LOG_WARNING_MESSAGE_ONCE("WebGPU only supports 0 and 1 present intervals");

    RefCntAutoPtr<IDeviceContext> pDeviceContext = m_wpDeviceContext.Lock();
    if (!pDeviceContext)
    {
        LOG_ERROR_MESSAGE("Immediate context has been released");
        return;
    }
    pDeviceContext->Flush();

#if PLATFORM_WEB

    emscripten_request_animation_frame([](double Time, void* pUserData) -> EM_BOOL { 
        auto* const pSwapChain = static_cast<SwapChainWebGPUImpl*>(pUserData);
        pSwapChain->RequestAnimationFrame(pSwapChain->m_RequestedSyncInterval);
        return EM_FALSE; }, this);
#else
    wgpuSurfacePresent(m_wgpuSurface);
    RequestAnimationFrame(m_RequestedSyncInterval);
#endif
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

void SwapChainWebGPUImpl::CreateSurface()
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
    WGPUSurfaceSourceCanvasHTMLSelector_Emscripten wgpuSurfaceNativeDesc{};
    wgpuSurfaceNativeDesc.chain    = {nullptr, WGPUSType_SurfaceSourceCanvasHTMLSelector_Emscripten};
    wgpuSurfaceNativeDesc.selector = GetWGPUStringView(m_NativeWindow.pCanvasId);
#endif

    WGPUSurfaceDescriptor wgpuSurfaceDesc{};
    wgpuSurfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuSurfaceNativeDesc);

    m_wgpuSurface.Reset(wgpuInstanceCreateSurface(pRenderDeviceWebGPU->GetWebGPUInstance(), &wgpuSurfaceDesc));
    if (!m_wgpuSurface)
    {
        LOG_ERROR_MESSAGE("Failed to create OS-specific surface");
    }
}

void SwapChainWebGPUImpl::ConfigureSurface()
{
    const RenderDeviceWebGPUImpl* pRenderDeviceWebGPU = m_pRenderDevice.RawPtr<RenderDeviceWebGPUImpl>();

    WGPUSurfaceCapabilities wgpuSurfaceCapabilities{};
    wgpuSurfaceGetCapabilities(m_wgpuSurface.Get(), pRenderDeviceWebGPU->GetWebGPUAdapter(), &wgpuSurfaceCapabilities);

    auto SelectPresentMode = [&](bool IsVSyncEnabled) -> WGPUPresentMode {
        WGPUPresentMode Result = WGPUPresentMode_Fifo;

        std::vector<WGPUPresentMode> PreferredPresentModes;
        if (IsVSyncEnabled)
        {
            PreferredPresentModes.push_back(WGPUPresentMode_Fifo);
        }
        else
        {
            PreferredPresentModes.push_back(WGPUPresentMode_Mailbox);
            PreferredPresentModes.push_back(WGPUPresentMode_Immediate);
            PreferredPresentModes.push_back(WGPUPresentMode_Fifo);
        }

        const WGPUPresentMode* FindBegin = wgpuSurfaceCapabilities.presentModes;
        const WGPUPresentMode* FindEnd   = wgpuSurfaceCapabilities.presentModes + wgpuSurfaceCapabilities.presentModeCount;

        for (WGPUPresentMode PreferredMode : PreferredPresentModes)
        {
            if (std::find(FindBegin, FindEnd, PreferredMode) != FindEnd)
            {
                Result = PreferredMode;
                break;
            }
        }

        return Result;
    };

    auto SelectUsage = [&](SWAP_CHAIN_USAGE_FLAGS Flags) -> WGPUTextureUsage {
        WGPUTextureUsage Result = {};

        DEV_CHECK_ERR(Flags != 0, "No swap chain usage flags defined");
        static_assert(SWAP_CHAIN_USAGE_LAST == 8, "Please update this function to handle the new swap chain usage");

        if (Flags & SWAP_CHAIN_USAGE_RENDER_TARGET)
            Result = static_cast<WGPUTextureUsage>(Result | WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst);
        if (Flags & SWAP_CHAIN_USAGE_SHADER_RESOURCE)
            Result = static_cast<WGPUTextureUsage>(Result | WGPUTextureUsage_TextureBinding);
        if (Flags & SWAP_CHAIN_USAGE_COPY_SOURCE)
            Result = static_cast<WGPUTextureUsage>(Result | WGPUTextureUsage_CopySrc);

        return Result;
    };

    auto SelectFormat = [&](TEXTURE_FORMAT Format) {
        WGPUTextureFormat wgpuFormat = TextureFormatToWGPUFormat(SRGBFormatToUnorm(Format));

        const WGPUTextureFormat* FindBegin = wgpuSurfaceCapabilities.formats;
        const WGPUTextureFormat* FindEnd   = wgpuSurfaceCapabilities.formats + wgpuSurfaceCapabilities.formatCount;

        if (std::find(FindBegin, FindEnd, wgpuFormat) != FindEnd)
        {
            return wgpuFormat;
        }
        else
        {
            LOG_ERROR_MESSAGE("Failed to find the requested format in the surface capabilities. Using the first available format instead.");
            return wgpuSurfaceCapabilities.formats[0];
        }
    };

    if (m_SwapChainDesc.Width == 0 || m_SwapChainDesc.Height == 0)
    {
#if PLATFORM_WIN32
        RECT WindowRect;
        GetClientRect(static_cast<HWND>(m_NativeWindow.hWnd), &WindowRect);

        m_SwapChainDesc.Width  = WindowRect.right - WindowRect.left;
        m_SwapChainDesc.Height = WindowRect.bottom - WindowRect.top;
#elif PLATFORM_WEB
        int32_t CanvasWidth  = 0;
        int32_t CanvasHeight = 0;
        emscripten_get_canvas_element_size(m_NativeWindow.pCanvasId, &CanvasWidth, &CanvasHeight);

        m_SwapChainDesc.Width  = static_cast<Uint32>(CanvasWidth);
        m_SwapChainDesc.Height = static_cast<Uint32>(CanvasHeight);
#endif

        m_SwapChainDesc.Width  = (std::max)(m_SwapChainDesc.Width, 1u);
        m_SwapChainDesc.Height = (std::max)(m_SwapChainDesc.Height, 1u);
    }

    const WGPUTextureFormat wgpuPreferredFormat = SelectFormat(m_SwapChainDesc.ColorBufferFormat);

    std::vector<WGPUTextureFormat> wgpuRTVFormats;
    wgpuRTVFormats.push_back(wgpuPreferredFormat);
    if (IsSRGBFormat(m_SwapChainDesc.ColorBufferFormat))
        wgpuRTVFormats.push_back(WGPUConvertUnormToSRGB(wgpuPreferredFormat));

    WGPUSurfaceColorManagement wgpuColorManagement{};
    wgpuColorManagement.chain.sType     = WGPUSType_SurfaceColorManagement;
    wgpuColorManagement.chain.next      = nullptr;
    wgpuColorManagement.colorSpace      = WGPUPredefinedColorSpace_DisplayP3;
    wgpuColorManagement.toneMappingMode = WGPUToneMappingMode_Extended;

    WGPUSurfaceConfiguration wgpuSurfaceConfig{};
    wgpuSurfaceConfig.nextInChain     = wgpuPreferredFormat != WGPUTextureFormat_RGBA16Float ? nullptr : &wgpuColorManagement.chain;
    wgpuSurfaceConfig.device          = pRenderDeviceWebGPU->GetWebGPUDevice();
    wgpuSurfaceConfig.usage           = SelectUsage(m_SwapChainDesc.Usage);
    wgpuSurfaceConfig.width           = m_SwapChainDesc.Width;
    wgpuSurfaceConfig.height          = m_SwapChainDesc.Height;
    wgpuSurfaceConfig.format          = wgpuPreferredFormat;
    wgpuSurfaceConfig.presentMode     = SelectPresentMode(m_VSyncEnabled);
    wgpuSurfaceConfig.alphaMode       = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfig.viewFormats     = wgpuRTVFormats.data();
    wgpuSurfaceConfig.viewFormatCount = static_cast<uint32_t>(wgpuRTVFormats.size());

    wgpuSurfaceConfigure(m_wgpuSurface, &wgpuSurfaceConfig);
    wgpuSurfaceCapabilitiesFreeMembers(wgpuSurfaceCapabilities);
}

WGPUSurfaceGetCurrentTextureStatus SwapChainWebGPUImpl::AcquireSurfaceTexture()
{
    m_pBackBufferRTV.Release();

    WGPUSurfaceTexture wgpuSurfaceTexture{};
    wgpuSurfaceGetCurrentTexture(m_wgpuSurface, &wgpuSurfaceTexture);

    WebGPUTextureWrapper wgpuTextureWrapper{wgpuSurfaceTexture.texture};

    switch (wgpuSurfaceTexture.status)
    {
        case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
        case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
            break;

        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
        case WGPUSurfaceGetCurrentTextureStatus_Timeout:
            return wgpuSurfaceTexture.status;

        case WGPUSurfaceGetCurrentTextureStatus_Lost:
            LOG_WARNING_MESSAGE("Unable to present: swap chain surface is lost");
            return wgpuSurfaceTexture.status;

        case WGPUSurfaceGetCurrentTextureStatus_Error:
            LOG_ERROR_MESSAGE("Unable to present: unknown error");
            return wgpuSurfaceTexture.status;

        default:
            UNEXPECTED("Unexpected status");
            return wgpuSurfaceTexture.status;
    }

    WGPUTextureFormat wgpuFormat        = wgpuTextureGetFormat(wgpuTextureWrapper.Get());
    uint32_t          wgpuSurfaceWidth  = wgpuTextureGetWidth(wgpuTextureWrapper.Get());
    uint32_t          wgpuSurfaceHeight = wgpuTextureGetHeight(wgpuTextureWrapper.Get());

    TextureDesc BackBufferDesc{};
    BackBufferDesc.Name      = "Main back buffer";
    BackBufferDesc.Type      = RESOURCE_DIM_TEX_2D;
    BackBufferDesc.Width     = wgpuSurfaceWidth;
    BackBufferDesc.Height    = wgpuSurfaceHeight;
    BackBufferDesc.Format    = WGPUFormatToTextureFormat(wgpuFormat);
    BackBufferDesc.Usage     = USAGE_DEFAULT;
    BackBufferDesc.BindFlags = BIND_RENDER_TARGET;

    RefCntAutoPtr<ITexture> pBackBufferTexture;
    m_pRenderDevice.RawPtr<RenderDeviceWebGPUImpl>()->CreateTextureFromWebGPUTexture(wgpuTextureWrapper.Get(), BackBufferDesc, RESOURCE_STATE_UNDEFINED, &pBackBufferTexture);

    if (IsSRGBFormat(m_SwapChainDesc.ColorBufferFormat))
    {
        TextureViewDesc BackBufferRTVDesc{};
        BackBufferRTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
        BackBufferRTVDesc.Format   = m_SwapChainDesc.ColorBufferFormat;

        RefCntAutoPtr<ITextureView> pBackBufferRTV;
        pBackBufferTexture->CreateView(BackBufferRTVDesc, &pBackBufferRTV);
        m_pBackBufferRTV = RefCntAutoPtr<ITextureViewWebGPU>{pBackBufferRTV, IID_TextureViewWebGPU};
    }
    else
    {
        m_pBackBufferRTV = RefCntAutoPtr<ITextureViewWebGPU>{pBackBufferTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET), IID_TextureViewWebGPU};
    }

    return wgpuSurfaceTexture.status;
}

void SwapChainWebGPUImpl::RequestAnimationFrame(Uint32 SyncInterval)
{
    RefCntAutoPtr<IDeviceContext> pDeviceContext = m_wpDeviceContext.Lock();

    if (m_SwapChainDesc.IsPrimary)
    {
        pDeviceContext->FinishFrame();
        m_pRenderDevice->ReleaseStaleResources();
    }

    WGPUSurfaceGetCurrentTextureStatus wgpuStatus = AcquireSurfaceTexture();

    const bool EnableVSync = SyncInterval != 0;
    if (wgpuStatus == WGPUSurfaceGetCurrentTextureStatus_Outdated ||
        wgpuStatus == WGPUSurfaceGetCurrentTextureStatus_Lost ||
        m_VSyncEnabled != EnableVSync)
    {
        m_VSyncEnabled = EnableVSync;
        RecreateSwapChain();
    }
}

void SwapChainWebGPUImpl::CreateDepthBufferView()
{
    if (m_SwapChainDesc.DepthBufferFormat != TEX_FORMAT_UNKNOWN)
    {
        TextureDesc DepthBufferDesc{};
        DepthBufferDesc.Name        = "Main depth buffer";
        DepthBufferDesc.Type        = RESOURCE_DIM_TEX_2D;
        DepthBufferDesc.Width       = m_SwapChainDesc.Width;
        DepthBufferDesc.Height      = m_SwapChainDesc.Height;
        DepthBufferDesc.Format      = m_SwapChainDesc.DepthBufferFormat;
        DepthBufferDesc.SampleCount = 1;
        DepthBufferDesc.Usage       = USAGE_DEFAULT;
        DepthBufferDesc.BindFlags   = BIND_DEPTH_STENCIL;

        DepthBufferDesc.ClearValue.Format               = DepthBufferDesc.Format;
        DepthBufferDesc.ClearValue.DepthStencil.Depth   = m_SwapChainDesc.DefaultDepthValue;
        DepthBufferDesc.ClearValue.DepthStencil.Stencil = m_SwapChainDesc.DefaultStencilValue;

        RefCntAutoPtr<ITexture> pDepthBufferTex;
        m_pRenderDevice->CreateTexture(DepthBufferDesc, nullptr, &pDepthBufferTex);
        m_pDepthBufferDSV = RefCntAutoPtr<ITextureViewWebGPU>{pDepthBufferTex->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL), IID_TextureViewWebGPU};
    }
}

void SwapChainWebGPUImpl::ReleaseSwapChainResources()
{
    if (!m_wgpuSurface)
        return;

    m_pBackBufferRTV.Release();
    m_pDepthBufferDSV.Release();
}

void SwapChainWebGPUImpl::RecreateSwapChain()
{
    try
    {
        ReleaseSwapChainResources();
        ConfigureSurface();
        CreateDepthBufferView();

        WGPUSurfaceGetCurrentTextureStatus wgpuStatus = AcquireSurfaceTexture();
        if (wgpuStatus != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
            wgpuStatus != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
        {
            LOG_ERROR("Failed to acquire the swap chain surface texture after recreation");
        }
    }
    catch (const std::runtime_error&)
    {
        LOG_ERROR("Failed to recreate the swap chain");
    }
}

} // namespace Diligent
