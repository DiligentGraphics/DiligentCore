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

/// \file
/// Routines that initialize D3D11-based engine implementation

#include "pch.h"
#include "RenderDeviceFactoryD3D11.h"
#include "RenderDeviceD3D11Impl.h"
#include "DeviceContextD3D11Impl.h"
#include "SwapChainD3D11Impl.h"
#include "D3D11TypeConversions.h"
#include "EngineMemory.h"
#include <Windows.h>

namespace Diligent
{

#if defined(_DEBUG)
// Check for SDK Layer support.
inline bool SdkLayersAvailable()
{
	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
		0,
		D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
		nullptr,                    // Any feature level will do.
		0,
		D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
		nullptr,                    // No need to keep the D3D device reference.
		nullptr,                    // No need to know the feature level.
		nullptr                     // No need to keep the D3D device context reference.
		);

	return SUCCEEDED(hr);
}
#endif

/// Creates render device and device contexts for Direct3D11-based engine implementation

/// \param [in] EngineAttribs - Engine creation attributes.
/// \param [out] ppDevice - Address of the memory location where pointer to 
///                         the created device will be written
/// \param [out] ppContexts - Address of the memory location where pointers to 
///                           the contexts will be written. Pointer to the immediate 
///                           context goes at position 0. If NumDeferredContexts > 0,
///                           pointers to the deferred contexts go afterwards.
/// \param [in] NumDeferredContexts - Number of deferred contexts. If non-zero number
///                                   of deferred contexts is requested, pointers to the
///                                   contexts are written to ppContexts array starting 
///                                   at position 1
void CreateDeviceAndContextsD3D11( const EngineD3D11Attribs& EngineAttribs, IRenderDevice **ppDevice, IDeviceContext **ppContexts, Uint32 NumDeferredContexts )
{
    VERIFY( ppDevice && ppContexts, "Null pointer is provided" );
    if( !ppDevice || !ppContexts )
        return;

    SetRawAllocator(EngineAttribs.pRawMemAllocator);

    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (1+NumDeferredContexts));

    try
    {
	    // This flag adds support for surfaces with a different color channel ordering
	    // than the API default. It is required for compatibility with Direct2D.
        // D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        UINT creationFlags = 0;

    #if defined(_DEBUG)
	    if (SdkLayersAvailable())
	    {
		    // If the project is in a debug build, enable debugging via SDK Layers with this flag.
		    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	    }
    #endif

	    // This array defines the set of DirectX hardware feature levels this app will support.
	    // Note the ordering should be preserved.
	    // Don't forget to declare your application's minimum required feature level in its
	    // description.  All applications are assumed to support 9.1 unless otherwise stated.
	    D3D_FEATURE_LEVEL featureLevels[] = 
	    {
#ifdef PLATFORM_UNIVERSAL_WINDOWS
		    D3D_FEATURE_LEVEL_11_1,
#endif
		    D3D_FEATURE_LEVEL_11_0,
		    D3D_FEATURE_LEVEL_10_1,
		    D3D_FEATURE_LEVEL_10_0,
		    D3D_FEATURE_LEVEL_9_3,
		    D3D_FEATURE_LEVEL_9_2,
		    D3D_FEATURE_LEVEL_9_1
	    };

	    // Create the Direct3D 11 API device object and a corresponding context.
	    CComPtr<ID3D11Device> pd3d11Device;
	    CComPtr<ID3D11DeviceContext> pd3d11Context;

        D3D_FEATURE_LEVEL d3dFeatureLevel = D3D_FEATURE_LEVEL_11_0;
	    HRESULT hr = D3D11CreateDevice(
		    nullptr,					// Specify nullptr to use the default adapter.
		    D3D_DRIVER_TYPE_HARDWARE,	// Create a device using the hardware graphics driver.
		    0,							// Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
		    creationFlags,				// Set debug and Direct2D compatibility flags.
		    featureLevels,				// List of feature levels this app can support.
		    ARRAYSIZE(featureLevels),	// Size of the list above.
		    D3D11_SDK_VERSION,			// Always set this to D3D11_SDK_VERSION for Windows Store apps.
		    &pd3d11Device,				// Returns the Direct3D device created.
		    &d3dFeatureLevel,			// Returns feature level of device created.
		    &pd3d11Context				// Returns the device immediate context.
		    );

	    if (FAILED(hr))
	    {
		    // If the initialization fails, fall back to the WARP device.
		    // For more information on WARP, see: 
		    // http://go.microsoft.com/fwlink/?LinkId=286690
		    hr = D3D11CreateDevice(
				    nullptr,
				    D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
				    0,
				    creationFlags,
				    featureLevels,
				    ARRAYSIZE(featureLevels),
				    D3D11_SDK_VERSION,
				    &pd3d11Device,
				    &d3dFeatureLevel,
				    &pd3d11Context
				    );
            CHECK_D3D_RESULT_THROW( hr, "Failed to create D3D11 Device and swap chain" );
	    }

        auto &RawAlloctor = GetRawAllocator();
        RenderDeviceD3D11Impl *pRenderDeviceD3D11( NEW(RawAlloctor, "RenderDeviceD3D11Impl instance", RenderDeviceD3D11Impl, EngineAttribs, pd3d11Device, NumDeferredContexts ) );
        pRenderDeviceD3D11->QueryInterface(IID_RenderDevice, reinterpret_cast<IObject**>(ppDevice) );

        RefCntAutoPtr<DeviceContextD3D11Impl> pDeviceContextD3D11( NEW(RawAlloctor, "DeviceContextD3D11Impl instance", DeviceContextD3D11Impl, pRenderDeviceD3D11, pd3d11Context, EngineAttribs, false) );
        // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceD3D11 will
        // keep a weak reference to the context
        pDeviceContextD3D11->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts) );
        pRenderDeviceD3D11->SetImmediateContext(pDeviceContextD3D11);

        for (Uint32 DeferredCtx = 0; DeferredCtx < NumDeferredContexts; ++DeferredCtx)
        {
            CComPtr<ID3D11DeviceContext> pd3d11DeferredCtx;
            hr = pd3d11Device->CreateDeferredContext(0, &pd3d11DeferredCtx);
            CHECK_D3D_RESULT_THROW(hr, "Failed to create D3D11 deferred context")

            RefCntAutoPtr<DeviceContextD3D11Impl> pDeferredCtxD3D11( NEW(RawAlloctor, "DeviceContextD3D11Impl instance", DeviceContextD3D11Impl, pRenderDeviceD3D11, pd3d11DeferredCtx, EngineAttribs, true) );
            // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceD3D12 will
            // keep a weak reference to the context
            pDeferredCtxD3D11->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts + 1 + DeferredCtx) );
            pRenderDeviceD3D11->SetDeferredContext(DeferredCtx, pDeferredCtxD3D11);
        }
    }
    catch( const std::runtime_error & )
    {
        if( *ppDevice )
        {
            (*ppDevice)->Release();
            *ppDevice = nullptr;
        }
        for(Uint32 ctx=0; ctx < 1 + NumDeferredContexts; ++ctx)
        {
            if( ppContexts[ctx] != nullptr )
            {
                ppContexts[ctx]->Release();
                ppContexts[ctx] = nullptr;
            }
        }

        LOG_ERROR( "Failed to create device and immediate context" );
    }
}


