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
#include "DynamicUploadHeap.h"
#include "CommandListVkImpl.h"

namespace Diligent
{

    DeviceContextVkImpl::DeviceContextVkImpl( IReferenceCounters *pRefCounters, RenderDeviceVkImpl *pDeviceVkImpl, bool bIsDeferred, const EngineVkAttribs &Attribs, Uint32 ContextId) :
        TDeviceContextBase(pRefCounters, pDeviceVkImpl, bIsDeferred),
        //m_pUploadHeap(pDeviceVkImpl->RequestUploadHeap() ),
        m_NumCommandsToFlush(bIsDeferred ? std::numeric_limits<decltype(m_NumCommandsToFlush)>::max() : Attribs.NumCommandsToFlushCmdBuffer),
        /*m_MipsGenerator(pDeviceVkImpl->GetVkDevice()),
        m_CmdListAllocator(GetRawAllocator(), sizeof(CommandListVkImpl), 64 ),
        m_ContextId(ContextId),*/
        m_CmdPool(pDeviceVkImpl->GetLogicalDevice().GetSharedPtr(), pDeviceVkImpl->GetCmdQueue()->GetQueueFamilyIndex(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
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
            DisposeVkCmdBuffer();
        }
        else
        {
            if (m_State.NumCommands != 0)
                LOG_WARNING_MESSAGE("Flusing outstanding commands from the device context being destroyed. This may result in synchronization errors");

            Flush();
        }
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

    inline void DeviceContextVkImpl::DisposeVkCmdBuffer()
    {
        VERIFY(m_CommandBuffer.GetState().RenderPass == VK_NULL_HANDLE, "Disposing command buffer with unifinished render pass");
        auto vkCmdBuff = m_CommandBuffer.GetVkCmdBuffer();
        if(vkCmdBuff != VK_NULL_HANDLE)
        {
            auto pDeviceVkImpl = m_pDevice.RawPtr<RenderDeviceVkImpl>();
            m_CmdPool.DisposeCommandBuffer(vkCmdBuff, pDeviceVkImpl->GetNextFenceValue());
            m_CommandBuffer.Reset();
        }
    }


    void DeviceContextVkImpl::SetPipelineState(IPipelineState *pPipelineState)
    {
        if (m_CommandBuffer.GetState().RenderPass)
        {
            m_CommandBuffer.EndRenderPass();
        }

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
#if 0
        VERIFY_EXPR(pPipelineState != nullptr);

        auto *pCtx = RequestCmdContext();
        auto *pPipelineStateVk = ValidatedCast<PipelineStateVkImpl>(pPipelineState);
        pPipelineStateVk->CommitAndTransitionShaderResources(pShaderResourceBinding, *pCtx, false, true);
#endif
    }

    void DeviceContextVkImpl::CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags)
    {
        if (!DeviceContextBase::CommitShaderResources<PipelineStateVkImpl>(pShaderResourceBinding, Flags, 0 /*Dummy*/))
            return;
#if 0
        auto *pCtx = RequestCmdContext();
        auto *pPipelineStateVk = m_pPipelineState.RawPtr<PipelineStateVkImpl>();

        m_pCommittedResourceCache = pPipelineStateVk->CommitAndTransitionShaderResources(pShaderResourceBinding, *pCtx, true, (Flags & COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES)!=0);
#endif
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

    void DeviceContextVkImpl::CommitRenderPassAndFramebuffer()
    {
        auto *pPipelineStateVk = m_pPipelineState.RawPtr<PipelineStateVkImpl>();
        
    }

    void DeviceContextVkImpl::CommitVkIndexBuffer(VALUE_TYPE IndexType)
    {
        VERIFY( m_pIndexBuffer != nullptr, "Index buffer is not set up for indexed draw command" );
#if 0
        Vk_INDEX_BUFFER_VIEW IBView;
        BufferVkImpl *pBuffVk = static_cast<BufferVkImpl *>(m_pIndexBuffer.RawPtr());
        IBView.BufferLocation = pBuffVk->GetGPUAddress(m_ContextId) + m_IndexDataStartOffset;
        if( IndexType == VT_UINT32 )
            IBView.Format = DXGI_FORMAT_R32_UINT;
        else if( IndexType == VT_UINT16 )
            IBView.Format = DXGI_FORMAT_R16_UINT;
        else
        {
            UNEXPECTED( "Unsupported index format. Only R16_UINT and R32_UINT are allowed." );
        }
        // Note that for a dynamic buffer, what we use here is the size of the buffer itself, not the upload heap buffer!
        IBView.SizeInBytes = pBuffVk->GetDesc().uiSizeInBytes - m_IndexDataStartOffset;

        // Device context keeps strong reference to bound index buffer.
        // When the buffer is unbound, the reference to the Vk resource
        // is added to the context. There is no need to add reference here
        //auto &GraphicsCtx = RequestCmdContext()->AsGraphicsContext();
        //auto *pVkResource = pBuffVk->GetVkBuffer();
        //GraphicsCtx.AddReferencedObject(pVkResource);

        bool IsDynamic = pBuffVk->GetDesc().Usage == USAGE_DYNAMIC;
#ifdef _DEBUG
        if(IsDynamic)
            pBuffVk->DbgVerifyDynamicAllocation(m_ContextId);
#endif
        auto &GraphicsCtx = RequestCmdContext()->AsGraphicsContext();
        // Resource transitioning must always be performed!
        GraphicsCtx.TransitionResource(pBuffVk, Vk_RESOURCE_STATE_INDEX_BUFFER, true);

        size_t BuffDataStartByteOffset;
        auto *pVkBuff = pBuffVk->GetVkBuffer(BuffDataStartByteOffset, m_ContextId);

        if( IsDynamic || 
            m_CommittedVkIndexBuffer != pVkBuff ||
            m_CommittedIBFormat != IndexType ||
            m_CommittedVkIndexDataStartOffset != m_IndexDataStartOffset + BuffDataStartByteOffset)
        {
            m_CommittedVkIndexBuffer = pVkBuff;
            m_CommittedIBFormat = IndexType;
            m_CommittedVkIndexDataStartOffset = m_IndexDataStartOffset + static_cast<Uint32>(BuffDataStartByteOffset);
            GraphicsCtx.SetIndexBuffer( IBView );
        }
        
        // GPU virtual address of a dynamic index buffer can change every time
        // a draw command is invoked
        m_bCommittedVkIBUpToDate = !IsDynamic;
#endif
    }

    void DeviceContextVkImpl::TransitionVkVertexBuffers(GraphicsContext &GraphCtx)
    {
#if 0
        for( UINT Buff = 0; Buff < m_NumVertexStreams; ++Buff )
        {
            auto &CurrStream = m_VertexStreams[Buff];
            VERIFY( CurrStream.pBuffer, "Attempting to bind a null buffer for rendering" );
            auto *pBufferVk = static_cast<BufferVkImpl*>(CurrStream.pBuffer.RawPtr());
            if(!pBufferVk->CheckAllStates(Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
                GraphCtx.TransitionResource(pBufferVk, Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        }
#endif
    }

    void DeviceContextVkImpl::CommitVkVertexBuffers(GraphicsContext &GraphCtx)
    {
        auto *pPipelineStateVk = m_pPipelineState.RawPtr<PipelineStateVkImpl>();
#if 0
        // Do not initialize array with zeroes for performance reasons
        Vk_VERTEX_BUFFER_VIEW VBViews[MaxBufferSlots];// = {}
        VERIFY( m_NumVertexStreams <= MaxBufferSlots, "Too many buffers are being set" );
        const auto *TightStrides = pPipelineStateVk->GetTightStrides();
        bool DynamicBufferPresent = false;
        for( UINT Buff = 0; Buff < m_NumVertexStreams; ++Buff )
        {
            auto &CurrStream = m_VertexStreams[Buff];
            auto &VBView = VBViews[Buff];
            VERIFY( CurrStream.pBuffer, "Attempting to bind a null buffer for rendering" );
            
            auto *pBufferVk = static_cast<BufferVkImpl*>(CurrStream.pBuffer.RawPtr());
            if (pBufferVk->GetDesc().Usage == USAGE_DYNAMIC)
            {
                DynamicBufferPresent = true;
#ifdef _DEBUG
                pBufferVk->DbgVerifyDynamicAllocation(m_ContextId);
#endif
            }

            GraphCtx.TransitionResource(pBufferVk, Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            
            // Device context keeps strong references to all vertex buffers.
            // When a buffer is unbound, a reference to Vk resource is added to the context,
            // so there is no need to reference the resource here
            //GraphicsCtx.AddReferencedObject(pVkResource);

            VBView.BufferLocation = pBufferVk->GetGPUAddress(m_ContextId) + CurrStream.Offset;
            VBView.StrideInBytes = CurrStream.Stride ? CurrStream.Stride : TightStrides[Buff];
            // Note that for a dynamic buffer, what we use here is the size of the buffer itself, not the upload heap buffer!
            VBView.SizeInBytes = pBufferVk->GetDesc().uiSizeInBytes - CurrStream.Offset;
        }

        GraphCtx.FlushResourceBarriers();
        GraphCtx.SetVertexBuffers( 0, m_NumVertexStreams, VBViews );

        // GPU virtual address of a dynamic vertex buffer can change every time
        // a draw command is invoked
        m_bCommittedVkVBsUpToDate = !DynamicBufferPresent;
#endif
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

        if(m_CommandBuffer.GetState().RenderPass == VK_NULL_HANDLE)
            CommitRenderPassAndFramebuffer();

#if 0
        auto &GraphCtx = RequestCmdContext()->AsGraphicsContext();
        if( DrawAttribs.IsIndexed )
        {
            if( m_CommittedIBFormat != DrawAttribs.IndexType )
                m_bCommittedVkIBUpToDate = false;

            if(m_bCommittedVkIBUpToDate)
            {
                BufferVkImpl *pBuffVk = static_cast<BufferVkImpl *>(m_pIndexBuffer.RawPtr());
                if(!pBuffVk->CheckAllStates(Vk_RESOURCE_STATE_INDEX_BUFFER))
                    GraphCtx.TransitionResource(pBuffVk, Vk_RESOURCE_STATE_INDEX_BUFFER, true);
            }
            else
                CommitVkIndexBuffer(DrawAttribs.IndexType);
        }

        auto *pPipelineStateVk = m_pPipelineState.RawPtr<PipelineStateVkImpl>();
        
        auto VkTopology = TopologyToVkTopology( DrawAttribs.Topology );
        GraphCtx.SetPrimitiveTopology(VkTopology);

        if(m_bCommittedVkVBsUpToDate)
            TransitionVkVertexBuffers(GraphCtx);
        else
            CommitVkVertexBuffers(GraphCtx);

        GraphCtx.SetRootSignature( pPipelineStateVk->GetVkRootSignature() );

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
        

        if( DrawAttribs.IsIndirect )
        {
            if( auto *pBufferVk = ValidatedCast<BufferVkImpl>(DrawAttribs.pIndirectDrawAttribs) )
            {
#ifdef _DEBUG
                if(pBufferVk->GetDesc().Usage == USAGE_DYNAMIC)
                    pBufferVk->DbgVerifyDynamicAllocation(m_ContextId);
#endif

                GraphCtx.TransitionResource(pBufferVk, Vk_RESOURCE_STATE_INDIRECT_ARGUMENT);
                size_t BuffDataStartByteOffset;
                IVkResource *pVkArgsBuff = pBufferVk->GetVkBuffer(BuffDataStartByteOffset, m_ContextId);
                GraphCtx.ExecuteIndirect(DrawAttribs.IsIndexed ? m_pDrawIndexedIndirectSignature : m_pDrawIndirectSignature, pVkArgsBuff, DrawAttribs.IndirectDrawArgsOffset + BuffDataStartByteOffset);
            }
            else
            {
                LOG_ERROR_MESSAGE("Valid pIndirectDrawAttribs must be provided for indirect draw command");
            }
        }
        else
        {
            if( DrawAttribs.IsIndexed )
                GraphCtx.DrawIndexed(DrawAttribs.NumIndices, DrawAttribs.NumInstances, DrawAttribs.FirstIndexLocation, DrawAttribs.BaseVertex, DrawAttribs.FirstInstanceLocation);
            else
                GraphCtx.Draw(DrawAttribs.NumVertices, DrawAttribs.NumInstances, DrawAttribs.StartVertexLocation, DrawAttribs.FirstInstanceLocation );
        }
#endif
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
        if(m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
        {
            UNSUPPORTED("Not yet implemented");
        }
        else
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
        if(m_CommandBuffer.GetState().RenderPass != VK_NULL_HANDLE)
        {
            UNSUPPORTED("Not yet implemented");
        }
        else
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

        if(SubmitInfo.commandBufferCount != 0 || SubmitInfo.waitSemaphoreCount !=0 || SubmitInfo.signalSemaphoreCount != 0)
        {
            pDeviceVkImpl->ExecuteCommandBuffer(SubmitInfo, true);
        }

        m_WaitSemaphores.clear();
        m_WaitDstStageMasks.clear();
        m_SignalSemaphores.clear();

        if (vkCmdBuff != VK_NULL_HANDLE)
        {
            DisposeVkCmdBuffer();
        }

        m_State.NumCommands = 0;
        m_pPipelineState.Release(); 
    }

    void DeviceContextVkImpl::SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )
    {
        TDeviceContextBase::SetVertexBuffers( StartSlot, NumBuffersSet, ppBuffers, pStrides, pOffsets, Flags );
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
            VkViewports[vp].y        = m_Viewports[vp].TopLeftY;
            VkViewports[vp].width    = m_Viewports[vp].Width;
            VkViewports[vp].height   = m_Viewports[vp].Height;
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
#if 0
            CommitRenderTargets();

            // Set the viewport to match the render target size
            SetViewports(1, nullptr, 0, 0);
#endif
        }
    }
   
#if 0
    DynamicAllocation DeviceContextVkImpl::AllocateDynamicSpace(size_t NumBytes)
    {
        return m_pUploadHeap->Allocate(NumBytes);
    }
#endif

#if 0
    void DeviceContextVkImpl::UpdateBufferRegion(class BufferVkImpl *pBuffVk, DynamicAllocation& Allocation, Uint64 DstOffset, Uint64 NumBytes)
    {
        auto pCmdCtx = RequestCmdContext();
        VERIFY_EXPR( static_cast<size_t>(NumBytes) == NumBytes );
        pCmdCtx->TransitionResource(pBuffVk, Vk_RESOURCE_STATE_COPY_DEST, true);
        size_t DstBuffDataStartByteOffset;
        auto *pVkBuff = pBuffVk->GetVkBuffer(DstBuffDataStartByteOffset, m_ContextId);
        VERIFY(DstBuffDataStartByteOffset == 0, "Dst buffer must not be suballocated");
        pCmdCtx->GetCommandList()->CopyBufferRegion( pVkBuff, DstOffset + DstBuffDataStartByteOffset, Allocation.pBuffer, Allocation.Offset, NumBytes);
        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::UpdateBufferRegion(BufferVkImpl *pBuffVk, const void *pData, Uint64 DstOffset, Uint64 NumBytes)
    {
        VERIFY(pBuffVk->GetDesc().Usage != USAGE_DYNAMIC, "Dynamic buffers must be updated via Map()");
        VERIFY_EXPR( static_cast<size_t>(NumBytes) == NumBytes );
        auto TmpSpace = m_pUploadHeap->Allocate(static_cast<size_t>(NumBytes));
	    memcpy(TmpSpace.CPUAddress, pData, static_cast<size_t>(NumBytes));
        UpdateBufferRegion(pBuffVk, TmpSpace, DstOffset, NumBytes);
    }
#endif

#if 0
    void DeviceContextVkImpl::CopyBufferRegion(BufferVkImpl *pSrcBuffVk, BufferVkImpl *pDstBuffVk, Uint64 SrcOffset, Uint64 DstOffset, Uint64 NumBytes)
    {
        VERIFY(pDstBuffVk->GetDesc().Usage != USAGE_DYNAMIC, "Dynamic buffers cannot be copy destinations");

        auto pCmdCtx = RequestCmdContext();
        pCmdCtx->TransitionResource(pSrcBuffVk, Vk_RESOURCE_STATE_COPY_SOURCE);
        pCmdCtx->TransitionResource(pDstBuffVk, Vk_RESOURCE_STATE_COPY_DEST, true);
        size_t DstDataStartByteOffset;
        auto *pVkDstBuff = pDstBuffVk->GetVkBuffer(DstDataStartByteOffset, m_ContextId);
        VERIFY(DstDataStartByteOffset == 0, "Dst buffer must not be suballocated");

        size_t SrcDataStartByteOffset;
        auto *pVkSrcBuff = pSrcBuffVk->GetVkBuffer(SrcDataStartByteOffset, m_ContextId);
        pCmdCtx->GetCommandList()->CopyBufferRegion( pVkDstBuff, DstOffset + DstDataStartByteOffset, pVkSrcBuff, SrcOffset+SrcDataStartByteOffset, NumBytes);
        ++m_State.NumCommands;
    }

    void DeviceContextVkImpl::CopyTextureRegion(TextureVkImpl *pSrcTexture, Uint32 SrcSubResIndex, const Vk_BOX *pVkSrcBox,
                                                   TextureVkImpl *pDstTexture, Uint32 DstSubResIndex, Uint32 DstX, Uint32 DstY, Uint32 DstZ)
    {
        auto pCmdCtx = RequestCmdContext();
        pCmdCtx->TransitionResource(pSrcTexture, Vk_RESOURCE_STATE_COPY_SOURCE);
        pCmdCtx->TransitionResource(pDstTexture, Vk_RESOURCE_STATE_COPY_DEST, true);

        Vk_TEXTURE_COPY_LOCATION DstLocation = {}, SrcLocation = {};

        DstLocation.Type = Vk_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.pResource = pDstTexture->GetVkResource();
        DstLocation.SubresourceIndex = DstSubResIndex;

        SrcLocation.Type = Vk_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        SrcLocation.pResource = pSrcTexture->GetVkResource();
        SrcLocation.SubresourceIndex = SrcSubResIndex;

        pCmdCtx->GetCommandList()->CopyTextureRegion( &DstLocation, DstX, DstY, DstZ, &SrcLocation, pVkSrcBox);
        ++m_State.NumCommands;
    }

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
#if 0
        CommandListVkImpl *pCmdListVk( NEW_RC_OBJ(m_CmdListAllocator, "CommandListVkImpl instance", CommandListVkImpl)
                                                       (m_pDevice, m_pCurrCmdCtx) );
        pCmdListVk->QueryInterface( IID_CommandList, reinterpret_cast<IObject**>(ppCommandList) );
        m_pCurrCmdCtx = nullptr;
        //Flush(); 

        m_CommandBuffer.Reset();
        m_State = ContextState{};
        m_pPipelineState.Release();

        InvalidateState();
#endif
    }

    void DeviceContextVkImpl::ExecuteCommandList(class ICommandList *pCommandList)
    {
        if (m_bIsDeferred)
        {
            LOG_ERROR("Only immediate context can execute command list");
            return;
        }
#if 0
        // First execute commands in this context
        Flush();

        InvalidateState();

        CommandListVkImpl* pCmdListVk = ValidatedCast<CommandListVkImpl>(pCommandList);
        m_pDevice.RawPtr<RenderDeviceVkImpl>()->CloseAndExecuteCommandContext(pCmdListVk->Close(), true);
#endif
    }


    void DeviceContextVkImpl::TransitionImageLayout(ITexture *pTexture, VkImageLayout NewLayout)
    {
        VERIFY_EXPR(pTexture != nullptr);
        auto pTextureVk = ValidatedCast<TextureVkImpl>(pTexture);
        if(pTextureVk->GetLayout() != NewLayout)
        {
            EnsureVkCmdBuffer();
        
            auto vkImg = pTextureVk->GetVkImage();
            const auto& TexDesc = pTextureVk->GetDesc();
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
            m_CommandBuffer.TransitionImageLayout(vkImg, pTextureVk->GetLayout(), NewLayout, SubresRange);
            pTextureVk->SetLayout(NewLayout);
        }
    }

#if 0
    void DeviceContextVkImpl::TransitionBufferState(IBuffer *pBuffer, Vk_RESOURCE_STATES State)
    {
        VERIFY_EXPR(pBuffer != nullptr);
        auto *pCmdCtx = RequestCmdContext();
        pCmdCtx->TransitionResource(ValidatedCast<IBufferVk>(pBuffer), State);
    }
#endif
}
