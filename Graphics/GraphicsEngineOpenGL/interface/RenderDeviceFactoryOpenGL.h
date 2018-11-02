/*     Copyright 2015-2018 Egor Yusov
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

#include <sstream>

#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../GraphicsEngine/interface/SwapChain.h"

#include "../../HLSL2GLSLConverterLib/interface/HLSL2GLSLConverter.h"

#include "EngineGLAttribs.h"

#if PLATFORM_WIN32 || PLATFORM_UNIVERSAL_WINDOWS

#   define API_QUALIFIER

#elif PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MACOS || PLATFORM_IOS

#   if ENGINE_DLL
#       if BUILDING_DLL
            // https://gcc.gnu.org/wiki/Visibility
#           define API_QUALIFIER __attribute__((visibility("default")))
#       else
#           define API_QUALIFIER __attribute__((visibility("default")))
#       endif
#   else
#       define API_QUALIFIER
#   endif

#endif

namespace Diligent
{

class IEngineFactoryOpenGL
{
public:
    virtual void CreateDeviceAndSwapChainGL(const EngineGLAttribs& CreationAttribs,
                                            IRenderDevice **ppDevice,
                                            IDeviceContext **ppImmediateContext,
                                            const SwapChainDesc& SCDesc,
                                            ISwapChain **ppSwapChain ) = 0;
    virtual void CreateHLSL2GLSLConverter(IHLSL2GLSLConverter **ppConverter) = 0;
    
    virtual void AttachToActiveGLContext( const EngineGLAttribs& CreationAttribs,
                                          IRenderDevice **ppDevice,
                                          IDeviceContext **ppImmediateContext ) = 0;
};


#if ENGINE_DLL && (PLATFORM_WIN32 || PLATFORM_UNIVERSAL_WINDOWS)

    typedef IEngineFactoryOpenGL* (*GetEngineFactoryOpenGLType)();

    static bool LoadGraphicsEngineOpenGL(GetEngineFactoryOpenGLType &GetFactoryFunc)
    {
        GetFactoryFunc = nullptr;
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
            std::stringstream ss;
            ss << "Failed to load " << LibName << " library.\n";
            OutputDebugStringA(ss.str().c_str());
            return false;
        }

        GetFactoryFunc = reinterpret_cast<GetEngineFactoryOpenGLType>( GetProcAddress(hModule, "GetEngineFactoryOpenGLInternal") );
        if( GetFactoryFunc == NULL )
        {
            std::stringstream ss;
            ss << "Failed to load GetEngineFactoryOpenGL() from " << LibName << " library.\n";
            OutputDebugStringA(ss.str().c_str());
            FreeLibrary( hModule );
            return false;
        }
        return true;
    }

#else

    IEngineFactoryOpenGL* GetEngineFactoryOpenGLInternal();

    // Do not forget to call System.loadLibrary("GraphicsEngineOpenGL") in Java on Android!
    API_QUALIFIER
    inline IEngineFactoryOpenGL* GetEngineFactoryOpenGL()
    {
        return GetEngineFactoryOpenGLInternal();
    }

#endif

}
