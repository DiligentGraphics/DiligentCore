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

#pragma once

/// \file
/// Declaration of functions that create OpenGL-based engine implementation

#include "RenderDevice.h"
#include "SwapChain.h"

#if defined(PLATFORM_WIN32) || defined(PLATFORM_UNIVERSAL_WINDOWS)

#   define API_QUALIFIER

#elif defined(PLATFORM_ANDROID)

#   ifdef ENGINE_DLL
#       ifdef BUILDING_DLL
            // https://gcc.gnu.org/wiki/Visibility
#           define API_QUALIFIER __attribute__((visibility("default")))
#       else
#           define API_QUALIFIER __attribute__((visibility("default")))
#       endif
#   else
#       define API_QUALIFIER
#   endif

#endif


extern "C"
{

#if defined(ENGINE_DLL) && (defined(PLATFORM_WIN32) || defined(PLATFORM_UNIVERSAL_WINDOWS))

    typedef void (*CreateDeviceAndSwapChainGLType)( const Diligent::EngineCreationAttribs& CreationAttribs, 
                                     Diligent::IRenderDevice **ppDevice,
                                     Diligent::IDeviceContext **ppImmediateContext,
                                     const Diligent::SwapChainDesc& SCDesc, 
                                     void *pNativeWndHandle, 
                                     Diligent::ISwapChain **ppSwapChain );

    static void LoadGraphicsEngineOpenGL(CreateDeviceAndSwapChainGLType &CreateDeviceFunc)
    {
        CreateDeviceFunc = nullptr;
        std::string LibName = "GraphicsEngineOpenGL_";

#if _WIN64
        LibName += "64";
#else
        LibName += "32";
#endif

#ifdef _DEBUG
        LibName += "d";
#else
        LibName += "r";
#endif

        LibName += ".dll";
        auto hModule = LoadLibraryA( LibName.c_str() );
        if( hModule == NULL )
        {
            LOG_ERROR_MESSAGE( "Failed to load ", LibName, " library." );
            return;
        }

        CreateDeviceFunc = reinterpret_cast<CreateDeviceAndSwapChainGLType>( GetProcAddress(hModule, "CreateDeviceAndSwapChainGL") );
        if( CreateDeviceFunc == NULL )
        {
            LOG_ERROR_MESSAGE( "Failed to load CreateDeviceAndSwapChainGL() from ", LibName, " library." );
            FreeLibrary( hModule );
            return;
        }
    }

#else

    // Do not forget to call System.loadLibrary("GraphicsEngineOpenGL") in Java on Android!
    API_QUALIFIER
    void CreateDeviceAndSwapChainGL( const Diligent::EngineCreationAttribs& CreationAttribs, 
                                     Diligent::IRenderDevice **ppDevice,
                                     Diligent::IDeviceContext **ppImmediateContext,
                                     const Diligent::SwapChainDesc& SCDesc, 
                                     void *pNativeWndHandle, 
                                     Diligent::ISwapChain **ppSwapChain );
#endif
}
