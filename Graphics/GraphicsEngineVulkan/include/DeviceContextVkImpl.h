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
/// Declaration of Diligent::DeviceContextVkImpl class

#include "DeviceContextVk.h"
#include "DeviceContextBase.h"
#include "GenerateMips.h"
#include "VulkanUtilities/VulkanCommandBufferPool.h"
#include "VulkanUtilities/VulkanCommandBuffer.h"

#ifdef _DEBUG
#   define VERIFY_CONTEXT_BINDINGS
#endif

namespace Diligent
{

/// Implementation of the Diligent::IDeviceContext interface
class DeviceContextVkImpl : public DeviceContextBase<IDeviceContextVk>
{
public:
    typedef DeviceContextBase<IDeviceContextVk> TDeviceContextBase;

    DeviceContextVkImpl(IReferenceCounters *pRefCounters, class RenderDeviceVkImpl *pDevice, bool bIsDeferred, const EngineVkAttribs &Attribs, Uint32 ContextId);
    ~DeviceContextVkImpl();
    
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

    virtual void SetPipelineState(IPipelineState *pPipelineState)override final;

    virtual void TransitionShaderResources(IPipelineState *pPipelineState, IShaderResourceBinding *pShaderResourceBinding)override final;

    virtual void CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags)override final;

    virtual void SetStencilRef(Uint32 StencilRef)override final;

    virtual void SetBlendFactors(const float* pBlendFactors = nullptr)override final;

    virtual void SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )override final;
    
    virtual void InvalidateState()override final;

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

    virtual void TransitionImageLayout(ITexture *pTexture, VkImageLayout NewLayout)override final;

    void AddWaitSemaphore(VkSemaphore Semaphore, VkPipelineStageFlags WaitDstStageMask)
    {
        m_WaitSemaphores.push_back(Semaphore);
        m_WaitDstStageMasks.push_back(WaitDstStageMask);
    }
    void AddSignalSemaphore(VkSemaphore Semaphore)
    {
        m_SignalSemaphores.push_back(Semaphore);
    }

#if 0
    virtual void TransitionBufferState(IBuffer *pBuffer, Vk_RESOURCE_STATES State)override final;

    ///// Clears the state caches. This function is called once per frame
    ///// (before present) to release all outstanding objects
    ///// that are only kept alive by references in the cache
    //void ClearShaderStateCache();

    ///// Number of different shader types (Vertex, Pixel, Geometry, Domain, Hull, Compute)
    //static constexpr int NumShaderTypes = 6;

    void UpdateBufferRegion(class BufferVkImpl *pBuffVk, struct DynamicAllocation& Allocation, Uint64 DstOffset, Uint64 NumBytes);
    void UpdateBufferRegion(class BufferVkImpl *pBuffVk, const void *pData, Uint64 DstOffset, Uint64 NumBytes);
    void CopyBufferRegion(class BufferVkImpl *pSrcBuffVk, class BufferVkImpl *pDstBuffVk, Uint64 SrcOffset, Uint64 DstOffset, Uint64 NumBytes);
    void CopyTextureRegion(class TextureVkImpl *pSrcTexture, Uint32 SrcSubResIndex, const Vk_BOX *pVkSrcBox,
                           class TextureVkImpl *pDstTexture, Uint32 DstSubResIndex, Uint32 DstX, Uint32 DstY, Uint32 DstZ);
    void CopyTextureRegion(IBuffer *pSrcBuffer, Uint32 SrcStride, Uint32 SrcDepthStride, class TextureVkImpl *pTextureVk, Uint32 DstSubResIndex, const Box &DstBox);
#endif
    void GenerateMips(class TextureViewVkImpl *pTexView);

#if 0
    struct DynamicAllocation AllocateDynamicSpace(size_t NumBytes);
    
    Uint32 GetContextId()const{return m_ContextId;}
#endif
    size_t GetNumCommandsInCtx()const { return m_State.NumCommands; }

private:
    void CommitRenderPassAndFramebuffer(class PipelineStateVkImpl *pPipelineStateVk);
    void CommitVkIndexBuffer(VALUE_TYPE IndexType);
    void CommitVkVertexBuffers(class GraphicsContext &GraphCtx);
    void TransitionVkVertexBuffers(class GraphicsContext &GraphCtx);
    void CommitRenderTargets();
    void CommitViewports();
    void CommitScissorRects();

#if 0
    friend class SwapChainVkImpl;
#endif
    inline void EnsureVkCmdBuffer();
    inline void DisposeVkCmdBuffer();
    
    VulkanUtilities::VulkanCommandBuffer m_CommandBuffer;

    const Uint32  m_NumCommandsToFlush = 192;
    struct ContextState
    {
        /// Flag indicating if currently committed vertex buffers are up to date
        bool CommittedVBsUpToDate = false;

        /// Flag indicating if currently committed index buffer is up to date
        bool CommittedIBUpToDate = false;

        Uint32 NumCommands = 0;
    }m_State;

#if 0
    CComPtr<IVkResource> m_CommittedVkIndexBuffer;
    VALUE_TYPE m_CommittedIBFormat = VT_UNDEFINED;
    Uint32 m_CommittedVkIndexDataStartOffset = 0;

    CComPtr<IVkCommandSignature> m_pDrawIndirectSignature;
    CComPtr<IVkCommandSignature> m_pDrawIndexedIndirectSignature;
    CComPtr<IVkCommandSignature> m_pDispatchIndirectSignature;
    
    GenerateMipsHelper m_MipsGenerator;
    class DynamicUploadHeap* m_pUploadHeap = nullptr;
    
    class ShaderResourceCacheVk *m_pCommittedResourceCache = nullptr;

    FixedBlockMemoryAllocator m_CmdListAllocator;
    const Uint32 m_ContextId;
#endif
    VulkanUtilities::VulkanCommandBufferPool m_CmdPool;

    std::vector<VkSemaphore> m_WaitSemaphores;
    std::vector<VkPipelineStageFlags> m_WaitDstStageMasks;
    std::vector<VkSemaphore> m_SignalSemaphores;
};

}
