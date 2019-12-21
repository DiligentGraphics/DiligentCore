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

#include <array>

#include "TestingEnvironment.h"

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

namespace Diligent
{

namespace Testing
{

class TestingEnvironmentVk final : public TestingEnvironment
{
public:
    TestingEnvironmentVk(DeviceType deviceType, ADAPTER_TYPE AdapterType, const SwapChainDesc& SCDesc);
    ~TestingEnvironmentVk();

    static TestingEnvironmentVk* GetInstance() { return ValidatedCast<TestingEnvironmentVk>(TestingEnvironment::GetInstance()); }

    void CreateImage2D(uint32_t          Width,
                       uint32_t          Height,
                       VkFormat          vkFormat,
                       VkImageUsageFlags vkUsage,
                       VkImageLayout     vkInitialLayout,
                       VkDeviceMemory&   vkMemory,
                       VkImage&          vkImage);

    void CreateBuffer(VkDeviceSize          Size,
                      VkBufferUsageFlags    vkUsage,
                      VkMemoryPropertyFlags MemoryFlags,
                      VkDeviceMemory&       vkMemory,
                      VkBuffer&             vkBuffer);

    uint32_t GetMemoryTypeIndex(uint32_t              memoryTypeBitsRequirement,
                                VkMemoryPropertyFlags requiredProperties) const;

    VkDevice GetVkDevice()
    {
        return m_vkDevice;
    }

    VkShaderModule CreateShaderModule(const SHADER_TYPE ShaderType, const std::string& ShaderSource);

    static VkRenderPassCreateInfo GetRenderPassCreateInfo(
        Uint32                                                     NumRenderTargets,
        const VkFormat                                             RTVFormats[],
        VkFormat                                                   DSVFormat,
        Uint32                                                     SampleCount,
        VkAttachmentLoadOp                                         DepthAttachmentLoadOp,
        VkAttachmentLoadOp                                         ColorAttachmentLoadOp,
        std::array<VkAttachmentDescription, MaxRenderTargets + 1>& Attachments,
        std::array<VkAttachmentReference, MaxRenderTargets + 1>&   AttachmentReferences,
        VkSubpassDescription&                                      SubpassDesc);

    VkCommandBuffer AllocateCommandBuffer();

    static void TransitionImageLayout(VkCommandBuffer                CmdBuffer,
                                      VkImage                        Image,
                                      VkImageLayout&                 CurrentLayout,
                                      VkImageLayout                  NewLayout,
                                      const VkImageSubresourceRange& SubresRange,
                                      VkPipelineStageFlags           EnabledGraphicsShaderStages,
                                      VkPipelineStageFlags           SrcStages  = 0,
                                      VkPipelineStageFlags           DestStages = 0);

private:
    VkDevice      m_vkDevice  = VK_NULL_HANDLE;
    VkCommandPool m_vkCmdPool = VK_NULL_HANDLE;

    VkPhysicalDeviceMemoryProperties m_MemoryProperties = {};
};

} // namespace Testing

} // namespace Diligent
