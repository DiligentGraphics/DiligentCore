/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

/// \file
/// Routines that initialize D3D11-based engine implementation

#include "pch.h"

#include <Windows.h>
#include <dxgi1_3.h>

#include "EngineFactoryD3D11.h"
#include "RenderDeviceD3D11Impl.hpp"
#include "DeviceContextD3D11Impl.hpp"
#include "SwapChainD3D11Impl.hpp"
#include "D3D11TypeConversions.hpp"
#include "EngineMemory.h"
#include "EngineFactoryD3DBase.hpp"
#include "EngineFactoryBase.hpp"

namespace Diligent
{

/// Engine factory for D3D11 implementation
class EngineFactoryD3D11Impl : public EngineFactoryD3DBase<IEngineFactoryD3D11, RENDER_DEVICE_TYPE_D3D11>
{
public:
    static EngineFactoryD3D11Impl* GetInstance()
    {
        static EngineFactoryD3D11Impl TheFactory;
        return &TheFactory;
    }

    using TBase = EngineFactoryD3DBase<IEngineFactoryD3D11, RENDER_DEVICE_TYPE_D3D11>;

    EngineFactoryD3D11Impl() :
        TBase{IID_EngineFactoryD3D11}
    {}

    virtual void DILIGENT_CALL_TYPE CreateDeviceAndContextsD3D11(const EngineD3D11CreateInfo& EngineCI,
                                                                 IRenderDevice**              ppDevice,
                                                                 IDeviceContext**             ppContexts) override final;

    virtual void DILIGENT_CALL_TYPE CreateSwapChainD3D11(IRenderDevice*            pDevice,
                                                         IDeviceContext*           pImmediateContext,
                                                         const SwapChainDesc&      SCDesc,
                                                         const FullScreenModeDesc& FSDesc,
                                                         const NativeWindow&       Window,
                                                         ISwapChain**              ppSwapChain) override final;

    virtual void DILIGENT_CALL_TYPE AttachToD3D11Device(void*                        pd3d11NativeDevice,
                                                        void*                        pd3d11ImmediateContext,
                                                        const EngineD3D11CreateInfo& EngineCI,
                                                        IRenderDevice**              ppDevice,
                                                        IDeviceContext**             ppContexts) override final;

    virtual void InitializeGraphicsAdapterInfo(void*                pd3Device,
                                               IDXGIAdapter1*       pDXIAdapter,
                                               GraphicsAdapterInfo& AdapterInfo) const override final;
};


#ifdef DILIGENT_DEVELOPMENT
// Check for SDK Layer support.
inline bool SdkLayersAvailable()
{
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_NULL, // There is no need to create a real hardware device.
        0,
        D3D11_CREATE_DEVICE_DEBUG, // Check for the SDK layers.
        nullptr,                   // Any feature level will do.
        0,
        D3D11_SDK_VERSION, // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        nullptr,           // No need to keep the D3D device reference.
        nullptr,           // No need to know the feature level.
        nullptr            // No need to keep the D3D device context reference.
    );

    return SUCCEEDED(hr);
}
#endif


void EngineFactoryD3D11Impl::CreateDeviceAndContextsD3D11(const EngineD3D11CreateInfo& EngineCI,
                                                          IRenderDevice**              ppDevice,
                                                          IDeviceContext**             ppContexts)
{
    if (EngineCI.DebugMessageCallback != nullptr)
        SetDebugMessageCallback(EngineCI.DebugMessageCallback);

    if (EngineCI.EngineAPIVersion != DILIGENT_API_VERSION)
    {
        LOG_ERROR_MESSAGE("Diligent Engine runtime (", DILIGENT_API_VERSION, ") is not compatible with the client API version (", EngineCI.EngineAPIVersion, ")");
        return;
    }

    VERIFY(ppDevice && ppContexts, "Null pointer provided");
    if (!ppDevice || !ppContexts)
        return;

    if (EngineCI.GraphicsAPIVersion >= Version{12, 0})
    {
        LOG_ERROR_MESSAGE("DIRECT3D_FEATURE_LEVEL_12_0 and above is not supported by Direct3D11 backend");
        return;
    }

    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (std::max(1u, EngineCI.NumContexts) + EngineCI.NumDeferredContexts));

    // This flag adds support for surfaces with a different color channel ordering
    // than the API default. It is required for compatibility with Direct2D.
    // D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    UINT creationFlags = 0;

