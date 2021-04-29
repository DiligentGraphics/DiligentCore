/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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
/// Declaration of functions that initialize Vulkan-based engine implementation

#include "../../GraphicsEngine/interface/EngineFactory.h"
#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../GraphicsEngine/interface/SwapChain.h"

// https://gcc.gnu.org/wiki/Visibility
#define API_QUALIFIER __attribute__((visibility("default")))

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {CF4A590D-2E40-4F48-9579-0D25991F963B}
static const INTERFACE_ID IID_EngineFactoryMtl =
    {0xcf4a590d, 0x2e40, 0x4f48, {0x95, 0x79, 0xd, 0x25, 0x99, 0x1f, 0x96, 0x3b}};

#define DILIGENT_INTERFACE_NAME IEngineFactoryMtl
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IEngineFactoryMtlInclusiveMethods \
    IEngineFactoryInclusiveMethods;       \
    IEngineFactoryMtlMethods EngineFactoryMtl

// clang-format off

DILIGENT_BEGIN_INTERFACE(IEngineFactoryMtl, IEngineFactory)
{
    VIRTUAL void METHOD(CreateDeviceAndContextsMtl)(
        THIS_
        const EngineMtlCreateInfo REF EngineCI,
        IRenderDevice**               ppDevice,
        IDeviceContext**              ppContexts) PURE;

    VIRTUAL void METHOD(CreateSwapChainMtl)(
        THIS_
        IRenderDevice*          pDevice,
        IDeviceContext*         pImmediateContext,
        const SwapChainDesc REF SCDesc,
        const NativeWindow REF  Window,
        ISwapChain**            ppSwapChain) PURE;

    VIRTUAL void METHOD(AttachToMtlDevice)(
        THIS_
        void*                         pMtlNativeDevice,
        const EngineMtlCreateInfo REF EngineAttribs,
        IRenderDevice**               ppDevice,
        IDeviceContext**              ppContexts) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IEngineFactoryMtl_CreateDeviceAndContextsMtl(This, ...) CALL_IFACE_METHOD(EngineFactoryMtl, CreateDeviceAndContextsMtl, This, __VA_ARGS__)
#    define IEngineFactoryMtl_CreateSwapChainMtl(This, ...)         CALL_IFACE_METHOD(EngineFactoryMtl, CreateSwapChainMtl,         This, __VA_ARGS__)
#    define IEngineFactoryMtl_AttachToMtlDevice(This, ...)          CALL_IFACE_METHOD(EngineFactoryMtl, AttachToMtlDevice,          This, __VA_ARGS__)

// clang-format on

#endif

API_QUALIFIER IEngineFactoryMtl* DILIGENT_GLOBAL_FUNCTION(GetEngineFactoryMtl)();

DILIGENT_END_NAMESPACE // namespace Diligent
