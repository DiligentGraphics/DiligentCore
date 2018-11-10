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

#pragma once

#include <vector>
#include <memory>
#include "vulkan.h"

namespace VulkanUtilities
{
    class VulkanInstance : public std::enable_shared_from_this<VulkanInstance>
    {
    public:
        VulkanInstance             (const VulkanInstance&)  = delete;
        VulkanInstance             (      VulkanInstance&&) = delete;
        VulkanInstance& operator = (const VulkanInstance&)  = delete;
        VulkanInstance& operator = (      VulkanInstance&&) = delete;

        static std::shared_ptr<VulkanInstance> Create(bool                   EnableValidation, 
                                                      uint32_t               GlobalExtensionCount, 
                                                      const char* const*     ppGlobalExtensionNames,
                                                      VkAllocationCallbacks* pVkAllocator);
        ~VulkanInstance();

        std::shared_ptr<VulkanInstance> GetSharedPtr()
        {
            return shared_from_this();
        }

        std::shared_ptr<const VulkanInstance> GetSharedPtr()const
        {
            return shared_from_this();
        }

        bool IsLayerAvailable    (const char* LayerName)    const;
        bool IsExtensionAvailable(const char* ExtensionName)const;
        VkPhysicalDevice SelectPhysicalDevice()const;
        VkAllocationCallbacks* GetVkAllocator()const{return m_pVkAllocator;}
        VkInstance             GetVkInstance() const{return m_VkInstance;}

    private:
        VulkanInstance(bool                   EnableValidation, 
                       uint32_t               GlobalExtensionCount, 
                       const char* const*     ppGlobalExtensionNames,
                       VkAllocationCallbacks* pVkAllocator);

        bool m_DebugUtilsEnabled = false;
        VkAllocationCallbacks* const m_pVkAllocator;
        VkInstance m_VkInstance = VK_NULL_HANDLE;

        std::vector<VkLayerProperties>     m_Layers;
        std::vector<VkExtensionProperties> m_Extensions;
        std::vector<VkPhysicalDevice>      m_PhysicalDevices;
    };
}
