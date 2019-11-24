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

#include <memory>
#include <vector>
#include "vulkan.h"

namespace VulkanUtilities
{

class VulkanPhysicalDevice
{
public:
    // clang-format off
    VulkanPhysicalDevice             (const VulkanPhysicalDevice&) = delete;
    VulkanPhysicalDevice             (VulkanPhysicalDevice&&)      = delete;
    VulkanPhysicalDevice& operator = (const VulkanPhysicalDevice&) = delete;
    VulkanPhysicalDevice& operator = (VulkanPhysicalDevice&&)      = delete;
    // clang-format on

    static std::unique_ptr<VulkanPhysicalDevice> Create(VkPhysicalDevice vkDevice);

    // clang-format off
    uint32_t         FindQueueFamily     (VkQueueFlags QueueFlags)                           const;
    VkPhysicalDevice GetVkDeviceHandle   ()                                                  const { return m_VkDevice; }
    bool             IsExtensionSupported(const char* ExtensionName)                         const;
    bool             CheckPresentSupport (uint32_t queueFamilyIndex, VkSurfaceKHR VkSurface) const;
    // clang-format on

    static constexpr uint32_t InvalidMemoryTypeIndex = static_cast<uint32_t>(-1);

    uint32_t GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

    const VkPhysicalDeviceProperties& GetProperties() const { return m_Properties; }
    const VkPhysicalDeviceFeatures&   GetFeatures() const { return m_Features; }
    VkFormatProperties                GetPhysicalDeviceFormatProperties(VkFormat imageFormat) const;

private:
    VulkanPhysicalDevice(VkPhysicalDevice vkDevice);

    const VkPhysicalDevice               m_VkDevice;
    VkPhysicalDeviceProperties           m_Properties       = {};
    VkPhysicalDeviceFeatures             m_Features         = {};
    VkPhysicalDeviceMemoryProperties     m_MemoryProperties = {};
    std::vector<VkQueueFamilyProperties> m_QueueFamilyProperties;
    std::vector<VkExtensionProperties>   m_SupportedExtensions;
};

} // namespace VulkanUtilities
