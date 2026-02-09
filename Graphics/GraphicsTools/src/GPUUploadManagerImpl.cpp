/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "GPUUploadManagerImpl.hpp"
#include "DebugUtilities.hpp"
#include "Align.hpp"
#include "Atomics.hpp"

#include <vector>
#include <cstring>
#include <thread>

namespace Diligent
{

GPUUploadManagerImpl::Page::Writer::Writer(Writer&& Other) noexcept :
    m_pPage{Other.m_pPage}
{
    Other.m_pPage = nullptr;
}

bool GPUUploadManagerImpl::Page::Writer::ScheduleBufferUpdate(IBuffer*                      pDstBuffer,
                                                              Uint32                        DstOffset,
                                                              Uint32                        NumBytes,
                                                              const void*                   pSrcData,
                                                              GPUUploadEnqueuedCallbackType Callback,
                                                              void*                         pCallbackData)
{
    if (m_pPage == nullptr)
    {
        UNEXPECTED("Attempting to schedule a buffer update with an invalid writer.");
        return false;
    }

    return m_pPage->ScheduleBufferUpdate(pDstBuffer, DstOffset, NumBytes, pSrcData, Callback, pCallbackData);
}

GPUUploadManagerImpl::Page::WritingStatus GPUUploadManagerImpl::Page::Writer::EndWriting()
{
    if (m_pPage == nullptr)
    {
        UNEXPECTED("Attempting to end writing with an invalid writer.");
        return WritingStatus::NotSealed;
    }

    WritingStatus Status = m_pPage->EndWriting();
    m_pPage              = nullptr;
    return Status;
}

GPUUploadManagerImpl::Page::Writer::~Writer()
{
    if (m_pPage != nullptr)
    {
        UNEXPECTED("Writer was not explicitly ended. Ending writing in destructor.");
        EndWriting();
    }
}

GPUUploadManagerImpl::Page::Page(Uint32 Size, bool PersistentMapped) noexcept :
    m_Size{Size},
    m_PersistentMapped{PersistentMapped}
{}

inline bool PersistentMapSupported(IRenderDevice* pDevice)
{
    RENDER_DEVICE_TYPE DeviceType = pDevice->GetDeviceInfo().Type;
    return DeviceType == RENDER_DEVICE_TYPE_D3D12 || DeviceType == RENDER_DEVICE_TYPE_VULKAN;
}

GPUUploadManagerImpl::Page::Page(IRenderDevice*  pDevice,
                                 IDeviceContext* pContext,
                                 Uint32          Size) :
    Page{
        Size,
        PersistentMapSupported(pDevice),
    }
{
    static std::atomic<int> PageCounter{0};
    const std::string       Name = "GPUUploadManagerImpl page " + std::to_string(PageCounter.fetch_add(1));

    BufferDesc Desc;
    Desc.Name           = Name.c_str();
    Desc.Size           = Size;
    Desc.Usage          = USAGE_STAGING;
    Desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(Desc, nullptr, &m_pStagingBuffer);
    VERIFY_EXPR(m_pStagingBuffer != nullptr);

    pContext->MapBuffer(m_pStagingBuffer, MAP_WRITE, MAP_FLAG_NONE, m_pData);
    VERIFY_EXPR(m_pData != nullptr);
}

GPUUploadManagerImpl::Page::Writer GPUUploadManagerImpl::Page::TryBeginWriting()
{
    Uint32 State = m_State.load(std::memory_order_acquire);
    for (;;)
    {
        if (State & SEALED_BIT)
        {
            // The page is sealed for new writes.
            return Writer{nullptr};
        }

        if ((State & WRITER_MASK) == WRITER_MASK)
        {
            // Too many writers.
            // This should never happen in practice, but we handle this case for robustness.
            return Writer{nullptr};
        }

        if (m_State.compare_exchange_weak(
                State, // On failure, updated state is written back to State variable.
                State + 1,
                std::memory_order_acq_rel))
            return Writer{this};
    }
}

GPUUploadManagerImpl::Page::WritingStatus GPUUploadManagerImpl::Page::EndWriting()
{
    const uint32_t PrevState   = m_State.fetch_sub(1, std::memory_order_acq_rel);
    const uint32_t PrevWriters = (PrevState & WRITER_MASK);
    VERIFY_EXPR(PrevWriters > 0);
    if (PrevState & SEALED_BIT)
    {
        return PrevWriters == 1 ? WritingStatus::LastWriterSealed : WritingStatus::NotLastWriter;
    }
    else
    {
        return WritingStatus::NotSealed;
    }
}

// Seals the page for new writes.
GPUUploadManagerImpl::Page::SealStatus GPUUploadManagerImpl::Page::TrySeal()
{
    uint32_t State = m_State.load(std::memory_order_acquire);
    for (;;)
    {
        if (State & SEALED_BIT)
        {
            return SealStatus::AlreadySealed;
        }

        if (m_State.compare_exchange_weak(
                State, // On failure, updated state is written back to State variable.
                State | SEALED_BIT,
                std::memory_order_acq_rel))
        {
            // If there were no writers at the instant we sealed the page,
            // it's now ready for execution because no new writers can start
            return (State & WRITER_MASK) == 0 ?
                SealStatus::Ready :
                SealStatus::NotReady;
        }
    }
}

bool GPUUploadManagerImpl::Page::ScheduleBufferUpdate(IBuffer*                      pDstBuffer,
                                                      Uint32                        DstOffset,
                                                      Uint32                        NumBytes,
                                                      const void*                   pSrcData,
                                                      GPUUploadEnqueuedCallbackType Callback,
                                                      void*                         pCallbackData)
{
    VERIFY_EXPR(DbgGetWriterCount() > 0);

    // Note that the page may be sealed for new writes at this point,
    // but we can still schedule the update since we have an active writer
    // that prevents the page from being submitted for execution.

    constexpr Uint32 Alignment   = 16;
    const Uint32     AlignedSize = AlignUp(NumBytes, Alignment);

    Uint32 Offset = m_Offset.load(std::memory_order_relaxed);
    for (;;)
    {
        if (Offset + AlignedSize > m_Size)
            return false; // Fail without incrementing offset

        if (m_Offset.compare_exchange_weak(Offset, Offset + AlignedSize, std::memory_order_relaxed))
            break; // Success
    }

    m_NumPendingOps.fetch_add(1, std::memory_order_relaxed);

    if (m_pData != nullptr)
    {
        VERIFY_EXPR(pSrcData != nullptr);
        std::memcpy(static_cast<Uint8*>(m_pData) + Offset, pSrcData, NumBytes);
    }

    PendingOp Op;
    Op.pDstBuffer    = pDstBuffer;
    Op.Callback      = Callback;
    Op.pCallbackData = pCallbackData;
    Op.SrcOffset     = Offset;
    Op.DstOffset     = DstOffset;
    Op.NumBytes      = NumBytes;
    m_PendingOps.Enqueue(std::move(Op));

    return true;
}

void GPUUploadManagerImpl::Page::ExecutePendingOps(IDeviceContext* pContext, Uint64 FenceValue)
{
    VERIFY(DbgIsSealed(), "Page must be sealed before executing pending operations");
    VERIFY(DbgGetWriterCount() == 0, "All writers must finish before executing pending operations");

    if (m_pData != nullptr && !m_PersistentMapped)
    {
        VERIFY_EXPR(pContext != nullptr);
        pContext->UnmapBuffer(m_pStagingBuffer, MAP_WRITE);
        m_pData = nullptr;
    }

    for (PendingOp Op; m_PendingOps.Dequeue(Op);)
    {
        if (pContext != nullptr && Op.NumBytes > 0)
        {
            pContext->CopyBuffer(m_pStagingBuffer, Op.SrcOffset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                 Op.pDstBuffer, Op.DstOffset, Op.NumBytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        if (Op.Callback != nullptr)
        {
            Op.Callback(Op.pCallbackData);
        }
    }
    m_NumPendingOps.store(0);
    m_FenceValue = FenceValue;
}

void GPUUploadManagerImpl::Page::Reset(IDeviceContext* pContext)
{
    VERIFY(DbgGetWriterCount() == 0, "All writers must finish before resetting the page");
    VERIFY(m_PendingOps.IsEmpty(), "All pending operations must be executed before resetting the page");

    m_Offset.store(0);
    m_State.store(0);
    m_NumPendingOps.store(0);
    m_Enqueued.store(false);
    m_FenceValue = 0;

    if (pContext != nullptr)
    {
        if (!m_PersistentMapped)
        {
            pContext->MapBuffer(m_pStagingBuffer, MAP_WRITE, MAP_FLAG_NONE, m_pData);
        }
        VERIFY_EXPR(m_pData != nullptr);
    }
}

bool GPUUploadManagerImpl::Page::TryEnqueue()
{
    VERIFY(DbgIsSealed(), "Page must be sealed before it can be enqueued for execution");
    VERIFY(DbgGetWriterCount() == 0, "All writers must finish before the page can be enqueued");

    bool Expected = false;
    return m_Enqueued.compare_exchange_strong(Expected, true, std::memory_order_acq_rel);
}

void GPUUploadManagerImpl::Page::ReleaseStagingBuffer(IDeviceContext* pContext)
{
    if (m_pData != nullptr)
    {
        VERIFY_EXPR(pContext != nullptr);
        pContext->UnmapBuffer(m_pStagingBuffer, MAP_WRITE);
        m_pData = nullptr;
    }
    m_pStagingBuffer.Release();
}


GPUUploadManagerImpl::GPUUploadManagerImpl(IReferenceCounters* pRefCounters, const GPUUploadManagerCreateInfo& CI) :
    TBase{pRefCounters},
    m_PageSize{AlignUpToPowerOfTwo(CI.PageSize)},
    m_pDevice{CI.pDevice},
    m_pContext{CI.pContext}
{
    FenceDesc Desc;
    Desc.Name = "GPU upload manager fence";
    Desc.Type = FENCE_TYPE_CPU_WAIT_ONLY;
    m_pDevice->CreateFence(Desc, &m_pFence);
    VERIFY_EXPR(m_pFence != nullptr);

    m_pCurrentPage.store(CreatePage(CI.pContext), std::memory_order_release);
}

GPUUploadManagerImpl::~GPUUploadManagerImpl()
{
    for (std::unique_ptr<Page>& P : m_Pages)
    {
        P->ReleaseStagingBuffer(m_pContext);
    }
}

void GPUUploadManagerImpl::RenderThreadUpdate(IDeviceContext* pContext)
{
    DEV_CHECK_ERR(pContext == m_pContext, "The context passed to RenderThreadUpdate must be the same as the one used to create the GPUUploadManagerImpl");
    (void)m_NextFenceValue;
}

void GPUUploadManagerImpl::ScheduleBufferUpdate(IBuffer*                      pDstBuffer,
                                                Uint32                        DstOffset,
                                                Uint32                        NumBytes,
                                                const void*                   pSrcData,
                                                GPUUploadEnqueuedCallbackType Callback,
                                                void*                         pCallbackData)
{
}

GPUUploadManagerImpl::Page* GPUUploadManagerImpl::CreatePage(IDeviceContext* pContext, Uint32 MinSize)
{
    Uint32 PageSize = m_PageSize;
    while (PageSize < MinSize)
        PageSize *= 2;

    std::unique_ptr<Page> NewPage = std::make_unique<Page>(m_pDevice, pContext, PageSize);

    Page* P = NewPage.get();
    m_Pages.emplace_back(std::move(NewPage));

    return P;
}


void CreateGPUUploadManager(const GPUUploadManagerCreateInfo& CreateInfo,
                            IGPUUploadManager**               ppManager)
{
    if (ppManager == nullptr)
    {
        DEV_ERROR("ppManager must not be null");
        return;
    }

    *ppManager = MakeNewRCObj<GPUUploadManagerImpl>()(CreateInfo);
    if (*ppManager != nullptr)
    {
        (*ppManager)->AddRef();
    }
}

} // namespace Diligent
