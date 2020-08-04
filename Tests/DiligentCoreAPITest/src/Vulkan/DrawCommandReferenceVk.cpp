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

#include "Vulkan/TestingEnvironmentVk.hpp"
#include "Vulkan/TestingSwapChainVk.hpp"

#include "DeviceContextVk.h"

#include "volk/volk.h"

#include "InlineShaders/DrawCommandTestGLSL.h"

namespace Diligent
{

namespace Testing
{

namespace
{

struct ReferenceTriangleRenderer
{
    ReferenceTriangleRenderer(ISwapChain* pSwapChain, VkRenderPass vkRenderPass)
    {
        auto* pEnv     = TestingEnvironmentVk::GetInstance();
        auto  vkDevice = pEnv->GetVkDevice();

        const auto& SCDesc = pSwapChain->GetDesc();

        VkResult res = VK_SUCCESS;
        (void)res;

        vkVSModule = pEnv->CreateShaderModule(SHADER_TYPE_VERTEX, GLSL::DrawTest_ProceduralTriangleVS);
        VERIFY_EXPR(vkVSModule != VK_NULL_HANDLE);
        vkPSModule = pEnv->CreateShaderModule(SHADER_TYPE_PIXEL, GLSL::DrawTest_FS);
        VERIFY_EXPR(vkPSModule != VK_NULL_HANDLE);

        VkGraphicsPipelineCreateInfo PipelineCI = {};

        PipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        PipelineCI.pNext = nullptr;

        VkPipelineShaderStageCreateInfo ShaderStages[2] = {};

        ShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ShaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        ShaderStages[0].module = vkVSModule;
        ShaderStages[0].pName  = "main";

        ShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        ShaderStages[1].module = vkPSModule;
        ShaderStages[1].pName  = "main";

        PipelineCI.pStages    = ShaderStages;
        PipelineCI.stageCount = _countof(ShaderStages);

        VkPipelineLayoutCreateInfo PipelineLayoutCI = {};

        PipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        vkCreatePipelineLayout(vkDevice, &PipelineLayoutCI, nullptr, &vkLayout);
        VERIFY_EXPR(vkLayout != VK_NULL_HANDLE);
        PipelineCI.layout = vkLayout;

        VkPipelineVertexInputStateCreateInfo VertexInputStateCI = {};

        VertexInputStateCI.sType     = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        PipelineCI.pVertexInputState = &VertexInputStateCI;


        VkPipelineInputAssemblyStateCreateInfo InputAssemblyCI = {};

        InputAssemblyCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        InputAssemblyCI.pNext                  = nullptr;
        InputAssemblyCI.flags                  = 0; // reserved for future use
        InputAssemblyCI.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        InputAssemblyCI.primitiveRestartEnable = VK_FALSE;
        PipelineCI.pInputAssemblyState         = &InputAssemblyCI;


        VkPipelineTessellationStateCreateInfo TessStateCI = {};

        TessStateCI.sType             = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        TessStateCI.pNext             = nullptr;
        TessStateCI.flags             = 0; // reserved for future use
        PipelineCI.pTessellationState = &TessStateCI;


        VkPipelineViewportStateCreateInfo ViewPortStateCI = {};

        ViewPortStateCI.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        ViewPortStateCI.pNext         = nullptr;
        ViewPortStateCI.flags         = 0; // reserved for future use
        ViewPortStateCI.viewportCount = 1;

        VkViewport Viewport = {};
        Viewport.y          = static_cast<float>(SCDesc.Height);
        Viewport.width      = static_cast<float>(SCDesc.Width);
        Viewport.height     = -static_cast<float>(SCDesc.Height);
        Viewport.maxDepth   = 1;

        ViewPortStateCI.pViewports   = &Viewport;
        ViewPortStateCI.scissorCount = ViewPortStateCI.viewportCount; // the number of scissors must match the number of viewports (23.5)
                                                                      // (why the hell it is in the struct then?)

        VkRect2D ScissorRect      = {};
        ScissorRect.extent.width  = SCDesc.Width;
        ScissorRect.extent.height = SCDesc.Height;
        ViewPortStateCI.pScissors = &ScissorRect;

        PipelineCI.pViewportState = &ViewPortStateCI;

        VkPipelineRasterizationStateCreateInfo RasterizerStateCI = {};

        RasterizerStateCI.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        RasterizerStateCI.polygonMode = VK_POLYGON_MODE_FILL;
        RasterizerStateCI.cullMode    = VK_CULL_MODE_NONE;
        RasterizerStateCI.lineWidth   = 1;

        PipelineCI.pRasterizationState = &RasterizerStateCI;

        // Multisample state (24)
        VkPipelineMultisampleStateCreateInfo MSStateCI = {};

        MSStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        MSStateCI.pNext = nullptr;
        MSStateCI.flags = 0; // reserved for future use
        // If subpass uses color and/or depth/stencil attachments, then the rasterizationSamples member of
        // pMultisampleState must be the same as the sample count for those subpass attachments
        MSStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        MSStateCI.sampleShadingEnable  = VK_FALSE;
        MSStateCI.minSampleShading     = 0;               // a minimum fraction of sample shading if sampleShadingEnable is set to VK_TRUE.
        uint32_t SampleMask[]          = {0xFFFFFFFF, 0}; // Vulkan spec allows up to 64 samples
        MSStateCI.pSampleMask          = SampleMask;      // an array of static coverage information that is ANDed with
                                                          // the coverage information generated during rasterization (25.3)
        MSStateCI.alphaToCoverageEnable = VK_FALSE;       // whether a temporary coverage value is generated based on
                                                          // the alpha component of the fragment's first color output
        MSStateCI.alphaToOneEnable   = VK_FALSE;          // whether the alpha component of the fragment's first color output is replaced with one
        PipelineCI.pMultisampleState = &MSStateCI;


        VkPipelineDepthStencilStateCreateInfo DepthStencilStateCI = {};

        DepthStencilStateCI.sType     = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        PipelineCI.pDepthStencilState = &DepthStencilStateCI;


        VkPipelineColorBlendStateCreateInfo BlendStateCI = {};

        VkPipelineColorBlendAttachmentState Attachment = {};

        Attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        BlendStateCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        BlendStateCI.pAttachments    = &Attachment;
        BlendStateCI.attachmentCount = 1; //  must equal the colorAttachmentCount for the subpass
                                          // in which this pipeline is used.
        PipelineCI.pColorBlendState = &BlendStateCI;


        VkPipelineDynamicStateCreateInfo DynamicStateCI = {};

        DynamicStateCI.sType     = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        PipelineCI.pDynamicState = &DynamicStateCI;

        PipelineCI.renderPass         = vkRenderPass;
        PipelineCI.subpass            = 0;
        PipelineCI.basePipelineHandle = VK_NULL_HANDLE; // a pipeline to derive from
        PipelineCI.basePipelineIndex  = 0;              // an index into the pCreateInfos parameter to use as a pipeline to derive from

        res = vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &PipelineCI, nullptr, &vkPipeline);
        VERIFY_EXPR(res >= 0);
        VERIFY_EXPR(vkPipeline != VK_NULL_HANDLE);
    }

