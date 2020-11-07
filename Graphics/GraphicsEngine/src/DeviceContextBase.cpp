/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include "DeviceContextBase.hpp"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

#if DILIGENT_DEBUG

#    define CHECK_PARAMETER(Expr, ...) \
        do                             \
        {                              \
            VERIFY(Expr, __VA_ARGS__); \
            if (!(Expr)) return false; \
        } while (false)

#else

#    define CHECK_PARAMETER(Expr, ...)          \
        do                                      \
        {                                       \
            if (!(Expr))                        \
            {                                   \
                LOG_ERROR_MESSAGE(__VA_ARGS__); \
                return false;                   \
            }                                   \
        } while (false)

#endif

bool VerifyDrawAttribs(const DrawAttribs& Attribs)
{
#define CHECK_DRAW_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Draw attribs are invalid: ", __VA_ARGS__)

    CHECK_DRAW_ATTRIBS(Attribs.NumVertices != 0, "NumVertices must not be zero.");

#undef CHECK_DRAW_ATTRIBS

    return true;
}

bool VerifyDrawIndexedAttribs(const DrawIndexedAttribs& Attribs)
{
#define CHECK_DRAW_INDEXED_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Draw indexed attribs are invalid: ", __VA_ARGS__)

    CHECK_DRAW_INDEXED_ATTRIBS(Attribs.IndexType == VT_UINT16 || Attribs.IndexType == VT_UINT32,
                               "IndexType (", GetValueTypeString(Attribs.IndexType), ") must be VT_UINT16 or VT_UINT32.");

    CHECK_DRAW_INDEXED_ATTRIBS(Attribs.NumIndices != 0, "NumIndices must not be zero.");

#undef CHECK_DRAW_INDEXED_ATTRIBS

    return true;
}

bool VerifyDrawMeshAttribs(Uint32 MaxDrawMeshTasksCount, const DrawMeshAttribs& Attribs)
{
#define CHECK_DRAW_MESH_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Draw mesh attribs are invalid: ", __VA_ARGS__)

    CHECK_DRAW_MESH_ATTRIBS(Attribs.ThreadGroupCount != 0, "ThreadGroupCount must not be zero.");
    CHECK_DRAW_MESH_ATTRIBS(Attribs.ThreadGroupCount <= MaxDrawMeshTasksCount,
                            "ThreadGroupCount (", Attribs.ThreadGroupCount, ") must not exceed ", MaxDrawMeshTasksCount);

#undef CHECK_DRAW_MESH_ATTRIBS

    return true;
}

bool VerifyDrawIndirectAttribs(const DrawIndirectAttribs& Attribs, const IBuffer* pAttribsBuffer)
{
#define CHECK_DRAW_INDIRECT_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Draw indirect attribs are invalid: ", __VA_ARGS__)

    CHECK_DRAW_INDIRECT_ATTRIBS(pAttribsBuffer != nullptr, "indirect draw arguments buffer must not be null.");
    CHECK_DRAW_INDIRECT_ATTRIBS((pAttribsBuffer->GetDesc().BindFlags & BIND_INDIRECT_DRAW_ARGS) != 0,
                                "indirect draw arguments buffer '", pAttribsBuffer->GetDesc().Name, "' was not created with BIND_INDIRECT_DRAW_ARGS flag.");

#undef CHECK_DRAW_INDIRECT_ATTRIBS

    return true;
}

bool VerifyDrawIndexedIndirectAttribs(const DrawIndexedIndirectAttribs& Attribs, const IBuffer* pAttribsBuffer)
{
#define CHECK_DRAW_INDEXED_INDIRECT_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Draw indexed indirect attribs are invalid: ", __VA_ARGS__)

    CHECK_DRAW_INDEXED_INDIRECT_ATTRIBS(pAttribsBuffer != nullptr, "indirect draw arguments buffer must not null.");
    CHECK_DRAW_INDEXED_INDIRECT_ATTRIBS(Attribs.IndexType == VT_UINT16 || Attribs.IndexType == VT_UINT32,
                                        "IndexType (", GetValueTypeString(Attribs.IndexType), ") must be VT_UINT16 or VT_UINT32.");
    CHECK_DRAW_INDEXED_INDIRECT_ATTRIBS((pAttribsBuffer->GetDesc().BindFlags & BIND_INDIRECT_DRAW_ARGS) != 0,
                                        "indirect draw arguments buffer '",
                                        pAttribsBuffer->GetDesc().Name, "' was not created with BIND_INDIRECT_DRAW_ARGS flag.");

#undef CHECK_DRAW_INDEXED_INDIRECT_ATTRIBS

    return true;
}

bool VerifyDrawMeshIndirectAttribs(const DrawMeshIndirectAttribs& Attribs, const IBuffer* pAttribsBuffer)
{
#define CHECK_DRAW_MESH_INDIRECT_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Draw mesh indirect attribs are invalid: ", __VA_ARGS__)

    CHECK_DRAW_MESH_INDIRECT_ATTRIBS(pAttribsBuffer != nullptr, "indirect draw arguments buffer must not be null.");
    CHECK_DRAW_MESH_INDIRECT_ATTRIBS((pAttribsBuffer->GetDesc().BindFlags & BIND_INDIRECT_DRAW_ARGS) != 0,
                                     "indirect draw arguments buffer '", pAttribsBuffer->GetDesc().Name,
                                     "' was not created with BIND_INDIRECT_DRAW_ARGS flag.");

#undef CHECK_DRAW_MESH_INDIRECT_ATTRIBS

    return true;
}


bool VerifyDispatchComputeAttribs(const DispatchComputeAttribs& Attribs)
{
#define CHECK_DISPATCH_COMPUTE_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Dispatch compute attribs are invalid: ", __VA_ARGS__)

    CHECK_DISPATCH_COMPUTE_ATTRIBS(Attribs.ThreadGroupCountX != 0, "ThreadGroupCountX must no be zero.");
    CHECK_DISPATCH_COMPUTE_ATTRIBS(Attribs.ThreadGroupCountY != 0, "ThreadGroupCountY must no be zero.");
    CHECK_DISPATCH_COMPUTE_ATTRIBS(Attribs.ThreadGroupCountZ != 0, "ThreadGroupCountZ must no be zero.");

#undef CHECK_DISPATCH_COMPUTE_ATTRIBS

    return true;
}

