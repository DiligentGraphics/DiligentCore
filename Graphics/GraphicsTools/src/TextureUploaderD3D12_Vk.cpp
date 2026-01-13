/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#include <mutex>
#include <unordered_map>
#include <deque>
#include <vector>

#include "TextureUploaderD3D12_Vk.hpp"
#include "ThreadSignal.hpp"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

namespace
{

class UploadTexture : public UploadBufferBase
{
public:
    UploadTexture(IReferenceCounters*     pRefCounters,
                  const UploadBufferDesc& Desc,
                  ITexture*               pStagingTexture) :
        // clang-format off
        UploadBufferBase {pRefCounters, Desc, /*AllocateStagingData = */pStagingTexture == nullptr},
        m_pStagingTexture{pStagingTexture}
    // clang-format on
    {
    }

    ~UploadTexture()
    {
        if (m_pStagingTexture)
        {
            for (Uint32 Slice = 0; Slice < m_Desc.ArraySize; ++Slice)
            {
                for (Uint32 Mip = 0; Mip < m_Desc.MipLevels; ++Mip)
                {
                    DEV_CHECK_ERR(!IsMapped(Mip, Slice), "Releasing mapped staging texture");
                }
            }
        }
    }

    void WaitForMap()
    {
        if (!HasStagingData())
        {
            m_TextureMappedSignal.Wait();
        }
    }

    void SignalMapped()
    {
        m_TextureMappedSignal.Trigger();
    }

    void SignalCopyScheduled(Uint64 FenceValue)
    {
        m_CopyScheduledFenceValue = FenceValue;
        m_CopyScheduledSignal.Trigger();
    }

    void Unmap(IDeviceContext* pDeviceContext, Uint32 Mip, Uint32 Slice)
    {
        VERIFY(IsMapped(Mip, Slice), "This subresource is not mapped");
        pDeviceContext->UnmapTextureSubresource(m_pStagingTexture, Mip, Slice);
        SetMappedData(Mip, Slice, MappedTextureSubresource{});
    }

    void Map(IDeviceContext* pDeviceContext, Uint32 Mip, Uint32 Slice)
    {
        VERIFY(!IsMapped(Mip, Slice), "This subresource is already mapped");
        MappedTextureSubresource MappedData;
        pDeviceContext->MapTextureSubresource(m_pStagingTexture, Mip, Slice, MAP_WRITE, MAP_FLAG_NO_OVERWRITE, nullptr, MappedData);
        SetMappedData(Mip, Slice, MappedData);
    }

    void Reset()
    {
        m_CopyScheduledSignal.Reset();
        m_TextureMappedSignal.Reset();
        m_CopyScheduledFenceValue = 0;
        UploadBufferBase::Reset();
    }

    virtual void WaitForCopyScheduled() override final
    {
        m_CopyScheduledSignal.Wait();
    }

    ITexture* GetStagingTexture() { return m_pStagingTexture; }

    bool DbgIsCopyScheduled() const
    {
        return m_CopyScheduledSignal.IsTriggered();
    }

    bool DbgIsMapped()
    {
        return m_TextureMappedSignal.IsTriggered();
    }

    Uint64 GetCopyScheduledFenceValue() const
    {
        VERIFY(m_CopyScheduledFenceValue != 0, "Fence value has not been initialized");
        return m_CopyScheduledFenceValue;
    }

private:
    Threading::Signal m_CopyScheduledSignal;
    Threading::Signal m_TextureMappedSignal;

    RefCntAutoPtr<ITexture> m_pStagingTexture;
    Uint64                  m_CopyScheduledFenceValue = 0;
};

} // namespace


struct TextureUploaderD3D12_Vk::InternalData
{
    using PendingBufferOperation = TextureUploaderBase::PendingOperation<UploadTexture>;

    InternalData(IRenderDevice* pDevice)
    {
        FenceDesc fenceDesc;
        fenceDesc.Name = "Texture uploader sync fence";
        pDevice->CreateFence(fenceDesc, &m_pFence);
    }

    ~InternalData()
    {
        for (auto it : m_UploadTexturesCache)
        {
            if (it.second.size())
            {
                const UploadBufferDesc&     desc    = it.first;
                const TextureFormatAttribs& FmtInfo = GetTextureFormatAttribs(desc.Format);
                LOG_INFO_MESSAGE("TextureUploaderD3D12_Vk: releasing ", it.second.size(), ' ',
                                 desc.Width, 'x', desc.Height, 'x', desc.Depth, ' ', FmtInfo.Name,
                                 " upload buffer(s)", (it.second.size() == 1 ? "" : "s"));
            }
        }
    }

