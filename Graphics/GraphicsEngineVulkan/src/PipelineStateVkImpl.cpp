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

#include "pch.h"
#include <array>
#include "PipelineStateVkImpl.hpp"
#include "ShaderVkImpl.hpp"
#include "VulkanTypeConversions.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"
#include "RenderPassVkImpl.hpp"
#include "ShaderResourceBindingVkImpl.hpp"
#include "EngineMemory.h"
#include "StringTools.hpp"


#if !DILIGENT_NO_HLSL
#    include "SPIRVUtils.hpp"
#endif

namespace Diligent
{

RenderPassDesc PipelineStateVkImpl::GetImplicitRenderPassDesc(
    Uint32                                                        NumRenderTargets,
    const TEXTURE_FORMAT                                          RTVFormats[],
    TEXTURE_FORMAT                                                DSVFormat,
    Uint8                                                         SampleCount,
    std::array<RenderPassAttachmentDesc, MAX_RENDER_TARGETS + 1>& Attachments,
    std::array<AttachmentReference, MAX_RENDER_TARGETS + 1>&      AttachmentReferences,
    SubpassDesc&                                                  SubpassDesc)
{
    VERIFY_EXPR(NumRenderTargets <= MAX_RENDER_TARGETS);

    RenderPassDesc RPDesc;

    RPDesc.AttachmentCount = (DSVFormat != TEX_FORMAT_UNKNOWN ? 1 : 0) + NumRenderTargets;

    uint32_t             AttachmentInd             = 0;
    AttachmentReference* pDepthAttachmentReference = nullptr;
    if (DSVFormat != TEX_FORMAT_UNKNOWN)
    {
        auto& DepthAttachment = Attachments[AttachmentInd];

        DepthAttachment.Format      = DSVFormat;
        DepthAttachment.SampleCount = SampleCount;
        DepthAttachment.LoadOp      = ATTACHMENT_LOAD_OP_LOAD; // previous contents of the image within the render area
                                                               // will be preserved. For attachments with a depth/stencil format,
                                                               // this uses the access type VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT.
        DepthAttachment.StoreOp = ATTACHMENT_STORE_OP_STORE;   // the contents generated during the render pass and within the render
                                                               // area are written to memory. For attachments with a depth/stencil format,
                                                               // this uses the access type VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT.
        DepthAttachment.StencilLoadOp  = ATTACHMENT_LOAD_OP_LOAD;
        DepthAttachment.StencilStoreOp = ATTACHMENT_STORE_OP_STORE;
        DepthAttachment.InitialState   = RESOURCE_STATE_DEPTH_WRITE;
        DepthAttachment.FinalState     = RESOURCE_STATE_DEPTH_WRITE;

        pDepthAttachmentReference                  = &AttachmentReferences[AttachmentInd];
        pDepthAttachmentReference->AttachmentIndex = AttachmentInd;
        pDepthAttachmentReference->State           = RESOURCE_STATE_DEPTH_WRITE;

        ++AttachmentInd;
    }

    AttachmentReference* pColorAttachmentsReference = NumRenderTargets > 0 ? &AttachmentReferences[AttachmentInd] : nullptr;
    for (Uint32 rt = 0; rt < NumRenderTargets; ++rt, ++AttachmentInd)
    {
        auto& ColorAttachment = Attachments[AttachmentInd];

        ColorAttachment.Format      = RTVFormats[rt];
        ColorAttachment.SampleCount = SampleCount;
        ColorAttachment.LoadOp      = ATTACHMENT_LOAD_OP_LOAD; // previous contents of the image within the render area
                                                               // will be preserved. For attachments with a depth/stencil format,
                                                               // this uses the access type VK_ACCESS_COLOR_ATTACHMENT_READ_BIT.
        ColorAttachment.StoreOp = ATTACHMENT_STORE_OP_STORE;   // the contents generated during the render pass and within the render
                                                               // area are written to memory. For attachments with a color format,
                                                               // this uses the access type VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT.
        ColorAttachment.StencilLoadOp  = ATTACHMENT_LOAD_OP_DISCARD;
        ColorAttachment.StencilStoreOp = ATTACHMENT_STORE_OP_DISCARD;
        ColorAttachment.InitialState   = RESOURCE_STATE_RENDER_TARGET;
        ColorAttachment.FinalState     = RESOURCE_STATE_RENDER_TARGET;

        auto& ColorAttachmentRef           = AttachmentReferences[AttachmentInd];
        ColorAttachmentRef.AttachmentIndex = AttachmentInd;
        ColorAttachmentRef.State           = RESOURCE_STATE_RENDER_TARGET;
    }

    RPDesc.pAttachments    = Attachments.data();
    RPDesc.SubpassCount    = 1;
    RPDesc.pSubpasses      = &SubpassDesc;
    RPDesc.DependencyCount = 0;       // the number of dependencies between pairs of subpasses, or zero indicating no dependencies.
    RPDesc.pDependencies   = nullptr; // an array of dependencyCount number of VkSubpassDependency structures describing
                                      // dependencies between pairs of subpasses, or NULL if dependencyCount is zero.


    SubpassDesc.InputAttachmentCount        = 0;
    SubpassDesc.pInputAttachments           = nullptr;
    SubpassDesc.RenderTargetAttachmentCount = NumRenderTargets;
    SubpassDesc.pRenderTargetAttachments    = pColorAttachmentsReference;
    SubpassDesc.pResolveAttachments         = nullptr;
    SubpassDesc.pDepthStencilAttachment     = pDepthAttachmentReference;
    SubpassDesc.PreserveAttachmentCount     = 0;
    SubpassDesc.pPreserveAttachments        = nullptr;

    return RPDesc;
}

#if DILIGENT_NO_HLSL
static std::vector<uint32_t> StripReflection(const std::vector<uint32_t>& OriginalSPIRV)
{
    return OriginalSPIRV;
}
#endif

PipelineStateVkImpl::PipelineStateVkImpl(IReferenceCounters*            pRefCounters,
                                         RenderDeviceVkImpl*            pDeviceVk,
                                         const PipelineStateCreateInfo& CreateInfo) :
    TPipelineStateBase{pRefCounters, pDeviceVk, CreateInfo.PSODesc},
    m_SRBMemAllocator{GetRawAllocator()}
{
    const auto& LogicalDevice = pDeviceVk->GetLogicalDevice();

    // Initialize shader resource layouts
    auto& ShaderResLayoutAllocator = GetRawAllocator();

    std::array<std::shared_ptr<const SPIRVShaderResources>, MAX_SHADERS_IN_PIPELINE> ShaderResources;
    std::array<std::vector<uint32_t>, MAX_SHADERS_IN_PIPELINE>                       ShaderSPIRVs;

    m_ShaderResourceLayouts = ALLOCATE(ShaderResLayoutAllocator, "Raw memory for ShaderResourceLayoutVk", ShaderResourceLayoutVk, m_NumShaders * 2);
    m_StaticResCaches       = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderResourceCacheVk", ShaderResourceCacheVk, m_NumShaders);
    m_StaticVarsMgrs        = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderVariableManagerVk", ShaderVariableManagerVk, m_NumShaders);
    m_ResourceLayoutIndex.fill(-1);

    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        new (m_ShaderResourceLayouts + s) ShaderResourceLayoutVk(LogicalDevice);
        auto* pShaderVk    = GetShader<const ShaderVkImpl>(s);
        ShaderResources[s] = pShaderVk->GetShaderResources();
        ShaderSPIRVs[s]    = pShaderVk->GetSPIRV();

        const auto ShaderType                = pShaderVk->GetDesc().ShaderType;
        const auto ShaderTypeInd             = GetShaderTypeIndex(ShaderType);
        m_ResourceLayoutIndex[ShaderTypeInd] = static_cast<Int8>(s);

        auto* pStaticResLayout = new (m_ShaderResourceLayouts + m_NumShaders + s) ShaderResourceLayoutVk(LogicalDevice);
        auto* pStaticResCache  = new (m_StaticResCaches + s) ShaderResourceCacheVk(ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources);
        pStaticResLayout->InitializeStaticResourceLayout(ShaderResources[s], ShaderResLayoutAllocator, m_Desc.ResourceLayout, m_StaticResCaches[s]);

        new (m_StaticVarsMgrs + s) ShaderVariableManagerVk(*this, *pStaticResLayout, GetRawAllocator(), nullptr, 0, *pStaticResCache);
    }
    ShaderResourceLayoutVk::Initialize(pDeviceVk, m_NumShaders, m_ShaderResourceLayouts, ShaderResources.data(), GetRawAllocator(),
                                       m_Desc.ResourceLayout, ShaderSPIRVs.data(), m_PipelineLayout,
                                       (CreateInfo.Flags & PSO_CREATE_FLAG_IGNORE_MISSING_VARIABLES) == 0,
                                       (CreateInfo.Flags & PSO_CREATE_FLAG_IGNORE_MISSING_STATIC_SAMPLERS) == 0);
    m_PipelineLayout.Finalize(LogicalDevice);

