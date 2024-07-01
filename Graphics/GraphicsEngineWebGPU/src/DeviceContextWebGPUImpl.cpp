/*
 *  Copyright 2023-2024 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "DeviceContextWebGPUImpl.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "TextureWebGPUImpl.hpp"
#include "TextureViewWebGPUImpl.hpp"
#include "BufferWebGPUImpl.hpp"
#include "PipelineStateWebGPUImpl.hpp"
#include "PipelineResourceSignatureWebGPUImpl.hpp"
#include "PipelineResourceAttribsWebGPU.hpp"
#include "ShaderResourceCacheWebGPU.hpp"
#include "RenderPassWebGPUImpl.hpp"
#include "FramebufferWebGPUImpl.hpp"
#include "FenceWebGPUImpl.hpp"
#include "QueryManagerWebGPU.hpp"
#include "QueryWebGPUImpl.hpp"
#include "AttachmentCleanerWebGPU.hpp"
#include "WebGPUTypeConversions.hpp"

namespace Diligent
{

DeviceContextWebGPUImpl::DeviceContextWebGPUImpl(IReferenceCounters*           pRefCounters,
                                                 RenderDeviceWebGPUImpl*       pDevice,
                                                 const EngineWebGPUCreateInfo& EngineCI,
                                                 const DeviceContextDesc&      Desc) :
    // clang-format off
    TDeviceContextBase
    {
        pRefCounters,
        pDevice,
        Desc
    }
// clang-format on
{
    m_wgpuQueue = wgpuDeviceGetQueue(pDevice->GetWebGPUDevice());
    (void)m_ActiveQueriesCounter;

    pDevice->CreateFence({}, &m_pFence);
}

void DeviceContextWebGPUImpl::Begin(Uint32 ImmediateContextId)
{
    DEV_CHECK_ERR(ImmediateContextId == 0, "WebGPU supports only one immediate context");
    TDeviceContextBase::Begin(DeviceContextIndex{ImmediateContextId}, COMMAND_QUEUE_TYPE_GRAPHICS);
}

void DeviceContextWebGPUImpl::SetPipelineState(IPipelineState* pPipelineState)
{
    RefCntAutoPtr<PipelineStateWebGPUImpl> pPipelineStateWebGPU{pPipelineState, PipelineStateWebGPUImpl::IID_InternalImpl};
    VERIFY(pPipelineState == nullptr || pPipelineStateWebGPU != nullptr, "Unknown pipeline state object implementation");
    if (PipelineStateWebGPUImpl::IsSameObject(m_pPipelineState, pPipelineStateWebGPU))
        return;

    TDeviceContextBase::SetPipelineState(std::move(pPipelineStateWebGPU), 0 /*Dummy*/);

    m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_PIPELINE_STATE);

    const Uint32 SignatureCount = m_pPipelineState->GetResourceSignatureCount();

    Uint32 DbgActiveBindGroupIndex = 0;
    m_BindInfo.ActiveBindGroups    = 0;
    for (Uint32 i = 0; i < SignatureCount; ++i)
    {
        PipelineResourceSignatureWebGPUImpl* pSign = m_pPipelineState->GetResourceSignature(i);
        if (pSign == nullptr)
            continue;

        VERIFY(m_pPipelineState->GetPipelineLayout().GetFirstBindGroupIndex(i) == DbgActiveBindGroupIndex, "Bind group index mismatch");
        Uint32 BGIndex = i * PipelineResourceSignatureWebGPUImpl::MAX_BIND_GROUPS;
        if (pSign->HasBindGroup(PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_STATIC_MUTABLE))
        {
            m_BindInfo.ActiveBindGroups |= 1u << (BGIndex++);
            ++DbgActiveBindGroupIndex;
        }
        if (pSign->HasBindGroup(PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_DYNAMIC))
        {
            m_BindInfo.ActiveBindGroups |= 1u << (BGIndex++);
            ++DbgActiveBindGroupIndex;
        }
    }
    VERIFY(m_pPipelineState->GetPipelineLayout().GetBindGroupCount() == DbgActiveBindGroupIndex, "Bind group count mismatch");
    (void)DbgActiveBindGroupIndex;

    m_BindInfo.DirtyBindGroups = m_BindInfo.ActiveBindGroups;

    Uint32 DvpCompatibleSRBCount = 0;
    PrepareCommittedResources(m_BindInfo, DvpCompatibleSRBCount);
}

void DeviceContextWebGPUImpl::TransitionShaderResources(IShaderResourceBinding* pShaderResourceBinding)
{
    DEV_CHECK_ERR(pShaderResourceBinding != nullptr, "Shader resource binding must not be null");
}


#ifdef DILIGENT_DEVELOPMENT
void DeviceContextWebGPUImpl::DvpValidateCommittedShaderResources()
{
    if (m_BindInfo.ResourcesValidated)
        return;

    DvpVerifySRBCompatibility(m_BindInfo);

    const Uint32 SignCount = m_pPipelineState->GetResourceSignatureCount();
    for (Uint32 i = 0; i < SignCount; ++i)
    {
        const PipelineResourceSignatureWebGPUImpl* pSign = m_pPipelineState->GetResourceSignature(i);
        if (pSign == nullptr || pSign->GetNumBindGroups() == 0)
            continue; // Skip null and empty signatures

        const Uint32 BGCount = pSign->GetNumBindGroups();
        for (Uint32 bg = 0; bg < BGCount; ++bg)
        {
            DEV_CHECK_ERR(m_BindInfo.BindGroups[i * 2 + bg].wgpuBindGroup,
                          "bind group with index ", bg, " is not bound for resource signature '",
                          pSign->GetDesc().Name, "', binding index ", i, ".");
        }
    }

    m_pPipelineState->DvpVerifySRBResources(m_BindInfo.ResourceCaches);

    m_BindInfo.ResourcesValidated = true;
}
#endif

void DeviceContextWebGPUImpl::CommitShaderResources(IShaderResourceBinding*        pShaderResourceBinding,
                                                    RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    TDeviceContextBase::CommitShaderResources(pShaderResourceBinding, StateTransitionMode, 0 /*Dummy*/);

    ShaderResourceBindingWebGPUImpl* pResBindingWebGPU = ClassPtrCast<ShaderResourceBindingWebGPUImpl>(pShaderResourceBinding);
    ShaderResourceCacheWebGPU&       ResourceCache     = pResBindingWebGPU->GetResourceCache();
    if (ResourceCache.GetNumBindGroups() == 0)
    {
        // Ignore SRBs that contain no resources
        return;
    }

#ifdef DILIGENT_DEBUG
    //ResourceCache.DbgVerifyDynamicBuffersCounter();
#endif

    const WGPUDevice wgpuDevice = m_pDevice->GetWebGPUDevice();

    const Uint32                               SRBIndex   = pResBindingWebGPU->GetBindingIndex();
    const PipelineResourceSignatureWebGPUImpl* pSignature = pResBindingWebGPU->GetSignature();
    m_BindInfo.Set(SRBIndex, pResBindingWebGPU);

    Uint32 BGIndex = 0;
    if (pSignature->HasBindGroup(PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_STATIC_MUTABLE))
    {
        VERIFY_EXPR(BGIndex == pSignature->GetBindGroupIndex<PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_STATIC_MUTABLE>());
        m_BindInfo.BindGroups[SRBIndex * 2 + BGIndex] = {
            ResourceCache.UpdateBindGroup(wgpuDevice, BGIndex, pSignature->GetWGPUBindGroupLayout(PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_STATIC_MUTABLE)),
            ResourceCache.GetDynamicOffsets(BGIndex),
            ResourceCache.GetDynamicOffsetCount(BGIndex),
        };
        m_BindInfo.DirtyBindGroups |= 1u << (SRBIndex * 2 + BGIndex);
        ++BGIndex;
    }

    if (pSignature->HasBindGroup(PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_DYNAMIC))
    {
        VERIFY_EXPR(BGIndex == pSignature->GetBindGroupIndex<PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_DYNAMIC>());
        m_BindInfo.BindGroups[SRBIndex * 2 + BGIndex] = {
            ResourceCache.UpdateBindGroup(wgpuDevice, BGIndex, pSignature->GetWGPUBindGroupLayout(PipelineResourceSignatureWebGPUImpl::BIND_GROUP_ID_DYNAMIC), true),
            ResourceCache.GetDynamicOffsets(BGIndex),
            ResourceCache.GetDynamicOffsetCount(BGIndex),
        };
        m_BindInfo.DirtyBindGroups |= 1u << (SRBIndex * 2 + BGIndex);
        ++BGIndex;
    }

    VERIFY_EXPR(BGIndex == ResourceCache.GetNumBindGroups());
}

void SetBindGroup(WGPURenderPassEncoder Encoder, uint32_t GroupIndex, WGPUBindGroup Group, size_t DynamicOffsetCount, uint32_t const* DynamicOffsets)
{
    wgpuRenderPassEncoderSetBindGroup(Encoder, GroupIndex, Group, DynamicOffsetCount, DynamicOffsets);
}
void SetBindGroup(WGPUComputePassEncoder Encoder, uint32_t GroupIndex, WGPUBindGroup Group, size_t DynamicOffsetCount, uint32_t const* DynamicOffsets)
{
    wgpuComputePassEncoderSetBindGroup(Encoder, GroupIndex, Group, DynamicOffsetCount, DynamicOffsets);
}

template <typename CmdEncoderType>
void DeviceContextWebGPUImpl::CommitBindGroups(CmdEncoderType CmdEncoder)
{
    // Bind groups in m_EncoderState.BindGroups are indexed by SRB index rather than bind group index
    // in the pipeline layout.
    m_BindInfo.DirtyBindGroups &= m_BindInfo.ActiveBindGroups;
    while (m_BindInfo.DirtyBindGroups != 0)
    {
        // m_BindInfo.DirtyBindGroups is indexed by SRB index
        const Uint32 SrcBindGroupIndex = PlatformMisc::GetLSB(m_BindInfo.DirtyBindGroups);
        // Count the number of active groups that are bound before the current group.
        // Note that there may be inactive groups if, for example, signature at preceding index
        // does not use dynamic resources.
        const Uint32 BindGroupIndex = PlatformMisc::CountOneBits(m_BindInfo.ActiveBindGroups & ((1u << SrcBindGroupIndex) - 1u));
        VERIFY_EXPR(BindGroupIndex < m_BindInfo.BindGroups.size());
        const WebGPUResourceBindInfo::BindGroupInfo& BindGroup = m_BindInfo.BindGroups[SrcBindGroupIndex];
        if (WGPUBindGroup wgpuBindGroup = BindGroup.wgpuBindGroup)
        {
            SetBindGroup(CmdEncoder, BindGroupIndex, wgpuBindGroup, BindGroup.DynamicOffsetCount, BindGroup.DynamicOffsets);
        }
        else
        {
            DEV_ERROR("Active bind group at index ", SrcBindGroupIndex, " is not initialized");
        }
        m_BindInfo.DirtyBindGroups &= ~(1u << SrcBindGroupIndex);
    }
}

void DeviceContextWebGPUImpl::InvalidateState()
{
    TDeviceContextBase::InvalidateState();
    m_PendingClears.Clear();
    m_EncoderState.Clear();
    m_BindInfo.Reset();
}

void DeviceContextWebGPUImpl::SetStencilRef(Uint32 StencilRef)
{
    if (TDeviceContextBase::SetStencilRef(StencilRef, 0))
        m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_STENCIL_REF);
}

