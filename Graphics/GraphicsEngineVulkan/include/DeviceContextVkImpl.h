/*     Copyright 2019 Diligent Graphics LLC
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
#include <unordered_map>

#include "DeviceContextVk.h"
#include "DeviceContextNextGenBase.h"
#include "VulkanUtilities/VulkanCommandBufferPool.h"
#include "VulkanUtilities/VulkanCommandBuffer.h"
#include "VulkanUploadHeap.h"
#include "VulkanDynamicHeap.h"
#include "ResourceReleaseQueue.h"
#include "DescriptorPoolManager.h"
#include "PipelineLayout.h"
#include "GenerateMipsVkHelper.h"
#include "BufferVkImpl.h"
#include "TextureVkImpl.h"
#include "PipelineStateVkImpl.h"
#include "HashUtils.h"

namespace Diligent
{

class RenderDeviceVkImpl;

struct DeviceContextVkImplTraits
{
    using BufferType        = BufferVkImpl;
    using TextureType       = TextureVkImpl;
    using PipelineStateType = PipelineStateVkImpl;
    using DeviceType        = RenderDeviceVkImpl;
    using ICommandQueueType = ICommandQueueVk;
};

/// Implementation of the Diligent::IDeviceContext interface
class DeviceContextVkImpl final : public DeviceContextNextGenBase<IDeviceContextVk, DeviceContextVkImplTraits>
{
public:
    using TDeviceContextBase = DeviceContextNextGenBase<IDeviceContextVk, DeviceContextVkImplTraits>;

    DeviceContextVkImpl(IReferenceCounters*                   pRefCounters,
                        RenderDeviceVkImpl*                   pDevice,
                        bool                                  bIsDeferred,
                        const EngineVkCreateInfo&             EngineCI,
                        Uint32                                ContextId,
                        Uint32                                CommandQueueId,
                        std::shared_ptr<GenerateMipsVkHelper> GenerateMipsHelper);
    ~DeviceContextVkImpl();
    
    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override final;

    virtual void SetPipelineState(IPipelineState* pPipelineState)override final;

    virtual void TransitionShaderResources(IPipelineState* pPipelineState, IShaderResourceBinding* pShaderResourceBinding)override final;

    virtual void CommitShaderResources(IShaderResourceBinding* pShaderResourceBinding, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)override final;

    virtual void SetStencilRef(Uint32 StencilRef)override final;

    virtual void SetBlendFactors(const float* pBlendFactors = nullptr)override final;

    virtual void SetVertexBuffers( Uint32                         StartSlot,
                                   Uint32                         NumBuffersSet,
                                   IBuffer**                      ppBuffers,
                                   Uint32*                        pOffsets,
                                   RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                   SET_VERTEX_BUFFERS_FLAGS       Flags )override final;
    
    virtual void InvalidateState()override final;

    virtual void SetIndexBuffer( IBuffer* pIndexBuffer, Uint32 ByteOffset, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode )override final;

    virtual void SetViewports( Uint32 NumViewports, const Viewport* pViewports, Uint32 RTWidth, Uint32 RTHeight )override final;

    virtual void SetScissorRects( Uint32 NumRects, const Rect* pRects, Uint32 RTWidth, Uint32 RTHeight )override final;

    virtual void SetRenderTargets( Uint32                         NumRenderTargets,
                                   ITextureView*                  ppRenderTargets[],
                                   ITextureView*                  pDepthStencil,
                                   RESOURCE_STATE_TRANSITION_MODE StateTransitionMode )override final;

    virtual void Draw               (const DrawAttribs& Attribs)override final;
    virtual void DrawIndexed        (const DrawIndexedAttribs& Attribs)override final;
    virtual void DrawIndirect       (const DrawIndirectAttribs& Attribs, IBuffer* pAttribsBuffer)override final;
    virtual void DrawIndexedIndirect(const DrawIndexedIndirectAttribs& Attribs, IBuffer* pAttribsBuffer)override final;

    virtual void DispatchCompute(const DispatchComputeAttribs& Attribs)override final;
    virtual void DispatchComputeIndirect(const DispatchComputeIndirectAttribs& Attribs, IBuffer* pAttribsBuffer)override final;

    virtual void ClearDepthStencil(ITextureView*                  pView,
                                   CLEAR_DEPTH_STENCIL_FLAGS      ClearFlags,
                                   float                          fDepth,
                                   Uint8                          Stencil,
                                   RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)override final;

    virtual void ClearRenderTarget( ITextureView* pView, const float* RGBA, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode )override final;

    virtual void UpdateBuffer(IBuffer*                       pBuffer,
                              Uint32                         Offset,
                              Uint32                         Size,
                              const PVoid                    pData,
                              RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)override final;

    virtual void CopyBuffer(IBuffer*                       pSrcBuffer,
                            Uint32                         SrcOffset,
                            RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                            IBuffer*                       pDstBuffer,
                            Uint32                         DstOffset,
                            Uint32                         Size,
                            RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode)override final;

    virtual void MapBuffer(IBuffer* pBuffer, MAP_TYPE MapType, MAP_FLAGS MapFlags, PVoid& pMappedData)override final;

    virtual void UnmapBuffer(IBuffer* pBuffer, MAP_TYPE MapType)override final;

    virtual void UpdateTexture(ITexture*                      pTexture,
                               Uint32                         MipLevel,
                               Uint32                         Slice,
                               const Box&                     DstBox,
                               const TextureSubResData&       SubresData,
                               RESOURCE_STATE_TRANSITION_MODE SrcBufferStateTransitionMode,
                               RESOURCE_STATE_TRANSITION_MODE TextureStateTransitionModee)override final;

    virtual void CopyTexture(const CopyTextureAttribs& CopyAttribs)override final;

    virtual void MapTextureSubresource( ITexture*                 pTexture,
                                        Uint32                    MipLevel,
                                        Uint32                    ArraySlice,
                                        MAP_TYPE                  MapType,
                                        MAP_FLAGS                 MapFlags,
                                        const Box*                pMapRegion,
                                        MappedTextureSubresource& MappedData )override final;


    virtual void UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice)override final;

    virtual void FinishCommandList(class ICommandList** ppCommandList)override final;

    virtual void ExecuteCommandList(class ICommandList* pCommandList)override final;

    virtual void SignalFence(IFence* pFence, Uint64 Value)override final;

    virtual void WaitForFence(IFence* pFence, Uint64 Value, bool FlushContext)override final;

    virtual void WaitForIdle()override final;

    virtual void Flush()override final;

    // Transitions texture subresources from OldState to NewState, and optionally updates
    // internal texture state.
    // If OldState == RESOURCE_STATE_UNKNOWN, internal texture state is used as old state.
    void TransitionTextureState(TextureVkImpl&           TextureVk,
                                RESOURCE_STATE           OldState,
                                RESOURCE_STATE           NewState,
                                bool                     UpdateTextureState,
                                VkImageSubresourceRange* pSubresRange = nullptr);
    
    void TransitionImageLayout(TextureVkImpl&                 TextureVk,
                               VkImageLayout                  OldLayout,
                               VkImageLayout                  NewLayout,
                               const VkImageSubresourceRange& SubresRange);

    virtual void TransitionImageLayout(ITexture* pTexture, VkImageLayout NewLayout)override final;


    // Transitions buffer state from OldState to NewState, and optionally updates
    // internal buffer state.
    // If OldState == RESOURCE_STATE_UNKNOWN, internal buffer state is used as old state.
    void TransitionBufferState(BufferVkImpl&  BufferVk, 
                               RESOURCE_STATE OldState,
                               RESOURCE_STATE NewState,
                               bool           UpdateBufferState);

    virtual void BufferMemoryBarrier(IBuffer* pBuffer, VkAccessFlags NewAccessFlags)override final;


    void AddWaitSemaphore(VkSemaphore Semaphore, VkPipelineStageFlags WaitDstStageMask)
    {
        m_WaitSemaphores.push_back(Semaphore);
        m_WaitDstStageMasks.push_back(WaitDstStageMask);
    }
    void AddSignalSemaphore(VkSemaphore Semaphore)
    {
        m_SignalSemaphores.push_back(Semaphore);
    }

    void UpdateBufferRegion(BufferVkImpl*                  pBuffVk,
                            Uint64                         DstOffset,
                            Uint64                         NumBytes,
                            VkBuffer                       vkSrcBuffer,
                            Uint64                         SrcOffset,
                            RESOURCE_STATE_TRANSITION_MODE TransitionMode);

    void CopyTextureRegion(TextureVkImpl*                 pSrcTexture,
                           RESOURCE_STATE_TRANSITION_MODE SrcTextureTransitionMode,
                           TextureVkImpl*                 pDstTexture,
                           RESOURCE_STATE_TRANSITION_MODE DstTextureTransitionMode,
                           const VkImageCopy&             CopyRegion);

    void UpdateTextureRegion(const void*                    pSrcData,
                             Uint32                         SrcStride,
                             Uint32                         SrcDepthStride,
                             TextureVkImpl&                 TextureVk,
                             Uint32                         MipLevel,
                             Uint32                         Slice,
                             const Box&                     DstBox,
                             RESOURCE_STATE_TRANSITION_MODE TextureTransitionMode);

    virtual void GenerateMips(ITextureView* pTexView)override final;

    Uint32 GetContextId()const{return m_ContextId;}

    size_t GetNumCommandsInCtx()const { return m_State.NumCommands; }

    __forceinline VulkanUtilities::VulkanCommandBuffer& GetCommandBuffer()
    {
        EnsureVkCmdBuffer();
        return m_CommandBuffer;
    }

    virtual void FinishFrame()override final;

    virtual void TransitionResourceStates(Uint32 BarrierCount, StateTransitionDesc* pResourceBarriers)override final;

    VkDescriptorSet AllocateDynamicDescriptorSet(VkDescriptorSetLayout SetLayout, const char* DebugName = "")
    {
        // Descriptor pools are externally synchronized, meaning that the application must not allocate 
        // and/or free descriptor sets from the same pool in multiple threads simultaneously (13.2.3)
        return m_DynamicDescrSetAllocator.Allocate(SetLayout, DebugName);
    }

    VulkanDynamicAllocation AllocateDynamicSpace(Uint32 SizeInBytes, Uint32 Alignment);

    void ResetRenderTargets();
    Int64 GetContextFrameNumber()const{return m_ContextFrameNumber;}

    GenerateMipsVkHelper& GetGenerateMipsHelper(){return *m_GenerateMipsHelper;}

private:
    void TransitionRenderTargets(RESOURCE_STATE_TRANSITION_MODE StateTransitionMode);
    __forceinline void CommitRenderPassAndFramebuffer(bool VerifyStates);
    void CommitVkVertexBuffers();
    void CommitViewports();
    void CommitScissorRects();
    
    __forceinline void TransitionOrVerifyBufferState(BufferVkImpl&                  Buffer,
                                                     RESOURCE_STATE_TRANSITION_MODE TransitionMode,
                                                     RESOURCE_STATE                 RequiredState,
                                                     VkAccessFlagBits               ExpectedAccessFlags,
                                                     const char*                    OperationName);

    __forceinline void TransitionOrVerifyTextureState(TextureVkImpl&                 Texture,
                                                      RESOURCE_STATE_TRANSITION_MODE TransitionMode,
                                                      RESOURCE_STATE                 RequiredState,
                                                      VkImageLayout                  ExpectedLayout,
                                                      const char*                    OperationName);


    __forceinline void EnsureVkCmdBuffer()
    {
        // Make sure that the number of commands in the context is at least one,
        // so that the context cannot be disposed by Flush()
        m_State.NumCommands = m_State.NumCommands != 0 ? m_State.NumCommands : 1;
        if (m_CommandBuffer.GetVkCmdBuffer() == VK_NULL_HANDLE)
        {
            auto vkCmdBuff = m_CmdPool.GetCommandBuffer();
            m_CommandBuffer.SetVkCmdBuffer(vkCmdBuff);
        }
    }
    
    inline void DisposeVkCmdBuffer(Uint32 CmdQueue, VkCommandBuffer vkCmdBuff, Uint64 FenceValue);
    inline void DisposeCurrentCmdBuffer(Uint32 CmdQueue, Uint64 FenceValue);

    struct BufferToTextureCopyInfo
    {
        Uint32  RowSize        = 0;
        Uint32  Stride         = 0;
        Uint32  StrideInTexels = 0;
        Uint32  DepthStride    = 0;
        Uint32  MemorySize     = 0;
        Uint32  RowCount       = 0;
        Box     Region;
    };
    BufferToTextureCopyInfo GetBufferToTextureCopyInfo(const TextureDesc& TexDesc,
                                                       Uint32             MipLevel,
                                                       const Box&         Region)const;

    void CopyBufferToTexture(VkBuffer                       vkSrcBuffer,
                             Uint32                         SrcBufferOffset,
                             Uint32                         SrcBufferRowStrideInTexels,
                             TextureVkImpl&                 DstTextureVk,
                             const Box&                     DstRegion,
                             Uint32                         DstMipLevel,
                             Uint32                         DstArraySlice,
                             RESOURCE_STATE_TRANSITION_MODE DstTextureTransitionMode);

    void CopyTextureToBuffer(TextureVkImpl&                 SrcTextureVk,
                             const Box&                     SrcRegion,
                             Uint32                         SrcMipLevel,
                             Uint32                         SrcArraySlice,
                             RESOURCE_STATE_TRANSITION_MODE SrcTextureTransitionMode,
                             VkBuffer                       vkDstBuffer,
                             Uint32                         DstBufferOffset,
                             Uint32                         DstBufferRowStrideInTexels);

    __forceinline void PrepareForDraw(DRAW_FLAGS Flags);
    __forceinline void PrepareForIndexedDraw(DRAW_FLAGS Flags, VALUE_TYPE IndexType);
    __forceinline BufferVkImpl* PrepareIndirectDrawAttribsBuffer(IBuffer* pAttribsBuffer, RESOURCE_STATE_TRANSITION_MODE TransitonMode);
    __forceinline void PrepareForDispatchCompute();

    void DvpLogRenderPass_PSOMismatch();

    VulkanUtilities::VulkanCommandBuffer m_CommandBuffer;

    const Uint32 m_NumCommandsToFlush = 192;
    struct ContextState
    {
        /// Flag indicating if currently committed vertex buffers are up to date
        bool CommittedVBsUpToDate = false;

        /// Flag indicating if currently committed index buffer is up to date
        bool CommittedIBUpToDate = false;

        Uint32 NumCommands = 0;
    }m_State;


    /// Render pass that matches currently bound render targets.
    /// This render pass may or may not be currently set in the command buffer
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;

    /// Framebuffer that matches currently bound render targets.
    /// This framebuffer may or may not be currently set in the command buffer
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;

    FixedBlockMemoryAllocator m_CmdListAllocator;

    // Semaphores are not owned by the command context
    std::vector<VkSemaphore>           m_WaitSemaphores;
    std::vector<VkPipelineStageFlags>  m_WaitDstStageMasks;
    std::vector<VkSemaphore>           m_SignalSemaphores;

    // List of fences to signal next time the command context is flushed
    std::vector<std::pair<Uint64, RefCntAutoPtr<IFence> > > m_PendingFences;

    std::unordered_map<BufferVkImpl*, VulkanUploadAllocation> m_UploadAllocations;

    struct MappedTextureKey
    {
        TextureVkImpl* const Texture;
        Uint32         const MipLevel;
        Uint32         const ArraySlice;

        bool operator == (const MappedTextureKey& rhs)const
        {
            return Texture    == rhs.Texture  &&
                   MipLevel   == rhs.MipLevel &&
                   ArraySlice == rhs.ArraySlice;
        }
        struct Hasher
        {
            size_t operator()(const MappedTextureKey& Key)const
            {
                return ComputeHash(Key.Texture, Key.MipLevel, Key.ArraySlice);
            }
        };
    };
    struct MappedTexture
    {
        BufferToTextureCopyInfo CopyInfo;
        VulkanDynamicAllocation Allocation;
    };
    std::unordered_map<MappedTextureKey, MappedTexture, MappedTextureKey::Hasher> m_MappedTextures;

    VulkanUtilities::VulkanCommandBufferPool m_CmdPool;
    VulkanUploadHeap                         m_UploadHeap;
    VulkanDynamicHeap                        m_DynamicHeap;
    DynamicDescriptorSetAllocator            m_DynamicDescrSetAllocator;

    PipelineLayout::DescriptorSetBindInfo m_DescrSetBindInfo;
    std::shared_ptr<GenerateMipsVkHelper> m_GenerateMipsHelper;
    RefCntAutoPtr<IShaderResourceBinding> m_GenerateMipsSRB;

    // In Vulkan we can't bind null vertex buffer, so we have to create a dummy VB
    RefCntAutoPtr<BufferVkImpl> m_DummyVB;
};

}