bool VerifyDispatchComputeIndirectAttribs(const DispatchComputeIndirectAttribs& Attribs, const IBuffer* pAttribsBuffer)
{
#define CHECK_DISPATCH_COMPUTE_INDIRECT_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Dispatch compute indirect attribs are invalid: ", __VA_ARGS__)

    CHECK_DISPATCH_COMPUTE_INDIRECT_ATTRIBS(pAttribsBuffer != nullptr, "indirect dispatch arguments buffer must not be null.");
    CHECK_DISPATCH_COMPUTE_INDIRECT_ATTRIBS((pAttribsBuffer->GetDesc().BindFlags & BIND_INDIRECT_DRAW_ARGS) != 0,
                                            "indirect dispatch arguments buffer '",
                                            pAttribsBuffer->GetDesc().Name, "' was not created with BIND_INDIRECT_DRAW_ARGS flag.");

#undef CHECK_DISPATCH_COMPUTE_INDIRECT_ATTRIBS

    return true;
}

bool VerifyResolveTextureSubresourceAttribs(const ResolveTextureSubresourceAttribs& ResolveAttribs,
                                            const TextureDesc&                      SrcTexDesc,
                                            const TextureDesc&                      DstTexDesc)
{
#define CHECK_RESOLVE_TEX_SUBRES_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Resolve texture subresource attribs are invalid: ", __VA_ARGS__)

    CHECK_RESOLVE_TEX_SUBRES_ATTRIBS(SrcTexDesc.SampleCount > 1, "source texture '", SrcTexDesc.Name, "' of a resolve operation is not multi-sampled");
    CHECK_RESOLVE_TEX_SUBRES_ATTRIBS(DstTexDesc.SampleCount == 1, "destination texture '", DstTexDesc.Name, "' of a resolve operation is multi-sampled");

    auto SrcMipLevelProps = GetMipLevelProperties(SrcTexDesc, ResolveAttribs.SrcMipLevel);
    auto DstMipLevelProps = GetMipLevelProperties(DstTexDesc, ResolveAttribs.DstMipLevel);
    CHECK_RESOLVE_TEX_SUBRES_ATTRIBS(SrcMipLevelProps.LogicalWidth == DstMipLevelProps.LogicalWidth && SrcMipLevelProps.LogicalHeight == DstMipLevelProps.LogicalHeight,
                                     "the size (", SrcMipLevelProps.LogicalWidth, "x", SrcMipLevelProps.LogicalHeight,
                                     ") of the source subresource of a resolve operation (texture '",
                                     SrcTexDesc.Name, "', mip ", ResolveAttribs.SrcMipLevel, ", slice ", ResolveAttribs.SrcSlice,
                                     ") does not match the size (", DstMipLevelProps.LogicalWidth, "x", DstMipLevelProps.LogicalHeight,
                                     ") of the destination subresource (texture '", DstTexDesc.Name, "', mip ", ResolveAttribs.DstMipLevel, ", slice ",
                                     ResolveAttribs.DstSlice, ")");

    const auto& SrcFmtAttribs     = GetTextureFormatAttribs(SrcTexDesc.Format);
    const auto& DstFmtAttribs     = GetTextureFormatAttribs(DstTexDesc.Format);
    const auto& ResolveFmtAttribs = GetTextureFormatAttribs(ResolveAttribs.Format);
    if (!SrcFmtAttribs.IsTypeless && !DstFmtAttribs.IsTypeless)
    {
        CHECK_RESOLVE_TEX_SUBRES_ATTRIBS(SrcTexDesc.Format == DstTexDesc.Format,
                                         "source (", SrcFmtAttribs.Name, ") and destination (", DstFmtAttribs.Name,
                                         ") texture formats of a resolve operation must match exaclty or be compatible typeless formats");
        CHECK_RESOLVE_TEX_SUBRES_ATTRIBS(ResolveAttribs.Format == TEX_FORMAT_UNKNOWN || SrcTexDesc.Format == ResolveAttribs.Format, "Invalid format of a resolve operation");
    }
    if (SrcFmtAttribs.IsTypeless && DstFmtAttribs.IsTypeless)
    {
        CHECK_RESOLVE_TEX_SUBRES_ATTRIBS(ResolveAttribs.Format != TEX_FORMAT_UNKNOWN,
                                         "format of a resolve operation must not be unknown when both src and dst texture formats are typeless");
    }
    if (SrcFmtAttribs.IsTypeless || DstFmtAttribs.IsTypeless)
    {
        CHECK_RESOLVE_TEX_SUBRES_ATTRIBS(!ResolveFmtAttribs.IsTypeless,
                                         "format of a resolve operation must not be typeless when one of the texture formats is typeless");
    }
#undef CHECK_RESOLVE_TEX_SUBRES_ATTRIBS

    return true;
}

bool VerifyBeginRenderPassAttribs(const BeginRenderPassAttribs& Attribs)
{
#define CHECK_BEGIN_RENDER_PASS_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Begin render pass attribs are invalid: ", __VA_ARGS__)

    CHECK_BEGIN_RENDER_PASS_ATTRIBS(Attribs.pRenderPass != nullptr, "pRenderPass pass must not be null");
    CHECK_BEGIN_RENDER_PASS_ATTRIBS(Attribs.pFramebuffer != nullptr, "pFramebuffer must not be null");

    const auto& RPDesc = Attribs.pRenderPass->GetDesc();

    Uint32 NumRequiredClearValues = 0;
    for (Uint32 i = 0; i < RPDesc.AttachmentCount; ++i)
    {
        const auto& Attchmnt = RPDesc.pAttachments[i];
        if (Attchmnt.LoadOp == ATTACHMENT_LOAD_OP_CLEAR)
            NumRequiredClearValues = i + 1;

        const auto& FmtAttribs = GetTextureFormatAttribs(Attchmnt.Format);
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
        {
            if (Attchmnt.StencilLoadOp == ATTACHMENT_LOAD_OP_CLEAR)
                NumRequiredClearValues = i + 1;
        }
    }

    CHECK_BEGIN_RENDER_PASS_ATTRIBS(Attribs.ClearValueCount >= NumRequiredClearValues,
                                    "at least ", NumRequiredClearValues, " clear values are required, but only ",
                                    Uint32{Attribs.ClearValueCount}, " are provided.");
    CHECK_BEGIN_RENDER_PASS_ATTRIBS(Attribs.ClearValueCount == 0 || Attribs.pClearValues != nullptr,
                                    "pClearValues must not be null when ClearValueCount (", Attribs.ClearValueCount, ") is not zero");

#undef CHECK_BEGIN_RENDER_PASS_ATTRIBS

    return true;
}

