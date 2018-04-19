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

#include <limits>
#include "VulkanErrors.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanDebug.h"

namespace VulkanUtilities
{
    std::shared_ptr<VulkanLogicalDevice> VulkanLogicalDevice::Create(VkPhysicalDevice vkPhysicalDevice,
                                                                     const VkDeviceCreateInfo &DeviceCI, 
                                                                     const VkAllocationCallbacks* vkAllocator,
                                                                     bool EnableDebugMarkers)
    {
        auto *LogicalDevice = new VulkanLogicalDevice(vkPhysicalDevice, DeviceCI, vkAllocator, EnableDebugMarkers);
        return std::shared_ptr<VulkanLogicalDevice>(LogicalDevice);
    }

    VulkanLogicalDevice::~VulkanLogicalDevice()
    {
        vkDestroyDevice(m_VkDevice, m_VkAllocator);
    }

    VulkanLogicalDevice::VulkanLogicalDevice(VkPhysicalDevice vkPhysicalDevice, 
                                             const VkDeviceCreateInfo &DeviceCI, 
                                             const VkAllocationCallbacks* vkAllocator,
                                             bool EnableDebugMarkers) :
        m_VkAllocator(vkAllocator)
    {
        auto res = vkCreateDevice(vkPhysicalDevice, &DeviceCI, vkAllocator, &m_VkDevice);
        CHECK_VK_ERROR_AND_THROW(res, "Failed to create logical device");

        if (EnableDebugMarkers)
        {
            SetupDebugMarkers(m_VkDevice);
        }
    }

    VkQueue VulkanLogicalDevice::GetQueue(uint32_t queueFamilyIndex, uint32_t queueIndex)
    {
        VkQueue vkQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(m_VkDevice,
            queueFamilyIndex, // Index of the queue family to which the queue belongs
            0,                          // Index within this queue family of the queue to retrieve
            &vkQueue);
        VERIFY_EXPR(vkQueue != VK_NULL_HANDLE);
        return vkQueue;
    }

    void VulkanLogicalDevice::WaitIdle()const
    {
        auto err = vkDeviceWaitIdle(m_VkDevice);
        VERIFY_EXPR(err == VK_SUCCESS);
    }

    template<typename VkObjectType, typename VkCreateObjectFuncType, typename VkObjectCreateInfoType>
    VulkanObjectWrapper<VkObjectType> VulkanLogicalDevice::CreateVulkanObject(VkCreateObjectFuncType VkCreateObject,
                                                                              const VkObjectCreateInfoType& CreateInfo,
                                                                              const char *DebugName,
                                                                              const char *ObjectType)const
    {
        if (DebugName == nullptr)
            DebugName = "";

        VkObjectType VkObject = VK_NULL_HANDLE;
        auto err = VkCreateObject(m_VkDevice, &CreateInfo, m_VkAllocator, &VkObject);
        CHECK_VK_ERROR_AND_THROW(err, "Failed to create Vulkan ", ObjectType, " '", DebugName, '\'');
        
        if (DebugName != nullptr && *DebugName != 0)
            SetVulkanObjectName(m_VkDevice, VkObject, DebugName);

        return VulkanObjectWrapper<VkObjectType>{ GetSharedPtr(), std::move(VkObject) };
    }

    CommandPoolWrapper VulkanLogicalDevice::CreateCommandPool(const VkCommandPoolCreateInfo &CmdPoolCI, 
                                                              const char *DebugName) const
    {
        return CreateVulkanObject<VkCommandPool>(vkCreateCommandPool, CmdPoolCI, DebugName, "command pool");
    }

    BufferWrapper VulkanLogicalDevice::CreateBuffer(const VkBufferCreateInfo &BufferCI, 
                                                    const char *DebugName)const
    {
        return CreateVulkanObject<VkBuffer>(vkCreateBuffer, BufferCI, DebugName, "buffer");
    }