    if (m_Desc.SRBAllocationGranularity > 1)
    {
        std::array<size_t, MAX_SHADERS_IN_PIPELINE> ShaderVariableDataSizes = {};
        for (Uint32 s = 0; s < m_NumShaders; ++s)
        {
            const SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};

            Uint32 UnusedNumVars       = 0;
            ShaderVariableDataSizes[s] = ShaderVariableManagerVk::GetRequiredMemorySize(m_ShaderResourceLayouts[s], AllowedVarTypes, _countof(AllowedVarTypes), UnusedNumVars);
        }

        Uint32 NumSets            = 0;
        auto   DescriptorSetSizes = m_PipelineLayout.GetDescriptorSetSizes(NumSets);
        auto   CacheMemorySize    = ShaderResourceCacheVk::GetRequiredMemorySize(NumSets, DescriptorSetSizes.data());

        m_SRBMemAllocator.Initialize(m_Desc.SRBAllocationGranularity, m_NumShaders, ShaderVariableDataSizes.data(), 1, &CacheMemorySize);
    }

    // Create shader modules and initialize shader stages
    std::array<VkPipelineShaderStageCreateInfo, MAX_SHADERS_IN_PIPELINE> ShaderStages = {};
    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto* pShaderVk  = GetShader<const ShaderVkImpl>(s);
        auto  ShaderType = pShaderVk->GetDesc().ShaderType;

        auto& StageCI = ShaderStages[s];
        StageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        StageCI.pNext = nullptr;
        StageCI.flags = 0; //  reserved for future use
        switch (ShaderType)
        {
            // clang-format off
            case SHADER_TYPE_VERTEX:        StageCI.stage = VK_SHADER_STAGE_VERTEX_BIT;                  break;
            case SHADER_TYPE_HULL:          StageCI.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;    break;
            case SHADER_TYPE_DOMAIN:        StageCI.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; break;
            case SHADER_TYPE_GEOMETRY:      StageCI.stage = VK_SHADER_STAGE_GEOMETRY_BIT;                break;
            case SHADER_TYPE_PIXEL:         StageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;                break;
            case SHADER_TYPE_COMPUTE:       StageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;                 break;
#ifdef VK_NV_mesh_shader
            case SHADER_TYPE_AMPLIFICATION: StageCI.stage = VK_SHADER_STAGE_TASK_BIT_NV;                 break;
            case SHADER_TYPE_MESH:          StageCI.stage = VK_SHADER_STAGE_MESH_BIT_NV;                 break;
#endif
            default: UNEXPECTED("Unknown shader type");
                // clang-format on
        }

        VkShaderModuleCreateInfo ShaderModuleCI = {};

        ShaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ShaderModuleCI.pNext = nullptr;
        ShaderModuleCI.flags = 0;
        const auto& SPIRV    = ShaderSPIRVs[s];

        // We have to strip reflection instructions to fix the follownig validation error:
        //     SPIR-V module not valid: DecorateStringGOOGLE requires one of the following extensions: SPV_GOOGLE_decorate_string
        // Optimizer also performs validation and may catch problems with the byte code.
        auto StrippedSPIRV = StripReflection(SPIRV);
        if (!StrippedSPIRV.empty())
        {
            ShaderModuleCI.codeSize = StrippedSPIRV.size() * sizeof(uint32_t);
            ShaderModuleCI.pCode    = StrippedSPIRV.data();
        }
        else
        {
            LOG_ERROR("Failed to strip reflection information from shader '", pShaderVk->GetDesc().Name, "'. This may indicate a problem with the byte code.");
            ShaderModuleCI.codeSize = SPIRV.size() * sizeof(uint32_t);
            ShaderModuleCI.pCode    = SPIRV.data();
        }

        m_ShaderModules[s] = LogicalDevice.CreateShaderModule(ShaderModuleCI, pShaderVk->GetDesc().Name);

        StageCI.module              = m_ShaderModules[s];
        StageCI.pName               = pShaderVk->GetEntryPoint();
        StageCI.pSpecializationInfo = nullptr;
    }

    // Create pipeline
    if (m_Desc.IsComputePipeline())
    {
        auto& ComputePipeline = m_Desc.ComputePipeline;

        if (ComputePipeline.pCS == nullptr)
            LOG_ERROR_AND_THROW("Compute shader is not set in the pipeline desc");

        VkComputePipelineCreateInfo PipelineCI = {};

        PipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        PipelineCI.pNext = nullptr;
#ifdef DILIGENT_DEBUG
        PipelineCI.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
#endif
        PipelineCI.basePipelineHandle = VK_NULL_HANDLE; // a pipeline to derive from
        PipelineCI.basePipelineIndex  = 0;              // an index into the pCreateInfos parameter to use as a pipeline to derive from

        PipelineCI.stage  = ShaderStages[0];
        PipelineCI.layout = m_PipelineLayout.GetVkPipelineLayout();

        m_Pipeline = LogicalDevice.CreateComputePipeline(PipelineCI, VK_NULL_HANDLE, m_Desc.Name);
    }
    else
    {
        const auto& PhysicalDevice   = pDeviceVk->GetPhysicalDevice();
        auto&       GraphicsPipeline = m_Desc.GraphicsPipeline;
        auto&       RPCache          = pDeviceVk->GetImplicitRenderPassCache();

        if (m_pRenderPass == nullptr)
        {
            RenderPassCache::RenderPassCacheKey Key{
                GraphicsPipeline.NumRenderTargets,
                GraphicsPipeline.SmplDesc.Count,
                GraphicsPipeline.RTVFormats,
                GraphicsPipeline.DSVFormat};
            m_pRenderPass = RPCache.GetRenderPass(Key);
        }

        VkGraphicsPipelineCreateInfo PipelineCI = {};

        PipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        PipelineCI.pNext = nullptr;
#ifdef DILIGENT_DEBUG
        PipelineCI.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
#endif

        PipelineCI.stageCount = m_NumShaders;
        PipelineCI.pStages    = ShaderStages.data();
        PipelineCI.layout     = m_PipelineLayout.GetVkPipelineLayout();

        VkPipelineVertexInputStateCreateInfo VertexInputStateCI = {};

        std::array<VkVertexInputBindingDescription, MAX_LAYOUT_ELEMENTS>   BindingDescriptions;
        std::array<VkVertexInputAttributeDescription, MAX_LAYOUT_ELEMENTS> AttributeDescription;
        InputLayoutDesc_To_VkVertexInputStateCI(GraphicsPipeline.InputLayout, VertexInputStateCI, BindingDescriptions, AttributeDescription);
        PipelineCI.pVertexInputState = &VertexInputStateCI;


        VkPipelineInputAssemblyStateCreateInfo InputAssemblyCI = {};

        InputAssemblyCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        InputAssemblyCI.pNext                  = nullptr;
        InputAssemblyCI.flags                  = 0; // reserved for future use
        InputAssemblyCI.primitiveRestartEnable = VK_FALSE;
        PipelineCI.pInputAssemblyState         = &InputAssemblyCI;


        VkPipelineTessellationStateCreateInfo TessStateCI = {};

        TessStateCI.sType             = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        TessStateCI.pNext             = nullptr;
        TessStateCI.flags             = 0; // reserved for future use
        PipelineCI.pTessellationState = &TessStateCI;

        if (m_Desc.PipelineType == PIPELINE_TYPE_MESH)
        {
            // Input assembly doesn't used during mesh pipeline creation, so topology may contain any value.
            // Validation layers may generate warning if used point_list topology, so set another value.
            InputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;

            // Vertex input state and tessellation state are ignored in mesh pipeline and shuld be null.
            PipelineCI.pVertexInputState  = nullptr;
            PipelineCI.pTessellationState = nullptr;
        }
        else
        {
            PrimitiveTopology_To_VkPrimitiveTopologyAndPatchCPCount(GraphicsPipeline.PrimitiveTopology, InputAssemblyCI.topology, TessStateCI.patchControlPoints);
        }

        VkPipelineViewportStateCreateInfo ViewPortStateCI = {};

        ViewPortStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        ViewPortStateCI.pNext = nullptr;
        ViewPortStateCI.flags = 0; // reserved for future use
        ViewPortStateCI.viewportCount =
            GraphicsPipeline.NumViewports;                            // Even though we use dynamic viewports, the number of viewports used
                                                                      // by the pipeline is still specified by the viewportCount member (23.5)
        ViewPortStateCI.pViewports   = nullptr;                       // We will be using dynamic viewport & scissor states
        ViewPortStateCI.scissorCount = ViewPortStateCI.viewportCount; // the number of scissors must match the number of viewports (23.5)
                                                                      // (why the hell it is in the struct then?)
        VkRect2D ScissorRect = {};
        if (GraphicsPipeline.RasterizerDesc.ScissorEnable)
        {
            ViewPortStateCI.pScissors = nullptr; // Ignored if the scissor state is dynamic
        }
        else
        {
            const auto& Props = PhysicalDevice.GetProperties();
            // There are limitiations on the viewport width and height (23.5), but
            // it is not clear if there are limitations on the scissor rect width and
            // height
            ScissorRect.extent.width  = Props.limits.maxViewportDimensions[0];
            ScissorRect.extent.height = Props.limits.maxViewportDimensions[1];
            ViewPortStateCI.pScissors = &ScissorRect;
        }
        PipelineCI.pViewportState = &ViewPortStateCI;

        VkPipelineRasterizationStateCreateInfo RasterizerStateCI =
            RasterizerStateDesc_To_VkRasterizationStateCI(GraphicsPipeline.RasterizerDesc);
        PipelineCI.pRasterizationState = &RasterizerStateCI;

        // Multisample state (24)
        VkPipelineMultisampleStateCreateInfo MSStateCI = {};

        MSStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        MSStateCI.pNext = nullptr;
        MSStateCI.flags = 0; // reserved for future use
        // If subpass uses color and/or depth/stencil attachments, then the rasterizationSamples member of
        // pMultisampleState must be the same as the sample count for those subpass attachments
        MSStateCI.rasterizationSamples = static_cast<VkSampleCountFlagBits>(GraphicsPipeline.SmplDesc.Count);
        MSStateCI.sampleShadingEnable  = VK_FALSE;
        MSStateCI.minSampleShading     = 0;                                // a minimum fraction of sample shading if sampleShadingEnable is set to VK_TRUE.
        uint32_t SampleMask[]          = {GraphicsPipeline.SampleMask, 0}; // Vulkan spec allows up to 64 samples
        MSStateCI.pSampleMask          = SampleMask;                       // an array of static coverage information that is ANDed with
                                                                           // the coverage information generated during rasterization (25.3)
        MSStateCI.alphaToCoverageEnable = VK_FALSE;                        // whether a temporary coverage value is generated based on
                                                                           // the alpha component of the fragment's first color output
        MSStateCI.alphaToOneEnable   = VK_FALSE;                           // whether the alpha component of the fragment's first color output is replaced with one
        PipelineCI.pMultisampleState = &MSStateCI;

        VkPipelineDepthStencilStateCreateInfo DepthStencilStateCI =
            DepthStencilStateDesc_To_VkDepthStencilStateCI(GraphicsPipeline.DepthStencilDesc);
        PipelineCI.pDepthStencilState = &DepthStencilStateCI;

        const auto& RPDesc           = m_pRenderPass->GetDesc();
        const auto  NumRTAttachments = RPDesc.pSubpasses[GraphicsPipeline.SubpassIndex].RenderTargetAttachmentCount;
        VERIFY_EXPR(GraphicsPipeline.pRenderPass != nullptr || GraphicsPipeline.NumRenderTargets == NumRTAttachments);
        std::vector<VkPipelineColorBlendAttachmentState> ColorBlendAttachmentStates(NumRTAttachments);

        VkPipelineColorBlendStateCreateInfo BlendStateCI = {};

        BlendStateCI.pAttachments    = !ColorBlendAttachmentStates.empty() ? ColorBlendAttachmentStates.data() : nullptr;
        BlendStateCI.attachmentCount = NumRTAttachments; // must equal the colorAttachmentCount for the subpass
                                                         // in which this pipeline is used.
        BlendStateDesc_To_VkBlendStateCI(GraphicsPipeline.BlendDesc, BlendStateCI, ColorBlendAttachmentStates);
        PipelineCI.pColorBlendState = &BlendStateCI;


        VkPipelineDynamicStateCreateInfo DynamicStateCI = {};

        DynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        DynamicStateCI.pNext = nullptr;
        DynamicStateCI.flags = 0; // reserved for future use
        std::vector<VkDynamicState> DynamicStates =
            {
                VK_DYNAMIC_STATE_VIEWPORT, // pViewports state in VkPipelineViewportStateCreateInfo will be ignored and must be
                                           // set dynamically with vkCmdSetViewport before any draw commands. The number of viewports
                                           // used by a pipeline is still specified by the viewportCount member of
                                           // VkPipelineViewportStateCreateInfo.

                VK_DYNAMIC_STATE_BLEND_CONSTANTS, // blendConstants state in VkPipelineColorBlendStateCreateInfo will be ignored
                                                  // and must be set dynamically with vkCmdSetBlendConstants

                VK_DYNAMIC_STATE_STENCIL_REFERENCE // pecifies that the reference state in VkPipelineDepthStencilStateCreateInfo
                                                   // for both front and back will be ignored and must be set dynamically
                                                   // with vkCmdSetStencilReference
            };

        if (GraphicsPipeline.RasterizerDesc.ScissorEnable)
        {
            // pScissors state in VkPipelineViewportStateCreateInfo will be ignored and must be set
            // dynamically with vkCmdSetScissor before any draw commands. The number of scissor rectangles
            // used by a pipeline is still specified by the scissorCount member of
            // VkPipelineViewportStateCreateInfo.
            DynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
        }
        DynamicStateCI.dynamicStateCount = static_cast<uint32_t>(DynamicStates.size());
        DynamicStateCI.pDynamicStates    = DynamicStates.data();
        PipelineCI.pDynamicState         = &DynamicStateCI;


        PipelineCI.renderPass         = GetRenderPass()->GetVkRenderPass();
        PipelineCI.subpass            = m_Desc.GraphicsPipeline.SubpassIndex;
        PipelineCI.basePipelineHandle = VK_NULL_HANDLE; // a pipeline to derive from
        PipelineCI.basePipelineIndex  = 0;              // an index into the pCreateInfos parameter to use as a pipeline to derive from

        m_Pipeline = LogicalDevice.CreateGraphicsPipeline(PipelineCI, VK_NULL_HANDLE, m_Desc.Name);
    }

    m_HasStaticResources    = false;
    m_HasNonStaticResources = false;
    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        const auto& Layout = m_ShaderResourceLayouts[s];
        if (Layout.GetResourceCount(SHADER_RESOURCE_VARIABLE_TYPE_STATIC) != 0)
            m_HasStaticResources = true;

        if (Layout.GetResourceCount(SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE) != 0 ||
            Layout.GetResourceCount(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC) != 0)
            m_HasNonStaticResources = true;
    }

    m_ShaderResourceLayoutHash = m_PipelineLayout.GetHash();
}