/// Creates a swap chain for Direct3D11-based engine implementation

/// \param [in] pDevice - Pointer to the render device
/// \param [in] pImmediateContext - Pointer to the immediate device context
/// \param [in] SCDesc - Swap chain description
/// \param [in] pNativeWndHandle - Platform-specific native handle of the window 
///                                the swap chain will be associated with:
///                                * On Win32 platform, this should be window handle (HWND)
///                                * On Universal Windows Platform, this should be reference to the 
///                                  core window (Windows::UI::Core::CoreWindow)
///                                
/// \param [out] ppSwapChain    - Address of the memory location where pointer to the new 
///                               swap chain will be written
void CreateSwapChainD3D11( IRenderDevice *pDevice, Diligent::IDeviceContext *pImmediateContext, const SwapChainDesc& SCDesc, void* pNativeWndHandle, ISwapChain **ppSwapChain )
{
    VERIFY( ppSwapChain, "Null pointer is provided" );
    if( !ppSwapChain )
        return;

    *ppSwapChain = nullptr;

    try
    {
        auto *pDeviceD3D11 = ValidatedCast<RenderDeviceD3D11Impl>( pDevice );
        auto *pDeviceContextD3D11 = ValidatedCast<DeviceContextD3D11Impl>(pImmediateContext);
        auto &RawMemAllocator = GetRawAllocator();

        auto *pSwapChainD3D11 = NEW(RawMemAllocator,  "SwapChainD3D11Impl instance", SwapChainD3D11Impl, SCDesc, pDeviceD3D11, pDeviceContextD3D11, pNativeWndHandle);
        pSwapChainD3D11->QueryInterface( IID_SwapChain, reinterpret_cast<IObject**>(ppSwapChain) );

        pDeviceContextD3D11->SetSwapChain(pSwapChainD3D11);
        // Bind default render target
        pDeviceContextD3D11->SetRenderTargets( 0, nullptr, nullptr );
        // Set default viewport
        pDeviceContextD3D11->SetViewports( 1, nullptr, 0, 0 );

        auto NumDeferredCtx = pDeviceD3D11->GetNumDeferredContexts();
        for (size_t ctx = 0; ctx < NumDeferredCtx; ++ctx)
        {
            if (auto pDeferredCtx = pDeviceD3D11->GetDeferredContext(ctx))
            {
                auto *pDeferredCtxD3D11 = ValidatedCast<DeviceContextD3D11Impl>(pDeferredCtx.RawPtr());
                pDeferredCtxD3D11->SetSwapChain(pSwapChainD3D11);
                // Bind default render target
                pDeferredCtxD3D11->SetRenderTargets( 0, nullptr, nullptr );
                // Set default viewport
                pDeferredCtxD3D11->SetViewports( 1, nullptr, 0, 0 );
            }
        }
    }
    catch( const std::runtime_error & )
    {
        if( *ppSwapChain )
        {
            (*ppSwapChain)->Release();
            *ppSwapChain = nullptr;
        }

        LOG_ERROR( "Failed to create the swap chain" );
    }
}

