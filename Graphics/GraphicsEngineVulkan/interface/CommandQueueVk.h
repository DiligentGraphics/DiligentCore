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
/// Definition of the Diligent::ICommandQueueVk interface

#include "../../../Primitives/interface/Object.h"

namespace Diligent
{

// {9FBF582F-3069-41B9-AC05-344D5AF5CE8C}
static constexpr INTERFACE_ID IID_CommandQueueVk =
{ 0x9fbf582f, 0x3069, 0x41b9,{ 0xac, 0x5, 0x34, 0x4d, 0x5a, 0xf5, 0xce, 0x8c } };

/// Command queue interface
class ICommandQueueVk : public Diligent::IObject
{
public:
	/// Returns the fence value that will be signaled next time
	virtual Uint64 GetNextFenceValue() = 0;

	/// Submits a given command buffer to the command queue

    /// \return Fence value associated with the submitted command buffer
	virtual Uint64 Submit(VkCommandBuffer cmdBuffer) = 0;

    /// Submits a given chunk of work to the command queue

    /// \return Fence value associated with the submitted command buffer
    virtual Uint64 Submit(const VkSubmitInfo& SubmitInfo) = 0;

    /// Presents the current swap chain image on the screen
    virtual VkResult Present(const VkPresentInfoKHR& PresentInfo) = 0;

    /// Returns Vulkan command queue. May return VK_NULL_HANDLE if queue is anavailable
    virtual VkQueue GetVkQueue() = 0;

    /// Returns vulkan command queue family index
    virtual uint32_t GetQueueFamilyIndex()const = 0;

    /// Returns value of the last completed fence
    virtual Uint64 GetCompletedFenceValue() = 0;

    /// Blocks execution until all pending GPU commands are complete

    /// \return Last completed fence value
    virtual Uint64 WaitForIdle() = 0;

    /// Signals the given fence
    virtual void SignalFence(VkFence vkFence) = 0;
};

}