PipelineStateVkImpl::~PipelineStateVkImpl()
{
    m_pDevice->SafeReleaseDeviceObject(std::move(m_Pipeline), m_Desc.CommandQueueMask);
    m_PipelineLayout.Release(m_pDevice, m_Desc.CommandQueueMask);

    for (auto& ShaderModule : m_ShaderModules)
    {
        if (ShaderModule != VK_NULL_HANDLE)
        {
            m_pDevice->SafeReleaseDeviceObject(std::move(ShaderModule), m_Desc.CommandQueueMask);
        }
    }

    auto& RawAllocator = GetRawAllocator();
    for (Uint32 s = 0; s < m_NumShaders * 2; ++s)
    {
        m_ShaderResourceLayouts[s].~ShaderResourceLayoutVk();
    }

    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        m_StaticResCaches[s].~ShaderResourceCacheVk();
        m_StaticVarsMgrs[s].DestroyVariables(GetRawAllocator());
        m_StaticVarsMgrs[s].~ShaderVariableManagerVk();
    }
    RawAllocator.Free(m_ShaderResourceLayouts);
    RawAllocator.Free(m_StaticResCaches);
    RawAllocator.Free(m_StaticVarsMgrs);
}

IMPLEMENT_QUERY_INTERFACE(PipelineStateVkImpl, IID_PipelineStateVk, TPipelineStateBase)