void DeviceContextWebGPUImpl::SetBlendFactors(const float* pBlendFactors)
{
    if (TDeviceContextBase::SetBlendFactors(pBlendFactors, 0))
        m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_BLEND_FACTORS);
}

void DeviceContextWebGPUImpl::SetVertexBuffers(Uint32                         StartSlot,
                                               Uint32                         NumBuffersSet,
                                               IBuffer* const*                ppBuffers,
                                               const Uint64*                  pOffsets,
                                               RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                               SET_VERTEX_BUFFERS_FLAGS       Flags)
{
    TDeviceContextBase::SetVertexBuffers(StartSlot, NumBuffersSet, ppBuffers, pOffsets, StateTransitionMode, Flags);
    m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_VERTEX_BUFFERS);
}


void DeviceContextWebGPUImpl::SetIndexBuffer(IBuffer*                       pIndexBuffer,
                                             Uint64                         ByteOffset,
                                             RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    TDeviceContextBase::SetIndexBuffer(pIndexBuffer, ByteOffset, StateTransitionMode);
    m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_INDEX_BUFFER);
}

void DeviceContextWebGPUImpl::SetViewports(Uint32          NumViewports,
                                           const Viewport* pViewports,
                                           Uint32          RTWidth,
                                           Uint32          RTHeight)
{
    TDeviceContextBase::SetViewports(NumViewports, pViewports, RTWidth, RTHeight);
    m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_VIEWPORTS);
}

void DeviceContextWebGPUImpl::SetScissorRects(Uint32 NumRects, const Rect* pRects, Uint32 RTWidth, Uint32 RTHeight)
{
    TDeviceContextBase::SetScissorRects(NumRects, pRects, RTWidth, RTHeight);
    m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_SCISSOR_RECTS);
}

void DeviceContextWebGPUImpl::SetRenderTargetsExt(const SetRenderTargetsAttribs& Attribs)
{
    if (m_PendingClears.AnyPending())
    {
        bool RTChanged =
            Attribs.NumRenderTargets != m_NumBoundRenderTargets ||
            Attribs.pDepthStencil != m_pBoundDepthStencil ||
            Attribs.pShadingRateMap != m_pBoundShadingRateMap;
        for (Uint32 RTIndex = 0; RTIndex < m_NumBoundRenderTargets && !RTChanged; ++RTIndex)
            RTChanged = m_pBoundRenderTargets[RTIndex] != Attribs.ppRenderTargets[RTIndex];

        if (RTChanged)
        {
            VERIFY(!m_wgpuRenderPassEncoder, "There should be no active render command encoder when pending clears mask is not zero");
            EndCommandEncoders(COMMAND_ENCODER_FLAG_ALL & ~COMMAND_ENCODER_FLAG_RENDER);
            CommitRenderTargets();
        }
    }

    if (TDeviceContextBase::SetRenderTargets(Attribs))
    {
        EndCommandEncoders(COMMAND_ENCODER_FLAG_RENDER);
        SetViewports(1, nullptr, 0, 0);
    }
}

void DeviceContextWebGPUImpl::BeginRenderPass(const BeginRenderPassAttribs& Attribs)
{
    TDeviceContextBase::BeginRenderPass(Attribs);
    m_AttachmentClearValues.resize(Attribs.ClearValueCount);
    for (Uint32 RTIndex = 0; RTIndex < Attribs.ClearValueCount; ++RTIndex)
        m_AttachmentClearValues[RTIndex] = Attribs.pClearValues[RTIndex];
    CommitSubpassRenderTargets();
}

void DeviceContextWebGPUImpl::NextSubpass()
{
    EndCommandEncoders();
    TDeviceContextBase::NextSubpass();
    CommitSubpassRenderTargets();
}

void DeviceContextWebGPUImpl::EndRenderPass()
{
    VERIFY(m_wgpuRenderPassEncoder, "There is no active render command encoder. Did you begin the render pass?");
    EndCommandEncoders();
    TDeviceContextBase::EndRenderPass();
}

void DeviceContextWebGPUImpl::Draw(const DrawAttribs& Attribs)
{
    TDeviceContextBase::Draw(Attribs, 0);

    auto wgpuRenderCmdEncoder = PrepareForDraw(Attribs.Flags);

    if (Attribs.NumVertices > 0 && Attribs.NumInstances > 0)
        wgpuRenderPassEncoderDraw(wgpuRenderCmdEncoder, Attribs.NumVertices, Attribs.NumInstances, Attribs.StartVertexLocation, Attribs.FirstInstanceLocation);
}

void DeviceContextWebGPUImpl::MultiDraw(const MultiDrawAttribs& Attribs)
{
    TDeviceContextBase::MultiDraw(Attribs, 0);

    if (Attribs.NumInstances == 0)
        return;

    auto wgpuRenderCmdEncoder = PrepareForDraw(Attribs.Flags);
    for (Uint32 DrawIdx = 0; DrawIdx < Attribs.DrawCount; ++DrawIdx)
    {
        const auto& Item = Attribs.pDrawItems[DrawIdx];
        if (Item.NumVertices > 0)
            wgpuRenderPassEncoderDraw(wgpuRenderCmdEncoder, Item.NumVertices, Attribs.NumInstances, Item.StartVertexLocation, Attribs.FirstInstanceLocation);
    }
}

void DeviceContextWebGPUImpl::DrawIndexed(const DrawIndexedAttribs& Attribs)
{
    TDeviceContextBase::DrawIndexed(Attribs, 0);

    auto wgpuRenderCmdEncoder = PrepareForIndexedDraw(Attribs.Flags, Attribs.IndexType);

    if (Attribs.NumIndices > 0 && Attribs.NumInstances > 0)
        wgpuRenderPassEncoderDrawIndexed(wgpuRenderCmdEncoder, Attribs.NumIndices, Attribs.NumInstances, Attribs.FirstIndexLocation, Attribs.BaseVertex, Attribs.FirstInstanceLocation);
}

void DeviceContextWebGPUImpl::MultiDrawIndexed(const MultiDrawIndexedAttribs& Attribs)
{
    TDeviceContextBase::MultiDrawIndexed(Attribs, 0);

    if (Attribs.NumInstances == 0)
        return;

    auto wgpuRenderCmdEncoder = PrepareForIndexedDraw(Attribs.Flags, Attribs.IndexType);
    for (Uint32 DrawIdx = 0; DrawIdx < Attribs.DrawCount; ++DrawIdx)
    {
        const auto& Item = Attribs.pDrawItems[DrawIdx];
        if (Item.NumIndices > 0)
            wgpuRenderPassEncoderDrawIndexed(wgpuRenderCmdEncoder, Item.NumIndices, Attribs.NumInstances, Item.FirstIndexLocation, Item.BaseVertex, Attribs.FirstInstanceLocation);
    }
}

void DeviceContextWebGPUImpl::DrawIndirect(const DrawIndirectAttribs& Attribs)
{
    TDeviceContextBase::DrawIndirect(Attribs, 0);

    auto wgpuRenderCmdEncoder = PrepareForDraw(Attribs.Flags);

    auto IndirectBufferOffset = Attribs.DrawArgsOffset;
    auto wgpuIndirectBuffer   = PrepareForIndirectCommand(Attribs.pAttribsBuffer, IndirectBufferOffset);

    for (Uint32 DrawIdx = 0; DrawIdx < Attribs.DrawCount; ++DrawIdx)
    {
        wgpuRenderPassEncoderDrawIndirect(wgpuRenderCmdEncoder, wgpuIndirectBuffer, IndirectBufferOffset);
        IndirectBufferOffset += Attribs.DrawArgsStride;
    }
}

void DeviceContextWebGPUImpl::DrawIndexedIndirect(const DrawIndexedIndirectAttribs& Attribs)
{
    TDeviceContextBase::DrawIndexedIndirect(Attribs, 0);

    auto wgpuRenderCmdEncoder = PrepareForIndexedDraw(Attribs.Flags, Attribs.IndexType);

    auto IndirectBufferOffset = Attribs.DrawArgsOffset;
    auto wgpuIndirectBuffer   = PrepareForIndirectCommand(Attribs.pAttribsBuffer, IndirectBufferOffset);

    for (Uint32 DrawIdx = 0; DrawIdx < Attribs.DrawCount; ++DrawIdx)
    {
        wgpuRenderPassEncoderDrawIndexedIndirect(wgpuRenderCmdEncoder, wgpuIndirectBuffer, IndirectBufferOffset);
        IndirectBufferOffset += Attribs.DrawArgsStride;
    }
}

