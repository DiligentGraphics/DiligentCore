/*     Copyright 2015-2017 Egor Yusov
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
#include "RenderDeviceD3D12Impl.h"
#include "DeviceContextD3D12Impl.h"
#include "SwapChainD3D12Impl.h"
#include "PipelineStateD3D12Impl.h"
#include "CommandContext.h"
#include "TextureD3D12Impl.h"
#include "BufferD3D12Impl.h"
#include "D3D12TypeConversions.h"
#include "d3dx12_win.h"
#include "DynamicUploadHeap.h"
#include "CommandListD3D12Impl.h"

namespace Diligent
{

    DeviceContextD3D12Impl::DeviceContextD3D12Impl( IMemoryAllocator &RawMemAllocator, RenderDeviceD3D12Impl *pDeviceD3D12Impl, bool bIsDeferred, const EngineD3D12Attribs &Attribs, Uint32 ContextId) :
        TDeviceContextBase(RawMemAllocator, pDeviceD3D12Impl, bIsDeferred),
        m_pUploadHeap(pDeviceD3D12Impl->RequestUploadHeap() ),
        m_NumCommandsInCurCtx(0),
        m_NumCommandsToFlush(bIsDeferred ? std::numeric_limits<decltype(m_NumCommandsToFlush)>::max() : Attribs.NumCommandsToFlushCmdList),
        m_pCurrCmdCtx(pDeviceD3D12Impl->AllocateCommandContext()),
        m_CommittedIBFormat(VT_UNDEFINED),
        m_CommittedD3D12IndexDataStartOffset(0),
        m_MipsGenerator(pDeviceD3D12Impl->GetD3D12Device()),
        m_CmdListAllocator(GetRawAllocator(), sizeof(CommandListD3D12Impl), 64 ),
        m_ContextId(ContextId)
    {
        m_NumBoundRenderTargets = 0;

        auto *pd3d12Device = pDeviceD3D12Impl->GetD3D12Device();

        D3D12_COMMAND_SIGNATURE_DESC CmdSignatureDesc = {};
        D3D12_INDIRECT_ARGUMENT_DESC IndirectArg = {};
        CmdSignatureDesc.NodeMask = 0;
        CmdSignatureDesc.NumArgumentDescs = 1;
        CmdSignatureDesc.pArgumentDescs = &IndirectArg;

        CmdSignatureDesc.ByteStride = sizeof(UINT)*4;
        IndirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        auto hr = pd3d12Device->CreateCommandSignature(&CmdSignatureDesc, nullptr, __uuidof(m_pDrawIndirectSignature), reinterpret_cast<void**>(static_cast<ID3D12CommandSignature**>(&m_pDrawIndirectSignature)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create indirect draw command signature")

        CmdSignatureDesc.ByteStride = sizeof(UINT)*5;
        IndirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        hr = pd3d12Device->CreateCommandSignature(&CmdSignatureDesc, nullptr, __uuidof(m_pDrawIndexedIndirectSignature), reinterpret_cast<void**>(static_cast<ID3D12CommandSignature**>(&m_pDrawIndexedIndirectSignature)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create draw indexed indirect command signature")

        CmdSignatureDesc.ByteStride = sizeof(UINT)*3;
        IndirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        hr = pd3d12Device->CreateCommandSignature(&CmdSignatureDesc, nullptr, __uuidof(m_pDispatchIndirectSignature), reinterpret_cast<void**>(static_cast<ID3D12CommandSignature**>(&m_pDispatchIndirectSignature)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create dispatch indirect command signature")
    }

    DeviceContextD3D12Impl::~DeviceContextD3D12Impl()
    {
        if(m_bIsDeferred)
            ValidatedCast<RenderDeviceD3D12Impl>(m_pDevice.RawPtr())->DisposeCommandContext(m_pCurrCmdCtx);
        else
            Flush(false);
    }

    //IMPLEMENT_QUERY_INTERFACE( DeviceContextD3D12Impl, IID_DeviceContextD3D12, TDeviceContextBase )
    
    const LONG MaxD3D12TexDim = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    const Uint32 MaxD3D12ScissorRects = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    static const RECT MaxD3D12TexSizeRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = 
    {
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},

        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},

        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},

        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim},
        {0,0, MaxD3D12TexDim,MaxD3D12TexDim}
    };

    void DeviceContextD3D12Impl::SetPipelineState(IPipelineState *pPipelineState)
    {
        if (m_NumCommandsInCurCtx >= m_NumCommandsToFlush)
        {
            Flush(true);
        }

        TDeviceContextBase::SetPipelineState( pPipelineState );
        auto *pPipelineStateD3D12 = ValidatedCast<PipelineStateD3D12Impl>(pPipelineState);

        auto *pCmdCtx = RequestCmdContext();
        const auto &Desc = pPipelineStateD3D12->GetDesc();
        auto *pd3d12PSO = pPipelineStateD3D12->GetD3D12PipelineState();
        if (Desc.IsComputePipeline)
        {
            pCmdCtx->AsComputeContext().SetPipelineState(pd3d12PSO);
        }
        else
        {
            auto &GraphicsCtx = pCmdCtx->AsGraphicsContext();
            GraphicsCtx.SetPipelineState(pd3d12PSO);

            if (Desc.GraphicsPipeline.RasterizerDesc.ScissorEnable)
            {
                // Commit currently set scissor rectangles
                D3D12_RECT d3d12ScissorRects[MaxD3D12ScissorRects]; // Do not waste time initializing array with zeroes
                for( Uint32 sr = 0; sr < m_NumScissorRects; ++sr )
                {
                    d3d12ScissorRects[sr].left   = m_ScissorRects[sr].left;
                    d3d12ScissorRects[sr].top    = m_ScissorRects[sr].top;
                    d3d12ScissorRects[sr].right  = m_ScissorRects[sr].right;
                    d3d12ScissorRects[sr].bottom = m_ScissorRects[sr].bottom;
                }
                GraphicsCtx.SetScissorRects(m_NumScissorRects, d3d12ScissorRects);
            }
            else
            {
                // Disable scissor rectangles
                static_assert(_countof(MaxD3D12TexSizeRects) == MaxD3D12ScissorRects, "Unexpected array size");
                GraphicsCtx.SetScissorRects(MaxD3D12ScissorRects, MaxD3D12TexSizeRects);
            }

            GraphicsCtx.SetStencilRef( m_StencilRef );
            GraphicsCtx.SetBlendFactor( m_BlendFactors );
            m_CommittedD3D12IndexBuffer = nullptr;
            m_CommittedD3D12IndexDataStartOffset = 0;
            m_CommittedIBFormat = VT_UNDEFINED;
            m_bCommittedD3D12VBsUpToDate = false;
            m_bCommittedD3D12IBUpToDate = false;
            RebindRenderTargets();
            CommitViewports();
        }
        m_pCommittedResourceCache = nullptr;
    }

    void DeviceContextD3D12Impl::TransitionShaderResources(IPipelineState *pPipelineState, IShaderResourceBinding *pShaderResourceBinding)
    {
        VERIFY_EXPR(pPipelineState != nullptr);

        auto *pCtx = RequestCmdContext();
        auto *pPipelineStateD3D12 = ValidatedCast<PipelineStateD3D12Impl>(pPipelineState);
        pPipelineStateD3D12->CommitAndTransitionShaderResources(pShaderResourceBinding, *pCtx, false, true);
    }

    void DeviceContextD3D12Impl::CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags)
    {
#ifdef _DEBUG
        if (!m_pPipelineState)
        {
            LOG_ERROR("No pipeline state is bound");
            return;
        }
#endif

        auto *pCtx = RequestCmdContext();
        auto *pPipelineStateD3D12 = ValidatedCast<PipelineStateD3D12Impl>(m_pPipelineState.RawPtr());

        m_pCommittedResourceCache = pPipelineStateD3D12->CommitAndTransitionShaderResources(pShaderResourceBinding, *pCtx, true, (Flags & COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES)!=0);
    }

    void DeviceContextD3D12Impl::SetStencilRef(Uint32 StencilRef)
    {
        if (TDeviceContextBase::SetStencilRef(StencilRef, 0))
        {
            RequestCmdContext()->AsGraphicsContext().SetStencilRef( m_StencilRef );
        }
    }

    void DeviceContextD3D12Impl::SetBlendFactors(const float* pBlendFactors)
    {
        if (TDeviceContextBase::SetBlendFactors(m_BlendFactors, 0))
        {
            RequestCmdContext()->AsGraphicsContext().SetBlendFactor( m_BlendFactors );
        }
    }

    void DeviceContextD3D12Impl::CommitD3D12IndexBuffer(VALUE_TYPE IndexType)
    {
        if( !m_pIndexBuffer )
        {
            LOG_ERROR_MESSAGE( "Index buffer is not set up for indexed draw command" );
            return;
        }

        D3D12_INDEX_BUFFER_VIEW IBView;
        BufferD3D12Impl *pBuffD3D12 = static_cast<BufferD3D12Impl *>(m_pIndexBuffer.RawPtr());
        IBView.BufferLocation = pBuffD3D12->GetGPUAddress(m_ContextId) + m_IndexDataStartOffset;
        if( IndexType == VT_UINT32 )
            IBView.Format = DXGI_FORMAT_R32_UINT;
        else if( IndexType == VT_UINT16 )
            IBView.Format = DXGI_FORMAT_R16_UINT;
        else
        {
            LOG_ERROR_MESSAGE( "Unsupported index format. Only R16_UINT and R32_UINT are allowed." );
            return;
        }
        // Note that for a dynamic buffer, what we use here is the size of the buffer itself, not the upload heap buffer!
        IBView.SizeInBytes = pBuffD3D12->GetDesc().uiSizeInBytes - m_IndexDataStartOffset;

        // Device context keeps strong reference to bound index buffer.
        // When the buffer is unbound, the reference to the D3D12 resource
        // is added to the context. There is no need to add reference here
        //auto &GraphicsCtx = RequestCmdContext()->AsGraphicsContext();
        //auto *pd3d12Resource = pBuffD3D12->GetD3D12Buffer();
        //GraphicsCtx.AddReferencedObject(pd3d12Resource);

#ifdef _DEBUG
        if(pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC)
            pBuffD3D12->DbgVerifyDynamicAllocation(m_ContextId);
#endif
        auto &GraphicsCtx = RequestCmdContext()->AsGraphicsContext();
        // Resource transitioning must always be performed!
        GraphicsCtx.TransitionResource(pBuffD3D12, D3D12_RESOURCE_STATE_INDEX_BUFFER, true);

        size_t BuffDataStartByteOffset;
        auto *pd3d12Buff = pBuffD3D12->GetD3D12Buffer(BuffDataStartByteOffset, m_ContextId);

        if( m_CommittedD3D12IndexBuffer != pd3d12Buff ||
            m_CommittedIBFormat != IndexType ||
            m_CommittedD3D12IndexDataStartOffset != m_IndexDataStartOffset + BuffDataStartByteOffset)
        {
            m_CommittedD3D12IndexBuffer = pd3d12Buff;
            m_CommittedIBFormat = IndexType;
            m_CommittedD3D12IndexDataStartOffset = m_IndexDataStartOffset + static_cast<Uint32>(BuffDataStartByteOffset);
            GraphicsCtx.SetIndexBuffer( IBView );
        }
        m_bCommittedD3D12IBUpToDate = true;
    }

    void DeviceContextD3D12Impl::TransitionD3D12VertexBuffers(GraphicsContext &GraphCtx)
    {
        for( UINT Buff = 0; Buff < m_NumVertexStreams; ++Buff )
        {
            auto &CurrStream = m_VertexStreams[Buff];
            VERIFY( CurrStream.pBuffer, "Attempting to bind a null buffer for rendering" );
            auto *pBufferD3D12 = static_cast<BufferD3D12Impl*>(CurrStream.pBuffer.RawPtr());
            if(!pBufferD3D12->CheckAllStates(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
                GraphCtx.TransitionResource(pBufferD3D12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        }
    }

    void DeviceContextD3D12Impl::CommitD3D12VertexBuffers(GraphicsContext &GraphCtx)
    {
        auto *pPipelineStateD3D12 = ValidatedCast<PipelineStateD3D12Impl>(m_pPipelineState.RawPtr());

        // Do not initialize array with zeroes for performance reasons
        D3D12_VERTEX_BUFFER_VIEW VBViews[MaxBufferSlots];// = {}
        VERIFY( m_NumVertexStreams <= MaxBufferSlots, "Too many buffers are being set" );
        const auto *TightStrides = pPipelineStateD3D12->GetTightStrides();
        for( UINT Buff = 0; Buff < m_NumVertexStreams; ++Buff )
        {
            auto &CurrStream = m_VertexStreams[Buff];
            auto &VBView = VBViews[Buff];
            VERIFY( CurrStream.pBuffer, "Attempting to bind a null buffer for rendering" );
            
            auto *pBufferD3D12 = static_cast<BufferD3D12Impl*>(CurrStream.pBuffer.RawPtr());
#ifdef _DEBUG
            if(pBufferD3D12->GetDesc().Usage == USAGE_DYNAMIC)
                pBufferD3D12->DbgVerifyDynamicAllocation(m_ContextId);
#endif

            GraphCtx.TransitionResource(pBufferD3D12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            
            // Device context keeps strong references to all vertex buffers.
            // When a buffer is unbound, a reference to D3D12 resource is added to the context,
            // so there is no need to reference the resource here
            //GraphicsCtx.AddReferencedObject(pd3d12Resource);

            VBView.BufferLocation = pBufferD3D12->GetGPUAddress(m_ContextId) + CurrStream.Offset;
            VBView.StrideInBytes = CurrStream.Stride ? CurrStream.Stride : TightStrides[Buff];
            // Note that for a dynamic buffer, what we use here is the size of the buffer itself, not the upload heap buffer!
            VBView.SizeInBytes = pBufferD3D12->GetDesc().uiSizeInBytes - CurrStream.Offset;
        }

        GraphCtx.FlushResourceBarriers();
        GraphCtx.SetVertexBuffers( 0, m_NumVertexStreams, VBViews );

        m_bCommittedD3D12VBsUpToDate = true;
    }

    void DeviceContextD3D12Impl::Draw( DrawAttribs &DrawAttribs )
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

        auto &GraphCtx = RequestCmdContext()->AsGraphicsContext();
        if( DrawAttribs.IsIndexed )
        {
            if( m_CommittedIBFormat != DrawAttribs.IndexType )
                m_bCommittedD3D12IBUpToDate = false;

            if(m_bCommittedD3D12IBUpToDate)
            {
                BufferD3D12Impl *pBuffD3D12 = static_cast<BufferD3D12Impl *>(m_pIndexBuffer.RawPtr());
                if(!pBuffD3D12->CheckAllStates(D3D12_RESOURCE_STATE_INDEX_BUFFER))
                    GraphCtx.TransitionResource(pBuffD3D12, D3D12_RESOURCE_STATE_INDEX_BUFFER, true);
            }
            else
                CommitD3D12IndexBuffer(DrawAttribs.IndexType);
        }

        auto *pPipelineStateD3D12 = ValidatedCast<PipelineStateD3D12Impl>(m_pPipelineState.RawPtr());
        
        auto D3D12Topology = TopologyToD3D12Topology( DrawAttribs.Topology );
        GraphCtx.SetPrimitiveTopology(D3D12Topology);

        if(m_bCommittedD3D12VBsUpToDate)
            TransitionD3D12VertexBuffers(GraphCtx);
        else
            CommitD3D12VertexBuffers(GraphCtx);

        GraphCtx.SetRootSignature( pPipelineStateD3D12->GetD3D12RootSignature() );

        if(m_pCommittedResourceCache != nullptr)
        {
            pPipelineStateD3D12->GetRootSignature().CommitRootViews(*m_pCommittedResourceCache, GraphCtx, false, m_ContextId);
        }
#ifdef _DEBUG
        else
        {
            if( pPipelineStateD3D12->dbgContainsShaderResources() )
                LOG_ERROR_MESSAGE("Pipeline state \"", pPipelineStateD3D12->GetDesc().Name, "\" contains shader resources, but IDeviceContext::CommitShaderResources() was not called" )
        }
#endif
        

        if( DrawAttribs.IsIndirect )
        {
            if( auto *pBufferD3D12 = ValidatedCast<BufferD3D12Impl>(DrawAttribs.pIndirectDrawAttribs) )
            {
#ifdef _DEBUG
                if(pBufferD3D12->GetDesc().Usage == USAGE_DYNAMIC)
                    pBufferD3D12->DbgVerifyDynamicAllocation(m_ContextId);
#endif

                GraphCtx.TransitionResource(pBufferD3D12, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
                size_t BuffDataStartByteOffset;
                ID3D12Resource *pd3d12ArgsBuff = pBufferD3D12->GetD3D12Buffer(BuffDataStartByteOffset, m_ContextId);
                GraphCtx.ExecuteIndirect(DrawAttribs.IsIndexed ? m_pDrawIndexedIndirectSignature : m_pDrawIndirectSignature, pd3d12ArgsBuff, DrawAttribs.IndirectDrawArgsOffset + BuffDataStartByteOffset);
            }
            else
            {
                LOG_ERROR_MESSAGE("Valid pIndirectDrawAttribs must be provided for indirect draw command")
            }
        }
        else
        {
            if( DrawAttribs.IsIndexed )
                GraphCtx.DrawIndexed(DrawAttribs.NumIndices, DrawAttribs.NumInstances, DrawAttribs.FirstIndexLocation, DrawAttribs.BaseVertex, DrawAttribs.FirstInstanceLocation);
            else
                GraphCtx.Draw(DrawAttribs.NumVertices, DrawAttribs.NumInstances, DrawAttribs.StartVertexLocation, DrawAttribs.FirstInstanceLocation );
        }
        ++m_NumCommandsInCurCtx;
    }

    void DeviceContextD3D12Impl::DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )
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

        auto *pPipelineStateD3D12 = ValidatedCast<PipelineStateD3D12Impl>(m_pPipelineState.RawPtr());

        auto &ComputeCtx = RequestCmdContext()->AsComputeContext();
        ComputeCtx.SetRootSignature( pPipelineStateD3D12->GetD3D12RootSignature() );
      
        if(m_pCommittedResourceCache != nullptr)
        {
            pPipelineStateD3D12->GetRootSignature().CommitRootViews(*m_pCommittedResourceCache, ComputeCtx, true, m_ContextId);
        }
#ifdef _DEBUG
        else
        {
            if( pPipelineStateD3D12->dbgContainsShaderResources() )
                LOG_ERROR_MESSAGE("Pipeline state \"", pPipelineStateD3D12->GetDesc().Name, "\" contains shader resources, but IDeviceContext::CommitShaderResources() was not called" )
        }
#endif

        if( DispatchAttrs.pIndirectDispatchAttribs )
        {
            if( auto *pBufferD3D12 = ValidatedCast<BufferD3D12Impl>(DispatchAttrs.pIndirectDispatchAttribs) )
            {
#ifdef _DEBUG
                if(pBufferD3D12->GetDesc().Usage == USAGE_DYNAMIC)
                    pBufferD3D12->DbgVerifyDynamicAllocation(m_ContextId);
#endif

                ComputeCtx.TransitionResource(pBufferD3D12, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
                size_t BuffDataStartByteOffset;
                ID3D12Resource *pd3d12ArgsBuff = pBufferD3D12->GetD3D12Buffer(BuffDataStartByteOffset, m_ContextId);
                ComputeCtx.ExecuteIndirect(m_pDispatchIndirectSignature, pd3d12ArgsBuff, DispatchAttrs.DispatchArgsByteOffset + BuffDataStartByteOffset);
            }
            else
            {
                LOG_ERROR_MESSAGE("Valid pIndirectDrawAttribs must be provided for indirect dispatch command")
            }
        }
        else
            ComputeCtx.Dispatch(DispatchAttrs.ThreadGroupCountX, DispatchAttrs.ThreadGroupCountY, DispatchAttrs.ThreadGroupCountZ);
        ++m_NumCommandsInCurCtx;
    }

    void DeviceContextD3D12Impl::ClearDepthStencil( ITextureView *pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil )
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
            auto *pDSV = ValidatedCast<SwapChainD3D12Impl>(m_pSwapChain.RawPtr())->GetDepthBufferDSV();
            pDSVD3D12 = ValidatedCast<ITextureViewD3D12>(pDSV);
        }
        D3D12_CLEAR_FLAGS d3d12ClearFlags = (D3D12_CLEAR_FLAGS)0;
        if( ClearFlags & CLEAR_DEPTH_FLAG )   d3d12ClearFlags |= D3D12_CLEAR_FLAG_DEPTH;
        if( ClearFlags & CLEAR_STENCIL_FLAG ) d3d12ClearFlags |= D3D12_CLEAR_FLAG_STENCIL;
        // The full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied??
        RequestCmdContext()->AsGraphicsContext().ClearDepthStencil( pDSVD3D12, d3d12ClearFlags, fDepth, Stencil );
        ++m_NumCommandsInCurCtx;
    }

    void DeviceContextD3D12Impl::ClearRenderTarget( ITextureView *pView, const float *RGBA )
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
            auto *pBackBufferRTV = ValidatedCast<SwapChainD3D12Impl>(m_pSwapChain.RawPtr())->GetCurrentBackBufferRTV();
            pd3d12RTV = ValidatedCast<ITextureViewD3D12>(pBackBufferRTV);
        }

        static const float Zero[4] = { 0.f, 0.f, 0.f, 0.f };
        if( RGBA == nullptr )
            RGBA = Zero;

        // The full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied??
        RequestCmdContext()->AsGraphicsContext().ClearRenderTarget( pd3d12RTV, RGBA );
        ++m_NumCommandsInCurCtx;
    }

    void DeviceContextD3D12Impl::Flush(bool RequestNewCmdCtx)
    {
        if( m_pCurrCmdCtx )
        {
            VERIFY(!m_bIsDeferred, "Deferred contexts cannot executre command lists directly");
            ValidatedCast<RenderDeviceD3D12Impl>(m_pDevice.RawPtr())->CloseAndExecuteCommandContext(m_pCurrCmdCtx);
        }

        m_pCurrCmdCtx = RequestNewCmdCtx ? ValidatedCast<RenderDeviceD3D12Impl>(m_pDevice.RawPtr())->AllocateCommandContext() : nullptr;
        m_NumCommandsInCurCtx = 0;

        m_pPipelineState.Release(); 
    }

    void DeviceContextD3D12Impl::Flush()
    {
        VERIFY(!m_bIsDeferred, "Flush() should only be called for immediate contexts");
        Flush(true);
    }

    void DeviceContextD3D12Impl::SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )
    {
        TDeviceContextBase::SetVertexBuffers( StartSlot, NumBuffersSet, ppBuffers, pStrides, pOffsets, Flags );
        m_bCommittedD3D12VBsUpToDate = false;
    }

    void DeviceContextD3D12Impl::ClearState()
    {
        TDeviceContextBase::ClearState();
        LOG_ERROR_ONCE("DeviceContextD3D12Impl::ClearState() is not implemented");
    }

    void DeviceContextD3D12Impl::SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )
    {
        TDeviceContextBase::SetIndexBuffer( pIndexBuffer, ByteOffset );
        m_bCommittedD3D12IBUpToDate = false;
    }

    void DeviceContextD3D12Impl::CommitViewports()
    {
        const Uint32 MaxViewports = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
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
        RequestCmdContext()->AsGraphicsContext().SetViewports( m_NumViewports, d3d12Viewports );
    }

    void DeviceContextD3D12Impl::SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight  )
    {
        const Uint32 MaxViewports = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        VERIFY( NumViewports < MaxViewports, "Too many viewports are being set" );
        NumViewports = std::min( NumViewports, MaxViewports );

        TDeviceContextBase::SetViewports( NumViewports, pViewports, RTWidth, RTHeight );
        VERIFY( NumViewports == m_NumViewports, "Unexpected number of viewports" );

        CommitViewports();
    }

    void DeviceContextD3D12Impl::SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight  )
    {
        const Uint32 MaxScissorRects = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        VERIFY( NumRects < MaxScissorRects, "Too many scissor rects are being set" );
        NumRects = std::min( NumRects, MaxScissorRects );

        TDeviceContextBase::SetScissorRects(NumRects, pRects, RTWidth, RTHeight);

        // Only commit scissor rects if scissor test is enabled in the rasterizer state. 
        if( !m_pPipelineState || m_pPipelineState->GetDesc().GraphicsPipeline.RasterizerDesc.ScissorEnable )
        {
            D3D12_RECT d3d12ScissorRects[MaxScissorRects];
            VERIFY( NumRects == m_NumScissorRects, "Unexpected number of scissor rects" );
            for( Uint32 sr = 0; sr < NumRects; ++sr )
            {
                d3d12ScissorRects[sr].left   = m_ScissorRects[sr].left;
                d3d12ScissorRects[sr].top    = m_ScissorRects[sr].top;
                d3d12ScissorRects[sr].right  = m_ScissorRects[sr].right;
                d3d12ScissorRects[sr].bottom = m_ScissorRects[sr].bottom;
            }
            RequestCmdContext()->AsGraphicsContext().SetScissorRects(NumRects, d3d12ScissorRects);
        }
    }


    void DeviceContextD3D12Impl::RebindRenderTargets()
    {
        const Uint32 MaxD3D12RTs = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
        Uint32 NumRenderTargets = m_NumBoundRenderTargets;
        VERIFY( NumRenderTargets <= MaxD3D12RTs, "D3D12 only allows 8 simultaneous render targets" );
        NumRenderTargets = std::min( MaxD3D12RTs, NumRenderTargets );

        ITextureViewD3D12 *ppRTVs[MaxD3D12RTs]; // Do not initialize with zeroes!
        ITextureViewD3D12 *pDSV = nullptr;
        if( NumRenderTargets == 0 && m_pBoundDepthStencil == nullptr )
        {
            NumRenderTargets = 1;
            auto *pSwapChainD3D12 = ValidatedCast<SwapChainD3D12Impl>( m_pSwapChain.RawPtr() );
            ppRTVs[0] = ValidatedCast<ITextureViewD3D12>(pSwapChainD3D12->GetCurrentBackBufferRTV());
            pDSV = ValidatedCast<ITextureViewD3D12>(pSwapChainD3D12->GetDepthBufferDSV());
        }
        else
        {
            for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
                ppRTVs[rt] = ValidatedCast<ITextureViewD3D12>(m_pBoundRenderTargets[rt].RawPtr());
            pDSV = ValidatedCast<ITextureViewD3D12>(m_pBoundDepthStencil.RawPtr());
        }
        RequestCmdContext()->AsGraphicsContext().SetRenderTargets(NumRenderTargets, ppRTVs, pDSV);
    }

    void DeviceContextD3D12Impl::SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )
    {
        if( TDeviceContextBase::SetRenderTargets( NumRenderTargets, ppRenderTargets, pDepthStencil ) )
        {
            RebindRenderTargets();

            // Set the viewport to match the render target size
            SetViewports(1, nullptr, 0, 0);
        }
    }
   
    DynamicAllocation DeviceContextD3D12Impl::AllocateDynamicSpace(size_t NumBytes)
    {
        return m_pUploadHeap->Allocate(NumBytes);
    }

    void DeviceContextD3D12Impl::UpdateBufferRegion(BufferD3D12Impl *pBuffD3D12, const void *pData, Uint64 DstOffset, Uint64 NumBytes)
    {
        VERIFY(pBuffD3D12->GetDesc().Usage != USAGE_DYNAMIC, "Dynamic buffers must be updated via Map()")

        auto pCmdCtx = RequestCmdContext();
//#if USE_RING_BUFFER
        //auto TmpSpace = pCmdCtx->AllocateUploadBuffer(NumBytes);
        auto TmpSpace = m_pUploadHeap->Allocate(NumBytes);
	    memcpy(TmpSpace.CPUAddress, pData, NumBytes);
        pCmdCtx->TransitionResource(pBuffD3D12, D3D12_RESOURCE_STATE_COPY_DEST, true);
        // Source buffer is already in right state, which cannot be changed
        size_t DstBuffDataStartByteOffset;
        auto *pd3d12Buff = pBuffD3D12->GetD3D12Buffer(DstBuffDataStartByteOffset, m_ContextId);
        VERIFY(DstBuffDataStartByteOffset == 0, "Dst buffer must not be suballocated");
        pCmdCtx->GetCommandList()->CopyBufferRegion( pd3d12Buff, DstOffset + DstBuffDataStartByteOffset, TmpSpace.pBuffer, TmpSpace.Offset, NumBytes);
        ++m_NumCommandsInCurCtx;
    }

    void DeviceContextD3D12Impl::CopyBufferRegion(BufferD3D12Impl *pSrcBuffD3D12, BufferD3D12Impl *pDstBuffD3D12, Uint64 SrcOffset, Uint64 DstOffset, Uint64 NumBytes)
    {
        VERIFY(pDstBuffD3D12->GetDesc().Usage != USAGE_DYNAMIC, "Dynamic buffers cannot be copy destinations")

        auto pCmdCtx = RequestCmdContext();
        pCmdCtx->TransitionResource(pSrcBuffD3D12, D3D12_RESOURCE_STATE_COPY_SOURCE);
        pCmdCtx->TransitionResource(pDstBuffD3D12, D3D12_RESOURCE_STATE_COPY_DEST, true);
        size_t DstDataStartByteOffset;
        auto *pd3d12DstBuff = pDstBuffD3D12->GetD3D12Buffer(DstDataStartByteOffset, m_ContextId);
        VERIFY(DstDataStartByteOffset == 0, "Dst buffer must not be suballocated");

        size_t SrcDataStartByteOffset;
        auto *pd3d12SrcBuff = pSrcBuffD3D12->GetD3D12Buffer(SrcDataStartByteOffset, m_ContextId);
        pCmdCtx->GetCommandList()->CopyBufferRegion( pd3d12DstBuff, DstOffset + DstDataStartByteOffset, pd3d12SrcBuff, SrcOffset+SrcDataStartByteOffset, NumBytes);
        ++m_NumCommandsInCurCtx;
    }

    void DeviceContextD3D12Impl::CopyTextureRegion(TextureD3D12Impl *pSrcTexture, Uint32 SrcSubResIndex, const D3D12_BOX *pD3D12SrcBox,
                                                   TextureD3D12Impl *pDstTexture, Uint32 DstSubResIndex, Uint32 DstX, Uint32 DstY, Uint32 DstZ)
    {
        auto pCmdCtx = RequestCmdContext();
        pCmdCtx->TransitionResource(pSrcTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
        pCmdCtx->TransitionResource(pDstTexture, D3D12_RESOURCE_STATE_COPY_DEST, true);

        D3D12_TEXTURE_COPY_LOCATION DstLocation = {}, SrcLocation = {};

        DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.pResource = pDstTexture->GetD3D12Resource();
        DstLocation.SubresourceIndex = SrcSubResIndex;

        SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        SrcLocation.pResource = pSrcTexture->GetD3D12Resource();
        SrcLocation.SubresourceIndex = DstSubResIndex;

        pCmdCtx->GetCommandList()->CopyTextureRegion( &DstLocation, DstX, DstY, DstZ, &SrcLocation, pD3D12SrcBox);
        ++m_NumCommandsInCurCtx;
    }

    void DeviceContextD3D12Impl::GenerateMips(TextureViewD3D12Impl *pTexView)
    {
        auto *pCtx = RequestCmdContext();
        m_MipsGenerator.GenerateMips(ValidatedCast<RenderDeviceD3D12Impl>(m_pDevice.RawPtr()), pTexView, *pCtx);
    }

    void DeviceContextD3D12Impl::FinishCommandList(class ICommandList **ppCommandList)
    {
        CommandListD3D12Impl *pCmdListD3D12( NEW(m_CmdListAllocator, "CommandListD3D12Impl instance", CommandListD3D12Impl, m_pDevice, m_pCurrCmdCtx) );
        pCmdListD3D12->QueryInterface( IID_CommandList, reinterpret_cast<IObject**>(ppCommandList) );
        m_pCurrCmdCtx = nullptr;
        Flush(true);
    }

    void DeviceContextD3D12Impl::ExecuteCommandList(class ICommandList *pCommandList)
    {
        if (m_bIsDeferred)
        {
            LOG_ERROR("Only immediate context can execute command list");
            return;
        }
        // First execute commands in this context
        Flush(true);

        CommandListD3D12Impl* pCmdListD3D12 = ValidatedCast<CommandListD3D12Impl>(pCommandList);
        ValidatedCast<RenderDeviceD3D12Impl>(m_pDevice.RawPtr())->CloseAndExecuteCommandContext(pCmdListD3D12->Close());
    }
}