void PipelineStateVkImpl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources)
{
    auto& SRBAllocator  = m_pDevice->GetSRBAllocator();
    auto  pResBindingVk = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingVkImpl instance", ShaderResourceBindingVkImpl)(this, false);
    if (InitStaticResources)
        pResBindingVk->InitializeStaticResources(nullptr);
    pResBindingVk->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
}

bool PipelineStateVkImpl::IsCompatibleWith(const IPipelineState* pPSO) const
{
    VERIFY_EXPR(pPSO != nullptr);

    if (pPSO == this)
        return true;

    const PipelineStateVkImpl* pPSOVk = ValidatedCast<const PipelineStateVkImpl>(pPSO);
    if (m_ShaderResourceLayoutHash != pPSOVk->m_ShaderResourceLayoutHash)
        return false;

    auto IsSamePipelineLayout = m_PipelineLayout.IsSameAs(pPSOVk->m_PipelineLayout);

#ifdef DILIGENT_DEBUG
    {
        bool IsCompatibleShaders = true;
        if (m_NumShaders != pPSOVk->m_NumShaders)
            IsCompatibleShaders = false;

        if (IsCompatibleShaders)
        {
            for (Uint32 s = 0; s < m_NumShaders; ++s)
            {
                auto* pShader0 = GetShader<const ShaderVkImpl>(s);
                auto* pShader1 = pPSOVk->GetShader<const ShaderVkImpl>(s);
                if (pShader0->GetDesc().ShaderType != pShader1->GetDesc().ShaderType)
                {
                    IsCompatibleShaders = false;
                    break;
                }
                const auto* pRes0 = pShader0->GetShaderResources().get();
                const auto* pRes1 = pShader1->GetShaderResources().get();
                if (!pRes0->IsCompatibleWith(*pRes1))
                {
                    IsCompatibleShaders = false;
                    break;
                }
            }
        }

        if (IsCompatibleShaders)
            VERIFY(IsSamePipelineLayout, "Compatible shaders must have same pipeline layouts");
    }
#endif

    return IsSamePipelineLayout;
}


