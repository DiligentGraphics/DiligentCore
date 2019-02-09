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

#include "pch.h"

#include <atlbase.h>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <vector>
#include <d3d12.h>

#include "TextureUploaderD3D12.h"
#include "RenderDeviceD3D12.h"
#include "BufferD3D12.h"
#include "TextureD3D12.h"
#include "ThreadSignal.h"

namespace Diligent
{

    class UploadBufferD3D12 : public UploadBufferBase
    {
    public:
        UploadBufferD3D12(IReferenceCounters *pRefCounters, 
                          IRenderDeviceD3D12 *pRenderDeviceD3D12,
                          const UploadBufferDesc &Desc, 
                          IBuffer *pStagingBuffer,
                          void *pData, 
                          size_t RowStride, 
                          size_t DepthStride) :
            UploadBufferBase(pRefCounters, Desc),
            m_pDeviceD3D12(pRenderDeviceD3D12),
            m_pStagingBuffer(pStagingBuffer)
        {
            m_pData = pData;
            m_RowStride = RowStride;
            m_DepthStride = DepthStride;
        }

        ~UploadBufferD3D12()
        {
            RefCntAutoPtr<IBufferD3D12> pBufferD3D12(m_pStagingBuffer, IID_BufferD3D12);
            size_t DataStartOffset;
            auto* pd3d12Buff = pBufferD3D12->GetD3D12Buffer(DataStartOffset, nullptr);
            pd3d12Buff->Unmap(0, nullptr);
            LOG_INFO_MESSAGE("Releasing staging buffer of size ", m_pStagingBuffer->GetDesc().uiSizeInBytes);
        }
          
        void SignalCopyScheduled()
        {
            m_CopyScheduledSignal.Trigger();
        }

        void Reset()
        {
            m_CopyScheduledSignal.Reset();
        }

        virtual void WaitForCopyScheduled()override final
        {
            m_CopyScheduledSignal.Wait();
        }

        IBuffer* GetStagingBuffer() { return m_pStagingBuffer; }

        bool DbgIsCopyScheduled()const { return m_CopyScheduledSignal.IsTriggered(); }
    private:

        ThreadingTools::Signal m_CopyScheduledSignal;

        RefCntAutoPtr<IBuffer> m_pStagingBuffer;
        RefCntAutoPtr<IRenderDeviceD3D12> m_pDeviceD3D12;
    };

    struct TextureUploaderD3D12::InternalData
    {
        InternalData(IRenderDevice *pDevice) :
            m_pDeviceD3D12(pDevice, IID_RenderDeviceD3D12)
        {
            m_pd3d12NativeDevice = m_pDeviceD3D12->GetD3D12Device();
        }

        CComPtr<ID3D12Device> m_pd3d12NativeDevice;
        RefCntAutoPtr<IRenderDeviceD3D12> m_pDeviceD3D12;

        void SwapMapQueues()
        {
            std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
            m_PendingOperations.swap(m_InWorkOperations);
        }

        void EnqueCopy(UploadBufferD3D12 *pUploadBuffer, ITextureD3D12 *pDstTex, Uint32 dstSlice, Uint32 dstMip)
        {
            std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
            m_PendingOperations.emplace_back(PendingBufferOperation::Operation::Copy, pUploadBuffer, pDstTex, dstSlice, dstMip);
        }

        std::mutex m_PendingOperationsMtx;
        struct PendingBufferOperation
        {
            enum Operation
            {
                Copy
            }operation;
            RefCntAutoPtr<UploadBufferD3D12> pUploadBuffer;
            RefCntAutoPtr<ITextureD3D12> pDstTexture;
            Uint32 DstSlice = 0;
            Uint32 DstMip = 0;

            PendingBufferOperation(Operation op, UploadBufferD3D12* pBuff) :
                operation(op),
                pUploadBuffer(pBuff)
            {}
            PendingBufferOperation(Operation op, UploadBufferD3D12* pBuff, ITextureD3D12 *pDstTex, Uint32 dstSlice, Uint32 dstMip) :
                operation(op),
                pUploadBuffer(pBuff),
                pDstTexture(pDstTex),
                DstSlice(dstSlice),
                DstMip(dstMip)
            {}
        };
        std::vector< PendingBufferOperation > m_PendingOperations;
        std::vector< PendingBufferOperation > m_InWorkOperations;

        std::mutex m_UploadBuffCacheMtx;
        std::unordered_map< UploadBufferDesc, std::deque< std::pair<Uint64, RefCntAutoPtr<UploadBufferD3D12> > > > m_UploadBufferCache;
    };

    TextureUploaderD3D12::TextureUploaderD3D12(IReferenceCounters *pRefCounters, IRenderDevice *pDevice, const TextureUploaderDesc Desc) :
        TextureUploaderBase(pRefCounters, pDevice, Desc),
        m_pInternalData(new InternalData(pDevice))
    {
    }

    TextureUploaderD3D12::~TextureUploaderD3D12()
    {
        for (auto BuffQueueIt : m_pInternalData->m_UploadBufferCache)
        {
            if (BuffQueueIt.second.size())
            {
                const auto &desc = BuffQueueIt.first;
                auto &FmtInfo = m_pDevice->GetTextureFormatInfo(desc.Format);
                LOG_INFO_MESSAGE("TextureUploaderD3D12: releasing ", BuffQueueIt.second.size(), ' ', desc.Width, 'x', desc.Height, 'x', desc.Depth, ' ', FmtInfo.Name, " upload buffer(s) ");
            }
        }
    }

