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

#include "pch.h"
#include <mutex>
#include <deque>
#include <unordered_map>
#include <vector>
#include "TextureUploaderGL.h"

namespace Diligent
{

    class UploadBufferGL : public UploadBufferBase
    {
    public:
        UploadBufferGL(IReferenceCounters *pRefCounters, const UploadBufferDesc &Desc) :
            UploadBufferBase(pRefCounters, Desc)
        {}

        void SetDataPtr(void *pData, size_t RowStride, size_t DepthStride)
        {
            m_pData = pData;
            m_RowStride = RowStride;
            m_DepthStride = DepthStride;
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

        virtual void WaitForCopyScheduled()override final
        {
            m_CopyScheduledSignal.Wait();
        }

        bool DbgIsCopyScheduled()const { return m_CopyScheduledSignal.IsTriggered(); }

        void Reset()
        {
            m_BufferMappedSignal.Reset();
            m_CopyScheduledSignal.Reset();
            m_pData = nullptr;
            // Do not zero out strides 
        }

    private:
        friend class TextureUploaderGL;
        ThreadingTools::Signal m_BufferMappedSignal;
        ThreadingTools::Signal m_CopyScheduledSignal;
        RefCntAutoPtr<IBuffer> m_pStagingBuffer;
    };

    struct TextureUploaderGL::InternalData
    {
        void SwapMapQueues()
        {
            std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
            m_PendingOperations.swap(m_InWorkOperations);
        }

        void EnqueCopy(UploadBufferGL *pUploadBuffer, ITexture *pDstTexture, Uint32 dstSlice, Uint32 dstMip)
        {
            std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
            m_PendingOperations.emplace_back(PendingBufferOperation::Operation::Copy, pUploadBuffer, pDstTexture, dstSlice, dstMip);
        }

        void EnqueMap(UploadBufferGL *pUploadBuffer)
        {
            std::lock_guard<std::mutex> QueueLock(m_PendingOperationsMtx);
            m_PendingOperations.emplace_back(PendingBufferOperation::Operation::Map, pUploadBuffer);
        }

        
        struct PendingBufferOperation
        {
            enum Operation
            {
                Map,
                Copy
            }operation;
            RefCntAutoPtr<UploadBufferGL> pUploadBuffer;
            RefCntAutoPtr<ITexture> pDstTexture;
            Uint32 DstSlice = 0;
            Uint32 DstMip = 0;

            PendingBufferOperation(Operation op, UploadBufferGL* pBuff) :
                operation(op),
                pUploadBuffer(pBuff)
            {}
            PendingBufferOperation(Operation op, UploadBufferGL* pBuff, ITexture *pDstTex, Uint32 dstSlice, Uint32 dstMip) :
                operation(op),
                pUploadBuffer(pBuff),
                pDstTexture(pDstTex),
                DstSlice(dstSlice),
                DstMip(dstMip)
            {}
        };

        std::mutex m_PendingOperationsMtx;
        std::vector< PendingBufferOperation > m_PendingOperations;
        std::vector< PendingBufferOperation > m_InWorkOperations;

        std::mutex m_UploadBuffCacheMtx;
        std::unordered_map< UploadBufferDesc, std::deque<RefCntAutoPtr<UploadBufferGL> > > m_UploadBufferCache;
    };

    TextureUploaderGL::TextureUploaderGL(IReferenceCounters *pRefCounters, IRenderDevice *pDevice, const TextureUploaderDesc Desc) :
        TextureUploaderBase(pRefCounters, pDevice, Desc),
        m_pInternalData(new InternalData())
    {
    }

    TextureUploaderGL::~TextureUploaderGL()
    {
        for (auto BuffQueueIt : m_pInternalData->m_UploadBufferCache)
        {
            if (BuffQueueIt.second.size())
            {
                const auto &desc = BuffQueueIt.first;
                auto &FmtInfo = m_pDevice->GetTextureFormatInfo(desc.Format);
                LOG_INFO_MESSAGE("TextureUploaderGL: releasing ", BuffQueueIt.second.size(), ' ', desc.Width, 'x', desc.Height, 'x', desc.Depth, ' ', FmtInfo.Name, " upload buffer(s) ");
            }
        }
    }

