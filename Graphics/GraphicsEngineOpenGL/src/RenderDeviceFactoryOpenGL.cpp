/*     Copyright 2015 Egor Yusov
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

#include "pch.h"
#include "RenderDeviceFactoryOpenGL.h"
#include "RenderDeviceGLImpl.h"
#include "DeviceContextGLImpl.h"
#include "SwapChainGLImpl.h"

#ifdef PLATFORM_ANDROID
    #include "RenderDeviceGLESImpl.h"
#endif

using namespace Diligent;
using namespace Diligent;

extern "C"
{

#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_WINDOWS_STORE)
    typedef RenderDeviceGLImpl TRenderDeviceGLImpl;
#elif defined(PLATFORM_ANDROID)
    typedef RenderDeviceGLESImpl TRenderDeviceGLImpl;
#endif

API_QUALIFIER
void CreateDeviceAndSwapChainGL( const EngineCreationAttribs& CreationAttribs, 
                                 IRenderDevice **ppDevice,
                                 IDeviceContext **ppImmediateContext,
                                 const SwapChainDesc& SwapChainDesc, 
                                 void *pNativeWndHandle,
                                 Diligent::ISwapChain **ppSwapChain )
{
    VERIFY( ppDevice && ppImmediateContext && ppSwapChain, "Null pointer is provided" );
    if( !ppDevice || !ppImmediateContext || !ppSwapChain )
        return;

    *ppDevice = nullptr;
    *ppImmediateContext = nullptr;
    *ppSwapChain = nullptr;

    try
    {
        ContextInitInfo InitInfo;
        InitInfo.pNativeWndHandle = pNativeWndHandle;
        InitInfo.SwapChainAttribs = SwapChainDesc;
        RenderDeviceGLImpl *pRenderDeviceOpenGL( new TRenderDeviceGLImpl( InitInfo ) );
        pRenderDeviceOpenGL->QueryInterface(IID_RenderDevice, reinterpret_cast<IObject**>(ppDevice) );

        DeviceContextGLImpl *pDeviceContextOpenGL( new DeviceContextGLImpl( pRenderDeviceOpenGL ) );
        // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceOpenGL will
        // keep a weak reference to the context
        pDeviceContextOpenGL->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppImmediateContext) );
        pRenderDeviceOpenGL->SetImmediateContext(pDeviceContextOpenGL);

        SwapChainGLImpl *pSwapChainGL = new SwapChainGLImpl(SwapChainDesc, pRenderDeviceOpenGL, pDeviceContextOpenGL );
        pSwapChainGL->QueryInterface(IID_SwapChain, reinterpret_cast<IObject**>(ppSwapChain) );

        pDeviceContextOpenGL->SetSwapChain(pSwapChainGL);
        // Bind default framebuffer and viewport
        pDeviceContextOpenGL->SetRenderTargets( 0, nullptr, nullptr );
        pDeviceContextOpenGL->SetViewports( 1, nullptr, 0, 0 );

        pDeviceContextOpenGL->CreateDefaultStates();
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

}
