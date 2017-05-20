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

#pragma once

/// \file
/// Declaration of Diligent::DeviceContextD3D12Impl class

#include "DeviceContext.h"
#include "DeviceContextBase.h"
#include "GenerateMips.h"

#ifdef _DEBUG
#   define VERIFY_CONTEXT_BINDINGS
#endif

namespace Diligent
{

/// Implementation of the Diligent::IDeviceContext interface
class DeviceContextD3D12Impl : public DeviceContextBase<IDeviceContext>
{
public:
    typedef DeviceContextBase<IDeviceContext> TDeviceContextBase;

    DeviceContextD3D12Impl(IMemoryAllocator &RawMemAllocator, class RenderDeviceD3D12Impl *pDevice, bool bIsDeferred, const EngineD3D12Attribs &Attribs, Uint32 ContextId);
    ~DeviceContextD3D12Impl();
    //virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

    virtual void SetPipelineState(IPipelineState *pPipelineState)override final;

    virtual void TransitionShaderResources(IPipelineState *pPipelineState, IShaderResourceBinding *pShaderResourceBinding)override final;

    virtual void CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags)override final;

    virtual void SetStencilRef(Uint32 StencilRef)override final;

    virtual void SetBlendFactors(const float* pBlendFactors = nullptr)override final;

    virtual void SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )override final;
    virtual void ClearState()override final;

    virtual void SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )override final;

    virtual void SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight )override final;

    virtual void SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight )override final;

    virtual void SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )override final;

    virtual void Draw( DrawAttribs &DrawAttribs )override final;

    virtual void DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )override final;

    virtual void ClearDepthStencil( ITextureView *pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil)override final;

    virtual void ClearRenderTarget( ITextureView *pView, const float *RGBA )override final;

    virtual void Flush()override final;
    
    virtual void FinishCommandList(class ICommandList **ppCommandList)override final;

    virtual void ExecuteCommandList(class ICommandList *pCommandList)override final;

    ///// Clears the state caches. This function is called once per frame
    ///// (before present) to release all outstanding objects
    ///// that are only kept alive by references in the cache
    //void ClearShaderStateCache();

    ///// Number of different shader types (Vertex, Pixel, Geometry, Domain, Hull, Compute)
    //static const int NumShaderTypes = 6;

    void UpdateBufferRegion(class BufferD3D12Impl *pBuffD3D12, const void *pData, Uint64 DstOffset, Uint64 NumBytes);
    void CopyBufferRegion(class BufferD3D12Impl *pSrcBuffD3D12, class BufferD3D12Impl *pDstBuffD3D12, Uint64 SrcOffset, Uint64 DstOffset, Uint64 NumBytes);
    void CopyTextureRegion(class TextureD3D12Impl *pSrcTexture, Uint32 SrcSubResIndex, const D3D12_BOX *pD3D12SrcBox,
                           class TextureD3D12Impl *pDstTexture, Uint32 DstSubResIndex, Uint32 DstX, Uint32 DstY, Uint32 DstZ);

    void GenerateMips(class TextureViewD3D12Impl *pTexView);

    struct DynamicAllocation AllocateDynamicSpace(size_t NumBytes);
    
    Uint32 GetContextId()const{return m_ContextId;}

private:
    void CommitD3D12IndexBuffer(VALUE_TYPE IndexType);
    void CommitD3D12VertexBuffers(class GraphicsContext &GraphCtx);
    void TransitionD3D12VertexBuffers(class GraphicsContext &GraphCtx);
    void RebindRenderTargets();
    void CommitViewports();
    void CommitScissorRects();
    void Flush(bool RequestNewCmdCtx);

    friend class SwapChainD3D12Impl;
    inline class CommandContext* RequestCmdContext(){return m_pCurrCmdCtx;}
    size_t m_NumCommandsInCurCtx = 0;
    const Uint32  m_NumCommandsToFlush = 192;
    CommandContext* m_pCurrCmdCtx = nullptr;

    CComPtr<ID3D12Resource> m_CommittedD3D12IndexBuffer;
    VALUE_TYPE m_CommittedIBFormat = VT_UNDEFINED;
    Uint32 m_CommittedD3D12IndexDataStartOffset = 0;

    CComPtr<ID3D12CommandSignature> m_pDrawIndirectSignature;
    CComPtr<ID3D12CommandSignature> m_pDrawIndexedIndirectSignature;
    CComPtr<ID3D12CommandSignature> m_pDispatchIndirectSignature;
    
    GenerateMipsHelper m_MipsGenerator;
    class DynamicUploadHeap* m_pUploadHeap = nullptr;
    
    /// Flag indicating if currently committed D3D12 vertex buffers are up to date
    bool m_bCommittedD3D12VBsUpToDate = false;

    /// Flag indicating if currently committed D3D11 index buffer is up to date
    bool m_bCommittedD3D12IBUpToDate = false;

    class ShaderResourceCacheD3D12 *m_pCommittedResourceCache = nullptr;

    FixedBlockMemoryAllocator m_CmdListAllocator;
    const Uint32 m_ContextId;
};

}
