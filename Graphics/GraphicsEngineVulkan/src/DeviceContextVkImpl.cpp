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
#include <sstream>
#include "RenderDeviceVkImpl.h"
#include "DeviceContextVkImpl.h"
#include "SwapChainVk.h"
#include "PipelineStateVkImpl.h"
#include "TextureVkImpl.h"
#include "BufferVkImpl.h"
#include "VulkanTypeConversions.h"
#include "CommandListVkImpl.h"

namespace Diligent
{
    static std::string GetUploadHeapName(bool bIsDeferred, Uint32 ContextId)
    {
        if (bIsDeferred)
        {
            std::stringstream ss;
            ss <<  "Upload heap of deferred context #" << ContextId;
            return ss.str();
        }
        else
            return "Upload heap of immediate context";
    }

    static std::string GetDynamicHeapName(bool bIsDeferred, Uint32 ContextId)
    {
        if (bIsDeferred)
        {
            std::stringstream ss;
            ss << "Dynamic heap of deferred context #" << ContextId;
            return ss.str();
        }
        else
            return "Dynamic heap of immediate context";
    }

    DeviceContextVkImpl::DeviceContextVkImpl( IReferenceCounters*                   pRefCounters, 
                                              RenderDeviceVkImpl*                   pDeviceVkImpl, 
                                              bool                                  bIsDeferred, 
                                              const EngineVkAttribs&                Attribs, 
                                              Uint32                                ContextId,
                                              std::shared_ptr<GenerateMipsVkHelper> GenerateMipsHelper) :
        TDeviceContextBase{pRefCounters, pDeviceVkImpl, bIsDeferred},
        m_NumCommandsToFlush{bIsDeferred ? std::numeric_limits<decltype(m_NumCommandsToFlush)>::max() : Attribs.NumCommandsToFlushCmdBuffer},
        m_CmdListAllocator{ GetRawAllocator(), sizeof(CommandListVkImpl), 64 },
        m_ContextId{ContextId},
        // Command pools for deferred contexts must be thread safe because finished command buffers are executed and released from another thread
        m_CmdPool
        {
            pDeviceVkImpl->GetLogicalDevice().GetSharedPtr(),
            pDeviceVkImpl->GetCmdQueue()->GetQueueFamilyIndex(),
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 
            bIsDeferred
        },
        // Upload heap must always be thread-safe as Finish() may be called from another thread
        m_UploadHeap
        {
            *pDeviceVkImpl,
            GetUploadHeapName(bIsDeferred, ContextId),
            Attribs.UploadHeapPageSize
        },
        // Descriptor pools must always be thread-safe as for a deferred context, Finish() may be called from another thread
        m_DynamicDescriptorPool
        {
            pDeviceVkImpl->GetLogicalDevice().GetSharedPtr(),
            std::vector<VkDescriptorPoolSize>
            {
                {VK_DESCRIPTOR_TYPE_SAMPLER,                Attribs.DynamicDescriptorPoolSize.NumSeparateSamplerDescriptors},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Attribs.DynamicDescriptorPoolSize.NumCombinedSamplerDescriptors},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          Attribs.DynamicDescriptorPoolSize.NumSampledImageDescriptors},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          Attribs.DynamicDescriptorPoolSize.NumStorageImageDescriptors},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   Attribs.DynamicDescriptorPoolSize.NumUniformTexelBufferDescriptors},
                {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   Attribs.DynamicDescriptorPoolSize.NumStorageTexelBufferDescriptors},
                //{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         Attribs.DynamicDescriptorPoolSize.NumUniformBufferDescriptors},
                //{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         Attribs.DynamicDescriptorPoolSize.NumStorageBufferDescriptors},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Attribs.DynamicDescriptorPoolSize.NumUniformBufferDescriptors},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, Attribs.DynamicDescriptorPoolSize.NumStorageBufferDescriptors},
            },
            Attribs.DynamicDescriptorPoolSize.MaxDescriptorSets,
        },
        m_NextCmdBuffNumber(0),
        m_ContextFrameNumber(0),
        m_DynamicHeap
        {
            pDeviceVkImpl->GetDynamicMemoryManager(),
            GetDynamicHeapName(bIsDeferred, ContextId),
            Attribs.DynamicHeapPageSize
        },
        m_GenerateMipsHelper(std::move(GenerateMipsHelper))
    {
        m_GenerateMipsHelper->CreateSRB(&m_GenerateMipsSRB);

        BufferDesc DummyVBDesc;
        DummyVBDesc.Name          = "Dummy vertex buffer";
        DummyVBDesc.BindFlags     = BIND_VERTEX_BUFFER;
        DummyVBDesc.Usage         = USAGE_DEFAULT;
        DummyVBDesc.uiSizeInBytes = 32;
        m_pDevice->CreateBuffer(DummyVBDesc, BufferData{}, &m_DummyVB);
    }

    DeviceContextVkImpl::~DeviceContextVkImpl()
    {
        auto* pDeviceVkImpl = m_pDevice.RawPtr<RenderDeviceVkImpl>();
        if (m_State.NumCommands != 0)
            LOG_ERROR_MESSAGE(m_bIsDeferred ? 
                                "There are outstanding commands in the deferred context being destroyed, which indicates that FinishCommandList() has not been called." :
                                "There are outstanding commands in the immediate context being destroyed, which indicates the context has not been Flush()'ed.",
                              " This is unexpected and may result in synchronization errors");

        if (!m_bIsDeferred)
        {
            // There should be no outstanding commands, but we need to call Flush to discard all stale
            // context resources to actually destroy them in the next call to ReleaseStaleContextResources()
            Flush();
        }

        // We must now wait for GPU to finish so that we can safely destroy all context resources.
        // We need to idle when destroying deferred contexts as well since some resources may still be in use.
        pDeviceVkImpl->IdleGPU(true);

        DisposeCurrentCmdBuffer(m_LastSubmittedFenceValue);

        // There must be no resources in the stale resource list. For immediate context, all stale resources must have been
        // moved to the release queue by Flush(). For deferred contexts, this should have happened in the last FinishCommandList()
        // call.
        VERIFY(m_UploadHeap.GetStaleAllocationsCount() == 0, "All stale allocations must have been discarded at this point");
        VERIFY(m_DynamicDescriptorPool.GetStaleAllocationCount() == 0, "All stale dynamic descriptor set allocations must have been discarded at this point");
        ReleaseStaleContextResources(m_NextCmdBuffNumber, m_LastSubmittedFenceValue, pDeviceVkImpl->GetCompletedFenceValue());
        // Since we idled the GPU, all stale resources must have been destroyed now
        VERIFY(m_DynamicDescriptorPool.GetPendingReleaseAllocationCount() == 0, "All stale descriptor set allocations must have been destroyed at this point");

        auto VkCmdPool = m_CmdPool.Release();
        pDeviceVkImpl->SafeReleaseVkObject(std::move(VkCmdPool));
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

    void DeviceContextVkImpl::DisposeVkCmdBuffer(VkCommandBuffer vkCmdBuff, Uint64 FenceValue)
    {
        VERIFY_EXPR(vkCmdBuff != VK_NULL_HANDLE);
        m_CmdPool.DisposeCommandBuffer(vkCmdBuff, FenceValue);
    }

    inline void DeviceContextVkImpl::DisposeCurrentCmdBuffer(Uint64 FenceValue)
    {
        VERIFY(m_CommandBuffer.GetState().RenderPass == VK_NULL_HANDLE, "Disposing command buffer with unifinished render pass");
        auto vkCmdBuff = m_CommandBuffer.GetVkCmdBuffer();
        if (vkCmdBuff != VK_NULL_HANDLE)
        {
            DisposeVkCmdBuffer(vkCmdBuff, FenceValue);
            m_CommandBuffer.Reset();
        }
    }


    void DeviceContextVkImpl::SetPipelineState(IPipelineState *pPipelineState)
    {
        // Never flush deferred context!
        if (!m_bIsDeferred && m_State.NumCommands >= m_NumCommandsToFlush)
        {
            Flush();
        }

        auto* pPipelineStateVk = ValidatedCast<PipelineStateVkImpl>(pPipelineState);
        const auto& PSODesc = pPipelineStateVk->GetDesc();

        bool CommitStates = false;
        bool CommitScissor = false;
        if (!m_pPipelineState)
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

        TDeviceContextBase::SetPipelineState( pPipelineStateVk, 0 /*Dummy*/ );
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

            if (CommitStates)
            {
                m_CommandBuffer.SetStencilReference(m_StencilRef);
                m_CommandBuffer.SetBlendConstants(m_BlendFactors);
                CommitRenderPassAndFramebuffer();
                CommitViewports();
            }

            if (PSODesc.GraphicsPipeline.RasterizerDesc.ScissorEnable && (CommitStates || CommitScissor))
            {
                CommitScissorRects();
            }
        }

        m_DescrSetBindInfo.Reset();
    }

    void DeviceContextVkImpl::TransitionShaderResources(IPipelineState *pPipelineState, IShaderResourceBinding *pShaderResourceBinding)
    {
        VERIFY_EXPR(pPipelineState != nullptr);

        auto *pPipelineStateVk = ValidatedCast<PipelineStateVkImpl>(pPipelineState);
        pPipelineStateVk->CommitAndTransitionShaderResources(pShaderResourceBinding, this, false, COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES, nullptr);
    }

    void DeviceContextVkImpl::CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags)
    {
        if (!DeviceContextBase::CommitShaderResources(pShaderResourceBinding, Flags, 0 /*Dummy*/))
            return;

        m_pPipelineState->CommitAndTransitionShaderResources(pShaderResourceBinding, this, true, Flags, &m_DescrSetBindInfo);
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

    void DeviceContextVkImpl::TransitionVkVertexBuffers()
    {
        for ( UINT Buff = 0; Buff < m_NumVertexStreams; ++Buff )
        {
            auto& CurrStream = m_VertexStreams[Buff];
            auto* pBufferVk = CurrStream.pBuffer.RawPtr();
            if (pBufferVk != nullptr && !pBufferVk->CheckAccessFlags(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT))
                BufferMemoryBarrier(*pBufferVk, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
        }
    }

    void DeviceContextVkImpl::CommitVkVertexBuffers()
    {
#ifdef DEVELOPMENT
        if (m_NumVertexStreams < m_pPipelineState->GetNumBufferSlotsUsed())
            LOG_ERROR("Currently bound pipeline state \"", m_pPipelineState->GetDesc().Name, "\" expects ", m_pPipelineState->GetNumBufferSlotsUsed(), " input buffer slots, but only ", m_NumVertexStreams, " is bound");
#endif
        // Do not initialize array with zeroes for performance reasons
        VkBuffer vkVertexBuffers[MaxBufferSlots];// = {}
        VkDeviceSize Offsets[MaxBufferSlots];
        VERIFY( m_NumVertexStreams <= MaxBufferSlots, "Too many buffers are being set" );
        bool DynamicBufferPresent = false;
        for ( UINT slot = 0; slot < m_NumVertexStreams; ++slot )
        {
            auto& CurrStream = m_VertexStreams[slot];
            if (auto* pBufferVk = CurrStream.pBuffer.RawPtr())
            {
                if (pBufferVk->GetDesc().Usage == USAGE_DYNAMIC)
                {
                    DynamicBufferPresent = true;
#ifdef DEVELOPMENT
                    pBufferVk->DvpVerifyDynamicAllocation(this);
#endif
                }
                if (!pBufferVk->CheckAccessFlags(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT))
                    BufferMemoryBarrier(*pBufferVk, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
            
                // Device context keeps strong references to all vertex buffers.

                vkVertexBuffers[slot] = pBufferVk->GetVkBuffer();
                Offsets[slot] = CurrStream.Offset + pBufferVk->GetDynamicOffset(m_ContextId, this);
            }
            else
            {
                // We can't bind null vertex buffer in Vulkan and have to use a dummy one
                vkVertexBuffers[slot] = m_DummyVB.RawPtr<BufferVkImpl>()->GetVkBuffer();
                Offsets[slot] = 0;
            }
        }

        //GraphCtx.FlushResourceBarriers();
        if (m_NumVertexStreams > 0)
            m_CommandBuffer.BindVertexBuffers( 0, m_NumVertexStreams, vkVertexBuffers, Offsets );

        // GPU offset for a dynamic vertex buffer can change every time a draw command is invoked
        m_State.CommittedVBsUpToDate = !DynamicBufferPresent;
    }

    void DeviceContextVkImpl::DvpLogRenderPass_PSOMismatch()
    {
        std::stringstream ss;
        ss << "Active render pass is incomaptible with PSO '" << m_pPipelineState->GetDesc().Name << "'. "
                "This indicates the mismatch between the number and/or format of bound render targets and/or depth stencil buffer "
                "and the PSO. Vulkand requires exact match.\n"
                "    Bound render targets (" << m_NumBoundRenderTargets << "):";
        Uint32 SampleCount = 0;
        for (Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt)
        {
            ss << ' ';
            if (auto* pRTV = m_pBoundRenderTargets[rt].RawPtr())
            {
                VERIFY_EXPR(SampleCount == 0 || SampleCount == pRTV->GetTexture()->GetDesc().SampleCount);
                SampleCount = pRTV->GetTexture()->GetDesc().SampleCount;
                ss << GetTextureFormatAttribs(pRTV->GetDesc().Format).Name;
            }
            else
                ss << "<Not set>";
        }
        ss << "; DSV: ";
        if (m_pBoundDepthStencil)
        {
            VERIFY_EXPR(SampleCount == 0 || SampleCount == m_pBoundDepthStencil->GetTexture()->GetDesc().SampleCount);
            SampleCount = m_pBoundDepthStencil->GetTexture()->GetDesc().SampleCount;
            ss << GetTextureFormatAttribs(m_pBoundDepthStencil->GetDesc().Format).Name;
        }
        else
            ss << "<Not set>";
        ss << "; Sample count: " << SampleCount;

        const auto& GrPipeline = m_pPipelineState->GetDesc().GraphicsPipeline;
        ss << "\n    PSO: render targets (" << Uint32{GrPipeline.NumRenderTargets} << "): ";
        for (Uint32 rt = 0; rt < GrPipeline.NumRenderTargets; ++rt)
            ss << ' ' << GetTextureFormatAttribs(GrPipeline.RTVFormats[rt]).Name;
        ss << "; DSV: " << GetTextureFormatAttribs(GrPipeline.DSVFormat).Name;
        ss << "; Sample count: " << Uint32{GrPipeline.SmplDesc.Count};

        LOG_ERROR_MESSAGE(ss.str());
    }

    void DeviceContextVkImpl::Draw( DrawAttribs &drawAttribs )
    {
#ifdef DEVELOPMENT
        if (!DvpVerifyDrawArguments(drawAttribs))
            return;
#endif

        EnsureVkCmdBuffer();

        if ( drawAttribs.IsIndexed )
        {
#ifdef DEVELOPMENT
            if (m_pIndexBuffer == nullptr)
            {
                LOG_ERROR("Index buffer is not set up for indexed draw command");
                return;
            }
#endif

            BufferVkImpl *pBuffVk = m_pIndexBuffer.RawPtr<BufferVkImpl>();
            if (!pBuffVk->CheckAccessFlags(VK_ACCESS_INDEX_READ_BIT))
                BufferMemoryBarrier(*pBuffVk, VK_ACCESS_INDEX_READ_BIT);

            DEV_CHECK_ERR(drawAttribs.IndexType == VT_UINT16 || drawAttribs.IndexType == VT_UINT32, "Unsupported index format. Only R16_UINT and R32_UINT are allowed.");
            VkIndexType vkIndexType = drawAttribs.IndexType == VT_UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
            m_CommandBuffer.BindIndexBuffer(pBuffVk->GetVkBuffer(), m_IndexDataStartOffset + pBuffVk->GetDynamicOffset(m_ContextId, this), vkIndexType);
        }

        if (m_State.CommittedVBsUpToDate)
            TransitionVkVertexBuffers();
        else
            CommitVkVertexBuffers();

        if (m_DescrSetBindInfo.DynamicOffsetCount != 0)
            m_pPipelineState->BindDescriptorSetsWithDynamicOffsets(this, m_DescrSetBindInfo);
#if 0
#ifdef _DEBUG
        else
        {
            if ( m_pPipelineState->dbgContainsShaderResources() )
                LOG_ERROR_MESSAGE("Pipeline state \"", m_pPipelineState->GetDesc().Name, "\" contains shader resources, but IDeviceContext::CommitShaderResources() was not called" );
        }
#endif
#endif

        auto* pIndirectDrawAttribsVk = ValidatedCast<BufferVkImpl>(drawAttribs.pIndirectDrawAttribs);
        if (pIndirectDrawAttribsVk != nullptr)
        {
            // Buffer memory barries must be executed outside of render pass
            if (!pIndirectDrawAttribsVk->CheckAccessFlags(VK_ACCESS_INDIRECT_COMMAND_READ_BIT))
                BufferMemoryBarrier(*pIndirectDrawAttribsVk, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
        }

#ifdef DEVELOPMENT
        if (m_pPipelineState->GetVkRenderPass() != m_RenderPass)
        {
            DvpLogRenderPass_PSOMismatch();
        }
#endif

        CommitRenderPassAndFramebuffer();

        if (pIndirectDrawAttribsVk != nullptr)
        {
#ifdef DEVELOPMENT
            if (pIndirectDrawAttribsVk->GetDesc().Usage == USAGE_DYNAMIC)
                pIndirectDrawAttribsVk->DvpVerifyDynamicAllocation(this);
#endif
            if (!pIndirectDrawAttribsVk->CheckAccessFlags(VK_ACCESS_INDIRECT_COMMAND_READ_BIT))
                BufferMemoryBarrier(*pIndirectDrawAttribsVk, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

            if ( drawAttribs.IsIndexed )
                m_CommandBuffer.DrawIndexedIndirect(pIndirectDrawAttribsVk->GetVkBuffer(), pIndirectDrawAttribsVk->GetDynamicOffset(m_ContextId, this) + drawAttribs.IndirectDrawArgsOffset, 1, 0);
            else
                m_CommandBuffer.DrawIndirect(pIndirectDrawAttribsVk->GetVkBuffer(), pIndirectDrawAttribsVk->GetDynamicOffset(m_ContextId, this) + drawAttribs.IndirectDrawArgsOffset, 1, 0);
        }
        else
        {
            if ( drawAttribs.IsIndexed )
                m_CommandBuffer.DrawIndexed(drawAttribs.NumIndices, drawAttribs.NumInstances, drawAttribs.FirstIndexLocation, drawAttribs.BaseVertex, drawAttribs.FirstInstanceLocation);
            else
                m_CommandBuffer.Draw(drawAttribs.NumVertices, drawAttribs.NumInstances, drawAttribs.StartVertexLocation, drawAttribs.FirstInstanceLocation );
        }

        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )
    {
#ifdef DEVELOPMENT
        if (!DvpVerifyDispatchArguments(DispatchAttrs))
            return;
#endif

        EnsureVkCmdBuffer();

        // Dispatch commands must be executed outside of render pass
        if (m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
            m_CommandBuffer.EndRenderPass();

        if (m_DescrSetBindInfo.DynamicOffsetCount != 0)
            m_pPipelineState->BindDescriptorSetsWithDynamicOffsets(this, m_DescrSetBindInfo);
#if 0
#ifdef _DEBUG
        else
        {
            if ( m_pPipelineState->dbgContainsShaderResources() )
                LOG_ERROR_MESSAGE("Pipeline state \"", m_pPipelineState->GetDesc().Name, "\" contains shader resources, but IDeviceContext::CommitShaderResources() was not called" );
        }
#endif
#endif

        if ( DispatchAttrs.pIndirectDispatchAttribs )
        {
            if ( auto *pBufferVk = ValidatedCast<BufferVkImpl>(DispatchAttrs.pIndirectDispatchAttribs) )
            {
#ifdef DEVELOPMENT
                if (pBufferVk->GetDesc().Usage == USAGE_DYNAMIC)
                    pBufferVk->DvpVerifyDynamicAllocation(this);
#endif

                // Buffer memory barries must be executed outside of render pass
                if (!pBufferVk->CheckAccessFlags(VK_ACCESS_INDIRECT_COMMAND_READ_BIT))
                    BufferMemoryBarrier(*pBufferVk, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

                m_CommandBuffer.DispatchIndirect(pBufferVk->GetVkBuffer(), pBufferVk->GetDynamicOffset(m_ContextId, this) + DispatchAttrs.DispatchArgsByteOffset);
            }
            else
            {
                LOG_ERROR_MESSAGE("Valid pIndirectDrawAttribs must be provided for indirect dispatch command");
            }
        }
        else
            m_CommandBuffer.Dispatch(DispatchAttrs.ThreadGroupCountX, DispatchAttrs.ThreadGroupCountY, DispatchAttrs.ThreadGroupCountZ);

        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::ClearDepthStencil( ITextureView* pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil )
    {
        ITextureViewVk* pVkDSV = nullptr;
        if ( pView != nullptr )
        {
            pVkDSV = ValidatedCast<ITextureViewVk>(pView);
#ifdef DEVELOPMENT
            const auto& ViewDesc = pVkDSV->GetDesc();
            if ( ViewDesc.ViewType != TEXTURE_VIEW_DEPTH_STENCIL)
            {
                LOG_ERROR("The type (", GetTexViewTypeLiteralName(ViewDesc.ViewType), ") of texture view '", pView->GetDesc().Name, "' is incorrect for ClearDepthStencil operation. Depth-stencil view (TEXTURE_VIEW_DEPTH_STENCIL) must be provided." );
                return;
            }
#endif
        }
        else
        {
            if (m_pSwapChain)
            {
                pVkDSV = ValidatedCast<ITextureViewVk>(m_pSwapChain->GetDepthBufferDSV());
            }
            else
            {
                LOG_ERROR("Failed to clear default depth stencil buffer: swap chain is not initialized in the device context");
                return;
            }
        }

        EnsureVkCmdBuffer();

        const auto& ViewDesc = pVkDSV->GetDesc();
        VERIFY(ViewDesc.TextureDim != RESOURCE_DIM_TEX_3D, "Depth-stencil view of a 3D texture should've been created as 2D texture array view");

        if (pVkDSV == m_pBoundDepthStencil)
        {
            // Render pass may not be currently committed
            VERIFY_EXPR(m_RenderPass != VK_NULL_HANDLE && m_Framebuffer != VK_NULL_HANDLE);
            CommitRenderPassAndFramebuffer();

            VkClearAttachment ClearAttachment = {};
            ClearAttachment.aspectMask = 0;
            if (ClearFlags & CLEAR_DEPTH_FLAG)   ClearAttachment.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
            if (ClearFlags & CLEAR_STENCIL_FLAG) ClearAttachment.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            // colorAttachment is only meaningful if VK_IMAGE_ASPECT_COLOR_BIT is set in aspectMask
            ClearAttachment.colorAttachment = VK_ATTACHMENT_UNUSED;
            ClearAttachment.clearValue.depthStencil.depth = fDepth;
            ClearAttachment.clearValue.depthStencil.stencil = Stencil;
            VkClearRect ClearRect;
            // m_FramebufferWidth, m_FramebufferHeight are scaled to the proper mip level
            ClearRect.rect = { {0, 0}, {m_FramebufferWidth, m_FramebufferHeight} };
            // The layers [baseArrayLayer, baseArrayLayer + layerCount) count from the base layer of 
            // the attachment image view (17.2), so baseArrayLayer is 0, not ViewDesc.FirstArraySlice
            ClearRect.baseArrayLayer = 0;
            ClearRect.layerCount     = ViewDesc.NumArraySlices;
            // No memory barriers are needed between vkCmdClearAttachments and preceding or 
            // subsequent draw or attachment clear commands in the same subpass (17.2)
            m_CommandBuffer.ClearAttachment(ClearAttachment, ClearRect);
        }
        else
        {
            // End render pass to clear the buffer with vkCmdClearDepthStencilImage
            if (m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
                m_CommandBuffer.EndRenderPass();

            auto* pTexture = pVkDSV->GetTexture();
            auto* pTextureVk = ValidatedCast<TextureVkImpl>(pTexture);

            // Image layout must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (17.1)
            TransitionImageLayout(pTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkClearDepthStencilValue ClearValue;
            ClearValue.depth = fDepth;
            ClearValue.stencil = Stencil;
            VkImageSubresourceRange Subresource;
            Subresource.aspectMask = 0;
            if (ClearFlags & CLEAR_DEPTH_FLAG)   Subresource.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
            if (ClearFlags & CLEAR_STENCIL_FLAG) Subresource.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            // We are clearing the image, not image view with vkCmdClearDepthStencilImage
            Subresource.baseArrayLayer = ViewDesc.FirstArraySlice;
            Subresource.layerCount     = ViewDesc.NumArraySlices;
            Subresource.baseMipLevel   = ViewDesc.MostDetailedMip;
            Subresource.levelCount     = ViewDesc.NumMipLevels;

            m_CommandBuffer.ClearDepthStencilImage(pTextureVk->GetVkImage(), ClearValue, Subresource);
        }

        ++m_State.NumCommands;
    }

    VkClearColorValue ClearValueToVkClearValue(const float *RGBA, TEXTURE_FORMAT TexFmt)
    {
        VkClearColorValue ClearValue;
        const auto& FmtAttribs = GetTextureFormatAttribs(TexFmt);
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_SINT)
        {
            for (int i=0; i < 4; ++i)
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
        ITextureViewVk* pVkRTV = nullptr;
        if ( pView != nullptr )
        {
#ifdef DEVELOPMENT
            const auto& ViewDesc = pView->GetDesc();
            if ( ViewDesc.ViewType != TEXTURE_VIEW_RENDER_TARGET)
            {
                LOG_ERROR("The type (", GetTexViewTypeLiteralName(ViewDesc.ViewType), ") of texture view '", pView->GetDesc().Name, "' is incorrect for ClearRenderTarget operation. Render target view (TEXTURE_VIEW_RENDER_TARGET) must be provided." );
                return;
            }
#endif
            pVkRTV = ValidatedCast<ITextureViewVk>(pView);
        }
        else
        {
            if (m_pSwapChain)
            {
                pVkRTV = ValidatedCast<ITextureViewVk>(m_pSwapChain->GetCurrentBackBufferRTV());
            }
            else
            {
                LOG_ERROR("Failed to clear default render target: swap chain is not initialized in the device context");
                return;
            }
        }

        static constexpr float Zero[4] = { 0.f, 0.f, 0.f, 0.f };
        if ( RGBA == nullptr )
            RGBA = Zero;
        
        EnsureVkCmdBuffer();

        const auto& ViewDesc = pVkRTV->GetDesc();
        VERIFY(ViewDesc.TextureDim != RESOURCE_DIM_TEX_3D, "Render target view of a 3D texture should've been created as 2D texture array view");

        // Check if the texture is one of the currently bound render targets
        static constexpr const Uint32 InvalidAttachmentIndex = static_cast<Uint32>(-1);
        Uint32 attachmentIndex = InvalidAttachmentIndex;
        for (Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt)
        {
            if (m_pBoundRenderTargets[rt] == pVkRTV)
            {
                attachmentIndex = rt;
                break;
            }
        }
        
        if (attachmentIndex != InvalidAttachmentIndex)
        {
            // Render pass may not be currently committed
            VERIFY_EXPR(m_RenderPass != VK_NULL_HANDLE && m_Framebuffer != VK_NULL_HANDLE);
            CommitRenderPassAndFramebuffer();

            VkClearAttachment ClearAttachment = {};
            ClearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            // colorAttachment is only meaningful if VK_IMAGE_ASPECT_COLOR_BIT is set in aspectMask, 
            // in which case it is an index to the pColorAttachments array in the VkSubpassDescription 
            // structure of the current subpass which selects the color attachment to clear (17.2)
            // It is NOT the render pass attachment index
            ClearAttachment.colorAttachment = attachmentIndex;
            ClearAttachment.clearValue.color = ClearValueToVkClearValue(RGBA, ViewDesc.Format);
            VkClearRect ClearRect;
            // m_FramebufferWidth, m_FramebufferHeight are scaled to the proper mip level
            ClearRect.rect = { {0, 0}, {m_FramebufferWidth, m_FramebufferHeight} };
            // The layers [baseArrayLayer, baseArrayLayer + layerCount) count from the base layer of 
            // the attachment image view (17.2), so baseArrayLayer is 0, not ViewDesc.FirstArraySlice
            ClearRect.baseArrayLayer = 0;
            ClearRect.layerCount     = ViewDesc.NumArraySlices; 
            // No memory barriers are needed between vkCmdClearAttachments and preceding or 
            // subsequent draw or attachment clear commands in the same subpass (17.2)
            m_CommandBuffer.ClearAttachment(ClearAttachment, ClearRect);
        }
        else
        {
            // End current render pass and clear the image with vkCmdClearColorImage
            if (m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
                m_CommandBuffer.EndRenderPass();

            auto* pTexture = pVkRTV->GetTexture();
            auto* pTextureVk = ValidatedCast<TextureVkImpl>(pTexture);

            // Image layout must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (17.1)
            TransitionImageLayout(pTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            auto ClearValue = ClearValueToVkClearValue(RGBA, ViewDesc.Format);
            VkImageSubresourceRange Subresource;
            Subresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            // We are clearing the image, not image view with vkCmdClearColorImage
            Subresource.baseArrayLayer = ViewDesc.FirstArraySlice;
            Subresource.layerCount     = ViewDesc.NumArraySlices;
            Subresource.baseMipLevel   = ViewDesc.MostDetailedMip;
            Subresource.levelCount     = ViewDesc.NumMipLevels;
            VERIFY(ViewDesc.NumMipLevels, "RTV must contain single mip level");

            m_CommandBuffer.ClearColorImage(pTextureVk->GetVkImage(), ClearValue, Subresource);
        }

        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::FinishFrame(bool ForceRelease)
    {
        FinishFrame(ForceRelease ? std::numeric_limits<Uint64>::max() : m_pDevice.RawPtr<RenderDeviceVkImpl>()->GetCompletedFenceValue());
    }

    void DeviceContextVkImpl::FinishFrame(Uint64 CompletedFenceValue)
    {
        if(GetNumCommandsInCtx() != 0)
            LOG_ERROR_MESSAGE(m_bIsDeferred ? 
                "There are outstanding commands in the deferred device context when finishing the frame. This is an error and may cause unpredicted behaviour. Close all deferred contexts and execute them before finishing the frame" :
                "There are outstanding commands in the immediate device context when finishing the frame. This is an error and may cause unpredicted behaviour. Call Flush() to submit all commands for execution before finishing the frame");

        m_UploadHeap.DiscardAllocations(m_LastSubmittedFenceValue);
        m_DynamicDescriptorPool.ReleaseStaleAllocations(CompletedFenceValue);
        m_DynamicHeap.FinishFrame(m_LastSubmittedFenceValue);
        Atomics::AtomicIncrement(m_ContextFrameNumber);
    }

    void DeviceContextVkImpl::ReleaseStaleContextResources(Uint64 SubmittedCmdBufferNumber, Uint64 SubmittedFenceValue, Uint64 CompletedFenceValue)
    {
        m_DynamicDescriptorPool.DisposeAllocations(SubmittedFenceValue);
        m_DynamicDescriptorPool.ReleaseStaleAllocations(CompletedFenceValue);
    }

    void DeviceContextVkImpl::Flush()
    {
#ifdef DEVELOPMENT
        if (m_bIsDeferred)
        {
            LOG_ERROR("Flush() should only be called for immediate contexts");
            return;
        }
#endif

        VkSubmitInfo SubmitInfo = {};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfo.pNext = nullptr;

        auto pDeviceVkImpl = m_pDevice.RawPtr<RenderDeviceVkImpl>();
        auto vkCmdBuff = m_CommandBuffer.GetVkCmdBuffer();
        if (vkCmdBuff != VK_NULL_HANDLE )
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
        //if (SubmitInfo.commandBufferCount != 0 || SubmitInfo.waitSemaphoreCount !=0 || SubmitInfo.signalSemaphoreCount != 0)
        m_LastSubmittedFenceValue = pDeviceVkImpl->ExecuteCommandBuffer(SubmitInfo, this, &m_PendingFences);
        
        m_WaitSemaphores.clear();
        m_WaitDstStageMasks.clear();
        m_SignalSemaphores.clear();
        m_PendingFences.clear();

        if (vkCmdBuff != VK_NULL_HANDLE)
        {
            DisposeCurrentCmdBuffer(m_LastSubmittedFenceValue);
        }

        // Release temporary resources that were used by this context while recording the last command buffer
        auto SubmittedCmdBuffNumber = m_NextCmdBuffNumber;
        Atomics::AtomicIncrement(m_NextCmdBuffNumber);
        auto CompletedFenceValue = pDeviceVkImpl->GetCompletedFenceValue();
        ReleaseStaleContextResources(SubmittedCmdBuffNumber, m_LastSubmittedFenceValue, CompletedFenceValue);

        m_State = ContextState{};
        m_DescrSetBindInfo.Reset();
        m_CommandBuffer.Reset();
        m_pPipelineState = nullptr;
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
        m_RenderPass = VK_NULL_HANDLE;
        m_Framebuffer = VK_NULL_HANDLE;
        m_DescrSetBindInfo.Reset();
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
        for ( Uint32 vp = 0; vp < m_NumViewports; ++vp )
        {
            VkViewports[vp].x        = m_Viewports[vp].TopLeftX;
            VkViewports[vp].y        = m_Viewports[vp].TopLeftY;
            VkViewports[vp].width    = m_Viewports[vp].Width;
            VkViewports[vp].height   = m_Viewports[vp].Height;
            VkViewports[vp].minDepth = m_Viewports[vp].MinDepth;
            VkViewports[vp].maxDepth = m_Viewports[vp].MaxDepth;

            // Turn the viewport upside down to be consistent with Direct3D. Note that in both APIs,
            // the viewport covers the same texture rows. The difference is that Direct3D invertes 
            // normalized device Y coordinate when transforming NDC to window coordinates. In Vulkan
            // we achieve the same effect by using negative viewport height. Therefore we need to
            // invert normalized device Y coordinate when transforming to texture V
            // 
            //             
            //       Image                Direct3D                                       Image               Vulkan
            //        row                                                                 row   
            //         0 _   (0,0)_______________________(1,0)                  Tex Height _   (0,1)_______________________(1,1)              
            //         1 _       |                       |      |             VP Top + Hght _ _ _ _|   __________          |      A           
            //         2 _       |                       |      |                          .       |  |   .--> +x|         |      |           
            //           .       |                       |      |                          .       |  |   |      |         |      |           
            //           .       |                       |      | V Coord                          |  |   V +y   |         |      | V Coord   
            //     VP Top _ _ _ _|   __________          |      |                    VP Top _ _ _ _|  |__________|         |      |           
            //           .       |  |    A +y  |         |      |                          .       |                       |      |           
            //           .       |  |    |     |         |      |                          .       |                       |      |           
            //           .       |  |    '-->+x|         |      |                        2 _       |                       |      |           
            //           .       |  |__________|         |      |                        1 _       |                       |      |           
            //Tex Height _       |_______________________|      V                        0 _       |_______________________|      |           
            //               (0,1)                       (1,1)                                 (0,0)                       (1,0)              
            //                                                                                
            //

            VkViewports[vp].y = VkViewports[vp].y + VkViewports[vp].height;
            VkViewports[vp].height = -VkViewports[vp].height;
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
        if ( m_pPipelineState )
        {
            const auto &PSODesc = m_pPipelineState->GetDesc();
            if (!PSODesc.IsComputePipeline && PSODesc.GraphicsPipeline.RasterizerDesc.ScissorEnable)
            {
                VERIFY(NumRects == m_NumScissorRects, "Unexpected number of scissor rects");
                CommitScissorRects();
            }
        }
    }


    void DeviceContextVkImpl::CommitRenderPassAndFramebuffer()
    {
        const auto& CmdBufferState = m_CommandBuffer.GetState();
        if (CmdBufferState.Framebuffer != m_Framebuffer)
        {
            if (CmdBufferState.RenderPass != VK_NULL_HANDLE)
                m_CommandBuffer.EndRenderPass();
        
            if (m_Framebuffer != VK_NULL_HANDLE)
            {
                VERIFY_EXPR(m_RenderPass != VK_NULL_HANDLE);
                if (m_pBoundDepthStencil)
                {
                    auto* pDSVVk = m_pBoundDepthStencil.RawPtr<TextureViewVkImpl>();
                    auto* pDepthBuffer = pDSVVk->GetTexture();
                    TransitionImageLayout(pDepthBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                }

                for (Uint32 rt=0; rt < m_NumBoundRenderTargets; ++rt)
                {
                    if (ITextureView* pRTV = m_pBoundRenderTargets[rt])
                    {
                        auto* pRTVVk = ValidatedCast<TextureViewVkImpl>(pRTV);
                        auto* pRenderTarget = pRTVVk->GetTexture();
                        TransitionImageLayout(pRenderTarget, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                    }
                }
                m_CommandBuffer.BeginRenderPass(m_RenderPass, m_Framebuffer, m_FramebufferWidth, m_FramebufferHeight);
            }
        }
    }

    void DeviceContextVkImpl::SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )
    {
        if ( TDeviceContextBase::SetRenderTargets( NumRenderTargets, ppRenderTargets, pDepthStencil ) )
        {
            FramebufferCache::FramebufferCacheKey FBKey;
            RenderPassCache::RenderPassCacheKey RenderPassKey;
            if (m_pBoundDepthStencil)
            {
                auto* pDSVVk = m_pBoundDepthStencil.RawPtr<TextureViewVkImpl>();
                auto* pDepthBuffer = pDSVVk->GetTexture();
                FBKey.DSV = pDSVVk->GetVulkanImageView();
                RenderPassKey.DSVFormat = pDSVVk->GetDesc().Format;
                RenderPassKey.SampleCount = static_cast<Uint8>(pDepthBuffer->GetDesc().SampleCount);
            }
            else
            {
                FBKey.DSV = VK_NULL_HANDLE;
                RenderPassKey.DSVFormat = TEX_FORMAT_UNKNOWN;
            }

            FBKey.NumRenderTargets = m_NumBoundRenderTargets;
            RenderPassKey.NumRenderTargets = static_cast<Uint8>(m_NumBoundRenderTargets);

            for (Uint32 rt=0; rt < m_NumBoundRenderTargets; ++rt)
            {
                if (ITextureView* pRTV = m_pBoundRenderTargets[rt])
                {
                    auto* pRTVVk = ValidatedCast<TextureViewVkImpl>(pRTV);
                    auto* pRenderTarget = pRTVVk->GetTexture();
                    FBKey.RTVs[rt] = pRTVVk->GetVulkanImageView();
                    RenderPassKey.RTVFormats[rt] = pRenderTarget->GetDesc().Format;
                    if (RenderPassKey.SampleCount == 0)
                        RenderPassKey.SampleCount = static_cast<Uint8>(pRenderTarget->GetDesc().SampleCount);
                    else
                        VERIFY(RenderPassKey.SampleCount == pRenderTarget->GetDesc().SampleCount, "Inconsistent sample count");
                }
                else
                {
                    FBKey.RTVs[rt] = VK_NULL_HANDLE;
                    RenderPassKey.RTVFormats[rt] = TEX_FORMAT_UNKNOWN;
                }
            }

            auto pDeviceVkImpl = m_pDevice.RawPtr<RenderDeviceVkImpl>();
            auto& FBCache = pDeviceVkImpl->GetFramebufferCache();
            auto& RPCache = pDeviceVkImpl->GetRenderPassCache();

            m_RenderPass = RPCache.GetRenderPass(RenderPassKey);
            FBKey.Pass = m_RenderPass;
            m_Framebuffer = FBCache.GetFramebuffer(FBKey, m_FramebufferWidth, m_FramebufferHeight, m_FramebufferSlices);

            // Set the viewport to match the render target size
            SetViewports(1, nullptr, 0, 0);
        }

        EnsureVkCmdBuffer();
        CommitRenderPassAndFramebuffer();
    }

    void DeviceContextVkImpl::ResetRenderTargets()
    {
        TDeviceContextBase::ResetRenderTargets();
        m_RenderPass  = VK_NULL_HANDLE;
        m_Framebuffer = VK_NULL_HANDLE;
    }

    void DeviceContextVkImpl::UpdateBufferRegion(BufferVkImpl* pBuffVk, Uint64 DstOffset, Uint64 NumBytes, VkBuffer vkSrcBuffer, Uint64 SrcOffset)
    {
#ifdef DEVELOPMENT
        if (DstOffset + NumBytes > pBuffVk->GetDesc().uiSizeInBytes)
        {
            LOG_ERROR("Update region is out of buffer bounds which will result in an undefined behavior");
        }
#endif

        EnsureVkCmdBuffer();
        if (!pBuffVk->CheckAccessFlags(VK_ACCESS_TRANSFER_WRITE_BIT))
        {
            BufferMemoryBarrier(*pBuffVk, VK_ACCESS_TRANSFER_WRITE_BIT);
        }
        VkBufferCopy CopyRegion;
        CopyRegion.srcOffset = SrcOffset;
        CopyRegion.dstOffset = DstOffset;
        CopyRegion.size = NumBytes;
        VERIFY(pBuffVk->m_VulkanBuffer != VK_NULL_HANDLE, "Copy destination buffer must not be suballocated");
        m_CommandBuffer.CopyBuffer(vkSrcBuffer, pBuffVk->GetVkBuffer(), 1, &CopyRegion);
        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::UpdateBufferRegion(BufferVkImpl *pBuffVk, const void *pData, Uint64 DstOffset, Uint64 NumBytes)
    {
#ifdef DEVELOPMENT
        if (pBuffVk->GetDesc().Usage == USAGE_DYNAMIC)
        {
            LOG_ERROR("Dynamic buffers must be updated via Map()");
            return;
        }
#endif

        VERIFY_EXPR( static_cast<size_t>(NumBytes) == NumBytes );
        auto TmpSpace = m_UploadHeap.Allocate(static_cast<size_t>(NumBytes), 0);
	    memcpy(TmpSpace.CPUAddress, pData, static_cast<size_t>(NumBytes));
        UpdateBufferRegion(pBuffVk, DstOffset, NumBytes, TmpSpace.vkBuffer, TmpSpace.AlignedOffset);
        // The allocation will stay in the upload heap until the end of the frame at which point all upload
        // pages will be discarded
    }

    void DeviceContextVkImpl::CopyBufferRegion(BufferVkImpl *pSrcBuffVk, BufferVkImpl *pDstBuffVk, Uint64 SrcOffset, Uint64 DstOffset, Uint64 NumBytes)
    {
#ifdef DEVELOPMENT
        if (pDstBuffVk->GetDesc().Usage == USAGE_DYNAMIC)
        {
            LOG_ERROR("Dynamic buffers cannot be copy destinations");
            return;
        }
#endif

        EnsureVkCmdBuffer();
        if (!pSrcBuffVk->CheckAccessFlags(VK_ACCESS_TRANSFER_READ_BIT))
            BufferMemoryBarrier(*pSrcBuffVk, VK_ACCESS_TRANSFER_READ_BIT);
        if (!pDstBuffVk->CheckAccessFlags(VK_ACCESS_TRANSFER_WRITE_BIT))
            BufferMemoryBarrier(*pDstBuffVk, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferCopy CopyRegion;
        CopyRegion.srcOffset = SrcOffset + pSrcBuffVk->GetDynamicOffset(m_ContextId, this);
        CopyRegion.dstOffset = DstOffset;
        CopyRegion.size = NumBytes;
        VERIFY(pDstBuffVk->m_VulkanBuffer != VK_NULL_HANDLE, "Copy destination buffer must not be suballocated");
        VERIFY_EXPR(pDstBuffVk->GetDynamicOffset(m_ContextId, this) == 0);
        m_CommandBuffer.CopyBuffer(pSrcBuffVk->GetVkBuffer(), pDstBuffVk->GetVkBuffer(), 1, &CopyRegion);
        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::CopyTextureRegion(TextureVkImpl *pSrcTexture, TextureVkImpl *pDstTexture, const VkImageCopy &CopyRegion)
    {
        EnsureVkCmdBuffer();
        if (pSrcTexture->GetLayout() != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            TransitionImageLayout(*pSrcTexture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        }
        if (pDstTexture->GetLayout() != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            TransitionImageLayout(*pDstTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }
        // srcImageLayout must be VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL
        // dstImageLayout must be VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL (18.3)
        m_CommandBuffer.CopyImage(pSrcTexture->GetVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDstTexture->GetVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyRegion);
        ++m_State.NumCommands;
    }

    DeviceContextVkImpl::BufferToTextureCopyInfo DeviceContextVkImpl::GetBufferToTextureCopyInfo(
                                                       const TextureDesc& TexDesc,
                                                       Uint32             MipLevel,
                                                       const Box&         Region)const
    {
        BufferToTextureCopyInfo CopyInfo;
        const auto& FmtAttribs = GetTextureFormatAttribs(TexDesc.Format);
        VERIFY_EXPR(Region.MaxX > Region.MinX && Region.MaxY > Region.MinY && Region.MaxZ > Region.MinZ);
        auto UpdateRegionWidth  = Region.MaxX - Region.MinX;
        auto UpdateRegionHeight = Region.MaxY - Region.MinY;
        auto UpdateRegionDepth  = Region.MaxZ - Region.MinZ;
        if(FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
        {
            // Align update region size by the block size. This is only necessary when updating
            // coarse mip levels. Otherwise UpdateRegionWidth/Height should be multiples of block size
            VERIFY_EXPR( (FmtAttribs.BlockWidth  & (FmtAttribs.BlockWidth-1))  == 0 );
            VERIFY_EXPR( (FmtAttribs.BlockHeight & (FmtAttribs.BlockHeight-1)) == 0 );
            const auto BlockAlignedRegionWidth =  (UpdateRegionWidth  + (FmtAttribs.BlockWidth-1))  & ~(FmtAttribs.BlockWidth -1);
            const auto BlockAlignedRegionHeight = (UpdateRegionHeight + (FmtAttribs.BlockHeight-1)) & ~(FmtAttribs.BlockHeight-1);
            CopyInfo.RowSize  = BlockAlignedRegionWidth  / Uint32{FmtAttribs.BlockWidth} * Uint32{FmtAttribs.ComponentSize};
            CopyInfo.RowCount = BlockAlignedRegionHeight / FmtAttribs.BlockHeight;
            
            // (imageExtent.width + imageOffset.x) must be less than or equal to the image subresource width, and
            // (imageExtent.height + imageOffset.y) must be less than or equal to the image subresource height (18.4),
            // so we need to clamp UpdateRegionWidth and Height
            const Uint32 MipWidth  = std::max(TexDesc.Width  >> MipLevel, 1u);
            const Uint32 MipHeight = std::max(TexDesc.Height >> MipLevel, 1u);
            VERIFY_EXPR(MipWidth > Region.MinX);
            UpdateRegionWidth  = std::min(UpdateRegionWidth,  MipWidth  - Region.MinX);
            VERIFY_EXPR(MipHeight > Region.MinY);
            UpdateRegionHeight = std::min(UpdateRegionHeight, MipHeight - Region.MinY);
        }
        else
        {
            CopyInfo.RowSize  = UpdateRegionWidth * Uint32{FmtAttribs.ComponentSize} * Uint32{FmtAttribs.NumComponents};
            CopyInfo.RowCount = UpdateRegionHeight;
        }

        CopyInfo.Stride      = CopyInfo.RowSize;
        CopyInfo.DepthStride = CopyInfo.RowCount * CopyInfo.Stride;
        CopyInfo.MemorySize  = UpdateRegionDepth * CopyInfo.DepthStride;
        CopyInfo.Region      = Region;
        return CopyInfo;
    }


    void DeviceContextVkImpl::UpdateTextureRegion(const void*    pSrcData,
                                                  Uint32         SrcStride,
                                                  Uint32         SrcDepthStride,
                                                  TextureVkImpl& TextureVk,
                                                  Uint32         MipLevel,
                                                  Uint32         Slice,
                                                  const Box&     DstBox)
    {
        const auto& TexDesc = TextureVk.GetDesc();
        VERIFY(TexDesc.SampleCount == 1, "Only single-sample textures can be updated with vkCmdCopyBufferToImage()");
        auto CopyInfo = GetBufferToTextureCopyInfo(TexDesc, MipLevel, DstBox);
        const auto UpdateRegionDepth  = CopyInfo.Region.MaxZ - CopyInfo.Region.MinZ;

        // For UpdateTextureRegion(), use UploadHeap, not dynamic heap
        static constexpr const size_t BufferOffsetAlignment = 4; // bufferOffset of VkBufferImageCopy must be a multiple of 4 (18.4)
        auto Allocation = m_UploadHeap.Allocate(CopyInfo.MemorySize, BufferOffsetAlignment);
        // The allocation will stay in the upload heap until the end of the frame at which point all upload
        // pages will be discarded
        VERIFY( (Allocation.AlignedOffset % BufferOffsetAlignment) == 0, "Allocation offset must be at least 32-bit algined");

#ifdef _DEBUG
        VERIFY(SrcStride >= CopyInfo.RowSize, "Source data stride (", SrcStride, ") is below the image row size (", CopyInfo.RowSize, ")");
        const Uint32 PlaneSize = SrcStride * CopyInfo.RowCount;
        VERIFY(UpdateRegionDepth == 1 || SrcDepthStride >= PlaneSize, "Source data depth stride (", SrcDepthStride, ") is below the image plane size (", PlaneSize, ")");
#endif
        for(Uint32 DepthSlice = 0; DepthSlice < UpdateRegionDepth; ++DepthSlice)
        {
            for(Uint32 row = 0; row < CopyInfo.RowCount; ++row)
            {
                const auto* pSrcPtr =
                    reinterpret_cast<const Uint8*>(pSrcData)
                    + row        * SrcStride
                    + DepthSlice * SrcDepthStride;
                auto* pDstPtr =
                    reinterpret_cast<Uint8*>(Allocation.CPUAddress)
                    + row        * CopyInfo.Stride
                    + DepthSlice * CopyInfo.DepthStride;
                
                memcpy(pDstPtr, pSrcPtr, CopyInfo.RowSize);
            }
        }
        CopyBufferToTexture(Allocation.vkBuffer,
                            static_cast<Uint32>(Allocation.AlignedOffset),
                            CopyInfo.Region,
                            TextureVk,
                            MipLevel,
                            Slice);
    }

    void DeviceContextVkImpl::CopyBufferToTexture(VkBuffer         vkBuffer,
                                                  Uint32           BufferOffset,
                                                  const Box&       Region,
                                                  TextureVkImpl&   TextureVk,
                                                  Uint32           MipLevel,
                                                  Uint32           ArraySlice)
    {
        EnsureVkCmdBuffer();
        if (TextureVk.GetLayout() != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            TransitionImageLayout(TextureVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }

        VkBufferImageCopy CopyRegion = {};
        VERIFY( (BufferOffset % 4) == 0, "Source buffer offset must be multiple of 4 (18.4)");
        CopyRegion.bufferOffset = BufferOffset; // must be a multiple of 4 (18.4)

        // bufferRowLength and bufferImageHeight specify the data in buffer memory as a subregion of a larger two- or
        // three-dimensional image, and control the addressing calculations of data in buffer memory. If either of these
        // values is zero, that aspect of the buffer memory is considered to be tightly packed according to the imageExtent (18.4).
        CopyRegion.bufferRowLength   = 0;
        CopyRegion.bufferImageHeight = 0;

        const auto& TexDesc = TextureVk.GetDesc();
        const auto& FmtAttribs = GetTextureFormatAttribs(TexDesc.Format);
        // The aspectMask member of imageSubresource must only have a single bit set (18.4)
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH)
            CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
        {
            UNSUPPORTED("Updating depth-stencil texture is not currently supported");
            // When copying to or from a depth or stencil aspect, the data in buffer memory uses a layout 
            // that is a (mostly) tightly packed representation of the depth or stencil data.
            // To copy both the depth and stencil aspects of a depth/stencil format, two entries in 
            // pRegions can be used, where one specifies the depth aspect in imageSubresource, and the 
            // other specifies the stencil aspect (18.4)
        }
        else
            CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        CopyRegion.imageSubresource.baseArrayLayer = ArraySlice;
        CopyRegion.imageSubresource.layerCount = 1;
        CopyRegion.imageSubresource.mipLevel = MipLevel;
        // - imageOffset.x and (imageExtent.width + imageOffset.x) must both be greater than or equal to 0 and
        //   less than or equal to the image subresource width (18.4)
        // - imageOffset.y and (imageExtent.height + imageOffset.y) must both be greater than or equal to 0 and
        //   less than or equal to the image subresource height (18.4)
        CopyRegion.imageOffset = 
            VkOffset3D
            {
                static_cast<int32_t>(Region.MinX),
                static_cast<int32_t>(Region.MinY),
                static_cast<int32_t>(Region.MinZ)
            };
        VERIFY(Region.MaxX > Region.MinX && Region.MaxY - Region.MinY && Region.MaxZ > Region.MinZ,
               "[", Region.MinX, " .. ", Region.MaxX, ") x [", Region.MinY, " .. ", Region.MaxY, ") x [", Region.MinZ, " .. ", Region.MaxZ, ") is not a vaild region");
        CopyRegion.imageExtent =
            VkExtent3D
            {
                static_cast<uint32_t>(Region.MaxX - Region.MinX),
                static_cast<uint32_t>(Region.MaxY - Region.MinY),
                static_cast<uint32_t>(Region.MaxZ - Region.MinZ)
            };
        
        m_CommandBuffer.CopyBufferToImage(
            vkBuffer,
            TextureVk.GetVkImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // must be VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL (18.4)
            1,
            &CopyRegion);
    }

    void DeviceContextVkImpl::MapTexture( TextureVkImpl&            TextureVk,
                                          Uint32                    MipLevel,
                                          Uint32                    ArraySlice,
                                          MAP_TYPE                  MapType,
                                          Uint32                    MapFlags,
                                          const Box&                MapRegion,
                                          MappedTextureSubresource& MappedData )
    {
        const auto& TexDesc = TextureVk.GetDesc();
        auto CopyInfo = GetBufferToTextureCopyInfo(TexDesc, MipLevel, MapRegion);
        auto Allocation  = AllocateDynamicSpace(CopyInfo.MemorySize);
        VERIFY( (Allocation.Offset % 4) == 0, "Allocation offset must be at least 32-bit algined");

        MappedData.pData       = reinterpret_cast<Uint8*>(Allocation.pDynamicMemMgr->GetCPUAddress()) + Allocation.Offset;
        MappedData.Stride      = CopyInfo.Stride;
        MappedData.DepthStride = CopyInfo.DepthStride;

        auto it = m_MappedTextures.emplace(MappedTextureKey{&TextureVk, MipLevel, ArraySlice}, MappedTexture{CopyInfo, std::move(Allocation)});
        if(!it.second)
            LOG_ERROR_MESSAGE("Mip level ", MipLevel, ", slice ", ArraySlice, " of texture '", TexDesc.Name, "' has already been mapped");
    }

    void DeviceContextVkImpl::UnmapTexture( TextureVkImpl& TextureVk,
                                            Uint32         MipLevel,
                                            Uint32         ArraySlice)
    {
        const auto& TexDesc = TextureVk.GetDesc();
        auto UploadSpaceIt = m_MappedTextures.find(MappedTextureKey{&TextureVk, MipLevel, ArraySlice});
        if(UploadSpaceIt != m_MappedTextures.end())
        {
            auto& MappedTex = UploadSpaceIt->second;
            CopyBufferToTexture(MappedTex.Allocation.pDynamicMemMgr->GetVkBuffer(),
                                static_cast<Uint32>(MappedTex.Allocation.Offset),
                                MappedTex.CopyInfo.Region,
                                TextureVk,
                                MipLevel,
                                ArraySlice);
            m_MappedTextures.erase(UploadSpaceIt);
        }
        else
        {
            LOG_ERROR_MESSAGE("Failed to unmap mip level ", MipLevel, ", slice ", ArraySlice, " of texture '", TexDesc.Name, "'. The texture has either been unmapped already or has not been mapped");
        }
    }

    void DeviceContextVkImpl::FinishCommandList(class ICommandList **ppCommandList)
    {
        if (m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
        {
            m_CommandBuffer.EndRenderPass();
        }

        auto vkCmdBuff = m_CommandBuffer.GetVkCmdBuffer();
        auto err = vkEndCommandBuffer(vkCmdBuff);
        VERIFY(err == VK_SUCCESS, "Failed to end command buffer");

        auto* pDeviceVkImpl = m_pDevice.RawPtr<RenderDeviceVkImpl>();
        CommandListVkImpl *pCmdListVk( NEW_RC_OBJ(m_CmdListAllocator, "CommandListVkImpl instance", CommandListVkImpl)
                                                 (pDeviceVkImpl, this, vkCmdBuff, m_NextCmdBuffNumber) );
        pCmdListVk->QueryInterface( IID_CommandList, reinterpret_cast<IObject**>(ppCommandList) );
        
        m_CommandBuffer.SetVkCmdBuffer(VK_NULL_HANDLE);
        
        // Increment command buffer number, but do not release any resources until the command list is executed
        Atomics::AtomicIncrement(m_NextCmdBuffNumber);

        m_CommandBuffer.Reset();
        m_State = ContextState{};
        m_DescrSetBindInfo.Reset();
        m_pPipelineState = nullptr;

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
        
        // Note that not discarding resources when flushing the context does not help in case of multiple 
        // deferred contexts, and resources must not be released until the command list is executed via 
        // immediate context.
        
        // Next Cmd Buff| Next Fence |       Deferred Context  1      |       Deferred Contex 2        |   Immediate Context
        //              |            |                                |                                |
        //      N       |     F      |                                |                                |
        //              |            | Draw(ResourceX)                |                                |
        //              |            | Release(ResourceX)             | Draw(ResourceY)                |
        //              |            | - {N, ResourceX} -> Stale Objs | Release(ResourceY)             |
        //              |            |                                | - {N, ResourceY} -> Stale Objs |
        //              |            |                                |                                |  ExecuteCmdList(CmdList1)
        //              |            |                                |                                |  {F, ResourceX}-> Release queue
        //              |            |                                |                                |  {F, ResourceY}-> Release queue
        //     N+1      |    F+1     |                                |                                |
        //              |            |                                |                                |  ExecuteCmdList(CmdList2)
        //              |            |                                |                                |  - ResourceY is in release queue
        //              |            |                                |                                |
        Flush();
        
        InvalidateState();

        CommandListVkImpl* pCmdListVk = ValidatedCast<CommandListVkImpl>(pCommandList);
        VkCommandBuffer vkCmdBuff = VK_NULL_HANDLE;
        RefCntAutoPtr<IDeviceContext> pDeferredCtx;
        Uint64 DeferredCtxCmdBuffNumber = 0;
        pCmdListVk->Close(vkCmdBuff, pDeferredCtx, DeferredCtxCmdBuffNumber);
        VERIFY(vkCmdBuff != VK_NULL_HANDLE, "Trying to execute empty command buffer");
        VERIFY_EXPR(pDeferredCtx);
        VkSubmitInfo SubmitInfo = {};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &vkCmdBuff;
        auto pDeviceVkImpl = m_pDevice.RawPtr<RenderDeviceVkImpl>();
        VERIFY_EXPR(m_PendingFences.empty());
        auto pDeferredCtxVkImpl = pDeferredCtx.RawPtr<DeviceContextVkImpl>();
        auto SubmittedFenceValue = pDeviceVkImpl->ExecuteCommandBuffer(SubmitInfo, this, nullptr);
        pDeferredCtxVkImpl->m_LastSubmittedFenceValue = SubmittedFenceValue;
        
        // It is OK to dispose command buffer from another thread. We are not going to
        // record any commands and only need to add the buffer to the queue
        pDeferredCtxVkImpl->DisposeVkCmdBuffer(vkCmdBuff, SubmittedFenceValue);
        // We can now release all temporary resources in the deferred context associated with the submitted command list
        auto CompletedFenceValue = pDeviceVkImpl->GetCompletedFenceValue();
        pDeferredCtxVkImpl->ReleaseStaleContextResources(DeferredCtxCmdBuffNumber, SubmittedFenceValue, CompletedFenceValue);
    }

    void DeviceContextVkImpl::SignalFence(IFence* pFence, Uint64 Value)
    {
        VERIFY(!m_bIsDeferred, "Fence can only be signalled from immediate context");
        m_PendingFences.emplace_back( std::make_pair(Value, pFence) );
    };

    void DeviceContextVkImpl::TransitionImageLayout(ITexture *pTexture, VkImageLayout NewLayout)
    {
        VERIFY_EXPR(pTexture != nullptr);
        auto pTextureVk = ValidatedCast<TextureVkImpl>(pTexture);
        if (pTextureVk->GetLayout() != NewLayout)
        {
            TransitionImageLayout(*pTextureVk, NewLayout);
        }
    }

    void DeviceContextVkImpl::TransitionImageLayout(TextureVkImpl& TextureVk, VkImageLayout NewLayout)
    {
        VERIFY(TextureVk.GetLayout() != NewLayout, "The texture is already transitioned to correct layout");
        EnsureVkCmdBuffer();
        
        auto vkImg = TextureVk.GetVkImage();
        const auto& TexDesc = TextureVk.GetDesc();
        const auto& FmtAttribs = GetTextureFormatAttribs(TexDesc.Format);
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

    void DeviceContextVkImpl::TransitionImageLayout(TextureVkImpl &TextureVk, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresRange)
    {
        VERIFY(TextureVk.GetLayout() != NewLayout, "The texture is already transitioned to correct layout");
        EnsureVkCmdBuffer();
        auto vkImg = TextureVk.GetVkImage();
        m_CommandBuffer.TransitionImageLayout(vkImg, OldLayout, NewLayout, SubresRange);
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

        VERIFY(BufferVk.m_VulkanBuffer != VK_NULL_HANDLE, "Cannot transition suballocated buffer");
        VERIFY_EXPR(BufferVk.GetDynamicOffset(m_ContextId, this) == 0);
        auto vkBuff = BufferVk.GetVkBuffer();
        m_CommandBuffer.BufferMemoryBarrier(vkBuff, BufferVk.m_AccessFlags, NewAccessFlags);
        BufferVk.SetAccessFlags(NewAccessFlags);
    }

    VulkanDynamicAllocation DeviceContextVkImpl::AllocateDynamicSpace(Uint32 SizeInBytes)
    {
        auto DynAlloc = m_DynamicHeap.Allocate(SizeInBytes, 0);
#ifdef DEVELOPMENT
        DynAlloc.dvpFrameNumber = m_ContextFrameNumber;
#endif
        return DynAlloc;
    }
}