    std::vector<PendingBufferOperation>& SwapMapQueues()
    {
        std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
        m_PendingOperations.swap(m_InWorkOperations);
        return m_InWorkOperations;
    }

    void EnqueueCopy(UploadTexture* pUploadBuffer, ITexture* pDstTex, Uint32 dstSlice, Uint32 dstMip, bool AutoRecycle)
    {
        std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
        m_PendingOperations.emplace_back(PendingBufferOperation::Type::Copy, pUploadBuffer, pDstTex, dstSlice, dstMip, AutoRecycle);
    }

    void EnqueueMap(UploadTexture* pUploadBuffer)
    {
        std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
        m_PendingOperations.emplace_back(PendingBufferOperation::Type::Map, pUploadBuffer);
    }

    Uint64 SignalFence(IDeviceContext* pContext)
    {
        // Fences can't be accessed from multiple threads simultaneously even
        // when protected by mutex
        Uint64 FenceValue = m_NextFenceValue++;
        pContext->EnqueueSignal(m_pFence, FenceValue);
        return FenceValue;
    }

    void UpdatedCompletedFenceValue()
    {
        // Fences can't be accessed from multiple threads simultaneously even
        // when protected by mutex
        m_CompletedFenceValue = m_pFence->GetCompletedValue();
    }

    RefCntAutoPtr<UploadTexture> FindCachedUploadTexture(const UploadBufferDesc& Desc)
    {
        RefCntAutoPtr<UploadTexture> pUploadTexture;
        std::lock_guard<std::mutex>  CacheLock{m_UploadTexturesCacheMtx};
        auto                         DequeIt = m_UploadTexturesCache.find(Desc);
        if (DequeIt != m_UploadTexturesCache.end())
        {
            auto& Deque = DequeIt->second;
            if (!Deque.empty())
            {
                RefCntAutoPtr<UploadTexture>& FrontBuff = Deque.front();
                if (FrontBuff->GetCopyScheduledFenceValue() <= m_CompletedFenceValue || FrontBuff->HasStagingData())
                {
                    pUploadTexture = std::move(FrontBuff);
                    Deque.pop_front();
                    pUploadTexture->Reset();
                }
            }
        }

        return pUploadTexture;
    }

    void RecycleUploadTexture(UploadTexture* pUploadTexture)
    {
        VERIFY(pUploadTexture->DbgIsCopyScheduled(), "Upload buffer must be recycled only after copy operation has been scheduled on the GPU");

        std::lock_guard<std::mutex> CacheLock(m_UploadTexturesCacheMtx);
        auto&                       Deque = m_UploadTexturesCache[pUploadTexture->GetDesc()];
#ifdef DILIGENT_DEBUG
        VERIFY(std::find(Deque.begin(), Deque.end(), pUploadTexture) == Deque.end(),
               "Upload texture is already in the cache");
#endif
        Deque.emplace_back(pUploadTexture);
    }

    Uint32 GetNumPendingOperations()
    {
        std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
        return static_cast<Uint32>(m_PendingOperations.size());
    }

    void Execute(IDeviceContext* pContext, PendingBufferOperation& Operation);

private:
    std::mutex                          m_PendingOperationsMtx;
    std::vector<PendingBufferOperation> m_PendingOperations;
    std::vector<PendingBufferOperation> m_InWorkOperations;

    std::mutex                                                                     m_UploadTexturesCacheMtx;
    std::unordered_map<UploadBufferDesc, std::deque<RefCntAutoPtr<UploadTexture>>> m_UploadTexturesCache;

    RefCntAutoPtr<IFence> m_pFence;
    Uint64                m_NextFenceValue      = 1;
    Uint64                m_CompletedFenceValue = 0;
};

TextureUploaderD3D12_Vk::TextureUploaderD3D12_Vk(IReferenceCounters* pRefCounters, IRenderDevice* pDevice, const TextureUploaderDesc Desc) :
    TextureUploaderBase{pRefCounters, pDevice, Desc},
    m_pInternalData{new InternalData(pDevice)}
{
}

TextureUploaderD3D12_Vk::~TextureUploaderD3D12_Vk()
{
    Uint32 NumPendingOperations = m_pInternalData->GetNumPendingOperations();
    if (NumPendingOperations != 0)
    {
        LOG_WARNING_MESSAGE("TextureUploaderD3D12_Vk::~TextureUploaderD3D12_Vk(): there ", (NumPendingOperations > 1 ? "are " : "is "),
                            NumPendingOperations, (NumPendingOperations > 1 ? " pending operations" : " pending operation"),
                            " in the queue. If other threads wait for ", (NumPendingOperations > 1 ? "these operations" : "this operation"),
                            ", they may deadlock.");
    }
}