    void TextureUploaderD3D12::RenderThreadUpdate(IDeviceContext *pContext)
    {
        m_pInternalData->SwapMapQueues();
        if (!m_pInternalData->m_InWorkOperations.empty())
        {
            for (auto &OperationInfo : m_pInternalData->m_InWorkOperations)
            {
                auto &pBuffer = OperationInfo.pUploadBuffer;

                switch (OperationInfo.operation)
                {
                    case InternalData::PendingBufferOperation::Copy:
                    {
                        TextureSubResData SubResData(pBuffer->GetStagingBuffer(), 0, static_cast<Uint32>(pBuffer->GetRowStride()));
                        const auto &TexDesc = OperationInfo.pDstTexture->GetDesc();
                        Box DstBox;
                        DstBox.MaxX = TexDesc.Width;
                        DstBox.MaxY = TexDesc.Height;
                        // UpdateTexture() transitions dst subresource to COPY_DEST state and then transitions back to original state
                        pContext->UpdateTexture(OperationInfo.pDstTexture, OperationInfo.DstMip, OperationInfo.DstSlice, DstBox,
                                                SubResData, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                        pBuffer->SignalCopyScheduled();
                    }
                    break;
                }
            }

            m_pInternalData->m_InWorkOperations.clear();
        }
    }

    void TextureUploaderD3D12::AllocateUploadBuffer(const UploadBufferDesc& Desc, bool IsRenderThread, IUploadBuffer **ppBuffer)
    {
        *ppBuffer = nullptr;

        {
            std::lock_guard<std::mutex> CacheLock(m_pInternalData->m_UploadBuffCacheMtx);
            auto &Cache = m_pInternalData->m_UploadBufferCache;
            if (!Cache.empty())
            {
                auto DequeIt = Cache.find(Desc);
                if (DequeIt != Cache.end())
                {
                    auto &Deque = DequeIt->second;
                    if (!Deque.empty())
                    {
                        auto &FrontBuff = Deque.front();
                        if (m_pInternalData->m_pDeviceD3D12->IsFenceSignaled(0, FrontBuff.first))
                        {
                            *ppBuffer = FrontBuff.second.Detach();
                            Deque.pop_front();
                        }
                    }
                }
            }
        }

        // No available buffer found in the cache
        if(*ppBuffer == nullptr)
        {
            BufferDesc BuffDesc;
            BuffDesc.Name = "Staging buffer for UploadBufferD3D12";
            BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            BuffDesc.Usage = USAGE_CPU_ACCESSIBLE;

            const auto &TexFmtInfo = m_pDevice->GetTextureFormatInfo(Desc.Format);
            Uint32 RowStride = Desc.Width * Uint32{TexFmtInfo.ComponentSize} * Uint32{TexFmtInfo.NumComponents};
            static_assert((D3D12_TEXTURE_DATA_PITCH_ALIGNMENT & (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) == 0, "Alginment is expected to be power of 2");
            Uint32 AlignmentMask = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1;
            RowStride = (RowStride + AlignmentMask) & (~AlignmentMask);

            BuffDesc.uiSizeInBytes = Desc.Height * RowStride;
            RefCntAutoPtr<IBuffer> pStagingBuffer;
            m_pDevice->CreateBuffer(BuffDesc, nullptr, &pStagingBuffer);

            PVoid CpuVirtualAddress = nullptr;
            RefCntAutoPtr<IBufferD3D12> pStagingBufferD3D12(pStagingBuffer, IID_BufferD3D12);
            size_t DataStartOffset;
            auto* pd3d12Buff = pStagingBufferD3D12->GetD3D12Buffer(DataStartOffset, nullptr);
            pd3d12Buff->Map(0, nullptr, &CpuVirtualAddress);
            if (CpuVirtualAddress == nullptr)
            {
                LOG_ERROR_MESSAGE("Failed to map upload buffer");
                return;
            }

            LOG_INFO_MESSAGE("Created staging buffer of size ", BuffDesc.uiSizeInBytes);

            RefCntAutoPtr<UploadBufferD3D12> pUploadBuffer(MakeNewRCObj<UploadBufferD3D12>()(m_pInternalData->m_pDeviceD3D12, Desc, pStagingBuffer, CpuVirtualAddress, RowStride, 0));
            *ppBuffer = pUploadBuffer.Detach();
        }
    }

    void TextureUploaderD3D12::ScheduleGPUCopy(ITexture *pDstTexture,
        Uint32 ArraySlice,
        Uint32 MipLevel,
        IUploadBuffer *pUploadBuffer)
    {
        auto *pUploadBufferD3D12 = ValidatedCast<UploadBufferD3D12>(pUploadBuffer);
        RefCntAutoPtr<ITextureD3D12> pDstTexD3D12(pDstTexture, IID_TextureD3D12);
        m_pInternalData->EnqueCopy(pUploadBufferD3D12, pDstTexD3D12, ArraySlice, MipLevel);
    }

    void TextureUploaderD3D12::RecycleBuffer(IUploadBuffer *pUploadBuffer)
    {
        auto *pUploadBufferD3D12 = ValidatedCast<UploadBufferD3D12>(pUploadBuffer);
        VERIFY(pUploadBufferD3D12->DbgIsCopyScheduled(), "Upload buffer must be recycled only after copy operation has been scheduled on the GPU");
        pUploadBufferD3D12->Reset();

        std::lock_guard<std::mutex> CacheLock(m_pInternalData->m_UploadBuffCacheMtx);
        auto &Cache = m_pInternalData->m_UploadBufferCache;
        auto &Deque = Cache[pUploadBufferD3D12->GetDesc()];
        Uint64 FenceValue = m_pInternalData->m_pDeviceD3D12->GetNextFenceValue(0);
        Deque.emplace_back( FenceValue, pUploadBufferD3D12 );
    }
}
