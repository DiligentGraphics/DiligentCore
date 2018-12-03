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

/// \file
/// Declaration of Diligent::PipelineStateVkImpl class

#include <array>

#include "RenderDeviceVk.h"
#include "PipelineStateVk.h"
#include "PipelineStateBase.h"
#include "PipelineLayout.h"
#include "ShaderResourceLayoutVk.h"
#include "FixedBlockMemoryAllocator.h"
#include "SRBMemoryAllocator.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"
#include "VulkanUtilities/VulkanCommandBuffer.h"
#include "PipelineLayout.h"
#include "RenderDeviceVkImpl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Implementation of the Diligent::IRenderDeviceVk interface
class PipelineStateVkImpl final : public PipelineStateBase<IPipelineStateVk, RenderDeviceVkImpl>
{
public:
    using TPipelineStateBase = PipelineStateBase<IPipelineStateVk, RenderDeviceVkImpl>;

    PipelineStateVkImpl( IReferenceCounters* pRefCounters, RenderDeviceVkImpl* pDeviceVk, const PipelineStateDesc &PipelineDesc );
    ~PipelineStateVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject** ppInterface );
   
    virtual void CreateShaderResourceBinding( IShaderResourceBinding **ppShaderResourceBinding, bool InitStaticResources )override final;

    virtual bool IsCompatibleWith(const IPipelineState* pPSO)const override final;

    virtual VkRenderPass GetVkRenderPass()const override final{return m_RenderPass;}

    virtual VkPipeline GetVkPipeline()const override final { return m_Pipeline; }

    void CommitAndTransitionShaderResources(IShaderResourceBinding*                 pShaderResourceBinding, 
                                            DeviceContextVkImpl*                    pCtxVkImpl,
                                            bool                                    CommitResources,
                                            RESOURCE_STATE_TRANSITION_MODE          StateTransitionMode,
                                            PipelineLayout::DescriptorSetBindInfo*  pDescrSetBindInfo)const;

    void BindDescriptorSetsWithDynamicOffsets(DeviceContextVkImpl*                   pCtxVkImpl,
                                              PipelineLayout::DescriptorSetBindInfo& BindInfo)
    {
        m_PipelineLayout.BindDescriptorSetsWithDynamicOffsets(pCtxVkImpl, BindInfo);
    }

    const PipelineLayout& GetPipelineLayout()const{return m_PipelineLayout;}
    
    const ShaderResourceLayoutVk& GetShaderResLayout(Uint32 ShaderInd)const
    {
        VERIFY_EXPR(ShaderInd < m_NumShaders);
        return m_ShaderResourceLayouts[ShaderInd];
    }

    SRBMemoryAllocator& GetSRBMemoryAllocator()
    {
        return m_SRBMemAllocator;
    }
   
    static VkRenderPassCreateInfo GetRenderPassCreateInfo(Uint32                                                   NumRenderTargets, 
                                                          const TEXTURE_FORMAT                                     RTVFormats[], 
                                                          TEXTURE_FORMAT                                           DSVFormat,
                                                          Uint32                                                   SampleCount,
                                                          std::array<VkAttachmentDescription, MaxRenderTargets+1>& Attachments,
                                                          std::array<VkAttachmentReference,   MaxRenderTargets+1>& AttachmentReferences,
                                                          VkSubpassDescription&                                    SubpassDesc);


private:
    ShaderResourceLayoutVk*    m_ShaderResourceLayouts  = nullptr;
    
    // SRB memory allocator must be declared before m_pDefaultShaderResBinding
    SRBMemoryAllocator m_SRBMemAllocator;
    
    std::array<VulkanUtilities::ShaderModuleWrapper, MaxShadersInPipeline> m_ShaderModules;

    VkRenderPass m_RenderPass = VK_NULL_HANDLE; // Render passes are managed by the render device
    VulkanUtilities::PipelineWrapper m_Pipeline;
    PipelineLayout                   m_PipelineLayout;
    bool m_HasStaticResources    = false;
    bool m_HasNonStaticResources = false;
};

}