    BufferViewWrapper VulkanLogicalDevice::CreateBufferView(const VkBufferViewCreateInfo  &BuffViewCI, 
                                                            const char *DebugName)const
    {
        return CreateVulkanObject<VkBufferView>(vkCreateBufferView, BuffViewCI, DebugName, "buffer view");
    }

    ImageWrapper VulkanLogicalDevice::CreateImage(const VkImageCreateInfo &ImageCI, 
                                                  const char *DebugName)const
    {
        return CreateVulkanObject<VkImage>(vkCreateImage, ImageCI, DebugName, "image");
    }

    ImageViewWrapper VulkanLogicalDevice::CreateImageView(const VkImageViewCreateInfo &ImageViewCI, 
                                                          const char *DebugName)const
    {
        return CreateVulkanObject<VkImageView>(vkCreateImageView, ImageViewCI, DebugName, "image view");
    }

    SamplerWrapper VulkanLogicalDevice::CreateSampler(const VkSamplerCreateInfo &SamplerCI, const char *DebugName)const
    {
        return CreateVulkanObject<VkSampler>(vkCreateSampler, SamplerCI, DebugName, "sampler");
    }

    FenceWrapper VulkanLogicalDevice::CreateFence(const VkFenceCreateInfo &FenceCI, const char *DebugName)const
    {
        return CreateVulkanObject<VkFence>(vkCreateFence, FenceCI, DebugName, "fence");
    }

    RenderPassWrapper VulkanLogicalDevice::CreateRenderPass(const VkRenderPassCreateInfo  &RenderPassCI, const char *DebugName)const
    {
        return CreateVulkanObject<VkRenderPass>(vkCreateRenderPass, RenderPassCI, DebugName, "render pass");
    }

    DeviceMemoryWrapper VulkanLogicalDevice::AllocateDeviceMemory(const VkMemoryAllocateInfo &AllocInfo, 
                                                                  const char *DebugName)const
    {
        if (DebugName == nullptr)
            DebugName = "";

        VkDeviceMemory vkDeviceMem = VK_NULL_HANDLE;

        auto err = vkAllocateMemory(m_VkDevice, &AllocInfo, m_VkAllocator, &vkDeviceMem);
        CHECK_VK_ERROR_AND_THROW(err, "Failed to allocate device memory '", DebugName, '\'');

        if (DebugName != nullptr && *DebugName != 0)
            SetDeviceMemoryName(m_VkDevice, vkDeviceMem, DebugName);

        return DeviceMemoryWrapper{ GetSharedPtr(), std::move(vkDeviceMem) };
    }

    PipelineWrapper VulkanLogicalDevice::CreateComputePipeline(const VkComputePipelineCreateInfo  &PipelineCI, 
                                                               VkPipelineCache cache, 
                                                               const char *DebugName)const
    {
        if (DebugName == nullptr)
            DebugName = "";

        VkPipeline vkPipeline = VK_NULL_HANDLE;
        auto err = vkCreateComputePipelines(m_VkDevice, cache, 1, &PipelineCI, m_VkAllocator, &vkPipeline);
        CHECK_VK_ERROR_AND_THROW(err, "Failed to create compute pipeline '", DebugName, '\'');

        if (DebugName != nullptr && *DebugName != 0)
            SetPipelineName(m_VkDevice, vkPipeline, DebugName);

        return PipelineWrapper{ GetSharedPtr(), std::move(vkPipeline) };
    }

    PipelineWrapper VulkanLogicalDevice::CreateGraphicsPipeline(const VkGraphicsPipelineCreateInfo &PipelineCI, 
                                                                VkPipelineCache cache, 
                                                                const char *DebugName)const
    {
        if (DebugName == nullptr)
            DebugName = "";

        VkPipeline vkPipeline = VK_NULL_HANDLE;
        auto err = vkCreateGraphicsPipelines(m_VkDevice, cache, 1, &PipelineCI, m_VkAllocator, &vkPipeline);
        CHECK_VK_ERROR_AND_THROW(err, "Failed to create graphics pipeline '", DebugName, '\'');

        if (DebugName != nullptr && *DebugName != 0)
            SetPipelineName(m_VkDevice, vkPipeline, DebugName);

        return PipelineWrapper{ GetSharedPtr(), std::move(vkPipeline) };
    }

