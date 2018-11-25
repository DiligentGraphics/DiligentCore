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
#include "RenderDeviceD3D12Impl.h"
#include "DeviceContextD3D12Impl.h"
#include "SwapChainD3D12.h"
#include "PipelineStateD3D12Impl.h"
#include "CommandContext.h"
#include "TextureD3D12Impl.h"
#include "BufferD3D12Impl.h"
#include "FenceD3D12Impl.h"
#include "D3D12TypeConversions.h"
#include "d3dx12_win.h"
#include "D3D12DynamicHeap.h"
#include "CommandListD3D12Impl.h"
#include "DXGITypeConversions.h"

namespace Diligent
{

    static std::string GetContextObjectName(const char* Object, bool bIsDeferred, Uint32 ContextId)
    {
        std::stringstream ss;
        ss << Object;
        if (bIsDeferred)
            ss << " of deferred context #" << ContextId;
        else
            ss << " of immediate context";
        return ss.str();
    }

    DeviceContextD3D12Impl::DeviceContextD3D12Impl( IReferenceCounters*       pRefCounters,
                                                    RenderDeviceD3D12Impl*    pDeviceD3D12Impl,
                                                    bool                      bIsDeferred,
                                                    const EngineD3D12Attribs& Attribs,
                                                    Uint32                    ContextId,
                                                    Uint32                    CommandQueueId) :
        TDeviceContextBase
        {
            pRefCounters,
            pDeviceD3D12Impl,
            ContextId,
            CommandQueueId,
            bIsDeferred ? std::numeric_limits<decltype(m_NumCommandsToFlush)>::max() : Attribs.NumCommandsToFlushCmdList,
            bIsDeferred
        },
        m_DynamicHeap
        {
            pDeviceD3D12Impl->GetDynamicMemoryManager(),
            GetContextObjectName("Dynamic heap", bIsDeferred, ContextId),
            Attribs.DynamicHeapPageSize
        },
        m_DynamicGPUDescriptorAllocator
        {
            {
                GetRawAllocator(),
                pDeviceD3D12Impl->GetGPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
        		Attribs.DynamicDescriptorAllocationChunkSize[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
                GetContextObjectName("CBV_SRV_UAV dynamic descriptor allocator", bIsDeferred, ContextId)
            },
            {
                GetRawAllocator(),
                pDeviceD3D12Impl->GetGPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
                Attribs.DynamicDescriptorAllocationChunkSize[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER],
                GetContextObjectName("SAMPLER     dynamic descriptor allocator", bIsDeferred, ContextId)
            }
        },
        m_MipsGenerator(pDeviceD3D12Impl->GetD3D12Device()),
        m_CmdListAllocator(GetRawAllocator(), sizeof(CommandListD3D12Impl), 64 )
    {
        RequestCommandContext(pDeviceD3D12Impl);

        auto *pd3d12Device = pDeviceD3D12Impl->GetD3D12Device();

        D3D12_COMMAND_SIGNATURE_DESC CmdSignatureDesc = {};
        D3D12_INDIRECT_ARGUMENT_DESC IndirectArg = {};
        CmdSignatureDesc.NodeMask = 0;
        CmdSignatureDesc.NumArgumentDescs = 1;
        CmdSignatureDesc.pArgumentDescs = &IndirectArg;

        CmdSignatureDesc.ByteStride = sizeof(UINT)*4;
        IndirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        auto hr = pd3d12Device->CreateCommandSignature(&CmdSignatureDesc, nullptr, __uuidof(m_pDrawIndirectSignature), reinterpret_cast<void**>(static_cast<ID3D12CommandSignature**>(&m_pDrawIndirectSignature)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create indirect draw command signature");

        CmdSignatureDesc.ByteStride = sizeof(UINT)*5;
        IndirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        hr = pd3d12Device->CreateCommandSignature(&CmdSignatureDesc, nullptr, __uuidof(m_pDrawIndexedIndirectSignature), reinterpret_cast<void**>(static_cast<ID3D12CommandSignature**>(&m_pDrawIndexedIndirectSignature)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create draw indexed indirect command signature");

        CmdSignatureDesc.ByteStride = sizeof(UINT)*3;
        IndirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        hr = pd3d12Device->CreateCommandSignature(&CmdSignatureDesc, nullptr, __uuidof(m_pDispatchIndirectSignature), reinterpret_cast<void**>(static_cast<ID3D12CommandSignature**>(&m_pDispatchIndirectSignature)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create dispatch indirect command signature");
    }

    DeviceContextD3D12Impl::~DeviceContextD3D12Impl()
    {
        if (m_State.NumCommands != 0)
        {
            LOG_ERROR_MESSAGE(m_bIsDeferred ? 
                                "There are outstanding commands in deferred context #", m_ContextId, " being destroyed, which indicates that FinishCommandList() has not been called." :
                                "There are outstanding commands in the immediate context being destroyed, which indicates the context has not been Flush()'ed.",
                              " This is unexpected and may result in synchronization errors");
        }

        if(m_bIsDeferred)
        {
            if (m_CurrCmdCtx)
            {
                // The command context has never been executed, so it can be disposed without going through release queue
                m_pDevice.RawPtr<RenderDeviceD3D12Impl>()->DisposeCommandContext(std::move(m_CurrCmdCtx));
            }
        }
        else
        {
            Flush(false);
        }

        // For deferred contexts, m_SubmittedBuffersCmdQueueMask is reset to 0 after every call to FinishFrame().
        // In this case there are no resources to release, so there will be no issues.
        FinishFrame();

        // Note: as dynamic pages are returned to the global dynamic memory manager hosted by the render device,
        // the dynamic heap can be destroyed before all pages are actually returned to the global manager.
        DEV_CHECK_ERR(m_DynamicHeap.GetAllocatedPagesCount() == 0, "All dynamic pages must have been released by now.");

        for(size_t i=0; i < _countof(m_DynamicGPUDescriptorAllocator); ++i)
        {
            // Note: as dynamic decriptor suballocations are returned to the global GPU descriptor heap that
            // is hosted by the render device, the descriptor allocator can be destroyed before all suballocations
            // are actually returned to the global heap.
            DEV_CHECK_ERR(m_DynamicGPUDescriptorAllocator[i].GetSuballocationCount() == 0, "All dynamic suballocations must have been released");
        }
    }

    IMPLEMENT_QUERY_INTERFACE( DeviceContextD3D12Impl, IID_DeviceContextD3D12, TDeviceContextBase )
    
    void DeviceContextD3D12Impl::SetPipelineState(IPipelineState* pPipelineState)
    {
        // Never flush deferred context!
        if (!m_bIsDeferred && m_State.NumCommands >= m_NumCommandsToFlush)
        {
            Flush(true);
        }

        auto* pPipelineStateD3D12 = ValidatedCast<PipelineStateD3D12Impl>(pPipelineState);
        const auto &PSODesc = pPipelineStateD3D12->GetDesc();

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
            // We also need to update scissor rect if ScissorEnable state has changed
            CommitScissor = OldPSODesc.GraphicsPipeline.RasterizerDesc.ScissorEnable != PSODesc.GraphicsPipeline.RasterizerDesc.ScissorEnable;
        }

        TDeviceContextBase::SetPipelineState( pPipelineStateD3D12, 0 /*Dummy*/ );

        auto& CmdCtx = GetCmdContext();
        
        auto *pd3d12PSO = pPipelineStateD3D12->GetD3D12PipelineState();
        if (PSODesc.IsComputePipeline)
        {
            CmdCtx.AsComputeContext().SetPipelineState(pd3d12PSO);
        }
        else
        {
            auto &GraphicsCtx = CmdCtx.AsGraphicsContext();
            GraphicsCtx.SetPipelineState(pd3d12PSO);

            auto D3D12Topology = TopologyToD3D12Topology(PSODesc.GraphicsPipeline.PrimitiveTopology);
            GraphicsCtx.SetPrimitiveTopology(D3D12Topology);

            if(CommitStates)
            {
                GraphicsCtx.SetStencilRef(m_StencilRef);
                GraphicsCtx.SetBlendFactor(m_BlendFactors);
                CommitRenderTargets();
                CommitViewports();
            }

            if(CommitStates || CommitScissor)
            {
                CommitScissorRects(GraphicsCtx, PSODesc.GraphicsPipeline.RasterizerDesc.ScissorEnable);
            }
        }
        m_State.pCommittedResourceCache = nullptr;
    }

    void DeviceContextD3D12Impl::TransitionShaderResources(IPipelineState* pPipelineState, IShaderResourceBinding* pShaderResourceBinding)
    {
        VERIFY_EXPR(pPipelineState != nullptr);

        auto& Ctx = GetCmdContext();
        auto *pPipelineStateD3D12 = ValidatedCast<PipelineStateD3D12Impl>(pPipelineState);
        pPipelineStateD3D12->CommitAndTransitionShaderResources(pShaderResourceBinding, Ctx, false, true);
    }

    void DeviceContextD3D12Impl::CommitShaderResources(IShaderResourceBinding* pShaderResourceBinding, Uint32 Flags)
    {
        if (!DeviceContextBase::CommitShaderResources(pShaderResourceBinding, Flags, 0 /*Dummy*/))
            return;

        auto& Ctx = GetCmdContext();
        m_State.pCommittedResourceCache = m_pPipelineState->CommitAndTransitionShaderResources(pShaderResourceBinding, Ctx, true, (Flags & COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES)!=0);
    }

    void DeviceContextD3D12Impl::SetStencilRef(Uint32 StencilRef)
    {
        if (TDeviceContextBase::SetStencilRef(StencilRef, 0))
        {
            GetCmdContext().AsGraphicsContext().SetStencilRef( m_StencilRef );
        }
    }

    void DeviceContextD3D12Impl::SetBlendFactors(const float* pBlendFactors)
    {
        if (TDeviceContextBase::SetBlendFactors(pBlendFactors, 0))
        {
            GetCmdContext().AsGraphicsContext().SetBlendFactor( m_BlendFactors );
        }
    }

    void DeviceContextD3D12Impl::CommitD3D12IndexBuffer(VALUE_TYPE IndexType, bool TransitionBuffer)
    {
        VERIFY( m_pIndexBuffer != nullptr, "Index buffer is not set up for indexed draw command" );

        D3D12_INDEX_BUFFER_VIEW IBView;
        BufferD3D12Impl *pBuffD3D12 = static_cast<BufferD3D12Impl *>(m_pIndexBuffer.RawPtr());
        IBView.BufferLocation = pBuffD3D12->GetGPUAddress(this) + m_IndexDataStartOffset;
        if( IndexType == VT_UINT32 )
            IBView.Format = DXGI_FORMAT_R32_UINT;
        else
        {
            DEV_CHECK_ERR( IndexType == VT_UINT16, "Unsupported index format. Only R16_UINT and R32_UINT are allowed.");
            IBView.Format = DXGI_FORMAT_R16_UINT;
        }
        // Note that for a dynamic buffer, what we use here is the size of the buffer itself, not the upload heap buffer!
        IBView.SizeInBytes = pBuffD3D12->GetDesc().uiSizeInBytes - m_IndexDataStartOffset;

        // Device context keeps strong reference to bound index buffer.
        // When the buffer is unbound, the reference to the D3D12 resource
        // is added to the context. There is no need to add reference here
        //auto &GraphicsCtx = GetCmdContext().AsGraphicsContext();
        //auto *pd3d12Resource = pBuffD3D12->GetD3D12Buffer();
        //GraphicsCtx.AddReferencedObject(pd3d12Resource);

        bool IsDynamic = pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC;
#ifdef DEVELOPMENT
        if(IsDynamic)
            pBuffD3D12->DvpVerifyDynamicAllocation(this);
#endif
        auto& GraphicsCtx = GetCmdContext().AsGraphicsContext();

        if (TransitionBuffer)
        {
            if (pBuffD3D12->IsInKnownState() && !pBuffD3D12->CheckState(RESOURCE_STATE_INDEX_BUFFER))
                GraphicsCtx.TransitionResource(pBuffD3D12, RESOURCE_STATE_INDEX_BUFFER);
        }
#ifdef DEVELOPMENT
        else
        {
            if (pBuffD3D12->IsInKnownState() && !pBuffD3D12->CheckState(RESOURCE_STATE_INDEX_BUFFER))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBuffD3D12->GetDesc().Name, "' used as index buffer must be in RESOURCE_STATE_INDEX_BUFFER "
                                  "state. Actual buffer state: ", GetResourceStateString(pBuffD3D12->GetState()), 
                                  ". Use DRAW_FLAG_TRANSITION_INDEX_BUFFER flag or explicitly transition the buffer to the required state.");

            }
        }
#endif

        size_t BuffDataStartByteOffset;
        auto *pd3d12Buff = pBuffD3D12->GetD3D12Buffer(BuffDataStartByteOffset, this);

        if( IsDynamic || 
            m_State.CommittedD3D12IndexBuffer          != pd3d12Buff ||
            m_State.CommittedIBFormat                  != IndexType  ||
            m_State.CommittedD3D12IndexDataStartOffset != m_IndexDataStartOffset + BuffDataStartByteOffset)
        {
            m_State.CommittedD3D12IndexBuffer = pd3d12Buff;
            m_State.CommittedIBFormat = IndexType;
            m_State.CommittedD3D12IndexDataStartOffset = m_IndexDataStartOffset + static_cast<Uint32>(BuffDataStartByteOffset);
            GraphicsCtx.SetIndexBuffer( IBView );
        }
        
        // GPU virtual address of a dynamic index buffer can change every time
        // a draw command is invoked
        m_State.bCommittedD3D12IBUpToDate = !IsDynamic;
    }

    void DeviceContextD3D12Impl::TransitionD3D12VertexBuffers(GraphicsContext& GraphCtx)
    {
        for( Uint32 Buff = 0; Buff < m_NumVertexStreams; ++Buff )
        {
            auto& CurrStream = m_VertexStreams[Buff];
            auto* pBufferD3D12 = CurrStream.pBuffer.RawPtr();
            if (pBufferD3D12 != nullptr && pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_VERTEX_BUFFER))
                GraphCtx.TransitionResource(pBufferD3D12, RESOURCE_STATE_VERTEX_BUFFER);
        }
    }

    void DeviceContextD3D12Impl::CommitD3D12VertexBuffers(GraphicsContext& GraphCtx, bool TransitionBuffers)
    {
        // Do not initialize array with zeroes for performance reasons
        D3D12_VERTEX_BUFFER_VIEW VBViews[MaxBufferSlots];// = {}
        VERIFY( m_NumVertexStreams <= MaxBufferSlots, "Too many buffers are being set" );
        const auto *Strides = m_pPipelineState->GetBufferStrides();
        DEV_CHECK_ERR( m_NumVertexStreams >= m_pPipelineState->GetNumBufferSlotsUsed(), "Currently bound pipeline state '", m_pPipelineState->GetDesc().Name, "' expects ", m_pPipelineState->GetNumBufferSlotsUsed(), " input buffer slots, but only ", m_NumVertexStreams, " is bound");
        bool DynamicBufferPresent = false;
        for( UINT Buff = 0; Buff < m_NumVertexStreams; ++Buff )
        {
            auto& CurrStream = m_VertexStreams[Buff];
            auto& VBView = VBViews[Buff];
            if (auto* pBufferD3D12 = CurrStream.pBuffer.RawPtr())
            {
                if (pBufferD3D12->GetDesc().Usage == USAGE_DYNAMIC)
                {
                    DynamicBufferPresent = true;
#ifdef DEVELOPMENT
                    pBufferD3D12->DvpVerifyDynamicAllocation(this);
#endif
                }

                if (TransitionBuffers)
                {
                    if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_VERTEX_BUFFER))
                        GraphCtx.TransitionResource(pBufferD3D12, RESOURCE_STATE_VERTEX_BUFFER);
                }
#ifdef DEVELOPMENT
                else
                {
                    if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_VERTEX_BUFFER))
                    {
                        LOG_ERROR_MESSAGE("Buffer '", pBufferD3D12->GetDesc().Name, "' used as vertex buffer at slot ", Buff, " must be in "
                                          "RESOURCE_STATE_VERTEX_BUFFER state. Actual buffer state: ", GetResourceStateString(pBufferD3D12->GetState()), 
                                          ". Use DRAW_FLAG_TRANSITION_VERTEX_BUFFERS flag or explicitly transition the buffer to the required state.");
                    }
                }
#endif
            
                // Device context keeps strong references to all vertex buffers.
                // When a buffer is unbound, a reference to D3D12 resource is added to the context,
                // so there is no need to reference the resource here
                //GraphicsCtx.AddReferencedObject(pd3d12Resource);

                VBView.BufferLocation = pBufferD3D12->GetGPUAddress(this) + CurrStream.Offset;
                VBView.StrideInBytes = Strides[Buff];
                // Note that for a dynamic buffer, what we use here is the size of the buffer itself, not the upload heap buffer!
                VBView.SizeInBytes = pBufferD3D12->GetDesc().uiSizeInBytes - CurrStream.Offset;
            }
            else
            {
                VBView = D3D12_VERTEX_BUFFER_VIEW{};
            }
        }