void PipelineStateVkImpl::CommitAndTransitionShaderResources(IShaderResourceBinding*                pShaderResourceBinding,
                                                             DeviceContextVkImpl*                   pCtxVkImpl,
                                                             bool                                   CommitResources,
                                                             RESOURCE_STATE_TRANSITION_MODE         StateTransitionMode,
                                                             PipelineLayout::DescriptorSetBindInfo* pDescrSetBindInfo) const
{
    VERIFY(CommitResources || StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION, "Resources should be transitioned or committed or both");

    if (!m_HasStaticResources && !m_HasNonStaticResources)
        return;

#ifdef DILIGENT_DEVELOPMENT
    if (pShaderResourceBinding == nullptr)
    {
        LOG_ERROR_MESSAGE("Pipeline state '", m_Desc.Name, "' requires shader resource binding object to ",
                          (CommitResources ? "commit" : "transition"), " resources, but none is provided.");
        return;
    }
#endif

    auto* pResBindingVkImpl = ValidatedCast<ShaderResourceBindingVkImpl>(pShaderResourceBinding);

#ifdef DILIGENT_DEVELOPMENT
    {
        auto* pRefPSO = pResBindingVkImpl->GetPipelineState();
        if (IsIncompatibleWith(pRefPSO))
        {
            LOG_ERROR_MESSAGE("Shader resource binding is incompatible with the pipeline state '", m_Desc.Name, "'. Operation will be ignored.");
            return;
        }
    }

    if (m_HasStaticResources && !pResBindingVkImpl->StaticResourcesInitialized())
    {
        LOG_ERROR_MESSAGE("Static resources have not been initialized in the shader resource binding object being committed for PSO '", m_Desc.Name, "'. Please call IShaderResourceBinding::InitializeStaticResources().");
    }
#endif

    auto& ResourceCache = pResBindingVkImpl->GetResourceCache();

#ifdef DILIGENT_DEVELOPMENT
    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        m_ShaderResourceLayouts[s].dvpVerifyBindings(ResourceCache);
    }
