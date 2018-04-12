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

#include "pch.h"
#include "PipelineStateVkImpl.h"
#include "ShaderVkImpl.h"
#include "VulkanTypeConversions.h"
#include "RenderDeviceVkImpl.h"
#include "ShaderResourceBindingVkImpl.h"
#include "CommandContext.h"
#include "EngineMemory.h"
#include "StringTools.h"

namespace Diligent
{
/*
Vk_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType_To_Vk_PRIMITIVE_TOPOLOGY_TYPE( PRIMITIVE_TOPOLOGY_TYPE TopologyType )
{
    static bool bIsInit = false;
    static Vk_PRIMITIVE_TOPOLOGY_TYPE VkTopologyType[PRIMITIVE_TOPOLOGY_TYPE_NUM_TYPES] = {};
    if( !bIsInit )
    {
        VkTopologyType[ PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED] =  Vk_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        VkTopologyType[ PRIMITIVE_TOPOLOGY_TYPE_POINT    ] =  Vk_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        VkTopologyType[ PRIMITIVE_TOPOLOGY_TYPE_LINE     ] =  Vk_PRIMITIVE_TOPOLOGY_TYPE_LINE;  
        VkTopologyType[ PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE ] =  Vk_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        VkTopologyType[ PRIMITIVE_TOPOLOGY_TYPE_PATCH    ] =  Vk_PRIMITIVE_TOPOLOGY_TYPE_PATCH;

        bIsInit = true;
    }

    if( TopologyType >= PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED && TopologyType < PRIMITIVE_TOPOLOGY_TYPE_NUM_TYPES )
    {
        auto VkTopType = VkTopologyType[TopologyType];
        return VkTopType;
    }
    else
    {
        UNEXPECTED( "Incorrect topology type operation (", TopologyType, ")" );
        return static_cast<Vk_PRIMITIVE_TOPOLOGY_TYPE>(0);
    }
}

void PipelineStateVkImpl::ParseShaderResourceLayout(IShader *pShader)
{
    VERIFY_EXPR(pShader);

    auto ShaderType = pShader->GetDesc().ShaderType;
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto *pShaderVk = ValidatedCast<ShaderVkImpl>(pShader);
    
    VERIFY(m_pShaderResourceLayouts[ShaderInd] == nullptr, "Shader resource layout has already been initialized");

    auto pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(pShaderVk->GetDevice());
    auto &ShaderResLayoutAllocator = GetRawAllocator();

    auto *pRawMem = ALLOCATE(ShaderResLayoutAllocator, "Raw memory for ShaderResourceLayoutVk", sizeof(ShaderResourceLayoutVk));
    m_pShaderResourceLayouts[ShaderInd] = new (pRawMem) ShaderResourceLayoutVk(*this, GetRawAllocator());
    m_pShaderResourceLayouts[ShaderInd]->Initialize(pDeviceVkImpl->GetVkDevice(), pShaderVk->GetShaderResources(), GetRawAllocator(), nullptr, 0, nullptr, &m_RootSig);
}
*/

void PipelineStateVkImpl::CreateRenderPass(const VulkanUtilities::VulkanLogicalDevice &LogicalDevice)
{
    const auto& GraphicsPipeline = m_Desc.GraphicsPipeline;
    // Create render pass (7.1)
    VkRenderPassCreateInfo RenderPassCI = {};
    RenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassCI.pNext = nullptr;
    RenderPassCI.flags = 0; // reserved for future use
    RenderPassCI.attachmentCount = (GraphicsPipeline.DSVFormat != TEX_FORMAT_UNKNOWN ? 1 : 0) + GraphicsPipeline.NumRenderTargets;
    std::vector<VkAttachmentDescription> Attachments(RenderPassCI.attachmentCount);
    uint32_t AttachmentInd = 0;
    VkSampleCountFlagBits SampleCountFlags = static_cast<VkSampleCountFlagBits>(1 << (GraphicsPipeline.SmplDesc.Count - 1));
    VkAttachmentReference DepthAttachmentReference = {};
    if (GraphicsPipeline.DSVFormat != TEX_FORMAT_UNKNOWN)
    {
        auto& DepthAttachment = Attachments[AttachmentInd];
        DepthAttachmentReference.attachment = AttachmentInd;
        DepthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        DepthAttachment.flags = 0; // Allowed value VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT
        DepthAttachment.format = TexFormatToVkFormat(GraphicsPipeline.DSVFormat);
        DepthAttachment.samples = SampleCountFlags;
        DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // previous contents of the image within the render area 
                                                             // will be preserved. For attachments with a depth/stencil format, 
                                                             // this uses the access type VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT.
        DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // the contents generated during the render pass and within the render 
                                                                // area are written to memory. For attachments with a depth/stencil format,
                                                                // this uses the access type VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT. 
        DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        ++AttachmentInd;
    }
    std::vector<VkAttachmentReference> ColorAttachmentReferences(GraphicsPipeline.NumRenderTargets);
    for (Uint32 rt = 0; rt < GraphicsPipeline.NumRenderTargets; ++rt, ++AttachmentInd)
    {
        auto& ColorAttachment = Attachments[AttachmentInd];
        auto& ColorAttachmentRef = ColorAttachmentReferences[rt];
        ColorAttachmentRef.attachment = AttachmentInd;
        ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        ColorAttachment.flags = 0; // Allowed value VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT
        ColorAttachment.format = TexFormatToVkFormat(GraphicsPipeline.RTVFormats[rt]);
        ColorAttachment.samples = SampleCountFlags;
        ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // previous contents of the image within the render area 
                                                             // will be preserved. For attachments with a depth/stencil format, 
                                                             // this uses the access type VK_ACCESS_COLOR_ATTACHMENT_READ_BIT.
        ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // the contents generated during the render pass and within the render
                                                                // area are written to memory. For attachments with a color format,
                                                                // this uses the access type VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT. 
        ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    RenderPassCI.pAttachments = Attachments.data();

    VkSubpassDescription SubpassDesc = {};
    RenderPassCI.subpassCount = 1;
    RenderPassCI.pSubpasses = &SubpassDesc;
    RenderPassCI.dependencyCount = 0; // the number of dependencies between pairs of subpasses, or zero indicating no dependencies.
    RenderPassCI.pDependencies = nullptr; // an array of dependencyCount number of VkSubpassDependency structures describing 
                                         // dependencies between pairs of subpasses, or NULL if dependencyCount is zero.


    SubpassDesc.flags = 0; // All bits for this type are defined by extensions
    SubpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Currently, only graphics subpasses are supported.
    SubpassDesc.inputAttachmentCount = 0;
    SubpassDesc.pInputAttachments = nullptr;
    SubpassDesc.colorAttachmentCount = GraphicsPipeline.NumRenderTargets;
    SubpassDesc.pColorAttachments = ColorAttachmentReferences.data();
    SubpassDesc.pResolveAttachments = nullptr;
    SubpassDesc.pDepthStencilAttachment = GraphicsPipeline.DSVFormat != TEX_FORMAT_UNKNOWN ? &DepthAttachmentReference : nullptr;
    SubpassDesc.preserveAttachmentCount = 0;
    SubpassDesc.pPreserveAttachments = nullptr;

    std::string RenderPassName = "Render pass for '";
    RenderPassName += m_Desc.Name;
    RenderPassName += '\'';
    m_RenderPass = LogicalDevice.CreateRenderPass(RenderPassCI, RenderPassName.c_str());
}

PipelineStateVkImpl :: PipelineStateVkImpl(IReferenceCounters *pRefCounters, RenderDeviceVkImpl *pDeviceVk, const PipelineStateDesc &PipelineDesc) : 
    TPipelineStateBase(pRefCounters, pDeviceVk, PipelineDesc)/*,
    m_DummyVar(*this),
    m_ResourceCacheDataAllocator(GetRawAllocator(), PipelineDesc.SRBAllocationGranularity),
    m_pDefaultShaderResBinding(nullptr, STDDeleter<ShaderResourceBindingVkImpl, FixedBlockMemoryAllocator>(pDeviceVk->GetSRBAllocator()) )
*/
{
    const auto &LogicalDevice = pDeviceVk->GetLogicalDevice();

    if (PipelineDesc.IsComputePipeline)
    {
#if 0
        auto &ComputePipeline = PipelineDesc.ComputePipeline;

        if( ComputePipeline.pCS == nullptr )
            LOG_ERROR_AND_THROW("Compute shader is not set in the pipeline desc");

        Vk_COMPUTE_PIPELINE_STATE_DESC VkPSODesc = {};
        VkPSODesc.pRootSignature = nullptr;
        
        auto *pByteCode = ValidatedCast<ShaderVkImpl>(ComputePipeline.pCS)->GetShaderByteCode();
        VkPSODesc.CS.pShaderBytecode = pByteCode->GetBufferPointer();
        VkPSODesc.CS.BytecodeLength = pByteCode->GetBufferSize();

        // For single GPU operation, set this to zero. If there are multiple GPU nodes, 
        // set bits to identify the nodes (the device's physical adapters) for which the 
        // graphics pipeline state is to apply. Each bit in the mask corresponds to a single node. 
        VkPSODesc.NodeMask = 0;

        VkPSODesc.CachedPSO.pCachedBlob = nullptr;
        VkPSODesc.CachedPSO.CachedBlobSizeInBytes = 0;
        
        // The only valid bit is Vk_PIPELINE_STATE_FLAG_TOOL_DEBUG, which can only be set on WARP devices.
        VkPSODesc.Flags = Vk_PIPELINE_STATE_FLAG_NONE;

        ParseShaderResourceLayout(ComputePipeline.pCS);
        m_RootSig.Finalize(pVkDevice);
        VkPSODesc.pRootSignature = m_RootSig.GetVkRootSignature();

        HRESULT hr = pVkDevice->CreateComputePipelineState(&VkPSODesc, __uuidof(IVkPipelineState), reinterpret_cast<void**>( static_cast<IVkPipelineState**>(&m_pVkPSO)) );
        if(FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create pipeline state");
#endif
    }
    else
    {
        auto &GraphicsPipeline = PipelineDesc.GraphicsPipeline;

        CreateRenderPass(LogicalDevice);
        
        VkGraphicsPipelineCreateInfo PipelineCI = {};
        PipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        PipelineCI.pNext = nullptr;
        PipelineCI.flags;
        PipelineCI.stageCount;
        PipelineCI.pStages;
        PipelineCI.pVertexInputState;
        PipelineCI.pInputAssemblyState;
        PipelineCI.pTessellationState;
        PipelineCI.pViewportState;

        VkPipelineRasterizationStateCreateInfo RasterizerStateCI = 
            RasterizerStateDesc_To_VkRasterizationStateCI(GraphicsPipeline.RasterizerDesc);
        PipelineCI.pRasterizationState = &RasterizerStateCI;
        PipelineCI.pMultisampleState;

        VkPipelineDepthStencilStateCreateInfo DepthStencilStateCI = 
            DepthStencilStateDesc_To_VkDepthStencilStateCI(GraphicsPipeline.DepthStencilDesc);
        PipelineCI.pDepthStencilState = &DepthStencilStateCI;

        std::vector<VkPipelineColorBlendAttachmentState> ColorBlendAttachmentStates(m_Desc.GraphicsPipeline.NumRenderTargets);
        VkPipelineColorBlendStateCreateInfo BlendStateCI = {};
        BlendStateCI.pAttachments = !ColorBlendAttachmentStates.empty() ? ColorBlendAttachmentStates.data() : nullptr;
        BlendStateCI.attachmentCount = m_Desc.GraphicsPipeline.NumRenderTargets; //  must equal the colorAttachmentCount for the subpass in which this pipeline is used.
        BlendStateDesc_To_VkBlendStateCI(GraphicsPipeline.BlendDesc, BlendStateCI, ColorBlendAttachmentStates);
        PipelineCI.pColorBlendState = &BlendStateCI;

        PipelineCI.pDynamicState;
        PipelineCI.layout;
        PipelineCI.renderPass = m_RenderPass;
        PipelineCI.subpass = 0;
        PipelineCI.basePipelineHandle;
        PipelineCI.basePipelineIndex;


#if 0
        Vk_GRAPHICS_PIPELINE_STATE_DESC VkPSODesc = {};

        m_RootSig.AllocateStaticSamplers( GetShaders(), GetNumShaders() );
            
#define INIT_SHADER(VarName, ExpectedType)\
        if (GraphicsPipeline.p##VarName)                                            \
        {                                                                           \
            auto ShaderType = GraphicsPipeline.p##VarName->GetDesc().ShaderType;    \
            if( ShaderType != ExpectedType )                                        \
                LOG_ERROR_AND_THROW( GetShaderTypeLiteralName(ShaderType), " shader is provided while ", GetShaderTypeLiteralName(ExpectedType), " is expected");\
            auto *pByteCode = ValidatedCast<ShaderVkImpl>(GraphicsPipeline.p##VarName)->GetShaderByteCode(); \
            VkPSODesc.VarName.pShaderBytecode = pByteCode->GetBufferPointer();   \
            VkPSODesc.VarName.BytecodeLength = pByteCode->GetBufferSize();       \
            ParseShaderResourceLayout(GraphicsPipeline.p##VarName);                 \
        }                                                                           \
        else                                                                        \
        {                                                                           \
            VkPSODesc.VarName.pShaderBytecode = nullptr;                         \
            VkPSODesc.VarName.BytecodeLength = 0;                                \
        }

        INIT_SHADER(VS, SHADER_TYPE_VERTEX);
        INIT_SHADER(PS, SHADER_TYPE_PIXEL);
        INIT_SHADER(GS, SHADER_TYPE_GEOMETRY);
        INIT_SHADER(DS, SHADER_TYPE_DOMAIN);
        INIT_SHADER(HS, SHADER_TYPE_HULL);
#undef INIT_SHADER

        m_RootSig.Finalize(pVkDevice);
        VkPSODesc.pRootSignature = m_RootSig.GetVkRootSignature();
        
        memset(&VkPSODesc.StreamOutput, 0, sizeof(VkPSODesc.StreamOutput));

        BlendStateDesc_To_Vk_BLEND_DESC(GraphicsPipeline.BlendDesc, VkPSODesc.BlendState);
        // The sample mask for the blend state.
        VkPSODesc.SampleMask = GraphicsPipeline.SampleMask;
    
        RasterizerStateDesc_To_Vk_RASTERIZER_DESC(GraphicsPipeline.RasterizerDesc, VkPSODesc.RasterizerState);
        DepthStencilStateDesc_To_Vk_DEPTH_STENCIL_DESC(GraphicsPipeline.DepthStencilDesc, VkPSODesc.DepthStencilState);

        std::vector<Vk_INPUT_ELEMENT_DESC, STDAllocatorRawMem<Vk_INPUT_ELEMENT_DESC>> d312InputElements( STD_ALLOCATOR_RAW_MEM(Vk_INPUT_ELEMENT_DESC, GetRawAllocator(), "Allocator for vector<Vk_INPUT_ELEMENT_DESC>") );
        if (m_LayoutElements.size() > 0)
        {
            LayoutElements_To_Vk_INPUT_ELEMENT_DESCs(m_LayoutElements, d312InputElements);
            VkPSODesc.InputLayout.NumElements = static_cast<UINT>(d312InputElements.size());
            VkPSODesc.InputLayout.pInputElementDescs = d312InputElements.data();
        }
        else
        {
            VkPSODesc.InputLayout.NumElements = 0;
            VkPSODesc.InputLayout.pInputElementDescs = nullptr;
        }

        VkPSODesc.IBStripCutValue = Vk_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        VkPSODesc.PrimitiveTopologyType = PrimitiveTopologyType_To_Vk_PRIMITIVE_TOPOLOGY_TYPE(GraphicsPipeline.PrimitiveTopologyType);

        VkPSODesc.NumRenderTargets = GraphicsPipeline.NumRenderTargets;
        for (Uint32 rt = 0; rt < GraphicsPipeline.NumRenderTargets; ++rt)
            VkPSODesc.RTVFormats[rt] = TexFormatToDXGI_Format(GraphicsPipeline.RTVFormats[rt]);
        for (Uint32 rt = GraphicsPipeline.NumRenderTargets; rt < 8; ++rt)
            VkPSODesc.RTVFormats[rt] = TexFormatToDXGI_Format(GraphicsPipeline.RTVFormats[rt]);
        VkPSODesc.DSVFormat = TexFormatToDXGI_Format(GraphicsPipeline.DSVFormat);

        VkPSODesc.SampleDesc.Count = GraphicsPipeline.SmplDesc.Count;
        VkPSODesc.SampleDesc.Quality = GraphicsPipeline.SmplDesc.Quality;

        // For single GPU operation, set this to zero. If there are multiple GPU nodes, 
        // set bits to identify the nodes (the device's physical adapters) for which the 
        // graphics pipeline state is to apply. Each bit in the mask corresponds to a single node. 
        VkPSODesc.NodeMask = 0;

        VkPSODesc.CachedPSO.pCachedBlob = nullptr;
        VkPSODesc.CachedPSO.CachedBlobSizeInBytes = 0;

        // The only valid bit is Vk_PIPELINE_STATE_FLAG_TOOL_DEBUG, which can only be set on WARP devices.
        VkPSODesc.Flags = Vk_PIPELINE_STATE_FLAG_NONE;

        HRESULT hr = pVkDevice->CreateGraphicsPipelineState(&VkPSODesc, __uuidof(IVkPipelineState), reinterpret_cast<void**>( static_cast<IVkPipelineState**>(&m_pVkPSO)) );
        if(FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create pipeline state");
#endif
    }
#if 0
    if (*m_Desc.Name != 0)
    {
        m_pVkPSO->SetName(WidenString(m_Desc.Name).c_str());
        String RootSignatureDesc("Root signature for PSO \"");
        RootSignatureDesc.append(m_Desc.Name);
        RootSignatureDesc.push_back('\"');
        m_RootSig.GetVkRootSignature()->SetName(WidenString(RootSignatureDesc).c_str());
    }

    if(PipelineDesc.SRBAllocationGranularity > 1)
        m_ResLayoutDataAllocators.Init(m_NumShaders, PipelineDesc.SRBAllocationGranularity);

    auto &SRBAllocator = pDeviceVk->GetSRBAllocator();
    // Default shader resource binding must be initialized after resource layouts are parsed!
    m_pDefaultShaderResBinding.reset( NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingVkImpl instance", ShaderResourceBindingVkImpl, this)(this, true) );

    m_ShaderResourceLayoutHash = m_RootSig.GetHash();
#endif
}

PipelineStateVkImpl::~PipelineStateVkImpl()
{
    auto pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(GetDevice());
    pDeviceVkImpl->SafeReleaseVkObject(std::move(m_RenderPass));

#if 0
    auto &ShaderResLayoutAllocator = GetRawAllocator();
    for(Int32 l = 0; l < _countof(m_pShaderResourceLayouts); ++l)
    {
        if (m_pShaderResourceLayouts[l] != nullptr)
        {
            m_pShaderResourceLayouts[l]->~ShaderResourceLayoutVk();
            ShaderResLayoutAllocator.Free(m_pShaderResourceLayouts[l]);
        }
    }

    // Vk object can only be destroyed when it is no longer used by the GPU
    auto *pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(GetDevice());
    pDeviceVkImpl->SafeReleaseVkObject(m_pVkPSO);
#endif
}

IMPLEMENT_QUERY_INTERFACE( PipelineStateVkImpl, IID_PipelineStateVk, TPipelineStateBase )

void PipelineStateVkImpl::BindShaderResources(IResourceMapping *pResourceMapping, Uint32 Flags)
{
#if 0
    if( m_Desc.IsComputePipeline )
    { 
        if(m_pCS)m_pCS->BindResources(pResourceMapping, Flags);
    }
    else
    {
        if(m_pVS)m_pVS->BindResources(pResourceMapping, Flags);
        if(m_pPS)m_pPS->BindResources(pResourceMapping, Flags);
        if(m_pGS)m_pGS->BindResources(pResourceMapping, Flags);
        if(m_pDS)m_pDS->BindResources(pResourceMapping, Flags);
        if(m_pHS)m_pHS->BindResources(pResourceMapping, Flags);
    }
#endif
}

void PipelineStateVkImpl::CreateShaderResourceBinding(IShaderResourceBinding **ppShaderResourceBinding)
{
#if 0
    auto *pRenderDeviceVk = ValidatedCast<RenderDeviceVkImpl>( GetDevice() );
    auto &SRBAllocator = pRenderDeviceVk->GetSRBAllocator();
    auto pResBindingVk = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingVkImpl instance", ShaderResourceBindingVkImpl)(this, false);
    pResBindingVk->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
#endif
}

bool PipelineStateVkImpl::IsCompatibleWith(const IPipelineState *pPSO)const
{
    VERIFY_EXPR(pPSO != nullptr);

    if (pPSO == this)
        return true;
#if 0
    const PipelineStateVkImpl *pPSOVk = ValidatedCast<const PipelineStateVkImpl>(pPSO);
    if (m_ShaderResourceLayoutHash != pPSOVk->m_ShaderResourceLayoutHash)
        return false;

    auto IsSameRootSignature = m_RootSig.IsSameAs(pPSOVk->m_RootSig);

#ifdef _DEBUG
    {
        bool IsCompatibleShaders = true;
        if (m_NumShaders != pPSOVk->m_NumShaders)
            IsCompatibleShaders = false;

        if(IsCompatibleShaders)
        {
            for (Uint32 s = 0; s < m_NumShaders; ++s)
            {
                auto *pShader0 = ValidatedCast<ShaderVkImpl>(m_ppShaders[s]);
                auto *pShader1 = ValidatedCast<ShaderVkImpl>(pPSOVk->m_ppShaders[s]);
                if (pShader0->GetDesc().ShaderType != pShader1->GetDesc().ShaderType)
                {
                    IsCompatibleShaders = false;
                    break;
                }
                const ShaderResourcesVk *pRes0 = pShader0->GetShaderResources().get();
                const ShaderResourcesVk *pRes1 = pShader1->GetShaderResources().get();
                if (!pRes0->IsCompatibleWith(*pRes1))
                {
                    IsCompatibleShaders = false;
                    break;
                }
            }
        }

        if(IsCompatibleShaders)
            VERIFY(IsSameRootSignature, "Compatible shaders must have same root signatures");
    }
#endif
    
    return IsSameRootSignature;
#endif
    return true;
}

#if 0
const ShaderResourceLayoutVk& PipelineStateVkImpl::GetShaderResLayout(SHADER_TYPE ShaderType)const 
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    VERIFY_EXPR(m_pShaderResourceLayouts[ShaderInd] != nullptr);
    return *m_pShaderResourceLayouts[ShaderInd];
}

ShaderResourceCacheVk* PipelineStateVkImpl::CommitAndTransitionShaderResources(IShaderResourceBinding *pShaderResourceBinding, 
                                                                                     CommandContext &Ctx,
                                                                                     bool CommitResources,
                                                                                     bool TransitionResources)const
{
#ifdef VERIFY_SHADER_BINDINGS
    if (pShaderResourceBinding == nullptr &&
        (m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_MUTABLE) != 0 ||
         m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_DYNAMIC) != 0))
    {
        LOG_ERROR_MESSAGE("Pipeline state \"", m_Desc.Name, "\" contains mutable/dynamic shader variables and requires shader resource binding to commit all resources, but none is provided.");
    }
#endif

    // If the shaders contain no resources or static resources only, shader resource binding may be null. 
    // In this case use special internal SRB object
    auto *pResBindingVkImpl = pShaderResourceBinding ? ValidatedCast<ShaderResourceBindingVkImpl>(pShaderResourceBinding) : m_pDefaultShaderResBinding.get();
    
#ifdef VERIFY_SHADER_BINDINGS
    {
        auto *pRefPSO = pResBindingVkImpl->GetPipelineState();
        if ( IsIncompatibleWith(pRefPSO) )
        {
            LOG_ERROR_MESSAGE("Shader resource binding is incompatible with the pipeline state \"", m_Desc.Name, "\". Operation will be ignored.");
            return nullptr;
        }
    }
#endif

    // First time only, copy static shader resources to the cache
    if(!pResBindingVkImpl->StaticResourcesInitialized())
        pResBindingVkImpl->InitializeStaticResources(this);

#ifdef VERIFY_SHADER_BINDINGS
    pResBindingVkImpl->dbgVerifyResourceBindings(this);
#endif

    auto *pDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>( GetDevice() );
    auto &ResourceCache = pResBindingVkImpl->GetResourceCache();
    if(CommitResources)
    {
        if(m_Desc.IsComputePipeline)
            Ctx.AsComputeContext().SetRootSignature( GetVkRootSignature() );
        else
            Ctx.AsGraphicsContext().SetRootSignature( GetVkRootSignature() );

        if(TransitionResources)
            (m_RootSig.*m_RootSig.TransitionAndCommitDescriptorHandles)(pDeviceVkImpl, ResourceCache, Ctx, m_Desc.IsComputePipeline);
        else
            (m_RootSig.*m_RootSig.CommitDescriptorHandles)(pDeviceVkImpl, ResourceCache, Ctx, m_Desc.IsComputePipeline);
    }
    else
    {
        VERIFY(TransitionResources, "Resources should be transitioned or committed or both");
        m_RootSig.TransitionResources(ResourceCache, Ctx);
    }
    return &ResourceCache;
}


bool PipelineStateVkImpl::dbgContainsShaderResources()const
{
    return m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_STATIC) != 0 ||
           m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_MUTABLE) != 0 ||
           m_RootSig.GetTotalSrvCbvUavSlots(SHADER_VARIABLE_TYPE_DYNAMIC) != 0;
}
#endif
}
