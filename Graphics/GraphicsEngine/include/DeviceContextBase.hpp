/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#pragma once

/// \file
/// Implementation of the Diligent::DeviceContextBase template class and related structures

#include <unordered_map>
#include <array>
#include <functional>

#include "PrivateConstants.h"
#include "DeviceContext.h"
#include "DeviceObjectBase.hpp"
#include "ResourceMapping.h"
#include "Sampler.h"
#include "ObjectBase.hpp"
#include "DebugUtilities.hpp"
#include "ValidatedCast.hpp"
#include "GraphicsAccessories.hpp"
#include "TextureBase.hpp"
#include "BasicMath.hpp"
#include "PlatformMisc.hpp"

namespace Diligent
{

// clang-format off
bool VerifyDrawAttribs               (const DrawAttribs&                Attribs);
bool VerifyDrawIndexedAttribs        (const DrawIndexedAttribs&         Attribs);
bool VerifyDrawIndirectAttribs       (const DrawIndirectAttribs&        Attribs, const IBuffer* pAttribsBuffer);
bool VerifyDrawIndexedIndirectAttribs(const DrawIndexedIndirectAttribs& Attribs, const IBuffer* pAttribsBuffer);

bool VerifyDispatchComputeAttribs        (const DispatchComputeAttribs&         Attribs);
bool VerifyDispatchComputeIndirectAttribs(const DispatchComputeIndirectAttribs& Attribs, const IBuffer* pAttribsBuffer);
// clang-format on

bool VerifyDrawMeshAttribs(Uint32 MaxDrawMeshTasksCount, const DrawMeshAttribs& Attribs);
bool VerifyDrawMeshIndirectAttribs(const DrawMeshIndirectAttribs& Attribs, const IBuffer* pAttribsBuffer);
bool VerifyDrawMeshIndirectCountAttribs(const DrawMeshIndirectCountAttribs& Attribs, const IBuffer* pAttribsBuffer, const IBuffer* pCountBuff, Uint32 IndirectCmdStride);

bool VerifyResolveTextureSubresourceAttribs(const ResolveTextureSubresourceAttribs& ResolveAttribs,
                                            const TextureDesc&                      SrcTexDesc,
                                            const TextureDesc&                      DstTexDesc);

bool VerifyBeginRenderPassAttribs(const BeginRenderPassAttribs& Attribs);
bool VerifyStateTransitionDesc(const IRenderDevice* pDevice, const StateTransitionDesc& Barrier);

bool VerifyBuildBLASAttribs(const BuildBLASAttribs& Attribs);
bool VerifyBuildTLASAttribs(const BuildTLASAttribs& Attribs);
bool VerifyCopyBLASAttribs(const IRenderDevice* pDevice, const CopyBLASAttribs& Attribs);
bool VerifyCopyTLASAttribs(const CopyTLASAttribs& Attribs);
bool VerifyWriteBLASCompactedSizeAttribs(const IRenderDevice* pDevice, const WriteBLASCompactedSizeAttribs& Attribs);
bool VerifyWriteTLASCompactedSizeAttribs(const IRenderDevice* pDevice, const WriteTLASCompactedSizeAttribs& Attribs);
bool VerifyTraceRaysAttribs(const TraceRaysAttribs& Attribs);
bool VerifyTraceRaysIndirectAttribs(const IRenderDevice*            pDevice,
                                    const TraceRaysIndirectAttribs& Attribs,
                                    const IBuffer*                  pAttribsBuffer,
                                    Uint32                          SBTSize);



/// Describes input vertex stream
template <typename BufferImplType>
struct VertexStreamInfo
{
    VertexStreamInfo() {}

    /// Strong reference to the buffer object
    RefCntAutoPtr<BufferImplType> pBuffer;

    /// Offset in bytes
    Uint32 Offset = 0;
};

/// Base implementation of the device context.

/// \tparam EngineImplTraits     - Engine implementation traits that define specific implementation details
///                                 (texture implementation type, buffer implementation type, etc.)
/// \remarks Device context keeps strong references to all objects currently bound to
///          the pipeline: buffers, tetxures, states, SRBs, etc.
///          The context also keeps strong references to the device and
///          the swap chain.
template <typename EngineImplTraits>
class DeviceContextBase : public ObjectBase<typename EngineImplTraits::DeviceContextInterface>
{
public:
    using BaseInterface                     = typename EngineImplTraits::DeviceContextInterface;
    using TObjectBase                       = ObjectBase<BaseInterface>;
    using DeviceImplType                    = typename EngineImplTraits::RenderDeviceImplType;
    using BufferImplType                    = typename EngineImplTraits::BufferImplType;
    using TextureImplType                   = typename EngineImplTraits::TextureImplType;
    using PipelineStateImplType             = typename EngineImplTraits::PipelineStateImplType;
    using ShaderResourceBindingImplType     = typename EngineImplTraits::ShaderResourceBindingImplType;
    using TextureViewImplType               = typename EngineImplTraits::TextureViewImplType;
    using QueryImplType                     = typename EngineImplTraits::QueryImplType;
    using FramebufferImplType               = typename EngineImplTraits::FramebufferImplType;
    using RenderPassImplType                = typename EngineImplTraits::RenderPassImplType;
    using BottomLevelASType                 = typename EngineImplTraits::BottomLevelASImplType;
    using TopLevelASType                    = typename EngineImplTraits::TopLevelASImplType;
    using ShaderBindingTableImplType        = typename EngineImplTraits::ShaderBindingTableImplType;
    using ShaderResourceCacheImplType       = typename EngineImplTraits::ShaderResourceCacheImplType;
    using PipelineResourceSignatureImplType = typename EngineImplTraits::PipelineResourceSignatureImplType;

    /// \param pRefCounters  - Reference counters object that controls the lifetime of this device context.
    /// \param pRenderDevice - Render device.
    /// \param Name          - Context name.
    /// \param bIsDeferred   - Flag indicating if this instance is a deferred context
    DeviceContextBase(IReferenceCounters* pRefCounters,
                      DeviceImplType*     pRenderDevice,
                      const char*         Name,
                      bool                bIsDeferred) :
        // clang-format off
        TObjectBase{pRefCounters    },
        m_pDevice  {pRenderDevice   },
        m_Name     {Name ? Name : ""}
    // clang-format on
    {
        m_Desc.Name           = m_Name.c_str();
        m_Desc.ContextType    = bIsDeferred ? CONTEXT_TYPE_UNKNOWN : CONTEXT_TYPE_GRAPHICS;
        m_Desc.IsDeferred     = bIsDeferred;
        m_Desc.QueueId        = MAX_COMMAND_QUEUES;
        m_Desc.CommandQueueId = MAX_COMMAND_QUEUES;

        m_Desc.TextureCopyGranularity[0] = 1;
        m_Desc.TextureCopyGranularity[1] = 1;
        m_Desc.TextureCopyGranularity[2] = 1;
    }

    ~DeviceContextBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_DeviceContext, TObjectBase)

    /// Implementation of IDeviceContext::GetDesc().
    virtual const DeviceContextDesc& DILIGENT_CALL_TYPE GetDesc() const override final { return m_Desc; }

    /// Base implementation of IDeviceContext::SetVertexBuffers(); validates parameters and
    /// caches references to the buffers.
    inline virtual void DILIGENT_CALL_TYPE SetVertexBuffers(Uint32                         StartSlot,
                                                            Uint32                         NumBuffersSet,
                                                            IBuffer**                      ppBuffers,
                                                            const Uint32*                  pOffsets,
                                                            RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                                            SET_VERTEX_BUFFERS_FLAGS       Flags) override = 0;

    inline virtual void DILIGENT_CALL_TYPE InvalidateState() override = 0;

    /// Base implementation of IDeviceContext::CommitShaderResources(); validates parameters.
    inline void CommitShaderResources(IShaderResourceBinding*        pShaderResourceBinding,
                                      RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                      int);