void DeviceContextWebGPUImpl::DrawMesh(const DrawMeshAttribs& Attribs)
{
    UNSUPPORTED("DrawMesh is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::DrawMeshIndirect(const DrawMeshIndirectAttribs& Attribs)
{
    UNSUPPORTED("DrawMeshIndirect is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::DispatchCompute(const DispatchComputeAttribs& Attribs)
{
    TDeviceContextBase::DispatchCompute(Attribs, 0);

    auto wgpuComputeCmdEncoder = PrepareForDispatchCompute();

    if (Attribs.ThreadGroupCountX > 0 && Attribs.ThreadGroupCountY > 0 && Attribs.ThreadGroupCountZ > 0)
        wgpuComputePassEncoderDispatchWorkgroups(wgpuComputeCmdEncoder, Attribs.ThreadGroupCountX, Attribs.ThreadGroupCountY, Attribs.ThreadGroupCountZ);
}

void DeviceContextWebGPUImpl::DispatchComputeIndirect(const DispatchComputeIndirectAttribs& Attribs)
{
    TDeviceContextBase::DispatchComputeIndirect(Attribs, 0);

    auto wgpuComputeCmdEncoder = PrepareForDispatchCompute();

    auto IndirectBufferOffset = Attribs.DispatchArgsByteOffset;
    auto wgpuIndirectBuffer   = PrepareForIndirectCommand(Attribs.pAttribsBuffer, IndirectBufferOffset);

    wgpuComputePassEncoderDispatchWorkgroupsIndirect(wgpuComputeCmdEncoder, wgpuIndirectBuffer, IndirectBufferOffset);
}

void DeviceContextWebGPUImpl::ClearDepthStencil(ITextureView*                  pView,
                                                CLEAR_DEPTH_STENCIL_FLAGS      ClearFlags,
                                                float                          Depth,
                                                Uint8                          Stencil,
                                                RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    TDeviceContextBase::ClearDepthStencil(pView);

    if (pView != m_pBoundDepthStencil)
    {
        LOG_ERROR_MESSAGE("Depth stencil buffer must be bound to the context to be cleared in WebGPU backend");
        return;
    }

    if (m_wgpuRenderPassEncoder)
        ClearAttachment(-1, COLOR_MASK_NONE, ClearFlags, &Depth, Stencil);
    else
    {
        if (ClearFlags & CLEAR_DEPTH_FLAG)
            m_PendingClears.SetDepth(Depth);

        if (ClearFlags & CLEAR_STENCIL_FLAG)
            m_PendingClears.SetStencil(Stencil);
    }
}

void DeviceContextWebGPUImpl::ClearRenderTarget(ITextureView*                  pView,
                                                const void*                    RGBA,
                                                RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    TDeviceContextBase::ClearRenderTarget(pView);

    Int32 RTIndex = -1;
    for (Uint32 Index = 0; Index < m_NumBoundRenderTargets; ++Index)
    {
        if (m_pBoundRenderTargets[Index] == pView)
        {
            RTIndex = static_cast<Int32>(Index);
            break;
        }
    }

    if (RTIndex == -1)
    {
        LOG_ERROR_MESSAGE("Render target must be bound to the context to be cleared in WebGPU backend");
        return;
    }

    if (m_wgpuRenderPassEncoder)
        ClearAttachment(RTIndex, COLOR_MASK_ALL, CLEAR_DEPTH_FLAG_NONE, static_cast<const float*>(RGBA), 0);
    else
        m_PendingClears.SetColor(RTIndex, static_cast<const float*>(RGBA));
}

void DeviceContextWebGPUImpl::UpdateBuffer(IBuffer*                       pBuffer,
                                           Uint64                         Offset,
                                           Uint64                         Size,
                                           const void*                    pData,
                                           RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    TDeviceContextBase::UpdateBuffer(pBuffer, Offset, Size, pData, StateTransitionMode);

    EndCommandEncoders();

    auto* const pBufferWebGPU = ClassPtrCast<BufferWebGPUImpl>(pBuffer);
    const auto& BuffDesc      = pBufferWebGPU->GetDesc();
    if (BuffDesc.Usage == USAGE_DEFAULT)
    {
        const auto DynAllocation = AllocateSharedMemory(Size);
        memcpy(DynAllocation.pData, pData, StaticCast<size_t>(Size));
        wgpuCommandEncoderCopyBufferToBuffer(GetCommandEncoder(), DynAllocation.wgpuBuffer, DynAllocation.Offset,
                                             pBufferWebGPU->GetWebGPUBuffer(), Offset, Size);
    }
    else
    {
        LOG_ERROR_MESSAGE(GetUsageString(BuffDesc.Usage), " buffers can't be updated using UpdateBuffer method");
    }
}

void DeviceContextWebGPUImpl::CopyBuffer(IBuffer*                       pSrcBuffer,
                                         Uint64                         SrcOffset,
                                         RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                                         IBuffer*                       pDstBuffer,
                                         Uint64                         DstOffset,
                                         Uint64                         Size,
                                         RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode)
{
    TDeviceContextBase::CopyBuffer(pSrcBuffer, SrcOffset, SrcBufferTransitionMode, pDstBuffer, DstOffset, Size, DstBufferTransitionMode);

    EndCommandEncoders();

    auto* const pSrcBufferWebGPU = ClassPtrCast<BufferWebGPUImpl>(pSrcBuffer);
    auto* const pDstBufferWebGPU = ClassPtrCast<BufferWebGPUImpl>(pDstBuffer);

    auto wgpuSrcBuffer = pSrcBufferWebGPU->GetWebGPUBuffer();
    auto wgpuDstBuffer = pDstBufferWebGPU->GetWebGPUBuffer();

    if (wgpuSrcBuffer == nullptr)
    {
        VERIFY_EXPR(pSrcBufferWebGPU->GetDesc().Usage == USAGE_DYNAMIC);
        const auto& DynAlloc = pSrcBufferWebGPU->GetDynamicAllocation(GetContextId());
        wgpuSrcBuffer        = DynAlloc.wgpuBuffer;
        SrcOffset += DynAlloc.Offset;
    }
    DEV_CHECK_ERR(wgpuDstBuffer != nullptr, "Copying to dynamic buffers is not supported");

    wgpuCommandEncoderCopyBufferToBuffer(GetCommandEncoder(), wgpuSrcBuffer, SrcOffset, wgpuDstBuffer, DstOffset, Size);
}

void DeviceContextWebGPUImpl::MapBuffer(IBuffer*  pBuffer,
                                        MAP_TYPE  MapType,
                                        MAP_FLAGS MapFlags,
                                        PVoid&    pMappedData)
{
    TDeviceContextBase::MapBuffer(pBuffer, MapType, MapFlags, pMappedData);

    auto* const pBufferWebGPU = ClassPtrCast<BufferWebGPUImpl>(pBuffer);
    const auto& BuffDesc      = pBufferWebGPU->GetDesc();

    if (MapType == MAP_READ)
    {
        pBufferWebGPU->Map(MapType, MapFlags, pMappedData);
    }
    else if (MapType == MAP_WRITE)
    {
        if (BuffDesc.Usage == USAGE_STAGING)
        {
            pBufferWebGPU->Map(MapType, MapFlags, pMappedData);
        }
        else if (BuffDesc.Usage == USAGE_DYNAMIC)
        {
            const auto& DynAllocation = pBufferWebGPU->GetDynamicAllocation(GetContextId());
            if ((MapFlags & MAP_FLAG_DISCARD) != 0 || DynAllocation.IsEmpty())
            {
                auto Allocation = AllocateSharedMemory(BuffDesc.Size, pBufferWebGPU->GetAlignment());
                pMappedData     = Allocation.pData;
                pBufferWebGPU->SetDynamicAllocation(GetContextId(), std::move(Allocation));
            }
            else
            {
                if (pBufferWebGPU->GetWebGPUBuffer() != nullptr)
                {
                    LOG_ERROR("Formatted or structured buffers require actual WebGPU backing resource and cannot be suballocated "
                              "from dynamic heap. In current implementation, the entire contents of the backing buffer is updated when the buffer is unmapped. "
                              "As a consequence, the buffer cannot be mapped with MAP_FLAG_NO_OVERWRITE flag because updating the whole "
                              "buffer will overwrite regions that may still be in use by the GPU.");
                    return;
                }

                pMappedData = DynAllocation.pData;
            }
        }
        else
        {
            LOG_ERROR("Only USAGE_DYNAMIC, USAGE_STAGING WebGPU buffers can be mapped for writing");
        }
    }
    else if (MapType == MAP_READ_WRITE)
    {
        LOG_ERROR("MAP_READ_WRITE is not supported in WebGPU backend");
    }
    else
    {
        UNEXPECTED("Unknown map type");
    }
}

void DeviceContextWebGPUImpl::UnmapBuffer(IBuffer* pBuffer, MAP_TYPE MapType)
{
    TDeviceContextBase::UnmapBuffer(pBuffer, MapType);

    auto* const pBufferWebGPU = ClassPtrCast<BufferWebGPUImpl>(pBuffer);
    const auto& BuffDesc      = pBufferWebGPU->GetDesc();

    if (MapType == MAP_READ)
    {
        pBufferWebGPU->Unmap(MapType);
    }
    else if (MapType == MAP_WRITE)
    {
        if (BuffDesc.Usage == USAGE_STAGING)
        {
            pBufferWebGPU->Unmap(MapType);
        }
        else if (BuffDesc.Usage == USAGE_DYNAMIC)
        {
            auto wgpuBuffer = pBufferWebGPU->GetWebGPUBuffer();
            if (wgpuBuffer != nullptr)
            {
                DEV_CHECK_ERR(!m_pActiveRenderPass,
                              "Unmapping dynamic buffer with backing WebGPU resource requires "
                              "copying the data from shared memory to private storage. This can only be "
                              "done by blit encoder outside of render pass.");

                const auto& DynAllocation = pBufferWebGPU->GetDynamicAllocation(GetContextId());

                EndCommandEncoders();
                wgpuCommandEncoderCopyBufferToBuffer(GetCommandEncoder(), DynAllocation.wgpuBuffer, DynAllocation.Offset, wgpuBuffer, 0, BuffDesc.Size);
            }
        }
        else
        {
            LOG_ERROR("Only USAGE_DYNAMIC, USAGE_STAGING WebGPU buffers can be mapped for writing");
        }
    }
}

void DeviceContextWebGPUImpl::UpdateTexture(ITexture*                      pTexture,
                                            Uint32                         MipLevel,
                                            Uint32                         Slice,
                                            const Box&                     DstBox,
                                            const TextureSubResData&       SubresData,
                                            RESOURCE_STATE_TRANSITION_MODE SrcBufferStateTransitionMode,
                                            RESOURCE_STATE_TRANSITION_MODE DstTextureStateTransitionMode)
{
    TDeviceContextBase::UpdateTexture(pTexture, MipLevel, Slice, DstBox, SubresData, SrcBufferStateTransitionMode, DstTextureStateTransitionMode);

    if (SubresData.pSrcBuffer != nullptr)
    {
        UNSUPPORTED("Copy buffer to texture is not implemented");
        return;
    }

    EndCommandEncoders();

    constexpr auto BufferToTextureCopyAlignment = 16u;

    auto* const pTextureWebGPU = ClassPtrCast<TextureWebGPUImpl>(pTexture);

    const auto& TexDesc           = pTextureWebGPU->GetDesc();
    const auto  CopyInfo          = GetBufferToTextureCopyInfo(TexDesc.Format, DstBox, BufferToTextureCopyAlignment);
    const auto  UpdateWidth       = DstBox.Width();
    const auto  UpdateHeight      = DstBox.Height();
    const auto  UpdateDepth       = DstBox.Depth();
    const auto  UpdateRegionDepth = CopyInfo.Region.Depth();

    const auto DynAllocation = AllocateSharedMemory(CopyInfo.MemorySize);

    for (Uint32 LayerIdx = 0; LayerIdx < UpdateRegionDepth; ++LayerIdx)
    {
        for (Uint32 RawIdx = 0; RawIdx < CopyInfo.RowCount; ++RawIdx)
        {
            const auto SrcOffset = RawIdx * SubresData.Stride + LayerIdx * SubresData.DepthStride;
            const auto DstOffset = RawIdx * CopyInfo.RowStride + LayerIdx * CopyInfo.DepthStride;
            memcpy(DynAllocation.pData + DstOffset, static_cast<const uint8_t*>(SubresData.pData) + SrcOffset, StaticCast<size_t>(CopyInfo.RowSize));
        }
    }

    WGPUImageCopyBuffer wgpuImageCopySrc{};
    wgpuImageCopySrc.buffer              = DynAllocation.wgpuBuffer;
    wgpuImageCopySrc.layout.offset       = DynAllocation.Offset;
    wgpuImageCopySrc.layout.bytesPerRow  = static_cast<Uint32>(CopyInfo.RowStride);
    wgpuImageCopySrc.layout.rowsPerImage = static_cast<Uint32>(CopyInfo.DepthStride);

    WGPUImageCopyTexture wgpuImageCopyDst{};
    wgpuImageCopyDst.texture  = pTextureWebGPU->GetWebGPUTexture();
    wgpuImageCopyDst.aspect   = WGPUTextureAspect_All;
    wgpuImageCopyDst.origin.x = DstBox.MinX;
    wgpuImageCopyDst.origin.y = DstBox.MinY;
    wgpuImageCopyDst.origin.z = DstBox.MaxZ;
    wgpuImageCopyDst.mipLevel = MipLevel;

    WGPUExtent3D wgpuCopySize{};
    wgpuCopySize.width              = UpdateWidth;
    wgpuCopySize.height             = UpdateHeight;
    wgpuCopySize.depthOrArrayLayers = UpdateDepth;

    wgpuCommandEncoderCopyBufferToTexture(GetCommandEncoder(), &wgpuImageCopySrc, &wgpuImageCopyDst, &wgpuCopySize);
}

void DeviceContextWebGPUImpl::CopyTexture(const CopyTextureAttribs& CopyAttribs)
{
    TDeviceContextBase::CopyTexture(CopyAttribs);

    EndCommandEncoders();

    auto* const pSrcTexWebGPU = ClassPtrCast<TextureWebGPUImpl>(CopyAttribs.pSrcTexture);
    auto* const pDstTexWebGPU = ClassPtrCast<TextureWebGPUImpl>(CopyAttribs.pDstTexture);

    const auto& SrcTexDesc = pSrcTexWebGPU->GetDesc();
    const auto& DstTexDesc = pDstTexWebGPU->GetDesc();

    auto wgpuCmdEncoder = GetCommandEncoder();

    auto  FullMipBox = Box{};
    auto* pSrcBox    = CopyAttribs.pSrcBox;

    if (pSrcBox == nullptr)
    {
        auto MipLevelAttribs = GetMipLevelProperties(SrcTexDesc, CopyAttribs.SrcMipLevel);
        FullMipBox.MaxX      = MipLevelAttribs.LogicalWidth;
        FullMipBox.MaxY      = MipLevelAttribs.LogicalHeight;
        FullMipBox.MaxZ      = MipLevelAttribs.Depth;
        pSrcBox              = &FullMipBox;
    }

    if (SrcTexDesc.Usage != USAGE_STAGING && DstTexDesc.Usage != USAGE_STAGING)
    {
        const auto& DstFmtAttribs = GetTextureFormatAttribs(DstTexDesc.Format);

        WGPUTextureAspect wgpuAspectMask = {};
        if (DstFmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH)
            wgpuAspectMask = WGPUTextureAspect_DepthOnly;
        else
            wgpuAspectMask = WGPUTextureAspect_All;

        WGPUImageCopyTexture wgpuImageCopySrc{};
        wgpuImageCopySrc.texture  = pSrcTexWebGPU->GetWebGPUTexture();
        wgpuImageCopySrc.aspect   = wgpuAspectMask;
        wgpuImageCopySrc.origin.x = pSrcBox->MinX;
        wgpuImageCopySrc.origin.y = pSrcBox->MinY;
        wgpuImageCopySrc.origin.z = pSrcBox->MinZ;
        wgpuImageCopySrc.mipLevel = CopyAttribs.SrcMipLevel;

        WGPUImageCopyTexture wgpuImageCopyDst{};
        wgpuImageCopyDst.texture  = pDstTexWebGPU->GetWebGPUTexture();
        wgpuImageCopyDst.aspect   = wgpuAspectMask;
        wgpuImageCopyDst.origin.x = CopyAttribs.DstX;
        wgpuImageCopyDst.origin.y = CopyAttribs.DstY;
        wgpuImageCopyDst.origin.z = CopyAttribs.DstZ;
        wgpuImageCopyDst.mipLevel = CopyAttribs.DstMipLevel;

        WGPUExtent3D wgpuCopySize{};
        wgpuCopySize.width              = std::max(pSrcBox->Width(), 1u);
        wgpuCopySize.height             = std::max(pSrcBox->Height(), 1u);
        wgpuCopySize.depthOrArrayLayers = std::max(pSrcBox->Depth(), 1u);

        wgpuCommandEncoderCopyTextureToTexture(wgpuCmdEncoder, &wgpuImageCopySrc, &wgpuImageCopyDst, &wgpuCopySize);
    }
    else if (SrcTexDesc.Usage == USAGE_STAGING && DstTexDesc.Usage != USAGE_STAGING)
    {
        const auto SrcBufferOffset = GetStagingTextureLocationOffset(SrcTexDesc, CopyAttribs.SrcSlice, CopyAttribs.SrcMipLevel, TextureWebGPUImpl::StagingDataAlignment, pSrcBox->MinX, pSrcBox->MinY, pSrcBox->MinZ);

        const auto SrcMipLevelAttribs = GetMipLevelProperties(SrcTexDesc, CopyAttribs.SrcMipLevel);

        const auto& DstFmtAttribs = GetTextureFormatAttribs(DstTexDesc.Format);

        WGPUTextureAspect wgpuAspectMask = {};
        if (DstFmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH)
            wgpuAspectMask = WGPUTextureAspect_DepthOnly;
        else
            wgpuAspectMask = WGPUTextureAspect_All;

        WGPUImageCopyBuffer wgpuImageCopySrc{};
        wgpuImageCopySrc.buffer              = pSrcTexWebGPU->GetWebGPUStagingBuffer();
        wgpuImageCopySrc.layout.offset       = SrcBufferOffset;
        wgpuImageCopySrc.layout.bytesPerRow  = static_cast<Uint32>(SrcMipLevelAttribs.RowSize);
        wgpuImageCopySrc.layout.rowsPerImage = SrcMipLevelAttribs.StorageHeight / DstFmtAttribs.BlockHeight;

        WGPUImageCopyTexture wgpuImageCopyDst{};
        wgpuImageCopyDst.texture  = pDstTexWebGPU->GetWebGPUTexture();
        wgpuImageCopyDst.aspect   = wgpuAspectMask;
        wgpuImageCopyDst.origin.x = CopyAttribs.DstX;
        wgpuImageCopyDst.origin.y = CopyAttribs.DstY;
        wgpuImageCopyDst.origin.z = CopyAttribs.DstZ;
        wgpuImageCopyDst.mipLevel = CopyAttribs.DstMipLevel;

        WGPUExtent3D wgpuCopySize{};
        wgpuCopySize.width              = std::max(pSrcBox->Width(), 1u);
        wgpuCopySize.height             = std::max(pSrcBox->Height(), 1u);
        wgpuCopySize.depthOrArrayLayers = std::max(pSrcBox->Depth(), 1u);

        wgpuCommandEncoderCopyBufferToTexture(wgpuCmdEncoder, &wgpuImageCopySrc, &wgpuImageCopyDst, &wgpuCopySize);
    }
    else if (SrcTexDesc.Usage != USAGE_STAGING && DstTexDesc.Usage == USAGE_STAGING)
    {
        const auto DstBufferOffset = GetStagingTextureLocationOffset(DstTexDesc, CopyAttribs.DstSlice, CopyAttribs.DstMipLevel, TextureWebGPUImpl::StagingDataAlignment, CopyAttribs.DstX, CopyAttribs.DstY, CopyAttribs.DstZ);

        const auto DstMipLevelAttribs = GetMipLevelProperties(DstTexDesc, CopyAttribs.DstMipLevel);

        const auto& SrcFmtAttribs = GetTextureFormatAttribs(SrcTexDesc.Format);

        WGPUTextureAspect wgpuAspectMask = {};
        if (SrcFmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH)
            wgpuAspectMask = WGPUTextureAspect_DepthOnly;
        else
            wgpuAspectMask = WGPUTextureAspect_All;

        WGPUImageCopyTexture wgpuImageCopySrc{};
        wgpuImageCopySrc.texture  = pSrcTexWebGPU->GetWebGPUTexture();
        wgpuImageCopySrc.aspect   = wgpuAspectMask;
        wgpuImageCopySrc.origin.x = pSrcBox->MinX;
        wgpuImageCopySrc.origin.y = pSrcBox->MinY;
        wgpuImageCopySrc.origin.z = pSrcBox->MinZ;
        wgpuImageCopySrc.mipLevel = CopyAttribs.SrcMipLevel;

        WGPUImageCopyBuffer wgpuImageCopyDst{};
        wgpuImageCopyDst.buffer              = pDstTexWebGPU->GetWebGPUStagingBuffer();
        wgpuImageCopyDst.layout.offset       = DstBufferOffset;
        wgpuImageCopyDst.layout.bytesPerRow  = static_cast<Uint32>(DstMipLevelAttribs.RowSize);
        wgpuImageCopyDst.layout.rowsPerImage = DstMipLevelAttribs.StorageHeight / SrcFmtAttribs.BlockHeight;

        WGPUExtent3D wgpuCopySize{};
        wgpuCopySize.width              = std::max(pSrcBox->Width(), 1u);
        wgpuCopySize.height             = std::max(pSrcBox->Height(), 1u);
        wgpuCopySize.depthOrArrayLayers = std::max(pSrcBox->Depth(), 1u);

        wgpuCommandEncoderCopyTextureToBuffer(wgpuCmdEncoder, &wgpuImageCopySrc, &wgpuImageCopyDst, &wgpuCopySize);
    }
    else
    {
        UNSUPPORTED("Copying data between staging textures is not supported and is likely not want you really want to do");
    }
}

void DeviceContextWebGPUImpl::MapTextureSubresource(ITexture*                 pTexture,
                                                    Uint32                    MipLevel,
                                                    Uint32                    ArraySlice,
                                                    MAP_TYPE                  MapType,
                                                    MAP_FLAGS                 MapFlags,
                                                    const Box*                pMapRegion,
                                                    MappedTextureSubresource& MappedData)
{
    TDeviceContextBase::MapTextureSubresource(pTexture, MipLevel, ArraySlice, MapType, MapFlags, pMapRegion, MappedData);

    EndCommandEncoders();

    auto* const pTextureWebGPU = ClassPtrCast<TextureWebGPUImpl>(pTexture);
    const auto& TexDesc        = pTextureWebGPU->GetDesc();

    Box FullExtentBox;
    if (pMapRegion == nullptr)
    {
        auto MipLevelAttribs = GetMipLevelProperties(TexDesc, MipLevel);
        FullExtentBox.MaxX   = MipLevelAttribs.LogicalWidth;
        FullExtentBox.MaxY   = MipLevelAttribs.LogicalHeight;
        FullExtentBox.MaxZ   = MipLevelAttribs.Depth;
        pMapRegion           = &FullExtentBox;
    }

    if (TexDesc.Usage == USAGE_DYNAMIC)
    {
        if (MapType != MAP_WRITE)
        {
            LOG_ERROR("Dynamic textures can only be mapped for writing in WebGPU backend");
            MappedData = MappedTextureSubresource{};
            return;
        }

        if ((MapFlags & (MAP_FLAG_DISCARD | MAP_FLAG_NO_OVERWRITE)) != 0)
            LOG_INFO_MESSAGE_ONCE("Mapping textures with flags MAP_FLAG_DISCARD or MAP_FLAG_NO_OVERWRITE has no effect in WebGPU backend");

        const auto CopyInfo      = GetBufferToTextureCopyInfo(TexDesc.Format, *pMapRegion, TextureWebGPUImpl::CopyTextureRawStride);
        const auto DynAllocation = AllocateSharedMemory(CopyInfo.MemorySize, TextureWebGPUImpl::CopyTextureRawStride);

        MappedData.pData       = DynAllocation.pData;
        MappedData.Stride      = CopyInfo.RowStride;
        MappedData.DepthStride = CopyInfo.DepthStride;

        const auto Iter = m_MappedTextures.emplace(MappedTextureKey{pTextureWebGPU->GetUniqueID(), MipLevel, ArraySlice}, MappedTexture{CopyInfo, DynAllocation});
        if (!Iter.second)
            LOG_ERROR_MESSAGE("Mip level ", MipLevel, ", slice ", ArraySlice, " of texture '", TexDesc.Name, "' has already been mapped");
    }
    else if (TexDesc.Usage == USAGE_STAGING)
    {
        auto LocationOffset = GetStagingTextureLocationOffset(
            TexDesc, ArraySlice, MipLevel, TextureWebGPUImpl::StagingDataAlignment,
            pMapRegion->MinX, pMapRegion->MinY, pMapRegion->MinZ);

        const auto MipInfo = GetMipLevelProperties(TexDesc, MipLevel);

        MappedData.pData       = static_cast<Uint8*>(pTextureWebGPU->Map(MapType, MapFlags)) + LocationOffset;
        MappedData.Stride      = AlignUp(MipInfo.RowSize, TextureWebGPUImpl::StagingDataAlignment);
        MappedData.DepthStride = MappedData.Stride * MipInfo.StorageHeight;

        if (MapType == MAP_READ)
        {
            if ((MapFlags & MAP_FLAG_DO_NOT_WAIT) == 0)
            {
                LOG_WARNING_MESSAGE("WebGPU backend never waits for GPU when mapping staging textures for reading. "
                                    "Applications must use fences or other synchronization methods to explicitly synchronize "
                                    "access and use MAP_FLAG_DO_NOT_WAIT flag.");
            }
            DEV_CHECK_ERR((TexDesc.CPUAccessFlags & CPU_ACCESS_READ), "Texture '", TexDesc.Name, "' was not created with CPU_ACCESS_READ flag and can't be mapped for reading");
        }
        else if (MapType == MAP_WRITE)
        {
            DEV_CHECK_ERR((TexDesc.CPUAccessFlags & CPU_ACCESS_WRITE), "Texture '", TexDesc.Name, "' was not created with CPU_ACCESS_WRITE flag and can't be mapped for writing");
        }
    }
    else
    {
        UNSUPPORTED(GetUsageString(TexDesc.Usage), " textures cannot be mapped in WebGPU back-end");
    }
}

void DeviceContextWebGPUImpl::UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice)
{
    TDeviceContextBase::UnmapTextureSubresource(pTexture, MipLevel, ArraySlice);

    EndCommandEncoders();

    auto* const pTextureWebGPU = ClassPtrCast<TextureWebGPUImpl>(pTexture);
    const auto& TexDesc        = pTextureWebGPU->GetDesc();

    if (TexDesc.Usage == USAGE_DYNAMIC)
    {
        const auto UploadSpaceIt = m_MappedTextures.find(MappedTextureKey{pTextureWebGPU->GetUniqueID(), MipLevel, ArraySlice});
        if (UploadSpaceIt != m_MappedTextures.end())
        {
            const auto& Allocation = UploadSpaceIt->second.Allocation;
            const auto& CopyInfo   = UploadSpaceIt->second.CopyInfo;

            WGPUImageCopyBuffer wgpuImageCopySrc{};
            wgpuImageCopySrc.buffer              = Allocation.wgpuBuffer;
            wgpuImageCopySrc.layout.offset       = Allocation.Offset;
            wgpuImageCopySrc.layout.bytesPerRow  = static_cast<Uint32>(CopyInfo.RowStride);
            wgpuImageCopySrc.layout.rowsPerImage = static_cast<Uint32>(CopyInfo.DepthStride);

            WGPUImageCopyTexture wgpuImageCopyDst{};
            wgpuImageCopyDst.texture  = pTextureWebGPU->GetWebGPUTexture();
            wgpuImageCopyDst.aspect   = WGPUTextureAspect_All;
            wgpuImageCopyDst.origin.x = CopyInfo.Region.MinX;
            wgpuImageCopyDst.origin.y = CopyInfo.Region.MinY;
            wgpuImageCopyDst.origin.z = CopyInfo.Region.MinZ;
            wgpuImageCopyDst.mipLevel = UploadSpaceIt->first.MipLevel;

            WGPUExtent3D wgpuCopySize{};
            wgpuCopySize.width              = CopyInfo.Region.Width();
            wgpuCopySize.height             = CopyInfo.Region.Height();
            wgpuCopySize.depthOrArrayLayers = CopyInfo.Region.Depth();

            wgpuCommandEncoderCopyBufferToTexture(GetCommandEncoder(), &wgpuImageCopySrc, &wgpuImageCopyDst, &wgpuCopySize);
            m_MappedTextures.erase(UploadSpaceIt);
        }
        else
        {
            LOG_ERROR_MESSAGE("Failed to unmap mip level ", MipLevel, ", slice ", ArraySlice, " of texture '", TexDesc.Name, "'. The texture has either been unmapped already or has not been mapped");
        }
    }
    else if (TexDesc.Usage == USAGE_STAGING)
    {
        pTextureWebGPU->Unmap();
    }
    else
    {
        UNSUPPORTED(GetUsageString(TexDesc.Usage), " textures cannot be mapped in Metal back-end");
    }
}

void DeviceContextWebGPUImpl::FinishCommandList(ICommandList** ppCommandList)
{
    LOG_ERROR("Deferred contexts are not supported in WebGPU");
}

void DeviceContextWebGPUImpl::ExecuteCommandLists(Uint32 NumCommandLists, ICommandList* const* ppCommandLists)
{
    LOG_ERROR("Deferred contexts are not supported in WebGPU");
}

void DeviceContextWebGPUImpl::EnqueueSignal(IFence* pFence, Uint64 Value)
{
    TDeviceContextBase::EnqueueSignal(pFence, Value, 0);
    m_SignalFences.emplace_back(std::make_pair(Value, ClassPtrCast<FenceWebGPUImpl>(pFence)));
}

void DeviceContextWebGPUImpl::DeviceWaitForFence(IFence* pFence, Uint64 Value)
{
    DEV_ERROR("DeviceWaitForFence() is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::WaitForIdle()
{
    m_FenceValue++;
    EnqueueSignal(m_pFence, m_FenceValue);
    Flush();
    m_pFence->Wait(m_FenceValue);
}

void DeviceContextWebGPUImpl::BeginQuery(IQuery* pQuery)
{
    TDeviceContextBase::BeginQuery(pQuery, 0);

    // auto* pQueryWebGPUImpl = ClassPtrCast<QueryWebGPUImpl>(pQuery);
    // auto  QueryType        = pQueryWebGPUImpl->GetDesc().Type;

    //TODO
}

void DeviceContextWebGPUImpl::EndQuery(IQuery* pQuery)
{
    TDeviceContextBase::EndQuery(pQuery, 0);

    // auto* pQueryWebGPUImpl = ClassPtrCast<QueryWebGPUImpl>(pQuery);
    // auto  QueryType        = pQueryWebGPUImpl->GetDesc().Type;

    //TODO
}

void DeviceContextWebGPUImpl::Flush()
{
    EndCommandEncoders();

    for (auto& MemPage : m_SharedMemPages)
        wgpuQueueWriteBuffer(m_wgpuQueue, MemPage.wgpuBuffer.Get(), 0, MemPage.pData, StaticCast<size_t>(MemPage.PageSize));

    if (m_wgpuCommandEncoder || !m_SignalFences.empty())
    {
        auto WorkDoneCallback = [](WGPUQueueWorkDoneStatus Status, void* pUserData) {
            if (DeviceContextWebGPUImpl* pDeviceCxt = static_cast<DeviceContextWebGPUImpl*>(pUserData))
            {
                for (auto& SignalItem : pDeviceCxt->m_SignalFences)
                {
                    auto* pFence = SignalItem.second.RawPtr<FenceWebGPUImpl>();
                    pFence->SetCompletedValue(SignalItem.first);
                }
                pDeviceCxt->m_SignalFences.clear();
            }

            if (Status != WGPUQueueWorkDoneStatus_Success)
                DEV_ERROR("Failed wgpuQueueOnSubmittedWorkDone: ", Status);
        };

        WGPUCommandBufferDescriptor wgpuCmdBufferDesc{};
        WGPUCommandBuffer           wgpuCmdBuffer = wgpuCommandEncoderFinish(GetCommandEncoder(), &wgpuCmdBufferDesc);
        DEV_CHECK_ERR(wgpuCmdBuffer != nullptr, "Failed to finish command encoder");

        wgpuQueueOnSubmittedWorkDone(m_wgpuQueue, WorkDoneCallback, this);
        wgpuQueueSubmit(m_wgpuQueue, 1, &wgpuCmdBuffer);
        wgpuCommandEncoderRelease(m_wgpuCommandEncoder);
        m_wgpuCommandEncoder = nullptr;
    }
}

void DeviceContextWebGPUImpl::BuildBLAS(const BuildBLASAttribs& Attribs)
{
    UNSUPPORTED("BuildBLAS is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::BuildTLAS(const BuildTLASAttribs& Attribs)
{
    UNSUPPORTED("BuildTLAS is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::CopyBLAS(const CopyBLASAttribs& Attribs)
{
    UNSUPPORTED("CopyBLAS is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::CopyTLAS(const CopyTLASAttribs& Attribs)
{
    UNSUPPORTED("CopyTLAS is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::WriteBLASCompactedSize(const WriteBLASCompactedSizeAttribs& Attribs)
{
    UNSUPPORTED("WriteBLASCompactedSize is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::WriteTLASCompactedSize(const WriteTLASCompactedSizeAttribs& Attribs)
{
    UNSUPPORTED("WriteTLASCompactedSize is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::TraceRays(const TraceRaysAttribs& Attribs)
{
    UNSUPPORTED("TraceRays is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::TraceRaysIndirect(const TraceRaysIndirectAttribs& Attribs)
{
    UNSUPPORTED("TraceRaysIndirect is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::UpdateSBT(IShaderBindingTable*                 pSBT,
                                        const UpdateIndirectRTBufferAttribs* pUpdateIndirectBufferAttribs)
{
    UNSUPPORTED("UpdateSBT is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::SetShadingRate(SHADING_RATE          BaseRate,
                                             SHADING_RATE_COMBINER PrimitiveCombiner,
                                             SHADING_RATE_COMBINER TextureCombiner)
{
    UNSUPPORTED("SetShadingRate is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::BindSparseResourceMemory(const BindSparseResourceMemoryAttribs& Attribs)
{
    UNSUPPORTED("BindSparseResourceMemory is not supported in WebGPU");
}

void DeviceContextWebGPUImpl::BeginDebugGroup(const Char* Name, const float* pColor)
{
    VERIFY(!m_wgpuRenderPassEncoder && !m_wgpuComputePassEncoder, "Another command encoder is currently active");
    TDeviceContextBase::BeginDebugGroup(Name, pColor, 0);

    if (m_wgpuRenderPassEncoder)
        wgpuRenderPassEncoderPushDebugGroup(GetRenderPassCommandEncoder(), Name);
    else if (m_wgpuComputePassEncoder)
        wgpuComputePassEncoderPushDebugGroup(GetComputePassCommandEncoder(), Name);
    else
        wgpuCommandEncoderPushDebugGroup(GetCommandEncoder(), Name);
}

void DeviceContextWebGPUImpl::EndDebugGroup()
{
    VERIFY(!m_wgpuRenderPassEncoder && !m_wgpuComputePassEncoder, "Another command encoder is currently active");
    TDeviceContextBase::EndDebugGroup(0);

    if (m_wgpuRenderPassEncoder)
        wgpuRenderPassEncoderPopDebugGroup(GetRenderPassCommandEncoder());
    else if (m_wgpuComputePassEncoder)
        wgpuComputePassEncoderPopDebugGroup(GetComputePassCommandEncoder());
    else
        wgpuCommandEncoderPopDebugGroup(GetCommandEncoder());
}

void DeviceContextWebGPUImpl::InsertDebugLabel(const Char* Label, const float* pColor)
{
    VERIFY(!m_wgpuRenderPassEncoder && !m_wgpuComputePassEncoder, "Another command encoder is currently active");
    TDeviceContextBase::InsertDebugLabel(Label, pColor, 0);

    if (m_wgpuRenderPassEncoder)
        wgpuRenderPassEncoderInsertDebugMarker(GetRenderPassCommandEncoder(), Label);
    else if (m_wgpuComputePassEncoder)
        wgpuComputePassEncoderInsertDebugMarker(GetComputePassCommandEncoder(), Label);
    else
        wgpuCommandEncoderInsertDebugMarker(GetCommandEncoder(), Label);
}

void DeviceContextWebGPUImpl::GenerateMips(ITextureView* pTexView)
{
    TDeviceContextBase::GenerateMips(pTexView);

    auto& MipGenerator = m_pDevice->GetMipsGenerator();
    auto  CmdEncoder   = GetComputePassCommandEncoder();
    MipGenerator.GenerateMips(CmdEncoder, ClassPtrCast<TextureViewWebGPUImpl>(pTexView));
}

void DeviceContextWebGPUImpl::FinishFrame()
{
    if (m_wgpuCommandEncoder != nullptr)
    {
        LOG_ERROR_MESSAGE("There are outstanding commands in the immediate device context when finishing the frame."
                          " This is an error and may cause unpredicted behaviour. Call Flush() to submit all commands"
                          " for execution before finishing the frame.");
    }

    if (m_pActiveRenderPass != nullptr)
        LOG_ERROR_MESSAGE("Finishing frame inside an active render pass.");

    if (!m_MappedTextures.empty())
        LOG_ERROR_MESSAGE("There are mapped textures in the device context when finishing the frame. All dynamic resources must be used in the same frame in which they are mapped.");

    for (auto& MemPage : m_SharedMemPages)
        MemPage.Recycle();
    m_SharedMemPages.clear();

    TDeviceContextBase::EndFrame();
}

void DeviceContextWebGPUImpl::TransitionResourceStates(Uint32 BarrierCount, const StateTransitionDesc* pResourceBarriers) {}

ICommandQueue* DeviceContextWebGPUImpl::LockCommandQueue() { return nullptr; }

void DeviceContextWebGPUImpl::UnlockCommandQueue() {}

void DeviceContextWebGPUImpl::ResolveTextureSubresource(ITexture*                               pSrcTexture,
                                                        ITexture*                               pDstTexture,
                                                        const ResolveTextureSubresourceAttribs& ResolveAttribs)
{
    TDeviceContextBase::ResolveTextureSubresource(pSrcTexture, pDstTexture, ResolveAttribs);

#ifdef DILIGENT_DEVELOPMENT
    LOG_WARNING_MESSAGE_ONCE("ResolveTextureSubresource is suboptimal in WebGPU. Use render pass resolve attachments instead");
#endif

    EndCommandEncoders();

    const auto& SrcTexDesc = pSrcTexture->GetDesc();
    const auto& FmtAttribs = GetTextureFormatAttribs(SrcTexDesc.Format);

    if (FmtAttribs.ComponentType != COMPONENT_TYPE_DEPTH && FmtAttribs.ComponentType != COMPONENT_TYPE_DEPTH_STENCIL)
    {
        const auto* pSrcRTVWebGPU = ClassPtrCast<TextureViewWebGPUImpl>(pSrcTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET));
        const auto* pDstRTVWebGPU = ClassPtrCast<TextureViewWebGPUImpl>(pDstTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET));

        WGPURenderPassDescriptor      wgpuRenderPassDesc{};
        WGPURenderPassColorAttachment wgpuRenderPassColorAttachment{};

        wgpuRenderPassColorAttachment.loadOp        = WGPULoadOp_Load;
        wgpuRenderPassColorAttachment.storeOp       = WGPUStoreOp_Discard;
        wgpuRenderPassColorAttachment.view          = pSrcRTVWebGPU->GetWebGPUTextureView();
        wgpuRenderPassColorAttachment.resolveTarget = pDstRTVWebGPU->GetWebGPUTextureView();
        wgpuRenderPassColorAttachment.depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED;

        wgpuRenderPassDesc.colorAttachmentCount = 1;
        wgpuRenderPassDesc.colorAttachments     = &wgpuRenderPassColorAttachment;

        WGPURenderPassEncoder wgpuRenderPassEncoder = wgpuCommandEncoderBeginRenderPass(GetCommandEncoder(), &wgpuRenderPassDesc);
        DEV_CHECK_ERR(wgpuRenderPassEncoder != nullptr, "Failed to begin render pass");
        wgpuRenderPassEncoderEnd(wgpuRenderPassEncoder);
    }
    else
    {
        LOG_ERROR_MESSAGE("ResolveTextureSubresource is not supported for the depth attachment");
    }
}

WGPUQueue DeviceContextWebGPUImpl::GetWebGPUQueue()
{
    return m_wgpuQueue;
}

WGPUCommandEncoder DeviceContextWebGPUImpl::GetCommandEncoder()
{
    if (!m_wgpuCommandEncoder)
    {
        WGPUCommandEncoderDescriptor wgpuCommandEncoderDesc{};
        m_wgpuCommandEncoder = wgpuDeviceCreateCommandEncoder(m_pDevice->GetWebGPUDevice(), &wgpuCommandEncoderDesc);
        DEV_CHECK_ERR(m_wgpuCommandEncoder != nullptr, "Failed wgpuDeviceCreateCommandEncoder");
    }

    return m_wgpuCommandEncoder;
}

WGPURenderPassEncoder DeviceContextWebGPUImpl::GetRenderPassCommandEncoder()
{
    if (!m_wgpuRenderPassEncoder)
    {
        EndCommandEncoders(COMMAND_ENCODER_FLAG_ALL & ~COMMAND_ENCODER_FLAG_RENDER);
        CommitRenderTargets();
    }

    return m_wgpuRenderPassEncoder;
}

WGPUComputePassEncoder DeviceContextWebGPUImpl::GetComputePassCommandEncoder()
{
    if (!m_wgpuComputePassEncoder)
    {
        EndCommandEncoders(COMMAND_ENCODER_FLAG_ALL & ~COMMAND_ENCODER_FLAG_COMPUTE);

        WGPUComputePassDescriptor wgpuComputePassDesc{};
        m_wgpuComputePassEncoder = wgpuCommandEncoderBeginComputePass(GetCommandEncoder(), &wgpuComputePassDesc);
        DEV_CHECK_ERR(m_wgpuComputePassEncoder != nullptr, "Failed to begin compute pass");
    }
    return m_wgpuComputePassEncoder;
}

void DeviceContextWebGPUImpl::EndCommandEncoders(Uint32 EncoderFlags)
{
    if ((EncoderFlags & COMMAND_ENCODER_FLAG_RENDER) != 0)
    {
        if (m_PendingClears.AnyPending())
        {
            VERIFY(!m_wgpuRenderPassEncoder, "There should be no active render command encoder when pending clears mask is not zero");
            VERIFY(!m_pActiveRenderPass, "There should be no pending clears inside a render pass");
            CommitRenderTargets();
        }

        if (m_wgpuRenderPassEncoder)
        {
            wgpuRenderPassEncoderEnd(m_wgpuRenderPassEncoder);
            wgpuRenderPassEncoderRelease(m_wgpuRenderPassEncoder);
            m_wgpuRenderPassEncoder = nullptr;
            ClearEncoderState();
        }
    }

    if ((EncoderFlags & COMMAND_ENCODER_FLAG_COMPUTE) != 0)
    {
        if (m_wgpuComputePassEncoder)
        {
            wgpuComputePassEncoderEnd(m_wgpuComputePassEncoder);
            wgpuComputePassEncoderRelease(m_wgpuComputePassEncoder);
            m_wgpuComputePassEncoder = nullptr;
            ClearEncoderState();
        }
    }
}

void DeviceContextWebGPUImpl::CommitRenderTargets()
{
    VERIFY(!m_wgpuRenderPassEncoder && !m_wgpuComputePassEncoder, "Another command encoder is currently active");

    WGPURenderPassDescriptor             wgpuRenderPassDesc{};
    WGPURenderPassColorAttachment        wgpuRenderPassColorAttachments[MAX_RENDER_TARGETS]{};
    WGPURenderPassDepthStencilAttachment wgpuRenderPassDepthStencilAttachment{};
    for (Uint32 RTIndex = 0; RTIndex < m_NumBoundRenderTargets; ++RTIndex)
    {
        if (auto* pRTV = m_pBoundRenderTargets[RTIndex].RawPtr())
        {
            const auto& ClearColor = m_PendingClears.Colors[RTIndex];

            wgpuRenderPassColorAttachments[RTIndex].view       = pRTV->GetWebGPUTextureView();
            wgpuRenderPassColorAttachments[RTIndex].storeOp    = WGPUStoreOp_Store;
            wgpuRenderPassColorAttachments[RTIndex].loadOp     = m_PendingClears.ColorPending(RTIndex) ? WGPULoadOp_Clear : WGPULoadOp_Load;
            wgpuRenderPassColorAttachments[RTIndex].clearValue = WGPUColor{ClearColor[0], ClearColor[1], ClearColor[2], ClearColor[3]};
            wgpuRenderPassColorAttachments[RTIndex].depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        }

        wgpuRenderPassDesc.colorAttachments     = wgpuRenderPassColorAttachments;
        wgpuRenderPassDesc.colorAttachmentCount = m_NumBoundRenderTargets;
    }

    if (m_pBoundDepthStencil)
    {
        wgpuRenderPassDepthStencilAttachment.view            = m_pBoundDepthStencil->GetWebGPUTextureView();
        wgpuRenderPassDepthStencilAttachment.depthLoadOp     = m_PendingClears.DepthPending() ? WGPULoadOp_Clear : WGPULoadOp_Load;
        wgpuRenderPassDepthStencilAttachment.depthStoreOp    = WGPUStoreOp_Store;
        wgpuRenderPassDepthStencilAttachment.depthClearValue = m_PendingClears.Depth;

        wgpuRenderPassDepthStencilAttachment.stencilLoadOp     = m_PendingClears.StencilPending() ? WGPULoadOp_Clear : WGPULoadOp_Load;
        wgpuRenderPassDepthStencilAttachment.stencilStoreOp    = WGPUStoreOp_Store;
        wgpuRenderPassDepthStencilAttachment.stencilClearValue = m_PendingClears.Stencil;

        wgpuRenderPassDesc.depthStencilAttachment = &wgpuRenderPassDepthStencilAttachment;
    }

    m_wgpuRenderPassEncoder = wgpuCommandEncoderBeginRenderPass(GetCommandEncoder(), &wgpuRenderPassDesc);
    DEV_CHECK_ERR(m_wgpuRenderPassEncoder != nullptr, "Failed to begin render pass");
    m_PendingClears.ResetFlags();
}

void DeviceContextWebGPUImpl::CommitSubpassRenderTargets()
{
    VERIFY(!m_wgpuRenderPassEncoder && !m_wgpuComputePassEncoder, "Another command encoder is currently active");
    VERIFY_EXPR(m_pActiveRenderPass);
    const auto& RPDesc = m_pActiveRenderPass->GetDesc();
    VERIFY_EXPR(m_pBoundFramebuffer);
    const auto& FBDesc = m_pBoundFramebuffer->GetDesc();
    VERIFY_EXPR(m_SubpassIndex < RPDesc.SubpassCount);
    const auto& Subpass = RPDesc.pSubpasses[m_SubpassIndex];
    VERIFY(Subpass.RenderTargetAttachmentCount == m_NumBoundRenderTargets,
           "The number of currently bound render targets (", m_NumBoundRenderTargets,
           ") is not consistent with the number of render target attachments (", Subpass.RenderTargetAttachmentCount,
           ") in current subpass");

    WGPURenderPassColorAttachment RenderPassColorAttachments[MAX_RENDER_TARGETS]{};
    for (Uint32 RTIndex = 0; RTIndex < m_NumBoundRenderTargets; ++RTIndex)
    {
        const auto& RTAttachmentRef = Subpass.pRenderTargetAttachments[RTIndex];
        if (RTAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
        {
            TextureViewWebGPUImpl* pRTV = m_pBoundRenderTargets[RTIndex];
            VERIFY(pRTV == FBDesc.ppAttachments[RTAttachmentRef.AttachmentIndex], "Render target bound in the device context at slot ", RTIndex, " is not consistent with the corresponding framebuffer attachment");
            const auto  FirstLastUse     = m_pActiveRenderPass->GetAttachmentFirstLastUse(RTAttachmentRef.AttachmentIndex);
            const auto& RTAttachmentDesc = RPDesc.pAttachments[RTAttachmentRef.AttachmentIndex];

            RenderPassColorAttachments[RTIndex].view       = pRTV->GetWebGPUTextureView();
            RenderPassColorAttachments[RTIndex].loadOp     = FirstLastUse.first == m_SubpassIndex ? AttachmentLoadOpToWGPULoadOp(RTAttachmentDesc.LoadOp) : WGPULoadOp_Load;
            RenderPassColorAttachments[RTIndex].depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

            if (RTAttachmentDesc.LoadOp == ATTACHMENT_LOAD_OP_CLEAR)
            {
                const auto ClearColor = m_AttachmentClearValues[RTAttachmentRef.AttachmentIndex].Color;

                RenderPassColorAttachments[RTIndex].clearValue = WGPUColor{ClearColor[0], ClearColor[1], ClearColor[2], ClearColor[3]};
            }

            if (FirstLastUse.second == m_SubpassIndex)
            {
                if (Subpass.pResolveAttachments != nullptr && Subpass.pResolveAttachments[RTIndex].AttachmentIndex != ATTACHMENT_UNUSED)
                {
                    LOG_ERROR_MESSAGE("Not implemented");
                }
                else
                {
                    RenderPassColorAttachments[RTIndex].storeOp = AttachmentStoreOpToWGPUStoreOp(RTAttachmentDesc.StoreOp);
                }
            }
            else
            {
                RenderPassColorAttachments[RTIndex].storeOp = WGPUStoreOp_Store;
            }
        }
        else
        {
            RenderPassColorAttachments[RTIndex].loadOp  = WGPULoadOp_Clear;
            RenderPassColorAttachments[RTIndex].storeOp = WGPUStoreOp_Discard;
        }
    }

    WGPURenderPassDepthStencilAttachment RenderPassDepthStencilAttachment{};
    if (m_pBoundDepthStencil)
    {
        const auto& DSAttachmentRef = *Subpass.pDepthStencilAttachment;
        VERIFY_EXPR(Subpass.pDepthStencilAttachment != nullptr && DSAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED);
        VERIFY(m_pBoundDepthStencil == FBDesc.ppAttachments[DSAttachmentRef.AttachmentIndex], "Depth-stencil buffer in the device context is inconsistent with the framebuffer");
        const auto  FirstLastUse     = m_pActiveRenderPass->GetAttachmentFirstLastUse(DSAttachmentRef.AttachmentIndex);
        const auto& DSAttachmentDesc = RPDesc.pAttachments[DSAttachmentRef.AttachmentIndex];

        RenderPassDepthStencilAttachment.view = m_pBoundDepthStencil->GetWebGPUTextureView();
        if (FirstLastUse.first == m_SubpassIndex)
        {
            RenderPassDepthStencilAttachment.depthLoadOp   = AttachmentLoadOpToWGPULoadOp(DSAttachmentDesc.LoadOp);
            RenderPassDepthStencilAttachment.stencilLoadOp = AttachmentLoadOpToWGPULoadOp(DSAttachmentDesc.StencilLoadOp);
        }
        else
        {
            RenderPassDepthStencilAttachment.depthLoadOp   = WGPULoadOp_Load;
            RenderPassDepthStencilAttachment.stencilLoadOp = WGPULoadOp_Load;
        }

        if (RenderPassDepthStencilAttachment.depthLoadOp == WGPULoadOp_Clear)
            RenderPassDepthStencilAttachment.depthClearValue = m_AttachmentClearValues[DSAttachmentRef.AttachmentIndex].DepthStencil.Depth;

        if (RenderPassDepthStencilAttachment.stencilLoadOp == WGPULoadOp_Clear)
            RenderPassDepthStencilAttachment.stencilClearValue = m_AttachmentClearValues[DSAttachmentRef.AttachmentIndex].DepthStencil.Stencil;

        if (FirstLastUse.second == m_SubpassIndex)
        {
            RenderPassDepthStencilAttachment.depthStoreOp   = AttachmentStoreOpToWGPUStoreOp(DSAttachmentDesc.StoreOp);
            RenderPassDepthStencilAttachment.stencilStoreOp = AttachmentStoreOpToWGPUStoreOp(DSAttachmentDesc.StencilStoreOp);
        }
        else
        {
            RenderPassDepthStencilAttachment.depthStoreOp   = WGPUStoreOp_Store;
            RenderPassDepthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
        }
    }

    WGPURenderPassDescriptor wgpuRenderPassDesc{};
    wgpuRenderPassDesc.colorAttachments       = RenderPassColorAttachments;
    wgpuRenderPassDesc.colorAttachmentCount   = Subpass.RenderTargetAttachmentCount;
    wgpuRenderPassDesc.depthStencilAttachment = m_pBoundDepthStencil ? &RenderPassDepthStencilAttachment : nullptr;

    m_wgpuRenderPassEncoder = wgpuCommandEncoderBeginRenderPass(GetCommandEncoder(), &wgpuRenderPassDesc);
    DEV_CHECK_ERR(m_wgpuRenderPassEncoder != nullptr, "Failed to begin render pass");
    SetViewports(1, nullptr, 0, 0);
}

void DeviceContextWebGPUImpl::ClearEncoderState()
{
    m_EncoderState.Clear();
    m_BindInfo.Reset();
}

void DeviceContextWebGPUImpl::ClearAttachment(Int32                     RTIndex,
                                              COLOR_MASK                ColorMask,
                                              CLEAR_DEPTH_STENCIL_FLAGS DSFlags,
                                              const float               ClearData[],
                                              Uint8                     Stencil)
{
    // How to clear sRGB texture view?
    // How to clear integer texture view?
    VERIFY_EXPR(m_wgpuRenderPassEncoder);

    auto& AttachmentCleaner = m_pDevice->GetAttachmentCleaner();

    AttachmentCleanerWebGPU::RenderPassInfo RPInfo{};
    RPInfo.NumRenderTargets = m_NumBoundRenderTargets;
    RPInfo.SampleCount      = static_cast<Uint8>(m_FramebufferSamples);
    for (Uint32 RTIdx = 0; RTIdx < RPInfo.NumRenderTargets; ++RTIdx)
        RPInfo.RTVFormats[RTIdx] = m_pBoundRenderTargets[RTIdx] ? m_pBoundRenderTargets[RTIdx]->GetDesc().Format : TEX_FORMAT_UNKNOWN;
    RPInfo.DSVFormat = m_pBoundDepthStencil ? m_pBoundDepthStencil->GetDesc().Format : TEX_FORMAT_UNKNOWN;

    const Viewport VP{0, 0, static_cast<float>(m_FramebufferWidth), static_cast<float>(m_FramebufferHeight), 0, 1};
    if (VP != m_EncoderState.Viewports[0])
    {
        m_EncoderState.Viewports[0] = VP;
        wgpuRenderPassEncoderSetViewport(m_wgpuRenderPassEncoder, VP.TopLeftX, VP.TopLeftY, VP.Width, VP.Height, VP.MinDepth, VP.MaxDepth);
        m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_VIEWPORTS);
    }

    const Rect SR{0, 0, static_cast<Int32>(m_FramebufferWidth), static_cast<Int32>(m_FramebufferHeight)};
    if (SR != m_EncoderState.ScissorRects[0])
    {
        m_EncoderState.ScissorRects[0] = SR;
        wgpuRenderPassEncoderSetScissorRect(m_wgpuRenderPassEncoder, SR.left, SR.top, SR.right, SR.bottom);
        m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_SCISSOR_RECTS);
    }

    if (RTIndex >= 0)
        AttachmentCleaner.ClearColor(m_wgpuRenderPassEncoder, RPInfo, ColorMask, RTIndex, ClearData);
    else
    {
        AttachmentCleaner.ClearDepthStencil(m_wgpuRenderPassEncoder, RPInfo, DSFlags, ClearData[0], Stencil);
        if ((DSFlags & CLEAR_STENCIL_FLAG) != 0)
            m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_STENCIL_REF);
    }

    m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_PIPELINE_STATE);
}

template <PIPELINE_TYPE, typename CmdEncoderType>
void DeviceContextWebGPUImpl::CommitSRBs(CmdEncoderType CmdEncoder, Uint32 CommitSRBMask)
{
    // TODO
}

WGPURenderPassEncoder DeviceContextWebGPUImpl::PrepareForDraw(DRAW_FLAGS Flags)
{
#ifdef DILIGENT_DEVELOPMENT
    if ((Flags & DRAW_FLAG_VERIFY_RENDER_TARGETS) != 0)
        DvpVerifyRenderTargets();
#endif
    DEV_CHECK_ERR(m_pPipelineState != nullptr, "No PSO is bound in the context");

    auto wgpuRenderCmdEncoder = GetRenderPassCommandEncoder();

    // Handle pipeline state first because CommitGraphicsPSO may update another flags
    if (!m_EncoderState.IsUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_PIPELINE_STATE))
        CommitGraphicsPSO(wgpuRenderCmdEncoder);

    if (auto CommitSRBMask = m_BindInfo.GetCommitMask(Flags & DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT))
        CommitSRBs<PIPELINE_TYPE_GRAPHICS>(wgpuRenderCmdEncoder, CommitSRBMask);

    if (!m_EncoderState.IsUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_VERTEX_BUFFERS) || (m_EncoderState.HasDynamicVertexBuffers && (Flags & DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT) == 0))
        CommitVertexBuffers(wgpuRenderCmdEncoder);

    if (!m_EncoderState.IsUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_VIEWPORTS))
        CommitViewports(wgpuRenderCmdEncoder);

    if (!m_EncoderState.IsUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_SCISSOR_RECTS))
        CommitScissorRects(wgpuRenderCmdEncoder);

    if (!m_EncoderState.IsUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_BLEND_FACTORS))
    {
        WGPUColor wgpuBlendColor;
        wgpuBlendColor.r = m_BlendFactors[0];
        wgpuBlendColor.g = m_BlendFactors[1];
        wgpuBlendColor.b = m_BlendFactors[2];
        wgpuBlendColor.a = m_BlendFactors[3];

        wgpuRenderPassEncoderSetBlendConstant(wgpuRenderCmdEncoder, &wgpuBlendColor);
        m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_BLEND_FACTORS);
    }

    if (!m_EncoderState.IsUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_STENCIL_REF))
    {
        wgpuRenderPassEncoderSetStencilReference(wgpuRenderCmdEncoder, m_StencilRef);
        m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_STENCIL_REF);
    }

#ifdef DILIGENT_DEVELOPMENT
    DvpValidateCommittedShaderResources();
#endif

    if (m_BindInfo.DirtyBindGroups != 0)
        CommitBindGroups(wgpuRenderCmdEncoder);

    return wgpuRenderCmdEncoder;
}

WGPURenderPassEncoder DeviceContextWebGPUImpl::PrepareForIndexedDraw(DRAW_FLAGS Flags, VALUE_TYPE IndexType)
{
    DEV_CHECK_ERR(m_pPipelineState != nullptr, "No PSO is bound in the context");

    auto wgpuRenderCmdEncoder = PrepareForDraw(Flags);

    if (!m_EncoderState.IsUpToDate((WebGPUEncoderState::CMD_ENCODER_STATE_INDEX_BUFFER)))
        CommitIndexBuffer(wgpuRenderCmdEncoder, IndexType);

    return wgpuRenderCmdEncoder;
}

WGPUComputePassEncoder DeviceContextWebGPUImpl::PrepareForDispatchCompute()
{
    DEV_CHECK_ERR(m_pPipelineState != nullptr, "No PSO is bound in the context");

    auto wgpuComputeCmdEncoder = GetComputePassCommandEncoder();

    if (!m_EncoderState.IsUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_PIPELINE_STATE))
        CommitComputePSO(wgpuComputeCmdEncoder);

    if (auto CommitSRBMask = m_BindInfo.GetCommitMask())
        CommitSRBs<PIPELINE_TYPE_COMPUTE>(wgpuComputeCmdEncoder, CommitSRBMask);

#ifdef DILIGENT_DEVELOPMENT
    DvpValidateCommittedShaderResources();
#endif

    if (m_BindInfo.DirtyBindGroups != 0)
    {
        CommitBindGroups(wgpuComputeCmdEncoder);
    }

    return wgpuComputeCmdEncoder;
}

WGPUBuffer DeviceContextWebGPUImpl::PrepareForIndirectCommand(IBuffer* pAttribsBuffer, Uint64& IdirectBufferOffset)
{
    VERIFY_EXPR(pAttribsBuffer != nullptr);

    auto* pAttribsBufferWebGPU = ClassPtrCast<BufferWebGPUImpl>(pAttribsBuffer);

    auto wgpuIndirectBuffer = pAttribsBufferWebGPU->GetWebGPUBuffer();
    if (wgpuIndirectBuffer == nullptr)
    {
        VERIFY_EXPR(pAttribsBufferWebGPU->GetDesc().Usage == USAGE_DYNAMIC);
        const auto& DynamicAlloc = pAttribsBufferWebGPU->GetDynamicAllocation(GetContextId());

        wgpuIndirectBuffer = DynamicAlloc.wgpuBuffer;
        IdirectBufferOffset += DynamicAlloc.Offset;
    }

    return wgpuIndirectBuffer;
}

void DeviceContextWebGPUImpl::CommitGraphicsPSO(WGPURenderPassEncoder CmdEncoder)
{
    DEV_CHECK_ERR(m_pPipelineState, "No pipeline state to commit!");
    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_GRAPHICS, "Current PSO is not a graphics pipeline");

    auto wgpuPipeline = m_pPipelineState->GetWebGPURenderPipeline();
    wgpuRenderPassEncoderSetPipeline(CmdEncoder, wgpuPipeline);

    const auto& GraphicsPipeline = m_pPipelineState->GetGraphicsPipelineDesc();
    const auto& BlendDesc        = GraphicsPipeline.BlendDesc;
    const auto& DepthDesc        = GraphicsPipeline.DepthStencilDesc;

    m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_PIPELINE_STATE);

    if (m_pPipelineState->GetNumBufferSlotsUsed() != 0)
        m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_VERTEX_BUFFERS); // Vertex buffers need to be reset
    else
        m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_VERTEX_BUFFERS); // Vertex buffers are not used

    if (BlendDesc.IndependentBlendEnable || BlendDesc.RenderTargets[0].BlendEnable)
        m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_BLEND_FACTORS); // Blend is enabled - may need to update blend factors
    else
        m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_BLEND_FACTORS); // Blend is disabled - blend factors are not used

    if (DepthDesc.StencilEnable)
        m_EncoderState.Invalidate(WebGPUEncoderState::CMD_ENCODER_STATE_STENCIL_REF); // Stencil is enabled - may need to update stencil ref value
    else
        m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_STENCIL_REF); // Stencil is disabled - stencil ref is not used
}

void DeviceContextWebGPUImpl::CommitComputePSO(WGPUComputePassEncoder CmdEncoder)
{
    DEV_CHECK_ERR(m_pPipelineState, "No pipeline state to commit!");
    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_COMPUTE, "Current PSO is not a compute pipeline");

    auto wgpuPipeline = m_pPipelineState->GetWebGPUComputePipeline();
    wgpuComputePassEncoderSetPipeline(CmdEncoder, wgpuPipeline);

    m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_PIPELINE_STATE);
}