bool VerifyStateTransitionDesc(const IRenderDevice* pDevice, const StateTransitionDesc& Barrier)
{
#define CHECK_STATE_TRANSITION_DESC(Expr, ...) CHECK_PARAMETER(Expr, "State transition parameters are invalid: ", __VA_ARGS__)

    CHECK_STATE_TRANSITION_DESC(Barrier.pResource != nullptr, "pResource must not be null");
    CHECK_STATE_TRANSITION_DESC(Barrier.NewState != RESOURCE_STATE_UNKNOWN, "NewState state can't be UNKNOWN");

    RESOURCE_STATE OldState = RESOURCE_STATE_UNKNOWN;

    if (RefCntAutoPtr<ITexture> pTexture{Barrier.pResource, IID_Texture})
    {
        const auto& TexDesc = pTexture->GetDesc();

        CHECK_STATE_TRANSITION_DESC(VerifyResourceStates(Barrier.NewState, true), "invlaid new state specified for texture '", TexDesc.Name, "'");
        OldState = Barrier.OldState != RESOURCE_STATE_UNKNOWN ? Barrier.OldState : pTexture->GetState();
        CHECK_STATE_TRANSITION_DESC(OldState != RESOURCE_STATE_UNKNOWN,
                                    "the state of texture '", TexDesc.Name,
                                    "' is unknown to the engine and is not explicitly specified in the barrier");
        CHECK_STATE_TRANSITION_DESC(VerifyResourceStates(OldState, true), "invlaid old state specified for texture '", TexDesc.Name, "'");

        CHECK_STATE_TRANSITION_DESC(Barrier.FirstMipLevel < TexDesc.MipLevels, "first mip level (", Barrier.FirstMipLevel,
                                    ") specified by the barrier is out of range. Texture '",
                                    TexDesc.Name, "' has only ", TexDesc.MipLevels, " mip level(s)");
        CHECK_STATE_TRANSITION_DESC(Barrier.MipLevelsCount == REMAINING_MIP_LEVELS || Barrier.FirstMipLevel + Barrier.MipLevelsCount <= TexDesc.MipLevels,
                                    "mip level range ", Barrier.FirstMipLevel, "..", Barrier.FirstMipLevel + Barrier.MipLevelsCount - 1,
                                    " specified by the barrier is out of range. Texture '",
                                    TexDesc.Name, "' has only ", TexDesc.MipLevels, " mip level(s)");

        CHECK_STATE_TRANSITION_DESC(Barrier.FirstArraySlice < TexDesc.ArraySize, "first array slice (", Barrier.FirstArraySlice,
                                    ") specified by the barrier is out of range. Array size of texture '",
                                    TexDesc.Name, "' is ", TexDesc.ArraySize);
        CHECK_STATE_TRANSITION_DESC(Barrier.ArraySliceCount == REMAINING_ARRAY_SLICES || Barrier.FirstArraySlice + Barrier.ArraySliceCount <= TexDesc.ArraySize,
                                    "array slice range ", Barrier.FirstArraySlice, "..", Barrier.FirstArraySlice + Barrier.ArraySliceCount - 1,
                                    " specified by the barrier is out of range. Array size of texture '",
                                    TexDesc.Name, "' is ", TexDesc.ArraySize);

        auto DevType = pDevice->GetDeviceCaps().DevType;
        if (DevType != RENDER_DEVICE_TYPE_D3D12 && DevType != RENDER_DEVICE_TYPE_VULKAN)
        {
            CHECK_STATE_TRANSITION_DESC(Barrier.FirstMipLevel == 0 && (Barrier.MipLevelsCount == REMAINING_MIP_LEVELS || Barrier.MipLevelsCount == TexDesc.MipLevels),
                                        "failed to transition texture '", TexDesc.Name, "': only whole resources can be transitioned on this device");
            CHECK_STATE_TRANSITION_DESC(Barrier.FirstArraySlice == 0 && (Barrier.ArraySliceCount == REMAINING_ARRAY_SLICES || Barrier.ArraySliceCount == TexDesc.ArraySize),
                                        "failed to transition texture '", TexDesc.Name, "': only whole resources can be transitioned on this device");
        }
    }
    else if (RefCntAutoPtr<IBuffer> pBuffer{Barrier.pResource, IID_Buffer})
    {
        const auto& BuffDesc = pBuffer->GetDesc();
        CHECK_STATE_TRANSITION_DESC(VerifyResourceStates(Barrier.NewState, false), "invlaid new state specified for buffer '", BuffDesc.Name, "'");
        OldState = Barrier.OldState != RESOURCE_STATE_UNKNOWN ? Barrier.OldState : pBuffer->GetState();
        CHECK_STATE_TRANSITION_DESC(OldState != RESOURCE_STATE_UNKNOWN, "the state of buffer '", BuffDesc.Name, "' is unknown to the engine and is not explicitly specified in the barrier");
        CHECK_STATE_TRANSITION_DESC(VerifyResourceStates(OldState, false), "invlaid old state specified for buffer '", BuffDesc.Name, "'");
    }
    else if (RefCntAutoPtr<IBottomLevelAS> pBottomLevelAS{Barrier.pResource, IID_BottomLevelAS})
    {
        const auto& BLASDesc = pBottomLevelAS->GetDesc();
        OldState             = Barrier.OldState != RESOURCE_STATE_UNKNOWN ? Barrier.OldState : pBottomLevelAS->GetState();
        CHECK_STATE_TRANSITION_DESC(OldState != RESOURCE_STATE_UNKNOWN, "the state of BLAS '", BLASDesc.Name, "' is unknown to the engine and is not explicitly specified in the barrier");
        CHECK_STATE_TRANSITION_DESC(Barrier.NewState == RESOURCE_STATE_BUILD_AS_READ || Barrier.NewState == RESOURCE_STATE_BUILD_AS_WRITE,
                                    "invlaid new state specified for BLAS '", BLASDesc.Name, "'");
        CHECK_STATE_TRANSITION_DESC(Barrier.TransitionType != STATE_TRANSITION_TYPE_IMMEDIATE, "split barriers are not supported for BLAS");
    }
    else if (RefCntAutoPtr<ITopLevelAS> pTopLevelAS{Barrier.pResource, IID_TopLevelAS})
    {
        const auto& TLASDesc = pTopLevelAS->GetDesc();
        OldState             = Barrier.OldState != RESOURCE_STATE_UNKNOWN ? Barrier.OldState : pTopLevelAS->GetState();
        CHECK_STATE_TRANSITION_DESC(OldState != RESOURCE_STATE_UNKNOWN, "the state of TLAS '", TLASDesc.Name, "' is unknown to the engine and is not explicitly specified in the barrier");
        CHECK_STATE_TRANSITION_DESC(Barrier.NewState == RESOURCE_STATE_BUILD_AS_READ || Barrier.NewState == RESOURCE_STATE_BUILD_AS_WRITE || Barrier.NewState == RESOURCE_STATE_RAY_TRACING,
                                    "invlaid new state specified for TLAS '", TLASDesc.Name, "'");
        CHECK_STATE_TRANSITION_DESC(Barrier.TransitionType != STATE_TRANSITION_TYPE_IMMEDIATE, "split barriers are not supported for TLAS");
    }
    else
    {
        UNEXPECTED("unsupported resource type");
    }

    if (OldState == RESOURCE_STATE_UNORDERED_ACCESS && Barrier.NewState == RESOURCE_STATE_UNORDERED_ACCESS)
    {
        CHECK_STATE_TRANSITION_DESC(Barrier.TransitionType == STATE_TRANSITION_TYPE_IMMEDIATE, "for UAV barriers, transition type must be STATE_TRANSITION_TYPE_IMMEDIATE");
    }

    if (Barrier.TransitionType == STATE_TRANSITION_TYPE_BEGIN)
    {
        CHECK_STATE_TRANSITION_DESC(!Barrier.UpdateResourceState, "resource state can't be updated in begin-split barrier");
    }

#undef CHECK_STATE_TRANSITION_DESC

    return true;
}


