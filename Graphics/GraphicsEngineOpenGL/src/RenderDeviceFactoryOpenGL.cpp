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
/// Routines that initialize OpenGL/GLES-based engine implementation

#include "pch.h"
#include "RenderDeviceFactoryOpenGL.h"
#include "RenderDeviceGLImpl.h"
#include "DeviceContextGLImpl.h"
#include "SwapChainGLImpl.h"
#include "EngineMemory.h"

#ifdef PLATFORM_ANDROID
    #include "RenderDeviceGLESImpl.h"
#endif

namespace Diligent
{

#if defined(PLATFORM_WIN32) || defined(PLATFORM_UNIVERSAL_WINDOWS)
    typedef RenderDeviceGLImpl TRenderDeviceGLImpl;
#elif defined(PLATFORM_ANDROID)
    typedef RenderDeviceGLESImpl TRenderDeviceGLImpl;
#endif


/// Creates render device, device context and swap chain for OpenGL/GLES-based engine implementation

/// \param [in] CreationAttribs - Engine creation attributes.
/// \param [out] ppDevice - Address of the memory location where pointer to 
///                         the created device will be written.
/// \param [out] ppImmediateContext - Address of the memory location where pointers to 
///                                   the immediate context will be written.
/// \param [in] SCDesc - Swap chain description.
/// \param [in] pNativeWndHandle - Platform-specific native handle of the window 
///                                the swap chain will be associated with:
///                                * On Win32 platform, this should be window handle (HWND)
///                                * On Android platform, this should be a pointer to the native window (ANativeWindow*)
/// \param [out] ppSwapChain    - Address of the memory location where pointer to the new 
///                               swap chain will be written.
void CreateDeviceAndSwapChainGL( const EngineCreationAttribs& CreationAttribs, 
                                 IRenderDevice **ppDevice,
                                 IDeviceContext **ppImmediateContext,
                                 const SwapChainDesc& SCDesc, 
                                 void *pNativeWndHandle,
                                 Diligent::ISwapChain **ppSwapChain )
{
    VERIFY( ppDevice && ppImmediateContext && ppSwapChain, "Null pointer is provided" );
    if( !ppDevice || !ppImmediateContext || !ppSwapChain )
        return;

    SetRawAllocator(CreationAttribs.pRawMemAllocator);

    *ppDevice = nullptr;
    *ppImmediateContext = nullptr;
    *ppSwapChain = nullptr;

    try
    {
        auto &RawMemAllocator = GetRawAllocator();

        ContextInitInfo InitInfo;
        InitInfo.pNativeWndHandle = pNativeWndHandle;
        InitInfo.SwapChainAttribs = SCDesc;
        RenderDeviceGLImpl *pRenderDeviceOpenGL( NEW(RawMemAllocator, "TRenderDeviceGLImpl instance", TRenderDeviceGLImpl, InitInfo ) );
        pRenderDeviceOpenGL->QueryInterface(IID_RenderDevice, reinterpret_cast<IObject**>(ppDevice) );

        DeviceContextGLImpl *pDeviceContextOpenGL( NEW(RawMemAllocator, "DeviceContextGLImpl instance", DeviceContextGLImpl, pRenderDeviceOpenGL, false ) );
        // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceOpenGL will
        // keep a weak reference to the context
        pDeviceContextOpenGL->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppImmediateContext) );
        pRenderDeviceOpenGL->SetImmediateContext(pDeviceContextOpenGL);

        SwapChainGLImpl *pSwapChainGL = NEW(RawMemAllocator, "SwapChainGLImpl instance", SwapChainGLImpl, SCDesc, pRenderDeviceOpenGL, pDeviceContextOpenGL );
        pSwapChainGL->QueryInterface(IID_SwapChain, reinterpret_cast<IObject**>(ppSwapChain) );

        pDeviceContextOpenGL->SetSwapChain(pSwapChainGL);
        // Bind default framebuffer and viewport
        pDeviceContextOpenGL->SetRenderTargets( 0, nullptr, nullptr );
        pDeviceContextOpenGL->SetViewports( 1, nullptr, 0, 0 );
    }
    catch( const std::runtime_error & )
    {
        if( *ppDevice )
        {
            (*ppDevice)->Release();
            *ppDevice = nullptr;
        }

        if( *ppImmediateContext )
        {
            (*ppImmediateContext)->Release();
            *ppImmediateContext = nullptr;
        }

        if( *ppSwapChain )
        {
            (*ppSwapChain)->Release();
            *ppSwapChain = nullptr;
        }

        LOG_ERROR( "Failed to initialize OpenGL-based render device" );
    }
}

#ifdef DOXYGEN
/// Loads OpenGL-based engine implementation and exports factory functions
/// \param [out] CreateDeviceFunc    - Pointer to the function that creates render device, device context and swap chain.
///                                    See CreateDeviceAndSwapChainGL().
/// \remarks Depending on the configuration and platform, the function loads different dll:
/// Platform\\Configuration    |           Debug              |         Release
/// --------------------------|------------------------------|----------------------------
///   Win32/x86               | GraphicsEngineOpenGL_32d.dll | GraphicsEngineOpenGL_32r.dll
///   Win32/x64               | GraphicsEngineOpenGL_64d.dll | GraphicsEngineOpenGL_64r.dll
///
/// To load the library on Android, it is necessary to call System.loadLibrary("GraphicsEngineOpenGL") from Java.
void LoadGraphicsEngineOpenGL(CreateDeviceAndSwapChainGLType &CreateDeviceFunc)
{
    // This function is only required because DoxyGen refuses to generate documentation for a static function when SHOW_FILES==NO
    #error This function must never be compiled;    
}
#endif

}

extern "C"
{
API_QUALIFIER
void CreateDeviceAndSwapChainGL( const EngineCreationAttribs& CreationAttribs, 
                                 IRenderDevice **ppDevice,
                                 IDeviceContext **ppImmediateContext,
                                 const SwapChainDesc& SwapChainDesc, 
                                 void *pNativeWndHandle,
                                 Diligent::ISwapChain **ppSwapChain )
{
    Diligent::CreateDeviceAndSwapChainGL( CreationAttribs, ppDevice, ppImmediateContext, SwapChainDesc, pNativeWndHandle, ppSwapChain );
}

}
