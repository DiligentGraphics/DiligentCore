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

#pragma once

/// \file
/// Implementation of the asynchronous GPU upload manager.

#include "GPUUploadManager.h"
#include "Fence.h"
#include "ObjectBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "MPSCQueue.hpp"

#include <memory>
#include <vector>
#include <mutex>

namespace Diligent
{

/// Implementation of the asynchronous GPU upload manager.
class GPUUploadManagerImpl final : public ObjectBase<IGPUUploadManager>
{
public:
    using TBase = ObjectBase<IGPUUploadManager>;

    GPUUploadManagerImpl(IReferenceCounters* pRefCounters, const GPUUploadManagerCreateInfo& CI);
    ~GPUUploadManagerImpl();

    virtual void DILIGENT_CALL_TYPE RenderThreadUpdate(IDeviceContext* pContext) override final;

    virtual void DILIGENT_CALL_TYPE ScheduleBufferUpdate(IBuffer*                      pDstBuffer,
                                                         Uint32                        DstOffset,
                                                         Uint32                        NumBytes,
                                                         const void*                   pSrcData,
                                                         GPUUploadEnqueuedCallbackType Callback,
                                                         void*                         pCallbackData) override final;

    class Page
    {
    public:
        explicit Page(Uint32 Size, bool PersistentMapped = false) noexcept;

        Page(IRenderDevice*  pDevice,
             IDeviceContext* pContext,
             Uint32          Size);

        enum class WritingStatus
        {
            NotSealed,
            NotLastWriter,
            LastWriterSealed
        };

        class Writer
        {
        public:
            operator bool() const { return m_pPage != nullptr; }

            // clang-format off
            Writer           (const Writer&) = delete;
            Writer& operator=(const Writer&) = delete;
            Writer           (Writer&& Other) noexcept;
            Writer& operator=(Writer&& Other) = delete;
            // clang-format on

            bool ScheduleBufferUpdate(IBuffer*                      pDstBuffer,
                                      Uint32                        DstOffset,
                                      Uint32                        NumBytes,
                                      const void*                   pSrcData,
                                      GPUUploadEnqueuedCallbackType Callback,
                                      void*                         pCallbackData);

            WritingStatus EndWriting();

            ~Writer();

        private:
            friend Page;
            explicit Writer(Page* pPage) noexcept :
                m_pPage{pPage}
            {}

        private:
            Page* m_pPage = nullptr;
        };

        // Tries to begin writing to the page. Returns a valid Writer object if the
        // page is not sealed for new writes, and an empty Writer otherwise.
        Writer TryBeginWriting();

        enum class SealStatus
        {
            // The page is already sealed for new writes by somebody else.
            AlreadySealed,

            // The page was sealed for the first time, but there were active writers at the moment of
            // sealing, so the page is not ready for execution yet.
            NotReady,

            // The page was sealed for the first time and there were no active writers at the moment of
            // sealing, so the page is ready for execution.
            Ready
        };

        // Seals the page for new writes and returns the sealing status.
        SealStatus TrySeal();

        void ExecutePendingOps(IDeviceContext* pContext, Uint64 FenceValue);
        void Reset(IDeviceContext* pContext);

        // Tries to set the page as enqueued for execution.
        // Returns true if the page was not previously enqueued, false otherwise.
        bool TryEnqueue();

        Uint64 GetFenceValue() const { return m_FenceValue; }
        Uint32 GetSize() const { return m_Size; }

        // Returns the number of pending operations. This is used for testing and debugging purposes.
        size_t DbgGetNumPendingOps() const { return m_NumPendingOps.load(std::memory_order_relaxed); }

        // Returns the number of active writers. This is used for testing and debugging purposes.
        Uint32 DbgGetWriterCount() const { return m_State.load(std::memory_order_relaxed) & WRITER_MASK; }

        // Returns true if the page is sealed for new writes. This is used for testing and debugging purposes.
        bool DbgIsSealed() const { return (m_State.load(std::memory_order_relaxed) & SEALED_BIT) != 0; }

        void ReleaseStagingBuffer(IDeviceContext* pContext);

    private:
        // Schedules a buffer update operation on the page.
        // Returns true if the operation was successfully scheduled, and false otherwise.
        bool ScheduleBufferUpdate(IBuffer*                      pDstBuffer,
                                  Uint32                        DstOffset,
                                  Uint32                        NumBytes,
                                  const void*                   pSrcData,
                                  GPUUploadEnqueuedCallbackType Callback,
                                  void*                         pCallbackData);

        WritingStatus EndWriting();

    private:
        const Uint32 m_Size             = 0;
        const bool   m_PersistentMapped = false;

        RefCntAutoPtr<IBuffer> m_pStagingBuffer;

        void* m_pData = nullptr;

        std::atomic<Uint32> m_Offset{0};

        static constexpr Uint32 SEALED_BIT  = 0x80000000u;
        static constexpr Uint32 WRITER_MASK = ~SEALED_BIT; // low 31 bits
        std::atomic<Uint32>     m_State{0};

        std::atomic<size_t> m_NumPendingOps{0};
        std::atomic<bool>   m_Enqueued{false};

        Uint64 m_FenceValue = 0;

        struct PendingOp
        {
            RefCntAutoPtr<IBuffer> pDstBuffer;

            GPUUploadEnqueuedCallbackType Callback      = nullptr;
            void*                         pCallbackData = nullptr;

            Uint32 SrcOffset = 0;
            Uint32 DstOffset = 0;
            Uint32 NumBytes  = 0;

            PendingOp() noexcept = default;
        };

        MPSCQueue<PendingOp> m_PendingOps;
    };

private:
    void  ReclaimCompletedPages(IDeviceContext* pContext);
    bool  SealAndSwapCurrentPage(IDeviceContext* pContext);
    bool  TryEnqueuePage(Page* P);
    Page* AcquireFreePage(IDeviceContext* pContext);
    Page* CreatePage(IDeviceContext* pContext, Uint32 MinSize = 0);

private:
    const Uint32 m_PageSize;

    RefCntAutoPtr<IRenderDevice>  m_pDevice;
    RefCntAutoPtr<IDeviceContext> m_pContext;

    // Pages that are pending for execution.
    MPSCQueue<Page*> m_PendingPages;

    // Pages that are ready to be used for writing. They are already mapped.
    std::mutex         m_FreePagesMtx;
    std::vector<Page*> m_FreePages;
    std::vector<Page*> m_NewFreePages;

    // Pages that have been submitted for execution and are being processed by the GPU.
    std::vector<Page*> m_InFlightPages;
    std::vector<Page*> m_TmpInFlightPages;

    RefCntAutoPtr<IFence> m_pFence;
    Uint64                m_NextFenceValue = 1;

    std::atomic<Page*> m_pCurrentPage{nullptr};

    std::vector<std::unique_ptr<Page>> m_Pages;

    std::atomic<Uint32> m_MaxPendingUpdateSize{0};
    std::atomic<Uint32> m_TotalPendingUpdateSize{0};
};

} // namespace Diligent
