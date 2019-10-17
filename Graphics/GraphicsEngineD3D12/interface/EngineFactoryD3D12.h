/*     Copyright 2019 Diligent Graphics LLC
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

#include "../../GraphicsEngine/interface/EngineFactory.h"
#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../GraphicsEngine/interface/SwapChain.h"

#if PLATFORM_UNIVERSAL_WINDOWS && defined(ENGINE_DLL)
#   include "../../../Common/interface/StringTools.h"
#endif

namespace Diligent
{

// {72BD38B0-684A-4889-9C68-0A80EC802DDE}
static const INTERFACE_ID IID_EngineFactoryD3D12 = 
{ 0x72bd38b0, 0x684a, 0x4889, { 0x9c, 0x68, 0xa, 0x80, 0xec, 0x80, 0x2d, 0xde } };

/// Engine factory for Direct3D12 rendering backend
class IEngineFactoryD3D12 : public IEngineFactory
{
public:
    /// Creates a render device and device contexts for Direct3D12-based engine implementation.

    /// \param [in] EngineCI    - Engine creation info.
    /// \param [out] ppDevice   - Address of the memory location where pointer to 
    ///                           the created device will be written.
    /// \param [out] ppContexts - Address of the memory location where pointers to 
    ///                           the contexts will be written. Immediate context goes at
    ///                           position 0. If EngineCI.NumDeferredContexts > 0,
    ///                           pointers to the deferred contexts are written afterwards.
    virtual void CreateDeviceAndContextsD3D12(const EngineD3D12CreateInfo& EngineCI, 
                                              IRenderDevice**              ppDevice, 
                                              IDeviceContext**             ppContexts) = 0;


    /// Attaches to existing Direct3D12 device.

    /// \param [in] pd3d12NativeDevice - Pointer to the native Direct3D12 device.
    /// \param [in] CommandQueueCount  - Number of command queues.
    /// \param [in] ppCommandQueues    - Pointer to the array of command queues.
    /// \param [in] EngineCI           - Engine creation info.
    /// \param [out] ppDevice          - Address of the memory location where pointer to 
    ///                                  the created device will be written
    /// \param [out] ppContexts - Address of the memory location where pointers to 
    ///                           the contexts will be written. Immediate context goes at
    ///                           position 0. If EngineCI.NumDeferredContexts > 0,
    ///                           pointers to the deferred contexts are written afterwards.
    virtual void AttachToD3D12Device(void*                         pd3d12NativeDevice, 
                                     size_t                        CommandQueueCount,
                                     class ICommandQueueD3D12**    ppCommandQueues,
                                     const EngineD3D12CreateInfo&  EngineCI, 
                                     IRenderDevice**               ppDevice, 
                                     IDeviceContext**              ppContexts) = 0;


    /// Creates a swap chain for Direct3D12-based engine implementation.

    /// \param [in] pDevice           - Pointer to the render device.
    /// \param [in] pImmediateContext - Pointer to the immediate device context.
    /// \param [in] SCDesc            - Swap chain description.
    /// \param [in] FSDesc            - Fullscreen mode description.
    /// \param [in] pNativeWndHandle - Platform-specific native handle of the window 
    ///                                the swap chain will be associated with:
    ///                                * On Win32 platform, this should be the window handle (HWND)
    ///                                * On Universal Windows Platform, this should be the reference
    ///                                  to the core window (Windows::UI::Core::CoreWindow)
    ///                                
    /// \param [out] ppSwapChain    - Address of the memory location where pointer to the new 
    ///                               swap chain will be written
    virtual void CreateSwapChainD3D12( IRenderDevice*            pDevice, 
                                       IDeviceContext*           pImmediateContext, 
                                       const SwapChainDesc&      SwapChainDesc, 
                                       const FullScreenModeDesc& FSDesc,
                                       void*                     pNativeWndHandle, 
                                       ISwapChain**              ppSwapChain ) = 0;


    /// Enumerates hardware adapters available on this machine.

    /// \param [in]     MinFeatureLevel - Minimum required feature level.
    /// \param [in,out] NumAdapters - Number of adapters. If Adapters is null, this value
    ///                               will be overwritten with the number of adapters available
    ///                               on this system. If Adapters is not null, this value should
    ///                               contain maximum number of elements reserved in the array 
    ///                               pointed to by Adapters. In the latter case, this value
    ///                               is overwritten with the actual number of elements written to
    ///                               Adapters.
    /// \param [out]    Adapters - Pointer to the array conataining adapter information. If
    ///                            null is provided, the number of available adapters is written to 
    ///                            NumAdapters
    virtual void EnumerateHardwareAdapters(DIRECT3D_FEATURE_LEVEL    MinFeatureLevel,
                                           Uint32&                   NumAdapters, 
                                           HardwareAdapterAttribs*   Adapters) = 0;


    /// Enumerates available display modes for the specified output of the specified adapter.

    /// \param [in] MinFeatureLevel - Minimum feature level of the adapter that was given to EnumerateHardwareAdapters().
    /// \param [in] AdapterId       - Id of the adapter enumerated by EnumerateHardwareAdapters().
    /// \param [in] OutputId        - Adapter output id.
    /// \param [in] Format          - Display mode format.
    /// \param [in, out] NumDisplayModes - Number of display modes. If DisplayModes is null, this
    ///                                    value is overwritten with the number of display modes 
    ///                                    available for this output. If DisplayModes is not null,
    ///                                    this value should contain the maximum number of elements
    ///                                    to be written to DisplayModes array. It is overwritten with
    ///                                    the actual number of display modes written.
    virtual void EnumerateDisplayModes(DIRECT3D_FEATURE_LEVEL MinFeatureLevel,
                                       Uint32                 AdapterId, 
                                       Uint32                 OutputId, 
                                       TEXTURE_FORMAT         Format, 
                                       Uint32&                NumDisplayModes, 
                                       DisplayModeAttribs*    DisplayModes) = 0;
};


#if ENGINE_DLL

    typedef IEngineFactoryD3D12* (*GetEngineFactoryD3D12Type)();

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

        GetFactoryFunc = reinterpret_cast<GetEngineFactoryD3D12Type>( GetProcAddress(hModule, "GetEngineFactoryD3D12") );
        if( GetFactoryFunc == NULL )
        {
            std::stringstream ss;
            ss << "Failed to load GetEngineFactoryD3D12() from " << LibName << " library.\n";
            OutputDebugStringA(ss.str().c_str());
            FreeLibrary( hModule );
            return false;
        }
        
        return true;
    }
#else

    IEngineFactoryD3D12* GetEngineFactoryD3D12();

#endif

}
