/*     Copyright 2015-2019 Egor Yusov
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
    template<typename VulkanObjectType>
    class VulkanObjectWrapper;

    using CommandPoolWrapper    = VulkanObjectWrapper<VkCommandPool>;
    using BufferWrapper         = VulkanObjectWrapper<VkBuffer>;
    using BufferViewWrapper     = VulkanObjectWrapper<VkBufferView>;
    using ImageWrapper          = VulkanObjectWrapper<VkImage>;
    using ImageViewWrapper      = VulkanObjectWrapper<VkImageView>;
    using DeviceMemoryWrapper   = VulkanObjectWrapper<VkDeviceMemory>;
    using FenceWrapper          = VulkanObjectWrapper<VkFence>;
    using RenderPassWrapper     = VulkanObjectWrapper<VkRenderPass>;
    using PipelineWrapper       = VulkanObjectWrapper<VkPipeline>;
    using ShaderModuleWrapper   = VulkanObjectWrapper<VkShaderModule>;
    using PipelineLayoutWrapper = VulkanObjectWrapper<VkPipelineLayout>;
    using SamplerWrapper        = VulkanObjectWrapper<VkSampler>;
    using FramebufferWrapper    = VulkanObjectWrapper<VkFramebuffer>;
    using DescriptorPoolWrapper = VulkanObjectWrapper<VkDescriptorPool>;
    using DescriptorSetLayoutWrapper = VulkanObjectWrapper<VkDescriptorSetLayout>;
    using SemaphoreWrapper      = VulkanObjectWrapper<VkSemaphore>;

    class VulkanLogicalDevice : public std::enable_shared_from_this<VulkanLogicalDevice>
    {
    public:
        static std::shared_ptr<VulkanLogicalDevice> Create(VkPhysicalDevice             vkPhysicalDevice, 
                                                           const VkDeviceCreateInfo&    DeviceCI, 
                                                           const VkAllocationCallbacks* vkAllocator);

        VulkanLogicalDevice             (const VulkanLogicalDevice&) = delete;
        VulkanLogicalDevice             (VulkanLogicalDevice&&)      = delete;
        VulkanLogicalDevice& operator = (const VulkanLogicalDevice&) = delete;
        VulkanLogicalDevice& operator = (VulkanLogicalDevice&&)      = delete;

        ~VulkanLogicalDevice();

        std::shared_ptr<VulkanLogicalDevice> GetSharedPtr()
        {
            return shared_from_this();
        }

        std::shared_ptr<const VulkanLogicalDevice> GetSharedPtr()const
        {
            return shared_from_this();
        }

        VkQueue GetQueue(uint32_t queueFamilyIndex, uint32_t queueIndex);

        VkDevice GetVkDevice()const
        { 
            return m_VkDevice; 
        }

        void WaitIdle()const;

        CommandPoolWrapper  CreateCommandPool   (const VkCommandPoolCreateInfo &CmdPoolCI,   const char* DebugName = "")const;
        BufferWrapper       CreateBuffer        (const VkBufferCreateInfo      &BufferCI,    const char* DebugName = "")const;
        BufferViewWrapper   CreateBufferView    (const VkBufferViewCreateInfo  &BuffViewCI,  const char* DebugName = "")const;
        ImageWrapper        CreateImage         (const VkImageCreateInfo       &ImageCI,     const char* DebugName = "")const;
        ImageViewWrapper    CreateImageView     (const VkImageViewCreateInfo   &ImageViewCI, const char* DebugName = "")const;
        SamplerWrapper      CreateSampler       (const VkSamplerCreateInfo     &SamplerCI,   const char* DebugName = "")const;
        FenceWrapper        CreateFence         (const VkFenceCreateInfo       &FenceCI,     const char* DebugName = "")const;
        RenderPassWrapper   CreateRenderPass    (const VkRenderPassCreateInfo  &RenderPassCI,const char* DebugName = "")const;
        DeviceMemoryWrapper AllocateDeviceMemory(const VkMemoryAllocateInfo    &AllocInfo,   const char* DebugName = "")const;
        PipelineWrapper     CreateComputePipeline (const VkComputePipelineCreateInfo  &PipelineCI, VkPipelineCache cache, const char* DebugName = "")const;
        PipelineWrapper     CreateGraphicsPipeline(const VkGraphicsPipelineCreateInfo &PipelineCI, VkPipelineCache cache, const char* DebugName = "")const;
        ShaderModuleWrapper CreateShaderModule    (const VkShaderModuleCreateInfo &ShaderModuleCI,  const char* DebugName = "")const;
        PipelineLayoutWrapper CreatePipelineLayout(const VkPipelineLayoutCreateInfo &LayoutCI,      const char* DebugName = "")const;
        FramebufferWrapper    CreateFramebuffer   (const VkFramebufferCreateInfo    &FramebufferCI, const char* DebugName = "")const;
        DescriptorPoolWrapper CreateDescriptorPool(const VkDescriptorPoolCreateInfo &DescrPoolCI,   const char* DebugName = "")const;
        DescriptorSetLayoutWrapper CreateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo &LayoutCI, const char* DebugName = "")const;
        SemaphoreWrapper    CreateSemaphore(const VkSemaphoreCreateInfo &SemaphoreCI, const char* DebugName = "")const;

        VkCommandBuffer     AllocateVkCommandBuffer(const VkCommandBufferAllocateInfo &AllocInfo, const char* DebugName = "")const;
        VkDescriptorSet     AllocateVkDescriptorSet(const VkDescriptorSetAllocateInfo &AllocInfo, const char* DebugName = "")const;

        void ReleaseVulkanObject(CommandPoolWrapper&&  CmdPool)const;
        void ReleaseVulkanObject(BufferWrapper&&       Buffer)const;
        void ReleaseVulkanObject(BufferViewWrapper&&   BufferView)const;
        void ReleaseVulkanObject(ImageWrapper&&        Image)const;
        void ReleaseVulkanObject(ImageViewWrapper&&    ImageView)const;
        void ReleaseVulkanObject(SamplerWrapper&&      Sampler)const;
        void ReleaseVulkanObject(FenceWrapper&&        Fence)const;
        void ReleaseVulkanObject(RenderPassWrapper&&   RenderPass)const;
        void ReleaseVulkanObject(DeviceMemoryWrapper&& Memory)const;
        void ReleaseVulkanObject(PipelineWrapper&&     Pipeline)const;
        void ReleaseVulkanObject(ShaderModuleWrapper&& ShaderModule)const;
        void ReleaseVulkanObject(PipelineLayoutWrapper&& PipelineLayout)const;
        void ReleaseVulkanObject(FramebufferWrapper&&   Framebuffer)const;
        void ReleaseVulkanObject(DescriptorPoolWrapper&& DescriptorPool)const;
        void ReleaseVulkanObject(DescriptorSetLayoutWrapper&& DescriptorSetLayout)const;
        void ReleaseVulkanObject(SemaphoreWrapper&&     Semaphore)const;

        void FreeDescriptorSet(VkDescriptorPool Pool, VkDescriptorSet Set)const;

        VkMemoryRequirements GetBufferMemoryRequirements(VkBuffer vkBuffer)const;
        VkMemoryRequirements GetImageMemoryRequirements (VkImage vkImage  )const;

        VkResult BindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)const;
        VkResult BindImageMemory (VkImage image,   VkDeviceMemory memory, VkDeviceSize memoryOffset)const;

        VkResult MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)const;
        void UnmapMemory(VkDeviceMemory memory)const;

        VkResult GetFenceStatus(VkFence fence)const;
        VkResult ResetFence(VkFence fence)const;
        VkResult WaitForFences(uint32_t          fenceCount,
                               const VkFence*    pFences,
                               VkBool32          waitAll,
                               uint64_t          timeout)const;

        void UpdateDescriptorSets(uint32_t                      descriptorWriteCount, 
                                  const VkWriteDescriptorSet*   pDescriptorWrites,
                                  uint32_t                      descriptorCopyCount,
                                  const VkCopyDescriptorSet*    pDescriptorCopies)const;

        VkResult ResetCommandPool(VkCommandPool             vkCmdPool,
                                  VkCommandPoolResetFlags   flags = 0)const;

        VkResult ResetDescriptorPool(VkDescriptorPool           descriptorPool,
                                     VkDescriptorPoolResetFlags flags = 0)const;

        VkPipelineStageFlags GetEnabledGraphicsShaderStages()const { return m_EnabledGraphicsShaderStages; }

    private:
        VulkanLogicalDevice(VkPhysicalDevice vkPhysicalDevice, 
                            const VkDeviceCreateInfo &DeviceCI, 
                            const VkAllocationCallbacks* vkAllocator);

        template<typename VkObjectType, typename VkCreateObjectFuncType, typename VkObjectCreateInfoType>
        VulkanObjectWrapper<VkObjectType> CreateVulkanObject(VkCreateObjectFuncType        VkCreateObject,
                                                             const VkObjectCreateInfoType& CreateInfo,
                                                             const char*                   DebugName,
                                                             const char*                   ObjectType)const;

        VkDevice m_VkDevice = VK_NULL_HANDLE;
        const VkAllocationCallbacks* const m_VkAllocator;
        VkPipelineStageFlags m_EnabledGraphicsShaderStages = 0;
    };
}