    ShaderModuleWrapper VulkanLogicalDevice::CreateShaderModule(const VkShaderModuleCreateInfo &ShaderModuleCI, const char *DebugName)const
    {
        return CreateVulkanObject<VkShaderModule>(vkCreateShaderModule, ShaderModuleCI, DebugName, "shader module");
    }

    PipelineLayoutWrapper VulkanLogicalDevice::CreatePipelineLayout(const VkPipelineLayoutCreateInfo &PipelineLayoutCI, const char *DebugName)const
    {
        return CreateVulkanObject<VkPipelineLayout>(vkCreatePipelineLayout, PipelineLayoutCI, DebugName, "pipeline layout");
    }

    FramebufferWrapper VulkanLogicalDevice::CreateFramebuffer(const VkFramebufferCreateInfo &FramebufferCI, const char *DebugName)const
    {
        return CreateVulkanObject<VkFramebuffer>(vkCreateFramebuffer, FramebufferCI, DebugName, "framebuffer");
    }

    DescriptorPoolWrapper VulkanLogicalDevice::CreateDescriptorPool(const VkDescriptorPoolCreateInfo &DescrPoolCI, const char *DebugName)const
    {
        return CreateVulkanObject<VkDescriptorPool>(vkCreateDescriptorPool, DescrPoolCI, DebugName, "descriptor pool");
    }

