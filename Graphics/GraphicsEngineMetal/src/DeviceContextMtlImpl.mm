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

#include "DeviceContextMtlImpl.h"
#include "BufferMtlImpl.h"
#include "ShaderMtlImpl.h"
#include "SamplerMtlImpl.h"
#include "MtlTypeConversions.h"
#include "TextureViewMtlImpl.h"
#include "PipelineStateMtlImpl.h"
#include "SwapChainMtl.h"
#include "ShaderResourceBindingMtlImpl.h"
#include "CommandListMtlImpl.h"
#include "RenderDeviceMtlImpl.h"
#include "FenceMtlImpl.h"

namespace Diligent
{
    DeviceContextMtlImpl::DeviceContextMtlImpl( IReferenceCounters*               pRefCounters,
                                                IMemoryAllocator&                 Allocator,
                                                RenderDeviceMtlImpl*              pDevice,
                                                const struct EngineMtlCreateInfo& EngineAttribs,
                                                bool                              bIsDeferred ) :
        TDeviceContextBase(pRefCounters, pDevice, bIsDeferred)
    {
    }

    IMPLEMENT_QUERY_INTERFACE( DeviceContextMtlImpl, IID_DeviceContextMtl, TDeviceContextBase )

    void DeviceContextMtlImpl::SetPipelineState(IPipelineState* pPipelineState)
    {
        auto* pPipelineStateMtl = ValidatedCast<PipelineStateMtlImpl>(pPipelineState);
        TDeviceContextBase::SetPipelineState( pPipelineStateMtl, 0 /*Dummy*/ );

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::SetPipelineState() is not implemented");

        auto& Desc = pPipelineStateMtl->GetDesc();
        if (Desc.IsComputePipeline)
        {

        }
        else
        {

        }
    }
    
    void DeviceContextMtlImpl::TransitionShaderResources(IPipelineState* pPipelineState, IShaderResourceBinding* pShaderResourceBinding)
    {
        DEV_CHECK_ERR(pPipelineState != nullptr, "Pipeline state must not be null");
        DEV_CHECK_ERR(pShaderResourceBinding != nullptr, "Shader resource binding must not be null");
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::TransitionShaderResources() is not implemented");
    }

    void DeviceContextMtlImpl::ResolveTextureSubresource(ITexture*                               pSrcTexture,
                                                         ITexture*                               pDstTexture,
                                                         const ResolveTextureSubresourceAttribs& ResolveAttribs)
    {
        TDeviceContextBase::ResolveTextureSubresource(pSrcTexture, pDstTexture, ResolveAttribs);
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::ResolveTextureSubresource() is not implemented");
    }

