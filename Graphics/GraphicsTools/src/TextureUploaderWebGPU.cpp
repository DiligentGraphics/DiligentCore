/*
 *  Copyright 2024 Diligent Graphics LLC
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
#include <deque>
#include <unordered_map>

#if !PLATFORM_EMSCRIPTEN
#    include <dawn/dawn_proc_table.h>
#endif

#include <webgpu/webgpu.h>

#include "BufferWebGPU.h"
#include "EngineFactoryWebGPU.h"
#include "TextureUploaderWebGPU.hpp"
#include "GraphicsAccessories.hpp"
#include "ThreadSignal.hpp"
#include "RenderDeviceWebGPU.h"

namespace Diligent
{

namespace
{

struct WebGPUProcessTable
{
    WGPUProcDeviceCreateBuffer   DeviceCreateBuffer;
    WGPUProcBufferGetMappedRange BufferGetMappedRange;
    WGPUProcBufferUnmap          BufferUnmap;
    WGPUProcBufferRelease        BufferRelease;
};

class UploadBufferWebGPU : public UploadBufferBase
{
public:
    UploadBufferWebGPU(IReferenceCounters* pRefCounters, const UploadBufferDesc& Desc, const WebGPUProcessTable& ProcessTable) :
        UploadBufferBase{pRefCounters, Desc},
        m_ProcessTable{ProcessTable},
        m_pStagingBuffer{nullptr},
        m_wgpuStagingBuffer{nullptr},
        m_SubresourceOffsets(size_t{Desc.MipLevels} * size_t{Desc.ArraySize} + 1),
        m_SubresourceStrides(size_t{Desc.MipLevels} * size_t{Desc.ArraySize})
    {
        TextureDesc TexDesc;
        TexDesc.Format = Desc.Format;
        TexDesc.Width  = Desc.Width;
        TexDesc.Height = Desc.Height;
        TexDesc.Depth  = Desc.Depth;
        TexDesc.Type   = Desc.ArraySize == 1 ? RESOURCE_DIM_TEX_2D : RESOURCE_DIM_TEX_2D_ARRAY;

        Uint32 SubRes = 0;
        for (Uint32 Slice = 0; Slice < Desc.ArraySize; ++Slice)
        {
            for (Uint32 Mip = 0; Mip < Desc.MipLevels; ++Mip)
            {
                // Stride must be 256-bytes aligned in WebGPU
                auto MipProps                = GetMipLevelProperties(TexDesc, Mip);
                auto RowStride               = AlignUp(StaticCast<Uint32>(MipProps.RowSize), Uint32{256});
                m_SubresourceStrides[SubRes] = RowStride;

                auto MipSize                             = MipProps.StorageHeight * RowStride;
                m_SubresourceOffsets[size_t{SubRes} + 1] = m_SubresourceOffsets[SubRes] + MipSize;
                ++SubRes;
            }
        }
    }

    ~UploadBufferWebGPU() override
    {
        if (m_wgpuStagingBuffer != nullptr)
            m_ProcessTable.BufferRelease(m_wgpuStagingBuffer);
    }

    // http://en.cppreference.com/w/cpp/thread/condition_variable
    void WaitForMap()
    {
        m_BufferMappedSignal.Wait();
    }

    void SignalMapped()
    {
        m_BufferMappedSignal.Trigger();
    }

    void SignalCopyScheduled()
    {
        m_CopyScheduledSignal.Trigger();
    }

    virtual void WaitForCopyScheduled() override final
    {
        m_CopyScheduledSignal.Wait();
    }

    bool DbgIsCopyScheduled() const
    {
        return m_CopyScheduledSignal.IsTriggered();
    }

    void SetDataPtr(Uint8* pBufferData)
    {
        for (Uint32 Slice = 0; Slice < m_Desc.ArraySize; ++Slice)
        {
            for (Uint32 Mip = 0; Mip < m_Desc.MipLevels; ++Mip)
            {
                SetMappedData(Mip, Slice, MappedTextureSubresource{pBufferData + GetOffset(Mip, Slice), GetStride(Mip, Slice), 0});
            }
        }
    }

    Uint32 GetOffset(Uint32 Mip, Uint32 Slice)
    {
        VERIFY_EXPR(Mip < m_Desc.MipLevels && Slice < m_Desc.ArraySize);
        return m_SubresourceOffsets[size_t{m_Desc.MipLevels} * size_t{Slice} + size_t{Mip}];
    }

    Uint32 GetStride(Uint32 Mip, Uint32 Slice)
    {
        VERIFY_EXPR(Mip < m_Desc.MipLevels && Slice < m_Desc.ArraySize);
        return m_SubresourceStrides[size_t{m_Desc.MipLevels} * size_t{Slice} + size_t{Mip}];
    }

    Uint32 GetTotalSize() const
    {
        return m_SubresourceOffsets.back();
    }

    void Reset()
    {
        m_BufferMappedSignal.Reset();
        m_CopyScheduledSignal.Reset();
        UploadBufferBase::Reset();
    }

private:
    friend TextureUploaderWebGPU;
    const WebGPUProcessTable&    m_ProcessTable;
    Threading::Signal            m_BufferMappedSignal;
    Threading::Signal            m_CopyScheduledSignal;
    RefCntAutoPtr<IBufferWebGPU> m_pStagingBuffer;
    WGPUBuffer                   m_wgpuStagingBuffer;
    std::vector<Uint32>          m_SubresourceOffsets;
    std::vector<Uint32>          m_SubresourceStrides;
};

} // namespace

struct TextureUploaderWebGPU::InternalData
{
    struct PendingBufferOperation
    {
        enum Operation
        {
            Map,
            Copy
        } operation;

        RefCntAutoPtr<UploadBufferWebGPU> pUploadBuffer;
        RefCntAutoPtr<ITexture>           pDstTexture;
        Uint32                            DstSlice = 0;
        Uint32                            DstMip   = 0;

        // clang-format off
        PendingBufferOperation(Operation op, UploadBufferWebGPU* pBuff) :
            operation    {op   },
            pUploadBuffer{pBuff}
        {}
        PendingBufferOperation(Operation op, UploadBufferWebGPU* pBuff, ITexture* pDstTex, Uint32 Slice, Uint32 Mip) :
            operation             {op          },
            pUploadBuffer         {pBuff       },
            pDstTexture           {pDstTex     },
            DstSlice              {Slice       },
            DstMip                {Mip         }
          
        {}
        // clang-format on
    };

    InternalData(IRenderDevice* pDevice)
    {
        RefCntAutoPtr<IRenderDeviceWebGPU>  pDeviceWebGPU(pDevice, IID_RenderDeviceWebGPU);
        RefCntAutoPtr<IEngineFactoryWebGPU> pEngineFactory(pDevice->GetEngineFactory(), IID_EngineFactoryWebGPU);
        m_pDeviceWebGPU = pDeviceWebGPU;
#if PLATFORM_EMSCRIPTEN
        m_ProcessTable.DeviceCreateBuffer   = wgpuDeviceCreateBuffer;
        m_ProcessTable.BufferGetMappedRange = wgpuBufferGetMappedRange;
        m_ProcessTable.BufferUnmap          = wgpuBufferUnmap;
        m_ProcessTable.BufferRelease        = wgpuBufferRelease;
#else
        const auto* pProcessTable           = static_cast<const DawnProcTable*>(pEngineFactory->GetProcessTable());
        m_ProcessTable.DeviceCreateBuffer   = pProcessTable->deviceCreateBuffer;
        m_ProcessTable.BufferGetMappedRange = pProcessTable->bufferGetMappedRange;
        m_ProcessTable.BufferUnmap          = pProcessTable->bufferUnmap;
        m_ProcessTable.BufferRelease        = pProcessTable->bufferRelease;
#endif
    }

    void SwapMapQueues()
    {
        std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
        m_PendingOperations.swap(m_InWorkOperations);
    }

    void EnqueueCopy(UploadBufferWebGPU* pUploadBuffer, ITexture* pDstTexture, Uint32 Slice, Uint32 MipLevel)
    {
        std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
        m_PendingOperations.emplace_back(PendingBufferOperation::Operation::Copy, pUploadBuffer, pDstTexture, Slice, MipLevel);
    }

    void EnqueueMap(UploadBufferWebGPU* pUploadBuffer)
    {
        std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
        m_PendingOperations.emplace_back(PendingBufferOperation::Operation::Map, pUploadBuffer);
    }

    void Execute(IDeviceContext*         pContext,
                 PendingBufferOperation& OperationInfo)
    {
        auto&       pBuffer        = OperationInfo.pUploadBuffer;
        const auto& UploadBuffDesc = pBuffer->GetDesc();

        switch (OperationInfo.operation)
        {
            case PendingBufferOperation::Map:
            {
                if (pBuffer->m_pStagingBuffer == nullptr)
                {
                    BufferDesc BuffDesc;
                    BuffDesc.Name           = "Staging buffer for UploadBufferWebGPU";
                    BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
                    BuffDesc.Usage          = USAGE_STAGING;
                    BuffDesc.Size           = pBuffer->GetTotalSize();

                    WGPUBufferDescriptor wgpuBufferDesc{};
                    wgpuBufferDesc.label            = BuffDesc.Name;
                    wgpuBufferDesc.size             = BuffDesc.Size;
                    wgpuBufferDesc.usage            = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
                    wgpuBufferDesc.mappedAtCreation = true;


                    WGPUBuffer wgpuBuffer = m_ProcessTable.DeviceCreateBuffer(m_pDeviceWebGPU->GetWebGPUDevice(), &wgpuBufferDesc);

                    RefCntAutoPtr<IBuffer> pStagingBuffer;
                    m_pDeviceWebGPU->CreateBufferFromWebGPUBuffer(wgpuBuffer, BuffDesc, RESOURCE_STATE_UNKNOWN, &pStagingBuffer);

                    RefCntAutoPtr<IBufferWebGPU> pStagingBufferWebGPU{pStagingBuffer, IID_BufferWebGPU};
                    pBuffer->m_pStagingBuffer    = pStagingBufferWebGPU;
                    pBuffer->m_wgpuStagingBuffer = wgpuBuffer;
                }

                PVoid CpuAddress = m_ProcessTable.BufferGetMappedRange(pBuffer->m_pStagingBuffer->GetWebGPUBuffer(), 0, static_cast<size_t>(pBuffer->m_pStagingBuffer->GetDesc().Size));
                pBuffer->SetDataPtr(static_cast<Uint8*>(CpuAddress));
                pBuffer->SignalMapped();
            }
            break;

            case PendingBufferOperation::Copy:
            {
                VERIFY_EXPR(pBuffer->m_wgpuStagingBuffer != nullptr);
                VERIFY_EXPR(pBuffer->m_pStagingBuffer != nullptr);

                const auto& TexDesc = OperationInfo.pDstTexture->GetDesc();
                m_ProcessTable.BufferUnmap(pBuffer->m_pStagingBuffer->GetWebGPUBuffer());

                for (Uint32 Slice = 0; Slice < UploadBuffDesc.ArraySize; ++Slice)
                {
                    for (Uint32 Mip = 0; Mip < UploadBuffDesc.MipLevels; ++Mip)
                    {
                        auto SrcOffset = pBuffer->GetOffset(Mip, Slice);
                        auto SrcStride = pBuffer->GetMappedData(Mip, Slice).Stride;

                        TextureSubResData SubResData(pBuffer->m_pStagingBuffer, SrcOffset, SrcStride);

                        auto MipLevelProps = GetMipLevelProperties(TexDesc, OperationInfo.DstMip + Mip);
                        Box  DstBox;
                        DstBox.MaxX = MipLevelProps.LogicalWidth;
                        DstBox.MaxY = MipLevelProps.LogicalHeight;
                        pContext->UpdateTexture(OperationInfo.pDstTexture, OperationInfo.DstMip + Mip, OperationInfo.DstSlice + Slice, DstBox,
                                                SubResData, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    }
                }
                pBuffer->m_pStagingBuffer.Release();
                m_ProcessTable.BufferRelease(pBuffer->m_wgpuStagingBuffer);
                pBuffer->m_wgpuStagingBuffer = nullptr;
                pBuffer->SignalCopyScheduled();
            }
            break;
        }
    }
    std::mutex                          m_PendingOperationsMtx;
    std::vector<PendingBufferOperation> m_PendingOperations;
    std::vector<PendingBufferOperation> m_InWorkOperations;

    std::mutex                                                                          m_UploadBuffCacheMtx;
    std::unordered_map<UploadBufferDesc, std::deque<RefCntAutoPtr<UploadBufferWebGPU>>> m_UploadBufferCache;

    WebGPUProcessTable                 m_ProcessTable;
    RefCntAutoPtr<IRenderDeviceWebGPU> m_pDeviceWebGPU;
};



TextureUploaderWebGPU::TextureUploaderWebGPU(IReferenceCounters* pRefCounters, IRenderDevice* pDevice, const Diligent::TextureUploaderDesc Desc) :
    TextureUploaderBase(pRefCounters, pDevice, Desc),
    m_pInternalData{new InternalData{pDevice}}
{
}

TextureUploaderWebGPU::~TextureUploaderWebGPU()
{
    auto Stats = TextureUploaderWebGPU::GetStats();
    if (Stats.NumPendingOperations != 0)
    {
        LOG_WARNING_MESSAGE("TextureUploaderWebGPU::~TextureUploaderWebGPU(): there ", (Stats.NumPendingOperations > 1 ? "are " : "is "),
                            Stats.NumPendingOperations, (Stats.NumPendingOperations > 1 ? " pending operations" : " pending operation"),
                            " in the queue. If other threads wait for ", (Stats.NumPendingOperations > 1 ? "these operations" : "this operation"),
                            ", they may deadlock.");
    }
}

void TextureUploaderWebGPU::RenderThreadUpdate(IDeviceContext* pContext)
{
    m_pInternalData->SwapMapQueues();
    if (!m_pInternalData->m_InWorkOperations.empty())
    {
        for (auto& OperationInfo : m_pInternalData->m_InWorkOperations)
        {
            m_pInternalData->Execute(pContext, OperationInfo);
        }
        m_pInternalData->m_InWorkOperations.clear();
    }
}

void TextureUploaderWebGPU::AllocateUploadBuffer(IDeviceContext*         pContext,
                                                 const UploadBufferDesc& Desc,
                                                 IUploadBuffer**         ppBuffer)
{
    *ppBuffer = nullptr;
    RefCntAutoPtr<UploadBufferWebGPU> pUploadBuffer;

    {
        std::lock_guard<std::mutex> CacheLock(m_pInternalData->m_UploadBuffCacheMtx);
        auto&                       Cache = m_pInternalData->m_UploadBufferCache;
        if (!Cache.empty())
        {
            auto DequeIt = Cache.find(Desc);
            if (DequeIt != Cache.end())
            {
                auto& Deque = DequeIt->second;
                if (!Deque.empty())
                {
                    pUploadBuffer.Attach(Deque.front().Detach());
                    Deque.pop_front();
                }
            }
        }
    }

    if (!pUploadBuffer)
    {
        pUploadBuffer = MakeNewRCObj<UploadBufferWebGPU>()(Desc, m_pInternalData->m_ProcessTable);
        LOG_INFO_MESSAGE("TextureUploaderWebGPU: created upload buffer for ", Desc.Width, 'x', Desc.Height, 'x',
                         Desc.Depth, ' ', Desc.MipLevels, "-mip ", Desc.ArraySize, "-slice ",
                         m_pDevice->GetTextureFormatInfo(Desc.Format).Name, " texture");
    }

    if (pContext != nullptr)
    {
        // Render thread
        InternalData::PendingBufferOperation MapOp{InternalData::PendingBufferOperation::Operation::Map, pUploadBuffer};
        m_pInternalData->Execute(pContext, MapOp);
    }
    else
    {
        // Worker thread
        m_pInternalData->EnqueueMap(pUploadBuffer);
        pUploadBuffer->WaitForMap();
    }
    *ppBuffer = pUploadBuffer.Detach();
}

void TextureUploaderWebGPU::ScheduleGPUCopy(IDeviceContext* pContext,
                                            ITexture*       pDstTexture,
                                            Uint32          ArraySlice,
                                            Uint32          MipLevel,
                                            IUploadBuffer*  pUploadBuffer)
{
    auto* pUploadBufferWebGPU = ClassPtrCast<UploadBufferWebGPU>(pUploadBuffer);
    if (pContext != nullptr)
    {
        // Render thread
        InternalData::PendingBufferOperation CopyOp //
            {
                InternalData::PendingBufferOperation::Operation::Copy,
                pUploadBufferWebGPU,
                pDstTexture,
                ArraySlice,
                MipLevel //
            };
        m_pInternalData->Execute(pContext, CopyOp);
    }
    else
    {
        // Worker thread
        m_pInternalData->EnqueueCopy(pUploadBufferWebGPU, pDstTexture, ArraySlice, MipLevel);
    }
}

void TextureUploaderWebGPU::RecycleBuffer(IUploadBuffer* pUploadBuffer)
{
    auto* pUploadBufferWebGPU = ClassPtrCast<UploadBufferWebGPU>(pUploadBuffer);
    VERIFY(pUploadBufferWebGPU->DbgIsCopyScheduled(), "Upload buffer must be recycled only after copy operation has been scheduled on the GPU");
    pUploadBufferWebGPU->Reset();

    std::lock_guard<std::mutex> CacheLock(m_pInternalData->m_UploadBuffCacheMtx);

    auto& Cache = m_pInternalData->m_UploadBufferCache;
    auto& Deque = Cache[pUploadBufferWebGPU->GetDesc()];
    Deque.emplace_back(pUploadBufferWebGPU);
}

TextureUploaderStats TextureUploaderWebGPU::GetStats()
{
    TextureUploaderStats        Stats;
    std::lock_guard<std::mutex> QueueLock(m_pInternalData->m_PendingOperationsMtx);
    Stats.NumPendingOperations = static_cast<Uint32>(m_pInternalData->m_PendingOperations.size());
    return Stats;
}

} // namespace Diligent
