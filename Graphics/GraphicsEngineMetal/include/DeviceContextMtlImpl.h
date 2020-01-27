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

#pragma once

/// \file
/// Declaration of Diligent::DeviceContextMtlImpl class

#include "DeviceContextMtl.h"
#include "DeviceContextBase.hpp"
#include "ShaderMtlImpl.h"
#include "BufferMtlImpl.h"
#include "TextureMtlImpl.h"
#include "PipelineStateMtlImpl.h"
#include "QueryMtlImpl.h"

namespace Diligent
{

class RenderDeviceMtlImpl;

struct DeviceContextMtlImplTraits
{
    using BufferType        = BufferMtlImpl;
    using TextureType       = TextureMtlImpl;
    using PipelineStateType = PipelineStateMtlImpl;
    using DeviceType        = RenderDeviceMtlImpl;
    using QueryType         = QueryMtlImpl;
};

/// Implementation of the Diligent::IDeviceContextMtl interface
class DeviceContextMtlImpl final : public DeviceContextBase<IDeviceContextMtl, DeviceContextMtlImplTraits>
{
public:
    using TDeviceContextBase = DeviceContextBase<IDeviceContextMtl, DeviceContextMtlImplTraits>;

    DeviceContextMtlImpl(IReferenceCounters*               pRefCounters,
                         IMemoryAllocator&                 Allocator,
                         RenderDeviceMtlImpl*              pDevice,
                         const struct EngineMtlCreateInfo& EngineAttribs,
                         bool                              bIsDeferred);

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    virtual void SetPipelineState(IPipelineState* pPipelineState) override final;

    virtual void TransitionShaderResources(IPipelineState* pPipelineState, IShaderResourceBinding* pShaderResourceBinding) override final;

    virtual void CommitShaderResources(IShaderResourceBinding* pShaderResourceBinding, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    virtual void SetStencilRef(Uint32 StencilRef) override final;

    virtual void SetBlendFactors(const float* pBlendFactors = nullptr) override final;

    virtual void SetVertexBuffers(Uint32                         StartSlot,
                                  Uint32                         NumBuffersSet,
                                  IBuffer**                      ppBuffers,
                                  Uint32*                        pOffsets,
                                  RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                  SET_VERTEX_BUFFERS_FLAGS       Flags) override final;

    virtual void InvalidateState() override final;

    virtual void SetIndexBuffer(IBuffer* pIndexBuffer, Uint32 ByteOffset, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    virtual void SetViewports(Uint32 NumViewports, const Viewport* pViewports, Uint32 RTWidth, Uint32 RTHeight) override final;

    virtual void SetScissorRects(Uint32 NumRects, const Rect* pRects, Uint32 RTWidth, Uint32 RTHeight) override final;

    virtual void SetRenderTargets(Uint32                         NumRenderTargets,
                                  ITextureView*                  ppRenderTargets[],
                                  ITextureView*                  pDepthStencil,
                                  RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    virtual void Draw(const DrawAttribs& Attribs) override final;
    virtual void DrawIndexed(const DrawIndexedAttribs& Attribs) override final;
    virtual void DrawIndirect(const DrawIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) override final;
    virtual void DrawIndexedIndirect(const DrawIndexedIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) override final;

    virtual void DispatchCompute(const DispatchComputeAttribs& Attribs) override final;
    virtual void DispatchComputeIndirect(const DispatchComputeIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) override final;

    virtual void ClearDepthStencil(ITextureView*                  pView,
                                   CLEAR_DEPTH_STENCIL_FLAGS      ClearFlags,
                                   float                          fDepth,
                                   Uint8                          Stencil,
                                   RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    virtual void ClearRenderTarget(ITextureView* pView, const float* RGBA, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    virtual void UpdateBuffer(IBuffer*                       pBuffer,
                              Uint32                         Offset,
                              Uint32                         Size,
                              const void*                    pData,
                              RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    virtual void CopyBuffer(IBuffer*                       pSrcBuffer,
                            Uint32                         SrcOffset,
                            RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                            IBuffer*                       pDstBuffer,
                            Uint32                         DstOffset,
                            Uint32                         Size,
                            RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode) override final;

    virtual void MapBuffer(IBuffer* pBuffer, MAP_TYPE MapType, MAP_FLAGS MapFlags, PVoid& pMappedData) override final;

    virtual void UnmapBuffer(IBuffer* pBuffer, MAP_TYPE MapType) override final;

    virtual void UpdateTexture(ITexture*                      pTexture,
                               Uint32                         MipLevel,
                               Uint32                         Slice,
                               const Box&                     DstBox,
                               const TextureSubResData&       SubresData,
                               RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                               RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override final;

    virtual void CopyTexture(const CopyTextureAttribs& CopyAttribs) override final;

    virtual void MapTextureSubresource(ITexture*                 pTexture,
                                       Uint32                    MipLevel,
                                       Uint32                    ArraySlice,
                                       MAP_TYPE                  MapType,
                                       MAP_FLAGS                 MapFlags,
                                       const Box*                pMapRegion,
                                       MappedTextureSubresource& MappedData) override final;


    virtual void UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice) override final;

    virtual void GenerateMips(ITextureView* pTextureView) override final;

    virtual void FinishFrame() override final;

    virtual void TransitionResourceStates(Uint32 BarrierCount, StateTransitionDesc* pResourceBarriers) override final;

    virtual void ResolveTextureSubresource(ITexture*                               pSrcTexture,
                                           ITexture*                               pDstTexture,
                                           const ResolveTextureSubresourceAttribs& ResolveAttribs) override final;

    void FinishCommandList(class ICommandList** ppCommandList) override final;

    virtual void ExecuteCommandList(class ICommandList* pCommandList) override final;

    virtual void SignalFence(IFence* pFence, Uint64 Value) override final;

    virtual void WaitForFence(IFence* pFence, Uint64 Value, bool FlushContext) override final;

    virtual void WaitForIdle() override final;

    /// Implementation of IDeviceContext::BeginQuery() in Metal backend.
    virtual void BeginQuery(IQuery* pQuery) override final;

    /// Implementation of IDeviceContext::EndQuery() in Metal backend.
    virtual void EndQuery(IQuery* pQuery) override final;

    virtual void Flush() override final;

private:
};

} // namespace Diligent