    /// Base implementation of IDeviceContext::SetIndexBuffer(); caches the strong reference to the index buffer
    inline virtual void DILIGENT_CALL_TYPE SetIndexBuffer(IBuffer*                       pIndexBuffer,
                                                          Uint32                         ByteOffset,
                                                          RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override = 0;

    /// Caches the viewports
    inline void SetViewports(Uint32 NumViewports, const Viewport* pViewports, Uint32& RTWidth, Uint32& RTHeight);

    /// Caches the scissor rects
    inline void SetScissorRects(Uint32 NumRects, const Rect* pRects, Uint32& RTWidth, Uint32& RTHeight);

    virtual void DILIGENT_CALL_TYPE BeginRenderPass(const BeginRenderPassAttribs& Attribs) override = 0;

    virtual void DILIGENT_CALL_TYPE NextSubpass() override = 0;

    virtual void DILIGENT_CALL_TYPE EndRenderPass() override = 0;

    /// Base implementation of IDeviceContext::UpdateBuffer(); validates input parameters.
    virtual void DILIGENT_CALL_TYPE UpdateBuffer(IBuffer*                       pBuffer,
                                                 Uint32                         Offset,
                                                 Uint32                         Size,
                                                 const void*                    pData,
                                                 RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) override = 0;

    /// Base implementation of IDeviceContext::CopyBuffer(); validates input parameters.
    virtual void DILIGENT_CALL_TYPE CopyBuffer(IBuffer*                       pSrcBuffer,
                                               Uint32                         SrcOffset,
                                               RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                                               IBuffer*                       pDstBuffer,
                                               Uint32                         DstOffset,
                                               Uint32                         Size,
                                               RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode) override = 0;

    /// Base implementation of IDeviceContext::MapBuffer(); validates input parameters.
    virtual void DILIGENT_CALL_TYPE MapBuffer(IBuffer*  pBuffer,
                                              MAP_TYPE  MapType,
                                              MAP_FLAGS MapFlags,
                                              PVoid&    pMappedData) override = 0;

    /// Base implementation of IDeviceContext::UnmapBuffer()
    virtual void DILIGENT_CALL_TYPE UnmapBuffer(IBuffer* pBuffer, MAP_TYPE MapType) override = 0;

    /// Base implementation of IDeviceContext::UpdateData(); validates input parameters
    virtual void DILIGENT_CALL_TYPE UpdateTexture(ITexture*                      pTexture,
                                                  Uint32                         MipLevel,
                                                  Uint32                         Slice,
                                                  const Box&                     DstBox,
                                                  const TextureSubResData&       SubresData,
                                                  RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                                                  RESOURCE_STATE_TRANSITION_MODE TextureTransitionMode) override = 0;

    /// Base implementation of IDeviceContext::CopyTexture(); validates input parameters
    virtual void DILIGENT_CALL_TYPE CopyTexture(const CopyTextureAttribs& CopyAttribs) override = 0;

    /// Base implementation of IDeviceContext::MapTextureSubresource()
    virtual void DILIGENT_CALL_TYPE MapTextureSubresource(ITexture*                 pTexture,
                                                          Uint32                    MipLevel,
                                                          Uint32                    ArraySlice,
                                                          MAP_TYPE                  MapType,
                                                          MAP_FLAGS                 MapFlags,
                                                          const Box*                pMapRegion,
                                                          MappedTextureSubresource& MappedData) override = 0;

    /// Base implementation of IDeviceContext::UnmapTextureSubresource()
    virtual void DILIGENT_CALL_TYPE UnmapTextureSubresource(ITexture* pTexture,
                                                            Uint32    MipLevel,
                                                            Uint32    ArraySlice) override = 0;

    virtual void DILIGENT_CALL_TYPE GenerateMips(ITextureView* pTexView) override = 0;

    virtual void DILIGENT_CALL_TYPE ResolveTextureSubresource(ITexture*                               pSrcTexture,
                                                              ITexture*                               pDstTexture,
                                                              const ResolveTextureSubresourceAttribs& ResolveAttribs) override = 0;

    virtual Uint64 DILIGENT_CALL_TYPE GetFrameNumber() const override final
    {
        return m_FrameNumber;
    }

    /// Implementation of IDeviceContext::SetUserData.
    virtual void DILIGENT_CALL_TYPE SetUserData(IObject* pUserData) override final
    {
        m_pUserData = pUserData;
    }

    /// Implementation of IDeviceContext::GetUserData.
    virtual IObject* DILIGENT_CALL_TYPE GetUserData() const override final
    {
        return m_pUserData.RawPtr<IObject>();
    }

    /// Returns currently bound pipeline state and blend factors
    inline void GetPipelineState(IPipelineState** ppPSO, float* BlendFactors, Uint32& StencilRef);

    /// Returns currently bound render targets
    inline void GetRenderTargets(Uint32& NumRenderTargets, ITextureView** ppRTVs, ITextureView** ppDSV);

    /// Returns currently set viewports
    inline void GetViewports(Uint32& NumViewports, Viewport* pViewports);

    /// Returns the render device
    IRenderDevice* GetDevice() { return m_pDevice; }

    virtual void ResetRenderTargets();

    bool IsDeferred() const { return m_Desc.IsDeferred; }

    /// Checks if a texture is bound as a render target or depth-stencil buffer and
    /// resets render targets if it is.
    bool UnbindTextureFromFramebuffer(TextureImplType* pTexture, bool bShowMessage);

    bool HasActiveRenderPass() const { return m_pActiveRenderPass != nullptr; }

    bool IsGraphicsCtx() const { return (m_Desc.ContextType & CONTEXT_TYPE_GRAPHICS) == CONTEXT_TYPE_GRAPHICS; }
    bool IsComputeCtx() const { return (m_Desc.ContextType & CONTEXT_TYPE_COMPUTE) == CONTEXT_TYPE_COMPUTE; }
    bool IsTransferCtx() const { return (m_Desc.ContextType & CONTEXT_TYPE_TRANSFER) == CONTEXT_TYPE_TRANSFER; }

protected:
    /// Committed shader resources for each resource signature
    struct CommittedShaderResources
    {
        // Pointers to shader resource caches for each signature
        std::array<ShaderResourceCacheImplType*, MAX_RESOURCE_SIGNATURES> ResourceCaches = {};

#ifdef DILIGENT_DEVELOPMENT
        // SRB array for each resource signature, corresponding to ResourceCaches
        std::array<RefCntWeakPtr<ShaderResourceBindingImplType>, MAX_RESOURCE_SIGNATURES> SRBs;

        // Shader resource cache version for every SRB at the time when the SRB was set
        std::array<Uint32, MAX_RESOURCE_SIGNATURES> CacheRevisions;

        // Indicates if the resources have been validated since they were committed
        bool ResourcesValidated = false;
#endif

        using SRBMaskType = Uint8;
        static_assert(sizeof(SRBMaskType) * 8 >= MAX_RESOURCE_SIGNATURES, "Not enough space to store MAX_RESOURCE_SIGNATURES bits");

        // Indicates which SRBs are active in current PSO
        SRBMaskType ActiveSRBMask = 0;

        // Indicates stale SRBs that have not been committed yet
        SRBMaskType StaleSRBMask = 0;

        // Indicates which SRBs have dynamic resources that need to be
        // processed every frame (e.g. USAGE_DYNAMIC buffers in Direct3D12 and Vulkan,
        // buffers with dynamic offsets in all backends).
        SRBMaskType DynamicSRBMask = 0;

        void Set(Uint32 Index, ShaderResourceBindingImplType* pSRB)
        {
            VERIFY_EXPR(Index < MAX_RESOURCE_SIGNATURES);
            auto* pResourceCache  = pSRB != nullptr ? &pSRB->GetResourceCache() : nullptr;
            ResourceCaches[Index] = pResourceCache;

            const auto SRBBit = static_cast<SRBMaskType>(1u << Index);
            if (pResourceCache != nullptr)
                StaleSRBMask |= SRBBit;
            else
                StaleSRBMask &= ~SRBBit;

            if (pResourceCache != nullptr && pResourceCache->HasDynamicResources())
                DynamicSRBMask |= SRBBit;
            else
                DynamicSRBMask &= ~SRBBit;

#ifdef DILIGENT_DEVELOPMENT
            SRBs[Index] = pSRB;
            if (pSRB != nullptr)
                ResourcesValidated = false;
            CacheRevisions[Index] = pResourceCache != nullptr ? pResourceCache->DvpGetRevision() : 0;
#endif
        }

        void MakeAllStale()
        {
            StaleSRBMask = 0xFFu;
        }

        // Returns the mask of SRBs whose resources need to be committed
        SRBMaskType GetCommitMask(bool DynamicResourcesIntact = false) const
        {
#ifdef DILIGENT_DEVELOPMENT
            DvpVerifyCacheRevisions();
#endif

            // Stale SRBs always have to be committed
            auto CommitMask = StaleSRBMask;
            // If dynamic resources are not intact, SRBs with dynamic resources
            // have to be handled
            if (!DynamicResourcesIntact)
                CommitMask |= DynamicSRBMask;
            // Only process SRBs that are used by current PSO
            CommitMask &= ActiveSRBMask;
            return CommitMask;
        }

#ifdef DILIGENT_DEVELOPMENT
        void DvpVerifyCacheRevisions() const
        {
            for (Uint32 ActiveSRBs = ActiveSRBMask; ActiveSRBs != 0;)
            {
                const auto  SRBBit = ExtractLSB(ActiveSRBs);
                const auto  Idx    = PlatformMisc::GetLSB(SRBBit);
                const auto* pCache = ResourceCaches[Idx];
                if (pCache != nullptr)
                {
                    DEV_CHECK_ERR(CacheRevisions[Idx] == pCache->DvpGetRevision(),
                                  "Reivsion of the shader resource cache at index ", Idx,
                                  " does not match the revision recorded when the SRB was committed. "
                                  "This indicates that resources have been changed since that time, but "
                                  "the SRB has not been committed with CommitShaderResources(). This usage is invalid.");
                }
                else
                {
                    // This error will be handled by DvpValidateCommittedShaderResources.
                }
            }
        }
#endif
    };

    /// Caches the render target and depth stencil views. Returns true if any view is different
    /// from the cached value and false otherwise.
    inline bool SetRenderTargets(Uint32 NumRenderTargets, ITextureView* ppRenderTargets[], ITextureView* pDepthStencil);

    /// Initializes render targets for the current subpass
    inline bool SetSubpassRenderTargets();

    inline bool SetBlendFactors(const float* BlendFactors, int Dummy);

    inline bool SetStencilRef(Uint32 StencilRef, int Dummy);

    inline void SetPipelineState(PipelineStateImplType* pPipelineState, int /*Dummy*/);

    /// Clears all cached resources
    inline void ClearStateCache();

    /// Checks if the texture is currently bound as a render target.
    bool CheckIfBoundAsRenderTarget(TextureImplType* pTexture);

    /// Checks if the texture is currently bound as depth-stencil buffer.
    bool CheckIfBoundAsDepthStencil(TextureImplType* pTexture);

    /// Updates the states of render pass attachments to match states within the gievn subpass
    void UpdateAttachmentStates(Uint32 SubpassIndex);

    void ClearDepthStencil(ITextureView* pView);

    void ClearRenderTarget(ITextureView* pView);

    void BeginQuery(IQuery* pQuery, int);

    void EndQuery(IQuery* pQuery, int);

    void EndFrame()
    {
        ++m_FrameNumber;
    }

    void PrepareCommittedResources(CommittedShaderResources& Resources, Uint32& DvpCompatibleSRBCount);

#ifdef DILIGENT_DEVELOPMENT
    // clang-format off
    void DvpVerifyDrawArguments                 (const DrawAttribs&                  Attribs) const;
    void DvpVerifyDrawIndexedArguments          (const DrawIndexedAttribs&           Attribs) const;
    void DvpVerifyDrawMeshArguments             (const DrawMeshAttribs&              Attribs) const;
    void DvpVerifyDrawIndirectArguments         (const DrawIndirectAttribs&          Attribs, const IBuffer* pAttribsBuffer) const;
    void DvpVerifyDrawIndexedIndirectArguments  (const DrawIndexedIndirectAttribs&   Attribs, const IBuffer* pAttribsBuffer) const;
    void DvpVerifyDrawMeshIndirectArguments     (const DrawMeshIndirectAttribs&      Attribs, const IBuffer* pAttribsBuffer) const;
    void DvpVerifyDrawMeshIndirectCountArguments(const DrawMeshIndirectCountAttribs& Attribs, const IBuffer* pAttribsBuffer, const IBuffer* pCountBuff) const;

    void DvpVerifyDispatchArguments        (const DispatchComputeAttribs& Attribs) const;
    void DvpVerifyDispatchIndirectArguments(const DispatchComputeIndirectAttribs& Attribs, const IBuffer* pAttribsBuffer) const;

    void DvpVerifyRenderTargets() const;
    void DvpVerifyStateTransitionDesc(const StateTransitionDesc& Barrier) const;
    void DvpVerifyTextureState(const TextureImplType&   Texture, RESOURCE_STATE RequiredState, const char* OperationName) const;
    void DvpVerifyBufferState (const BufferImplType&    Buffer,  RESOURCE_STATE RequiredState, const char* OperationName) const;
    void DvpVerifyBLASState   (const BottomLevelASType& BLAS,    RESOURCE_STATE RequiredState, const char* OperationName) const;
    void DvpVerifyTLASState   (const TopLevelASType&    TLAS,    RESOURCE_STATE RequiredState, const char* OperationName) const;
 
    // Verifies compatibility between current PSO and SRBs
    void DvpVerifySRBCompatibility(
        CommittedShaderResources&                                 Resources,
        std::function<PipelineResourceSignatureImplType*(Uint32)> CustomGetSignature = nullptr) const;
#else
    void DvpVerifyDrawArguments                 (const DrawAttribs&                  Attribs)const {}
    void DvpVerifyDrawIndexedArguments          (const DrawIndexedAttribs&           Attribs)const {}
    void DvpVerifyDrawMeshArguments             (const DrawMeshAttribs&              Attribs)const {}
    void DvpVerifyDrawIndirectArguments         (const DrawIndirectAttribs&          Attribs, const IBuffer* pAttribsBuffer)const {}
    void DvpVerifyDrawIndexedIndirectArguments  (const DrawIndexedIndirectAttribs&   Attribs, const IBuffer* pAttribsBuffer)const {}
    void DvpVerifyDrawMeshIndirectArguments     (const DrawMeshIndirectAttribs&      Attribs, const IBuffer* pAttribsBuffer)const {}
    void DvpVerifyDrawMeshIndirectCountArguments(const DrawMeshIndirectCountAttribs& Attribs, const IBuffer* pAttribsBuffer, const IBuffer* pCountBuff) const {}

    void DvpVerifyDispatchArguments        (const DispatchComputeAttribs& Attribs)const {}
    void DvpVerifyDispatchIndirectArguments(const DispatchComputeIndirectAttribs& Attribs, const IBuffer* pAttribsBuffer)const {}

    void DvpVerifyRenderTargets()const {}
    void DvpVerifyStateTransitionDesc(const StateTransitionDesc& Barrier)const {}
    void DvpVerifyTextureState(const TextureImplType&   Texture, RESOURCE_STATE RequiredState, const char* OperationName)const {}
    void DvpVerifyBufferState (const BufferImplType&    Buffer,  RESOURCE_STATE RequiredState, const char* OperationName)const {}
    void DvpVerifyBLASState   (const BottomLevelASType& BLAS,    RESOURCE_STATE RequiredState, const char* OperationName)const {}
    void DvpVerifyTLASState   (const TopLevelASType&    TLAS,    RESOURCE_STATE RequiredState, const char* OperationName)const {}
    // clang-format on
#endif

    void BuildBLAS(const BuildBLASAttribs& Attribs, int) const;
    void BuildTLAS(const BuildTLASAttribs& Attribs, int) const;
    void CopyBLAS(const CopyBLASAttribs& Attribs, int) const;
    void CopyTLAS(const CopyTLASAttribs& Attribs, int) const;
    void WriteBLASCompactedSize(const WriteBLASCompactedSizeAttribs& Attribs, int) const;
    void WriteTLASCompactedSize(const WriteTLASCompactedSizeAttribs& Attribs, int) const;
    void TraceRays(const TraceRaysAttribs& Attribs, int) const;
    void TraceRaysIndirect(const TraceRaysIndirectAttribs& Attribs, IBuffer* pAttribsBuffer, int) const;
    void UpdateSBT(IShaderBindingTable* pSBT, const UpdateIndirectRTBufferAttribs* pUpdateIndirectBufferAttribs, int) const;

protected:
    static constexpr Uint32 DrawMeshIndirectCommandStride = sizeof(Uint32) * 3; // D3D12: 12 bytes (x, y, z dimension)
                                                                                // Vulkan: 8 bytes (task count, first task)
    static constexpr Uint32 TraceRaysIndirectCommandSBTSize = 88;               // D3D12: 88 bytes, size of SBT offsets
                                                                                // Vulkan: 0 bytes, SBT offsets placed directly into function call
    static constexpr Uint32 TraceRaysIndirectCommandSize = 104;                 // SBT (88 bytes) + Dimension (3*4 bytes) aligned to 8 bytes

    /// Strong reference to the device.
    RefCntAutoPtr<DeviceImplType> m_pDevice;

    /// Vertex streams. Every stream holds strong reference to the buffer
    VertexStreamInfo<BufferImplType> m_VertexStreams[MAX_BUFFER_SLOTS];

    /// Number of bound vertex streams
    Uint32 m_NumVertexStreams = 0;

    /// Strong reference to the bound pipeline state object.
    /// Use final PSO implementation type to avoid virtual calls to AddRef()/Release().
    /// We need to keep strong reference as we examine previous pipeline state in
    /// SetPipelineState()
    RefCntAutoPtr<PipelineStateImplType> m_pPipelineState;

    /// Strong reference to the bound index buffer.
    /// Use final buffer implementation type to avoid virtual calls to AddRef()/Release()
    RefCntAutoPtr<BufferImplType> m_pIndexBuffer;

    /// Offset from the beginning of the index buffer to the start of the index data, in bytes.
    Uint32 m_IndexDataStartOffset = 0;

    /// Current stencil reference value
    Uint32 m_StencilRef = 0;

    /// Current blend factors
    Float32 m_BlendFactors[4] = {-1, -1, -1, -1};

    /// Current viewports
    Viewport m_Viewports[MAX_VIEWPORTS];
    /// Number of current viewports
    Uint32 m_NumViewports = 0;

    /// Current scissor rects
    Rect m_ScissorRects[MAX_VIEWPORTS];
    /// Number of current scissor rects
    Uint32 m_NumScissorRects = 0;

    /// Vector of strong references to the bound render targets.
    /// Use final texture view implementation type to avoid virtual calls to AddRef()/Release()
    RefCntAutoPtr<TextureViewImplType> m_pBoundRenderTargets[MAX_RENDER_TARGETS];
    /// Number of bound render targets
    Uint32 m_NumBoundRenderTargets = 0;
    /// Width of the currently bound framebuffer
    Uint32 m_FramebufferWidth = 0;
    /// Height of the currently bound framebuffer
    Uint32 m_FramebufferHeight = 0;
    /// Number of array slices in the currently bound framebuffer
    Uint32 m_FramebufferSlices = 0;
    /// Number of samples in the currently bound framebuffer
    Uint32 m_FramebufferSamples = 0;

    /// Strong references to the bound depth stencil view.
    /// Use final texture view implementation type to avoid virtual calls to AddRef()/Release()
    RefCntAutoPtr<TextureViewImplType> m_pBoundDepthStencil;

    /// Strong reference to the bound framebuffer.
    RefCntAutoPtr<FramebufferImplType> m_pBoundFramebuffer;

    /// Strong reference to the render pass.
    RefCntAutoPtr<RenderPassImplType> m_pActiveRenderPass;

    /// Current subpass index.
    Uint32 m_SubpassIndex = 0;

    /// Render pass attachments transition mode.
    RESOURCE_STATE_TRANSITION_MODE m_RenderPassAttachmentsTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;

    Uint64 m_FrameNumber = 0;

    RefCntAutoPtr<IObject> m_pUserData;

    DeviceContextDesc m_Desc;

    const String m_Name;

#ifdef DILIGENT_DEBUG
    // std::unordered_map is unbelievably slow. Keeping track of mapped buffers
    // in release builds is not feasible
    struct DbgMappedBufferInfo
    {
        MAP_TYPE MapType;
    };
    std::unordered_map<IBuffer*, DbgMappedBufferInfo> m_DbgMappedBuffers;
#endif
};


template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::SetVertexBuffers(
    Uint32                         StartSlot,
    Uint32                         NumBuffersSet,
    IBuffer**                      ppBuffers,
    const Uint32*                  pOffsets,
    RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
    SET_VERTEX_BUFFERS_FLAGS       Flags)
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "SetVertexBuffers is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(StartSlot < MAX_BUFFER_SLOTS, "Start vertex buffer slot ", StartSlot, " is out of allowed range [0, ", MAX_BUFFER_SLOTS - 1, "].");