void DeviceContextWebGPUImpl::CommitVertexBuffers(WGPURenderPassEncoder CmdEncoder)
{
    DEV_CHECK_ERR(m_pPipelineState, "No pipeline state to commit!");

#ifdef DILIGENT_DEVELOPMENT
    if (m_NumVertexStreams < m_pPipelineState->GetNumBufferSlotsUsed())
        LOG_ERROR("Currently bound pipeline state '", m_pPipelineState->GetDesc().Name, "' expects ", m_pPipelineState->GetNumBufferSlotsUsed(), " input buffer slots, but only ", m_NumVertexStreams, " is bound");
#endif

    for (Uint32 SlotIdx = 0; SlotIdx < m_NumVertexStreams; ++SlotIdx)
    {
        auto& CurrStream = m_VertexStreams[SlotIdx];
        if (auto* pBufferWebGPU = CurrStream.pBuffer.RawPtr())
            wgpuRenderPassEncoderSetVertexBuffer(CmdEncoder, SlotIdx, pBufferWebGPU->GetWebGPUBuffer(), CurrStream.Offset, WGPU_WHOLE_SIZE);
        else
            wgpuRenderPassEncoderSetVertexBuffer(CmdEncoder, SlotIdx, nullptr, 0, 0);
    }

    m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_VERTEX_BUFFERS);
}