#ifdef DILIGENT_DEVELOPMENT
    if (EngineCI.EnableValidation && SdkLayersAvailable())
    {
        // If the project is in a debug build, enable debugging via SDK Layers with this flag.
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }
#endif

    CComPtr<IDXGIAdapter1> SpecificAdapter;
    if (EngineCI.AdapterId != DEFAULT_ADAPTER_ID)
    {
        auto Adapters = FindCompatibleAdapters(EngineCI.GraphicsAPIVersion);
        if (EngineCI.AdapterId < Adapters.size())
            SpecificAdapter = Adapters[EngineCI.AdapterId];
        else
        {
            LOG_ERROR_AND_THROW(EngineCI.AdapterId, " is not a valid hardware adapter id. Total number of compatible adapters available on this system: ", Adapters.size());
        }
    }

    // Create the Direct3D 11 API device object and a corresponding context.
    CComPtr<ID3D11Device>        pd3d11Device;
    CComPtr<ID3D11DeviceContext> pd3d11Context;

    const Version FeatureLevelList[] = {{11, 1}, {11, 0}, {10, 1}, {10, 0}};

    for (int adapterType = 0; adapterType < 2 && !pd3d11Device; ++adapterType)
    {
        IDXGIAdapter*   adapter    = nullptr;
        D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_UNKNOWN;
        switch (adapterType)
        {
            case 0:
                adapter    = SpecificAdapter;
                driverType = SpecificAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
                break;

            case 1:
                driverType = D3D_DRIVER_TYPE_WARP;
                break;

            default:
                UNEXPECTED("Unexpected adapter type");
        }

        // This page https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice says the following:
        //     If you provide a D3D_FEATURE_LEVEL array that contains D3D_FEATURE_LEVEL_11_1 on a computer that doesn't have the Direct3D 11.1
        //     runtime installed, this function immediately fails with E_INVALIDARG.
        // To avoid failure in this case we will try one feature level at a time
        for (auto FeatureLevel : FeatureLevelList)
        {
            auto d3dFeatureLevel = GetD3DFeatureLevel(FeatureLevel);
            auto hr              = D3D11CreateDevice(
                adapter,           // Specify nullptr to use the default adapter.
                driverType,        // If no adapter specified, request hardware graphics driver.
                0,                 // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
                creationFlags,     // Set debug and Direct2D compatibility flags.
                &d3dFeatureLevel,  // List of feature levels this app can support.
                1,                 // Size of the list above.
                D3D11_SDK_VERSION, // Always set this to D3D11_SDK_VERSION for Windows Store apps.
                &pd3d11Device,     // Returns the Direct3D device created.
                nullptr,           // Returns feature level of device created.
                &pd3d11Context     // Returns the device immediate context.
            );

            if (SUCCEEDED(hr))
            {
                VERIFY_EXPR(pd3d11Device && pd3d11Context);
                break;
            }
        }
    }

    if (!pd3d11Device)
        LOG_ERROR_AND_THROW("Failed to create d3d11 device and immediate context");

    AttachToD3D11Device(pd3d11Device, pd3d11Context, EngineCI, ppDevice, ppContexts);
}


static CComPtr<IDXGIAdapter1> DXGIAdapterFromD3D11Device(ID3D11Device* pd3d11Device)
{
    CComPtr<IDXGIDevice> pDXGIDevice;

    auto hr = pd3d11Device->QueryInterface(__uuidof(pDXGIDevice), reinterpret_cast<void**>(static_cast<IDXGIDevice**>(&pDXGIDevice)));
    if (SUCCEEDED(hr))
    {
        CComPtr<IDXGIAdapter> pDXGIAdapter;
        hr = pDXGIDevice->GetAdapter(&pDXGIAdapter);
        if (SUCCEEDED(hr))
        {
            CComPtr<IDXGIAdapter1> pDXGIAdapter1;
            pDXGIAdapter.QueryInterface(&pDXGIAdapter1);
            return pDXGIAdapter1;
        }
        else
        {
            LOG_ERROR("Failed to get DXGI Adapter from DXGI Device.");
        }
    }
    else
    {
        LOG_ERROR("Failed to query IDXGIDevice from D3D device.");
    }

    return nullptr;
}