bool VerifyBuildBLASAttribs(const BuildBLASAttribs& Attribs)
{
#define CHECK_BUILD_BLAS_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Build BLAS attribs are invalid: ", __VA_ARGS__)

    CHECK_BUILD_BLAS_ATTRIBS(Attribs.pBLAS != nullptr, "pBLAS must not be null");
    CHECK_BUILD_BLAS_ATTRIBS(Attribs.pScratchBuffer != nullptr, "pScratchBuffer must not be null");
    CHECK_BUILD_BLAS_ATTRIBS((Attribs.pTriangleData != nullptr) ^ (Attribs.pBoxData != nullptr), "exactly one of pTriangles and pBoxes must be defined");
    CHECK_BUILD_BLAS_ATTRIBS(Attribs.pBoxData != nullptr || Attribs.BoxDataCount == 0, "pBoxData is null, but BoxDataCount is not 0");
    CHECK_BUILD_BLAS_ATTRIBS(Attribs.pTriangleData != nullptr || Attribs.TriangleDataCount == 0, "pTriangleData is null, but TriangleDataCount is not 0");

    const auto& BLASDesc = Attribs.pBLAS->GetDesc();

    CHECK_BUILD_BLAS_ATTRIBS(Attribs.BoxDataCount <= BLASDesc.BoxCount, "BoxDataCount must be less than or equal to pBLAS->GetDesc().BoxCount");
    CHECK_BUILD_BLAS_ATTRIBS(Attribs.TriangleDataCount <= BLASDesc.TriangleCount, "TriangleDataCount must be less than or equal to pBLAS->GetDesc().TriangleCount");

    if (Attribs.Update)
    {
        CHECK_BUILD_BLAS_ATTRIBS((BLASDesc.Flags & RAYTRACING_BUILD_AS_ALLOW_UPDATE) == RAYTRACING_BUILD_AS_ALLOW_UPDATE,
                                 "Update is true, but BLAS created without RAYTRACING_BUILD_AS_ALLOW_UPDATE flag");

        const Uint32 GeomCount = Attribs.pBLAS->GetActualGeometryCount();
        CHECK_BUILD_BLAS_ATTRIBS(Attribs.BoxDataCount == 0 || Attribs.BoxDataCount == GeomCount,
                                 "Update is true, but BoxDataCount does not match with a previous value (", GeomCount, ")");
        CHECK_BUILD_BLAS_ATTRIBS(Attribs.TriangleDataCount == 0 || Attribs.TriangleDataCount == GeomCount,
                                 "Update is true, but TriangleDataCount does not match with a previous value (", GeomCount, ")");
    }

    for (Uint32 i = 0; i < Attribs.TriangleDataCount; ++i)
    {
        const auto&  tri            = Attribs.pTriangleData[i];
        const Uint32 VertexSize     = GetValueSize(tri.VertexValueType) * tri.VertexComponentCount;
        const Uint32 VertexDataSize = tri.VertexStride * tri.VertexCount;
        const Uint32 GeomIndex      = Attribs.pBLAS->GetGeometryDescIndex(tri.GeometryName);

        CHECK_BUILD_BLAS_ATTRIBS(GeomIndex != INVALID_INDEX,
                                 "pTriangleData[", i, "].GeometryName (", tri.GeometryName, ") is not found in BLAS description");

        const auto& TriDesc = BLASDesc.pTriangles[GeomIndex];

        CHECK_BUILD_BLAS_ATTRIBS(tri.VertexValueType == VT_UNDEFINED || tri.VertexValueType == TriDesc.VertexValueType,
                                 "pTriangleData[", i, "].VertexValueType must be undefined or match the VertexValueType in geometry description");

        CHECK_BUILD_BLAS_ATTRIBS(tri.VertexComponentCount == 0 || tri.VertexComponentCount == TriDesc.VertexComponentCount,
                                 "pTriangleData[", i, "].VertexComponentCount (", tri.VertexComponentCount, ") must be 0 or match the VertexComponentCount (",
                                 TriDesc.VertexComponentCount, ") in geometry description");

        CHECK_BUILD_BLAS_ATTRIBS(tri.VertexCount <= TriDesc.MaxVertexCount,
                                 "pTriangleData[", i, "].VertexCount (", tri.VertexCount, ") must not be greater than MaxVertexCount(", TriDesc.MaxVertexCount, ")");

        CHECK_BUILD_BLAS_ATTRIBS(tri.VertexStride >= VertexSize,
                                 "pTriangleData[", i, "].VertexStride (", tri.VertexStride, ") must be at least ", VertexSize, " bytes");

        CHECK_BUILD_BLAS_ATTRIBS(tri.pVertexBuffer != nullptr, "pTriangleData[", i, "].pVertexBuffer must not be null");

        const BufferDesc& VertBufDesc = tri.pVertexBuffer->GetDesc();
        CHECK_BUILD_BLAS_ATTRIBS((VertBufDesc.BindFlags & BIND_RAY_TRACING) == BIND_RAY_TRACING,
                                 "pTriangleData[", i, "].pVertexBuffer was not created with BIND_RAY_TRACING flag");

        CHECK_BUILD_BLAS_ATTRIBS(tri.VertexOffset + VertexDataSize <= VertBufDesc.uiSizeInBytes,
                                 "pTriangleData[", i, "].pVertexBuffer is too small for the specified VertexStride (", tri.VertexStride, ") and VertexCount (",
                                 tri.VertexCount, "): at least ", tri.VertexOffset + VertexDataSize, " bytes are required");

        CHECK_BUILD_BLAS_ATTRIBS(tri.IndexType == VT_UNDEFINED || tri.IndexType == TriDesc.IndexType,
                                 "pTriangleData[", i, "].IndexType (", GetValueTypeString(tri.IndexType), ") must match the IndexType (",
                                 TriDesc.IndexType, ") in geometry description");

        CHECK_BUILD_BLAS_ATTRIBS(tri.PrimitiveCount <= TriDesc.MaxPrimitiveCount,
                                 "pTriangleData[", i, "].PrimitiveCount (", tri.PrimitiveCount, ") must not be greater than MaxPrimitiveCount (",
                                 TriDesc.MaxPrimitiveCount, ")");

        if (TriDesc.IndexType != VT_UNDEFINED)
        {
            CHECK_BUILD_BLAS_ATTRIBS(tri.pIndexBuffer != nullptr, "pTriangleData[", i, "].pIndexBuffer must not be null");

            const BufferDesc& InstBufDesc   = tri.pIndexBuffer->GetDesc();
            const Uint32      IndexDataSize = tri.PrimitiveCount * 3 * GetValueSize(tri.IndexType);

            CHECK_BUILD_BLAS_ATTRIBS((InstBufDesc.BindFlags & BIND_RAY_TRACING) == BIND_RAY_TRACING,
                                     "pTriangleData[", i, "].pIndexBuffer was not created with BIND_RAY_TRACING flag");

            CHECK_BUILD_BLAS_ATTRIBS(tri.IndexOffset + IndexDataSize <= InstBufDesc.uiSizeInBytes,
                                     "pTriangleData[", i, "].pIndexBuffer is too small for specified IndexType and IndexCount: at least",
                                     tri.IndexOffset + IndexDataSize, " bytes are required");
        }
        else
        {
            CHECK_BUILD_BLAS_ATTRIBS(tri.VertexCount == tri.PrimitiveCount * 3,
                                     "pTriangleData[", i, "].VertexCount (", tri.VertexCount, ") must equal to PrimitiveCount * 3 (",
                                     tri.PrimitiveCount * 3, ")");

            CHECK_BUILD_BLAS_ATTRIBS(tri.pIndexBuffer == nullptr, "pTriangleData[", i, "].pIndexBuffer must be null if IndexType is VT_UNDEFINED");
        }

        if (tri.pTransformBuffer != nullptr)
        {
            CHECK_BUILD_BLAS_ATTRIBS((tri.pTransformBuffer->GetDesc().BindFlags & BIND_RAY_TRACING) == BIND_RAY_TRACING,
                                     "pTriangleData[", i, "].pTransformBuffer was not created with BIND_RAY_TRACING flag");

            CHECK_BUILD_BLAS_ATTRIBS(TriDesc.AllowsTransforms, "pTriangleData[", i, "] uses transform buffer, but AllowsTransforms is false");
        }
    }

    for (Uint32 i = 0; i < Attribs.BoxDataCount; ++i)
    {
        const auto&  box       = Attribs.pBoxData[i];
        const Uint32 BoxSize   = sizeof(float) * 6;
        const Uint32 GeomIndex = Attribs.pBLAS->GetGeometryDescIndex(box.GeometryName);

        CHECK_BUILD_BLAS_ATTRIBS(GeomIndex != INVALID_INDEX,
                                 "pBoxData[", i, "].GeometryName (", box.GeometryName, ") is not found in BLAS description");

        const auto& BoxDesc = BLASDesc.pBoxes[GeomIndex];

        CHECK_BUILD_BLAS_ATTRIBS(box.BoxCount <= BoxDesc.MaxBoxCount,
                                 "pBoxData[", i, "].BoxCount (", box.BoxCount, ") must not be greated than MaxBoxCount (", BoxDesc.MaxBoxCount, ")");

        CHECK_BUILD_BLAS_ATTRIBS(box.BoxStride >= BoxSize,
                                 "pBoxData[", i, "].BoxStride (", box.BoxStride, ") must be at least ", BoxSize, " bytes");

        CHECK_BUILD_BLAS_ATTRIBS(box.pBoxBuffer != nullptr, "pBoxData[", i, "].pBoxBuffer must not be null");

        CHECK_BUILD_BLAS_ATTRIBS((box.pBoxBuffer->GetDesc().BindFlags & BIND_RAY_TRACING) == BIND_RAY_TRACING,
                                 "pBoxData[", i, "].pBoxBuffer was not created with BIND_RAY_TRACING flag");
    }

    const auto& ScratchDesc = Attribs.pScratchBuffer->GetDesc();

    CHECK_BUILD_BLAS_ATTRIBS(Attribs.ScratchBufferOffset <= ScratchDesc.uiSizeInBytes,
                             "ScratchBufferOffset (", Attribs.ScratchBufferOffset, ") is greater than the buffer size (", ScratchDesc.uiSizeInBytes, ")");

    if (Attribs.Update)
        CHECK_BUILD_BLAS_ATTRIBS(ScratchDesc.uiSizeInBytes - Attribs.ScratchBufferOffset >= Attribs.pBLAS->GetScratchBufferSizes().Update,
                                 "pScratchBuffer size is too small, use pBLAS->GetScratchBufferSizes().Update to get the required size for the scratch buffer");
    else
        CHECK_BUILD_BLAS_ATTRIBS(ScratchDesc.uiSizeInBytes - Attribs.ScratchBufferOffset >= Attribs.pBLAS->GetScratchBufferSizes().Build,
                                 "pScratchBuffer size is too small, use pBLAS->GetScratchBufferSizes().Build to get the required size for the scratch buffer");

    CHECK_BUILD_BLAS_ATTRIBS((ScratchDesc.BindFlags & BIND_RAY_TRACING) == BIND_RAY_TRACING,
                             "pScratchBuffer was not created with BIND_RAY_TRACING flag");

#undef CHECK_BUILD_BLAS_ATTRIBS

    return true;
}