void TextureUploaderD3D12_Vk::RenderThreadUpdate(IDeviceContext* pContext)
{
    auto& InWorkOperations = m_pInternalData->SwapMapQueues();
    if (!InWorkOperations.empty())
    {
        Uint32 NumCopyOperations = 0;
        for (InternalData::PendingBufferOperation& Operation : InWorkOperations)
        {
            m_pInternalData->Execute(pContext, Operation);
            if (Operation.OpType == InternalData::PendingBufferOperation::Type::Copy)
                ++NumCopyOperations;
        }

        if (NumCopyOperations > 0)
        {
            // The buffer may be recycled immediately after the copy scheduled is signaled,
            // so we must signal the fence first.
            Uint64 SignaledFenceValue = m_pInternalData->SignalFence(pContext);

            for (InternalData::PendingBufferOperation& Operation : InWorkOperations)
            {
                if (Operation.OpType == InternalData::PendingBufferOperation::Type::Copy)
                {
                    Operation.pUploadBuffer->SignalCopyScheduled(SignaledFenceValue);
                    if (Operation.AutoRecycle)
                    {
                        m_pInternalData->RecycleUploadTexture(Operation.pUploadBuffer);
                    }
                }
            }
        }

        InWorkOperations.clear();
    }

    // This must be called by the same thread that signals the fence
    m_pInternalData->UpdatedCompletedFenceValue();
}