#endif
#ifdef DILIGENT_DEBUG
    ResourceCache.DbgVerifyDynamicBuffersCounter();
#endif

    if (StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
    {
        ResourceCache.TransitionResources<false>(pCtxVkImpl);
    }
#ifdef DILIGENT_DEVELOPMENT
    else if (StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_VERIFY)
    {
        ResourceCache.TransitionResources<true>(pCtxVkImpl);
    }
#endif

    if (CommitResources)
    {
        VkDescriptorSet DynamicDescrSet              = VK_NULL_HANDLE;
        auto            DynamicDescriptorSetVkLayout = m_PipelineLayout.GetDynamicDescriptorSetVkLayout();
        if (DynamicDescriptorSetVkLayout != VK_NULL_HANDLE)
        {
            const char* DynamicDescrSetName = "Dynamic Descriptor Set";
#ifdef DILIGENT_DEVELOPMENT
            std::string _DynamicDescrSetName(m_Desc.Name);
            _DynamicDescrSetName.append(" - dynamic set");
            DynamicDescrSetName = _DynamicDescrSetName.c_str();
#endif
            // Allocate vulkan descriptor set for dynamic resources
            DynamicDescrSet = pCtxVkImpl->AllocateDynamicDescriptorSet(DynamicDescriptorSetVkLayout, DynamicDescrSetName);
            // Commit all dynamic resource descriptors
            for (Uint32 s = 0; s < m_NumShaders; ++s)
            {
                const auto& Layout = m_ShaderResourceLayouts[s];
                if (Layout.GetResourceCount(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC) != 0)
                    Layout.CommitDynamicResources(ResourceCache, DynamicDescrSet);
            }
        }
        // Prepare descriptor sets, and also bind them if there are no dynamic descriptors
        VERIFY_EXPR(pDescrSetBindInfo != nullptr);
        m_PipelineLayout.PrepareDescriptorSets(pCtxVkImpl, m_Desc.IsComputePipeline(), ResourceCache, *pDescrSetBindInfo, DynamicDescrSet);
        // Dynamic descriptor sets are not released individually. Instead, all dynamic descriptor pools
        // are released at the end of the frame by DeviceContextVkImpl::FinishFrame().
    }
}

