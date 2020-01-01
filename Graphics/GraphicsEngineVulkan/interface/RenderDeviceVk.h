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
/// Definition of the Diligent::IRenderDeviceVk interface

#include "../../GraphicsEngine/interface/RenderDevice.h"

namespace Diligent
{

// {AB8CF3A6-D959-41C1-AE00-A58AE9820E6A}
static constexpr INTERFACE_ID IID_RenderDeviceVk =
    {0xab8cf3a6, 0xd959, 0x41c1, {0xae, 0x0, 0xa5, 0x8a, 0xe9, 0x82, 0xe, 0x6a}};

/// Exposes Vulkan-specific functionality of a render device.
class IRenderDeviceVk : public IRenderDevice
{
public:
    /// Returns logical Vulkan device handle
    virtual VkDevice GetVkDevice() = 0;

    /// Returns physical Vulkan device
    virtual VkPhysicalDevice GetVkPhysicalDevice() = 0;

    /// Returns Vulkan instance
    virtual VkInstance GetVkInstance() = 0;

    /// Returns the fence value that will be signaled by the GPU command queue next
    virtual Uint64 GetNextFenceValue(Uint32 QueueIndex) = 0;

    /// Returns the last completed fence value for the given command queue
    virtual Uint64 GetCompletedFenceValue(Uint32 QueueIndex) = 0;

    /// Checks if the fence value has been signaled by the GPU. True means
    /// that all associated work has been finished
    virtual Bool IsFenceSignaled(Uint32 QueueIndex, Uint64 FenceValue) = 0;

    /// Creates a texture object from native Vulkan image

    /// \param [in]  vkImage      - Vulkan image handle
    /// \param [in]  TexDesc      - Texture description. Vulkan provides no means to retrieve any
    ///                             image properties from the image handle, so complete texture
    ///                             description must be provided
    /// \param [in]  InitialState - Initial texture state. See Diligent::RESOURCE_STATE.
    /// \param [out] ppTexture    - Address of the memory location where the pointer to the
    ///                             texture interface will be stored.
    ///                             The function calls AddRef(), so that the new object will contain
    ///                             one reference.
    /// \note  Created texture object does not take ownership of the Vulkan image and will not
    ///        destroy it once released. The application must not destroy the image while it is
    ///        in use by the engine.
    virtual void CreateTextureFromVulkanImage(VkImage            vkImage,
                                              const TextureDesc& TexDesc,
                                              RESOURCE_STATE     InitialState,
                                              ITexture**         ppTexture) = 0;

    /// Creates a buffer object from native Vulkan resource

    /// \param [in] vkBuffer      - Vulkan buffer handle
    /// \param [in] BuffDesc      - Buffer description. Vulkan provides no means to retrieve any
    ///                             buffer properties from the buffer handle, so complete buffer
    ///                             description must be provided
    /// \param [in]  InitialState - Initial buffer state. See Diligent::RESOURCE_STATE.
    /// \param [out] ppBuffer     - Address of the memory location where the pointer to the
    ///                             buffer interface will be stored.
    ///                             The function calls AddRef(), so that the new object will contain
    ///                             one reference.
    /// \note  Created buffer object does not take ownership of the Vulkan buffer and will not
    ///        destroy it once released. The application must not destroy Vulkan buffer while it is
    ///        in use by the engine.
    virtual void CreateBufferFromVulkanResource(VkBuffer          vkBuffer,
                                                const BufferDesc& BuffDesc,
                                                RESOURCE_STATE    InitialState,
                                                IBuffer**         ppBuffer) = 0;
};

} // namespace Diligent
