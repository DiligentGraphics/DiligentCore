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

#include <limits>
#include <cstring>
#include "VulkanErrors.hpp"
#include "VulkanUtilities/VulkanPhysicalDevice.hpp"

namespace VulkanUtilities
{

std::unique_ptr<VulkanPhysicalDevice> VulkanPhysicalDevice::Create(VkPhysicalDevice vkDevice)
{
    auto* PhysicalDevice = new VulkanPhysicalDevice{vkDevice};
    return std::unique_ptr<VulkanPhysicalDevice>{PhysicalDevice};
}

VulkanPhysicalDevice::VulkanPhysicalDevice(VkPhysicalDevice vkDevice) :
    m_VkDevice{vkDevice}
{
    VERIFY_EXPR(m_VkDevice != VK_NULL_HANDLE);

    vkGetPhysicalDeviceProperties(m_VkDevice, &m_Properties);
    vkGetPhysicalDeviceFeatures(m_VkDevice, &m_Features);
    vkGetPhysicalDeviceMemoryProperties(m_VkDevice, &m_MemoryProperties);
    uint32_t QueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_VkDevice, &QueueFamilyCount, nullptr);
    VERIFY_EXPR(QueueFamilyCount > 0);
    m_QueueFamilyProperties.resize(QueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_VkDevice, &QueueFamilyCount, m_QueueFamilyProperties.data());
    VERIFY_EXPR(QueueFamilyCount == m_QueueFamilyProperties.size());

    // Get list of supported extensions
    uint32_t ExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(m_VkDevice, nullptr, &ExtensionCount, nullptr);
    if (ExtensionCount > 0)
    {
        m_SupportedExtensions.resize(ExtensionCount);
        auto res = vkEnumerateDeviceExtensionProperties(m_VkDevice, nullptr, &ExtensionCount, m_SupportedExtensions.data());
        VERIFY_EXPR(res == VK_SUCCESS);
        (void)res;
        VERIFY_EXPR(ExtensionCount == m_SupportedExtensions.size());
    }
}

uint32_t VulkanPhysicalDevice::FindQueueFamily(VkQueueFlags QueueFlags) const
{
    // All commands that are allowed on a queue that supports transfer operations are also allowed on
    // a queue that supports either graphics or compute operations. Thus, if the capabilities of a queue
    // family include VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT, then reporting the VK_QUEUE_TRANSFER_BIT
    // capability separately for that queue family is optional (4.1).
    VkQueueFlags QueueFlagsOpt = QueueFlags;
    if (QueueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
    {
        QueueFlags &= ~VK_QUEUE_TRANSFER_BIT;
        QueueFlagsOpt = QueueFlags | VK_QUEUE_TRANSFER_BIT;
    }

    static constexpr uint32_t InvalidFamilyInd = std::numeric_limits<uint32_t>::max();
    uint32_t                  FamilyInd        = InvalidFamilyInd;

    for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); ++i)
    {
        // First try to find a queue, for which the flags match exactly
        // (i.e. dedicated compute or transfer queue)
        const auto& Props = m_QueueFamilyProperties[i];
        if (Props.queueFlags == QueueFlags ||
            Props.queueFlags == QueueFlagsOpt)
        {
            FamilyInd = i;
            break;
        }
    }

    if (FamilyInd == InvalidFamilyInd)
    {
        for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); ++i)
        {
            // Try to find a queue for which all requested flags are set
            const auto& Props = m_QueueFamilyProperties[i];
            // Check only QueueFlags as VK_QUEUE_TRANSFER_BIT is
            // optional for graphics and/or compute queues
            if ((Props.queueFlags & QueueFlags) == QueueFlags)
            {
                FamilyInd = i;
                break;
            }
        }
    }

    if (FamilyInd != InvalidFamilyInd)
    {
        if (QueueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        {
#ifdef _DEBUG
            const auto& Props = m_QueueFamilyProperties[FamilyInd];
            // Queues supporting graphics and/or compute operations must report (1,1,1)
            // in minImageTransferGranularity, meaning that there are no additional restrictions
            // on the granularity of image transfer operations for these queues (4.1).
            VERIFY_EXPR(Props.minImageTransferGranularity.width == 1 &&
                        Props.minImageTransferGranularity.height == 1 &&
                        Props.minImageTransferGranularity.depth == 1);
#endif
        }
    }
    else
    {
        LOG_ERROR_AND_THROW("Failed to find suitable queue family");
    }

    return FamilyInd;
}