    DEV_CHECK_ERR(StartSlot + NumBuffersSet <= MAX_BUFFER_SLOTS,
                  "The range of vertex buffer slots being set [", StartSlot, ", ", StartSlot + NumBuffersSet - 1,
                  "] is out of allowed range  [0, ", MAX_BUFFER_SLOTS - 1, "].");

    DEV_CHECK_ERR(!(m_pActiveRenderPass != nullptr && StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION),
                  "Resource state transitions are not allowed inside a render pass and may result in an undefined behavior. "
                  "Do not use RESOURCE_STATE_TRANSITION_MODE_TRANSITION or end the render pass first.");

    if (Flags & SET_VERTEX_BUFFERS_FLAG_RESET)
    {
        // Reset only these buffer slots that are not being set.
        // It is very important to not reset buffers that stay unchanged
        // as AddRef()/Release() are not free
        for (Uint32 s = 0; s < StartSlot; ++s)
            m_VertexStreams[s] = VertexStreamInfo<BufferImplType>{};
        for (Uint32 s = StartSlot + NumBuffersSet; s < m_NumVertexStreams; ++s)
            m_VertexStreams[s] = VertexStreamInfo<BufferImplType>{};
        m_NumVertexStreams = 0;
    }
    m_NumVertexStreams = std::max(m_NumVertexStreams, StartSlot + NumBuffersSet);

