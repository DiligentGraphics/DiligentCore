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
#include "ThreadSignal.hpp"
#include "DynamicAtlasManager.hpp"

#include <memory>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <unordered_map>
#include <variant>

namespace Diligent
{

struct BufferToTextureCopyInfo;

/// Implementation of the asynchronous GPU upload manager.
class GPUUploadManagerImpl final : public ObjectBase<IGPUUploadManager>
{
public:
    using TBase = ObjectBase<IGPUUploadManager>;

    GPUUploadManagerImpl(IReferenceCounters* pRefCounters, const GPUUploadManagerCreateInfo& CI);
    ~GPUUploadManagerImpl();

    virtual void DILIGENT_CALL_TYPE RenderThreadUpdate(IDeviceContext* pContext) override final;

    virtual void DILIGENT_CALL_TYPE ScheduleBufferUpdate(const ScheduleBufferUpdateInfo& UpdateInfo) override final;

    virtual void DILIGENT_CALL_TYPE ScheduleTextureUpdate(const ScheduleTextureUpdateInfo& UpdateInfo) override final;

    virtual void DILIGENT_CALL_TYPE GetStats(GPUUploadManagerStats& Stats) override final;

private:
    class UploadStream;

public:
    class Page
    {
    public:
        explicit Page(Uint32 Size, bool PersistentMapped = false) noexcept;

        Page(UploadStream& Stream, IRenderDevice* pDevice, Uint32 Size);

        // Special page for texture updates in Direct3D11, which require a staging texture.
        // The texture dimensions are Size x Size.
        Page(UploadStream& Stream, IRenderDevice* pDevice, Uint32 Size, TEXTURE_FORMAT Format);

        ~Page();

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

            bool ScheduleBufferUpdate(const ScheduleBufferUpdateInfo& UpdateInfo);
            bool ScheduleTextureUpdate(const ScheduleTextureUpdateInfo& UpdateInfo,
                                       const BufferToTextureCopyInfo&   CopyInfo,
                                       Uint32                           OffsetAlignment);

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

        // Tries to atomically begin writing to the page:
        // - If the page is not sealed for new writes, increments the writer count and returns a valid Writer object.
        // - If the page is sealed for new writes, returns an empty Writer object.
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
        void       Unseal();
        void       Seal();

        void ExecutePendingOps(IDeviceContext* pContext, Uint64 FenceValue);
        void Reset(IDeviceContext* pContext);

        // Tries to set the page as enqueued for execution.
        // Returns true if the page was not previously enqueued, false otherwise.
        bool TryEnqueue();

        // Return the page to the stream's pool of free pages.
        void Recycle();

        Uint64 GetFenceValue() const { return m_FenceValue; }
        Uint32 GetSize() const { return m_Size; }

        // Returns the number of pending operations.
        size_t GetNumPendingOps() const { return m_PendingOps.Size(); }

#ifdef DILIGENT_DEBUG
        // Returns the number of active writers. This is used for testing and debugging purposes.
        Uint32 DbgGetWriterCount() const { return m_State.load(std::memory_order_acquire) & WRITER_MASK; }

        // Returns true if the page is sealed for new writes. This is used for testing and debugging purposes.
        bool DbgIsSealed() const { return (m_State.load(std::memory_order_acquire) & SEALED_BIT) != 0; }
#endif

        void ReleaseStagingBuffer(IDeviceContext* pContext);

    private:
        // Schedules a buffer update operation on the page.
        // Returns true if the operation was successfully scheduled, and false otherwise.
        bool ScheduleBufferUpdate(const ScheduleBufferUpdateInfo& UpdateInfo);

        // Schedules a texture update operation on the page.
        // Returns true if the operation was successfully scheduled, and false otherwise.
        bool ScheduleTextureUpdate(const ScheduleTextureUpdateInfo& UpdateInfo,
                                   const BufferToTextureCopyInfo&   CopyInfo,
                                   Uint32                           OffsetAlignment);

        // Allocates a block of memory from the page for a new update operation.
        // Returns the offset of the allocated block within the page.
        // If there is not enough space in the page for the requested allocation, returns ~0u.
        Uint32 Allocate(Uint32 NumBytes, Uint32 Alignment);

        WritingStatus EndWriting();

        void UnmapStagingResource(IDeviceContext* pContext);

    private:
        UploadStream* const m_pStream = nullptr;

        const Uint32 m_Size             = 0;
        const bool   m_PersistentMapped = false;