    DescriptorSetLayoutWrapper VulkanLogicalDevice::CreateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo &LayoutCI, const char *DebugName)const
    {
        return CreateVulkanObject<VkDescriptorSetLayout>(vkCreateDescriptorSetLayout, LayoutCI, DebugName, "descriptor set layout");
    }

    SemaphoreWrapper VulkanLogicalDevice::CreateSemaphore(const VkSemaphoreCreateInfo &SemaphoreCI, const char *DebugName)const
    {
        return CreateVulkanObject<VkSemaphore>(vkCreateSemaphore, SemaphoreCI, DebugName, "semaphore");
    }

    VkCommandBuffer VulkanLogicalDevice::AllocateVkCommandBuffer(const VkCommandBufferAllocateInfo &AllocInfo, const char *DebugName)const
    {
        if (DebugName == nullptr)
            DebugName = "";

        VkCommandBuffer CmdBuff = VK_NULL_HANDLE;
        auto err = vkAllocateCommandBuffers(m_VkDevice, &AllocInfo, &CmdBuff);
        VERIFY(err == VK_SUCCESS, "Failed to allocate command buffer '", DebugName, '\'');

        if (DebugName != nullptr && *DebugName != 0)
            SetCommandBufferName(m_VkDevice, CmdBuff, DebugName);

        return CmdBuff;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(CommandPoolWrapper &&CmdPool)const
    {
        vkDestroyCommandPool(m_VkDevice, CmdPool.m_VkObject, m_VkAllocator);
        CmdPool.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(BufferWrapper&& Buffer)const
    {
        vkDestroyBuffer(m_VkDevice, Buffer.m_VkObject, m_VkAllocator);
        Buffer.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(BufferViewWrapper&& BufferView)const
    {
        vkDestroyBufferView(m_VkDevice, BufferView.m_VkObject, m_VkAllocator);
        BufferView.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(ImageWrapper&& Image)const
    {
        vkDestroyImage(m_VkDevice, Image.m_VkObject, m_VkAllocator);
        Image.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(ImageViewWrapper&& ImageView)const
    {
        vkDestroyImageView(m_VkDevice, ImageView.m_VkObject, m_VkAllocator);
        ImageView.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(SamplerWrapper&& Sampler)const
    {
        vkDestroySampler(m_VkDevice, Sampler.m_VkObject, m_VkAllocator);
        Sampler.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(FenceWrapper&& Fence)const
    {
        vkDestroyFence(m_VkDevice, Fence.m_VkObject, m_VkAllocator);
        Fence.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(RenderPassWrapper&& RenderPass)const
    {
        vkDestroyRenderPass(m_VkDevice, RenderPass.m_VkObject, m_VkAllocator);
        RenderPass.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(DeviceMemoryWrapper&& Memory)const
    {
        vkFreeMemory(m_VkDevice, Memory.m_VkObject, m_VkAllocator);
        Memory.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(PipelineWrapper&& Pipeline)const
    {
        vkDestroyPipeline(m_VkDevice, Pipeline.m_VkObject, m_VkAllocator);
        Pipeline.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(ShaderModuleWrapper&& ShaderModule)const
    {
        vkDestroyShaderModule(m_VkDevice, ShaderModule.m_VkObject, m_VkAllocator);
        ShaderModule.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(PipelineLayoutWrapper&& PipelineLayout)const
    {
        vkDestroyPipelineLayout(m_VkDevice, PipelineLayout.m_VkObject, m_VkAllocator);
        PipelineLayout.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(FramebufferWrapper&& Framebuffer)const
    {
        vkDestroyFramebuffer(m_VkDevice, Framebuffer.m_VkObject, m_VkAllocator);
        Framebuffer.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(DescriptorPoolWrapper&& DescriptorPool)const
    {
        vkDestroyDescriptorPool(m_VkDevice, DescriptorPool.m_VkObject, m_VkAllocator);
        DescriptorPool.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(DescriptorSetLayoutWrapper&& DescriptorSetLayout)const
    {
        vkDestroyDescriptorSetLayout(m_VkDevice, DescriptorSetLayout.m_VkObject, m_VkAllocator);
        DescriptorSetLayout.m_VkObject = VK_NULL_HANDLE;
    }

    void VulkanLogicalDevice::ReleaseVulkanObject(SemaphoreWrapper&& Semaphore)const
    {
        vkDestroySemaphore(m_VkDevice, Semaphore.m_VkObject, m_VkAllocator);
        Semaphore.m_VkObject = VK_NULL_HANDLE;
    }





    VkMemoryRequirements VulkanLogicalDevice::GetBufferMemoryRequirements(VkBuffer vkBuffer)const
    {
        VkMemoryRequirements MemReqs = {};
        vkGetBufferMemoryRequirements(m_VkDevice, vkBuffer, &MemReqs);
        return MemReqs;
    }

    VkMemoryRequirements VulkanLogicalDevice::GetImageMemoryRequirements(VkImage vkImage)const
    {
        VkMemoryRequirements MemReqs = {};
        vkGetImageMemoryRequirements(m_VkDevice, vkImage, &MemReqs);
        return MemReqs;
    }

    VkResult VulkanLogicalDevice::BindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)const
    {
        return vkBindBufferMemory(m_VkDevice, buffer, memory, memoryOffset);
    }

    VkResult VulkanLogicalDevice::BindImageMemory(VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)const
    {
        return vkBindImageMemory(m_VkDevice, image, memory, memoryOffset);
    }

    VkResult VulkanLogicalDevice::MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)const
    {
        return vkMapMemory(m_VkDevice, memory, offset, size, flags, ppData);
    }

    void VulkanLogicalDevice::UnmapMemory(VkDeviceMemory memory)const
    {
        vkUnmapMemory(m_VkDevice, memory);
    }

    VkResult VulkanLogicalDevice::GetFenceStatus(VkFence fence)const
    {
        return vkGetFenceStatus(m_VkDevice, fence);
    }

    VkResult VulkanLogicalDevice::ResetFence(VkFence fence)const
    {
        auto err = vkResetFences(m_VkDevice, 1, &fence);
        VERIFY(err == VK_SUCCESS, "Failed to reset fence");
        return err;
    }
}
