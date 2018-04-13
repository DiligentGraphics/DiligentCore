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

#include <memory>
#include "vulkan.h"

namespace VulkanUtilities
{
    class VulkanLogicalDevice;

    template<typename VulkanObjectType>
    class VulkanObjectWrapper
    {
    public:
        using VkObjectType = VulkanObjectType;

        VulkanObjectWrapper() : 
            m_pLogicalDevice(nullptr),
            m_VkObject(VK_NULL_HANDLE)
        {}
        VulkanObjectWrapper(std::shared_ptr<const VulkanLogicalDevice> pLogicalDevice, VulkanObjectType&& vkObject) :
            m_pLogicalDevice(pLogicalDevice),
            m_VkObject(vkObject)
        {
            vkObject = VK_NULL_HANDLE;
        }
        VulkanObjectWrapper(const VulkanObjectWrapper&) = delete;
        VulkanObjectWrapper& operator = (const VulkanObjectWrapper&) = delete;
        VulkanObjectWrapper(VulkanObjectWrapper&& rhs) : 
            m_pLogicalDevice(std::move(rhs.m_pLogicalDevice)),
            m_VkObject(rhs.m_VkObject)
        {
            rhs.m_VkObject = VK_NULL_HANDLE;
        }
        VulkanObjectWrapper& operator = (VulkanObjectWrapper&& rhs) 
        {
            m_pLogicalDevice = std::move(rhs.m_pLogicalDevice);
            m_VkObject = rhs.m_VkObject;
            rhs.m_VkObject = VK_NULL_HANDLE;
            return *this;
        }

        operator VulkanObjectType()const
        {
            return m_VkObject;
        }

        void Release()
        {
            // For externally managed objects, m_pLogicalDevice is null
            if(m_pLogicalDevice && m_VkObject != VK_NULL_HANDLE)
            {
                m_pLogicalDevice->ReleaseVulkanObject(std::move(*this));
            }
            m_VkObject = VK_NULL_HANDLE;
            m_pLogicalDevice.reset();
        }

        ~VulkanObjectWrapper()
        {
            Release();
        }

    private:
        friend class VulkanLogicalDevice;

        std::shared_ptr<const VulkanLogicalDevice> m_pLogicalDevice;
        VulkanObjectType m_VkObject;
    };

    using CommandPoolWrapper  = VulkanObjectWrapper<VkCommandPool>;
    using BufferWrapper       = VulkanObjectWrapper<VkBuffer>;
    using BufferViewWrapper   = VulkanObjectWrapper<VkBufferView>;
    using ImageWrapper        = VulkanObjectWrapper<VkImage>;
    using ImageViewWrapper    = VulkanObjectWrapper<VkImageView>;
    using DeviceMemoryWrapper = VulkanObjectWrapper<VkDeviceMemory>;
    using FenceWrapper        = VulkanObjectWrapper<VkFence>;
    using RenderPassWrapper   = VulkanObjectWrapper<VkRenderPass>;
    using PipelineWrapper     = VulkanObjectWrapper<VkPipeline>;
}