        RefCntAutoPtr<IBuffer> m_pStagingBuffer;

        // Direct3D11 does not support buffer to texture copies, so we need a
        // staging texture to perform texture updates.
        struct StagingTextureAtlas
        {
            StagingTextureAtlas(IRenderDevice* pDevice, Uint32 Width, Uint32 Height, TEXTURE_FORMAT Format, const std::string& Name);
            ~StagingTextureAtlas();

            void* Map(IDeviceContext* pContext);
            void  Unmap(IDeviceContext* pContext);
            void  Reset();

            DynamicAtlasManager::Region Allocate(Uint32 Width, Uint32 Height);

            RefCntAutoPtr<ITexture> pTex;

            Uint32 RowStride   = 0;
            Uint32 DepthStride = 0;

        private:
            std::mutex          MgrMtx;
            DynamicAtlasManager Mgr;
        };
        std::unique_ptr<StagingTextureAtlas> m_pStagingAtlas;

        void* m_pData = nullptr;

        std::atomic<Uint32> m_Offset{0};

        static constexpr Uint32 SEALED_BIT  = 0x80000000u;
        static constexpr Uint32 WRITER_MASK = ~SEALED_BIT; // low 31 bits
        std::atomic<Uint32>     m_State{0};

        std::atomic<bool> m_Enqueued{false};

        Uint64 m_FenceValue = 0;

        static constexpr Uint32 kMinimumOffsetAlignment = 16;

        struct PendingBufferOp
        {
            RefCntAutoPtr<IBuffer> pDstBuffer;

            CopyStagingBufferCallbackType CopyBuffer      = nullptr;
            void*                         pCopyBufferData = nullptr;

            GPUBufferUploadEnqueuedCallbackType UploadEnqueued      = nullptr;
            void*                               pUploadEnqueuedData = nullptr;

            RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;

            Uint32 SrcOffset = 0;
            Uint32 DstOffset = 0;
            Uint32 NumBytes  = 0;

            PendingBufferOp() noexcept = default;
        };

        struct PendingTextureOp
        {
            RefCntAutoPtr<ITexture> pDstTexture;

            RESOURCE_STATE_TRANSITION_MODE DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;

            // For Direct3D11, coordinates of the source region within the staging texture.
            Uint32 SrcX = 0;
            Uint32 SrcY = 0;

            Uint32 SrcOffset      = 0;
            Uint32 SrcStride      = 0;
            Uint32 SrcDepthStride = 0;

            Uint32 DstMipLevel = 0;
            Uint32 DstSlice    = 0;
            Box    DstBox;

            CopyStagingTextureCallbackType      CopyTexture      = nullptr;
            CopyStagingD3D11TextureCallbackType CopyD3D11Texture = nullptr;
            void*                               pCopyTextureData = nullptr;

            GPUTextureUploadEnqueuedCallbackType UploadEnqueued      = nullptr;
            void*                                pUploadEnqueuedData = nullptr;
        };

        using PendingOp = std::variant<PendingBufferOp, PendingTextureOp>;
        MPSCQueue<PendingOp> m_PendingOps;

        static std::atomic<Uint32> sm_PageCounter;
    };

private:
    void ReclaimCompletedPages(IDeviceContext* pContext);
    void ProcessPendingPages(IDeviceContext* pContext);

    UploadStream& GetStreamForUpdateSize(Uint32 UpdateSize);

private:
    RefCntAutoPtr<IRenderDevice>  m_pDevice;
    RefCntAutoPtr<IDeviceContext> m_pContext;

    const Uint32 m_TextureUpdateOffsetAlignment;
    const Uint32 m_TextureUpdateStrideAlignment;

    // Pages that are pending for execution.
    MPSCQueue<Page*> m_PendingPages;

    // Pages that are ready to be used for writing. They are already mapped.
    class FreePages
    {
    public:
        void   Push(Page** ppPages, size_t NumPages);
        void   Push(Page* pPage) { Push(&pPage, 1); }
        Page*  Pop(Uint32 MinSize = 0);
        size_t Size() const { return m_Size.load(std::memory_order_acquire); }

    private:
        std::mutex                           m_PagesMtx;
        std::map<Uint32, std::vector<Page*>> m_PagesBySize;
        std::atomic<size_t>                  m_Size{0};
    };

    // Pages that have been submitted for execution and are being processed by the GPU.
    std::vector<Page*> m_InFlightPages;
    std::vector<Page*> m_TmpPages;

    RefCntAutoPtr<IFence> m_pFence;
    Uint64                m_NextFenceValue = 1;

