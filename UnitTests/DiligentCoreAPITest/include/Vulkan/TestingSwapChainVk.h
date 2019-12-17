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

#include "TestingSwapChainBase.h"

#include "vulkan.h"

namespace Diligent
{

namespace Testing
{

class TestingEnvironmentVk;

class TestingSwapChainVk : public TestingSwapChainBase<ISwapChain>
{
public:
    using TBase = TestingSwapChainBase<ISwapChain>;
    TestingSwapChainVk(IReferenceCounters*   pRefCounters,
                       TestingEnvironmentVk* pEnv,
                       const SwapChainDesc&  SCDesc);
    ~TestingSwapChainVk();

    virtual void TakeSnapshot() override final;

    VkRenderPass GetRenderPass()
    {
        return m_vkRenderPass;
    }

    void BeginRenderPass(VkCommandBuffer vkCmdBuffer, VkPipelineStageFlags GraphicsShaderStages);
    void EndRenderPass(VkCommandBuffer vkCmdBuffer);

private:
    void CreateFramebuffer();

    VkDevice m_vkDevice = VK_NULL_HANDLE;

    VkDeviceMemory m_vkRenderTargetMemory = VK_NULL_HANDLE;
    VkImage        m_vkRenderTargetImage  = VK_NULL_HANDLE;
    VkImageLayout  m_vkRenerTargetLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageView    m_vkRenderTargetView   = VK_NULL_HANDLE;

    VkDeviceMemory m_vkDepthBufferMemory = VK_NULL_HANDLE;
    VkImage        m_vkDepthBufferImage  = VK_NULL_HANDLE;
    VkImageLayout  m_vkDepthBufferLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageView    m_vkDepthBufferView   = VK_NULL_HANDLE;

    VkDeviceSize   m_StagingBufferSize     = 0;
    VkDeviceMemory m_vkStagingBufferMemory = VK_NULL_HANDLE;
    VkBuffer       m_vkStagingBuffer       = VK_NULL_HANDLE;

    VkRenderPass  m_vkRenderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_vkFramebuffer = VK_NULL_HANDLE;

    VkPipelineStageFlags m_ActiveGraphicsShaderStages = 0;
};

} // namespace Testing

} // namespace Diligent