        GraphCtx.FlushResourceBarriers();
        GraphCtx.SetVertexBuffers( 0, m_NumVertexStreams, VBViews );

        // GPU virtual address of a dynamic vertex buffer can change every time
        // a draw command is invoked
        m_State.bCommittedD3D12VBsUpToDate = !DynamicBufferPresent;
    }

    void DeviceContextD3D12Impl::Draw( DrawAttribs& drawAttribs )
    {
#ifdef DEVELOPMENT
        if (!DvpVerifyDrawArguments(drawAttribs))
            return;
#endif

        auto& GraphCtx = GetCmdContext().AsGraphicsContext();
        if( drawAttribs.IsIndexed )
        {
            if( m_State.CommittedIBFormat != drawAttribs.IndexType )
                m_State.bCommittedD3D12IBUpToDate = false;

            bool TransitionIndexBuffer = (drawAttribs.Flags & DRAW_FLAG_TRANSITION_INDEX_BUFFER) != 0;
            if (m_State.bCommittedD3D12IBUpToDate)
            {
                BufferD3D12Impl *pBuffD3D12 = static_cast<BufferD3D12Impl *>(m_pIndexBuffer.RawPtr());
                if(TransitionIndexBuffer)
                {
                    if (pBuffD3D12->IsInKnownState() && !pBuffD3D12->CheckState(RESOURCE_STATE_INDEX_BUFFER))
                        GraphCtx.TransitionResource(pBuffD3D12, RESOURCE_STATE_INDEX_BUFFER);
                }
#ifdef DEVELOPMENT
                else
                {
                    if (pBuffD3D12->IsInKnownState() && !pBuffD3D12->CheckState(RESOURCE_STATE_INDEX_BUFFER))
                    {
                        LOG_ERROR_MESSAGE("Buffer '", pBuffD3D12->GetDesc().Name, "' used as index buffer must be in RESOURCE_STATE_INDEX_BUFFER "
                                           "state. Actual buffer state: ", GetResourceStateString(pBuffD3D12->GetState()), 
                                           ". Use DRAW_FLAG_TRANSITION_INDEX_BUFFER flag or explicitly transition the buffer to the required state.");
                    }
                }
#endif
            }
            else
            {
                CommitD3D12IndexBuffer(drawAttribs.IndexType, TransitionIndexBuffer);
            }
        }

        bool TransitionVertexBuffers = (drawAttribs.Flags & DRAW_FLAG_TRANSITION_VERTEX_BUFFERS) != 0;
        if (m_State.bCommittedD3D12VBsUpToDate)
        {
            if (TransitionVertexBuffers)
            {
                TransitionD3D12VertexBuffers(GraphCtx);
            }
#ifdef DEVELOPMENT
            else
            {
                for( Uint32 Buff = 0; Buff < m_NumVertexStreams; ++Buff )
                {
                    auto& CurrStream = m_VertexStreams[Buff];
                    auto* pBufferD3D12 = CurrStream.pBuffer.RawPtr();
                    if (pBufferD3D12 != nullptr && pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_VERTEX_BUFFER))
                    {
                        LOG_ERROR_MESSAGE("Buffer '", pBufferD3D12->GetDesc().Name, "' used as vertex buffer at slot ", Buff, " must be in "
                                          "RESOURCE_STATE_VERTEX_BUFFER state. Actual buffer state: ", GetResourceStateString(pBufferD3D12->GetState()), 
                                          ". Use DRAW_FLAG_TRANSITION_VERTEX_BUFFERS flag or explicitly transition the buffer to the required state.");
                    }
                }
            }
#endif
        }
        else
        {
            CommitD3D12VertexBuffers(GraphCtx, TransitionVertexBuffers);
        }

        GraphCtx.SetRootSignature( m_pPipelineState->GetD3D12RootSignature() );

        if (m_State.pCommittedResourceCache != nullptr)
        {
            m_pPipelineState->GetRootSignature().CommitRootViews(*m_State.pCommittedResourceCache, GraphCtx, false, this);
        }