bool VerifyBuildTLASAttribs(const BuildTLASAttribs& Attribs, Uint32 PrevInstanceCount)
{
#define CHECK_BUILD_TLAS_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Build TLAS attribs are invalid: ", __VA_ARGS__)

    CHECK_BUILD_TLAS_ATTRIBS(Attribs.pTLAS != nullptr, "pTLAS must not be null");
    CHECK_BUILD_TLAS_ATTRIBS(Attribs.pScratchBuffer != nullptr, "pScratchBuffer must not be null");
    CHECK_BUILD_TLAS_ATTRIBS(Attribs.pInstances != nullptr, "pInstances must not be null");
    CHECK_BUILD_TLAS_ATTRIBS(Attribs.pInstanceBuffer != nullptr, "pInstanceBuffer must not be null");

    CHECK_BUILD_TLAS_ATTRIBS(Attribs.BindingMode == SHADER_BINDING_USER_DEFINED || Attribs.HitShadersPerInstance != 0,
                             "HitShadersPerInstance must be greater than 0 if BindingMode is not SHADER_BINDING_USER_DEFINED");

    const auto& TLASDesc = Attribs.pTLAS->GetDesc();

    CHECK_BUILD_TLAS_ATTRIBS(Attribs.InstanceCount <= TLASDesc.MaxInstanceCount,
                             "InstanceCount (", Attribs.InstanceCount, ") must be less than or equal to pTLAS->GetDesc().MaxInstanceCount (",
                             TLASDesc.MaxInstanceCount, ")");

    if (Attribs.Update)
    {
        CHECK_BUILD_TLAS_ATTRIBS((TLASDesc.Flags & RAYTRACING_BUILD_AS_ALLOW_UPDATE) == RAYTRACING_BUILD_AS_ALLOW_UPDATE,
                                 "Update is true, but TLAS created without RAYTRACING_BUILD_AS_ALLOW_UPDATE flag");
        CHECK_BUILD_TLAS_ATTRIBS(PrevInstanceCount == Attribs.InstanceCount,
                                 "Update is true, but InstanceCount (", Attribs.InstanceCount, ") does not match with the previous value (", PrevInstanceCount, ")");
    }

    const auto& InstDesc          = Attribs.pInstanceBuffer->GetDesc();
    const auto  InstDataSize      = size_t{Attribs.InstanceCount} * size_t{TLAS_INSTANCE_DATA_SIZE};
    Uint32      AutoOffsetCounter = 0;

    // Calculate instance data size
    for (Uint32 i = 0; i < Attribs.InstanceCount; ++i)
    {
        constexpr Uint32 BitMask = (1u << 24) - 1;
        const auto&      Inst    = Attribs.pInstances[i];

        VERIFY((Inst.CustomId & ~BitMask) == 0, "Only the lower 24 bits are used");

        VERIFY(Inst.ContributionToHitGroupIndex == TLAS_INSTANCE_OFFSET_AUTO ||
                   (Inst.ContributionToHitGroupIndex & ~BitMask) == 0,
               "Only the lower 24 bits are used");

        CHECK_BUILD_TLAS_ATTRIBS(Inst.InstanceName != nullptr, "pInstances[", i, "].InstanceName must not be null");

        if (Attribs.Update)
        {
            const TLASInstanceDesc IDesc = Attribs.pTLAS->GetInstanceDesc(Inst.InstanceName);
            CHECK_BUILD_TLAS_ATTRIBS(IDesc.InstanceIndex != INVALID_INDEX, "Update is true, but pInstances[", i, "].InstanceName does not exists");
        }
        else
        {
            CHECK_BUILD_TLAS_ATTRIBS(Inst.pBLAS != nullptr, "pInstances[", i, "].pBLAS must not be null");
        }

        if (Inst.ContributionToHitGroupIndex == TLAS_INSTANCE_OFFSET_AUTO)
            ++AutoOffsetCounter;

        CHECK_BUILD_TLAS_ATTRIBS(Attribs.BindingMode == SHADER_BINDING_USER_DEFINED || Inst.ContributionToHitGroupIndex == TLAS_INSTANCE_OFFSET_AUTO,
                                 "pInstances[", i,
                                 "].ContributionToHitGroupIndex must be TLAS_INSTANCE_OFFSET_AUTO "
                                 "if BindingMode is not SHADER_BINDING_USER_DEFINED");
    }

    CHECK_BUILD_TLAS_ATTRIBS(AutoOffsetCounter == 0 || AutoOffsetCounter == Attribs.InstanceCount,
                             "all pInstances[i].ContributionToHitGroupIndex must be TLAS_INSTANCE_OFFSET_AUTO, or none of them should");

    CHECK_BUILD_TLAS_ATTRIBS(Attribs.InstanceBufferOffset <= InstDesc.uiSizeInBytes,
                             "InstanceBufferOffset (", Attribs.InstanceBufferOffset, ") is greater than the buffer size (", InstDesc.uiSizeInBytes, ")");

    CHECK_BUILD_TLAS_ATTRIBS(InstDesc.uiSizeInBytes - Attribs.InstanceBufferOffset >= InstDataSize,
                             "pInstanceBuffer size (", InstDesc.uiSizeInBytes, ") is too small: at least ",
                             InstDataSize + Attribs.InstanceBufferOffset, " bytes are required");

    CHECK_BUILD_TLAS_ATTRIBS((InstDesc.BindFlags & BIND_RAY_TRACING) == BIND_RAY_TRACING,
                             "pInstanceBuffer was not created with BIND_RAY_TRACING flag");

    const auto& ScratchDesc = Attribs.pScratchBuffer->GetDesc();

    CHECK_BUILD_TLAS_ATTRIBS(Attribs.ScratchBufferOffset <= ScratchDesc.uiSizeInBytes,
                             "ScratchBufferOffset (", Attribs.ScratchBufferOffset, ") is greater than the buffer size (", ScratchDesc.uiSizeInBytes, ")");

    if (Attribs.Update)
        CHECK_BUILD_TLAS_ATTRIBS(ScratchDesc.uiSizeInBytes - Attribs.ScratchBufferOffset >= Attribs.pTLAS->GetScratchBufferSizes().Update,
                                 "pScratchBuffer size is too small, use pTLAS->GetScratchBufferSizes().Update to get the required size for scratch buffer");
    else
        CHECK_BUILD_TLAS_ATTRIBS(ScratchDesc.uiSizeInBytes - Attribs.ScratchBufferOffset >= Attribs.pTLAS->GetScratchBufferSizes().Build,
                                 "pScratchBuffer size is too small, use pTLAS->GetScratchBufferSizes().Build to get the required size for scratch buffer");

    CHECK_BUILD_TLAS_ATTRIBS((ScratchDesc.BindFlags & BIND_RAY_TRACING) == BIND_RAY_TRACING,
                             "pScratchBuffer was not created with BIND_RAY_TRACING flag");
#undef CHECK_BUILD_TLAS_ATTRIBS

    return true;
}


