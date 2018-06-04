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
#include "RenderDeviceVkImpl.h"
#include "DeviceContextVkImpl.h"
#include "SwapChainVk.h"
#include "PipelineStateVkImpl.h"
#include "CommandContext.h"
#include "TextureVkImpl.h"
#include "BufferVkImpl.h"
#include "VulkanTypeConversions.h"
#include "CommandListVkImpl.h"

namespace Diligent
{

    DeviceContextVkImpl::DeviceContextVkImpl( IReferenceCounters *pRefCounters, RenderDeviceVkImpl *pDeviceVkImpl, bool bIsDeferred, const EngineVkAttribs &Attribs, Uint32 ContextId) :
        TDeviceContextBase(pRefCounters, pDeviceVkImpl, bIsDeferred),
        m_NumCommandsToFlush(bIsDeferred ? std::numeric_limits<decltype(m_NumCommandsToFlush)>::max() : Attribs.NumCommandsToFlushCmdBuffer),
        /*m_MipsGenerator(pDeviceVkImpl->GetVkDevice()),*/
        m_CmdListAllocator(GetRawAllocator(), sizeof(CommandListVkImpl), 64 ),
        m_ContextId(ContextId),
        // Command pools for deferred contexts must be thread safe because finished command buffers are executed and released from another thread
        m_CmdPool(pDeviceVkImpl->GetLogicalDevice().GetSharedPtr(), pDeviceVkImpl->GetCmdQueue()->GetQueueFamilyIndex(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, bIsDeferred)
    {
#if 0
        auto *pVkDevice = pDeviceVkImpl->GetVkDevice();

        Vk_COMMAND_SIGNATURE_DESC CmdSignatureDesc = {};
        Vk_INDIRECT_ARGUMENT_DESC IndirectArg = {};
        CmdSignatureDesc.NodeMask = 0;
        CmdSignatureDesc.NumArgumentDescs = 1;
        CmdSignatureDesc.pArgumentDescs = &IndirectArg;

        CmdSignatureDesc.ByteStride = sizeof(UINT)*4;
        IndirectArg.Type = Vk_INDIRECT_ARGUMENT_TYPE_DRAW;
        auto hr = pVkDevice->CreateCommandSignature(&CmdSignatureDesc, nullptr, __uuidof(m_pDrawIndirectSignature), reinterpret_cast<void**>(static_cast<IVkCommandSignature**>(&m_pDrawIndirectSignature)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create indirect draw command signature")

        CmdSignatureDesc.ByteStride = sizeof(UINT)*5;
        IndirectArg.Type = Vk_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        hr = pVkDevice->CreateCommandSignature(&CmdSignatureDesc, nullptr, __uuidof(m_pDrawIndexedIndirectSignature), reinterpret_cast<void**>(static_cast<IVkCommandSignature**>(&m_pDrawIndexedIndirectSignature)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create draw indexed indirect command signature")

        CmdSignatureDesc.ByteStride = sizeof(UINT)*3;
        IndirectArg.Type = Vk_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        hr = pVkDevice->CreateCommandSignature(&CmdSignatureDesc, nullptr, __uuidof(m_pDispatchIndirectSignature), reinterpret_cast<void**>(static_cast<IVkCommandSignature**>(&m_pDispatchIndirectSignature)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create dispatch indirect command signature")
#endif
    }

    DeviceContextVkImpl::~DeviceContextVkImpl()
    {
        if(m_bIsDeferred)
        {
            DisposeCurrentCmdBuffer();
        }
        else
        {
            if (m_State.NumCommands != 0)
                LOG_WARNING_MESSAGE("Flusing outstanding commands from the device context being destroyed. This may result in synchronization errors");

            Flush();
        }

        auto VkCmdPool = m_CmdPool.Release();
        m_pDevice.RawPtr<RenderDeviceVkImpl>()->SafeReleaseVkObject(std::move(VkCmdPool));
    }

    IMPLEMENT_QUERY_INTERFACE( DeviceContextVkImpl, IID_DeviceContextVk, TDeviceContextBase )
    
    inline void DeviceContextVkImpl::EnsureVkCmdBuffer()
    {
        // Make sure that the number of commands in the context is at least one,
        // so that the context cannot be disposed by Flush()
        m_State.NumCommands = m_State.NumCommands != 0 ? m_State.NumCommands : 1;
        if (m_CommandBuffer.GetVkCmdBuffer() == VK_NULL_HANDLE)
        {
            auto pDeviceVkImpl = m_pDevice.RawPtr<RenderDeviceVkImpl>();
            auto vkCmdBuff = m_CmdPool.GetCommandBuffer(pDeviceVkImpl->GetCompletedFenceValue());
            m_CommandBuffer.SetVkCmdBuffer(vkCmdBuff);
        }
    }

    void DeviceContextVkImpl::DisposeVkCmdBuffer(VkCommandBuffer vkCmdBuff)
    {
        VERIFY_EXPR(vkCmdBuff != VK_NULL_HANDLE);
        auto pDeviceVkImpl = m_pDevice.RawPtr<RenderDeviceVkImpl>();
        m_CmdPool.DisposeCommandBuffer(vkCmdBuff, pDeviceVkImpl->GetNextFenceValue());
    }

    inline void DeviceContextVkImpl::DisposeCurrentCmdBuffer()
    {
        VERIFY(m_CommandBuffer.GetState().RenderPass == VK_NULL_HANDLE, "Disposing command buffer with unifinished render pass");
        auto vkCmdBuff = m_CommandBuffer.GetVkCmdBuffer();
        if(vkCmdBuff != VK_NULL_HANDLE)
        {
            DisposeVkCmdBuffer(vkCmdBuff);
            m_CommandBuffer.Reset();
        }
    }


    void DeviceContextVkImpl::SetPipelineState(IPipelineState *pPipelineState)
    {
        if (m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
        {
            m_CommandBuffer.EndRenderPass();
        }
        m_CommandBuffer.ResetFramebuffer();

        // Never flush deferred context!
        if (!m_bIsDeferred && m_State.NumCommands >= m_NumCommandsToFlush)
        {
            Flush();
        }

        auto *pPipelineStateVk = ValidatedCast<PipelineStateVkImpl>(pPipelineState);
        const auto &PSODesc = pPipelineStateVk->GetDesc();

        bool CommitStates = false;
        bool CommitScissor = false;
        if(!m_pPipelineState)
        {
            // If no pipeline state is bound, we are working with the fresh command
            // list. We have to commit the states set in the context that are not
            // committed by the draw command (render targets, viewports, scissor rects, etc.)
            CommitStates = true;
        }
        else
        {
            const auto& OldPSODesc = m_pPipelineState->GetDesc();
            // Commit all graphics states when switching from compute pipeline 
            // This is necessary because if the command list had been flushed
            // and the first PSO set on the command list was a compute pipeline, 
            // the states would otherwise never be committed (since m_pPipelineState != nullptr)
            CommitStates = OldPSODesc.IsComputePipeline;
            // We also need to update scissor rect if ScissorEnable state was disabled in previous pipeline
            CommitScissor = !OldPSODesc.GraphicsPipeline.RasterizerDesc.ScissorEnable;
        }

        TDeviceContextBase::SetPipelineState( pPipelineState );
        EnsureVkCmdBuffer();

        if (PSODesc.IsComputePipeline)
        {
            auto vkPipeline = pPipelineStateVk->GetVkPipeline();
            m_CommandBuffer.BindComputePipeline(vkPipeline);
        }
        else
        {
            auto vkPipeline = pPipelineStateVk->GetVkPipeline();
            m_CommandBuffer.BindGraphicsPipeline(vkPipeline);

            if(CommitStates)
            {
                m_CommandBuffer.SetStencilReference(m_StencilRef);
                m_CommandBuffer.SetBlendConstants(m_BlendFactors);
                CommitRenderTargets();
                CommitViewports();
            }

            if(PSODesc.GraphicsPipeline.RasterizerDesc.ScissorEnable && (CommitStates || CommitScissor))
            {
                CommitScissorRects();
            }
        }
        
#if 0
        m_pCommittedResourceCache = nullptr;
#endif
    }

    void DeviceContextVkImpl::TransitionShaderResources(IPipelineState *pPipelineState, IShaderResourceBinding *pShaderResourceBinding)
    {
        VERIFY_EXPR(pPipelineState != nullptr);

        auto *pPipelineStateVk = ValidatedCast<PipelineStateVkImpl>(pPipelineState);
        pPipelineStateVk->CommitAndTransitionShaderResources(pShaderResourceBinding, this, false, true);
    }

    void DeviceContextVkImpl::CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags)
    {
        if (!DeviceContextBase::CommitShaderResources<PipelineStateVkImpl>(pShaderResourceBinding, Flags, 0 /*Dummy*/))
            return;

        auto *pPipelineStateVk = m_pPipelineState.RawPtr<PipelineStateVkImpl>();
        m_pCommittedResourceCache = pPipelineStateVk->CommitAndTransitionShaderResources(pShaderResourceBinding, this, true, (Flags & COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES)!=0);
    }

    void DeviceContextVkImpl::SetStencilRef(Uint32 StencilRef)
    {
        if (TDeviceContextBase::SetStencilRef(StencilRef, 0))
        {
            EnsureVkCmdBuffer();
            m_CommandBuffer.SetStencilReference(m_StencilRef);
        }
    }

    void DeviceContextVkImpl::SetBlendFactors(const float* pBlendFactors)
    {
        if (TDeviceContextBase::SetBlendFactors(m_BlendFactors, 0))
        {
            EnsureVkCmdBuffer();
            m_CommandBuffer.SetBlendConstants(m_BlendFactors);
        }
    }

    void DeviceContextVkImpl::CommitRenderPassAndFramebuffer(PipelineStateVkImpl *pPipelineStateVk)
    {
        auto *pRenderDeviceVk = m_pDevice.RawPtr<RenderDeviceVkImpl>();        
        auto &FBCache = pRenderDeviceVk->GetFramebufferCache();
        auto RenderPass = pPipelineStateVk->GetVkRenderPass();
        const auto& CmdBufferState = m_CommandBuffer.GetState();
        if(CmdBufferState.Framebuffer != VK_NULL_HANDLE)
        {
            // Render targets have not changed since the last time, so we can reuse 
            // previously bound framebuffer
            VERIFY_EXPR(m_FramebufferWidth == CmdBufferState.FramebufferWidth && m_FramebufferHeight == CmdBufferState.FramebufferHeight);
            m_CommandBuffer.BeginRenderPass(RenderPass, CmdBufferState.Framebuffer, CmdBufferState.FramebufferWidth, CmdBufferState.FramebufferHeight);
            return;
        }

        const auto &GrPipelineDesc = pPipelineStateVk->GetDesc().GraphicsPipeline;
        FramebufferCache::FramebufferCacheKey Key;
        Key.Pass = RenderPass;
        if(m_NumBoundRenderTargets == 0 && !m_pBoundDepthStencil)
        {
            auto *pSwapChainVk = m_pSwapChain.RawPtr<ISwapChainVk>();
            if(GrPipelineDesc.DSVFormat != TEX_FORMAT_UNKNOWN)
            {
                auto *pDSV = pSwapChainVk->GetDepthBufferDSV();
                TransitionImageLayout(pDSV->GetTexture(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                Key.DSV = pDSV->GetVulkanImageView();
            }
            else 
                Key.DSV = VK_NULL_HANDLE;

            VERIFY(GrPipelineDesc.NumRenderTargets <= 1, "Pipeline state expects ", GrPipelineDesc.NumRenderTargets, " render targets, but default framebuffer has only one");
            if(GrPipelineDesc.RTVFormats[0] != TEX_FORMAT_UNKNOWN)
            {
                auto *pRTV = pSwapChainVk->GetCurrentBackBufferRTV();
                TransitionImageLayout(pRTV->GetTexture(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                Key.NumRenderTargets = 1;
                Key.RTVs[0] = pRTV->GetVulkanImageView();
            }
            else
                Key.NumRenderTargets = 0;
        }
        else
        {
            if (GrPipelineDesc.DSVFormat != TEX_FORMAT_UNKNOWN)
            {
                if(m_pBoundDepthStencil)
                {
                    auto *pDSVVk = m_pBoundDepthStencil.RawPtr<TextureViewVkImpl>();
                    TransitionImageLayout(pDSVVk->GetTexture(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                    Key.DSV = pDSVVk->GetVulkanImageView();
                }
                else
                {
                    LOG_ERROR("Currently bound graphics pipeline state expects depth-stencil buffer, but none is currently bound. This is an error and will result in undefined behavior");
                    Key.DSV = VK_NULL_HANDLE;
                }
            }
            else
                Key.DSV = nullptr;

            VERIFY(m_NumBoundRenderTargets >= GrPipelineDesc.NumRenderTargets, "Pipeline state expects ", GrPipelineDesc.NumRenderTargets, " render targets, but only ", m_NumBoundRenderTargets, " is bound");
            Key.NumRenderTargets = GrPipelineDesc.NumRenderTargets;
            for(Uint32 rt=0; rt < Key.NumRenderTargets; ++rt)
            {
                if(ITextureView *pRTV = m_pBoundRenderTargets[rt])
                {
                    auto *pRTVVk = ValidatedCast<TextureViewVkImpl>(pRTV);
                    TransitionImageLayout(pRTV->GetTexture(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                    Key.RTVs[rt] = pRTVVk->GetVulkanImageView();
                }
                else
                    Key.RTVs[rt] = nullptr;
            }
        }
        auto vkFramebuffer = FBCache.GetFramebuffer(Key, m_FramebufferWidth, m_FramebufferHeight, m_FramebufferSlices);
        m_CommandBuffer.BeginRenderPass(Key.Pass, vkFramebuffer, m_FramebufferWidth, m_FramebufferHeight);
    }

    void DeviceContextVkImpl::TransitionVkVertexBuffers()
    {
        for( UINT Buff = 0; Buff < m_NumVertexStreams; ++Buff )
        {
            auto &CurrStream = m_VertexStreams[Buff];
            VERIFY( CurrStream.pBuffer, "Attempting to bind a null buffer for rendering" );
            auto *pBufferVk = CurrStream.pBuffer.RawPtr<BufferVkImpl>();
            if(!pBufferVk->CheckAccessFlags(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT))
                BufferMemoryBarrier(*pBufferVk, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
        }
    }

    void DeviceContextVkImpl::CommitVkVertexBuffers()
    {
#ifdef _DEBUG
        auto *pPipelineStateVk = m_pPipelineState.RawPtr<PipelineStateVkImpl>();
        VERIFY( m_NumVertexStreams >= pPipelineStateVk->GetNumBufferSlotsUsed(), "Currently bound pipeline state \"", pPipelineStateVk->GetDesc().Name, "\" expects ", pPipelineStateVk->GetNumBufferSlotsUsed(), " input buffer slots, but only ", m_NumVertexStreams, " is bound");
#endif
        // Do not initialize array with zeroes for performance reasons
        VkBuffer vkVertexBuffers[MaxBufferSlots];// = {}
        VkDeviceSize Offsets[MaxBufferSlots];
        VERIFY( m_NumVertexStreams <= MaxBufferSlots, "Too many buffers are being set" );
        //bool DynamicBufferPresent = false;
        for( UINT slot = 0; slot < m_NumVertexStreams; ++slot )
        {
            auto &CurrStream = m_VertexStreams[slot];
            //auto &VBView = VBViews[Buff];
            VERIFY( CurrStream.pBuffer, "Attempting to bind a null buffer for rendering" );
            
            auto *pBufferVk = CurrStream.pBuffer.RawPtr<BufferVkImpl>();
//            if (pBufferVk->GetDesc().Usage == USAGE_DYNAMIC)
//            {
//                DynamicBufferPresent = true;
//#ifdef _DEBUG
//                pBufferVk->DbgVerifyDynamicAllocation(m_ContextId);
//#endif
//            }
            if(!pBufferVk->CheckAccessFlags(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT))
                BufferMemoryBarrier(*pBufferVk, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
            
            // Device context keeps strong references to all vertex buffers.
            // When a buffer is unbound, a reference to Vk resource is added to the context,
            // so there is no need to reference the resource here
            //GraphicsCtx.AddReferencedObject(pVkResource);

            vkVertexBuffers[slot] = pBufferVk->GetVkBuffer();
            Offsets[slot] = CurrStream.Offset;

            ///VBView.BufferLocation = pBufferVk->GetGPUAddress(m_ContextId) + CurrStream.Offset;
        }

        //GraphCtx.FlushResourceBarriers();
        if(m_NumVertexStreams > 0)
            m_CommandBuffer.BindVertexBuffers( 0, m_NumVertexStreams, vkVertexBuffers, Offsets );

        // GPU virtual address of a dynamic vertex buffer can change every time
        // a draw command is invoked
        m_State.CommittedVBsUpToDate = true;//!DynamicBufferPresent;
    }


    void DeviceContextVkImpl::Draw( DrawAttribs &DrawAttribs )
    {
#ifdef _DEBUG
        if (!m_pPipelineState)
        {
            LOG_ERROR("No pipeline state is bound");
            return;
        }
        if (m_pPipelineState->GetDesc().IsComputePipeline)
        {
            LOG_ERROR("No graphics pipeline state is bound");
            return;
        }
#endif

        auto *pPipelineStateVk = m_pPipelineState.RawPtr<PipelineStateVkImpl>();

        EnsureVkCmdBuffer();

        if( DrawAttribs.IsIndexed )
        {
            VERIFY( m_pIndexBuffer != nullptr, "Index buffer is not set up for indexed draw command" );

            BufferVkImpl *pBuffVk = m_pIndexBuffer.RawPtr<BufferVkImpl>();
            if(!pBuffVk->CheckAccessFlags(VK_ACCESS_INDEX_READ_BIT))
                BufferMemoryBarrier(*pBuffVk, VK_ACCESS_INDEX_READ_BIT);

            VERIFY(DrawAttribs.IndexType == VT_UINT16 || DrawAttribs.IndexType == VT_UINT32, "Unsupported index format. Only R16_UINT and R32_UINT are allowed.");
            VkIndexType vkIndexType = DrawAttribs.IndexType == VT_UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
            m_CommandBuffer.BindIndexBuffer(pBuffVk->GetVkBuffer(), m_IndexDataStartOffset, vkIndexType);
        }

        if(m_State.CommittedVBsUpToDate)
            TransitionVkVertexBuffers();
        else
            CommitVkVertexBuffers();

#if 0
        if(m_pCommittedResourceCache != nullptr)
        {
            pPipelineStateVk->GetRootSignature().CommitRootViews(*m_pCommittedResourceCache, GraphCtx, false, m_ContextId);
        }
#ifdef _DEBUG
        else
        {
            if( pPipelineStateVk->dbgContainsShaderResources() )
                LOG_ERROR_MESSAGE("Pipeline state \"", pPipelineStateVk->GetDesc().Name, "\" contains shader resources, but IDeviceContext::CommitShaderResources() was not called" );
        }
#endif
#endif
        if( DrawAttribs.IsIndirect )
        {
            VERIFY(DrawAttribs.pIndirectDrawAttribs != nullptr, "Valid pIndirectDrawAttribs must be provided for indirect draw command");

            auto *pBufferVk = ValidatedCast<BufferVkImpl>(DrawAttribs.pIndirectDrawAttribs);
            
            // Buffer memory barries must be executed outside of render pass
            if(!pBufferVk->CheckAccessFlags(VK_ACCESS_INDIRECT_COMMAND_READ_BIT))
                BufferMemoryBarrier(*pBufferVk, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
        }

        if(m_CommandBuffer.GetState().RenderPass == VK_NULL_HANDLE)
            CommitRenderPassAndFramebuffer(pPipelineStateVk);

        if( DrawAttribs.IsIndirect )
        {
            if( auto *pBufferVk = ValidatedCast<BufferVkImpl>(DrawAttribs.pIndirectDrawAttribs) )
            {
//#ifdef _DEBUG
//                if(pBufferVk->GetDesc().Usage == USAGE_DYNAMIC)
//                    pBufferVk->DbgVerifyDynamicAllocation(m_ContextId);
//#endif
                if(!pBufferVk->CheckAccessFlags(VK_ACCESS_INDIRECT_COMMAND_READ_BIT))
                    BufferMemoryBarrier(*pBufferVk, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

                if( DrawAttribs.IsIndexed )
                    m_CommandBuffer.DrawIndexedIndirect(pBufferVk->GetVkBuffer(), DrawAttribs.IndirectDrawArgsOffset, 1, 0);
                else
                    m_CommandBuffer.DrawIndirect(pBufferVk->GetVkBuffer(), DrawAttribs.IndirectDrawArgsOffset, 1, 0);
            }
        }
        else
        {
            if( DrawAttribs.IsIndexed )
                m_CommandBuffer.DrawIndexed(DrawAttribs.NumIndices, DrawAttribs.NumInstances, DrawAttribs.FirstIndexLocation, DrawAttribs.BaseVertex, DrawAttribs.FirstInstanceLocation);
            else
                m_CommandBuffer.Draw(DrawAttribs.NumVertices, DrawAttribs.NumInstances, DrawAttribs.StartVertexLocation, DrawAttribs.FirstInstanceLocation );
        }

        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )
    {
#ifdef _DEBUG
        if (!m_pPipelineState)
        {
            LOG_ERROR("No pipeline state is bound");
            return;
        }
        if (!m_pPipelineState->GetDesc().IsComputePipeline)
        {
            LOG_ERROR("No compute pipeline state is bound");
            return;
        }
#endif

#if 0
        auto *pPipelineStateVk = m_pPipelineState.RawPtr<PipelineStateVkImpl>();

        auto &ComputeCtx = RequestCmdContext()->AsComputeContext();
        ComputeCtx.SetRootSignature( pPipelineStateVk->GetVkRootSignature() );
      
        if(m_pCommittedResourceCache != nullptr)
        {
            pPipelineStateVk->GetRootSignature().CommitRootViews(*m_pCommittedResourceCache, ComputeCtx, true, m_ContextId);
        }
#ifdef _DEBUG
        else
        {
            if( pPipelineStateVk->dbgContainsShaderResources() )
                LOG_ERROR_MESSAGE("Pipeline state \"", pPipelineStateVk->GetDesc().Name, "\" contains shader resources, but IDeviceContext::CommitShaderResources() was not called" );
        }
#endif

        if( DispatchAttrs.pIndirectDispatchAttribs )
        {
            if( auto *pBufferVk = ValidatedCast<BufferVkImpl>(DispatchAttrs.pIndirectDispatchAttribs) )
            {
#ifdef _DEBUG
                if(pBufferVk->GetDesc().Usage == USAGE_DYNAMIC)
                    pBufferVk->DbgVerifyDynamicAllocation(m_ContextId);
#endif

                ComputeCtx.TransitionResource(pBufferVk, Vk_RESOURCE_STATE_INDIRECT_ARGUMENT);
                size_t BuffDataStartByteOffset;
                IVkResource *pVkArgsBuff = pBufferVk->GetVkBuffer(BuffDataStartByteOffset, m_ContextId);
                ComputeCtx.ExecuteIndirect(m_pDispatchIndirectSignature, pVkArgsBuff, DispatchAttrs.DispatchArgsByteOffset + BuffDataStartByteOffset);
            }
            else
            {
                LOG_ERROR_MESSAGE("Valid pIndirectDrawAttribs must be provided for indirect dispatch command");
            }
        }
        else
            ComputeCtx.Dispatch(DispatchAttrs.ThreadGroupCountX, DispatchAttrs.ThreadGroupCountY, DispatchAttrs.ThreadGroupCountZ);
#endif
        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::ClearDepthStencil( ITextureView *pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil )
    {
        ITextureViewVk *pVkDSV = nullptr;
        if( pView != nullptr )
        {
            pVkDSV = ValidatedCast<ITextureViewVk>(pView);
#ifdef _DEBUG
            const auto& ViewDesc = pVkDSV->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Incorrect view type: depth stencil is expected" );
#endif
        }
        else
        {
            if (m_pSwapChain)
            {
                pVkDSV = m_pSwapChain.RawPtr<ISwapChainVk>()->GetDepthBufferDSV();
            }
            else
            {
                LOG_ERROR("Failed to clear default depth stencil buffer: swap chain is not initialized in the device context");
                return;
            }
        }

        auto *pTexture = pVkDSV->GetTexture();
        auto *pTextureVk = ValidatedCast<TextureVkImpl>(pTexture);
        const auto &ViewDesc = pVkDSV->GetDesc();
        EnsureVkCmdBuffer();
        bool ClearedAsAttachment = false;
        if(m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
        {
            if(m_pPipelineState && 
               m_pPipelineState->GetDesc().GraphicsPipeline.DSVFormat != TEX_FORMAT_UNKNOWN && 
               (pView == nullptr || pView == m_pBoundDepthStencil))
            {
                VkClearAttachment ClearAttachment = {};
                ClearAttachment.aspectMask = 0;
                if (ClearFlags & CLEAR_DEPTH_FLAG)   ClearAttachment.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
                if (ClearFlags & CLEAR_STENCIL_FLAG) ClearAttachment.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                // colorAttachment is only meaningful if VK_IMAGE_ASPECT_COLOR_BIT is set in aspectMask
                ClearAttachment.colorAttachment = VK_ATTACHMENT_UNUSED;
                ClearAttachment.clearValue.depthStencil.depth = fDepth;
                ClearAttachment.clearValue.depthStencil.stencil = Stencil;
                VkClearRect ClearRect;
                ClearRect.rect = { { 0,0 },{ m_FramebufferWidth, m_FramebufferHeight } };
                ClearRect.baseArrayLayer = ViewDesc.FirstArraySlice;
                ClearRect.layerCount = ViewDesc.NumArraySlices;
                // No memory barriers are needed between vkCmdClearAttachments and preceding or 
                // subsequent draw or attachment clear commands in the same subpass (17.2)
                m_CommandBuffer.ClearAttachment(ClearAttachment, ClearRect);
                ClearedAsAttachment = true;
            }
            else
            {
                // End render pass to clear the buffer with vkCmdClearDepthStencilImage
                m_CommandBuffer.EndRenderPass();
            }
        }

        if(!ClearedAsAttachment)
        {
            // Image layout must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (17.1)
            TransitionImageLayout(pTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkClearDepthStencilValue ClearValue;
            ClearValue.depth = fDepth;
            ClearValue.stencil = Stencil;
            VkImageSubresourceRange Subresource;
            Subresource.aspectMask = 0;
            if (ClearFlags & CLEAR_DEPTH_FLAG)   Subresource.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
            if (ClearFlags & CLEAR_STENCIL_FLAG) Subresource.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            Subresource.baseArrayLayer = ViewDesc.FirstArraySlice;
            Subresource.layerCount = ViewDesc.NumArraySlices;
            Subresource.baseMipLevel = ViewDesc.MostDetailedMip;
            Subresource.levelCount = ViewDesc.NumMipLevels;

            m_CommandBuffer.ClearDepthStencilImage(pTextureVk->GetVkImage(), ClearValue, Subresource);
        }

        ++m_State.NumCommands;
    }

    VkClearColorValue ClearValueToVkClearValue(const float *RGBA, TEXTURE_FORMAT TexFmt)
    {
        VkClearColorValue ClearValue;
        const auto& FmtAttribs = GetTextureFormatAttribs(TexFmt);
        if(FmtAttribs.ComponentType == COMPONENT_TYPE_SINT)
        {
            for(int i=0; i < 4; ++i)
                ClearValue.int32[i] = static_cast<int32_t>(RGBA[i]);
        }
        else if (FmtAttribs.ComponentType == COMPONENT_TYPE_UINT)
        {
            for (int i = 0; i < 4; ++i)
                ClearValue.uint32[i] = static_cast<uint32_t>(RGBA[i]);
        }
        else
        {
            for (int i = 0; i < 4; ++i)
                ClearValue.float32[i] = RGBA[i];
        }

        return ClearValue;
    }

    void DeviceContextVkImpl::ClearRenderTarget( ITextureView *pView, const float *RGBA )
    {
        ITextureViewVk *pVkRTV = nullptr;
        if( pView != nullptr )
        {
#ifdef _DEBUG
            const auto& ViewDesc = pView->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET, "Incorrect view type: render target is expected" );
#endif
            pVkRTV = ValidatedCast<ITextureViewVk>(pView);
        }
        else
        {
            if (m_pSwapChain)
            {
                pVkRTV = m_pSwapChain.RawPtr<ISwapChainVk>()->GetCurrentBackBufferRTV();
            }
            else
            {
                LOG_ERROR("Failed to clear default render target: swap chain is not initialized in the device context");
                return;
            }
        }

        static constexpr float Zero[4] = { 0.f, 0.f, 0.f, 0.f };
        if( RGBA == nullptr )
            RGBA = Zero;
        
        auto *pTexture = pVkRTV->GetTexture();
        auto *pTextureVk = ValidatedCast<TextureVkImpl>(pTexture);
        const auto &ViewDesc = pVkRTV->GetDesc();
        EnsureVkCmdBuffer();

        bool ClearedAsAttachment = false;
        if(m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
        {
            static constexpr Uint32 InvalidAttachmentIndex = static_cast<Uint32>(-1);
            Uint32 attachmentIndex = InvalidAttachmentIndex;
            if(pView == nullptr)
            {
                attachmentIndex = 0;
            }
            else
            {
                VERIFY(m_pPipelineState, "Valid pipeline state must be bound inside active render pass");
                const auto &GrPipelineDesc = m_pPipelineState->GetDesc().GraphicsPipeline;
                VERIFY(m_NumBoundRenderTargets >= GrPipelineDesc.NumRenderTargets, "Pipeline state expects ", GrPipelineDesc.NumRenderTargets, " render targets, but only ", m_NumBoundRenderTargets, " is bound");
                Uint32 MaxRTIndex = std::min(Uint32{GrPipelineDesc.NumRenderTargets}, m_NumBoundRenderTargets);
                for(Uint32 rt = 0; rt < MaxRTIndex; ++rt)
                {
                    if(m_pBoundRenderTargets[rt] == pView)
                    {
                        attachmentIndex = rt;
                        break;
                    }
                }
            }

            if(attachmentIndex != InvalidAttachmentIndex)
            {
                VkClearAttachment ClearAttachment = {};
                ClearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                // colorAttachment is only meaningful if VK_IMAGE_ASPECT_COLOR_BIT is set in aspectMask, 
                // in which case it is an index to the pColorAttachments array in the VkSubpassDescription 
                // structure of the current subpass which selects the color attachment to clear (17.2)
                // It is NOT the render pass attachment index
                ClearAttachment.colorAttachment = attachmentIndex;
                ClearAttachment.clearValue.color = ClearValueToVkClearValue(RGBA, ViewDesc.Format);
                VkClearRect ClearRect;
                ClearRect.rect = {{0,0}, {m_FramebufferWidth, m_FramebufferHeight}};
                ClearRect.baseArrayLayer = ViewDesc.FirstArraySlice;
                ClearRect.layerCount = ViewDesc.NumArraySlices;
                // No memory barriers are needed between vkCmdClearAttachments and preceding or 
                // subsequent draw or attachment clear commands in the same subpass (17.2)
                m_CommandBuffer.ClearAttachment(ClearAttachment, ClearRect);
                ClearedAsAttachment = true;
            }
            else
            {
                // End current render pass and clear the image with vkCmdClearColorImage
                m_CommandBuffer.EndRenderPass();
            }
        }

        if(!ClearedAsAttachment)
        {
            // Image layout must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (17.1)
            TransitionImageLayout(pTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            auto ClearValue = ClearValueToVkClearValue(RGBA, ViewDesc.Format);
            VkImageSubresourceRange Subresource;
            Subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Subresource.baseArrayLayer = ViewDesc.FirstArraySlice;
            Subresource.layerCount = ViewDesc.NumArraySlices;
            Subresource.baseMipLevel = ViewDesc.MostDetailedMip;
            Subresource.levelCount = ViewDesc.NumMipLevels;
            VERIFY(ViewDesc.NumMipLevels, "RTV must contain single mip level");

            m_CommandBuffer.ClearColorImage(pTextureVk->GetVkImage(), ClearValue, Subresource);
        }

        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::Flush()
    {
        VERIFY(!m_bIsDeferred, "Flush() should only be called for immediate contexts");

        VkSubmitInfo SubmitInfo = {};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfo.pNext = nullptr;

        auto pDeviceVkImpl = m_pDevice.RawPtr<RenderDeviceVkImpl>();
        auto vkCmdBuff = m_CommandBuffer.GetVkCmdBuffer();
        if(vkCmdBuff != VK_NULL_HANDLE )
        {
            VERIFY(!m_bIsDeferred, "Deferred contexts cannot execute command lists directly");
            if (m_State.NumCommands != 0)
            {
                if (m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
                {
                    m_CommandBuffer.EndRenderPass();
                }

                m_CommandBuffer.FlushBarriers();
                m_CommandBuffer.EndCommandBuffer();
                
                SubmitInfo.commandBufferCount = 1;
                SubmitInfo.pCommandBuffers = &vkCmdBuff;
            }
        }

        SubmitInfo.waitSemaphoreCount = static_cast<uint32_t>(m_WaitSemaphores.size());
        VERIFY_EXPR(m_WaitSemaphores.size() == m_WaitDstStageMasks.size());
        SubmitInfo.pWaitSemaphores = SubmitInfo.waitSemaphoreCount != 0 ? m_WaitSemaphores.data() : nullptr;
        SubmitInfo.pWaitDstStageMask = SubmitInfo.waitSemaphoreCount != 0 ? m_WaitDstStageMasks.data() : nullptr;
        SubmitInfo.signalSemaphoreCount = static_cast<uint32_t>(m_SignalSemaphores.size());
        SubmitInfo.pSignalSemaphores = SubmitInfo.signalSemaphoreCount != 0 ? m_SignalSemaphores.data() : nullptr;

        // Submit command buffer even if there are no commands to release stale resources.
        //if(SubmitInfo.commandBufferCount != 0 || SubmitInfo.waitSemaphoreCount !=0 || SubmitInfo.signalSemaphoreCount != 0)
        {
            pDeviceVkImpl->ExecuteCommandBuffer(SubmitInfo, true);
        }

        m_WaitSemaphores.clear();
        m_WaitDstStageMasks.clear();
        m_SignalSemaphores.clear();

        if (vkCmdBuff != VK_NULL_HANDLE)
        {
            DisposeCurrentCmdBuffer();
        }

        m_State = ContextState{};
        m_CommandBuffer.Reset();
        m_pPipelineState.Release(); 
    }

    void DeviceContextVkImpl::SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pOffsets, Uint32 Flags )
    {
        TDeviceContextBase::SetVertexBuffers( StartSlot, NumBuffersSet, ppBuffers, pOffsets, Flags );
        m_State.CommittedVBsUpToDate = false;
    }

    void DeviceContextVkImpl::InvalidateState()
    {
        if (m_State.NumCommands != 0)
            LOG_WARNING_MESSAGE("Invalidating context that has outstanding commands in it. Call Flush() to submit commands for execution");

        TDeviceContextBase::InvalidateState();
        m_State = ContextState{};
        VERIFY(m_CommandBuffer.GetState().RenderPass == VK_NULL_HANDLE, "Invalidating context with unifinished render pass");
        m_CommandBuffer.Reset();
    }

    void DeviceContextVkImpl::SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )
    {
        TDeviceContextBase::SetIndexBuffer( pIndexBuffer, ByteOffset );
        m_State.CommittedIBUpToDate = false;
    }


    void DeviceContextVkImpl::CommitViewports()
    {
        VkViewport VkViewports[MaxViewports]; // Do not waste time initializing array to zero
        for( Uint32 vp = 0; vp < m_NumViewports; ++vp )
        {
            VkViewports[vp].x        = m_Viewports[vp].TopLeftX;
            VkViewports[vp].y        = m_FramebufferHeight - m_Viewports[vp].TopLeftY;
            VkViewports[vp].width    = m_Viewports[vp].Width;
            VkViewports[vp].height   = -m_Viewports[vp].Height;
            VkViewports[vp].minDepth = m_Viewports[vp].MinDepth;
            VkViewports[vp].maxDepth = m_Viewports[vp].MaxDepth;
        }
        EnsureVkCmdBuffer();
        // TODO: reinterpret_cast m_Viewports to VkViewports?
        m_CommandBuffer.SetViewports(0, m_NumViewports, VkViewports);
    }

    void DeviceContextVkImpl::SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight  )
    {
        TDeviceContextBase::SetViewports( NumViewports, pViewports, RTWidth, RTHeight );
        VERIFY( NumViewports == m_NumViewports, "Unexpected number of viewports" );

        CommitViewports();
    }

    void DeviceContextVkImpl::CommitScissorRects()
    {
        VERIFY(m_pPipelineState && m_pPipelineState->GetDesc().GraphicsPipeline.RasterizerDesc.ScissorEnable, "Scissor test must be enabled in the graphics pipeline");

        VkRect2D VkScissorRects[MaxViewports]; // Do not waste time initializing array with zeroes
        for (Uint32 sr = 0; sr < m_NumScissorRects; ++sr)
        {
            const auto &SrcRect = m_ScissorRects[sr];
            VkScissorRects[sr].offset = {SrcRect.left, SrcRect.top};
            VkScissorRects[sr].extent = {static_cast<uint32_t>(SrcRect.right - SrcRect.left), static_cast<uint32_t>(SrcRect.bottom - SrcRect.top)};
        }

        EnsureVkCmdBuffer();
        // TODO: reinterpret_cast m_Viewports to m_Viewports?
        m_CommandBuffer.SetScissorRects(0, m_NumScissorRects, VkScissorRects);
    }


    void DeviceContextVkImpl::SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight  )
    {
        TDeviceContextBase::SetScissorRects(NumRects, pRects, RTWidth, RTHeight);

        // Only commit scissor rects if scissor test is enabled in the rasterizer state. 
        // If scissor is currently disabled, or no PSO is bound, scissor rects will be committed by 
        // the SetPipelineState() when a PSO with enabled scissor test is set.
        if( m_pPipelineState )
        {
            const auto &PSODesc = m_pPipelineState->GetDesc();
            if(!PSODesc.IsComputePipeline && PSODesc.GraphicsPipeline.RasterizerDesc.ScissorEnable)
            {
                VERIFY(NumRects == m_NumScissorRects, "Unexpected number of scissor rects");
                CommitScissorRects();
            }
        }
    }


    void DeviceContextVkImpl::CommitRenderTargets()
    {
#if 0
        const Uint32 MaxVkRTs = Vk_SIMULTANEOUS_RENDER_TARGET_COUNT;
        Uint32 NumRenderTargets = m_NumBoundRenderTargets;
        VERIFY( NumRenderTargets <= MaxVkRTs, "Vk only allows 8 simultaneous render targets" );
        NumRenderTargets = std::min( MaxVkRTs, NumRenderTargets );

        ITextureViewVk *ppRTVs[MaxVkRTs]; // Do not initialize with zeroes!
        ITextureViewVk *pDSV = nullptr;
        if( m_IsDefaultFramebufferBound )
        {
            if (m_pSwapChain)
            {
                NumRenderTargets = 1;
                auto *pSwapChainVk = m_pSwapChain.RawPtr<ISwapChainVk>();
                ppRTVs[0] = pSwapChainVk->GetCurrentBackBufferRTV();
                pDSV = pSwapChainVk->GetDepthBufferDSV();
            }
            else
            {
                LOG_WARNING_MESSAGE("Failed to bind default render targets and depth-stencil buffer: swap chain is not initialized in the device context");
                return;
            }
        }
        else
        {
            for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
                ppRTVs[rt] = m_pBoundRenderTargets[rt].RawPtr<ITextureViewVk>();
            pDSV = m_pBoundDepthStencil.RawPtr<ITextureViewVk>();
        }
        RequestCmdContext()->AsGraphicsContext().SetRenderTargets(NumRenderTargets, ppRTVs, pDSV);
#endif
    }

    void DeviceContextVkImpl::SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )
    {
        if( TDeviceContextBase::SetRenderTargets( NumRenderTargets, ppRenderTargets, pDepthStencil ) )
        {
            if(m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
                m_CommandBuffer.EndRenderPass();
            m_CommandBuffer.ResetFramebuffer();

            // Set the viewport to match the render target size
            SetViewports(1, nullptr, 0, 0);
        }
    }

    void DeviceContextVkImpl::UpdateBufferRegion(BufferVkImpl *pBuffVk, VulkanDynamicAllocation& Allocation, Uint64 DstOffset, Uint64 NumBytes)
    {
        VERIFY(DstOffset + NumBytes <= pBuffVk->GetDesc().uiSizeInBytes, "Update region is out of buffer");
        VERIFY_EXPR(NumBytes <= Allocation.Size);
        EnsureVkCmdBuffer();
        if(!pBuffVk->CheckAccessFlags(VK_ACCESS_TRANSFER_WRITE_BIT))
        {
            BufferMemoryBarrier(*pBuffVk, VK_ACCESS_TRANSFER_WRITE_BIT);
        }
        VkBufferCopy CopyRegion;
        CopyRegion.srcOffset = Allocation.Offset;
        CopyRegion.dstOffset = DstOffset;
        CopyRegion.size = NumBytes;
        m_CommandBuffer.CopyBuffer(Allocation.vkBuffer, pBuffVk->GetVkBuffer(), 1, &CopyRegion);
        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::UpdateBufferRegion(BufferVkImpl *pBuffVk, const void *pData, Uint64 DstOffset, Uint64 NumBytes)
    {
        VERIFY(pBuffVk->GetDesc().Usage != USAGE_DYNAMIC, "Dynamic buffers must be updated via Map()");
        VERIFY_EXPR( static_cast<size_t>(NumBytes) == NumBytes );
        auto TmpSpace = m_pDevice.RawPtr<RenderDeviceVkImpl>()->AllocateDynamicUploadSpace(m_ContextId, NumBytes, 0);
	    memcpy(TmpSpace.CPUAddress, pData, static_cast<size_t>(NumBytes));
        UpdateBufferRegion(pBuffVk, TmpSpace, DstOffset, NumBytes);
    }

    void DeviceContextVkImpl::CopyBufferRegion(BufferVkImpl *pSrcBuffVk, BufferVkImpl *pDstBuffVk, Uint64 SrcOffset, Uint64 DstOffset, Uint64 NumBytes)
    {
        VERIFY(pDstBuffVk->GetDesc().Usage != USAGE_DYNAMIC, "Dynamic buffers cannot be copy destinations");

        EnsureVkCmdBuffer();
        if(!pSrcBuffVk->CheckAccessFlags(VK_ACCESS_TRANSFER_READ_BIT))
            BufferMemoryBarrier(*pSrcBuffVk, VK_ACCESS_TRANSFER_READ_BIT);
        if(!pDstBuffVk->CheckAccessFlags(VK_ACCESS_TRANSFER_WRITE_BIT))
            BufferMemoryBarrier(*pDstBuffVk, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferCopy CopyRegion;
        CopyRegion.srcOffset = SrcOffset;
        CopyRegion.dstOffset = DstOffset;
        CopyRegion.size = NumBytes;
        //size_t DstDataStartByteOffset;
        //auto *pVkDstBuff = pDstBuffVk->GetVkBuffer(DstDataStartByteOffset, m_ContextId);
        //VERIFY(DstDataStartByteOffset == 0, "Dst buffer must not be suballocated");

        //size_t SrcDataStartByteOffset;
        //auto *pVkSrcBuff = pSrcBuffVk->GetVkBuffer(SrcDataStartByteOffset, m_ContextId);
        m_CommandBuffer.CopyBuffer(pSrcBuffVk->GetVkBuffer(), pDstBuffVk->GetVkBuffer(), 1, &CopyRegion);
        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::CopyTextureRegion(TextureVkImpl *pSrcTexture, TextureVkImpl *pDstTexture, const VkImageCopy &CopyRegion)
    {
        EnsureVkCmdBuffer();
        if(pSrcTexture->GetLayout() != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            TransitionImageLayout(*pSrcTexture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        }
        if(pDstTexture->GetLayout() != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            TransitionImageLayout(*pDstTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }
        // srcImageLayout must be VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL
        // dstImageLayout must be VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL (18.3)
        m_CommandBuffer.CopyImage(pSrcTexture->GetVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDstTexture->GetVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyRegion);
        ++m_State.NumCommands;
    }

#if 0
    void DeviceContextVkImpl::CopyTextureRegion(IBuffer *pSrcBuffer, Uint32 SrcStride, Uint32 SrcDepthStride, class TextureVkImpl *pTextureVk, Uint32 DstSubResIndex, const Box &DstBox)
    {
        auto *pBufferVk = ValidatedCast<BufferVkImpl>(pSrcBuffer);
        const auto& TexDesc = pTextureVk->GetDesc();
        VERIFY(pBufferVk->GetState() == Vk_RESOURCE_STATE_GENERIC_READ, "Staging buffer is expected to always be in Vk_RESOURCE_STATE_GENERIC_READ state");

        auto *pCmdCtx = RequestCmdContext();
        auto *pCmdList = pCmdCtx->GetCommandList();
        auto TextureState = pTextureVk->GetState();
        Vk_RESOURCE_BARRIER BarrierDesc;
		BarrierDesc.Type = Vk_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Transition.pResource = pTextureVk->GetVkResource();
		BarrierDesc.Transition.Subresource = DstSubResIndex;
		BarrierDesc.Transition.StateBefore = TextureState;
		BarrierDesc.Transition.StateAfter = Vk_RESOURCE_STATE_COPY_DEST;
        BarrierDesc.Flags = Vk_RESOURCE_BARRIER_FLAG_NONE;
        bool StateTransitionRequired = (TextureState & Vk_RESOURCE_STATE_COPY_DEST) != Vk_RESOURCE_STATE_COPY_DEST;
	    if (StateTransitionRequired)
            pCmdList->ResourceBarrier(1, &BarrierDesc);

        Vk_TEXTURE_COPY_LOCATION DstLocation;
        DstLocation.Type = Vk_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.pResource = pTextureVk->GetVkResource();
        DstLocation.SubresourceIndex = static_cast<UINT>(DstSubResIndex);

        Vk_TEXTURE_COPY_LOCATION SrcLocation;
        SrcLocation.Type = Vk_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        SrcLocation.pResource = pBufferVk->GetVkResource();
        Vk_PLACED_SUBRESOURCE_FOOTPRINT &Footpring = SrcLocation.PlacedFootprint;
        Footpring.Offset = 0;
        Footpring.Footprint.Width = static_cast<UINT>(DstBox.MaxX - DstBox.MinX);
        Footpring.Footprint.Height = static_cast<UINT>(DstBox.MaxY - DstBox.MinY);
        Footpring.Footprint.Depth = static_cast<UINT>(DstBox.MaxZ - DstBox.MinZ); // Depth cannot be 0
        Footpring.Footprint.Format = TexFormatToDXGI_Format(TexDesc.Format);

        Footpring.Footprint.RowPitch = static_cast<UINT>(SrcStride);
        VERIFY(Footpring.Footprint.RowPitch * Footpring.Footprint.Height * Footpring.Footprint.Depth <= pBufferVk->GetDesc().uiSizeInBytes, "Buffer is not large enough");
        VERIFY(SrcDepthStride == 0 || static_cast<UINT>(SrcDepthStride) == Footpring.Footprint.RowPitch * Footpring.Footprint.Height, "Depth stride must be equal to the size 2D level");

        Vk_BOX VkSrcBox;
        VkSrcBox.left    = 0;
        VkSrcBox.right   = Footpring.Footprint.Width;
        VkSrcBox.top     = 0;
        VkSrcBox.bottom  = Footpring.Footprint.Height;
        VkSrcBox.front   = 0;
        VkSrcBox.back    = Footpring.Footprint.Depth;
        pCmdCtx->GetCommandList()->CopyTextureRegion( &DstLocation, 
            static_cast<UINT>( DstBox.MinX ), 
            static_cast<UINT>( DstBox.MinY ), 
            static_cast<UINT>( DstBox.MinZ ),
            &SrcLocation, &VkSrcBox);

        ++m_State.NumCommands;

        if (StateTransitionRequired)
        {
            std::swap(BarrierDesc.Transition.StateBefore, BarrierDesc.Transition.StateAfter);
            pCmdList->ResourceBarrier(1, &BarrierDesc);
        }
    }
#endif

    void DeviceContextVkImpl::GenerateMips(TextureViewVkImpl *pTexView)
    {
#if 0
        auto *pCtx = RequestCmdContext();
        m_MipsGenerator.GenerateMips(m_pDevice.RawPtr<RenderDeviceVkImpl>(), pTexView, *pCtx);
        ++m_State.NumCommands;
#endif
    }

    void DeviceContextVkImpl::FinishCommandList(class ICommandList **ppCommandList)
    {
        auto vkCmdBuff = m_CommandBuffer.GetVkCmdBuffer();
        CommandListVkImpl *pCmdListVk( NEW_RC_OBJ(m_CmdListAllocator, "CommandListVkImpl instance", CommandListVkImpl)
                                                 (m_pDevice, this, vkCmdBuff) );
        pCmdListVk->QueryInterface( IID_CommandList, reinterpret_cast<IObject**>(ppCommandList) );
        
        m_CommandBuffer.SetVkCmdBuffer(VK_NULL_HANDLE);
        //Flush();

        m_CommandBuffer.Reset();
        m_State = ContextState{};
        m_pPipelineState.Release();

        InvalidateState();
    }

    void DeviceContextVkImpl::ExecuteCommandList(class ICommandList *pCommandList)
    {
        if (m_bIsDeferred)
        {
            LOG_ERROR("Only immediate context can execute command list");
            return;
        }

        // First execute commands in this context
        Flush();

        InvalidateState();

        CommandListVkImpl* pCmdListVk = ValidatedCast<CommandListVkImpl>(pCommandList);
        VkCommandBuffer vkCmdBuff = VK_NULL_HANDLE;
        RefCntAutoPtr<IDeviceContext> pDeferredCtx;
        pCmdListVk->Close(vkCmdBuff, pDeferredCtx);
        VERIFY(vkCmdBuff != VK_NULL_HANDLE, "Trying to execute empty command buffer");
        VERIFY_EXPR(pDeferredCtx);
        m_pDevice.RawPtr<RenderDeviceVkImpl>()->ExecuteCommandBuffer(vkCmdBuff, true);
        // It is OK to dispose command buffer from another thread. We are not going to
        // record any commands and only need to add the buffer to the queue
        pDeferredCtx.RawPtr<DeviceContextVkImpl>()->DisposeVkCmdBuffer(vkCmdBuff);
    }

    void DeviceContextVkImpl::TransitionImageLayout(ITexture *pTexture, VkImageLayout NewLayout)
    {
        VERIFY_EXPR(pTexture != nullptr);
        auto pTextureVk = ValidatedCast<TextureVkImpl>(pTexture);
        if (pTextureVk->GetLayout() != NewLayout)
        {
            TransitionImageLayout(*pTextureVk, NewLayout);
        }
    }

    void DeviceContextVkImpl::TransitionImageLayout(TextureVkImpl &TextureVk, VkImageLayout NewLayout)
    {
        VERIFY(TextureVk.GetLayout() != NewLayout, "The texture is already transitioned to correct layout");
        EnsureVkCmdBuffer();
        
        auto vkImg = TextureVk.GetVkImage();
        const auto& TexDesc = TextureVk.GetDesc();
        auto Fmt = TexDesc.Format;
        const auto& FmtAttribs = GetTextureFormatAttribs(Fmt);
        VkImageSubresourceRange SubresRange;
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH)
            SubresRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
        {
            // If image has a depth / stencil format with both depth and stencil components, then the 
            // aspectMask member of subresourceRange must include both VK_IMAGE_ASPECT_DEPTH_BIT and 
            // VK_IMAGE_ASPECT_STENCIL_BIT (6.7.3)
            SubresRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        else
            SubresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        SubresRange.baseArrayLayer = 0;
        SubresRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        SubresRange.baseMipLevel = 0;
        SubresRange.levelCount = VK_REMAINING_MIP_LEVELS;
        m_CommandBuffer.TransitionImageLayout(vkImg, TextureVk.GetLayout(), NewLayout, SubresRange);
        TextureVk.SetLayout(NewLayout);
    }

    void DeviceContextVkImpl::BufferMemoryBarrier(IBuffer *pBuffer, VkAccessFlags NewAccessFlags)
    {
        VERIFY_EXPR(pBuffer != nullptr);
        auto pBuffVk = ValidatedCast<BufferVkImpl>(pBuffer);
        if (!pBuffVk->CheckAccessFlags(NewAccessFlags))
        {
            BufferMemoryBarrier(*pBuffVk, NewAccessFlags);
        }
    }

    void DeviceContextVkImpl::BufferMemoryBarrier(BufferVkImpl &BufferVk, VkAccessFlags NewAccessFlags)
    {
        VERIFY(!BufferVk.CheckAccessFlags(NewAccessFlags), "The buffer already has requested access flags");
        EnsureVkCmdBuffer();

        auto vkBuff = BufferVk.GetVkBuffer();
        m_CommandBuffer.BufferMemoryBarrier(vkBuff, BufferVk.m_AccessFlags, NewAccessFlags);
        BufferVk.SetAccessFlags(NewAccessFlags);
    }

    void* DeviceContextVkImpl::AllocateDynamicUploadSpace(BufferVkImpl* pBuffer, size_t NumBytes, size_t Alignment)
    {
        VERIFY(m_UploadAllocations.find(pBuffer) == m_UploadAllocations.end(), "Upload space has already been allocated for this buffer");
        auto UploadAllocation = m_pDevice.RawPtr<RenderDeviceVkImpl>()->AllocateDynamicUploadSpace(m_ContextId, NumBytes, Alignment);
        auto *CPUAddress = UploadAllocation.CPUAddress;
        m_UploadAllocations.emplace(pBuffer, std::move(UploadAllocation));
        return CPUAddress;
    }
    
    void DeviceContextVkImpl::CopyAndFreeDynamicUploadData(BufferVkImpl* pBuffer)
    {
        auto it = m_UploadAllocations.find(pBuffer);
        if(it != m_UploadAllocations.end())
        {

#ifdef _DEBUG
	        auto CurrentFrame = m_pDevice.RawPtr<RenderDeviceVkImpl>()->GetCurrentFrameNumber();
            VERIFY(it->second.FrameNum == CurrentFrame, "Dynamic allocation is out-of-date. Dynamic buffer \"", pBuffer->GetDesc().Name, "\" must be unmapped in the same frame it is used.");
#endif
            UpdateBufferRegion(pBuffer, it->second, 0, pBuffer->GetDesc().uiSizeInBytes);
            m_UploadAllocations.erase(it);
        }
        else
        {
            UNEXPECTED("Unable to find dynamic allocation for this buffer");
        }
    }
}