#ifdef _DEBUG
        else
        {
            if( m_pPipelineState->dbgContainsShaderResources() )
                LOG_ERROR_MESSAGE("Pipeline state '", m_pPipelineState->GetDesc().Name, "' contains shader resources, but IDeviceContext::CommitShaderResources() was not called with non-null SRB" );
        }
#endif
        

        auto *pIndirectDrawAttribsD3D12 = ValidatedCast<BufferD3D12Impl>(drawAttribs.pIndirectDrawAttribs);
        if (pIndirectDrawAttribsD3D12 != nullptr)
        {
#ifdef DEVELOPMENT
            if (pIndirectDrawAttribsD3D12->GetDesc().Usage == USAGE_DYNAMIC)
                pIndirectDrawAttribsD3D12->DvpVerifyDynamicAllocation(this);
#endif

            if (drawAttribs.Flags & DRAW_FLAG_TRANSITION_INDIRECT_ARGS_BUFFER)
            {
                if (pIndirectDrawAttribsD3D12->IsInKnownState() && !pIndirectDrawAttribsD3D12->CheckState(RESOURCE_STATE_INDIRECT_ARGUMENT))
                    GraphCtx.TransitionResource(pIndirectDrawAttribsD3D12, RESOURCE_STATE_INDIRECT_ARGUMENT);
            }
#ifdef DEVELOPMENT
            else
            {
                if (pIndirectDrawAttribsD3D12->IsInKnownState() && !pIndirectDrawAttribsD3D12->CheckState(RESOURCE_STATE_INDIRECT_ARGUMENT))
                {
                    LOG_ERROR_MESSAGE("Buffer '", pIndirectDrawAttribsD3D12->GetDesc().Name, "' used as indirect draw arguments buffer must be in RESOURCE_STATE_INDIRECT_ARGUMENT "
                                       "state. Actual buffer state: ", GetResourceStateString(pIndirectDrawAttribsD3D12->GetState()), 
                                       ". Use DRAW_FLAG_TRANSITION_INDIRECT_ARGS_BUFFER flag or explicitly transition the buffer to the required state.");
                }
            }
#endif

            size_t BuffDataStartByteOffset;
            ID3D12Resource *pd3d12ArgsBuff = pIndirectDrawAttribsD3D12->GetD3D12Buffer(BuffDataStartByteOffset, this);
            GraphCtx.ExecuteIndirect(drawAttribs.IsIndexed ? m_pDrawIndexedIndirectSignature : m_pDrawIndirectSignature, pd3d12ArgsBuff, drawAttribs.IndirectDrawArgsOffset + BuffDataStartByteOffset);
        }
        else
        {
            if( drawAttribs.IsIndexed )
                GraphCtx.DrawIndexed(drawAttribs.NumIndices, drawAttribs.NumInstances, drawAttribs.FirstIndexLocation, drawAttribs.BaseVertex, drawAttribs.FirstInstanceLocation);
            else
                GraphCtx.Draw(drawAttribs.NumVertices, drawAttribs.NumInstances, drawAttribs.StartVertexLocation, drawAttribs.FirstInstanceLocation );
        }
        ++m_State.NumCommands;
    }

    void DeviceContextD3D12Impl::DispatchCompute( const DispatchComputeAttribs& DispatchAttrs )
    {
#ifdef DEVELOPMENT
        if (!DvpVerifyDispatchArguments(DispatchAttrs))
            return;
#endif

        auto& ComputeCtx = GetCmdContext().AsComputeContext();
        ComputeCtx.SetRootSignature( m_pPipelineState->GetD3D12RootSignature() );
      
        if (m_State.pCommittedResourceCache != nullptr)
        {
            m_pPipelineState->GetRootSignature().CommitRootViews(*m_State.pCommittedResourceCache, ComputeCtx, true, this);
        }
#ifdef _DEBUG
        else
        {
            if( m_pPipelineState->dbgContainsShaderResources() )
                LOG_ERROR_MESSAGE("Pipeline state '", m_pPipelineState->GetDesc().Name, "' contains shader resources, but IDeviceContext::CommitShaderResources() was not called with non-null SRB" );
        }
#endif

        if( DispatchAttrs.pIndirectDispatchAttribs )
        {
            if( auto *pBufferD3D12 = ValidatedCast<BufferD3D12Impl>(DispatchAttrs.pIndirectDispatchAttribs) )
            {
#ifdef DEVELOPMENT
                if(pBufferD3D12->GetDesc().Usage == USAGE_DYNAMIC)
                    pBufferD3D12->DvpVerifyDynamicAllocation(this);
#endif

                if (DispatchAttrs.Flags & DISPATCH_FLAG_TRANSITION_INDIRECT_ARGS_BUFFER)
                {
                    if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_INDIRECT_ARGUMENT))
                        ComputeCtx.TransitionResource(pBufferD3D12, RESOURCE_STATE_INDIRECT_ARGUMENT);
                }
#ifdef DEVELOPMENT
                else
                {
                    if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_INDIRECT_ARGUMENT))
                    {
                        LOG_ERROR_MESSAGE("Buffer '", pBufferD3D12->GetDesc().Name, "' used as indirect dispatch arguments buffer must be in RESOURCE_STATE_INDIRECT_ARGUMENT "
                                           "state. Actual buffer state: ", GetResourceStateString(pBufferD3D12->GetState()), 
                                           ". Use DISPATCH_FLAG_TRANSITION_INDIRECT_ARGS_BUFFER flag or explicitly transition the buffer to the required state.");
                    }
                }