    void TextureUploaderGL::RenderThreadUpdate(IDeviceContext *pContext)
    {
        m_pInternalData->SwapMapQueues();
        if (!m_pInternalData->m_InWorkOperations.empty())
        {
            for (auto &OperationInfo : m_pInternalData->m_InWorkOperations)
            {
                auto &pBuffer = OperationInfo.pUploadBuffer;

                switch (OperationInfo.operation)
                {
                    case InternalData::PendingBufferOperation::Map:
                    {
                        Uint32 RowStride = static_cast<Uint32>(pBuffer->GetRowStride());
                        if (pBuffer->m_pStagingBuffer == nullptr)
                        {
                            const auto &Desc = pBuffer->GetDesc();
                            BufferDesc BuffDesc;
                            BuffDesc.Name = "Staging buffer for UploadBufferGL";
                            BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
                            BuffDesc.Usage = USAGE_CPU_ACCESSIBLE;

                            const auto &TexFmtInfo = m_pDevice->GetTextureFormatInfo(Desc.Format);
                            RowStride = Desc.Width * Uint32{TexFmtInfo.ComponentSize} * Uint32{TexFmtInfo.NumComponents};
                            const Uint32 Alignment = 16;
                            const Uint32 AlignmentMask = Alignment-1;
                            RowStride = (RowStride + AlignmentMask) & (~AlignmentMask);

                            BuffDesc.uiSizeInBytes = Desc.Height * RowStride;
                            RefCntAutoPtr<IBuffer> pStagingBuffer;
                            m_pDevice->CreateBuffer(BuffDesc, BufferData(), &pBuffer->m_pStagingBuffer);
                        }

                        PVoid CpuAddress = nullptr;
                        pBuffer->m_pStagingBuffer->Map(pContext, MAP_WRITE, MAP_FLAG_DISCARD, CpuAddress);
                        pBuffer->SetDataPtr(CpuAddress, RowStride, 0);
                    
                        pBuffer->SignalMapped();
                    }
                    break;

                    case InternalData::PendingBufferOperation::Copy:
                    {
                        pBuffer->m_pStagingBuffer->Unmap(pContext, MAP_WRITE, MAP_FLAG_DISCARD);
                        TextureSubResData SubResData(pBuffer->m_pStagingBuffer, static_cast<Uint32>(pBuffer->GetRowStride()));
                        Box DstBox;
                        const auto &TexDesc = OperationInfo.pDstTexture->GetDesc();
                        DstBox.MaxX = TexDesc.Width;
                        DstBox.MaxY = TexDesc.Height;
                        OperationInfo.pDstTexture->UpdateData(pContext, OperationInfo.DstMip, OperationInfo.DstSlice, DstBox, SubResData);
                        pBuffer->SignalCopyScheduled();
                    }
                    break;
                }
            }
            m_pInternalData->m_InWorkOperations.clear();
        }
    }

    void TextureUploaderGL::AllocateUploadBuffer(const UploadBufferDesc& Desc, bool IsRenderThread, IUploadBuffer **ppBuffer)
    {
        *ppBuffer = nullptr;
        RefCntAutoPtr<UploadBufferGL> pUploadBuffer;

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
                        pUploadBuffer.Attach(Deque.front().Detach());
                        Deque.pop_front();
                    }
                }
            }
        }

        if( !pUploadBuffer )
        {
            pUploadBuffer = MakeNewRCObj<UploadBufferGL>()(Desc);
            LOG_INFO_MESSAGE("TextureUploaderGL: created upload buffer for ", Desc.Width, 'x', Desc.Height, 'x', Desc.Depth, ' ', m_pDevice->GetTextureFormatInfo(Desc.Format).Name, " texture");
        }

        m_pInternalData->EnqueMap(pUploadBuffer);
        pUploadBuffer->WaitForMap();
        *ppBuffer = pUploadBuffer.Detach();
    }

    void TextureUploaderGL::ScheduleGPUCopy(ITexture *pDstTexture,
        Uint32 ArraySlice,
        Uint32 MipLevel,
        IUploadBuffer *pUploadBuffer)
    {
        auto *pUploadBufferGL = ValidatedCast<UploadBufferGL>(pUploadBuffer);
        m_pInternalData->EnqueCopy(pUploadBufferGL, pDstTexture, ArraySlice, MipLevel);
    }

    void TextureUploaderGL::RecycleBuffer(IUploadBuffer *pUploadBuffer)
    {
        auto *pUploadBufferGL = ValidatedCast<UploadBufferGL>(pUploadBuffer);
        VERIFY(pUploadBufferGL->DbgIsCopyScheduled(), "Upload buffer must be recycled only after copy operation has been scheduled on the GPU");
        pUploadBufferGL->Reset();

        std::lock_guard<std::mutex> CacheLock(m_pInternalData->m_UploadBuffCacheMtx);
        auto &Cache = m_pInternalData->m_UploadBufferCache;
        auto &Deque = Cache[pUploadBufferGL->GetDesc()];
        Deque.emplace_back( pUploadBufferGL );
    }
}
