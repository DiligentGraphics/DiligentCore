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
/// Definition of the Diligent::IDeviceContextVk interface

#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "CommandQueueVk.h"

namespace Diligent
{

// {72AEB1BA-C6AD-42EC-8811-7ED9C72176BB}
static constexpr INTERFACE_ID IID_DeviceContextVk =
{ 0x72aeb1ba, 0xc6ad, 0x42ec,{ 0x88, 0x11, 0x7e, 0xd9, 0xc7, 0x21, 0x76, 0xbb } };

/// Interface to the device context object implemented in Vulkan
class IDeviceContextVk : public IDeviceContext
{
public:

    /// Transitions internal vulkan image to a specified layout

    /// \param [in] pTexture - texture to transition
    /// \param [in] NewLayout - Vulkan image layout this texture to transition to
    /// \remarks The texture state must be known to the engine.
    virtual void TransitionImageLayout(ITexture *pTexture, VkImageLayout NewLayout) = 0;

    /// Transitions internal vulkan buffer object to a specified state

    /// \param [in] pBuffer - Buffer to transition
    /// \param [in] NewAccessFlags - Access flags to set for the buffer
    /// \remarks The buffer state must be known to the engine.
    virtual void BufferMemoryBarrier(IBuffer *pBuffer, VkAccessFlags NewAccessFlags) = 0;

    /// Locks the internal mutex and returns a pointer to the command queue that is associated with this device context.

    /// \return - a pointer to ICommandQueueVk interface of the command queue associated with the context.
    ///
    /// \remarks  Only immediate device contexts have associated command queues.
    ///
    ///           The engine locks the internal mutex to prevent simultaneous access to the command queue.
    ///           An application must release the lock by calling IDeviceContextVk::UnlockCommandQueue()
    ///           when it is done working with the queue or the engine will not be able to submit any command 
    ///           list to the queue. Nested calls to LockCommandQueue() are not allowed.
    ///           The queue pointer never changes while the context is alive, so an application may cache and
    ///           use the pointer if it does not need to prevent potential simultaneous access to the queue from
    ///           other threads.
    ///
    ///           The engine manages the lifetimes of command queues and all other device objects,
    ///           so an application must not call AddRef/Release methods on the returned interface.
    virtual ICommandQueueVk* LockCommandQueue() = 0;

    /// Unlocks the command queue that was previously locked by IDeviceContextVk::LockCommandQueue().
    virtual void UnlockCommandQueue() = 0;
};

}
