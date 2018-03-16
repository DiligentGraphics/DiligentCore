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

#include <sstream>

#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../GraphicsEngine/interface/SwapChain.h"

#if PLATFORM_UNIVERSAL_WINDOWS && defined(ENGINE_DLL)
#   include "../../../Common/interface/StringTools.h"
#endif

namespace Diligent
{

    class IEngineFactoryVk
    {
    public:
        virtual void CreateDeviceAndContextsVk(const EngineVkAttribs& CreationAttribs,
            IRenderDevice **ppDevice,
            IDeviceContext **ppContexts,
            Uint32 NumDeferredContexts) = 0;

        //virtual void AttachToVulkanDevice(void *pVkNativeDevice, 
        //                                 class ICommandQueueVk *pCommandQueue,
        //                                 const EngineVkAttribs& EngineAttribs, 
        //                                 IRenderDevice **ppDevice, 
        //                                 IDeviceContext **ppContexts,
        //                                 Uint32 NumDeferredContexts) = 0;

        virtual void CreateSwapChainVk(IRenderDevice *pDevice,
            IDeviceContext *pImmediateContext,
            const SwapChainDesc& SwapChainDesc,
            void* pNativeWndHandle,
            ISwapChain **ppSwapChain) = 0;

    };


#if ENGINE_DLL

    typedef IEngineFactoryVk* (*GetEngineFactoryVkType)();

    static bool LoadGraphicsEngineVk(GetEngineFactoryVkType &GetFactoryFunc)
    {
        GetFactoryFunc = nullptr;
        std::string LibName = "GraphicsEngineVk_";

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
        auto hModule = LoadPackagedLibrary(WidenString(LibName).c_str(), 0);
#else
#   error Unexpected platform
#endif

        if (hModule == NULL)
        {
            std::stringstream ss;
            ss << "Failed to load " << LibName << " library.\n";
            OutputDebugStringA(ss.str().c_str());
            return false;
        }

        GetFactoryFunc = reinterpret_cast<GetEngineFactoryVkType>(GetProcAddress(hModule, "GetEngineFactoryVk"));
        if (GetFactoryFunc == NULL)
        {
            std::stringstream ss;
            ss << "Failed to load GetEngineFactoryVk() from " << LibName << " library.\n";
            OutputDebugStringA(ss.str().c_str());
            FreeLibrary(hModule);
            return false;
        }

        return true;
    }
#else

    IEngineFactoryVk* GetEngineFactoryVk();

#endif

}
