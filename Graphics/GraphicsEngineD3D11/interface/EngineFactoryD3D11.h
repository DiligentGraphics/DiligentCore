/*     Copyright 2015-2019 Egor Yusov
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
/// Declaration of functions that initialize Direct3D11-based engine implementation

#include <sstream>

#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../GraphicsEngine/interface/SwapChain.h"

#if PLATFORM_UNIVERSAL_WINDOWS && defined(ENGINE_DLL)
#   include "../../../Common/interface/StringTools.h"
#endif

namespace Diligent
{

class IEngineFactoryD3D11
{
public:
    virtual void CreateDeviceAndContextsD3D11(const EngineD3D11CreateInfo& EngineCI, 
                                              IRenderDevice**              ppDevice, 
                                              IDeviceContext**             ppContexts,
                                              Uint32                       NumDeferredContexts ) = 0;

   virtual void CreateSwapChainD3D11( IRenderDevice*            pDevice, 
                                      IDeviceContext*           pImmediateContext, 
                                      const SwapChainDesc&      SCDesc, 
                                      const FullScreenModeDesc& FSDesc,
                                      void*                     pNativeWndHandle, 
                                      ISwapChain**              ppSwapChain ) = 0;

   virtual void AttachToD3D11Device(void*                        pd3d11NativeDevice, 
                                    void*                        pd3d11ImmediateContext,
                                    const EngineD3D11CreateInfo& EngineCI, 
                                    IRenderDevice**              ppDevice, 
                                    IDeviceContext**             ppContexts,
                                    Uint32                       NumDeferredContexts) = 0;

   virtual void EnumerateHardwareAdapters(Uint32&                 NumAdapters, 
                                          HardwareAdapterAttribs* Adapters) = 0;

   virtual void EnumerateDisplayModes(Uint32              AdapterId, 
                                      Uint32              OutputId, 
                                      TEXTURE_FORMAT      Format, 
                                      Uint32&             NumDisplayModes, 
                                      DisplayModeAttribs* DisplayModes) = 0;
};

#if ENGINE_DLL

    typedef IEngineFactoryD3D11* (*GetEngineFactoryD3D11Type)();

    static bool LoadGraphicsEngineD3D11(GetEngineFactoryD3D11Type &GetFactoryFunc)
    {
        GetFactoryFunc = nullptr;
        std::string LibName = "GraphicsEngineD3D11_";

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
#if PLATFORM_WIN32
        auto hModule = LoadLibraryA( LibName.c_str() );
#elif PLATFORM_UNIVERSAL_WINDOWS
        auto hModule = LoadPackagedLibrary(WidenString(LibName).c_str(), 0);
#else
#   error Unexpected platform
#endif
        if( hModule == NULL )
        {
            std::stringstream ss;
            ss << "Failed to load " << LibName << " library.\n";
            OutputDebugStringA(ss.str().c_str());
            return false;
        }

        GetFactoryFunc = reinterpret_cast<GetEngineFactoryD3D11Type>( GetProcAddress(hModule, "GetEngineFactoryD3D11") );
        if( GetFactoryFunc == NULL )
        {
            std::stringstream ss;
            ss << "Failed to load GetEngineFactoryD3D11() from " << LibName << " library.\n";
            OutputDebugStringA(ss.str().c_str());
            FreeLibrary( hModule );
            return false;
        }

        return true;
    }
#else

    IEngineFactoryD3D11* GetEngineFactoryD3D11();

#endif

}