void DeviceContextWebGPUImpl::CommitIndexBuffer(WGPURenderPassEncoder CmdEncoder, VALUE_TYPE IndexType)
{
    DEV_CHECK_ERR(m_pPipelineState, "No pipeline state to commit!");
    DEV_CHECK_ERR(IndexType == VT_UINT16 || IndexType == VT_UINT32, "Unsupported index format. Only R16_UINT and R32_UINT are allowed.");

    wgpuRenderPassEncoderSetIndexBuffer(CmdEncoder, m_pIndexBuffer->GetWebGPUBuffer(), IndexTypeToWGPUIndexFormat(IndexType), m_IndexDataStartOffset, WGPU_WHOLE_SIZE);
    m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_INDEX_BUFFER);
}

void DeviceContextWebGPUImpl::CommitViewports(WGPURenderPassEncoder CmdEncoder)
{
    DEV_CHECK_ERR(m_pPipelineState, "No pipeline state to commit!");

    bool UpdateViewports = false;

    for (Uint32 ViewportIdx = 0; ViewportIdx < m_NumViewports; ++ViewportIdx)
    {
        const auto& RHS = m_Viewports[ViewportIdx];
        const auto& LHS = m_EncoderState.Viewports[ViewportIdx];

        if (LHS != RHS)
        {
            UpdateViewports                       = true;
            m_EncoderState.Viewports[ViewportIdx] = RHS;
        }
    }

    for (Uint32 ViewportIdx = m_NumViewports; ViewportIdx < m_EncoderState.Viewports.size(); ++ViewportIdx)
        m_EncoderState.Viewports[ViewportIdx] = Viewport{};

    if (UpdateViewports)
    {
        // WebGPU does not support multiple viewports
        wgpuRenderPassEncoderSetViewport(CmdEncoder,
                                         m_EncoderState.Viewports[0].TopLeftX, m_EncoderState.Viewports[0].TopLeftY,
                                         m_EncoderState.Viewports[0].Width, m_EncoderState.Viewports[0].Height,
                                         m_EncoderState.Viewports[0].MinDepth, m_EncoderState.Viewports[0].MaxDepth);
    }

    m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_VIEWPORTS);
}