    class UploadStream
    {
    public:
        UploadStream(GPUUploadManagerImpl& Mgr,
                     IDeviceContext*       pContext,
                     Uint32                PageSize,
                     Uint32                MaxPageCount,
                     Uint32                InitialPageCount,
                     TEXTURE_FORMAT        Format = TEX_FORMAT_UNKNOWN) noexcept;

        Page* CreatePage(IDeviceContext* pContext, Uint32 RequiredSize = 0);
        Page* AcquireFreePage(IDeviceContext* pContext, Uint32 RequiredSize = 0);
        bool  SealAndSwapCurrentPage(IDeviceContext* pContext);
        bool  TryRotatePage(IDeviceContext* pContext, Page* ExpectedCurrent, Uint32 RequiredSize);
        bool  TryEnqueuePage(Page* P);
        void  ProcessPagesToRelease(IDeviceContext* pContext);
        void  AddFreePages(IDeviceContext* pContext);
        void  AddFreePage(Page* pPage) { m_FreePages.Push(pPage); }

        void ScheduleUpdate(IDeviceContext* pContext,
                            Uint32          UpdateSize,
                            const void*     pUpdateInfo,
                            bool            ScheduleUpdate(Page::Writer& Writer, const void* pUpdateInfo));
        void ReleaseStagingBuffers();
        void SignalPageRotated() { m_PageRotatedSignal.Tick(); }
        void SignalStop();

        Uint32 GetPageSize() const { return m_PageSize; }

        void GetStats(GPUUploadManagerStreamStats& Stats) const;

    private:
        GPUUploadManagerImpl& m_Mgr;
        const Uint32          m_PageSize;
        const Uint32          m_MaxPageCount;
        const TEXTURE_FORMAT  m_Format;

        std::atomic<Page*> m_pCurrentPage{nullptr};

        Threading::TickSignal m_PageRotatedSignal;

        std::unordered_map<Page*, std::unique_ptr<Page>> m_Pages;
        std::map<Uint32, Uint32>                         m_PageSizeToCount;
        mutable std::vector<GPUUploadManagerBucketInfo>  m_BucketInfo;

        std::atomic<Uint32> m_NumRunningUpdates{0};
        std::atomic<Uint32> m_MaxPendingUpdateSize{0};
        std::atomic<Uint32> m_TotalPendingUpdateSize{0};

        FreePages m_FreePages;

        std::atomic<Uint32> m_PeakUpdateSize{0};
        Uint32              m_PeakTotalPendingUpdateSize = 0;
        Uint32              m_PeakPageCount              = 0;
    };

    using UploadStreamUniquePtr = std::unique_ptr<UploadStream>;
    std::vector<UploadStreamUniquePtr> m_Streams;

    UploadStream* m_pNormalStream = nullptr;
    UploadStream* m_pLargeStream  = nullptr;

    class TextureUploadStreams
    {
    public:
        TextureUploadStreams(GPUUploadManagerImpl& Mgr, Uint32 PageSizeInBytes, Uint32 MaxPageCount) :
            m_Mgr{Mgr},
            m_PageSizeInBytes{PageSizeInBytes},
            m_MaxPageCount{MaxPageCount}
        {}

        UploadStream* GetStreamForFormat(IDeviceContext* pContext, TEXTURE_FORMAT Format);

        void MoveNewStreamsToManager();
        void SetStopping();

    private:
        GPUUploadManagerImpl& m_Mgr;
        const Uint32          m_PageSizeInBytes;
        const Uint32          m_MaxPageCount;

        std::shared_mutex                                 m_Mtx;
        std::unordered_map<TEXTURE_FORMAT, UploadStream*> m_StreamsByFormat;

        // New streams created by worker threads that are not yet added to parent m_Streams vector.
        std::mutex                                 m_NewStreamsMtx;
        std::vector<std::unique_ptr<UploadStream>> m_NewStreams;
        std::atomic<bool>                          m_HasNewStreams{false};
        std::atomic<bool>                          m_Stopping{false};
    };
    std::unique_ptr<TextureUploadStreams> m_pTextureStreams;

    // The number of running ScheduleBufferUpdate operations.
    std::atomic<Uint32> m_NumRunningUpdates{0};
    std::atomic<bool>   m_Stopping{false};
    Threading::Signal   m_LastRunningThreadFinishedSignal;

    std::vector<GPUUploadManagerStreamStats> m_StreamStats;
};

} // namespace Diligent