void EngineFactoryD3D11Impl::AttachToD3D11Device(void*                        pd3d11NativeDevice,
                                                 void*                        pd3d11ImmediateContext,
                                                 const EngineD3D11CreateInfo& _EngineCI,
                                                 IRenderDevice**              ppDevice,
                                                 IDeviceContext**             ppContexts)
{
    EngineD3D11CreateInfo EngineCI = _EngineCI;

    if (EngineCI.DebugMessageCallback != nullptr)
        SetDebugMessageCallback(EngineCI.DebugMessageCallback);

    if (EngineCI.EngineAPIVersion != DILIGENT_API_VERSION)
    {
        LOG_ERROR_MESSAGE("Diligent Engine runtime (", DILIGENT_API_VERSION, ") is not compatible with the client API version (", EngineCI.EngineAPIVersion, ")");
        return;
    }

    VERIFY(ppDevice && ppContexts, "Null pointer provided");
    if (!ppDevice || !ppContexts)
        return;

    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (std::max(1u, EngineCI.NumContexts) + EngineCI.NumDeferredContexts));

    if (EngineCI.NumContexts > 1)
    {
        LOG_WARNING_MESSAGE("Direct3D11 back-end does not support multiple immediate contexts");
        EngineCI.NumContexts = 1;
    }

    try
    {
        ID3D11Device*        pd3d11Device       = reinterpret_cast<ID3D11Device*>(pd3d11NativeDevice);
        ID3D11DeviceContext* pd3d11ImmediateCtx = reinterpret_cast<ID3D11DeviceContext*>(pd3d11ImmediateContext);
        auto                 pDXGIAdapter1      = DXGIAdapterFromD3D11Device(pd3d11Device);

        GraphicsAdapterInfo AdapterInfo;
        InitializeGraphicsAdapterInfo(pd3d11NativeDevice, pDXGIAdapter1, AdapterInfo);
        EnableDeviceFeatures(AdapterInfo.Capabilities.Features, EngineCI.Features);
        AdapterInfo.Capabilities.Features = EngineCI.Features;
        VerifyEngineCreateInfo(EngineCI, AdapterInfo);

        SetRawAllocator(EngineCI.pRawMemAllocator);
        auto&                  RawAlloctor = GetRawAllocator();
        RenderDeviceD3D11Impl* pRenderDeviceD3D11(NEW_RC_OBJ(RawAlloctor, "RenderDeviceD3D11Impl instance", RenderDeviceD3D11Impl)(RawAlloctor, this, EngineCI, AdapterInfo, pd3d11Device));
        pRenderDeviceD3D11->QueryInterface(IID_RenderDevice, reinterpret_cast<IObject**>(ppDevice));

        CComQIPtr<ID3D11DeviceContext1> pd3d11ImmediateCtx1{pd3d11ImmediateCtx};
        if (!pd3d11ImmediateCtx1)
            LOG_ERROR_AND_THROW("Failed to get ID3D11DeviceContext1 interface from device context");

        RefCntAutoPtr<DeviceContextD3D11Impl> pDeviceContextD3D11(NEW_RC_OBJ(RawAlloctor, "DeviceContextD3D11Impl instance", DeviceContextD3D11Impl)(RawAlloctor, pRenderDeviceD3D11, pd3d11ImmediateCtx1, EngineCI, false));
        // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceD3D11 will
        // keep a weak reference to the context
        pDeviceContextD3D11->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts));
        pRenderDeviceD3D11->SetImmediateContext(0, pDeviceContextD3D11);

        for (Uint32 DeferredCtx = 0; DeferredCtx < EngineCI.NumDeferredContexts; ++DeferredCtx)
        {
            CComPtr<ID3D11DeviceContext> pd3d11DeferredCtx;

            HRESULT hr = pd3d11Device->CreateDeferredContext(0, &pd3d11DeferredCtx);
            CHECK_D3D_RESULT_THROW(hr, "Failed to create D3D11 deferred context");

            CComQIPtr<ID3D11DeviceContext1> pd3d11DeferredCtx1{pd3d11DeferredCtx};
            if (!pd3d11DeferredCtx1)
                LOG_ERROR_AND_THROW("Failed to get ID3D11DeviceContext1 interface from device context");

            RefCntAutoPtr<DeviceContextD3D11Impl> pDeferredCtxD3D11{
                NEW_RC_OBJ(RawAlloctor, "DeviceContextD3D11Impl instance", DeviceContextD3D11Impl)(RawAlloctor, pRenderDeviceD3D11, pd3d11DeferredCtx1, EngineCI, true)};
            // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceD3D12 will
            // keep a weak reference to the context
            pDeferredCtxD3D11->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts + 1 + DeferredCtx));
            pRenderDeviceD3D11->SetDeferredContext(DeferredCtx, pDeferredCtxD3D11);
        }
    }
    catch (const std::runtime_error&)
    {
        if (*ppDevice)
        {
            (*ppDevice)->Release();
            *ppDevice = nullptr;
        }
        for (Uint32 ctx = 0; ctx < 1 + EngineCI.NumDeferredContexts; ++ctx)
        {
            if (ppContexts[ctx] != nullptr)
            {
                ppContexts[ctx]->Release();
                ppContexts[ctx] = nullptr;
            }
        }

        LOG_ERROR("Failed to initialize D3D11 device and contexts");
    }
}


