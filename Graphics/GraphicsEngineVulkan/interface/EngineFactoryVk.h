/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#pragma once

/// \file
/// Declaration of functions that initialize Direct3D12-based engine implementation

#include <sstream>

#include "../../GraphicsEngine/interface/EngineFactory.h"
#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../GraphicsEngine/interface/SwapChain.h"

#if PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MACOS || PLATFORM_IOS || (PLATFORM_WIN32 && !defined(_MSC_VER))
// https://gcc.gnu.org/wiki/Visibility
#    define API_QUALIFIER __attribute__((visibility("default")))
#elif PLATFORM_WIN32
#    define API_QUALIFIER
#else
#    error Unsupported platform
#endif

#if ENGINE_DLL && PLATFORM_WIN32 && defined(_MSC_VER)
#    include "../../GraphicsEngine/interface/LoadEngineDll.h"
#    define EXPLICITLY_LOAD_ENGINE_VK_DLL 1
#endif

namespace Diligent
{

// {F554EEE4-57C2-4637-A508-85BE80DC657C}
static const INTERFACE_ID IID_EngineFactoryVk =
    {0xf554eee4, 0x57c2, 0x4637, {0xa5, 0x8, 0x85, 0xbe, 0x80, 0xdc, 0x65, 0x7c}};

class IEngineFactoryVk : public IEngineFactory
{
public:
    virtual void CreateDeviceAndContextsVk(const EngineVkCreateInfo& EngineCI,
                                           IRenderDevice**           ppDevice,
                                           IDeviceContext**          ppContexts) = 0;

    //virtual void AttachToVulkanDevice(void *pVkNativeDevice,
    //                                 class ICommandQueueVk *pCommandQueue,
    //                                 const EngineVkCreateInfo& EngineCI,
    //                                 IRenderDevice **ppDevice,
    //                                 IDeviceContext **ppContexts) = 0;

    virtual void CreateSwapChainVk(IRenderDevice*       pDevice,
                                   IDeviceContext*      pImmediateContext,
                                   const SwapChainDesc& SwapChainDesc,
                                   void*                pNativeWndHandle,
                                   ISwapChain**         ppSwapChain) = 0;
};


#if EXPLICITLY_LOAD_ENGINE_VK_DLL

using GetEngineFactoryVkType = IEngineFactoryVk* (*)();

static bool LoadGraphicsEngineVk(GetEngineFactoryVkType& GetFactoryFunc)
{
    auto ProcAddress = LoadEngineDll("GraphicsEngineVk", "GetEngineFactoryVk");
    GetFactoryFunc   = reinterpret_cast<GetEngineFactoryVkType>(ProcAddress);
    return GetFactoryFunc != nullptr;
}

#else

API_QUALIFIER
IEngineFactoryVk* GetEngineFactoryVk();

#endif

} // namespace Diligent