    void DeviceContextMtlImpl::CommitShaderResources(IShaderResourceBinding* pShaderResourceBinding, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
    {
        if (!TDeviceContextBase::CommitShaderResources(pShaderResourceBinding, StateTransitionMode, 0 /*Dummy*/))
            return;

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::CommitShaderResources() is not implemented");
    }

    void DeviceContextMtlImpl::SetStencilRef(Uint32 StencilRef)
    {
        if (TDeviceContextBase::SetStencilRef(StencilRef, 0))
        {
            LOG_ERROR_MESSAGE("DeviceContextMtlImpl::SetStencilRef() is not implemented");
        }
    }


    void DeviceContextMtlImpl::SetBlendFactors(const float* pBlendFactors)
    {
        if (TDeviceContextBase::SetBlendFactors(pBlendFactors, 0))
        {
            LOG_ERROR_MESSAGE("DeviceContextMtlImpl::SetBlendFactors() is not implemented");
        }
    }

    void DeviceContextMtlImpl::Draw(const DrawAttribs& Attribs)
    {
        if (!DvpVerifyDrawArguments(Attribs))
            return;
    
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::Draw() is not implemented");
    }

    void DeviceContextMtlImpl::DrawIndexed(const DrawIndexedAttribs& Attribs)
    {
        if (!DvpVerifyDrawIndexedArguments(Attribs))
            return;

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::DrawIndexed() is not implemented");
    }

    void DeviceContextMtlImpl::DrawIndirect(const DrawIndirectAttribs& Attribs, IBuffer* pAttribsBuffer)
    {
        if (!DvpVerifyDrawIndirectArguments(Attribs, pAttribsBuffer))
            return;

    
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::DrawIndirect() is not implemented");
    }

    void DeviceContextMtlImpl::DrawIndexedIndirect(const DrawIndexedIndirectAttribs& Attribs, IBuffer* pAttribsBuffer)
    {
        if (!DvpVerifyDrawIndexedIndirectArguments(Attribs, pAttribsBuffer))
            return;
    
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::DrawIndexedIndirect() is not implemented");
    }

    void DeviceContextMtlImpl::DispatchCompute(const DispatchComputeAttribs& Attribs)
    {
        if (!DvpVerifyDispatchArguments(Attribs))
            return;
    
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::DispatchCompute() is not implemented");
    }

    void DeviceContextMtlImpl::DispatchComputeIndirect(const DispatchComputeIndirectAttribs& Attribs, IBuffer* pAttribsBuffer)
    {
        if (!DvpVerifyDispatchIndirectArguments(Attribs, pAttribsBuffer))
            return;
    
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::DispatchComputeIndirect() is not implemented");
    }

    void DeviceContextMtlImpl::ClearDepthStencil(ITextureView*                    pView,
                                                   CLEAR_DEPTH_STENCIL_FLAGS      ClearFlags,
                                                   float                          fDepth,
                                                   Uint8                          Stencil,
                                                   RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
    {
        if (!TDeviceContextBase::ClearDepthStencil(pView))
            return;

        VERIFY_EXPR(pView != nullptr);

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::ClearDepthStencil() is not implemented");
    }

    void DeviceContextMtlImpl::ClearRenderTarget( ITextureView* pView, const float *RGBA, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode )
    {
        if (!TDeviceContextBase::ClearRenderTarget(pView))
            return;

        VERIFY_EXPR(pView != nullptr);

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::ClearRenderTarget() is not implemented");
    }

    void DeviceContextMtlImpl::Flush()
    {
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::Flush() is not implemented");
    }

    void DeviceContextMtlImpl::UpdateBuffer(IBuffer*                       pBuffer,
                                            Uint32                         Offset,
                                            Uint32                         Size,
                                            const void*                    pData,
                                            RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
    {                                       
        TDeviceContextBase::UpdateBuffer(pBuffer, Offset, Size, pData, StateTransitionMode);

        auto* pBufferMtlImpl = ValidatedCast<BufferMtlImpl>( pBuffer );

        (void)pBufferMtlImpl;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::UpdateBuffer() is not implemented");
    }

    void DeviceContextMtlImpl::CopyBuffer(IBuffer*                       pSrcBuffer,
                                            Uint32                         SrcOffset,
                                            RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                                            IBuffer*                       pDstBuffer,
                                            Uint32                         DstOffset,
                                            Uint32                         Size,
                                            RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode)
    {
        TDeviceContextBase::CopyBuffer(pSrcBuffer, SrcOffset, SrcBufferTransitionMode, pDstBuffer, DstOffset, Size, DstBufferTransitionMode);

        auto* pSrcBufferMtlImpl = ValidatedCast<BufferMtlImpl>( pSrcBuffer );
        auto* pDstBufferMtlImpl = ValidatedCast<BufferMtlImpl>( pDstBuffer );

        (void)pSrcBufferMtlImpl;
        (void)pDstBufferMtlImpl;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::CopyBuffer() is not implemented");
    }


    void DeviceContextMtlImpl::MapBuffer(IBuffer* pBuffer, MAP_TYPE MapType, MAP_FLAGS MapFlags, PVoid& pMappedData)
    {
        TDeviceContextBase::MapBuffer(pBuffer, MapType, MapFlags, pMappedData);

        auto* pBufferMtl = ValidatedCast<BufferMtlImpl>(pBuffer);

        (void)pBufferMtl;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::MapBuffer() is not implemented");
    }

    void DeviceContextMtlImpl::UnmapBuffer(IBuffer* pBuffer, MAP_TYPE MapType)
    {
        TDeviceContextBase::UnmapBuffer(pBuffer, MapType);
        auto* pBufferMtl = ValidatedCast<BufferMtlImpl>(pBuffer);

        (void)pBufferMtl;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::UnmapBuffer() is not implemented");
    }

    void DeviceContextMtlImpl::UpdateTexture(ITexture*                      pTexture,
                                             Uint32                         MipLevel,
                                             Uint32                         Slice,
                                             const Box&                     DstBox,
                                             const TextureSubResData&       SubresData,
                                             RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                                             RESOURCE_STATE_TRANSITION_MODE DstTextureTransitionMode)
    {
        TDeviceContextBase::UpdateTexture( pTexture, MipLevel, Slice, DstBox, SubresData, SrcBufferTransitionMode, DstTextureTransitionMode );

        auto* pTexMtl = ValidatedCast<TextureMtlImpl>(pTexture);
        const auto& Desc = pTexMtl->GetDesc();

        (void)Desc;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::UpdateTexture() is not implemented");
    }

    void DeviceContextMtlImpl::CopyTexture(const CopyTextureAttribs& CopyAttribs)
    {
        TDeviceContextBase::CopyTexture( CopyAttribs );

        auto* pSrcTexMtl = ValidatedCast<TextureMtlImpl>( CopyAttribs.pSrcTexture );
        auto* pDstTexMtl = ValidatedCast<TextureMtlImpl>( CopyAttribs.pDstTexture );
    
        (void)pSrcTexMtl;
        (void)pDstTexMtl;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::CopyTexture() is not implemented");
    }


    void DeviceContextMtlImpl::MapTextureSubresource( ITexture*                 pTexture,
                                                      Uint32                    MipLevel,
                                                      Uint32                    ArraySlice,
                                                      MAP_TYPE                  MapType,
                                                      MAP_FLAGS                 MapFlags,
                                                      const Box*                pMapRegion,
                                                      MappedTextureSubresource& MappedData )
    {
        TDeviceContextBase::MapTextureSubresource(pTexture, MipLevel, ArraySlice, MapType, MapFlags, pMapRegion, MappedData);

        auto* pTexMtl = ValidatedCast<TextureMtlImpl>(pTexture);
        const auto& TexDesc = pTexMtl->GetDesc();

        (void)TexDesc;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::MapTextureSubresource() is not implemented");
    }

    void DeviceContextMtlImpl::UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice)
    {
        TDeviceContextBase::UnmapTextureSubresource( pTexture, MipLevel, ArraySlice);

        auto* pTexMtl = ValidatedCast<TextureMtlImpl>(pTexture);
        const auto& TexDesc = pTexMtl->GetDesc();

        (void)TexDesc;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::UnmapTextureSubresource() is not implemented");
    }

    void DeviceContextMtlImpl::GenerateMips(ITextureView* pTextureView)
    {
        TDeviceContextBase::GenerateMips(pTextureView);
        auto& TexViewMtl = *ValidatedCast<TextureViewMtlImpl>(pTextureView);

        (void)TexViewMtl;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::GenerateMips() is not implemented");
    }

    void DeviceContextMtlImpl::FinishFrame()
    {
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::FinishFrame() is not implemented");
    }

    void DeviceContextMtlImpl::InvalidateState()
    {
        TDeviceContextBase::InvalidateState();

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::InvalidateState() is not implemented");
    }

    void DeviceContextMtlImpl::SetVertexBuffers( Uint32                         StartSlot,
                                                 Uint32                         NumBuffersSet,
                                                 IBuffer**                      ppBuffers,
                                                 Uint32*                        pOffsets,
                                                 RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                                 SET_VERTEX_BUFFERS_FLAGS       Flags )
    {
        TDeviceContextBase::SetVertexBuffers( StartSlot, NumBuffersSet, ppBuffers, pOffsets, StateTransitionMode, Flags );

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::SetVertexBuffers() is not implemented");
        for (Uint32 Slot = 0; Slot < m_NumVertexStreams; ++Slot)
        {
            auto& CurrStream = m_VertexStreams[Slot];
            if (auto* pBuffMtlImpl = CurrStream.pBuffer.RawPtr())
            {

            }
        }
    }

    void DeviceContextMtlImpl::SetIndexBuffer( IBuffer* pIndexBuffer, Uint32 ByteOffset, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode )
    {
        TDeviceContextBase::SetIndexBuffer( pIndexBuffer, ByteOffset, StateTransitionMode );

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::SetIndexBuffer() is not implemented");
        if (m_pIndexBuffer)
        {

        }

    }

    void DeviceContextMtlImpl::SetViewports( Uint32 NumViewports, const Viewport* pViewports, Uint32 RTWidth, Uint32 RTHeight  )
    {
        TDeviceContextBase::SetViewports( NumViewports, pViewports, RTWidth, RTHeight );

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::SetViewports() is not implemented");
    }

    void DeviceContextMtlImpl::SetScissorRects( Uint32 NumRects, const Rect* pRects, Uint32 RTWidth, Uint32 RTHeight  )
    {
        TDeviceContextBase::SetScissorRects(NumRects, pRects, RTWidth, RTHeight);

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::SetScissorRects() is not implemented");
    }

    void DeviceContextMtlImpl::SetRenderTargets(Uint32                         NumRenderTargets,
                                                ITextureView*                  ppRenderTargets[],
                                                ITextureView*                  pDepthStencil,
                                                RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
    {
        if( TDeviceContextBase::SetRenderTargets( NumRenderTargets, ppRenderTargets, pDepthStencil ) )
        {
            LOG_ERROR_MESSAGE("DeviceContextMtlImpl::SetRenderTargets() is not implemented");
        }
    }

    void DeviceContextMtlImpl::FinishCommandList(ICommandList **ppCommandList)
    {
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::FinishCommandList() is not implemented");
    }

    void DeviceContextMtlImpl::ExecuteCommandList(ICommandList* pCommandList)
    {
        if (m_bIsDeferred)
        {
            LOG_ERROR("Only immediate context can execute command list");
            return;
        }

        CommandListMtlImpl* pCmdListMtl = ValidatedCast<CommandListMtlImpl>(pCommandList);

        (void)pCmdListMtl;
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::ExecuteCommandList() is not implemented");
    }
       
    void DeviceContextMtlImpl::SignalFence(IFence* pFence, Uint64 Value)
    {
        VERIFY(!m_bIsDeferred, "Fence can only be signalled from immediate context");

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::SignalFence() is not implemented");
    }

    void DeviceContextMtlImpl::WaitForFence(IFence* pFence, Uint64 Value, bool FlushContext)
    {
        VERIFY(!m_bIsDeferred, "Fence can only be waited from immediate context");
        Flush();

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::Wait() is not implemented");
    }

    void DeviceContextMtlImpl::WaitForIdle()
    {
        VERIFY(!m_bIsDeferred, "Only immediate contexts can be idled");
        Flush();

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::WaitForIdle() is not implemented");
    }

    /// Implementation of IDeviceContext::BeginQuery() in Metal backend.
    void DeviceContextMtlImpl::BeginQuery(IQuery* pQuery)
    {
        if (!TDeviceContextBase::BeginQuery(pQuery, 0))
            return;

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::BeginQuery() is not implemented");
    }

    /// Implementation of IDeviceContext::EndQuery() in Metal backend.
    void DeviceContextMtlImpl::EndQuery(IQuery* pQuery)
    {
        if (!TDeviceContextBase::EndQuery(pQuery, 0))
            return;

        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::EndQuery() is not implemented");
    }


    void DeviceContextMtlImpl::TransitionResourceStates(Uint32 BarrierCount, StateTransitionDesc* pResourceBarriers)
    {
        LOG_ERROR_MESSAGE("DeviceContextMtlImpl::TransitionResourceStates() is not implemented");

        for (Uint32 i=0; i < BarrierCount; ++i)
        {
            const auto& Barrier = pResourceBarriers[i];
#ifdef DEVELOPMENT
            DvpVerifyStateTransitionDesc(Barrier);
#endif
            DEV_CHECK_ERR((Barrier.pTexture != nullptr) ^ (Barrier.pBuffer != nullptr), "Exactly one of pTexture or pBuffer must not be null");
            DEV_CHECK_ERR(Barrier.NewState != RESOURCE_STATE_UNKNOWN, "New resource state can't be unknown");

            if (Barrier.TransitionType == STATE_TRANSITION_TYPE_BEGIN)
            {
                // Skip begin-split barriers
                VERIFY(!Barrier.UpdateResourceState, "Resource state can't be updated in begin-split barrier");
                continue;
            }
            VERIFY(Barrier.TransitionType == STATE_TRANSITION_TYPE_IMMEDIATE || Barrier.TransitionType == STATE_TRANSITION_TYPE_END, "Unexpected barrier type");

            if (Barrier.pTexture)
            {
                auto* pTextureMtlImpl = ValidatedCast<TextureMtlImpl>(Barrier.pTexture);
                auto OldState = Barrier.OldState;
                if (OldState == RESOURCE_STATE_UNKNOWN)
                {
                    if (pTextureMtlImpl->IsInKnownState())
                    {
                        OldState = pTextureMtlImpl->GetState();
                    }
                    else
                    {
                        LOG_ERROR_MESSAGE("Failed to transition the state of texture '", pTextureMtlImpl->GetDesc().Name, "' because the buffer state is unknown and is not explicitly specified");
                        continue;
                    }
                }
                else
                {
                    if (pTextureMtlImpl->IsInKnownState() && pTextureMtlImpl->GetState() != OldState)
                    {
                        LOG_ERROR_MESSAGE("The state ", GetResourceStateString(pTextureMtlImpl->GetState()), " of texture '",
                                           pTextureMtlImpl->GetDesc().Name, "' does not match the old state ", GetResourceStateString(OldState),
                                           " specified by the barrier");
                    }
                }


                // Actual barrier code goes here


                if (Barrier.UpdateResourceState)
                {
                    pTextureMtlImpl->SetState(Barrier.NewState);
                }
            }
            else
            {
                VERIFY_EXPR(Barrier.pBuffer);
                auto* pBufferMtlImpl = ValidatedCast<BufferMtlImpl>(Barrier.pBuffer);
                auto OldState = Barrier.OldState;
                if (OldState == RESOURCE_STATE_UNKNOWN)
                {
                    if (pBufferMtlImpl->IsInKnownState())
                    {
                        OldState = pBufferMtlImpl->GetState();
                    }
                    else
                    {
                        LOG_ERROR_MESSAGE("Failed to transition the state of buffer '", pBufferMtlImpl->GetDesc().Name, "' because the buffer state is unknown and is not explicitly specified");
                        continue;
                    }
                }
                else
                {
                    if (pBufferMtlImpl->IsInKnownState() && pBufferMtlImpl->GetState() != OldState)
                    {
                        LOG_ERROR_MESSAGE("The state ", GetResourceStateString(pBufferMtlImpl->GetState()), " of buffer '",
                                           pBufferMtlImpl->GetDesc().Name, "' does not match the old state ", GetResourceStateString(OldState),
                                           " specified by the barrier");
                    }
                }


                // Actual barrier code goes here


                if (Barrier.UpdateResourceState)
                {
                    pBufferMtlImpl->SetState(Barrier.NewState);
                }
            }
        }
    }
}