void TextureUploaderD3D12_Vk::InternalData::Execute(IDeviceContext*         pContext,
                                                    PendingBufferOperation& Operation)
{
    RefCntAutoPtr<UploadTexture>& pUploadTex     = Operation.pUploadBuffer;
    const UploadBufferDesc&       StagingTexDesc = pUploadTex->GetDesc();

    switch (Operation.OpType)
    {
        case InternalData::PendingBufferOperation::Type::Map:
        {
            for (Uint32 Slice = 0; Slice < StagingTexDesc.ArraySize; ++Slice)
            {
                for (Uint32 Mip = 0; Mip < StagingTexDesc.MipLevels; ++Mip)
                {
                    pUploadTex->Map(pContext, Mip, Slice);
                }
            }
            pUploadTex->SignalMapped();
        }
        break;

        case InternalData::PendingBufferOperation::Type::Copy:
        {
            ITexture*          pStagingTex = pUploadTex->GetStagingTexture();
            const TextureDesc& TexDesc     = Operation.pDstTexture->GetDesc();

            VERIFY(pUploadTex->DbgIsMapped(), "Upload texture must be copied only after it has been mapped");
            for (Uint32 Slice = 0; Slice < StagingTexDesc.ArraySize; ++Slice)
            {
                for (Uint32 Mip = 0; Mip < StagingTexDesc.MipLevels; ++Mip)
                {
                    if (pStagingTex != nullptr)
                    {
                        pUploadTex->Unmap(pContext, Mip, Slice);

                        CopyTextureAttribs CopyInfo //
                            {
                                pUploadTex->GetStagingTexture(),
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                Operation.pDstTexture,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION //
                            };
                        CopyInfo.SrcMipLevel = Mip;
                        CopyInfo.SrcSlice    = Slice;
                        CopyInfo.DstMipLevel = Operation.DstMip + Mip;
                        CopyInfo.DstSlice    = Operation.DstSlice + Slice;
                        pContext->CopyTexture(CopyInfo);
                    }
                    else
                    {
                        MipLevelProperties MipLevelProps = GetMipLevelProperties(TexDesc, Operation.DstMip + Mip);
                        Box                DstBox;
                        DstBox.MaxX = MipLevelProps.LogicalWidth;
                        DstBox.MaxY = MipLevelProps.LogicalHeight;

                        const MappedTextureSubresource SrcMappedData = pUploadTex->GetMappedData(Mip, Slice);
                        const TextureSubResData        SubResData{SrcMappedData.pData, SrcMappedData.Stride, SrcMappedData.DepthStride};
                        pContext->UpdateTexture(Operation.pDstTexture, Operation.DstMip + Mip, Operation.DstSlice + Slice, DstBox,
                                                SubResData, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    }
                }
            }
        }
        break;
    }
}

void TextureUploaderD3D12_Vk::AllocateUploadBuffer(IDeviceContext*         pContext,
                                                   const UploadBufferDesc& Desc,
                                                   IUploadBuffer**         ppBuffer)
{
    RefCntAutoPtr<UploadTexture> pUploadTexture = m_pInternalData->FindCachedUploadTexture(Desc);

    // No available buffer found in the cache
    if (!pUploadTexture)
    {
        RefCntAutoPtr<ITexture> pStagingTexture;

        if (m_Desc.Mode == TEXTURE_UPLOADER_MODE_STAGING_RESOURCE)
        {
            TextureDesc StagingTexDesc;
            StagingTexDesc.Type           = Desc.ArraySize == 1 ? RESOURCE_DIM_TEX_2D : RESOURCE_DIM_TEX_2D_ARRAY;
            StagingTexDesc.Width          = Desc.Width;
            StagingTexDesc.Height         = Desc.Height;
            StagingTexDesc.Format         = Desc.Format;
            StagingTexDesc.MipLevels      = Desc.MipLevels;
            StagingTexDesc.ArraySize      = Desc.ArraySize;
            StagingTexDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            StagingTexDesc.Usage          = USAGE_STAGING;

            m_pDevice->CreateTexture(StagingTexDesc, nullptr, &pStagingTexture);
        }

        LOG_INFO_MESSAGE("Created ", Desc.Width, "x", Desc.Height, 'x', Desc.Depth, ' ', Desc.MipLevels, "-mip ",
                         Desc.ArraySize, "-slice ",
                         GetTextureFormatAttribs(Desc.Format).Name, pStagingTexture ? " staging texture" : " CPU upload buffer");

        pUploadTexture = MakeNewRCObj<UploadTexture>()(Desc, pStagingTexture);
    }

    if (m_Desc.Mode == TEXTURE_UPLOADER_MODE_STAGING_RESOURCE)
    {
        if (pContext != nullptr)
        {
            // Render thread
            InternalData::PendingBufferOperation MapOp{InternalData::PendingBufferOperation::Type::Map, pUploadTexture};
            m_pInternalData->Execute(pContext, MapOp);
        }
        else
        {
            // Worker thread
            m_pInternalData->EnqueueMap(pUploadTexture);
            pUploadTexture->WaitForMap();
        }
    }
    else
    {
        pUploadTexture->SignalMapped();
    }

    *ppBuffer = pUploadTexture.Detach();
}

void TextureUploaderD3D12_Vk::ScheduleGPUCopy(IDeviceContext* pContext,
                                              ITexture*       pDstTexture,
                                              Uint32          ArraySlice,
                                              Uint32          MipLevel,
                                              IUploadBuffer*  pUploadBuffer,
                                              bool            AutoRecycle)
{
    UploadTexture* pUploadTexture = ClassPtrCast<UploadTexture>(pUploadBuffer);
    if (pContext != nullptr)
    {
        // Render thread
        InternalData::PendingBufferOperation CopyOp{
            InternalData::PendingBufferOperation::Type::Copy,
            pUploadTexture,
            pDstTexture,
            ArraySlice,
            MipLevel,
            AutoRecycle,
        };
        m_pInternalData->Execute(pContext, CopyOp);

        // The buffer may be recycled immediately after the copy scheduled is signaled,
        // so we must signal the fence first.
        Uint64 SignaledFenceValue = m_pInternalData->SignalFence(pContext);
        pUploadTexture->SignalCopyScheduled(SignaledFenceValue);
        // This must be called by the same thread that signals the fence
        m_pInternalData->UpdatedCompletedFenceValue();

        if (AutoRecycle)
        {
            m_pInternalData->RecycleUploadTexture(pUploadTexture);
        }
    }
    else
    {
        // Worker thread
        m_pInternalData->EnqueueCopy(pUploadTexture, pDstTexture, ArraySlice, MipLevel, AutoRecycle);
    }
}

void TextureUploaderD3D12_Vk::RecycleBuffer(IUploadBuffer* pUploadBuffer)
{
    UploadTexture* pUploadTexture = ClassPtrCast<UploadTexture>(pUploadBuffer);
    m_pInternalData->RecycleUploadTexture(pUploadTexture);
}

TextureUploaderStats TextureUploaderD3D12_Vk::GetStats()
{
    TextureUploaderStats Stats;
    Stats.NumPendingOperations = static_cast<Uint32>(m_pInternalData->GetNumPendingOperations());
    return Stats;
}

} // namespace Diligent
