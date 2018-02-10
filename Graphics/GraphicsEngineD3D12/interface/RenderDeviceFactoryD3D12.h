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
/// Declaration of functions that initialize Direct3D12-based engine implementation

#include "Errors.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"

#if PLATFORM_UNIVERSAL_WINDOWS && defined(ENGINE_DLL)
#   include "StringTools.h"
#endif

namespace Diligent
{

class IEngineFactoryD3D12
{
public:
    virtual void CreateDeviceAndContextsD3D12( const EngineD3D12Attribs& CreationAttribs, 
                                               IRenderDevice **ppDevice, 
                                               IDeviceContext **ppContexts,
                                               Uint32 NumDeferredContexts) = 0;

    virtual void AttachToD3D12Device(void *pd3d12NativeDevice, 
                                     class ICommandQueueD3D12 *pCommandQueue,
                                     const EngineD3D12Attribs& EngineAttribs, 
                                     IRenderDevice **ppDevice, 
                                     IDeviceContext **ppContexts,
                                     Uint32 NumDeferredContexts) = 0;

    virtual void CreateSwapChainD3D12( IRenderDevice *pDevice, 
                                       IDeviceContext *pImmediateContext, 
                                       const SwapChainDesc& SwapChainDesc, 
                                       void* pNativeWndHandle, 
                                       ISwapChain **ppSwapChain ) = 0;

};

}

extern "C"
{
#if ENGINE_DLL

    typedef Diligent::IEngineFactoryD3D12* (*GetEngineFactoryD3D12Type)();

    static bool LoadGraphicsEngineD3D12(GetEngineFactoryD3D12Type &GetFactoryFunc)
    {
        GetFactoryFunc = nullptr;
        std::string LibName = "GraphicsEngineD3D12_";

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
        auto hModule = LoadLibraryA(LibName.c_str());
#elif PLATFORM_UNIVERSAL_WINDOWS
        auto hModule = LoadPackagedLibrary(Diligent::WidenString(LibName).c_str(), 0);
#else
#   error Unexpected platform
#endif

        if( hModule == NULL )
        {
            LOG_ERROR_MESSAGE( "Failed to load ", LibName, " library." );
            return false;
        }

        GetFactoryFunc = reinterpret_cast<GetEngineFactoryD3D12Type>( GetProcAddress(hModule, "GetEngineFactoryD3D12") );
        if( GetFactoryFunc == NULL )
        {
            LOG_ERROR_MESSAGE( "Failed to load GetEngineFactoryD3D12() from ", LibName, " library." );
            FreeLibrary( hModule );
            return false;
        }
        
        return true;
    }
#else

    Diligent::IEngineFactoryD3D12* GetEngineFactoryD3D12();

#endif
}