#endif

                size_t BuffDataStartByteOffset;
                ID3D12Resource *pd3d12ArgsBuff = pBufferD3D12->GetD3D12Buffer(BuffDataStartByteOffset, this);
                ComputeCtx.ExecuteIndirect(m_pDispatchIndirectSignature, pd3d12ArgsBuff, DispatchAttrs.DispatchArgsByteOffset + BuffDataStartByteOffset);
            }
            else
            {
                LOG_ERROR_MESSAGE("Valid pIndirectDrawAttribs must be provided for indirect dispatch command");
            }
        }
        else
            ComputeCtx.Dispatch(DispatchAttrs.ThreadGroupCountX, DispatchAttrs.ThreadGroupCountY, DispatchAttrs.ThreadGroupCountZ);
        ++m_State.NumCommands;
    }

    void DeviceContextD3D12Impl::ClearDepthStencil( ITextureView* pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil )
    {
        ITextureViewD3D12 *pDSVD3D12 = nullptr;
        if( pView != nullptr )
        {
            pDSVD3D12 = ValidatedCast<ITextureViewD3D12>(pView);
#ifdef _DEBUG
            const auto& ViewDesc = pDSVD3D12->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Incorrect view type: depth stencil is expected" );
#endif
        }
        else
        {
            if (m_pSwapChain)
            {
                pDSVD3D12 = ValidatedCast<ITextureViewD3D12>(m_pSwapChain.RawPtr<ISwapChainD3D12>()->GetDepthBufferDSV());
            }
            else
            {
                LOG_ERROR("Failed to clear default depth stencil buffer: swap chain is not initialized in the device context");
                return;
            }
        }
        D3D12_CLEAR_FLAGS d3d12ClearFlags = (D3D12_CLEAR_FLAGS)0;
        if( ClearFlags & CLEAR_DEPTH_FLAG )   d3d12ClearFlags |= D3D12_CLEAR_FLAG_DEPTH;
        if( ClearFlags & CLEAR_STENCIL_FLAG ) d3d12ClearFlags |= D3D12_CLEAR_FLAG_STENCIL;
        // The full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied??
        GetCmdContext().AsGraphicsContext().ClearDepthStencil( pDSVD3D12, d3d12ClearFlags, fDepth, Stencil );
        ++m_State.NumCommands;
    }

    void DeviceContextD3D12Impl::ClearRenderTarget( ITextureView* pView, const float* RGBA )
    {
        ITextureViewD3D12 *pd3d12RTV = nullptr;
        if( pView != nullptr )
        {
#ifdef _DEBUG
            const auto& ViewDesc = pView->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET, "Incorrect view type: render target is expected" );
#endif
            pd3d12RTV = ValidatedCast<ITextureViewD3D12>(pView);
        }
        else
        {
            if (m_pSwapChain)
            {
                pd3d12RTV = ValidatedCast<ITextureViewD3D12>(m_pSwapChain.RawPtr<ISwapChainD3D12>()->GetCurrentBackBufferRTV());
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

        // The full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied??
        GetCmdContext().AsGraphicsContext().ClearRenderTarget( pd3d12RTV, RGBA );
        ++m_State.NumCommands;
    }

    void DeviceContextD3D12Impl::RequestCommandContext(RenderDeviceD3D12Impl* pDeviceD3D12Impl)
    {
        m_CurrCmdCtx = pDeviceD3D12Impl->AllocateCommandContext();
        m_CurrCmdCtx->SetDynamicGPUDescriptorAllocators(m_DynamicGPUDescriptorAllocator);
    }

    void DeviceContextD3D12Impl::Flush(bool RequestNewCmdCtx)
    {
        auto pDeviceD3D12Impl = m_pDevice.RawPtr<RenderDeviceD3D12Impl>();
        if( m_CurrCmdCtx )
        {
            VERIFY(!m_bIsDeferred, "Deferred contexts cannot execute command lists directly");
            if (m_State.NumCommands != 0)
            {
                m_CurrCmdCtx->FlushResourceBarriers();
                pDeviceD3D12Impl->CloseAndExecuteCommandContext(m_CommandQueueId, std::move(m_CurrCmdCtx), true, &m_PendingFences);
                m_PendingFences.clear();
            }
            else
                pDeviceD3D12Impl->DisposeCommandContext(std::move(m_CurrCmdCtx));
        }

        if(RequestNewCmdCtx)
            RequestCommandContext(pDeviceD3D12Impl);

        m_State = State{};

        m_pPipelineState = nullptr; 
    }

    void DeviceContextD3D12Impl::Flush()
    {
        if (m_bIsDeferred)
        {
            LOG_ERROR_MESSAGE("Flush() should only be called for immediate contexts");
            return;
        }

        Flush(true);
    }

    void DeviceContextD3D12Impl::FinishFrame()
    {
        if (GetNumCommandsInCtx() != 0)
        {
            LOG_ERROR_MESSAGE(m_bIsDeferred ? 
                "There are outstanding commands in deferred device context #", m_ContextId, " when finishing the frame. This is an error and may cause unpredicted behaviour. Close all deferred contexts and execute them before finishing the frame" :
                "There are outstanding commands in the immediate device context when finishing the frame. This is an error and may cause unpredicted behaviour. Call Flush() to submit all commands for execution before finishing the frame");
        }

        VERIFY_EXPR(m_bIsDeferred || m_SubmittedBuffersCmdQueueMask == (Uint64{1}<<m_CommandQueueId));

        // Released pages are returned to the global dynamic memory manager hosted by render device.
        m_DynamicHeap.ReleaseAllocatedPages(m_SubmittedBuffersCmdQueueMask);
        
        // Dynamic GPU descriptor allocations are returned to the global GPU descriptor heap
        // hosted by the render device.
        for(size_t i=0; i < _countof(m_DynamicGPUDescriptorAllocator); ++i)
            m_DynamicGPUDescriptorAllocator[i].ReleaseAllocations(m_SubmittedBuffersCmdQueueMask);

        EndFrame(*m_pDevice.RawPtr<RenderDeviceD3D12Impl>());
    }

    void DeviceContextD3D12Impl::SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer** ppBuffers, Uint32* pOffsets, Uint32 Flags )
    {
        TDeviceContextBase::SetVertexBuffers( StartSlot, NumBuffersSet, ppBuffers, pOffsets, Flags );
        m_State.bCommittedD3D12VBsUpToDate = false;
    }

    void DeviceContextD3D12Impl::InvalidateState()
    {
        if (m_State.NumCommands != 0)
            LOG_WARNING_MESSAGE("Invalidating context that has outstanding commands in it. Call Flush() to submit commands for execution");

        TDeviceContextBase::InvalidateState();
        m_State = State{};
    }

    void DeviceContextD3D12Impl::SetIndexBuffer( IBuffer* pIndexBuffer, Uint32 ByteOffset )
    {
        TDeviceContextBase::SetIndexBuffer( pIndexBuffer, ByteOffset );
        m_State.bCommittedD3D12IBUpToDate = false;
    }

    void DeviceContextD3D12Impl::CommitViewports()
    {
        static_assert(MaxViewports >= D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, "MaxViewports constant must be greater than D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE");
        D3D12_VIEWPORT d3d12Viewports[MaxViewports]; // Do not waste time initializing array to zero
        
        for( Uint32 vp = 0; vp < m_NumViewports; ++vp )
        {
            d3d12Viewports[vp].TopLeftX = m_Viewports[vp].TopLeftX;
            d3d12Viewports[vp].TopLeftY = m_Viewports[vp].TopLeftY;
            d3d12Viewports[vp].Width    = m_Viewports[vp].Width;
            d3d12Viewports[vp].Height   = m_Viewports[vp].Height;
            d3d12Viewports[vp].MinDepth = m_Viewports[vp].MinDepth;
            d3d12Viewports[vp].MaxDepth = m_Viewports[vp].MaxDepth;
        }
        // All viewports must be set atomically as one operation. 
        // Any viewports not defined by the call are disabled.
        GetCmdContext().AsGraphicsContext().SetViewports( m_NumViewports, d3d12Viewports );
    }

    void DeviceContextD3D12Impl::SetViewports( Uint32 NumViewports, const Viewport* pViewports, Uint32 RTWidth, Uint32 RTHeight  )
    {
        static_assert(MaxViewports >= D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, "MaxViewports constant must be greater than D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE");
        TDeviceContextBase::SetViewports( NumViewports, pViewports, RTWidth, RTHeight );
        VERIFY( NumViewports == m_NumViewports, "Unexpected number of viewports" );

        CommitViewports();
    }


    constexpr LONG MaxD3D12TexDim = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    constexpr Uint32 MaxD3D12ScissorRects = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    static constexpr RECT MaxD3D12TexSizeRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] =
    {
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },

        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },

        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },

        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim },
        { 0,0, MaxD3D12TexDim,MaxD3D12TexDim }
    };

    void DeviceContextD3D12Impl::CommitScissorRects(GraphicsContext& GraphCtx, bool ScissorEnable)
    { 
        if (ScissorEnable)
        {
            // Commit currently set scissor rectangles
            D3D12_RECT d3d12ScissorRects[MaxD3D12ScissorRects]; // Do not waste time initializing array with zeroes
            for (Uint32 sr = 0; sr < m_NumScissorRects; ++sr)
            {
                d3d12ScissorRects[sr].left   = m_ScissorRects[sr].left;
                d3d12ScissorRects[sr].top    = m_ScissorRects[sr].top;
                d3d12ScissorRects[sr].right  = m_ScissorRects[sr].right;
                d3d12ScissorRects[sr].bottom = m_ScissorRects[sr].bottom;
            }
            GraphCtx.SetScissorRects(m_NumScissorRects, d3d12ScissorRects);
        }
        else
        {
            // Disable scissor rectangles
            static_assert(_countof(MaxD3D12TexSizeRects) == MaxD3D12ScissorRects, "Unexpected array size");
            GraphCtx.SetScissorRects(MaxD3D12ScissorRects, MaxD3D12TexSizeRects);
        }
    }

    void DeviceContextD3D12Impl::SetScissorRects( Uint32 NumRects, const Rect* pRects, Uint32 RTWidth, Uint32 RTHeight  )
    {
        const Uint32 MaxScissorRects = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        VERIFY( NumRects < MaxScissorRects, "Too many scissor rects are being set" );
        NumRects = std::min( NumRects, MaxScissorRects );

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
                auto& Ctx = GetCmdContext().AsGraphicsContext();
                CommitScissorRects(Ctx, true);
            }
        }
    }


    void DeviceContextD3D12Impl::CommitRenderTargets()
    {
        const Uint32 MaxD3D12RTs = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
        Uint32 NumRenderTargets = m_NumBoundRenderTargets;
        VERIFY( NumRenderTargets <= MaxD3D12RTs, "D3D12 only allows 8 simultaneous render targets" );
        NumRenderTargets = std::min( MaxD3D12RTs, NumRenderTargets );

        ITextureViewD3D12 *ppRTVs[MaxD3D12RTs]; // Do not initialize with zeroes!
        ITextureViewD3D12 *pDSV = nullptr;
        if( m_IsDefaultFramebufferBound )
        {
            if (m_pSwapChain)
            {
                NumRenderTargets = 1;
                auto *pSwapChainD3D12 = m_pSwapChain.RawPtr<ISwapChainD3D12>();
                ppRTVs[0] = ValidatedCast<ITextureViewD3D12>(pSwapChainD3D12->GetCurrentBackBufferRTV());
                pDSV = ValidatedCast<ITextureViewD3D12>(pSwapChainD3D12->GetDepthBufferDSV());
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
                ppRTVs[rt] = m_pBoundRenderTargets[rt].RawPtr<ITextureViewD3D12>();
            pDSV = m_pBoundDepthStencil.RawPtr<ITextureViewD3D12>();
        }
        GetCmdContext().AsGraphicsContext().SetRenderTargets(NumRenderTargets, ppRTVs, pDSV);
    }

    void DeviceContextD3D12Impl::SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )
    {
        if( TDeviceContextBase::SetRenderTargets( NumRenderTargets, ppRenderTargets, pDepthStencil ) )
        {
            CommitRenderTargets();

            // Set the viewport to match the render target size
            SetViewports(1, nullptr, 0, 0);
        }
    }
   
    D3D12DynamicAllocation DeviceContextD3D12Impl::AllocateDynamicSpace(size_t NumBytes, size_t Alignment)
    {
        return m_DynamicHeap.Allocate(NumBytes, Alignment, m_ContextFrameNumber);
    }

    void DeviceContextD3D12Impl::UpdateBufferRegion(BufferD3D12Impl* pBuffD3D12, D3D12DynamicAllocation& Allocation, Uint64 DstOffset, Uint64 NumBytes)
    {
        auto& CmdCtx = GetCmdContext();
        VERIFY_EXPR( static_cast<size_t>(NumBytes) == NumBytes );
        if (pBuffD3D12->IsInKnownState() && !pBuffD3D12->CheckState(RESOURCE_STATE_COPY_DEST))
            CmdCtx.TransitionResource(pBuffD3D12, RESOURCE_STATE_COPY_DEST);
        size_t DstBuffDataStartByteOffset;
        auto *pd3d12Buff = pBuffD3D12->GetD3D12Buffer(DstBuffDataStartByteOffset, this);
        VERIFY(DstBuffDataStartByteOffset == 0, "Dst buffer must not be suballocated");
        CmdCtx.FlushResourceBarriers();
        CmdCtx.GetCommandList()->CopyBufferRegion( pd3d12Buff, DstOffset + DstBuffDataStartByteOffset, Allocation.pBuffer, Allocation.Offset, NumBytes);
        ++m_State.NumCommands;
    }

    void DeviceContextD3D12Impl::UpdateBuffer(IBuffer* pBuffer, Uint32 Offset, Uint32 Size, const PVoid pData)
    {
        TDeviceContextBase::UpdateBuffer(pBuffer, Offset, Size, pData);

        // We must use cmd context from the device context provided, otherwise there will
        // be resource barrier issues in the cmd list in the device context
        auto* pBuffD3D12 = ValidatedCast<BufferD3D12Impl>(pBuffer);
        VERIFY(pBuffD3D12->GetDesc().Usage != USAGE_DYNAMIC, "Dynamic buffers must be updated via Map()");
        constexpr size_t DefaultAlginment = 16;
        auto TmpSpace = m_DynamicHeap.Allocate(Size, DefaultAlginment, m_ContextFrameNumber);
	    memcpy(TmpSpace.CPUAddress, pData, Size);
        UpdateBufferRegion(pBuffD3D12, TmpSpace, Offset, Size);
    }

    void DeviceContextD3D12Impl::CopyBuffer(IBuffer* pSrcBuffer, Uint32 SrcOffset, IBuffer* pDstBuffer, Uint32 DstOffset, Uint32 Size)
    {
        TDeviceContextBase::CopyBuffer(pSrcBuffer, SrcOffset, pDstBuffer, DstOffset, Size);

        auto* pSrcBuffD3D12 = ValidatedCast<BufferD3D12Impl>(pSrcBuffer);
        auto* pDstBuffD3D12 = ValidatedCast<BufferD3D12Impl>(pDstBuffer);

        VERIFY(pDstBuffD3D12->GetDesc().Usage != USAGE_DYNAMIC, "Dynamic buffers cannot be copy destinations");

        auto& CmdCtx = GetCmdContext();
        if (pSrcBuffD3D12->IsInKnownState() && !pSrcBuffD3D12->CheckState(RESOURCE_STATE_COPY_SOURCE))
            CmdCtx.TransitionResource(pSrcBuffD3D12, RESOURCE_STATE_COPY_SOURCE);
        if (pDstBuffD3D12->IsInKnownState() && !pDstBuffD3D12->CheckState(RESOURCE_STATE_COPY_DEST))
            CmdCtx.TransitionResource(pDstBuffD3D12, RESOURCE_STATE_COPY_DEST);
        size_t DstDataStartByteOffset;
        auto* pd3d12DstBuff = pDstBuffD3D12->GetD3D12Buffer(DstDataStartByteOffset, this);
        VERIFY(DstDataStartByteOffset == 0, "Dst buffer must not be suballocated");

        size_t SrcDataStartByteOffset;
        auto* pd3d12SrcBuff = pSrcBuffD3D12->GetD3D12Buffer(SrcDataStartByteOffset, this);
        CmdCtx.FlushResourceBarriers();
        CmdCtx.GetCommandList()->CopyBufferRegion( pd3d12DstBuff, DstOffset + DstDataStartByteOffset, pd3d12SrcBuff, SrcOffset+SrcDataStartByteOffset, Size);
        ++m_State.NumCommands;
    }

    void DeviceContextD3D12Impl::UpdateTexture(ITexture* pTexture, Uint32 MipLevel, Uint32 Slice, const Box& DstBox, const TextureSubResData& SubresData)
    {
        TDeviceContextBase::UpdateTexture( pTexture, MipLevel, Slice, DstBox, SubresData );

        auto* pTexD3D12 = ValidatedCast<TextureD3D12Impl>(pTexture);
        const auto& Desc = pTexD3D12->GetDesc();
        // OpenGL backend uses UpdateData() to initialize textures, so we can't check the usage in ValidateUpdateTextureParams()
        DEV_CHECK_ERR( Desc.Usage == USAGE_DEFAULT, "Only USAGE_DEFAULT textures should be updated with UpdateData()" );

        Box BlockAlignedBox;
        const auto& FmtAttribs = GetTextureFormatAttribs(Desc.Format);
        const Box* pBox = nullptr;
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
        {
            // Align update region by the compressed block size

            VERIFY( (DstBox.MinX % FmtAttribs.BlockWidth) == 0, "Update region min X coordinate (", DstBox.MinX, ") must be multiple of a compressed block width (", Uint32{FmtAttribs.BlockWidth}, ")");
            BlockAlignedBox.MinX = DstBox.MinX;
            VERIFY( (FmtAttribs.BlockWidth & (FmtAttribs.BlockWidth-1)) == 0, "Compressed block width (", Uint32{FmtAttribs.BlockWidth}, ") is expected to be power of 2");
            BlockAlignedBox.MaxX = (DstBox.MaxX + FmtAttribs.BlockWidth-1) & ~(FmtAttribs.BlockWidth-1);
 
            VERIFY( (DstBox.MinY % FmtAttribs.BlockHeight) == 0, "Update region min Y coordinate (", DstBox.MinY, ") must be multiple of a compressed block height (", Uint32{FmtAttribs.BlockHeight}, ")");
            BlockAlignedBox.MinY = DstBox.MinY;
            VERIFY( (FmtAttribs.BlockHeight & (FmtAttribs.BlockHeight-1)) == 0, "Compressed block height (", Uint32{FmtAttribs.BlockHeight}, ") is expected to be power of 2");
            BlockAlignedBox.MaxY = (DstBox.MaxY + FmtAttribs.BlockHeight-1) & ~(FmtAttribs.BlockHeight-1);

            BlockAlignedBox.MinZ = DstBox.MinZ;
            BlockAlignedBox.MaxZ = DstBox.MaxZ;

            pBox = &BlockAlignedBox;
        }
        else
        {
            pBox = &DstBox;
        }
        auto DstSubResIndex = D3D12CalcSubresource(MipLevel, Slice, 0, Desc.MipLevels, Desc.ArraySize);
        if (SubresData.pSrcBuffer == nullptr)
            UpdateTextureRegion(SubresData.pData, SubresData.Stride, SubresData.DepthStride, *pTexD3D12, DstSubResIndex, *pBox);
        else
            CopyTextureRegion(SubresData.pSrcBuffer, 0, SubresData.Stride, SubresData.DepthStride, *pTexD3D12, DstSubResIndex, *pBox);
    }

    void DeviceContextD3D12Impl::CopyTexture(ITexture*  pSrcTexture, 
                                             Uint32     SrcMipLevel,
                                             Uint32     SrcSlice,
                                             const Box* pSrcBox,
                                             ITexture*  pDstTexture, 
                                             Uint32     DstMipLevel,
                                             Uint32     DstSlice,
                                             Uint32     DstX,
                                             Uint32     DstY,
                                             Uint32     DstZ)
    {
        TDeviceContextBase::CopyTexture( pSrcTexture, SrcMipLevel, SrcSlice, pSrcBox,
                                         pDstTexture, DstMipLevel, DstSlice, DstX, DstY, DstZ );

        auto* pSrcTexD3D12 = ValidatedCast<TextureD3D12Impl>( pSrcTexture );
        auto* pDstTexD3D12 = ValidatedCast<TextureD3D12Impl>( pDstTexture );
        const auto& SrcTexDesc = pSrcTexD3D12->GetDesc();
        const auto& DstTexDesc = pDstTexD3D12->GetDesc();

        D3D12_BOX D3D12SrcBox, *pD3D12SrcBox = nullptr;
        if( pSrcBox )
        {
            D3D12SrcBox.left    = pSrcBox->MinX;
            D3D12SrcBox.right   = pSrcBox->MaxX;
            D3D12SrcBox.top     = pSrcBox->MinY;
            D3D12SrcBox.bottom  = pSrcBox->MaxY;
            D3D12SrcBox.front   = pSrcBox->MinZ;
            D3D12SrcBox.back    = pSrcBox->MaxZ;
            pD3D12SrcBox = &D3D12SrcBox;
        }

        auto DstSubResIndex = D3D12CalcSubresource(DstMipLevel, DstSlice, 0, DstTexDesc.MipLevels, DstTexDesc.ArraySize);
        auto SrcSubResIndex = D3D12CalcSubresource(SrcMipLevel, SrcSlice, 0, SrcTexDesc.MipLevels, SrcTexDesc.ArraySize);
        CopyTextureRegion(pSrcTexD3D12, SrcSubResIndex, pD3D12SrcBox, pDstTexD3D12, DstSubResIndex, DstX, DstY, DstZ);
    }

    void DeviceContextD3D12Impl::CopyTextureRegion(TextureD3D12Impl* pSrcTexture, Uint32 SrcSubResIndex, const D3D12_BOX* pD3D12SrcBox,
                                                   TextureD3D12Impl* pDstTexture, Uint32 DstSubResIndex, Uint32 DstX, Uint32 DstY, Uint32 DstZ)
    {
        auto& CmdCtx = GetCmdContext();
        
        if (pSrcTexture->IsInKnownState() && !pSrcTexture->CheckState(RESOURCE_STATE_COPY_SOURCE))
            CmdCtx.TransitionResource(pSrcTexture, RESOURCE_STATE_COPY_SOURCE);
        if (pDstTexture->IsInKnownState() && !pDstTexture->CheckState(RESOURCE_STATE_COPY_DEST))
            CmdCtx.TransitionResource(pDstTexture, RESOURCE_STATE_COPY_DEST);

        D3D12_TEXTURE_COPY_LOCATION DstLocation = {}, SrcLocation = {};

        DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.pResource = pDstTexture->GetD3D12Resource();
        DstLocation.SubresourceIndex = DstSubResIndex;

        SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        SrcLocation.pResource = pSrcTexture->GetD3D12Resource();
        SrcLocation.SubresourceIndex = SrcSubResIndex;

        CmdCtx.FlushResourceBarriers();
        CmdCtx.GetCommandList()->CopyTextureRegion( &DstLocation, DstX, DstY, DstZ, &SrcLocation, pD3D12SrcBox);
        ++m_State.NumCommands;
    }

    void DeviceContextD3D12Impl::CopyTextureRegion(ID3D12Resource*   pd3d12Buffer,
                                                   Uint32            SrcOffset, 
                                                   Uint32            SrcStride,
                                                   Uint32            SrcDepthStride,
                                                   Uint32            BufferSize,
                                                   TextureD3D12Impl& TextureD3D12,
                                                   Uint32            DstSubResIndex,
                                                   const Box&        DstBox)
    {
        const auto& TexDesc = TextureD3D12.GetDesc();
        auto& CmdCtx = GetCmdContext();
        auto* pCmdList = CmdCtx.GetCommandList();
        bool StateTransitionRequired = TextureD3D12.IsInKnownState() && !TextureD3D12.CheckState(RESOURCE_STATE_COPY_DEST);
        D3D12_RESOURCE_BARRIER BarrierDesc;
        if (StateTransitionRequired)
        {
		    BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		    BarrierDesc.Transition.pResource = TextureD3D12.GetD3D12Resource();
		    BarrierDesc.Transition.Subresource = DstSubResIndex;
		    BarrierDesc.Transition.StateBefore = ResourceStateFlagsToD3D12ResourceStates(TextureD3D12.GetState());
		    BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            pCmdList->ResourceBarrier(1, &BarrierDesc);
        }

        D3D12_TEXTURE_COPY_LOCATION DstLocation;
        DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.pResource = TextureD3D12.GetD3D12Resource();
        DstLocation.SubresourceIndex = static_cast<UINT>(DstSubResIndex);

        D3D12_TEXTURE_COPY_LOCATION SrcLocation;
        SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        SrcLocation.pResource = pd3d12Buffer;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT &Footpring = SrcLocation.PlacedFootprint;
        Footpring.Offset           = SrcOffset;
        Footpring.Footprint.Width  = static_cast<UINT>(DstBox.MaxX - DstBox.MinX);
        Footpring.Footprint.Height = static_cast<UINT>(DstBox.MaxY - DstBox.MinY);
        Footpring.Footprint.Depth  = static_cast<UINT>(DstBox.MaxZ - DstBox.MinZ); // Depth cannot be 0
        Footpring.Footprint.Format = TexFormatToDXGI_Format(TexDesc.Format);

        Footpring.Footprint.RowPitch = static_cast<UINT>(SrcStride);

#ifdef _DEBUG
        {
            const auto& FmtAttribs = GetTextureFormatAttribs(TexDesc.Format);
            const Uint32 RowCount = std::max((Footpring.Footprint.Height/FmtAttribs.BlockHeight), 1u);
            VERIFY(BufferSize >= Footpring.Footprint.RowPitch * RowCount * Footpring.Footprint.Depth, "Buffer is not large enough");
            VERIFY(Footpring.Footprint.Depth == 1 || static_cast<UINT>(SrcDepthStride) == Footpring.Footprint.RowPitch * RowCount, "Depth stride must be equal to the size of 2D plane");
        }
#endif

        D3D12_BOX D3D12SrcBox;
        D3D12SrcBox.left    = 0;
        D3D12SrcBox.right   = Footpring.Footprint.Width;
        D3D12SrcBox.top     = 0;
        D3D12SrcBox.bottom  = Footpring.Footprint.Height;
        D3D12SrcBox.front   = 0;
        D3D12SrcBox.back    = Footpring.Footprint.Depth;
        CmdCtx.GetCommandList()->CopyTextureRegion( &DstLocation, 
            static_cast<UINT>( DstBox.MinX ), 
            static_cast<UINT>( DstBox.MinY ), 
            static_cast<UINT>( DstBox.MinZ ),
            &SrcLocation, &D3D12SrcBox);

        ++m_State.NumCommands;

        if (StateTransitionRequired)
        {
            std::swap(BarrierDesc.Transition.StateBefore, BarrierDesc.Transition.StateAfter);
            pCmdList->ResourceBarrier(1, &BarrierDesc);
        }
    }

    void DeviceContextD3D12Impl::CopyTextureRegion(IBuffer*                pSrcBuffer,
                                                   Uint32                  SrcOffset, 
                                                   Uint32                  SrcStride,
                                                   Uint32                  SrcDepthStride,
                                                   class TextureD3D12Impl& TextureD3D12,
                                                   Uint32                  DstSubResIndex,
                                                   const Box&              DstBox)
    {
        auto* pBufferD3D12 = ValidatedCast<BufferD3D12Impl>(pSrcBuffer);
        auto BufferState = pBufferD3D12->GetState();
        if (pBufferD3D12->GetDesc().Usage == USAGE_DYNAMIC)
            DEV_CHECK_ERR(BufferState == RESOURCE_STATE_GENERIC_READ, "Dynamic buffer is expected to always be in RESOURCE_STATE_GENERIC_READ state");
        else if (BufferState != RESOURCE_STATE_GENERIC_READ && BufferState != RESOURCE_STATE_UNKNOWN)
            GetCmdContext().TransitionResource(pBufferD3D12, RESOURCE_STATE_GENERIC_READ);
        GetCmdContext().FlushResourceBarriers();
        size_t DataStartByteOffset = 0;
        auto* pd3d12Buffer = pBufferD3D12->GetD3D12Buffer(DataStartByteOffset, this);
        CopyTextureRegion(pd3d12Buffer, static_cast<Uint32>(DataStartByteOffset) + SrcOffset, SrcStride, SrcDepthStride, pBufferD3D12->GetDesc().uiSizeInBytes, TextureD3D12, DstSubResIndex, DstBox);
    }

    DeviceContextD3D12Impl::TextureUploadSpace DeviceContextD3D12Impl::AllocateTextureUploadSpace(TEXTURE_FORMAT TexFmt,
                                                                                                  const Box&     Region)
    {
        TextureUploadSpace UploadSpace;
        VERIFY_EXPR(Region.MaxX > Region.MinX && Region.MaxY > Region.MinY && Region.MaxZ > Region.MinZ);
        auto UpdateRegionWidth  = Region.MaxX - Region.MinX;
        auto UpdateRegionHeight = Region.MaxY - Region.MinY;
        auto UpdateRegionDepth  = Region.MaxZ - Region.MinZ;
        const auto& FmtAttribs = GetTextureFormatAttribs(TexFmt);
        if(FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
        {
            // Box must be aligned by the calling function
            VERIFY_EXPR((UpdateRegionWidth  % FmtAttribs.BlockWidth) == 0);
            VERIFY_EXPR((UpdateRegionHeight % FmtAttribs.BlockHeight) == 0);
            UploadSpace.RowSize  = UpdateRegionWidth / Uint32{FmtAttribs.BlockWidth} * Uint32{FmtAttribs.ComponentSize};
            UploadSpace.RowCount = UpdateRegionHeight / FmtAttribs.BlockHeight;
        }
        else
        {
            UploadSpace.RowSize  = UpdateRegionWidth * Uint32{FmtAttribs.ComponentSize} * Uint32{FmtAttribs.NumComponents};
            UploadSpace.RowCount = UpdateRegionHeight;
        }
        // RowPitch must be a multiple of 256 (aka. D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
        UploadSpace.Stride        = (UploadSpace.RowSize + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1);
        UploadSpace.DepthStride   = UploadSpace.RowCount * UploadSpace.Stride;
        const auto MemorySize     = UpdateRegionDepth * UploadSpace.DepthStride;
        UploadSpace.Allocation    = AllocateDynamicSpace(MemorySize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        UploadSpace.AlignedOffset = (UploadSpace.Allocation.Offset + (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT-1)) & ~(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT-1);
        UploadSpace.Region        = Region;

        return UploadSpace;
    }

    void DeviceContextD3D12Impl::UpdateTextureRegion(const void*       pSrcData,
                                                     Uint32            SrcStride,
                                                     Uint32            SrcDepthStride,
                                                     TextureD3D12Impl& TextureD3D12,
                                                     Uint32            DstSubResIndex,
                                                     const Box&        DstBox)
    {
        const auto& TexDesc = TextureD3D12.GetDesc();
        auto UploadSpace = AllocateTextureUploadSpace(TexDesc.Format, DstBox);
        auto UpdateRegionDepth = DstBox.MaxZ-DstBox.MinZ;
#ifdef _DEBUG
        {
            VERIFY(SrcStride >= UploadSpace.RowSize, "Source data stride (", SrcStride, ") is below the image row size (", UploadSpace.RowSize, ")");
            const Uint32 PlaneSize = SrcStride * UploadSpace.RowCount;
            VERIFY(UpdateRegionDepth == 1 || SrcDepthStride >= PlaneSize, "Source data depth stride (", SrcDepthStride, ") is below the image plane size (", PlaneSize, ")");
        }
#endif
        const auto AlignedOffset = UploadSpace.AlignedOffset;

        for(Uint32 DepthSlice = 0; DepthSlice < UpdateRegionDepth; ++DepthSlice)
        {
            for(Uint32 row = 0; row < UploadSpace.RowCount; ++row)
            {
                const auto* pSrcPtr =
                    reinterpret_cast<const Uint8*>(pSrcData)
                    + row   * SrcStride
                    + DepthSlice * SrcDepthStride;
                auto* pDstPtr =
                    reinterpret_cast<Uint8*>(UploadSpace.Allocation.CPUAddress)
                    + (AlignedOffset - UploadSpace.Allocation.Offset)
                    + row        * UploadSpace.Stride
                    + DepthSlice * UploadSpace.DepthStride;
                
                memcpy(pDstPtr, pSrcPtr, UploadSpace.RowSize);
            }
        }
        CopyTextureRegion(UploadSpace.Allocation.pBuffer,
                          static_cast<Uint32>(AlignedOffset),
                          UploadSpace.Stride,
                          UploadSpace.DepthStride,
                          static_cast<Uint32>(UploadSpace.Allocation.Size - (AlignedOffset - UploadSpace.Allocation.Offset)),
                          TextureD3D12,
                          DstSubResIndex,
                          DstBox);
    }

    void DeviceContextD3D12Impl::MapTextureSubresource( ITexture*                 pTexture,
                                                        Uint32                    MipLevel,
                                                        Uint32                    ArraySlice,
                                                        MAP_TYPE                  MapType,
                                                        Uint32                    MapFlags,
                                                        const Box*                pMapRegion,
                                                        MappedTextureSubresource& MappedData )
    {
        TDeviceContextBase::MapTextureSubresource(pTexture, MipLevel, ArraySlice, MapType, MapFlags, pMapRegion, MappedData);

        if (MapType != MAP_WRITE)
        {
            LOG_ERROR("Textures can currently only be mapped for writing in D3D12 backend");
            MappedData = MappedTextureSubresource{};
            return;
        }

        if( (MapFlags & (MAP_FLAG_DISCARD | MAP_FLAG_DO_NOT_SYNCHRONIZE)) != 0 )
            LOG_WARNING_MESSAGE_ONCE("Mapping textures with flags MAP_FLAG_DISCARD or MAP_FLAG_DO_NOT_SYNCHRONIZE has no effect in D3D12 backend");

        auto& TextureD3D12 = *ValidatedCast<TextureD3D12Impl>(pTexture);
        const auto& TexDesc = TextureD3D12.GetDesc();

        Box FullExtentBox;
        if (pMapRegion == nullptr)
        {
            FullExtentBox.MaxX = std::max(TexDesc.Width  >> MipLevel, 1u);
            FullExtentBox.MaxY = std::max(TexDesc.Height >> MipLevel, 1u);
            if (TexDesc.Type == RESOURCE_DIM_TEX_3D)
                FullExtentBox.MaxZ = std::max(TexDesc.Depth >> MipLevel, 1u);
            pMapRegion = &FullExtentBox;
        }

        auto UploadSpace = AllocateTextureUploadSpace(TexDesc.Format, *pMapRegion);
        MappedData.pData       = reinterpret_cast<Uint8*>(UploadSpace.Allocation.CPUAddress) + (UploadSpace.AlignedOffset - UploadSpace.Allocation.Offset);
        MappedData.Stride      = UploadSpace.Stride;
        MappedData.DepthStride = UploadSpace.DepthStride;

        auto Subres = D3D12CalcSubresource(MipLevel, ArraySlice, 0, TexDesc.MipLevels, TexDesc.ArraySize);
        auto it = m_MappedTextures.emplace(MappedTextureKey{&TextureD3D12, Subres}, std::move(UploadSpace));
        if(!it.second)
            LOG_ERROR_MESSAGE("Mip level ", MipLevel, ", slice ", ArraySlice, " of texture '", TexDesc.Name, "' has already been mapped");
    }

    void DeviceContextD3D12Impl::UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice)
    {
        TDeviceContextBase::UnmapTextureSubresource( pTexture, MipLevel, ArraySlice);

        TextureD3D12Impl& TextureD3D12 = *ValidatedCast<TextureD3D12Impl>(pTexture);
        const auto& TexDesc = TextureD3D12.GetDesc();
        auto Subres = D3D12CalcSubresource(MipLevel, ArraySlice, 0, TexDesc.MipLevels, TexDesc.ArraySize);
        auto UploadSpaceIt = m_MappedTextures.find(MappedTextureKey{&TextureD3D12, Subres});
        if(UploadSpaceIt != m_MappedTextures.end())
        {
            auto& UploadSpace = UploadSpaceIt->second;
            CopyTextureRegion(UploadSpace.Allocation.pBuffer,
                              UploadSpace.AlignedOffset,
                              UploadSpace.Stride,
                              UploadSpace.DepthStride,
                              static_cast<Uint32>(UploadSpace.Allocation.Size - (UploadSpace.AlignedOffset - UploadSpace.Allocation.Offset)),
                              TextureD3D12,
                              Subres,
                              UploadSpace.Region);
            m_MappedTextures.erase(UploadSpaceIt);
        }
        else
        {
            LOG_ERROR_MESSAGE("Failed to unmap mip level ", MipLevel, ", slice ", ArraySlice, " of texture '", TexDesc.Name, "'. The texture has either been unmapped already or has not been mapped");
        }
    }


    void DeviceContextD3D12Impl::GenerateMips(TextureViewD3D12Impl* pTexView)
    {
        auto& Ctx = GetCmdContext();
        m_MipsGenerator.GenerateMips(m_pDevice.RawPtr<RenderDeviceD3D12Impl>(), pTexView, Ctx);
        ++m_State.NumCommands;
    }

    void DeviceContextD3D12Impl::FinishCommandList(ICommandList** ppCommandList)
    {
        auto* pDeviceD3D12Impl = m_pDevice.RawPtr<RenderDeviceD3D12Impl>();
        CommandListD3D12Impl* pCmdListD3D12( NEW_RC_OBJ(m_CmdListAllocator, "CommandListD3D12Impl instance", CommandListD3D12Impl)
                                                       (pDeviceD3D12Impl, this, std::move(m_CurrCmdCtx)) );
        pCmdListD3D12->QueryInterface( IID_CommandList, reinterpret_cast<IObject**>(ppCommandList) );
        Flush(true);

        InvalidateState();
    }

    void DeviceContextD3D12Impl::ExecuteCommandList(ICommandList* pCommandList)
    {
        if (m_bIsDeferred)
        {
            LOG_ERROR_MESSAGE("Only immediate context can execute command list");
            return;
        }
        // First execute commands in this context
        Flush(true);

        InvalidateState();

        CommandListD3D12Impl* pCmdListD3D12 = ValidatedCast<CommandListD3D12Impl>(pCommandList);
        VERIFY_EXPR(m_PendingFences.empty());
        RefCntAutoPtr<DeviceContextD3D12Impl> pDeferredCtx;
        auto CmdContext = pCmdListD3D12->Close(pDeferredCtx);
        m_pDevice.RawPtr<RenderDeviceD3D12Impl>()->CloseAndExecuteCommandContext(m_CommandQueueId, std::move(CmdContext), true, nullptr);
        // Set the bit in the deferred context cmd queue mask corresponding to cmd queue of this context
        pDeferredCtx->m_SubmittedBuffersCmdQueueMask |= Uint64{1} << m_CommandQueueId;
    }

    void DeviceContextD3D12Impl::SignalFence(IFence* pFence, Uint64 Value)
    {
        VERIFY(!m_bIsDeferred, "Fence can only be signalled from immediate context");
        m_PendingFences.emplace_back(Value, pFence);
    };

    void DeviceContextD3D12Impl::TransitionResourceStates(Uint32 BarrierCount, StateTransitionDesc* pResourceBarriers)
    {
        auto& CmdCtx = GetCmdContext();
        for(Uint32 i = 0; i < BarrierCount; ++i)
        {
#ifdef DEVELOPMENT
            DvpVerifyStateTransitionDesc(pResourceBarriers[i]);
#endif
            CmdCtx.TransitionResource(pResourceBarriers[i]);
        }
    }

    void DeviceContextD3D12Impl::TransitionTextureState(ITexture *pTexture, D3D12_RESOURCE_STATES State)
    {
        VERIFY_EXPR(pTexture != nullptr);
        DEV_CHECK_ERR(pTexture->GetState() != RESOURCE_STATE_UNKNOWN, "Texture state is unknown");
        auto& CmdCtx = GetCmdContext();
        CmdCtx.TransitionResource(ValidatedCast<ITextureD3D12>(pTexture), D3D12ResourceStatesToResourceStateFlags(State));
    }

    void DeviceContextD3D12Impl::TransitionBufferState(IBuffer *pBuffer, D3D12_RESOURCE_STATES State)
    {
        VERIFY_EXPR(pBuffer != nullptr);
        DEV_CHECK_ERR(pBuffer->GetState() != RESOURCE_STATE_UNKNOWN, "Buffer state is unknown");
        auto& CmdCtx = GetCmdContext();
        CmdCtx.TransitionResource(ValidatedCast<IBufferD3D12>(pBuffer), D3D12ResourceStatesToResourceStateFlags(State));
    }
}