bool VerifyCopyBLASAttribs(const IRenderDevice* pDevice, const CopyBLASAttribs& Attribs)
{
#define CHECK_COPY_BLAS_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Copy BLAS attribs are invalid: ", __VA_ARGS__)

    CHECK_COPY_BLAS_ATTRIBS(Attribs.pSrc != nullptr, "pSrc must not be null");
    CHECK_COPY_BLAS_ATTRIBS(Attribs.pDst != nullptr, "pDst must not be null");

    if (Attribs.Mode == COPY_AS_MODE_CLONE)
    {
        if (pDevice->GetDeviceCaps().DevType == RENDER_DEVICE_TYPE_VULKAN)
        {
            auto& SrcDesc = Attribs.pSrc->GetDesc();
            auto& DstDesc = Attribs.pDst->GetDesc();

            CHECK_COPY_BLAS_ATTRIBS(SrcDesc.TriangleCount == DstDesc.TriangleCount,
                                    "Src BLAS triangle count (", SrcDesc.TriangleCount, ") must be equal to the dst BLAS triangle count (", DstDesc.TriangleCount, ")");

            CHECK_COPY_BLAS_ATTRIBS(SrcDesc.BoxCount == DstDesc.BoxCount,
                                    "Src BLAS box count (", SrcDesc.BoxCount, ") must be equal to the dst BLAS box count (", DstDesc.BoxCount, ")");

            CHECK_COPY_BLAS_ATTRIBS(SrcDesc.Flags == DstDesc.Flags,
                                    "Source and destination BLASes must have been created with the same flags");

            for (Uint32 i = 0; i < SrcDesc.TriangleCount; ++i)
            {
                const BLASTriangleDesc& SrcTri = SrcDesc.pTriangles[i];
                const Uint32            Index  = Attribs.pDst->GetGeometryDescIndex(SrcTri.GeometryName);
                CHECK_COPY_BLAS_ATTRIBS(Index != INVALID_INDEX,
                                        "Src GeometryName ('", SrcTri.GeometryName, "') at index ", i, " is not found in pDst");
                const BLASTriangleDesc& DstTri = DstDesc.pTriangles[Index];

                CHECK_COPY_BLAS_ATTRIBS(SrcTri.MaxVertexCount == DstTri.MaxVertexCount,
                                        "MaxVertexCount value (", SrcTri.MaxVertexCount, ") in source triangle description at index ", i,
                                        " does not match MaxVertexCount value (", DstTri.MaxVertexCount, ") in the destination description");
                CHECK_COPY_BLAS_ATTRIBS(SrcTri.VertexValueType == DstTri.VertexValueType,
                                        "VertexValueType value (", GetValueTypeString(SrcTri.VertexValueType), ") in source triangle description at index ", i,
                                        " does not match VertexValueType value (", GetValueTypeString(DstTri.VertexValueType), ") in destination description");
                CHECK_COPY_BLAS_ATTRIBS(SrcTri.VertexComponentCount == DstTri.VertexComponentCount,
                                        "VertexComponentCount value (", Uint32{SrcTri.VertexComponentCount}, ") in source triangle description at index ", i,
                                        " does not match VertexComponentCount value (", Uint32{DstTri.VertexComponentCount}, ") in destination description");
                CHECK_COPY_BLAS_ATTRIBS(SrcTri.MaxPrimitiveCount == DstTri.MaxPrimitiveCount,
                                        "MaxPrimitiveCount value (", SrcTri.MaxPrimitiveCount, ") in source triangle description at index ", i,
                                        " does not match MaxPrimitiveCount value (", DstTri.MaxPrimitiveCount, ") in destination description");
                CHECK_COPY_BLAS_ATTRIBS(SrcTri.IndexType == DstTri.IndexType,
                                        "IndexType value (", GetValueTypeString(SrcTri.IndexType), ") in source triangle description at index ", i,
                                        " does not match IndexType value (", GetValueTypeString(DstTri.IndexType), ") in destination description");
                CHECK_COPY_BLAS_ATTRIBS(SrcTri.AllowsTransforms == DstTri.AllowsTransforms,
                                        "AllowsTransforms value (", (SrcTri.AllowsTransforms ? "true" : "false"), ") in source triangle description at index ", i,
                                        " does not match AllowsTransforms value (", (DstTri.AllowsTransforms ? "true" : "false"), ") in destination description");
            }

            for (Uint32 i = 0; i < SrcDesc.BoxCount; ++i)
            {
                const BLASBoundingBoxDesc& SrcBox = SrcDesc.pBoxes[i];
                const Uint32               Index  = Attribs.pDst->GetGeometryDescIndex(SrcBox.GeometryName);
                if (Index == INVALID_INDEX)
                {
                    LOG_ERROR_MESSAGE("Copy BLAS attribs are invalid: pSrc->GetDesc().pBoxes[", i, "].GeometryName ('", SrcBox.GeometryName, "') is not found in pDst");
                    return false;
                }
                const BLASBoundingBoxDesc& DstBox = DstDesc.pBoxes[Index];

                CHECK_COPY_BLAS_ATTRIBS(SrcBox.MaxBoxCount == DstBox.MaxBoxCount,
                                        "MaxBoxCountt value (", SrcBox.MaxBoxCount, ") in source box description at index ", i,
                                        " does not match MaxBoxCount value (", DstBox.MaxBoxCount, ") in destination description");
            }
        }
    }
    else if (Attribs.Mode == COPY_AS_MODE_COMPACT)
    {
        auto& SrcDesc = Attribs.pSrc->GetDesc();
        auto& DstDesc = Attribs.pDst->GetDesc();

        CHECK_COPY_BLAS_ATTRIBS((SrcDesc.Flags & RAYTRACING_BUILD_AS_ALLOW_COMPACTION) == RAYTRACING_BUILD_AS_ALLOW_COMPACTION, "must be have been create with RAYTRACING_BUILD_AS_ALLOW_COMPACTION flag");
        CHECK_COPY_BLAS_ATTRIBS(DstDesc.CompactedSize != 0, "pDst must have been create with non-zero CompactedSize");
    }
    else
    {
        LOG_ERROR_MESSAGE("IDeviceContext::CopyBLAS: unknown Mode");
        return false;
    }

#undef CHECK_COPY_BLAS_ATTRIBS

    return true;
}