    for (Uint32 Buff = 0; Buff < NumBuffersSet; ++Buff)
    {
        auto& CurrStream   = m_VertexStreams[StartSlot + Buff];
        CurrStream.pBuffer = ppBuffers ? ValidatedCast<BufferImplType>(ppBuffers[Buff]) : nullptr;
        CurrStream.Offset  = pOffsets ? pOffsets[Buff] : 0;
#ifdef DILIGENT_DEVELOPMENT
        if (CurrStream.pBuffer)
        {
            const auto& BuffDesc = CurrStream.pBuffer->GetDesc();
            DEV_CHECK_ERR((BuffDesc.BindFlags & BIND_VERTEX_BUFFER) != 0,
                          "Buffer '", BuffDesc.Name ? BuffDesc.Name : "", "' being bound as vertex buffer to slot ", Buff,
                          " was not created with BIND_VERTEX_BUFFER flag");
        }
#endif
    }
    // Remove null buffers from the end of the array
    while (m_NumVertexStreams > 0 && !m_VertexStreams[m_NumVertexStreams - 1].pBuffer)
        m_VertexStreams[m_NumVertexStreams--] = VertexStreamInfo<BufferImplType>{};
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::SetPipelineState(
    PipelineStateImplType* pPipelineState,
    int /*Dummy*/)
{
    DEV_CHECK_ERR(IsComputeCtx(), "SetPipelineState is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    m_pPipelineState = pPipelineState;
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::CommitShaderResources(
    IShaderResourceBinding*        pShaderResourceBinding,
    RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
    int)
{
    DEV_CHECK_ERR(IsComputeCtx(), "CommitShaderResources is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(!(m_pActiveRenderPass != nullptr && StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION),
                  "Resource state transitions are not allowed inside a render pass and may result in an undefined behavior. "
                  "Do not use RESOURCE_STATE_TRANSITION_MODE_TRANSITION or end the render pass first.");

    DEV_CHECK_ERR(pShaderResourceBinding != nullptr, "pShaderResourceBinding must not be null");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::InvalidateState()
{
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "Invalidating context inside an active render pass. Call EndRenderPass() to finish the pass.");

    DeviceContextBase<ImplementationTraits>::ClearStateCache();
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::SetIndexBuffer(
    IBuffer*                       pIndexBuffer,
    Uint32                         ByteOffset,
    RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    m_pIndexBuffer         = ValidatedCast<BufferImplType>(pIndexBuffer);
    m_IndexDataStartOffset = ByteOffset;

#ifdef DILIGENT_DEVELOPMENT
    DEV_CHECK_ERR(IsGraphicsCtx(), "SetIndexBuffer is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(!(m_pActiveRenderPass != nullptr && StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION),
                  "Resource state transitions are not allowed inside a render pass and may result in an undefined behavior. "
                  "Do not use RESOURCE_STATE_TRANSITION_MODE_TRANSITION or end the render pass first.");

    if (m_pIndexBuffer)
    {
        const auto& BuffDesc = m_pIndexBuffer->GetDesc();
        DEV_CHECK_ERR((BuffDesc.BindFlags & BIND_INDEX_BUFFER) != 0,
                      "Buffer '", BuffDesc.Name ? BuffDesc.Name : "", "' being bound as index buffer was not created with BIND_INDEX_BUFFER flag");
    }
#endif
}


template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::GetPipelineState(IPipelineState** ppPSO, float* BlendFactors, Uint32& StencilRef)
{
    DEV_CHECK_ERR(ppPSO != nullptr, "Null pointer provided null");
    DEV_CHECK_ERR(*ppPSO == nullptr, "Memory address contains a pointer to a non-null blend state");
    if (m_pPipelineState)
    {
        m_pPipelineState->QueryInterface(IID_PipelineState, reinterpret_cast<IObject**>(ppPSO));
    }
    else
    {
        *ppPSO = nullptr;
    }

    for (Uint32 f = 0; f < 4; ++f)
        BlendFactors[f] = m_BlendFactors[f];
    StencilRef = m_StencilRef;
};

template <typename ImplementationTraits>
inline bool DeviceContextBase<ImplementationTraits>::SetBlendFactors(const float* BlendFactors, int)
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "SetBlendFactors is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    bool FactorsDiffer = false;
    for (Uint32 f = 0; f < 4; ++f)
    {
        if (m_BlendFactors[f] != BlendFactors[f])
            FactorsDiffer = true;
        m_BlendFactors[f] = BlendFactors[f];
    }
    return FactorsDiffer;
}

template <typename ImplementationTraits>
inline bool DeviceContextBase<ImplementationTraits>::SetStencilRef(Uint32 StencilRef, int)
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "SetStencilRef is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    if (m_StencilRef != StencilRef)
    {
        m_StencilRef = StencilRef;
        return true;
    }
    return false;
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::SetViewports(
    Uint32          NumViewports,
    const Viewport* pViewports,
    Uint32&         RTWidth,
    Uint32&         RTHeight)
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "SetViewports is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    if (RTWidth == 0 || RTHeight == 0)
    {
        RTWidth  = m_FramebufferWidth;
        RTHeight = m_FramebufferHeight;
    }

    DEV_CHECK_ERR(NumViewports < MAX_VIEWPORTS, "Number of viewports (", NumViewports, ") exceeds the limit (", MAX_VIEWPORTS, ")");
    m_NumViewports = std::min(MAX_VIEWPORTS, NumViewports);

    Viewport DefaultVP(0, 0, static_cast<float>(RTWidth), static_cast<float>(RTHeight));
    // If no viewports are specified, use default viewport
    if (m_NumViewports == 1 && pViewports == nullptr)
    {
        pViewports = &DefaultVP;
    }

    for (Uint32 vp = 0; vp < m_NumViewports; ++vp)
    {
        m_Viewports[vp] = pViewports[vp];
        DEV_CHECK_ERR(m_Viewports[vp].Width >= 0, "Incorrect viewport width (", m_Viewports[vp].Width, ")");
        DEV_CHECK_ERR(m_Viewports[vp].Height >= 0, "Incorrect viewport height (", m_Viewports[vp].Height, ")");
        DEV_CHECK_ERR(m_Viewports[vp].MaxDepth >= m_Viewports[vp].MinDepth, "Incorrect viewport depth range [", m_Viewports[vp].MinDepth, ", ", m_Viewports[vp].MaxDepth, "]");
    }
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::GetViewports(Uint32& NumViewports, Viewport* pViewports)
{
    NumViewports = m_NumViewports;
    if (pViewports)
    {
        for (Uint32 vp = 0; vp < m_NumViewports; ++vp)
            pViewports[vp] = m_Viewports[vp];
    }
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::SetScissorRects(
    Uint32      NumRects,
    const Rect* pRects,
    Uint32&     RTWidth,
    Uint32&     RTHeight)
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "SetScissorRects is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    if (RTWidth == 0 || RTHeight == 0)
    {
        RTWidth  = m_FramebufferWidth;
        RTHeight = m_FramebufferHeight;
    }

    DEV_CHECK_ERR(NumRects < MAX_VIEWPORTS, "Number of scissor rects (", NumRects, ") exceeds the limit (", MAX_VIEWPORTS, ")");
    m_NumScissorRects = std::min(MAX_VIEWPORTS, NumRects);

    for (Uint32 sr = 0; sr < m_NumScissorRects; ++sr)
    {
        m_ScissorRects[sr] = pRects[sr];
        DEV_CHECK_ERR(m_ScissorRects[sr].left <= m_ScissorRects[sr].right, "Incorrect horizontal bounds for a scissor rect [", m_ScissorRects[sr].left, ", ", m_ScissorRects[sr].right, ")");
        DEV_CHECK_ERR(m_ScissorRects[sr].top <= m_ScissorRects[sr].bottom, "Incorrect vertical bounds for a scissor rect [", m_ScissorRects[sr].top, ", ", m_ScissorRects[sr].bottom, ")");
    }
}

template <typename ImplementationTraits>
inline bool DeviceContextBase<ImplementationTraits>::SetRenderTargets(
    Uint32        NumRenderTargets,
    ITextureView* ppRenderTargets[],
    ITextureView* pDepthStencil)
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "SetRenderTargets is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    if (NumRenderTargets == 0 && pDepthStencil == nullptr)
    {
        ResetRenderTargets();
        return false;
    }

    bool bBindRenderTargets = false;
    m_FramebufferWidth      = 0;
    m_FramebufferHeight     = 0;
    m_FramebufferSlices     = 0;
    m_FramebufferSamples    = 0;

    if (NumRenderTargets != m_NumBoundRenderTargets)
    {
        bBindRenderTargets = true;
        for (Uint32 rt = NumRenderTargets; rt < m_NumBoundRenderTargets; ++rt)
            m_pBoundRenderTargets[rt].Release();

        m_NumBoundRenderTargets = NumRenderTargets;
    }

    for (Uint32 rt = 0; rt < NumRenderTargets; ++rt)
    {
        auto* pRTView = ppRenderTargets[rt];
        if (pRTView)
        {
            const auto& RTVDesc = pRTView->GetDesc();
            DEV_CHECK_ERR(RTVDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET,
                          "Texture view object named '", RTVDesc.Name ? RTVDesc.Name : "", "' has incorrect view type (", GetTexViewTypeLiteralName(RTVDesc.ViewType), "). Render target view is expected");

            // Use this RTV to set the render target size
            if (m_FramebufferWidth == 0)
            {
                auto*       pTex     = pRTView->GetTexture();
                const auto& TexDesc  = pTex->GetDesc();
                m_FramebufferWidth   = std::max(TexDesc.Width >> RTVDesc.MostDetailedMip, 1U);
                m_FramebufferHeight  = std::max(TexDesc.Height >> RTVDesc.MostDetailedMip, 1U);
                m_FramebufferSlices  = RTVDesc.NumArraySlices;
                m_FramebufferSamples = TexDesc.SampleCount;
            }
            else
            {
#ifdef DILIGENT_DEVELOPMENT
                const auto& TexDesc = pRTView->GetTexture()->GetDesc();
                DEV_CHECK_ERR(m_FramebufferWidth == std::max(TexDesc.Width >> RTVDesc.MostDetailedMip, 1U),
                              "Render target width (", std::max(TexDesc.Width >> RTVDesc.MostDetailedMip, 1U), ") specified by RTV '", RTVDesc.Name, "' is inconsistent with the width of previously bound render targets (", m_FramebufferWidth, ")");
                DEV_CHECK_ERR(m_FramebufferHeight == std::max(TexDesc.Height >> RTVDesc.MostDetailedMip, 1U),
                              "Render target height (", std::max(TexDesc.Height >> RTVDesc.MostDetailedMip, 1U), ") specified by RTV '", RTVDesc.Name, "' is inconsistent with the height of previously bound render targets (", m_FramebufferHeight, ")");
                DEV_CHECK_ERR(m_FramebufferSlices == RTVDesc.NumArraySlices,
                              "The number of slices (", RTVDesc.NumArraySlices, ") specified by RTV '", RTVDesc.Name, "' is inconsistent with the number of slices in previously bound render targets (", m_FramebufferSlices, ")");
                DEV_CHECK_ERR(m_FramebufferSamples == TexDesc.SampleCount,
                              "Sample count (", TexDesc.SampleCount, ") of RTV '", RTVDesc.Name, "' is inconsistent with the sample count of previously bound render targets (", m_FramebufferSamples, ")");
#endif
            }
        }

        // Here both views are certainly live objects, since we store
        // strong references to all bound render targets. So we
        // can safely compare pointers.
        if (m_pBoundRenderTargets[rt] != pRTView)
        {
            m_pBoundRenderTargets[rt] = ValidatedCast<TextureViewImplType>(pRTView);
            bBindRenderTargets        = true;
        }
    }

    if (pDepthStencil != nullptr)
    {
        const auto& DSVDesc = pDepthStencil->GetDesc();
        DEV_CHECK_ERR(DSVDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL,
                      "Texture view object named '", DSVDesc.Name ? DSVDesc.Name : "", "' has incorrect view type (", GetTexViewTypeLiteralName(DSVDesc.ViewType), "). Depth stencil view is expected");

        // Use depth stencil size to set render target size
        if (m_FramebufferWidth == 0)
        {
            auto*       pTex     = pDepthStencil->GetTexture();
            const auto& TexDesc  = pTex->GetDesc();
            m_FramebufferWidth   = std::max(TexDesc.Width >> DSVDesc.MostDetailedMip, 1U);
            m_FramebufferHeight  = std::max(TexDesc.Height >> DSVDesc.MostDetailedMip, 1U);
            m_FramebufferSlices  = DSVDesc.NumArraySlices;
            m_FramebufferSamples = TexDesc.SampleCount;
        }
        else
        {
#ifdef DILIGENT_DEVELOPMENT
            const auto& TexDesc = pDepthStencil->GetTexture()->GetDesc();
            DEV_CHECK_ERR(m_FramebufferWidth == std::max(TexDesc.Width >> DSVDesc.MostDetailedMip, 1U),
                          "Depth-stencil target width (", std::max(TexDesc.Width >> DSVDesc.MostDetailedMip, 1U), ") specified by DSV '", DSVDesc.Name, "' is inconsistent with the width of previously bound render targets (", m_FramebufferWidth, ")");
            DEV_CHECK_ERR(m_FramebufferHeight == std::max(TexDesc.Height >> DSVDesc.MostDetailedMip, 1U),
                          "Depth-stencil target height (", std::max(TexDesc.Height >> DSVDesc.MostDetailedMip, 1U), ") specified by DSV '", DSVDesc.Name, "' is inconsistent with the height of previously bound render targets (", m_FramebufferHeight, ")");
            DEV_CHECK_ERR(m_FramebufferSlices == DSVDesc.NumArraySlices,
                          "The number of slices (", DSVDesc.NumArraySlices, ") specified by DSV '", DSVDesc.Name, "' is inconsistent with the number of slices in previously bound render targets (", m_FramebufferSlices, ")");
            DEV_CHECK_ERR(m_FramebufferSamples == TexDesc.SampleCount,
                          "Sample count (", TexDesc.SampleCount, ") of DSV '", DSVDesc.Name, "' is inconsistent with the sample count of previously bound render targets (", m_FramebufferSamples, ")");
#endif
        }
    }

    if (m_pBoundDepthStencil != pDepthStencil)
    {
        m_pBoundDepthStencil = ValidatedCast<TextureViewImplType>(pDepthStencil);
        bBindRenderTargets   = true;
    }

    VERIFY_EXPR(m_FramebufferWidth > 0 && m_FramebufferHeight > 0 && m_FramebufferSlices > 0 && m_FramebufferSamples > 0);

    return bBindRenderTargets;
}

template <typename ImplementationTraits>
inline bool DeviceContextBase<ImplementationTraits>::SetSubpassRenderTargets()
{
    VERIFY_EXPR(m_pBoundFramebuffer);
    VERIFY_EXPR(m_pActiveRenderPass);

    const auto& RPDesc = m_pActiveRenderPass->GetDesc();
    const auto& FBDesc = m_pBoundFramebuffer->GetDesc();
    VERIFY_EXPR(m_SubpassIndex < RPDesc.SubpassCount);
    const auto& Subpass = RPDesc.pSubpasses[m_SubpassIndex];

    m_FramebufferSamples = 0;

    ITextureView* ppRTVs[MAX_RENDER_TARGETS] = {};
    ITextureView* pDSV                       = nullptr;
    for (Uint32 rt = 0; rt < Subpass.RenderTargetAttachmentCount; ++rt)
    {
        const auto& RTAttachmentRef = Subpass.pRenderTargetAttachments[rt];
        if (RTAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
        {
            VERIFY_EXPR(RTAttachmentRef.AttachmentIndex < RPDesc.AttachmentCount);
            ppRTVs[rt] = FBDesc.ppAttachments[RTAttachmentRef.AttachmentIndex];
            if (ppRTVs[rt] != nullptr)
            {
                if (m_FramebufferSamples == 0)
                    m_FramebufferSamples = ppRTVs[rt]->GetTexture()->GetDesc().SampleCount;
                else
                    DEV_CHECK_ERR(m_FramebufferSamples == ppRTVs[rt]->GetTexture()->GetDesc().SampleCount, "Inconsistent sample count");
            }
        }
    }

    if (Subpass.pDepthStencilAttachment != nullptr)
    {
        const auto& DSAttachmentRef = *Subpass.pDepthStencilAttachment;
        if (DSAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
        {
            VERIFY_EXPR(DSAttachmentRef.AttachmentIndex < RPDesc.AttachmentCount);
            pDSV = FBDesc.ppAttachments[DSAttachmentRef.AttachmentIndex];
            if (pDSV != nullptr)
            {
                if (m_FramebufferSamples == 0)
                    m_FramebufferSamples = pDSV->GetTexture()->GetDesc().SampleCount;
                else
                    DEV_CHECK_ERR(m_FramebufferSamples == pDSV->GetTexture()->GetDesc().SampleCount, "Inconsistent sample count");
            }
        }
    }
    bool BindRenderTargets = SetRenderTargets(Subpass.RenderTargetAttachmentCount, ppRTVs, pDSV);

    // Use framebuffer dimensions (override what was set by SetRenderTargets)
    m_FramebufferWidth  = FBDesc.Width;
    m_FramebufferHeight = FBDesc.Height;
    m_FramebufferSlices = FBDesc.NumArraySlices;
    VERIFY_EXPR(m_FramebufferSamples > 0);

    return BindRenderTargets;
}


template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::GetRenderTargets(
    Uint32&        NumRenderTargets,
    ITextureView** ppRTVs,
    ITextureView** ppDSV)
{
    NumRenderTargets = m_NumBoundRenderTargets;

    if (ppRTVs)
    {
        for (Uint32 rt = 0; rt < NumRenderTargets; ++rt)
        {
            DEV_CHECK_ERR(ppRTVs[rt] == nullptr, "Non-null pointer found in RTV array element #", rt);
            auto pBoundRTV = m_pBoundRenderTargets[rt];
            if (pBoundRTV)
                pBoundRTV->QueryInterface(IID_TextureView, reinterpret_cast<IObject**>(ppRTVs + rt));
            else
                ppRTVs[rt] = nullptr;
        }
        for (Uint32 rt = NumRenderTargets; rt < MAX_RENDER_TARGETS; ++rt)
        {
            DEV_CHECK_ERR(ppRTVs[rt] == nullptr, "Non-null pointer found in RTV array element #", rt);
            ppRTVs[rt] = nullptr;
        }
    }

    if (ppDSV)
    {
        DEV_CHECK_ERR(*ppDSV == nullptr, "Non-null DSV pointer found");
        if (m_pBoundDepthStencil)
            m_pBoundDepthStencil->QueryInterface(IID_TextureView, reinterpret_cast<IObject**>(ppDSV));
        else
            *ppDSV = nullptr;
    }
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::ClearStateCache()
{
    for (Uint32 stream = 0; stream < m_NumVertexStreams; ++stream)
        m_VertexStreams[stream] = VertexStreamInfo<BufferImplType>{};
#ifdef DILIGENT_DEBUG
    for (Uint32 stream = m_NumVertexStreams; stream < _countof(m_VertexStreams); ++stream)
    {
        VERIFY(m_VertexStreams[stream].pBuffer == nullptr, "Unexpected non-null buffer");
        VERIFY(m_VertexStreams[stream].Offset == 0, "Unexpected non-zero offset");
    }
#endif
    m_NumVertexStreams = 0;

    m_pPipelineState.Release();

    m_pIndexBuffer.Release();
    m_IndexDataStartOffset = 0;

    m_StencilRef = 0;

    for (int i = 0; i < 4; ++i)
        m_BlendFactors[i] = -1;

    for (Uint32 vp = 0; vp < m_NumViewports; ++vp)
        m_Viewports[vp] = Viewport();
    m_NumViewports = 0;

    for (Uint32 sr = 0; sr < m_NumScissorRects; ++sr)
        m_ScissorRects[sr] = Rect();
    m_NumScissorRects = 0;

    ResetRenderTargets();

    VERIFY(!m_pActiveRenderPass, "Clearing state cache inside an active render pass");
    m_pActiveRenderPass = nullptr;
    m_pBoundFramebuffer = nullptr;
}

template <typename ImplementationTraits>
bool DeviceContextBase<ImplementationTraits>::CheckIfBoundAsRenderTarget(TextureImplType* pTexture)
{
    if (pTexture == nullptr)
        return false;

    for (Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt)
    {
        if (m_pBoundRenderTargets[rt] && m_pBoundRenderTargets[rt]->GetTexture() == pTexture)
        {
            return true;
        }
    }

    return false;
}

template <typename ImplementationTraits>
bool DeviceContextBase<ImplementationTraits>::CheckIfBoundAsDepthStencil(TextureImplType* pTexture)
{
    if (pTexture == nullptr)
        return false;

    return m_pBoundDepthStencil && m_pBoundDepthStencil->GetTexture() == pTexture;
}

template <typename ImplementationTraits>
bool DeviceContextBase<ImplementationTraits>::UnbindTextureFromFramebuffer(TextureImplType* pTexture, bool bShowMessage)
{
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "State transitions are not allowed inside a render pass.");

    if (pTexture == nullptr)
        return false;

    const auto& TexDesc = pTexture->GetDesc();

    bool bResetRenderTargets = false;
    if (TexDesc.BindFlags & BIND_RENDER_TARGET)
    {
        if (CheckIfBoundAsRenderTarget(pTexture))
        {
            if (bShowMessage)
            {
                LOG_INFO_MESSAGE("Texture '", TexDesc.Name,
                                 "' is currently bound as render target and will be unset along with all "
                                 "other render targets and depth-stencil buffer. "
                                 "Call SetRenderTargets() to reset the render targets.\n"
                                 "To silence this message, explicitly unbind the texture with "
                                 "SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE)");
            }

            bResetRenderTargets = true;
        }
    }

    if (TexDesc.BindFlags & BIND_DEPTH_STENCIL)
    {
        if (CheckIfBoundAsDepthStencil(pTexture))
        {
            if (bShowMessage)
            {
                LOG_INFO_MESSAGE("Texture '", TexDesc.Name,
                                 "' is currently bound as depth buffer and will be unset along with "
                                 "all render targets. Call SetRenderTargets() to reset the render targets.\n"
                                 "To silence this message, explicitly unbind the texture with "
                                 "SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE)");
            }

            bResetRenderTargets = true;
        }
    }

    if (bResetRenderTargets)
    {
        ResetRenderTargets();
    }

    return bResetRenderTargets;
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::ResetRenderTargets()
{
    for (Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt)
        m_pBoundRenderTargets[rt].Release();
#ifdef DILIGENT_DEBUG
    for (Uint32 rt = m_NumBoundRenderTargets; rt < _countof(m_pBoundRenderTargets); ++rt)
    {
        VERIFY(m_pBoundRenderTargets[rt] == nullptr, "Non-null render target found");
    }
#endif
    m_NumBoundRenderTargets = 0;
    m_FramebufferWidth      = 0;
    m_FramebufferHeight     = 0;
    m_FramebufferSlices     = 0;
    m_FramebufferSamples    = 0;

    m_pBoundDepthStencil.Release();

    // Do not reset framebuffer here as there may potentially
    // be a subpass without any render target attachments.
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::BeginRenderPass(const BeginRenderPassAttribs& Attribs)
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "BeginRenderPass is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "Attempting to begin render pass while another render pass ('", m_pActiveRenderPass->GetDesc().Name, "') is active.");
    DEV_CHECK_ERR(m_pBoundFramebuffer == nullptr, "Attempting to begin render pass while another framebuffer ('", m_pBoundFramebuffer->GetDesc().Name, "') is bound.");

    VerifyBeginRenderPassAttribs(Attribs);

    // Reset current render targets (in Vulkan backend, this may end current render pass).
    ResetRenderTargets();

    auto* pNewRenderPass  = ValidatedCast<RenderPassImplType>(Attribs.pRenderPass);
    auto* pNewFramebuffer = ValidatedCast<FramebufferImplType>(Attribs.pFramebuffer);
    if (Attribs.StateTransitionMode != RESOURCE_STATE_TRANSITION_MODE_NONE)
    {
        const auto& RPDesc = pNewRenderPass->GetDesc();
        const auto& FBDesc = pNewFramebuffer->GetDesc();
        DEV_CHECK_ERR(RPDesc.AttachmentCount <= FBDesc.AttachmentCount,
                      "The number of attachments (", FBDesc.AttachmentCount,
                      ") in currently bound framebuffer is smaller than the number of attachments in the render pass (", RPDesc.AttachmentCount, ")");
        for (Uint32 i = 0; i < FBDesc.AttachmentCount; ++i)
        {
            auto* pView = FBDesc.ppAttachments[i];
            if (pView == nullptr)
                return;

            auto* pTex          = ValidatedCast<TextureImplType>(pView->GetTexture());
            auto  RequiredState = RPDesc.pAttachments[i].InitialState;
            if (Attribs.StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
            {
                if (pTex->IsInKnownState() && !pTex->CheckState(RequiredState))
                {
                    StateTransitionDesc Barrier{pTex, RESOURCE_STATE_UNKNOWN, RequiredState, true};
                    this->TransitionResourceStates(1, &Barrier);
                }
            }
            else if (Attribs.StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_VERIFY)
            {
                DvpVerifyTextureState(*pTex, RequiredState, "BeginRenderPass");
            }
        }
    }

    m_pActiveRenderPass                   = pNewRenderPass;
    m_pBoundFramebuffer                   = pNewFramebuffer;
    m_SubpassIndex                        = 0;
    m_RenderPassAttachmentsTransitionMode = Attribs.StateTransitionMode;

    UpdateAttachmentStates(m_SubpassIndex);
    SetSubpassRenderTargets();
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::NextSubpass()
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "NextSubpass is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pActiveRenderPass != nullptr, "There is no active render pass");
    VERIFY(m_SubpassIndex + 1 < m_pActiveRenderPass->GetDesc().SubpassCount, "The render pass has reached the final subpass already");
    ++m_SubpassIndex;
    UpdateAttachmentStates(m_SubpassIndex);
    SetSubpassRenderTargets();
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::UpdateAttachmentStates(Uint32 SubpassIndex)
{
    if (m_RenderPassAttachmentsTransitionMode != RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
        return;

    DEV_CHECK_ERR(m_pActiveRenderPass != nullptr, "There is no active render pass");
    DEV_CHECK_ERR(m_pBoundFramebuffer != nullptr, "There is no active framebuffer");

    const auto& RPDesc = m_pActiveRenderPass->GetDesc();
    const auto& FBDesc = m_pBoundFramebuffer->GetDesc();
    VERIFY(FBDesc.AttachmentCount == RPDesc.AttachmentCount,
           "Framebuffer attachment count (", FBDesc.AttachmentCount, ") is not consistent with the render pass attachment count (", RPDesc.AttachmentCount, ")");
    VERIFY_EXPR(SubpassIndex <= RPDesc.SubpassCount);
    for (Uint32 i = 0; i < RPDesc.AttachmentCount; ++i)
    {
        if (auto* pView = FBDesc.ppAttachments[i])
        {
            auto* pTex = ValidatedCast<TextureImplType>(pView->GetTexture());
            if (pTex->IsInKnownState())
            {
                auto CurrState = SubpassIndex < RPDesc.SubpassCount ?
                    m_pActiveRenderPass->GetAttachmentState(SubpassIndex, i) :
                    RPDesc.pAttachments[i].FinalState;
                pTex->SetState(CurrState);
            }
        }
    }
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::EndRenderPass()
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "EndRenderPass is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pActiveRenderPass != nullptr, "There is no active render pass");
    DEV_CHECK_ERR(m_pBoundFramebuffer != nullptr, "There is no active framebuffer");
    VERIFY(m_pActiveRenderPass->GetDesc().SubpassCount == m_SubpassIndex + 1,
           "Ending render pass at subpass ", m_SubpassIndex, " before reaching the final subpass");

    UpdateAttachmentStates(m_SubpassIndex + 1);

    m_pActiveRenderPass.Release();
    m_pBoundFramebuffer.Release();
    m_SubpassIndex                        = 0;
    m_RenderPassAttachmentsTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;
    ResetRenderTargets();
}


template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::ClearDepthStencil(ITextureView* pView)
{
    DEV_CHECK_ERR(pView != nullptr, "Depth-stencil view to clear must not be null");

    DEV_CHECK_ERR(IsGraphicsCtx(), "ClearDepthStencil is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

#ifdef DILIGENT_DEVELOPMENT
    {
        const auto& ViewDesc = pView->GetDesc();
        DEV_CHECK_ERR(ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL,
                      "The type (", GetTexViewTypeLiteralName(ViewDesc.ViewType), ") of the texture view '", ViewDesc.Name,
                      "' is invalid: ClearDepthStencil command expects depth-stencil view (TEXTURE_VIEW_DEPTH_STENCIL).");

        if (pView != m_pBoundDepthStencil)
        {
            DEV_CHECK_ERR(m_pActiveRenderPass == nullptr,
                          "Depth-stencil view '", ViewDesc.Name,
                          "' is not bound as framebuffer attachment. ClearDepthStencil command inside a render pass "
                          "requires depth-stencil view to be bound as a framebuffer attachment.");

            if (m_pDevice->GetDeviceCaps().IsGLDevice())
            {
                LOG_ERROR_MESSAGE("Depth-stencil view '", ViewDesc.Name,
                                  "' is not bound to the device context. ClearDepthStencil command requires "
                                  "depth-stencil view be bound to the device context in OpenGL backend");
            }
            else
            {
                LOG_WARNING_MESSAGE("Depth-stencil view '", ViewDesc.Name,
                                    "' is not bound to the device context. "
                                    "ClearDepthStencil command is more efficient when depth-stencil "
                                    "view is bound to the context. In OpenGL backend this is a requirement.");
            }
        }
    }
#endif
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::ClearRenderTarget(ITextureView* pView)
{
    DEV_CHECK_ERR(pView != nullptr, "Render target view to clear must not be null");
    DEV_CHECK_ERR(IsGraphicsCtx(), "ClearRenderTarget is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

#ifdef DILIGENT_DEVELOPMENT
    {
        const auto& ViewDesc = pView->GetDesc();
        DEV_CHECK_ERR(ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET,
                      "The type (", GetTexViewTypeLiteralName(ViewDesc.ViewType), ") of texture view '", pView->GetDesc().Name,
                      "' is invalid: ClearRenderTarget command expects render target view (TEXTURE_VIEW_RENDER_TARGET).");

        bool RTFound = false;
        for (Uint32 i = 0; i < m_NumBoundRenderTargets && !RTFound; ++i)
        {
            RTFound = m_pBoundRenderTargets[i] == pView;
        }

        if (!RTFound)
        {
            DEV_CHECK_ERR(m_pActiveRenderPass == nullptr,
                          "Render target view '", ViewDesc.Name,
                          "' is not bound as framebuffer attachment. ClearRenderTarget command inside a render pass "
                          "requires render target view to be bound as a framebuffer attachment.");

            if (m_pDevice->GetDeviceCaps().IsGLDevice())
            {
                LOG_ERROR_MESSAGE("Render target view '", ViewDesc.Name,
                                  "' is not bound to the device context. ClearRenderTarget command "
                                  "requires render target view to be bound to the device context in OpenGL backend");
            }
            else
            {
                LOG_WARNING_MESSAGE("Render target view '", ViewDesc.Name,
                                    "' is not bound to the device context. ClearRenderTarget command is more efficient "
                                    "if render target view is bound to the device context. In OpenGL backend this is a requirement.");
            }
        }
    }
#endif
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::BeginQuery(IQuery* pQuery, int)
{
    DEV_CHECK_ERR(pQuery != nullptr, "IDeviceContext::BeginQuery: pQuery must not be null");

    DEV_CHECK_ERR(!IsDeferred(), "IDeviceContext::BeginQuery: Deferred contexts do not support queries");

    const auto QueryType = pQuery->GetDesc().Type;
    DEV_CHECK_ERR(QueryType != QUERY_TYPE_TIMESTAMP,
                  "BeginQuery() is disabled for timestamp queries. Call EndQuery() to set the timestamp.");

    DEV_CHECK_ERR((QueryType == QUERY_TYPE_DURATION ? IsComputeCtx() : IsGraphicsCtx()),
                  "BeginQuery with type ", GetQueryTypeString(QueryType), " is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    ValidatedCast<QueryImplType>(pQuery)->OnBeginQuery(this);
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::EndQuery(IQuery* pQuery, int)
{
    DEV_CHECK_ERR(pQuery != nullptr, "IDeviceContext::EndQuery: pQuery must not be null");

    DEV_CHECK_ERR(!IsDeferred(), "IDeviceContext::EndQuery: Deferred contexts do not support queries");

    const auto QueryType = pQuery->GetDesc().Type;
    DEV_CHECK_ERR((QueryType == QUERY_TYPE_DURATION || QueryType == QUERY_TYPE_TIMESTAMP ? IsComputeCtx() : IsGraphicsCtx()),
                  "BeginQuery with type ", GetQueryTypeString(QueryType), " is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    ValidatedCast<QueryImplType>(pQuery)->OnEndQuery(this);
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::UpdateBuffer(
    IBuffer*                       pBuffer,
    Uint32                         Offset,
    Uint32                         Size,
    const void*                    pData,
    RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    DEV_CHECK_ERR(IsTransferCtx(), "UpdateBuffer is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(pBuffer != nullptr, "Buffer must not be null");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "UpdateBuffer command must be used outside of render pass.");
#ifdef DILIGENT_DEVELOPMENT
    {
        const auto& BuffDesc = ValidatedCast<BufferImplType>(pBuffer)->GetDesc();
        DEV_CHECK_ERR(BuffDesc.Usage == USAGE_DEFAULT, "Unable to update buffer '", BuffDesc.Name, "': only USAGE_DEFAULT buffers can be updated with UpdateData()");
        DEV_CHECK_ERR(Offset < BuffDesc.uiSizeInBytes, "Unable to update buffer '", BuffDesc.Name, "': offset (", Offset, ") exceeds the buffer size (", BuffDesc.uiSizeInBytes, ")");
        DEV_CHECK_ERR(Size + Offset <= BuffDesc.uiSizeInBytes, "Unable to update buffer '", BuffDesc.Name, "': Update region [", Offset, ",", Size + Offset, ") is out of buffer bounds [0,", BuffDesc.uiSizeInBytes, ")");
    }
#endif
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::CopyBuffer(
    IBuffer*                       pSrcBuffer,
    Uint32                         SrcOffset,
    RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
    IBuffer*                       pDstBuffer,
    Uint32                         DstOffset,
    Uint32                         Size,
    RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode)
{
    DEV_CHECK_ERR(IsTransferCtx(), "CopyBuffer is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(pSrcBuffer != nullptr, "Source buffer must not be null");
    DEV_CHECK_ERR(pDstBuffer != nullptr, "Destination buffer must not be null");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "CopyBuffer command must be used outside of render pass.");
#ifdef DILIGENT_DEVELOPMENT
    {
        const auto& SrcBufferDesc = ValidatedCast<BufferImplType>(pSrcBuffer)->GetDesc();
        const auto& DstBufferDesc = ValidatedCast<BufferImplType>(pDstBuffer)->GetDesc();
        DEV_CHECK_ERR(DstOffset + Size <= DstBufferDesc.uiSizeInBytes, "Failed to copy buffer '", SrcBufferDesc.Name, "' to '", DstBufferDesc.Name, "': Destination range [", DstOffset, ",", DstOffset + Size, ") is out of buffer bounds [0,", DstBufferDesc.uiSizeInBytes, ")");
        DEV_CHECK_ERR(SrcOffset + Size <= SrcBufferDesc.uiSizeInBytes, "Failed to copy buffer '", SrcBufferDesc.Name, "' to '", DstBufferDesc.Name, "': Source range [", SrcOffset, ",", SrcOffset + Size, ") is out of buffer bounds [0,", SrcBufferDesc.uiSizeInBytes, ")");
    }
#endif
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::MapBuffer(
    IBuffer*  pBuffer,
    MAP_TYPE  MapType,
    MAP_FLAGS MapFlags,
    PVoid&    pMappedData)
{
    DEV_CHECK_ERR(pBuffer, "pBuffer must not be null");

    const auto& BuffDesc = pBuffer->GetDesc();

#ifdef DILIGENT_DEBUG
    {
        VERIFY(m_DbgMappedBuffers.find(pBuffer) == m_DbgMappedBuffers.end(), "Buffer '", BuffDesc.Name, "' has already been mapped");
        m_DbgMappedBuffers[pBuffer] = DbgMappedBufferInfo{MapType};
    }
#endif

    pMappedData = nullptr;
    switch (MapType)
    {
        case MAP_READ:
            DEV_CHECK_ERR(BuffDesc.Usage == USAGE_STAGING || BuffDesc.Usage == USAGE_UNIFIED,
                          "Only buffers with usage USAGE_STAGING or USAGE_UNIFIED can be mapped for reading");
            DEV_CHECK_ERR((BuffDesc.CPUAccessFlags & CPU_ACCESS_READ), "Buffer being mapped for reading was not created with CPU_ACCESS_READ flag");
            DEV_CHECK_ERR((MapFlags & MAP_FLAG_DISCARD) == 0, "MAP_FLAG_DISCARD is not valid when mapping buffer for reading");
            break;

        case MAP_WRITE:
            DEV_CHECK_ERR(BuffDesc.Usage == USAGE_DYNAMIC || BuffDesc.Usage == USAGE_STAGING || BuffDesc.Usage == USAGE_UNIFIED,
                          "Only buffers with usage USAGE_STAGING, USAGE_DYNAMIC or USAGE_UNIFIED can be mapped for writing");
            DEV_CHECK_ERR((BuffDesc.CPUAccessFlags & CPU_ACCESS_WRITE), "Buffer being mapped for writing was not created with CPU_ACCESS_WRITE flag");
            break;

        case MAP_READ_WRITE:
            DEV_CHECK_ERR(BuffDesc.Usage == USAGE_STAGING || BuffDesc.Usage == USAGE_UNIFIED,
                          "Only buffers with usage USAGE_STAGING or USAGE_UNIFIED can be mapped for reading and writing");
            DEV_CHECK_ERR((BuffDesc.CPUAccessFlags & CPU_ACCESS_WRITE), "Buffer being mapped for reading & writing was not created with CPU_ACCESS_WRITE flag");
            DEV_CHECK_ERR((BuffDesc.CPUAccessFlags & CPU_ACCESS_READ), "Buffer being mapped for reading & writing was not created with CPU_ACCESS_READ flag");
            DEV_CHECK_ERR((MapFlags & MAP_FLAG_DISCARD) == 0, "MAP_FLAG_DISCARD is not valid when mapping buffer for reading and writing");
            break;

        default: UNEXPECTED("Unknown map type");
    }

    if (BuffDesc.Usage == USAGE_DYNAMIC)
    {
        DEV_CHECK_ERR((MapFlags & (MAP_FLAG_DISCARD | MAP_FLAG_NO_OVERWRITE)) != 0 && MapType == MAP_WRITE, "Dynamic buffers can only be mapped for writing with MAP_FLAG_DISCARD or MAP_FLAG_NO_OVERWRITE flag");
        DEV_CHECK_ERR((MapFlags & (MAP_FLAG_DISCARD | MAP_FLAG_NO_OVERWRITE)) != (MAP_FLAG_DISCARD | MAP_FLAG_NO_OVERWRITE), "When mapping dynamic buffer, only one of MAP_FLAG_DISCARD or MAP_FLAG_NO_OVERWRITE flags must be specified");
    }

    if ((MapFlags & MAP_FLAG_DISCARD) != 0)
    {
        DEV_CHECK_ERR(BuffDesc.Usage == USAGE_DYNAMIC || BuffDesc.Usage == USAGE_STAGING, "Only dynamic and staging buffers can be mapped with discard flag");
        DEV_CHECK_ERR(MapType == MAP_WRITE, "MAP_FLAG_DISCARD is only valid when mapping buffer for writing");
    }
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::UnmapBuffer(IBuffer* pBuffer, MAP_TYPE MapType)
{
    VERIFY(pBuffer, "pBuffer must not be null");
#ifdef DILIGENT_DEBUG
    {
        auto MappedBufferIt = m_DbgMappedBuffers.find(pBuffer);
        VERIFY(MappedBufferIt != m_DbgMappedBuffers.end(), "Buffer '", pBuffer->GetDesc().Name, "' has not been mapped.");
        VERIFY(MappedBufferIt->second.MapType == MapType, "MapType (", MapType, ") does not match the map type that was used to map the buffer ", MappedBufferIt->second.MapType);
        m_DbgMappedBuffers.erase(MappedBufferIt);
    }
#endif
}


template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::UpdateTexture(
    ITexture*                      pTexture,
    Uint32                         MipLevel,
    Uint32                         Slice,
    const Box&                     DstBox,
    const TextureSubResData&       SubresData,
    RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
    RESOURCE_STATE_TRANSITION_MODE TextureTransitionMode)
{
    DEV_CHECK_ERR(IsTransferCtx(), "UpdateTexture is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(pTexture != nullptr, "pTexture must not be null");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "UpdateTexture command must be used outside of render pass.");

    ValidateUpdateTextureParams(pTexture->GetDesc(), MipLevel, Slice, DstBox, SubresData);
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::CopyTexture(const CopyTextureAttribs& CopyAttribs)
{
    DEV_CHECK_ERR(IsTransferCtx(), "CopyTexture is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(CopyAttribs.pSrcTexture, "Src texture must not be null");
    DEV_CHECK_ERR(CopyAttribs.pDstTexture, "Dst texture must not be null");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "CopyTexture command must be used outside of render pass.");

    ValidateCopyTextureParams(CopyAttribs);
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::MapTextureSubresource(
    ITexture*                 pTexture,
    Uint32                    MipLevel,
    Uint32                    ArraySlice,
    MAP_TYPE                  MapType,
    MAP_FLAGS                 MapFlags,
    const Box*                pMapRegion,
    MappedTextureSubresource& MappedData)
{
    DEV_CHECK_ERR(pTexture, "pTexture must not be null");
    ValidateMapTextureParams(pTexture->GetDesc(), MipLevel, ArraySlice, MapType, MapFlags, pMapRegion);
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::UnmapTextureSubresource(
    ITexture* pTexture,
    Uint32    MipLevel,
    Uint32    ArraySlice)
{
    DEV_CHECK_ERR(pTexture, "pTexture must not be null");
    DEV_CHECK_ERR(MipLevel < pTexture->GetDesc().MipLevels, "Mip level is out of range");
    DEV_CHECK_ERR(ArraySlice < pTexture->GetDesc().ArraySize, "Array slice is out of range");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::GenerateMips(ITextureView* pTexView)
{
    DEV_CHECK_ERR(IsGraphicsCtx(), "GenerateMips is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(pTexView != nullptr, "pTexView must not be null");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "GenerateMips command must be used outside of render pass.");
#ifdef DILIGENT_DEVELOPMENT
    {
        const auto& ViewDesc = pTexView->GetDesc();
        DEV_CHECK_ERR(ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE, "Shader resource view '", ViewDesc.Name,
                      "' can't be used to generate mipmaps because its type is ", GetTexViewTypeLiteralName(ViewDesc.ViewType), ". Required view type: TEXTURE_VIEW_SHADER_RESOURCE.");
        DEV_CHECK_ERR((ViewDesc.Flags & TEXTURE_VIEW_FLAG_ALLOW_MIP_MAP_GENERATION) != 0, "Shader resource view '", ViewDesc.Name,
                      "' was not created with TEXTURE_VIEW_FLAG_ALLOW_MIP_MAP_GENERATION flag and can't be used to generate mipmaps.");
    }
#endif
}


template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::ResolveTextureSubresource(
    ITexture*                               pSrcTexture,
    ITexture*                               pDstTexture,
    const ResolveTextureSubresourceAttribs& ResolveAttribs)
{
#ifdef DILIGENT_DEVELOPMENT
    DEV_CHECK_ERR(IsGraphicsCtx(), "ResolveTextureSubresource is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "ResolveTextureSubresource command must be used outside of render pass.");

    DEV_CHECK_ERR(pSrcTexture != nullptr && pDstTexture != nullptr, "Src and Dst textures must not be null");
    const auto& SrcTexDesc = pSrcTexture->GetDesc();
    const auto& DstTexDesc = pDstTexture->GetDesc();

    VerifyResolveTextureSubresourceAttribs(ResolveAttribs, SrcTexDesc, DstTexDesc);
#endif
}


template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::BuildBLAS(const BuildBLASAttribs& Attribs, int) const
{
    DEV_CHECK_ERR(IsComputeCtx(), "BuildBLAS is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.RayTracing, "IDeviceContext::BuildBLAS: ray tracing is not supported by this device");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "IDeviceContext::BuildBLAS command must be performed outside of render pass");
    DEV_CHECK_ERR(VerifyBuildBLASAttribs(Attribs), "BuildBLASAttribs are invalid");
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::BuildTLAS(const BuildTLASAttribs& Attribs, int) const
{
    DEV_CHECK_ERR(IsComputeCtx(), "BuildTLAS is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.RayTracing, "IDeviceContext::BuildTLAS: ray tracing is not supported by this device");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "IDeviceContext::BuildTLAS command must be performed outside of render pass");
    DEV_CHECK_ERR(VerifyBuildTLASAttribs(Attribs), "BuildTLASAttribs are invalid");
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::CopyBLAS(const CopyBLASAttribs& Attribs, int) const
{
    DEV_CHECK_ERR(IsComputeCtx(), "CopyBLAS is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.RayTracing, "IDeviceContext::CopyBLAS: ray tracing is not supported by this device");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "IDeviceContext::CopyBLAS command must be performed outside of render pass");
    DEV_CHECK_ERR(VerifyCopyBLASAttribs(m_pDevice, Attribs), "CopyBLASAttribs are invalid");
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::CopyTLAS(const CopyTLASAttribs& Attribs, int) const
{
    DEV_CHECK_ERR(IsComputeCtx(), "CopyTLAS is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.RayTracing, "IDeviceContext::CopyTLAS: ray tracing is not supported by this device");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "IDeviceContext::CopyTLAS command must be performed outside of render pass");
    DEV_CHECK_ERR(VerifyCopyTLASAttribs(Attribs), "CopyTLASAttribs are invalid");
    DEV_CHECK_ERR(ValidatedCast<TopLevelASType>(Attribs.pSrc)->ValidateContent(), "IDeviceContext::CopyTLAS: pSrc acceleration structure is not valid");
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::WriteBLASCompactedSize(const WriteBLASCompactedSizeAttribs& Attribs, int) const
{
    DEV_CHECK_ERR(IsComputeCtx(), "WriteBLASCompactedSize is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.RayTracing, "IDeviceContext::WriteBLASCompactedSize: ray tracing is not supported by this device");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "IDeviceContext::WriteBLASCompactedSize: command must be performed outside of render pass");
    DEV_CHECK_ERR(VerifyWriteBLASCompactedSizeAttribs(m_pDevice, Attribs), "WriteBLASCompactedSizeAttribs are invalid");
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::WriteTLASCompactedSize(const WriteTLASCompactedSizeAttribs& Attribs, int) const
{
    DEV_CHECK_ERR(IsComputeCtx(), "WriteTLASCompactedSize is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");
    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.RayTracing, "IDeviceContext::WriteTLASCompactedSize: ray tracing is not supported by this device");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "IDeviceContext::WriteTLASCompactedSize: command must be performed outside of render pass");
    DEV_CHECK_ERR(VerifyWriteTLASCompactedSizeAttribs(m_pDevice, Attribs), "WriteTLASCompactedSizeAttribs are invalid");
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::TraceRays(const TraceRaysAttribs& Attribs, int) const
{
    DEV_CHECK_ERR(IsComputeCtx(), "TraceRays is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.RayTracing,
                  "IDeviceContext::TraceRays: ray tracing is not supported by this device");
    DEV_CHECK_ERR(m_pPipelineState,
                  "IDeviceContext::TraceRays command arguments are invalid: no pipeline state is bound.");
    DEV_CHECK_ERR(m_pPipelineState->GetDesc().IsRayTracingPipeline(),
                  "IDeviceContext::TraceRays command arguments are invalid: pipeline state '", m_pPipelineState->GetDesc().Name, "' is not a ray tracing pipeline.");

    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "IDeviceContext::TraceRays must be performed outside of render pass");

    DEV_CHECK_ERR(VerifyTraceRaysAttribs(Attribs), "TraceRaysAttribs are invalid");

    DEV_CHECK_ERR(PipelineStateImplType::IsSameObject(m_pPipelineState, ValidatedCast<PipelineStateImplType>(Attribs.pSBT->GetDesc().pPSO)),
                  "IDeviceContext::TraceRays command arguments are invalid: currently bound pipeline '", m_pPipelineState->GetDesc().Name,
                  "' doesn't match the pipeline '", Attribs.pSBT->GetDesc().pPSO->GetDesc().Name, "' that was used in ShaderBindingTable");

    const auto* pSBTImpl = ValidatedCast<const ShaderBindingTableImplType>(Attribs.pSBT);
    DEV_CHECK_ERR(!pSBTImpl->HasPendingData(), "IDeviceContext::TraceRaysIndirect command arguments are invalid: SBT '",
                  pSBTImpl->GetDesc().Name, "' has uncommitted changes, call UpdateSBT() first");

    VERIFY(pSBTImpl->GetInternalBuffer() != nullptr,
           "SBT '", pSBTImpl->GetDesc().Name, "' internal buffer must not be null, this should never happen, ",
           "because HasPendingData() must've returned true triggering the assert above.");
    VERIFY(pSBTImpl->GetInternalBuffer()->CheckState(RESOURCE_STATE_RAY_TRACING),
           "SBT '", pSBTImpl->GetDesc().Name, "' internal buffer is expected to be in RESOURCE_STATE_RAY_TRACING, but current state is ",
           GetResourceStateString(pSBTImpl->GetInternalBuffer()->GetState()));

    DEV_CHECK_ERR((Attribs.DimensionX * Attribs.DimensionY * Attribs.DimensionZ) <= m_pDevice->GetProperties().MaxRayGenThreads,
                  "IDeviceContext::TraceRays command arguments are invalid: the dimension must not exceed the ",
                  m_pDevice->GetProperties().MaxRayGenThreads, " threads");
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::TraceRaysIndirect(const TraceRaysIndirectAttribs& Attribs, IBuffer* pAttribsBuffer, int) const
{
    DEV_CHECK_ERR(IsComputeCtx(), "TraceRaysIndirect is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.RayTracing2,
                  "IDeviceContext::TraceRaysIndirect: indirect trace rays is not supported by this device");
    DEV_CHECK_ERR(m_pPipelineState,
                  "IDeviceContext::TraceRaysIndirect command arguments are invalid: no pipeline state is bound.");
    DEV_CHECK_ERR(m_pPipelineState->GetDesc().IsRayTracingPipeline(),
                  "IDeviceContext::TraceRaysIndirect command arguments are invalid: pipeline state '", m_pPipelineState->GetDesc().Name,
                  "' is not a ray tracing pipeline.");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr,
                  "IDeviceContext::TraceRaysIndirect must be performed outside of render pass");

    DEV_CHECK_ERR(VerifyTraceRaysIndirectAttribs(m_pDevice, Attribs, pAttribsBuffer, TraceRaysIndirectCommandSize),
                  "TraceRaysIndirectAttribs are invalid");

    DEV_CHECK_ERR(PipelineStateImplType::IsSameObject(m_pPipelineState, ValidatedCast<PipelineStateImplType>(Attribs.pSBT->GetDesc().pPSO)),
                  "IDeviceContext::TraceRaysIndirect command arguments are invalid: currently bound pipeline '", m_pPipelineState->GetDesc().Name,
                  "' doesn't match the pipeline '", Attribs.pSBT->GetDesc().pPSO->GetDesc().Name, "' that was used in ShaderBindingTable");

    const auto* pSBTImpl = ValidatedCast<const ShaderBindingTableImplType>(Attribs.pSBT);
    DEV_CHECK_ERR(!pSBTImpl->HasPendingData(),
                  "IDeviceContext::TraceRaysIndirect command arguments are invalid: SBT '",
                  pSBTImpl->GetDesc().Name, "' has uncommitted changes, call UpdateSBT() first");


    VERIFY(pSBTImpl->GetInternalBuffer() != nullptr,
           "SBT '", pSBTImpl->GetDesc().Name, "' internal buffer must not be null, this should never happen, ",
           "because HasPendingData() must've returned true triggering the assert above.");
    VERIFY(pSBTImpl->GetInternalBuffer()->CheckState(RESOURCE_STATE_RAY_TRACING),
           "SBT '", pSBTImpl->GetDesc().Name, "' internal buffer is expected to be in RESOURCE_STATE_RAY_TRACING, but current state is ",
           GetResourceStateString(pSBTImpl->GetInternalBuffer()->GetState()));
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::UpdateSBT(IShaderBindingTable* pSBT, const UpdateIndirectRTBufferAttribs* pUpdateIndirectBufferAttribs, int) const
{
    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.RayTracing, "IDeviceContext::UpdateSBT: ray tracing is not supported by this device");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "IDeviceContext::UpdateSBT must be performed outside of render pass");
    DEV_CHECK_ERR(pSBT != nullptr, "IDeviceContext::UpdateSBT command arguments are invalid: pSBT must not be null");

    if (pUpdateIndirectBufferAttribs != nullptr)
    {
        DEV_CHECK_ERR(pUpdateIndirectBufferAttribs->pAttribsBuffer != nullptr,
                      "IDeviceContext::UpdateSBT command arguments are invalid: pUpdateIndirectBufferAttribs->pAttribsBuffer must not be null");
    }
}


template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::PrepareCommittedResources(CommittedShaderResources& Resources, Uint32& DvpCompatibleSRBCount)
{
    const auto SignCount = m_pPipelineState->GetResourceSignatureCount();

    Resources.ActiveSRBMask = 0;
    for (Uint32 i = 0; i < SignCount; ++i)
    {
        const auto* pSignature = m_pPipelineState->GetResourceSignature(i);
        if (pSignature == nullptr || pSignature->GetTotalResourceCount() == 0)
            continue;

        Resources.ActiveSRBMask |= 1u << i;
    }

    DvpCompatibleSRBCount = 0;

#ifdef DILIGENT_DEVELOPMENT
    // Layout compatibility means that descriptor sets can be bound to a command buffer
    // for use by any pipeline created with a compatible pipeline layout, and without having bound
    // a particular pipeline first. It also means that descriptor sets can remain valid across
    // a pipeline change, and the same resources will be accessible to the newly bound pipeline.
    // (14.2.2. Pipeline Layouts, clause 'Pipeline Layout Compatibility')
    // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#descriptorsets-compatibility

    // Find the number of SRBs compatible with signatures in the current pipeline
    for (; DvpCompatibleSRBCount < SignCount; ++DvpCompatibleSRBCount)
    {
        const auto pSRB = Resources.SRBs[DvpCompatibleSRBCount].Lock();

        const auto* pPSOSign = m_pPipelineState->GetResourceSignature(DvpCompatibleSRBCount);
        const auto* pSRBSign = pSRB ? pSRB->GetSignature() : nullptr;

        if ((pPSOSign == nullptr || pPSOSign->GetTotalResourceCount() == 0) !=
            (pSRBSign == nullptr || pSRBSign->GetTotalResourceCount() == 0))
        {
            // One signature is null or empty while the other is not - SRB is not compatible with the PSO.
            break;
        }

        if (pPSOSign != nullptr && pSRBSign != nullptr && pPSOSign->IsIncompatibleWith(*pSRBSign))
        {
            // Signatures are incompatible
            break;
        }
    }

    // Unbind incompatible shader resources
    // A consequence of layout compatibility is that when the implementation compiles a pipeline
    // layout and maps pipeline resources to implementation resources, the mechanism for set N
    // should only be a function of sets [0..N].
    for (Uint32 sign = DvpCompatibleSRBCount; sign < SignCount; ++sign)
    {
        Resources.Set(sign, nullptr);
    }

    Resources.ResourcesValidated = false;
#endif
}

#ifdef DILIGENT_DEVELOPMENT

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyDrawArguments(const DrawAttribs& Attribs) const
{
    if ((Attribs.Flags & DRAW_FLAG_VERIFY_DRAW_ATTRIBS) == 0)
        return;

    DEV_CHECK_ERR(IsGraphicsCtx(), "Draw is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(m_pPipelineState, "Draw command arguments are invalid: no pipeline state is bound.");

    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_GRAPHICS,
                  "Draw command arguments are invalid: pipeline state '", m_pPipelineState->GetDesc().Name, "' is not a graphics pipeline.");

    DEV_CHECK_ERR(VerifyDrawAttribs(Attribs), "DrawAttribs are invalid");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyDrawIndexedArguments(const DrawIndexedAttribs& Attribs) const
{
    if ((Attribs.Flags & DRAW_FLAG_VERIFY_DRAW_ATTRIBS) == 0)
        return;

    DEV_CHECK_ERR(IsGraphicsCtx(), "DrawIndexed is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(m_pPipelineState, "DrawIndexed command arguments are invalid: no pipeline state is bound.");

    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_GRAPHICS,
                  "DrawIndexed command arguments are invalid: pipeline state '",
                  m_pPipelineState->GetDesc().Name, "' is not a graphics pipeline.");

    DEV_CHECK_ERR(m_pIndexBuffer, "DrawIndexed command arguments are invalid: no index buffer is bound.");

    DEV_CHECK_ERR(VerifyDrawIndexedAttribs(Attribs), "DrawIndexedAttribs are invalid");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyDrawMeshArguments(const DrawMeshAttribs& Attribs) const
{
    if ((Attribs.Flags & DRAW_FLAG_VERIFY_DRAW_ATTRIBS) == 0)
        return;

    DEV_CHECK_ERR(IsGraphicsCtx(), "DrawMesh is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.MeshShaders, "DrawMesh: mesh shaders are not supported by this device");

    DEV_CHECK_ERR(m_pPipelineState, "DrawMesh command arguments are invalid: no pipeline state is bound.");

    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_MESH,
                  "DrawMesh command arguments are invalid: pipeline state '",
                  m_pPipelineState->GetDesc().Name, "' is not a mesh pipeline.");

    DEV_CHECK_ERR(VerifyDrawMeshAttribs(m_pDevice->GetProperties().MaxDrawMeshTasksCount, Attribs), "DrawMeshAttribs are invalid");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyDrawIndirectArguments(
    const DrawIndirectAttribs& Attribs,
    const IBuffer*             pAttribsBuffer) const
{
    if ((Attribs.Flags & DRAW_FLAG_VERIFY_DRAW_ATTRIBS) == 0)
        return;

    DEV_CHECK_ERR(IsGraphicsCtx(), "DrawIndirect is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(m_pPipelineState, "DrawIndirect command arguments are invalid: no pipeline state is bound.");

    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_GRAPHICS,
                  "DrawIndirect command arguments are invalid: pipeline state '",
                  m_pPipelineState->GetDesc().Name, "' is not a graphics pipeline.");

    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr || Attribs.IndirectAttribsBufferStateTransitionMode != RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                  "Resource state transitions are not allowed inside a render pass and may result in an undefined behavior. "
                  "Do not use RESOURCE_STATE_TRANSITION_MODE_TRANSITION or end the render pass first.");

    DEV_CHECK_ERR(VerifyDrawIndirectAttribs(Attribs, pAttribsBuffer), "DrawIndirectAttribs are invalid");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyDrawIndexedIndirectArguments(
    const DrawIndexedIndirectAttribs& Attribs,
    const IBuffer*                    pAttribsBuffer) const
{
    if ((Attribs.Flags & DRAW_FLAG_VERIFY_DRAW_ATTRIBS) == 0)
        return;

    DEV_CHECK_ERR(IsGraphicsCtx(), "DrawIndexedIndirect is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(m_pPipelineState, "DrawIndexedIndirect command arguments are invalid: no pipeline state is bound.");

    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_GRAPHICS,
                  "DrawIndexedIndirect command arguments are invalid: pipeline state '",
                  m_pPipelineState->GetDesc().Name, "' is not a graphics pipeline.");

    DEV_CHECK_ERR(m_pIndexBuffer, "DrawIndexedIndirect command arguments are invalid: no index buffer is bound.");

    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr || Attribs.IndirectAttribsBufferStateTransitionMode != RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                  "Resource state transitions are not allowed inside a render pass and may result in an undefined behavior. "
                  "Do not use RESOURCE_STATE_TRANSITION_MODE_TRANSITION or end the render pass first.");

    DEV_CHECK_ERR(VerifyDrawIndexedIndirectAttribs(Attribs, pAttribsBuffer), "DrawIndexedIndirectAttribs are invalid");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyDrawMeshIndirectArguments(
    const DrawMeshIndirectAttribs& Attribs,
    const IBuffer*                 pAttribsBuffer) const
{
    if ((Attribs.Flags & DRAW_FLAG_VERIFY_DRAW_ATTRIBS) == 0)
        return;

    DEV_CHECK_ERR(IsGraphicsCtx(), "DrawMeshIndirect is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.MeshShaders, "DrawMeshIndirect: mesh shaders are not supported by this device");

    DEV_CHECK_ERR(m_pPipelineState, "DrawMeshIndirect command arguments are invalid: no pipeline state is bound.");

    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_MESH,
                  "DrawMeshIndirect command arguments are invalid: pipeline state '",
                  m_pPipelineState->GetDesc().Name, "' is not a mesh pipeline.");

    DEV_CHECK_ERR(VerifyDrawMeshIndirectAttribs(Attribs, pAttribsBuffer), "DrawMeshIndirectAttribs are invalid");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyDrawMeshIndirectCountArguments(
    const DrawMeshIndirectCountAttribs& Attribs,
    const IBuffer*                      pAttribsBuffer,
    const IBuffer*                      pCountBuff) const
{
    if ((Attribs.Flags & DRAW_FLAG_VERIFY_DRAW_ATTRIBS) == 0)
        return;

    DEV_CHECK_ERR(IsGraphicsCtx(), "DrawMeshIndirectCount is not supported in ", GetContextTypeString(m_Desc.ContextType), " context");

    DEV_CHECK_ERR(m_pDevice->GetDeviceCaps().Features.MeshShaders, "DrawMeshIndirectCount: mesh shaders are not supported by this device");

    DEV_CHECK_ERR(m_pPipelineState, "DrawMeshIndirectCount command arguments are invalid: no pipeline state is bound.");

    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_MESH,
                  "DrawMeshIndirectCount command arguments are invalid: pipeline state '",
                  m_pPipelineState->GetDesc().Name, "' is not a mesh pipeline.");

    DEV_CHECK_ERR(VerifyDrawMeshIndirectCountAttribs(Attribs, pAttribsBuffer, pCountBuff, DrawMeshIndirectCommandStride),
                  "DrawMeshIndirectCountAttribs are invalid");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyRenderTargets() const
{
    DEV_CHECK_ERR(m_pPipelineState, "No pipeline state is bound");

    const auto& PSODesc = m_pPipelineState->GetDesc();
    DEV_CHECK_ERR(PSODesc.IsAnyGraphicsPipeline(),
                  "Pipeline state '", PSODesc.Name, "' is not a graphics pipeline");

    TEXTURE_FORMAT BoundRTVFormats[8] = {TEX_FORMAT_UNKNOWN};
    TEXTURE_FORMAT BoundDSVFormat     = TEX_FORMAT_UNKNOWN;

    for (Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt)
    {
        if (auto* pRT = m_pBoundRenderTargets[rt].RawPtr())
            BoundRTVFormats[rt] = pRT->GetDesc().Format;
        else
            BoundRTVFormats[rt] = TEX_FORMAT_UNKNOWN;
    }

    BoundDSVFormat = m_pBoundDepthStencil ? m_pBoundDepthStencil->GetDesc().Format : TEX_FORMAT_UNKNOWN;

    const auto& GraphicsPipeline = m_pPipelineState->GetGraphicsPipelineDesc();
    if (GraphicsPipeline.NumRenderTargets != m_NumBoundRenderTargets)
    {
        LOG_WARNING_MESSAGE("The number of currently bound render targets (", m_NumBoundRenderTargets,
                            ") does not match the number of outputs specified by the PSO '", PSODesc.Name,
                            "' (", Uint32{GraphicsPipeline.NumRenderTargets}, ").");
    }

    if (BoundDSVFormat != GraphicsPipeline.DSVFormat)
    {
        LOG_WARNING_MESSAGE("Currently bound depth-stencil buffer format (", GetTextureFormatAttribs(BoundDSVFormat).Name,
                            ") does not match the DSV format specified by the PSO '", PSODesc.Name,
                            "' (", GetTextureFormatAttribs(GraphicsPipeline.DSVFormat).Name, ").");
    }

    for (Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt)
    {
        auto BoundFmt = BoundRTVFormats[rt];
        auto PSOFmt   = GraphicsPipeline.RTVFormats[rt];
        if (BoundFmt != PSOFmt)
        {
            LOG_WARNING_MESSAGE("Render target bound to slot ", rt, " (", GetTextureFormatAttribs(BoundFmt).Name,
                                ") does not match the RTV format specified by the PSO '", PSODesc.Name,
                                "' (", GetTextureFormatAttribs(PSOFmt).Name, ").");
        }
    }
}



template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyDispatchArguments(const DispatchComputeAttribs& Attribs) const
{
    DEV_CHECK_ERR(m_pPipelineState, "DispatchCompute command arguments are invalid: no pipeline state is bound.");

    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_COMPUTE,
                  "DispatchCompute command arguments are invalid: pipeline state '", m_pPipelineState->GetDesc().Name,
                  "' is not a compute pipeline.");

    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr,
                  "DispatchCompute command must be performed outside of render pass");

    DEV_CHECK_ERR(VerifyDispatchComputeAttribs(Attribs), "DispatchComputeAttribs attribs");
}

template <typename ImplementationTraits>
inline void DeviceContextBase<ImplementationTraits>::DvpVerifyDispatchIndirectArguments(
    const DispatchComputeIndirectAttribs& Attribs,
    const IBuffer*                        pAttribsBuffer) const
{
    DEV_CHECK_ERR(m_pPipelineState, "DispatchComputeIndirect command arguments are invalid: no pipeline state is bound.");

    DEV_CHECK_ERR(m_pPipelineState->GetDesc().PipelineType == PIPELINE_TYPE_COMPUTE,
                  "DispatchComputeIndirect command arguments are invalid: pipeline state '",
                  m_pPipelineState->GetDesc().Name, "' is not a compute pipeline.");

    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "DispatchComputeIndirect command must be performed outside of render pass");

    DEV_CHECK_ERR(VerifyDispatchComputeIndirectAttribs(Attribs, pAttribsBuffer), "DispatchComputeIndirectAttribs are invalid");
}


template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::DvpVerifyStateTransitionDesc(const StateTransitionDesc& Barrier) const
{
    DEV_CHECK_ERR(VerifyStateTransitionDesc(m_pDevice, Barrier), "StateTransitionDesc are invalid");
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::DvpVerifyTextureState(
    const TextureImplType& Texture,
    RESOURCE_STATE         RequiredState,
    const char*            OperationName) const
{
    if (Texture.IsInKnownState() && !Texture.CheckState(RequiredState))
    {
        LOG_ERROR_MESSAGE(OperationName, " requires texture '", Texture.GetDesc().Name, "' to be transitioned to ", GetResourceStateString(RequiredState),
                          " state. Actual texture state: ", GetResourceStateString(Texture.GetState()),
                          ". Use appropriate state transition flags or explicitly transition the texture using IDeviceContext::TransitionResourceStates() method.");
    }
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::DvpVerifyBufferState(
    const BufferImplType& Buffer,
    RESOURCE_STATE        RequiredState,
    const char*           OperationName) const
{
    if (Buffer.IsInKnownState() && !Buffer.CheckState(RequiredState))
    {
        LOG_ERROR_MESSAGE(OperationName, " requires buffer '", Buffer.GetDesc().Name, "' to be transitioned to ", GetResourceStateString(RequiredState),
                          " state. Actual buffer state: ", GetResourceStateString(Buffer.GetState()),
                          ". Use appropriate state transition flags or explicitly transition the buffer using IDeviceContext::TransitionResourceStates() method.");
    }
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::DvpVerifyBLASState(
    const BottomLevelASType& BLAS,
    RESOURCE_STATE           RequiredState,
    const char*              OperationName) const
{
    if (BLAS.IsInKnownState() && !BLAS.CheckState(RequiredState))
    {
        LOG_ERROR_MESSAGE(OperationName, " requires BLAS '", BLAS.GetDesc().Name, "' to be transitioned to ", GetResourceStateString(RequiredState),
                          " state. Actual BLAS state: ", GetResourceStateString(BLAS.GetState()),
                          ". Use appropriate state transition flags or explicitly transition the BLAS using IDeviceContext::TransitionResourceStates() method.");
    }
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::DvpVerifyTLASState(
    const TopLevelASType& TLAS,
    RESOURCE_STATE        RequiredState,
    const char*           OperationName) const
{
    if (TLAS.IsInKnownState() && !TLAS.CheckState(RequiredState))
    {
        LOG_ERROR_MESSAGE(OperationName, " requires TLAS '", TLAS.GetDesc().Name, "' to be transitioned to ", GetResourceStateString(RequiredState),
                          " state. Actual TLAS state: ", GetResourceStateString(TLAS.GetState()),
                          ". Use appropriate state transition flags or explicitly transition the TLAS using IDeviceContext::TransitionResourceStates() method.");
    }
}

template <typename ImplementationTraits>
void DeviceContextBase<ImplementationTraits>::DvpVerifySRBCompatibility(
    CommittedShaderResources&                                 Resources,
    std::function<PipelineResourceSignatureImplType*(Uint32)> CustomGetSignature) const
{
    DEV_CHECK_ERR(m_pPipelineState, "No PSO is bound in the context");

    const auto SignCount = m_pPipelineState->GetResourceSignatureCount();
    for (Uint32 sign = 0; sign < SignCount; ++sign)
    {
        const auto* const pPSOSign = CustomGetSignature ? CustomGetSignature(sign) : m_pPipelineState->GetResourceSignature(sign);
        if (pPSOSign == nullptr || pPSOSign->GetTotalResourceCount() == 0)
            continue; // Skip null and empty signatures

        VERIFY_EXPR(sign < MAX_RESOURCE_SIGNATURES);
        VERIFY_EXPR(pPSOSign->GetDesc().BindingIndex == sign);

        const auto  pSRB   = Resources.SRBs[sign].Lock();
        const auto* pCache = Resources.ResourceCaches[sign];
        if (pCache != nullptr)
        {
            DEV_CHECK_ERR(pSRB, "Shader resource cache pointer at index ", sign,
                          " is non-null, but the corresponding SRB is null. This indicates that the SRB has been released while still "
                          "being used by the context commands. This usage is invalid. A resource must be released only after "
                          "the last command that uses it.");
        }
        else
        {
            VERIFY(!pSRB, "Shader resource cache pointer is null, but SRB is not null. This is unexpected and is likely a bug.");
        }

        DEV_CHECK_ERR(pSRB, "Pipeline state '", m_pPipelineState->GetDesc().Name, "' requires SRB at index ", sign,
                      ", but none is bound in the device context. Did you call CommitShaderResources()?");

        VERIFY_EXPR(pCache == &pSRB->GetResourceCache());

        const auto* const pSRBSign = pSRB->GetSignature();
        DEV_CHECK_ERR(pPSOSign->IsCompatibleWith(pSRBSign), "Shader resource binding at index ", sign, " with signature '",
                      pSRBSign->GetDesc().Name, "' is not compatible with the signature in PSO '",
                      m_pPipelineState->GetDesc().Name, "'.");
    }
}

#endif // DILIGENT_DEVELOPMENT

} // namespace Diligent