void EngineFactoryD3D11Impl::CreateSwapChainD3D11(IRenderDevice*            pDevice,
                                                  IDeviceContext*           pImmediateContext,
                                                  const SwapChainDesc&      SCDesc,
                                                  const FullScreenModeDesc& FSDesc,
                                                  const NativeWindow&       Window,
                                                  ISwapChain**              ppSwapChain)
{
    VERIFY(ppSwapChain, "Null pointer provided");
    if (!ppSwapChain)
        return;

    *ppSwapChain = nullptr;

    try
    {
        auto* pDeviceD3D11        = ValidatedCast<RenderDeviceD3D11Impl>(pDevice);
        auto* pDeviceContextD3D11 = ValidatedCast<DeviceContextD3D11Impl>(pImmediateContext);
        auto& RawMemAllocator     = GetRawAllocator();

        auto* pSwapChainD3D11 = NEW_RC_OBJ(RawMemAllocator, "SwapChainD3D11Impl instance", SwapChainD3D11Impl)(SCDesc, FSDesc, pDeviceD3D11, pDeviceContextD3D11, Window);
        pSwapChainD3D11->QueryInterface(IID_SwapChain, reinterpret_cast<IObject**>(ppSwapChain));
    }
    catch (const std::runtime_error&)
    {
        if (*ppSwapChain)
        {
            (*ppSwapChain)->Release();
            *ppSwapChain = nullptr;
        }

        LOG_ERROR("Failed to create the swap chain");
    }
}


