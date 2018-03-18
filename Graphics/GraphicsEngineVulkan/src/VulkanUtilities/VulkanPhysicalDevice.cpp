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

#include "VulkanErrors.h"
#include "VulkanUtilities/VulkanPhysicalDevice.h"

namespace VulkanUtilities
{
    VulkanPhysicalDevice::VulkanPhysicalDevice(VkPhysicalDevice vkDevice) :
        m_VkDevice(vkDevice)
    {
        VERIFY_EXPR(m_VkDevice != VK_NULL_HANDLE);

        vkGetPhysicalDeviceProperties(m_VkDevice, &m_Properties);
        vkGetPhysicalDeviceFeatures(m_VkDevice, &m_Features);
        vkGetPhysicalDeviceMemoryProperties(m_VkDevice, &m_MemoryProperties);
        uint32_t QueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_VkDevice, &QueueFamilyCount, nullptr);
        VERIFY_EXPR(QueueFamilyCount> 0);
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
            VERIFY_EXPR(ExtensionCount == m_SupportedExtensions.size());
        }
    }

    uint32_t VulkanPhysicalDevice::FindQueueFamily(VkQueueFlags QueueFlags)
    {
        for(uint32_t i=0; i < m_QueueFamilyProperties.size(); ++i)
        {
            auto &Props = m_QueueFamilyProperties[i];
            if( (Props.queueFlags & QueueFlags) == QueueFlags)
            {
                if(QueueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
                {
                    // Queues supporting graphics and/or compute operations must report (1,1,1) 
                    // in minImageTransferGranularity, meaning that there are no additional restrictions 
                    // on the granularity of image transfer operations for these queues. 
                    VERIFY_EXPR(Props.minImageTransferGranularity.width  == 1 && 
                                Props.minImageTransferGranularity.height == 1 &&
                                Props.minImageTransferGranularity.depth  == 1);
                    return i;
                }
            }
        }

        LOG_ERROR_AND_THROW("Failed to find suitable queue family");
        return 0;
    }

    bool VulkanPhysicalDevice::IsExtensionSupported(const char *ExtensionName)
    {
        for(const auto& Extension : m_SupportedExtensions)
            if(strcmp(Extension.extensionName, ExtensionName) == 0)
                return true;

        return false;
    }
}