bool VerifyCopyTLASAttribs(const CopyTLASAttribs& Attribs)
{
#define CHECK_COPY_TLAS_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Copy TLAS attribs are invalid: ", __VA_ARGS__)

    CHECK_COPY_TLAS_ATTRIBS(Attribs.pSrc != nullptr, "pSrc must not be null");
    CHECK_COPY_TLAS_ATTRIBS(Attribs.pDst != nullptr, "pDst must not be null");

    if (Attribs.Mode == COPY_AS_MODE_CLONE)
    {
        auto& SrcDesc = Attribs.pSrc->GetDesc();
        auto& DstDesc = Attribs.pDst->GetDesc();

        CHECK_COPY_TLAS_ATTRIBS(SrcDesc.MaxInstanceCount == DstDesc.MaxInstanceCount && SrcDesc.Flags == DstDesc.Flags,
                                "pDst must have been created with the same parameters as pSrc");
    }
    else if (Attribs.Mode == COPY_AS_MODE_COMPACT)
    {
        auto& SrcDesc = Attribs.pSrc->GetDesc();
        auto& DstDesc = Attribs.pDst->GetDesc();

        CHECK_COPY_TLAS_ATTRIBS((SrcDesc.Flags & RAYTRACING_BUILD_AS_ALLOW_COMPACTION) == RAYTRACING_BUILD_AS_ALLOW_COMPACTION, "pSrc was not created with RAYTRACING_BUILD_AS_ALLOW_COMPACTION flag");
        CHECK_COPY_TLAS_ATTRIBS(DstDesc.CompactedSize != 0, "pDst must have been create with non-zero CompactedSize");
    }
    else
    {
        LOG_ERROR_MESSAGE("IDeviceContext::CopyTLAS: unknown Mode");
        return false;
    }
#undef CHECK_COPY_TLAS_ATTRIBS

    return true;
}

