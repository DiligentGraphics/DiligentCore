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
#include "GraphicsAccessories.hpp"

#include <vector>
#include <cstring>
#include <thread>
#include <algorithm>

namespace Diligent
{

GPUUploadManagerImpl::Page::Writer::Writer(Writer&& Other) noexcept :
    m_pPage{Other.m_pPage}
{
    Other.m_pPage = nullptr;
}

bool GPUUploadManagerImpl::Page::Writer::ScheduleBufferUpdate(const ScheduleBufferUpdateInfo& UpdateInfo)
{
    if (m_pPage == nullptr)
    {
        UNEXPECTED("Attempting to schedule a buffer update with an invalid writer.");
        return false;
    }

    return m_pPage->ScheduleBufferUpdate(UpdateInfo);
}

bool GPUUploadManagerImpl::Page::Writer::ScheduleTextureUpdate(const ScheduleTextureUpdateInfo& UpdateInfo,
                                                               const BufferToTextureCopyInfo&   CopyInfo,
                                                               Uint32                           OffsetAlignment)
{
    if (m_pPage == nullptr)
    {
        UNEXPECTED("Attempting to schedule a buffer update with an invalid writer.");
        return false;
    }

    return m_pPage->ScheduleTextureUpdate(UpdateInfo, CopyInfo, OffsetAlignment);
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

GPUUploadManagerImpl::Page::Page(size_t StreamIndex, IRenderDevice* pDevice, Uint32 Size) :
    m_StreamIdx{StreamIndex},
    m_Size{Size},
    m_PersistentMapped{PersistentMapSupported(pDevice)}
{
    static std::atomic<int> PageCounter{0};
    const std::string       Name = std::string{"GPUUploadManagerImpl "} + GetMemorySizeString(Size) + " page " + std::to_string(PageCounter.fetch_add(1));

    BufferDesc Desc;
    Desc.Name           = Name.c_str();
    Desc.Size           = Size;
    Desc.Usage          = USAGE_STAGING;
    Desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(Desc, nullptr, &m_pStagingBuffer);
    VERIFY_EXPR(m_pStagingBuffer != nullptr);
}

GPUUploadManagerImpl::Page::~Page()
{
    for (PendingOp Op; m_PendingOps.Dequeue(Op);)
    {
        if (Op.index() == 0)
        {
            const PendingBufferOp& BuffOp = std::get<PendingBufferOp>(Op);
            if (BuffOp.CopyBuffer != nullptr)
            {
                BuffOp.CopyBuffer(nullptr, nullptr, ~0u, BuffOp.NumBytes, BuffOp.pCopyBufferData);
            }

            if (BuffOp.UploadEnqueued != nullptr)
            {
                BuffOp.UploadEnqueued(nullptr, BuffOp.DstOffset, BuffOp.NumBytes, BuffOp.pUploadEnqueuedData);
            }
        }
        else if (Op.index() == 1)
        {
            const PendingTextureOp& TexOp = std::get<PendingTextureOp>(Op);
            if (TexOp.CopyTexture != nullptr)
            {
                TexOp.CopyTexture(nullptr, TexOp.DstMipLevel, TexOp.DstSlice, TexOp.DstBox, TextureSubResData{}, TexOp.pCopyTextureData);
            }

            if (TexOp.UploadEnqueued != nullptr)
            {
                TexOp.UploadEnqueued(nullptr, TexOp.DstMipLevel, TexOp.DstSlice, TexOp.DstBox, TexOp.pUploadEnqueuedData);
            }
        }
        else
        {
            UNEXPECTED("Unexpected pending operation type");
        }
    }
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

void GPUUploadManagerImpl::Page::Unseal()
{
    VERIFY_EXPR(DbgGetWriterCount() == 0);
    m_State.fetch_and(WRITER_MASK, std::memory_order_release);
}

void GPUUploadManagerImpl::Page::Seal()
{
    VERIFY_EXPR(DbgGetWriterCount() == 0);
    m_State.fetch_or(SEALED_BIT, std::memory_order_release);
}

Uint32 GPUUploadManagerImpl::Page::Allocate(Uint32 NumBytes, Uint32 Alignment)
{
    const Uint32 AlignedSize = AlignUp(NumBytes, Alignment);
    for (;;)
    {
        Uint32 Offset        = m_Offset.load(std::memory_order_acquire);
        Uint32 AlignedOffset = AlignUp(Offset, Alignment);
        if (AlignedOffset + AlignedSize > m_Size)
            return ~0u; // Fail without incrementing offset

        if (m_Offset.compare_exchange_weak(Offset, AlignedOffset + AlignedSize, std::memory_order_acq_rel))
            return AlignedOffset; // Success
    }
}

bool GPUUploadManagerImpl::Page::ScheduleBufferUpdate(const ScheduleBufferUpdateInfo& UpdateInfo)
{
    VERIFY_EXPR(DbgGetWriterCount() > 0);

    // Note that the page may be sealed for new writes at this point,
    // but we can still schedule the update since we have an active writer
    // that prevents the page from being submitted for execution.

    Uint32 Offset = 0;
    if (UpdateInfo.NumBytes > 0)
    {
        Offset = Allocate(UpdateInfo.NumBytes, kMinimumOffsetAlignment);
        if (Offset == ~0u)
        {
            return false;
        }

        if (m_pData != nullptr)
        {
            Uint8* pDst = static_cast<Uint8*>(m_pData) + Offset;
            if (UpdateInfo.WriteDataCallback != nullptr)
            {
                UpdateInfo.WriteDataCallback(pDst, UpdateInfo.NumBytes, UpdateInfo.pWriteDataCallbackUserData);
            }
            else if (UpdateInfo.pSrcData != nullptr)
            {
                std::memcpy(pDst, UpdateInfo.pSrcData, UpdateInfo.NumBytes);
            }
        }
    }

    PendingBufferOp Op;
    Op.pDstBuffer      = UpdateInfo.pDstBuffer;
    Op.CopyBuffer      = UpdateInfo.CopyBuffer;
    Op.pCopyBufferData = UpdateInfo.pCopyBufferData;
    if (Op.CopyBuffer == nullptr)
    {
        Op.UploadEnqueued      = UpdateInfo.UploadEnqueued;
        Op.pUploadEnqueuedData = UpdateInfo.pUploadEnqueuedData;
    }
    Op.SrcOffset = Offset;
    Op.DstOffset = UpdateInfo.DstOffset;
    Op.NumBytes  = UpdateInfo.NumBytes;
    m_PendingOps.Enqueue(std::move(Op));

    return true;
}


bool GPUUploadManagerImpl::Page::ScheduleTextureUpdate(const ScheduleTextureUpdateInfo& UpdateInfo,
                                                       const BufferToTextureCopyInfo&   CopyInfo,
                                                       Uint32                           OffsetAlignment)
{
    VERIFY_EXPR(DbgGetWriterCount() > 0);

    // Note that the page may be sealed for new writes at this point,
    // but we can still schedule the update since we have an active writer
    // that prevents the page from being submitted for execution.

    Uint32 Offset = 0;
    if (CopyInfo.MemorySize > 0)
    {
        Offset = Allocate(static_cast<Uint32>(CopyInfo.MemorySize), std::max(kMinimumOffsetAlignment, OffsetAlignment));
        if (Offset == ~0u)
        {
            return false;
        }

        if (m_pData != nullptr)
        {
            Uint8* pDst = static_cast<Uint8*>(m_pData) + Offset;
            if (UpdateInfo.WriteDataCallback != nullptr)
            {
                UpdateInfo.WriteDataCallback(pDst, static_cast<Uint32>(CopyInfo.RowStride), static_cast<Uint32>(CopyInfo.DepthStride), UpdateInfo.DstBox, UpdateInfo.pWriteDataCallbackUserData);
            }
            else if (UpdateInfo.pSrcData != nullptr)
            {
                const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(UpdateInfo.pDstTexture->GetDesc().Format);
                const Uint32                NumRows    = AlignUp(UpdateInfo.DstBox.Height(), FmtAttribs.BlockHeight) / FmtAttribs.BlockHeight;
                for (Uint32 DepthSlice = 0; DepthSlice < UpdateInfo.DstBox.Depth(); ++DepthSlice)
                {
                    for (Uint32 row = 0; row < NumRows; ++row)
                    {
                        const size_t SrcRowOffset = static_cast<size_t>(DepthSlice * UpdateInfo.DepthStride + row * UpdateInfo.Stride);
                        const size_t DstRowOffset = static_cast<size_t>(DepthSlice * CopyInfo.DepthStride + row * CopyInfo.RowStride);
                        const void*  pSrcRow      = static_cast<const Uint8*>(UpdateInfo.pSrcData) + SrcRowOffset;
                        void*        pDstRow      = pDst + DstRowOffset;
                        std::memcpy(pDstRow, pSrcRow, static_cast<size_t>(CopyInfo.RowSize));
                    }
                }
            }
        }
    }

    PendingTextureOp Op;
    Op.pDstTexture      = UpdateInfo.pDstTexture;
    Op.CopyTexture      = UpdateInfo.CopyTexture;
    Op.pCopyTextureData = UpdateInfo.pCopyTextureData;
    Op.SrcOffset        = Offset;
    Op.SrcStride        = static_cast<Uint32>(CopyInfo.RowStride);
    Op.SrcDepthStride   = static_cast<Uint32>(CopyInfo.DepthStride);
    Op.DstMipLevel      = UpdateInfo.DstMipLevel;
    Op.DstSlice         = UpdateInfo.DstSlice;
    Op.DstBox           = UpdateInfo.DstBox;
    if (Op.CopyTexture == nullptr)
    {
        Op.UploadEnqueued      = UpdateInfo.UploadEnqueued;
        Op.pUploadEnqueuedData = UpdateInfo.pUploadEnqueuedData;
    }
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
        if (Op.index() == 0)
        {
            const PendingBufferOp& BuffOp = std::get<PendingBufferOp>(Op);
            if (BuffOp.CopyBuffer != nullptr)
            {
                BuffOp.CopyBuffer(pContext, m_pStagingBuffer, BuffOp.SrcOffset, BuffOp.NumBytes, BuffOp.pCopyBufferData);
            }
            else
            {
                if (pContext != nullptr && BuffOp.pDstBuffer != nullptr && BuffOp.NumBytes > 0)
                {
                    pContext->CopyBuffer(m_pStagingBuffer, BuffOp.SrcOffset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                         BuffOp.pDstBuffer, BuffOp.DstOffset, BuffOp.NumBytes, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                }

                if (BuffOp.UploadEnqueued != nullptr)
                {
                    BuffOp.UploadEnqueued(BuffOp.pDstBuffer, BuffOp.DstOffset, BuffOp.NumBytes, BuffOp.pUploadEnqueuedData);
                }
            }
        }
        else if (Op.index() == 1)
        {
            const PendingTextureOp& TexOp = std::get<PendingTextureOp>(Op);

            TextureSubResData SrcData;
            SrcData.pSrcBuffer  = m_pStagingBuffer;
            SrcData.SrcOffset   = TexOp.SrcOffset;
            SrcData.Stride      = TexOp.SrcStride;
            SrcData.DepthStride = TexOp.SrcDepthStride;

            if (TexOp.CopyTexture)
            {
                TexOp.CopyTexture(pContext, TexOp.DstMipLevel, TexOp.DstSlice, TexOp.DstBox, SrcData, TexOp.pCopyTextureData);
            }
            else
            {
                if (pContext != nullptr && TexOp.pDstTexture != nullptr)
                {
                    pContext->UpdateTexture(TexOp.pDstTexture, TexOp.DstMipLevel, TexOp.DstSlice, TexOp.DstBox, SrcData,
                                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                }

                if (TexOp.UploadEnqueued != nullptr)
                {
                    TexOp.UploadEnqueued(TexOp.pDstTexture, TexOp.DstMipLevel, TexOp.DstSlice,
                                         TexOp.DstBox, TexOp.pUploadEnqueuedData);
                }
            }
        }
    }
    m_FenceValue = FenceValue;
}

void GPUUploadManagerImpl::Page::Reset(IDeviceContext* pContext)
{
    VERIFY(DbgGetWriterCount() == 0, "All writers must finish before resetting the page");
    VERIFY(m_PendingOps.IsEmpty(), "All pending operations must be executed before resetting the page");

    m_Offset.store(0);
    // Keep the page sealed until we make it current to prevent any accidental writes.
    m_State.store(SEALED_BIT);
    m_Enqueued.store(false);
    m_FenceValue = 0;

    if (pContext != nullptr && m_pData == nullptr)
    {
        const MAP_FLAGS MapFlags = m_PersistentMapped ?
            MAP_FLAG_DO_NOT_WAIT :
            MAP_FLAG_NONE;
        pContext->MapBuffer(m_pStagingBuffer, MAP_WRITE, MapFlags, m_pData);
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

void GPUUploadManagerImpl::FreePages::Push(Page** ppPages, size_t NumPages)
{
    if (NumPages == 0)
        return;

    std::lock_guard<std::mutex> Guard{m_PagesMtx};

    size_t TotalAddedPages = 0;
    for (size_t i = 0; i < NumPages; ++i)
    {
        if (Page* pPage = ppPages[i])
        {
            const Uint32 PageSize = pPage->GetSize();
            m_PagesBySize[PageSize].emplace_back(pPage);
            ++TotalAddedPages;
        }
        else
        {
            UNEXPECTED("Null page pointer");
        }
    }
    m_Size.fetch_add(TotalAddedPages, std::memory_order_release);
}

GPUUploadManagerImpl::Page* GPUUploadManagerImpl::FreePages::Pop(Uint32 RequiredSize)
{
    Page* P = nullptr;
    {
        std::lock_guard<std::mutex> Guard{m_PagesMtx};
        // Find the first page that is large enough to accommodate the required size
        auto it = m_PagesBySize.lower_bound(RequiredSize);
        if (it != m_PagesBySize.end())
        {
            std::vector<Page*>& Pages = it->second;
            if (!Pages.empty())
            {
                P = Pages.back();
                Pages.pop_back();
                m_Size.fetch_sub(1, std::memory_order_release);
                if (Pages.empty())
                {
                    // Remove the entry from the map if there are no more pages of this size,
                    // so that lower_bound in future calls won't return this empty entry.
                    m_PagesBySize.erase(it);
                }
            }
            else
            {
                UNEXPECTED("Page size entry exists in the map, but there are no pages in the vector");
                m_PagesBySize.erase(it);
            }
        }
    }
    return P;
}

GPUUploadManagerImpl::UploadStream::UploadStream(GPUUploadManagerImpl& Mgr,
                                                 size_t                StreamIdx,
                                                 Uint32                PageSize,
                                                 Uint32                MaxPageCount,
                                                 Uint32                InitialPageCount) noexcept :
    m_Mgr{Mgr},
    m_StreamIdx{StreamIdx},
    m_PageSize{AlignUpToPowerOfTwo(PageSize)},
    // Ensure that the max page count is at least 2 to allow for double buffering, unless it's set to 0 which means no limit.
    m_MaxPageCount{MaxPageCount != 0 ? std::max(MaxPageCount, 2u) : 0}
{
    if (m_Mgr.m_pContext)
    {
        // Create at least one page.
        if (Page* pPage = CreatePage(m_Mgr.m_pContext))
        {
            pPage->Unseal();
            m_pCurrentPage.store(pPage, std::memory_order_release);
        }
        else
        {
            UNEXPECTED("Failed to create the initial page");
        }
    }

    // Create additional pages if requested.
    if (m_MaxPageCount > 0)
        InitialPageCount = std::min(InitialPageCount, m_MaxPageCount);
    while (m_Pages.size() < InitialPageCount)
    {
        if (Page* pPage = CreatePage(m_Mgr.m_pContext))
        {
            if (m_Mgr.m_pContext != nullptr)
            {
                // If a context is provided, we can immediately map the staging buffer and
                // prepare the page for use, so we push it to the free list.
                m_FreePages.Push(pPage);
            }
            else
            {
                // If no context is provided, the page needs to be mapped in Reset(),
                // so we add it to the list of in-flight pages.
                m_Mgr.m_InFlightPages.emplace_back(pPage);
            }
        }
        else
        {
            UNEXPECTED("Failed to create an initial page");
            break;
        }
    }
}

GPUUploadManagerImpl::GPUUploadManagerImpl(IReferenceCounters* pRefCounters, const GPUUploadManagerCreateInfo& CI) :
    TBase{pRefCounters},
    m_pDevice{CI.pDevice},
    m_pContext{CI.pContext},
    m_TextureUpdateOffsetAlignment{m_pDevice->GetAdapterInfo().Buffer.TextureUpdateOffsetAlignment},
    m_TextureUpdateStrideAlignment{m_pDevice->GetAdapterInfo().Buffer.TextureUpdateStrideAlignment},
    m_Streams{
        UploadStream{
            *this,
            0,
            CI.PageSize,
            CI.MaxPageCount,
            CI.InitialPageCount,
        },
        UploadStream{
            *this,
            1,
            std::max(CI.LargePageSize, CI.PageSize * 2),
            CI.MaxLargePageCount,
            CI.InitialLargePageCount,
        },
    }
{
    FenceDesc Desc;
    Desc.Name = "GPU upload manager fence";
    Desc.Type = FENCE_TYPE_CPU_WAIT_ONLY;
    m_pDevice->CreateFence(Desc, &m_pFence);
    VERIFY_EXPR(m_pFence != nullptr);
}

void GPUUploadManagerImpl::UploadStream::ReleaseStagingBuffers()
{
    for (auto& it : m_Pages)
    {
        it.second->ReleaseStagingBuffer(m_Mgr.m_pContext);
    }
}

void GPUUploadManagerImpl::UploadStream::SignalStop()
{
    m_PageRotatedSignal.RequestStop();
}

GPUUploadManagerImpl::~GPUUploadManagerImpl()
{
    m_Stopping.store(true, std::memory_order_release);
    for (UploadStream& Stream : m_Streams)
    {
        Stream.SignalStop();
    }

    // Wait for any running updates to finish.
    if (m_NumRunningUpdates.load(std::memory_order_acquire) > 0)
    {
        m_LastRunningThreadFinishedSignal.Wait();
    }

    for (UploadStream& Stream : m_Streams)
    {
        Stream.ReleaseStagingBuffers();
    }
}

void GPUUploadManagerImpl::RenderThreadUpdate(IDeviceContext* pContext)
{
    DEV_CHECK_ERR(pContext != nullptr, "A valid context must be provided to RenderThreadUpdate");

    if (!m_pContext)
    {
        // If no context was provided at creation, we can accept any context in RenderThreadUpdate, but it must be the same across calls.
        m_pContext = pContext;
    }
    else
    {
        DEV_CHECK_ERR(pContext == m_pContext, "The context provided to RenderThreadUpdate must be the same as the one used to create the GPUUploadManagerImpl");
    }

    // 1. Reclaim completed pages to make them available.
    ReclaimCompletedPages(pContext);

    // 2. Add free pages to accommodate pending updates.
    for (UploadStream& Stream : m_Streams)
    {
        Stream.AddFreePages(pContext);
    }

    // 3. Seal and swap the current page.
    for (UploadStream& Stream : m_Streams)
    {
        Stream.SealAndSwapCurrentPage(pContext);
    }

    // 4. Process pending pages and move them to in-fligt list.
    ProcessPendingPages(pContext);

    // 5. Lastly, process pages to release. Do this at the very end so that newly available free pages
    //    first get a chance to be used for pending updates.
    for (UploadStream& Stream : m_Streams)
    {
        Stream.ProcessPagesToRelease(pContext);
    }

    pContext->EnqueueSignal(m_pFence, m_NextFenceValue++);

    for (UploadStream& Stream : m_Streams)
    {
        Stream.SignalPageRotated();
    }
}

GPUUploadManagerImpl::UploadStream& GPUUploadManagerImpl::GetStreamForUpdateSize(Uint32 UpdateSize)
{
    const Uint32 LargeUpdateThreshold = m_Streams[static_cast<size_t>(UploadStreamType::Normal)].GetPageSize();
    return m_Streams[static_cast<size_t>(UpdateSize > LargeUpdateThreshold ? UploadStreamType::Large : UploadStreamType::Normal)];
}

void GPUUploadManagerImpl::UploadStream::ScheduleUpdate(IDeviceContext* pContext,
                                                        Uint32          UpdateSize,
                                                        const void*     pUpdateInfo,
                                                        bool            ScheduleUpdate(Page::Writer& Writer, const void* pUpdateInfo))
{
    DEV_CHECK_ERR(pContext == nullptr || pContext == m_Mgr.m_pContext,
                  "If a context is provided to ScheduleBufferUpdate/ScheduleTextureUpdate, it must be the same as the "
                  "one used to create the GPUUploadManagerImpl");

    class RunningUpdatesGuard
    {
    public:
        explicit RunningUpdatesGuard(UploadStream& Stream) noexcept :
            m_Stream{Stream}
        {
            m_Stream.m_Mgr.m_NumRunningUpdates.fetch_add(1, std::memory_order_acq_rel);
            m_Stream.m_NumRunningUpdates.fetch_add(1, std::memory_order_acq_rel);
        }

        ~RunningUpdatesGuard()
        {
            // Decrement the stream's counter first because if the manager is stopping, the
            // stream can be destroyed after m_LastRunningThreadFinishedSignal is triggered.
            m_Stream.m_NumRunningUpdates.fetch_sub(1, std::memory_order_acq_rel);
            Uint32 NumRunningUpdates = m_Stream.m_Mgr.m_NumRunningUpdates.fetch_sub(1, std::memory_order_acq_rel);
            if (m_Stream.m_Mgr.m_Stopping.load(std::memory_order_acquire) && NumRunningUpdates == 1)
            {
                m_Stream.m_Mgr.m_LastRunningThreadFinishedSignal.Trigger();
            }
        }

    private:
        UploadStream& m_Stream;
    };
    RunningUpdatesGuard Guard{*this};


    bool IsFirstAttempt = true;
    bool AbortUpdate    = false;

    auto UpdatePendingSizeAndTryRotate = [&](Page* P) {
        Uint64 PageEpoch = m_PageRotatedSignal.CurrentEpoch();
        if (!TryRotatePage(pContext, P, UpdateSize))
        {
            // Atomically update the max pending update size to ensure the next page is large enough
            AtomicMax(m_MaxPendingUpdateSize, UpdateSize, std::memory_order_acq_rel);
            if (IsFirstAttempt)
            {
                m_TotalPendingUpdateSize.fetch_add(UpdateSize, std::memory_order_acq_rel);
                IsFirstAttempt = false;
            }
            AbortUpdate = m_PageRotatedSignal.WaitNext(PageEpoch) == 0;
        }
    };

    while (!AbortUpdate && !m_Mgr.m_Stopping.load(std::memory_order_acquire))
    {
        Page* P = m_pCurrentPage.load(std::memory_order_acquire);
        if (P == nullptr)
        {
            // No current page, wait for a page to become available
            UpdatePendingSizeAndTryRotate(P);
            continue;
        }

        Page::Writer Writer = P->TryBeginWriting();
        if (!Writer)
        {
            // The page is sealed, so we need to rotate to a new page and try again
            UpdatePendingSizeAndTryRotate(P);
            continue;
        }

        auto EndWriting = [&]() {
            if (Writer.EndWriting() == Page::WritingStatus::LastWriterSealed)
            {
                // We were the last writer - enqueue the page whether the update was successfully
                // scheduled or not, because the page is sealed and can't be written to anymore.
                TryEnqueuePage(P);
            }
        };

        // Prevent ABA-style bug when pages are reset and reused by validating that the page is still current.
        if (P != m_pCurrentPage.load(std::memory_order_acquire))
        {
            EndWriting();
            // Reload current and retry
            continue;
        }

        const bool UpdateScheduled = ScheduleUpdate(Writer, pUpdateInfo);
        EndWriting();

        if (UpdateScheduled)
        {
            break;
        }
        else
        {
            UpdatePendingSizeAndTryRotate(P);
        }
    }

    AtomicMax(m_PeakUpdateSize, UpdateSize, std::memory_order_relaxed);
}

void GPUUploadManagerImpl::ScheduleBufferUpdate(const ScheduleBufferUpdateInfo& UpdateInfo)
{
    UploadStream& Stream = GetStreamForUpdateSize(UpdateInfo.NumBytes);
    Stream.ScheduleUpdate(UpdateInfo.pContext, UpdateInfo.NumBytes, &UpdateInfo,
                          [](Page::Writer& Writer, const void* pUpdateData) {
                              const ScheduleBufferUpdateInfo* pBufferUpdateInfo = static_cast<const ScheduleBufferUpdateInfo*>(pUpdateData);
                              return Writer.ScheduleBufferUpdate(*pBufferUpdateInfo);
                          });
}

void GPUUploadManagerImpl::ScheduleTextureUpdate(const ScheduleTextureUpdateInfo& UpdateInfo)
{
    if (UpdateInfo.pDstTexture == nullptr)
    {
        DEV_ERROR("Dst texture must not be null");
        return;
    }

    const TextureDesc& TexDesc = UpdateInfo.pDstTexture->GetDesc();

    struct ScheduleUpdateData
    {
        const ScheduleTextureUpdateInfo& UpdateInfo;
        const BufferToTextureCopyInfo    CopyInfo;
        const Uint32                     OffsetAlignment;
    };
    ScheduleUpdateData UpdateData{
        UpdateInfo,
        GetBufferToTextureCopyInfo(TexDesc.Format, UpdateInfo.DstBox, m_TextureUpdateStrideAlignment),
        m_TextureUpdateOffsetAlignment,
    };

    UploadStream& Stream = GetStreamForUpdateSize(static_cast<Uint32>(UpdateData.CopyInfo.MemorySize));
    Stream.ScheduleUpdate(UpdateInfo.pContext, static_cast<Uint32>(UpdateData.CopyInfo.MemorySize), &UpdateData,
                          [](Page::Writer& Writer, const void* pData) {
                              const ScheduleUpdateData* pUpdateData = static_cast<const ScheduleUpdateData*>(pData);
                              return Writer.ScheduleTextureUpdate(pUpdateData->UpdateInfo, pUpdateData->CopyInfo, pUpdateData->OffsetAlignment);
                          });
}

GPUUploadManagerImpl::Page* GPUUploadManagerImpl::UploadStream::CreatePage(IDeviceContext* pContext, Uint32 RequiredSize)
{
    const Uint32 MaxExistingPageSize = m_PageSizeToCount.empty() ? 0 : m_PageSizeToCount.rbegin()->first;
    if (m_MaxPageCount != 0 && m_Pages.size() >= m_MaxPageCount && RequiredSize <= MaxExistingPageSize)
    {
        return nullptr;
    }

    Uint32 PageSize = m_PageSize;
    while (PageSize < RequiredSize)
        PageSize *= 2;

    std::unique_ptr<Page> NewPage = std::make_unique<Page>(m_StreamIdx, m_Mgr.m_pDevice, PageSize);

    Page* P = NewPage.get();
    if (pContext != nullptr)
    {
        P->Reset(pContext);
    }
    m_Pages.emplace(P, std::move(NewPage));
    m_PageSizeToCount[PageSize]++;
    m_PeakPageCount = std::max(m_PeakPageCount, static_cast<Uint32>(m_Pages.size()));

    return P;
}


bool GPUUploadManagerImpl::UploadStream::SealAndSwapCurrentPage(IDeviceContext* pContext)
{
    VERIFY_EXPR(pContext != nullptr);

    // Get a fresh page (from free-list or allocate)
    Page* pFreshPage = AcquireFreePage(pContext);

    // Swap it in
    Page* pOld = m_pCurrentPage.exchange(pFreshPage, std::memory_order_acq_rel);

    // Seal old page and enqueue if no writers; otherwise last writer will enqueue it
    if (pOld != nullptr && pOld->TrySeal() == Page::SealStatus::Ready)
    {
        TryEnqueuePage(pOld);
    }

    return true;
}


bool GPUUploadManagerImpl::UploadStream::TryRotatePage(IDeviceContext* pContext, Page* ExpectedCurrent, Uint32 RequiredSize)
{
    // Grab a free page (workers can't create, so pContext=null)
    Page* Fresh = AcquireFreePage(pContext, RequiredSize);
    if (!Fresh)
        return false;

    Page* Cur = ExpectedCurrent;
    if (!m_pCurrentPage.compare_exchange_strong(Cur, Fresh, std::memory_order_acq_rel))
    {
        // Lost the race: put Fresh back
        Fresh->Seal();
        m_FreePages.Push(Fresh);
        return true; // Rotation happened by someone else
    }

    // We won: seal and enqueue if no writers
    if (ExpectedCurrent != nullptr && ExpectedCurrent->TrySeal() == Page::SealStatus::Ready)
        TryEnqueuePage(ExpectedCurrent);

    m_PageRotatedSignal.Tick();
    return true;
}


bool GPUUploadManagerImpl::UploadStream::TryEnqueuePage(Page* P)
{
    VERIFY_EXPR(P->DbgIsSealed());
    if (P->TryEnqueue())
    {
        if (P->GetNumPendingOps() > 0)
        {
            m_Mgr.m_PendingPages.Enqueue(P);
        }
        else
        {
            P->Reset(nullptr);
            m_FreePages.Push(P);
        }
        return true;
    }
    return false;
}

void GPUUploadManagerImpl::ReclaimCompletedPages(IDeviceContext* pContext)
{
    VERIFY_EXPR(pContext != nullptr);

    Uint64 CompletedFenceValue = m_pFence->GetCompletedValue();

    m_TmpPages.clear();
    size_t NewInFlightPageCount = 0;
    for (size_t i = 0; i < m_InFlightPages.size(); ++i)
    {
        Page* P = m_InFlightPages[i];
        if (P->GetFenceValue() <= CompletedFenceValue)
        {
            P->Reset(pContext);
            m_TmpPages.push_back(P);
        }
        else
        {
            m_InFlightPages[NewInFlightPageCount++] = P;
        }
    }
    VERIFY_EXPR(NewInFlightPageCount + m_TmpPages.size() == m_InFlightPages.size());
    m_InFlightPages.resize(NewInFlightPageCount);

    if (!m_TmpPages.empty())
    {
        for (Page* P : m_TmpPages)
        {
            m_Streams[P->GetStreamIndex()].AddFreePage(P);
        }
        m_TmpPages.clear();
    }
}


void GPUUploadManagerImpl::UploadStream::AddFreePages(IDeviceContext* pContext)
{
    VERIFY_EXPR(pContext != nullptr);

    const Uint32 TotalPendingSize = m_TotalPendingUpdateSize.exchange(0, std::memory_order_acq_rel);
    const Uint32 MinimalPageCount = std::max((TotalPendingSize + m_PageSize - 1) / m_PageSize, 1u);

    m_PeakTotalPendingUpdateSize = std::max(m_PeakTotalPendingUpdateSize, TotalPendingSize);

    const Uint32 NumFreePages = static_cast<Uint32>(m_FreePages.Size());

    Uint32 NumPagesToCreate = MinimalPageCount > NumFreePages ? MinimalPageCount - NumFreePages : 0;
    if (m_MaxPageCount != 0 && m_Pages.size() + NumPagesToCreate > m_MaxPageCount)
    {
        NumPagesToCreate = m_MaxPageCount > m_Pages.size() ? m_MaxPageCount - static_cast<Uint32>(m_Pages.size()) : 0;
    }

    if (NumPagesToCreate > 0)
    {
        m_Mgr.m_TmpPages.clear();
        for (Uint32 i = 0; i < NumPagesToCreate; ++i)
        {
            if (Page* pPage = CreatePage(pContext))
            {
                m_Mgr.m_TmpPages.push_back(pPage);
            }
            else
            {
                UNEXPECTED("Failed to create a new page");
                break;
            }
        }
        m_FreePages.Push(m_Mgr.m_TmpPages.data(), m_Mgr.m_TmpPages.size());
        m_Mgr.m_TmpPages.clear();
    }
}


void GPUUploadManagerImpl::UploadStream::ProcessPagesToRelease(IDeviceContext* pContext)
{
    if (m_MaxPageCount == 0)
        return;

    if (m_NumRunningUpdates.load(std::memory_order_acquire) > 0)
    {
        // Delay releasing pages until there are no running updates, to avoid ABA issue in ScheduleBufferUpdate:
        // * T1:
        //      Page* P = m_pCurrentPage.load(std::memory_order_acquire);
        // * T1 gets descheduled
        // * Render thread frees the page.
        // * T1 resumes and crashes at
        //      Page::Writer Writer = P->TryBeginWriting();
        return;
    }

    VERIFY_EXPR(pContext != nullptr);
    while (m_Pages.size() > m_MaxPageCount)
    {
        // Pop the smallest free page and release it until we are within the limit.
        if (Page* pPage = m_FreePages.Pop())
        {
            pPage->ReleaseStagingBuffer(pContext);
            auto it = m_PageSizeToCount.find(pPage->GetSize());
            if (it != m_PageSizeToCount.end())
            {
                if (--(it->second) == 0)
                    m_PageSizeToCount.erase(it);
            }
            else
            {
                UNEXPECTED("Page size not found in the map");
            }
            m_Pages.erase(pPage);
        }
        else
        {
            break; // No more free pages to remove
        }
    }
}

void GPUUploadManagerImpl::ProcessPendingPages(IDeviceContext* pContext)
{
    VERIFY_EXPR(pContext != nullptr);

    Page* ReadyPage = nullptr;
    while (m_PendingPages.Dequeue(ReadyPage))
    {
        ReadyPage->ExecutePendingOps(pContext, m_NextFenceValue);
        m_InFlightPages.push_back(ReadyPage);
    }
}

GPUUploadManagerImpl::Page* GPUUploadManagerImpl::UploadStream::AcquireFreePage(IDeviceContext* pContext, Uint32 RequiredSize)
{
    Uint32 MaxPendingUpdateSize = std::max(m_MaxPendingUpdateSize.load(std::memory_order_acquire), RequiredSize);

    Page* P = m_FreePages.Pop(MaxPendingUpdateSize);
    if (P == nullptr && pContext != nullptr)
    {
        P = CreatePage(pContext, MaxPendingUpdateSize);
    }

    if (P != nullptr)
    {
        // Unseal the page only when we make it current to prevent any accidental writes in scenario like this:
        // * Thread T1 runs GPUUploadManagerImpl::ScheduleBufferUpdate:
        //   * loads P0 as current, then gets descheduled before TryBeginWriting().
        // * Render thread swaps current to P1, seals P0, sees it has 0 ops, calls Reset(nullptr), and pushes P0 to FreePages
        // * T1 resumes and calls P0->TryBeginWriting() and succeeds (if SEALED_BIT is cleared).
        // * T1 schedules an update into a page that is not current and is simultaneously sitting in the free list,
        //   potentially being popped/installed elsewhere.
        P->Unseal();

        // Note that we MUST unseal the page BEFORE we make it current because otherwise
        // GPUUploadManagerImpl::ScheduleBufferUpdate may find a sealed page and try to
        // rotate it.

        const Uint32 PageSize = P->GetSize();
        // Clear if the page is larger than the max pending update
        for (;;)
        {
            if (PageSize < MaxPendingUpdateSize)
                break;
            if (m_MaxPendingUpdateSize.compare_exchange_weak(MaxPendingUpdateSize, 0, std::memory_order_acq_rel))
                break;
        }
    }

    return P;
}

void GPUUploadManagerImpl::UploadStream::GetStats(GPUUploadManagerStreamStats& Stats) const
{
    m_BucketInfo.clear();
    for (const auto& it : m_PageSizeToCount)
    {
        m_BucketInfo.emplace_back(GPUUploadManagerBucketInfo{it.first, it.second});
    }

    Stats.PageSize                   = m_PageSize;
    Stats.NumPages                   = static_cast<Uint32>(m_Pages.size());
    Stats.NumFreePages               = static_cast<Uint32>(m_FreePages.Size());
    Stats.PeakNumPages               = m_PeakPageCount;
    Stats.PeakTotalPendingUpdateSize = m_PeakTotalPendingUpdateSize;
    Stats.PeakUpdateSize             = m_PeakUpdateSize.load(std::memory_order_relaxed);
    Stats.NumBuckets                 = static_cast<Uint32>(m_BucketInfo.size());
    Stats.pBucketInfo                = m_BucketInfo.data();
}

void GPUUploadManagerImpl::GetStats(GPUUploadManagerStats& Stats) const
{
    for (size_t i = 0; i < m_Streams.size(); ++i)
    {
        m_Streams[i].GetStats(m_StreamStats[i]);
    }

    Stats.pStreamStats = m_StreamStats.data();
    Stats.NumStreams   = static_cast<Uint32>(m_Streams.size());

    Stats.NumInFlightPages = static_cast<Uint32>(m_InFlightPages.size());
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

std::string GetGPUUploadManagerStatsString(const GPUUploadManagerStats& MgrStats)
{
    std::stringstream ss;
    for (Uint32 stream = 0; stream < MgrStats.NumStreams; ++stream)
    {
        const GPUUploadManagerStreamStats& StreamStats = MgrStats.pStreamStats[stream];
        ss << "Stream " << GetMemorySizeString(StreamStats.PageSize) << std::endl
           << "  NumPages: " << StreamStats.NumPages << std::endl;

        for (Uint32 i = 0; i < StreamStats.NumBuckets; ++i)
        {
            ss << "      ";
            const GPUUploadManagerBucketInfo& BucketInfo = StreamStats.pBucketInfo[i];
            ss << GetMemorySizeString(BucketInfo.PageSize) << ": ";
            for (Uint32 j = 0; j < BucketInfo.NumPages; ++j)
            {
                ss << '#';
            }
            ss << " " << std::to_string(BucketInfo.NumPages) << std::endl;
        }

        ss << "  NumFreePages: " << StreamStats.NumFreePages << std::endl
           << "  PeakNumPages: " << StreamStats.PeakNumPages << std::endl
           << "  PeakTotalPendingUpdateSize: " << GetMemorySizeString(StreamStats.PeakTotalPendingUpdateSize) << std::endl
           << "  PeakUpdateSize: " << GetMemorySizeString(StreamStats.PeakUpdateSize) << std::endl;
    }

    ss << "NumInFlightPages: " << MgrStats.NumInFlightPages << std::endl;

    return ss.str();
}

} // namespace Diligent

extern "C"
{
    void Diligent_CreateGPUUploadManager(const Diligent::GPUUploadManagerCreateInfo& CreateInfo, Diligent::IGPUUploadManager** ppManager)
    {
        Diligent::CreateGPUUploadManager(CreateInfo, ppManager);
    }
}