void EngineFactoryD3D11Impl::InitializeGraphicsAdapterInfo(void*                pd3Device,
                                                           IDXGIAdapter1*       pDXIAdapter,
                                                           GraphicsAdapterInfo& AdapterInfo) const
{
    TBase::InitializeGraphicsAdapterInfo(pd3Device, pDXIAdapter, AdapterInfo);

    AdapterInfo.Capabilities.DevType = RENDER_DEVICE_TYPE_D3D11;

    ID3D11Device* pd3d11Device = reinterpret_cast<ID3D11Device*>(pd3Device);
    if (pd3d11Device == nullptr)
    {
        const Version FeatureLevelList[] = {{11, 1}, {11, 0}, {10, 1}, {10, 0}};
        for (auto FeatureLevel : FeatureLevelList)
        {
            auto d3dFeatureLevel = GetD3DFeatureLevel(FeatureLevel);
            auto hr              = D3D11CreateDevice(
                pDXIAdapter,             // Specify nullptr to use the default adapter.
                D3D_DRIVER_TYPE_UNKNOWN, // If no adapter specified, request hardware graphics driver.
                0,                       // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
                0,                       // Set debug and Direct2D compatibility flags.
                &d3dFeatureLevel,        // List of feature levels this app can support.
                1,                       // Size of the list above.
                D3D11_SDK_VERSION,       // Always set this to D3D11_SDK_VERSION for Windows Store apps.
                &pd3d11Device,           // Returns the Direct3D device created.
                nullptr,                 // Returns feature level of device created.
                nullptr                  // Returns the device immediate context.
            );
            if (SUCCEEDED(hr))
                break;
        }
        if (pd3d11Device == nullptr)
            return;
    }
    VERIFY_EXPR(pd3d11Device != nullptr);

    switch (pd3d11Device->GetFeatureLevel())
    {
        case D3D_FEATURE_LEVEL_11_1: AdapterInfo.Capabilities.APIVersion = {11, 1}; break;
        case D3D_FEATURE_LEVEL_11_0: AdapterInfo.Capabilities.APIVersion = {11, 0}; break;
        case D3D_FEATURE_LEVEL_10_1: AdapterInfo.Capabilities.APIVersion = {10, 1}; break;
        case D3D_FEATURE_LEVEL_10_0: AdapterInfo.Capabilities.APIVersion = {10, 0}; break;
        default: UNEXPECTED("Unexpected D3D feature level");
    }

    // Set texture and sampler capabilities
    {
        auto& Features = AdapterInfo.Capabilities.Features;
        {
            bool ShaderFloat16Supported = false;

            D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT d3d11MinPrecisionSupport = {};
            if (SUCCEEDED(pd3d11Device->CheckFeatureSupport(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &d3d11MinPrecisionSupport, sizeof(d3d11MinPrecisionSupport))))
            {
                ShaderFloat16Supported =
                    (d3d11MinPrecisionSupport.PixelShaderMinPrecision & D3D11_SHADER_MIN_PRECISION_16_BIT) != 0 &&
                    (d3d11MinPrecisionSupport.AllOtherShaderStagesMinPrecision & D3D11_SHADER_MIN_PRECISION_16_BIT) != 0;
            }
            Features.ShaderFloat16 = ShaderFloat16Supported ? DEVICE_FEATURE_STATE_ENABLED : DEVICE_FEATURE_STATE_DISABLED;
        }

        auto& TexCaps = AdapterInfo.Capabilities.TexCaps;

        TexCaps.MaxTexture1DDimension     = D3D11_REQ_TEXTURE1D_U_DIMENSION;
        TexCaps.MaxTexture1DArraySlices   = D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION;
        TexCaps.MaxTexture2DDimension     = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        TexCaps.MaxTexture2DArraySlices   = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        TexCaps.MaxTexture3DDimension     = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        TexCaps.MaxTextureCubeDimension   = D3D11_REQ_TEXTURECUBE_DIMENSION;
        TexCaps.Texture2DMSSupported      = True;
        TexCaps.Texture2DMSArraySupported = True;
        TexCaps.TextureViewSupported      = True;
        TexCaps.CubemapArraysSupported    = True;

        auto& SamCaps = AdapterInfo.Capabilities.SamCaps;

        SamCaps.BorderSamplingModeSupported   = True;
        SamCaps.AnisotropicFilteringSupported = True;
        SamCaps.LODBiasSupported              = True;
    }

    // Set properties
    {
        auto& BufferProps = AdapterInfo.Properties.Buffer;
        // Offsets passed to *SSetConstantBuffers1 are measured in shader constants, which are
        // 16 bytes (4*32-bit components). Each offset must be a multiple of 16 constants,
        // i.e. 256 bytes.
        BufferProps.ConstantBufferOffsetAlignment   = 256;
        BufferProps.StructuredBufferOffsetAlignment = D3D11_RAW_UAV_SRV_BYTE_ALIGNMENT;
    }

    if (pd3Device == nullptr)
        pd3d11Device->Release();

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(DeviceFeatures) == 37, "Did you add a new feature to DeviceFeatures? Please handle its satus here.");
    static_assert(sizeof(DeviceProperties) == 28, "Did you add a new peroperty to DeviceProperties? Please handle its satus here.");
#endif
}

#ifdef DOXYGEN
/// Loads Direct3D11-based engine implementation and exports factory functions
///
/// \return      - Pointer to the function that returns factory for D3D11 engine implementation
///                See Diligent::EngineFactoryD3D11Impl.
///
/// \remarks Depending on the configuration and platform, the function loads different dll:
///
/// Platform\\Configuration   |           Debug               |        Release
/// --------------------------|-------------------------------|----------------------------
///         x86               | GraphicsEngineD3D11_32d.dll   |    GraphicsEngineD3D11_32r.dll
///         x64               | GraphicsEngineD3D11_64d.dll   |    GraphicsEngineD3D11_64r.dll
///
GetEngineFactoryD3D11Type LoadGraphicsEngineD3D11()
{
// This function is only required because DoxyGen refuses to generate documentation for a static function when SHOW_FILES==NO
#    error This function must never be compiled;
}
#endif


IEngineFactoryD3D11* GetEngineFactoryD3D11()
{
    return EngineFactoryD3D11Impl::GetInstance();
}

} // namespace Diligent

extern "C"
{
    Diligent::IEngineFactoryD3D11* Diligent_GetEngineFactoryD3D11()
    {
        return Diligent::GetEngineFactoryD3D11();
    }
}