bool VulkanPhysicalDevice::IsExtensionSupported(const char* ExtensionName) const
{
    for (const auto& Extension : m_SupportedExtensions)
        if (strcmp(Extension.extensionName, ExtensionName) == 0)
            return true;

    return false;
}

bool VulkanPhysicalDevice::CheckPresentSupport(uint32_t queueFamilyIndex, VkSurfaceKHR VkSurface) const
{
    VkBool32 PresentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_VkDevice, queueFamilyIndex, VkSurface, &PresentSupport);
    return PresentSupport == VK_TRUE;
}


// This function is used to find a device memory type that supports all the property flags we request
// Params:
// * memoryTypeBitsRequirement  -  a bitmask that contains one bit set for every supported memory type for
//                                 the resource. Bit i is set if and only if the memory type i in the
//                                 VkPhysicalDeviceMemoryProperties structure for the physical device is
//                                 supported for the resource.
// * requiredProperties   -  required memory properties (device local, host visible, etc.)
uint32_t VulkanPhysicalDevice::GetMemoryTypeIndex(uint32_t              memoryTypeBitsRequirement,
                                                  VkMemoryPropertyFlags requiredProperties) const
{
    // Iterate over all memory types available for the device
    // For each pair of elements X and Y returned in memoryTypes, X must be placed at a lower index position than Y if:
    //   * either the set of bit flags of X is a strict subset of the set of bit flags of Y.
    //   * or the propertyFlags members of X and Y are equal, and X belongs to a memory heap with greater performance

    for (uint32_t memoryIndex = 0; memoryIndex < m_MemoryProperties.memoryTypeCount; memoryIndex++)
    {
        // Each memory type returned by vkGetPhysicalDeviceMemoryProperties must have its propertyFlags set
        // to one of the following values:
        // * 0
        // * HOST_VISIBLE_BIT | HOST_COHERENT_BIT
        // * HOST_VISIBLE_BIT | HOST_CACHED_BIT
        // * HOST_VISIBLE_BIT | HOST_CACHED_BIT | HOST_COHERENT_BIT
        // * DEVICE_LOCAL_BIT
        // * DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT | HOST_COHERENT_BIT
        // * DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT | HOST_CACHED_BIT
        // * DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT | HOST_CACHED_BIT | HOST_COHERENT_BIT
        // * DEVICE_LOCAL_BIT | LAZILY_ALLOCATED_BIT
        //
        // There must be at least one memory type with both the HOST_VISIBLE_BIT and HOST_COHERENT_BIT bits set
        // There must be at least one memory type with the DEVICE_LOCAL_BIT bit set

        const uint32_t memoryTypeBit        = (1 << memoryIndex);
        const bool     isRequiredMemoryType = (memoryTypeBitsRequirement & memoryTypeBit) != 0;
        if (isRequiredMemoryType)
        {
            const VkMemoryPropertyFlags properties            = m_MemoryProperties.memoryTypes[memoryIndex].propertyFlags;
            const bool                  hasRequiredProperties = (properties & requiredProperties) == requiredProperties;

            if (hasRequiredProperties)
                return memoryIndex;
        }
    }
    return InvalidMemoryTypeIndex;
}

VkFormatProperties VulkanPhysicalDevice::GetPhysicalDeviceFormatProperties(VkFormat imageFormat) const
{
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_VkDevice, imageFormat, &formatProperties);
    return formatProperties;
}

} // namespace VulkanUtilities