bool VerifyWriteBLASCompactedSizeAttribs(const IRenderDevice* pDevice, const WriteBLASCompactedSizeAttribs& Attribs)
{
#define CHECK_WRITE_BLAS_SIZE_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Write compacted BLAS size attribs are invalid: ", __VA_ARGS__)

    CHECK_WRITE_BLAS_SIZE_ATTRIBS(Attribs.pBLAS != nullptr, "pBLAS must not be null");
    CHECK_WRITE_BLAS_SIZE_ATTRIBS((Attribs.pBLAS->GetDesc().Flags & RAYTRACING_BUILD_AS_ALLOW_COMPACTION) == RAYTRACING_BUILD_AS_ALLOW_COMPACTION,
                                  "pBLAS was not created with RAYTRACING_BUILD_AS_ALLOW_COMPACTION flag");

    CHECK_WRITE_BLAS_SIZE_ATTRIBS(Attribs.pDestBuffer != nullptr, "pDestBuffer must not be null");

    const BufferDesc& DstDesc = Attribs.pDestBuffer->GetDesc();
    CHECK_WRITE_BLAS_SIZE_ATTRIBS(Attribs.DestBufferOffset + sizeof(Uint64) <= DstDesc.uiSizeInBytes, "pDestBuffer is too small");

    if (pDevice->GetDeviceCaps().DevType == RENDER_DEVICE_TYPE_D3D12)
    {
        CHECK_WRITE_BLAS_SIZE_ATTRIBS((DstDesc.BindFlags & BIND_UNORDERED_ACCESS) == BIND_UNORDERED_ACCESS,
                                      "pDestBuffer must have been created with BIND_UNORDERED_ACCESS flag in Direct3D12");
    }

#undef CHECK_WRITE_BLAS_SIZE_ATTRIBS

    return true;
}

bool VerifyWriteTLASCompactedSizeAttribs(const IRenderDevice* pDevice, const WriteTLASCompactedSizeAttribs& Attribs)
{
#define CHECK_WRITE_TLAS_SIZE_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Write compacted TLAS size attribs are invalid: ", __VA_ARGS__)

    CHECK_WRITE_TLAS_SIZE_ATTRIBS(Attribs.pTLAS != nullptr, "pTLAS must not be null");
    CHECK_WRITE_TLAS_SIZE_ATTRIBS((Attribs.pTLAS->GetDesc().Flags & RAYTRACING_BUILD_AS_ALLOW_COMPACTION) == RAYTRACING_BUILD_AS_ALLOW_COMPACTION,
                                  "pTLAS was not created with RAYTRACING_BUILD_AS_ALLOW_COMPACTION flag");

    CHECK_WRITE_TLAS_SIZE_ATTRIBS(Attribs.pDestBuffer != nullptr, "pDestBuffer must not be null");

    const BufferDesc& DstDesc = Attribs.pDestBuffer->GetDesc();
    CHECK_WRITE_TLAS_SIZE_ATTRIBS(Attribs.DestBufferOffset + sizeof(Uint64) <= DstDesc.uiSizeInBytes, "pDestBuffer is too small");

    if (pDevice->GetDeviceCaps().DevType == RENDER_DEVICE_TYPE_D3D12)
    {
        CHECK_WRITE_TLAS_SIZE_ATTRIBS((DstDesc.BindFlags & BIND_UNORDERED_ACCESS) == BIND_UNORDERED_ACCESS,
                                      "pDestBuffer must have been created with BIND_UNORDERED_ACCESS flag");
    }

#undef CHECK_WRITE_TLAS_SIZE_ATTRIBS

    return true;
}

bool VerifyTraceRaysAttribs(const TraceRaysAttribs& Attribs)
{
#define CHECK_TRACE_RAYS_ATTRIBS(Expr, ...) CHECK_PARAMETER(Expr, "Trace rays attribs are invalid: ", __VA_ARGS__)
    CHECK_TRACE_RAYS_ATTRIBS(Attribs.pSBT != nullptr, "pSBT must not be null");

#ifdef DILIGENT_DEVELOPMENT
    CHECK_TRACE_RAYS_ATTRIBS(Attribs.pSBT->Verify(SHADER_BINDING_VALIDATION_SHADER_ONLY | SHADER_BINDING_VALIDATION_TLAS),
                             "pSBT not all shaders are binded or instance to shader mapping are incorrect");
#endif // DILIGENT_DEVELOPMENT

    CHECK_TRACE_RAYS_ATTRIBS(Attribs.DimensionX != 0, "DimensionX must not be zero.");
    CHECK_TRACE_RAYS_ATTRIBS(Attribs.DimensionY != 0, "DimensionY must not be zero.");
    CHECK_TRACE_RAYS_ATTRIBS(Attribs.DimensionZ != 0, "DimensionZ must not be zero.");

#undef CHECK_TRACE_RAYS_ATTRIBS

    return true;
}

} // namespace Diligent