#ifdef DOXYGEN
/// Loads Direct3D11-based engine implementation and exports factory functions
/// \param [out] CreateDeviceFunc    - Pointer to the function that creates render device and device contexts.
///                                    See CreateDeviceAndContextsD3D11().
/// \param [out] CreateSwapChainFunc - Pointer to the function that creates swap chain.
///                                    See CreateSwapChainD3D11().
/// \remarks Depending on the configuration and platform, the function loads different dll:
/// Platform\\Configuration    |           Debug               |        Release
/// --------------------------|-------------------------------|----------------------------
///         x86               | GraphicsEngineD3D11_32d.dll   |    GraphicsEngineD3D11_32r.dll
///         x64               | GraphicsEngineD3D11_64d.dll   |    GraphicsEngineD3D11_64r.dll
///
void LoadGraphicsEngineD3D11(CreateDeviceAndContextsD3D11Type &CreateDeviceFunc, CreateSwapChainD3D11Type &CreateSwapChainFunc)
{
    // This function is only required because DoxyGen refuses to generate documentation for a static function when SHOW_FILES==NO
    #error This function must never be compiled;    
}
#endif

}


using namespace Diligent;
extern "C"
{
    void CreateDeviceAndContextsD3D11(const EngineD3D11Attribs& EngineAttribs, IRenderDevice **ppDevice, IDeviceContext **ppContexts, Uint32 NumDeferredContexts)
    {
        Diligent::CreateDeviceAndContextsD3D11(EngineAttribs, ppDevice, ppContexts, NumDeferredContexts);
    }


    void CreateSwapChainD3D11(IRenderDevice *pDevice, IDeviceContext *pImmediateContext, const SwapChainDesc& SwapChainDesc, void* pNativeWndHandle, ISwapChain **ppSwapChain)
    {
        Diligent::CreateSwapChainD3D11(pDevice, pImmediateContext, SwapChainDesc, pNativeWndHandle, ppSwapChain);
    }
}