void PipelineStateVkImpl::BindStaticResources(Uint32 ShaderFlags, IResourceMapping* pResourceMapping, Uint32 Flags)
{
    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto ShaderType = GetStaticShaderResLayout(s).GetShaderType();
        if ((ShaderType & ShaderFlags) != 0)
        {
            auto& StaticVarMgr = GetStaticVarMgr(s);
            StaticVarMgr.BindResources(pResourceMapping, Flags);
        }
    }
}

Uint32 PipelineStateVkImpl::GetStaticVariableCount(SHADER_TYPE ShaderType) const
{
    const auto LayoutInd = m_ResourceLayoutIndex[GetShaderTypeIndex(ShaderType)];
    if (LayoutInd < 0)
        return 0;

    auto& StaticVarMgr = GetStaticVarMgr(LayoutInd);
    return StaticVarMgr.GetVariableCount();
}

IShaderResourceVariable* PipelineStateVkImpl::GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name)
{
    const auto LayoutInd = m_ResourceLayoutIndex[GetShaderTypeIndex(ShaderType)];
    if (LayoutInd < 0)
        return nullptr;

    auto& StaticVarMgr = GetStaticVarMgr(LayoutInd);
    return StaticVarMgr.GetVariable(Name);
}

IShaderResourceVariable* PipelineStateVkImpl::GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    const auto LayoutInd = m_ResourceLayoutIndex[GetShaderTypeIndex(ShaderType)];
    if (LayoutInd < 0)
        return nullptr;

    auto& StaticVarMgr = GetStaticVarMgr(LayoutInd);
    return StaticVarMgr.GetVariable(Index);
}


void PipelineStateVkImpl::InitializeStaticSRBResources(ShaderResourceCacheVk& ResourceCache) const
{
    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        const auto& StaticResLayout = GetStaticShaderResLayout(s);
        const auto& StaticResCache  = GetStaticResCache(s);
#ifdef DILIGENT_DEVELOPMENT
        if (!StaticResLayout.dvpVerifyBindings(StaticResCache))
        {
            const auto* pShaderVk = GetShader<const ShaderVkImpl>(s);
            LOG_ERROR_MESSAGE("Static resources in SRB of PSO '", GetDesc().Name,
                              "' will not be successfully initialized because not all static resource bindings in shader '",
                              pShaderVk->GetDesc().Name,
                              "' are valid. Please make sure you bind all static resources to PSO before calling InitializeStaticResources() "
                              "directly or indirectly by passing InitStaticResources=true to CreateShaderResourceBinding() method.");
        }
#endif
        const auto& ShaderResourceLayouts = GetShaderResLayout(s);
        ShaderResourceLayouts.InitializeStaticResources(StaticResLayout, StaticResCache, ResourceCache);
    }
#ifdef DILIGENT_DEBUG
    ResourceCache.DbgVerifyDynamicBuffersCounter();
#endif
}

} // namespace Diligent