    void Draw(VkCommandBuffer vkCmdBuffer)
    {
        vkCmdBindPipeline(vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
        vkCmdDraw(vkCmdBuffer, 6, 1, 0, 0);
    }

    ~ReferenceTriangleRenderer()
    {
        auto* pEnv     = TestingEnvironmentVk::GetInstance();
        auto  vkDevice = pEnv->GetVkDevice();

        vkDestroyPipeline(vkDevice, vkPipeline, nullptr);
        vkDestroyPipelineLayout(vkDevice, vkLayout, nullptr);
        vkDestroyShaderModule(vkDevice, vkVSModule, nullptr);
        vkDestroyShaderModule(vkDevice, vkPSModule, nullptr);
    }

private:
    VkShaderModule   vkVSModule = VK_NULL_HANDLE;
    VkShaderModule   vkPSModule = VK_NULL_HANDLE;
    VkPipeline       vkPipeline = VK_NULL_HANDLE;
    VkPipelineLayout vkLayout   = VK_NULL_HANDLE;
};

} // namespace

void RenderDrawCommandReferenceVk(ISwapChain* pSwapChain)
{
    auto* pEnv = TestingEnvironmentVk::GetInstance();

    auto* pTestingSwapChainVk = ValidatedCast<TestingSwapChainVk>(pSwapChain);

    ReferenceTriangleRenderer TriRenderer{pSwapChain, pTestingSwapChainVk->GetRenderPass()};

    VkCommandBuffer vkCmdBuffer = pEnv->AllocateCommandBuffer();

    pTestingSwapChainVk->BeginRenderPass(vkCmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    TriRenderer.Draw(vkCmdBuffer);
    pTestingSwapChainVk->EndRenderPass(vkCmdBuffer);
    vkEndCommandBuffer(vkCmdBuffer);
    pEnv->SubmitCommandBuffer(vkCmdBuffer, true);
}

} // namespace Testing

} // namespace Diligent
