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
/// Definition of the Diligent::IDeviceContextMtl interface

#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "CommandQueueMtl.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {2DEA7704-C586-4BA7-B938-93B239DFA268}
static const INTERFACE_ID IID_DeviceContextMtl =
    {0x2dea7704, 0xc586, 0x4ba7, {0xb9, 0x38, 0x93, 0xb2, 0x39, 0xdf, 0xa2, 0x68}};

#define DILIGENT_INTERFACE_NAME IDeviceContextMtl
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IDeviceContextMtlInclusiveMethods \
    IDeviceContextInclusiveMethods;       \
    IDeviceContextMtlMethods DeviceContextMt

// clang-format off

/// Exposes Metal-specific functionality of a device context.
DILIGENT_BEGIN_INTERFACE(IDeviceContextMtl, IDeviceContext)
{
    /// Returns a command buffer pointer that is currently being recorded

    /// \return - a pointer to the current command buffer
    ///
    /// \remarks  Any command on the device context command may potentially submit the command buffer for
    ///           execution into the command queue and make it invalid. An application should
    ///           never cache the pointer and should instead request the command buffer every time it
    ///           needs it.
    ///
    ///           Diligent Engine internally keeps track of all resource state changes (vertex and index
    ///           buffers, pipeline states, render targets, etc.). If an application changes any of these
    ///           states in the command buffer, it must invalidate the engine's internal state tracking by
    ///           calling IDeviceContext::InvalidateState() and then manually restore all required states via
    ///           appropriate Diligent API calls.
    ///
    ///           Engine will end all active encoders.
    VIRTUAL id<MTLCommandBuffer> METHOD(GetMtlCommandBuffer)(THIS) PURE;

    /// Locks the internal mutex and returns a pointer to the command queue that is associated with this device context.

    /// \return - a pointer to ICommandQueueMtl interface of the command queue associated with the context.
    ///
    /// \remarks  Only immediate device contexts have associated command queues.
    ///
    ///           The engine locks the internal mutex to prevent simultaneous access to the command queue.
    ///           An application must release the lock by calling IDeviceContextMtl::UnlockCommandQueue()
    ///           when it is done working with the queue or the engine will not be able to submit any command
    ///           list to the queue. Nested calls to LockCommandQueue() are not allowed.
    ///           The queue pointer never changes while the context is alive, so an application may cache and
    ///           use the pointer if it does not need to prevent potential simultaneous access to the queue from
    ///           other threads.
    VIRTUAL ICommandQueueMtl* METHOD(LockCommandQueue)(THIS) PURE;

    /// Unlocks the command queue that was previously locked by IDeviceContextMtl::LockCommandQueue().
    VIRTUAL void METHOD(UnlockCommandQueue)(THIS) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IDeviceContextMtl_GetMtlCommandBuffer(This)  CALL_IFACE_METHOD(DeviceContextMtl, GetMtlCommandBuffer,   This)
#    define IDeviceContextMtl_LockCommandQueue(This)     CALL_IFACE_METHOD(DeviceContextMtl, LockCommandQueue,      This)
#    define IDeviceContextMtl_UnlockCommandQueue(This)   CALL_IFACE_METHOD(DeviceContextMtl, UnlockCommandQueue,    This)

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