void DeviceContextWebGPUImpl::CommitScissorRects(WGPURenderPassEncoder CmdEncoder)
{
    DEV_CHECK_ERR(m_pPipelineState, "No pipeline state to commit!");

    const auto ScissorEnabled = m_pPipelineState->GetGraphicsPipelineDesc().RasterizerDesc.ScissorEnable;

    bool UpdateScissorRects = false;

    auto UpdateWebGPUScissorRect = [&](const Rect& LHS, Rect& RHS) {
        const auto ScissorWidth  = std::max(std::min(LHS.right - LHS.left, static_cast<Int32>(m_FramebufferWidth) - LHS.left), 0);
        const auto ScissorHeight = std::max(std::min(LHS.bottom - LHS.top, static_cast<Int32>(m_FramebufferHeight) - LHS.top), 0);

        // clang-format off
        if (RHS.left   != LHS.left     ||
            RHS.top    != LHS.top      ||
            RHS.right  != ScissorWidth ||
            RHS.bottom != ScissorHeight)
        // clang-format on
        {
            UpdateScissorRects = true;

            RHS.left   = LHS.left;
            RHS.top    = LHS.top;
            RHS.right  = ScissorWidth;
            RHS.bottom = ScissorHeight;
        }
    };

    Uint32 NumScissors = 0;
    if (ScissorEnabled)
    {
        NumScissors = m_NumScissorRects;
        for (Uint32 i = 0; i < NumScissors; ++i)
            UpdateWebGPUScissorRect(m_ScissorRects[i], m_EncoderState.ScissorRects[i]);
    }
    else
    {
        NumScissors = m_NumViewports;
        Rect ScreenSizeRect(0, 0, m_FramebufferWidth, m_FramebufferHeight);
        for (Uint32 i = 0; i < NumScissors; ++i)
            UpdateWebGPUScissorRect(ScreenSizeRect, m_EncoderState.ScissorRects[i]);
    }

    for (auto i = NumScissors; i < m_EncoderState.ScissorRects.size(); ++i)
        m_EncoderState.ScissorRects[i] = Rect{};

    if (UpdateScissorRects)
    {
        // WebGPU does not support multiple scissor rects
        (void)NumScissors;
        wgpuRenderPassEncoderSetScissorRect(CmdEncoder,
                                            m_EncoderState.ScissorRects[0].left, m_EncoderState.ScissorRects[0].top,
                                            m_EncoderState.ScissorRects[0].right, m_EncoderState.ScissorRects[0].bottom);
    }

    m_EncoderState.SetUpToDate(WebGPUEncoderState::CMD_ENCODER_STATE_SCISSOR_RECTS);
}


SharedMemoryManagerWebGPU::Allocation DeviceContextWebGPUImpl::AllocateSharedMemory(Uint64 Size, Uint64 Alignment)
{
    SharedMemoryManagerWebGPU::Allocation Alloc;
    if (!m_SharedMemPages.empty())
        Alloc = m_SharedMemPages.back().Allocate(Size, Alignment);

    if (Alloc.IsEmpty())
    {
        m_SharedMemPages.emplace_back(m_pDevice->GetSharedMemoryPage(Size));
        Alloc = m_SharedMemPages.back().Allocate(Size, Alignment);
    }

    VERIFY_EXPR(!Alloc.IsEmpty());
    return Alloc;
}

QueryManagerWebGPU& DeviceContextWebGPUImpl::GetQueryManager()
{
    return *m_pQueryMgr;
}

} // namespace Diligent
