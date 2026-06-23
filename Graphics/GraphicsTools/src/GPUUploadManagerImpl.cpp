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

namespace
{

class BufferUpdateCancelGuard
{
public:
    explicit BufferUpdateCancelGuard(const ScheduleBufferUpdateInfo& UpdateInfo) noexcept :
        m_pUpdateInfo{&UpdateInfo}
    {}

    ~BufferUpdateCancelGuard()
    {
        if (m_pUpdateInfo == nullptr)
            return;

        const ScheduleBufferUpdateInfo& UpdateInfo = *m_pUpdateInfo;
        if (UpdateInfo.CopyBuffer != nullptr)
        {
            UpdateInfo.CopyBuffer(nullptr, nullptr, ~0u, UpdateInfo.NumBytes, UpdateInfo.pCopyBufferData);
        }
        else if (UpdateInfo.UploadEnqueued != nullptr)
        {
            UpdateInfo.UploadEnqueued(nullptr, UpdateInfo.DstOffset, UpdateInfo.NumBytes, UpdateInfo.pUploadEnqueuedData);
        }
    }

    void Disarm() noexcept
    {
        m_pUpdateInfo = nullptr;
    }

private:
    const ScheduleBufferUpdateInfo* m_pUpdateInfo = nullptr;
};

class TextureUpdateCancelGuard
{
public:
    TextureUpdateCancelGuard(const ScheduleTextureUpdateInfo& UpdateInfo,
                             bool                             UseD3D11TextureCallback) noexcept :
        m_pUpdateInfo{&UpdateInfo},
        m_UseD3D11TextureCallback{UseD3D11TextureCallback}
    {}

    ~TextureUpdateCancelGuard()
    {
        if (m_pUpdateInfo == nullptr)
            return;

        const ScheduleTextureUpdateInfo& UpdateInfo = *m_pUpdateInfo;

        // CopyTexture and CopyD3D11Texture share pCopyTextureData and are backend alternatives,
        // so cancellation must call the same copy callback that would be used for this backend.
        bool CopyCallbackCalled = false;
        if (m_UseD3D11TextureCallback)
        {
            if (UpdateInfo.CopyD3D11Texture != nullptr)
            {
                UpdateInfo.CopyD3D11Texture(nullptr, UpdateInfo.DstMipLevel, UpdateInfo.DstSlice, UpdateInfo.DstBox, nullptr, 0, 0, UpdateInfo.pCopyTextureData);
                CopyCallbackCalled = true;
            }
        }
        else
        {
            if (UpdateInfo.CopyTexture != nullptr)
            {
                UpdateInfo.CopyTexture(nullptr, UpdateInfo.DstMipLevel, UpdateInfo.DstSlice, UpdateInfo.DstBox, TextureSubResData{}, UpdateInfo.pCopyTextureData);
                CopyCallbackCalled = true;
            }
        }

        if (!CopyCallbackCalled && UpdateInfo.UploadEnqueued != nullptr)
        {
            UpdateInfo.UploadEnqueued(nullptr, UpdateInfo.DstMipLevel, UpdateInfo.DstSlice, UpdateInfo.DstBox, UpdateInfo.pUploadEnqueuedData);
        }
    }

    void Disarm() noexcept
    {
        m_pUpdateInfo = nullptr;
    }

private:
    const ScheduleTextureUpdateInfo* m_pUpdateInfo             = nullptr;
    const bool                       m_UseD3D11TextureCallback = false;
};

bool ValidateBufferUpdate(const ScheduleBufferUpdateInfo& UpdateInfo)
{
    if (UpdateInfo.NumBytes != 0 &&
        UpdateInfo.pSrcData == nullptr &&
        UpdateInfo.WriteDataCallback == nullptr)
    {
        LOG_ERROR_MESSAGE("ScheduleBufferUpdate() with non-zero NumBytes requires pSrcData or WriteDataCallback");
        return false;
    }

    if (UpdateInfo.CopyBuffer == nullptr)
    {
        if (UpdateInfo.pDstBuffer == nullptr)
        {
            LOG_ERROR_MESSAGE("ScheduleBufferUpdate() requires pDstBuffer when CopyBuffer is not provided");
            return false;
        }

        const Uint64 BufferSize = UpdateInfo.pDstBuffer->GetDesc().Size;
        if (UpdateInfo.DstOffset > BufferSize ||
            static_cast<Uint64>(UpdateInfo.NumBytes) > BufferSize - UpdateInfo.DstOffset)
        {
            LOG_ERROR_MESSAGE("ScheduleBufferUpdate() destination range is outside of the destination buffer");
            return false;
        }
    }

    return true;
}

bool ValidateTextureUpdate(const ScheduleTextureUpdateInfo& UpdateInfo,
                           TEXTURE_FORMAT                   Format,
                           bool                             HasCopyCallback,
                           RENDER_DEVICE_TYPE               DeviceType)
{
    if (!UpdateInfo.DstBox.IsValid())
    {
        LOG_ERROR_MESSAGE("ScheduleTextureUpdate() requires a valid destination box");
        return false;
    }

    if (UpdateInfo.pSrcData == nullptr && UpdateInfo.WriteDataCallback == nullptr)
    {
        LOG_ERROR_MESSAGE("ScheduleTextureUpdate() requires pSrcData or WriteDataCallback");
        return false;
    }

    if (!HasCopyCallback && UpdateInfo.pDstTexture == nullptr)
    {
        LOG_ERROR_MESSAGE("ScheduleTextureUpdate() requires pDstTexture when no copy callback is provided");
        return false;
    }

    if (UpdateInfo.pDstTexture != nullptr)
    {
        const TextureDesc& TexDesc = UpdateInfo.pDstTexture->GetDesc();
        if (UpdateInfo.DstMipLevel >= TexDesc.MipLevels)
        {
            LOG_ERROR_MESSAGE("ScheduleTextureUpdate() destination mip level is outside of the destination texture");
            return false;
        }

        if (UpdateInfo.DstSlice >= TexDesc.GetArraySize())
        {
            LOG_ERROR_MESSAGE("ScheduleTextureUpdate() destination slice is outside of the destination texture");
            return false;
        }

        const MipLevelProperties Mip = GetMipLevelProperties(TexDesc, UpdateInfo.DstMipLevel);
        if (UpdateInfo.DstBox.MaxX > Mip.LogicalWidth ||
            UpdateInfo.DstBox.MaxY > Mip.LogicalHeight ||
            UpdateInfo.DstBox.MaxZ > Mip.Depth)
        {
            LOG_ERROR_MESSAGE("ScheduleTextureUpdate() destination box is outside of the destination texture subresource");
            return false;
        }

        if (TexDesc.SampleCount != 1)
        {
            LOG_ERROR_MESSAGE("ScheduleTextureUpdate() does not support multisampled destination textures");
            return false;
        }

        if (DeviceType == RENDER_DEVICE_TYPE_D3D11)
        {
            if (TexDesc.Type != RESOURCE_DIM_TEX_2D &&
                TexDesc.Type != RESOURCE_DIM_TEX_2D_ARRAY)
            {
                LOG_ERROR_MESSAGE("ScheduleTextureUpdate() in D3D11 only supports 2D and 2D array destination textures");
                return false;
            }
        }
    }

    if (DeviceType == RENDER_DEVICE_TYPE_D3D11)
    {
        if (UpdateInfo.DstBox.Depth() != 1)
        {
            LOG_ERROR_MESSAGE("ScheduleTextureUpdate() in D3D11 only supports single-slice texture updates");
            return false;
        }

        if (GetTextureFormatAttribs(Format).IsDepthStencil())
        {
            LOG_ERROR_MESSAGE("ScheduleTextureUpdate() in D3D11 does not support depth-stencil texture updates");
            return false;
        }
    }

    if (UpdateInfo.pSrcData != nullptr &&
        UpdateInfo.WriteDataCallback == nullptr)
    {
        const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Format);
        const Uint64                RowSize =
            static_cast<Uint64>(AlignUp(UpdateInfo.DstBox.Width(), FmtAttribs.BlockWidth) / FmtAttribs.BlockWidth) *
            FmtAttribs.GetElementSize();
        if (UpdateInfo.Stride < RowSize)
        {
            LOG_ERROR_MESSAGE("ScheduleTextureUpdate() source stride is too small");
            return false;
        }

        const Uint32 NumRows = AlignUp(UpdateInfo.DstBox.Height(), FmtAttribs.BlockHeight) / FmtAttribs.BlockHeight;
        if (UpdateInfo.DstBox.Depth() > 1)
        {
            const Uint64 MinDepthStride = UpdateInfo.Stride * (NumRows - 1) + RowSize;
            if (UpdateInfo.DepthStride < MinDepthStride)
            {
                LOG_ERROR_MESSAGE("ScheduleTextureUpdate() source depth stride is too small");
                return false;
            }
        }
    }

    return true;
}

} // namespace

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
        UNEXPECTED("Attempting to schedule a texture update with an invalid writer.");
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


GPUUploadManagerImpl::Page::StagingTextureAtlas::StagingTextureAtlas(IRenderDevice* pDevice, Uint32 Width, Uint32 Height, TEXTURE_FORMAT Format, const std::string& Name) :
    Mgr{Width, Height}
{
    TextureDesc TexDesc;
    TexDesc.Format         = Format;
    TexDesc.Name           = Name.c_str();
    TexDesc.Type           = RESOURCE_DIM_TEX_2D;
    TexDesc.Width          = Width;
    TexDesc.Height         = Height;
    TexDesc.Usage          = USAGE_STAGING;
    TexDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

    pDevice->CreateTexture(TexDesc, nullptr, &pTex);
    VERIFY_EXPR(pTex);
}

GPUUploadManagerImpl::Page::StagingTextureAtlas::~StagingTextureAtlas()
{
    Mgr.Reset();
}

void* GPUUploadManagerImpl::Page::StagingTextureAtlas::Map(IDeviceContext* pContext)
{
    pContext->TransitionResourceState({pTex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE});
    MappedTextureSubresource MappedData;
    pContext->MapTextureSubresource(pTex, 0, 0, MAP_WRITE, MAP_FLAG_NONE, nullptr, MappedData);
    RowStride   = static_cast<Uint32>(MappedData.Stride);
    DepthStride = static_cast<Uint32>(MappedData.DepthStride);
    return MappedData.pData;
}

void GPUUploadManagerImpl::Page::StagingTextureAtlas::Unmap(IDeviceContext* pContext)
{
    pContext->UnmapTextureSubresource(pTex, 0, 0);
    RowStride   = 0;
    DepthStride = 0;
}

void GPUUploadManagerImpl::Page::StagingTextureAtlas::Reset()
{
    Mgr.Reset();
}

DynamicAtlasManager::Region GPUUploadManagerImpl::Page::StagingTextureAtlas::Allocate(Uint32 Width, Uint32 Height)
{
    std::lock_guard<std::mutex> Lock{MgrMtx};
    return Mgr.Allocate(Width, Height);
}


GPUUploadManagerImpl::Page::Page(Uint32 Size, bool PersistentMapped) noexcept :
    m_Size{Size},
    m_PersistentMapped{PersistentMapped}
{}

inline bool PersistentMapSupported(IRenderDevice* pDevice)
{
    RENDER_DEVICE_TYPE DeviceType = pDevice->GetDeviceInfo().Type;
    return (DeviceType == RENDER_DEVICE_TYPE_D3D12 ||
            DeviceType == RENDER_DEVICE_TYPE_VULKAN ||
            DeviceType == RENDER_DEVICE_TYPE_METAL);
}

std::atomic<Uint32> GPUUploadManagerImpl::Page::sm_PageCounter{0};

GPUUploadManagerImpl::Page::Page(UploadStream* pStream, IRenderDevice* pDevice, Uint32 Size) :
    m_pStream{pStream},
    m_Size{Size},
    m_PersistentMapped{PersistentMapSupported(pDevice)}
{
    const std::string Name =
        std::string{"GPUUploadManagerImpl page "} + std::to_string(sm_PageCounter.fetch_add(1)) +
        " (" + GetMemorySizeString(Size) + ')';

    BufferDesc Desc;
    Desc.Name           = Name.c_str();
    Desc.Size           = Size;
    Desc.Usage          = USAGE_STAGING;
    Desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(Desc, nullptr, &m_pStagingBuffer);
    VERIFY_EXPR(m_pStagingBuffer != nullptr);
}

GPUUploadManagerImpl::Page::Page(UploadStream* pStream, IRenderDevice* pDevice, Uint32 Size, TEXTURE_FORMAT Format) :
    m_pStream{pStream},
    m_Size{Size},
    m_PersistentMapped{PersistentMapSupported(pDevice)},
    m_pStagingAtlas{
        std::make_unique<StagingTextureAtlas>(
            pDevice,
            Size,
            Size,
            Format,
            std::string{"GPUUploadManagerImpl page "} + std::to_string(sm_PageCounter.fetch_add(1)) +
                " (" + GetTextureFormatAttribs(Format).Name + ' ' + std::to_string(Size) + 'x' + std::to_string(Size) + ')'),
    }
{
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
            if (TexOp.CopyD3D11Texture != nullptr)
            {
                TexOp.CopyD3D11Texture(nullptr, TexOp.DstMipLevel, TexOp.DstSlice, TexOp.DstBox, nullptr, 0, 0, TexOp.pCopyTextureData);
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
    VERIFY(!m_pStagingAtlas, "Buffer updates must not be scheduled on texture-backed pages.");

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
    Op.pDstBuffer              = UpdateInfo.pDstBuffer;
    Op.DstBufferTransitionMode = UpdateInfo.DstBufferTransitionMode;
    Op.CopyBuffer              = UpdateInfo.CopyBuffer;
    Op.pCopyBufferData         = UpdateInfo.pCopyBufferData;
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
    Uint32 SrcX   = 0;
    Uint32 SrcY   = 0;
    if (UpdateInfo.DstBox.IsValid())
    {
        Uint8* pDst           = nullptr;
        Uint32 DstRowStride   = 0;
        Uint32 DstDepthStride = 0;
        size_t DstRowSize     = 0;
        if (m_pStagingBuffer)
        {
            Offset = Allocate(static_cast<Uint32>(CopyInfo.MemorySize), std::max(kMinimumOffsetAlignment, OffsetAlignment));
            if (Offset == ~0u)
            {
                return false;
            }
            if (m_pData != nullptr)
            {
                pDst           = static_cast<Uint8*>(m_pData) + Offset;
                DstRowStride   = static_cast<Uint32>(CopyInfo.RowStride);
                DstDepthStride = static_cast<Uint32>(CopyInfo.DepthStride);
                DstRowSize     = static_cast<size_t>(CopyInfo.RowSize);
            }
        }
        else if (m_pStagingAtlas)
        {
            const TEXTURE_FORMAT Format = UpdateInfo.pDstTexture != nullptr ?
                UpdateInfo.pDstTexture->GetDesc().Format :
                UpdateInfo.Format;
            VERIFY_EXPR(Format != TEX_FORMAT_UNKNOWN);

            const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Format);

            DynamicAtlasManager::Region Region = m_pStagingAtlas->Allocate(
                AlignUp(UpdateInfo.DstBox.Width(), FmtAttribs.BlockWidth),
                AlignUp(UpdateInfo.DstBox.Height(), FmtAttribs.BlockHeight));
            if (!Region)
            {
                return false;
            }
            SrcX = Region.x;
            SrcY = Region.y;

            const Uint32 ElementSize = FmtAttribs.GetElementSize();

            DstRowStride   = m_pStagingAtlas->RowStride;
            DstDepthStride = m_pStagingAtlas->DepthStride;

            pDst       = static_cast<Uint8*>(m_pData) + static_cast<size_t>((SrcY / FmtAttribs.BlockHeight) * DstRowStride + (SrcX / FmtAttribs.BlockWidth) * ElementSize);
            DstRowSize = Region.width / FmtAttribs.BlockWidth * ElementSize;
        }

        if (pDst != nullptr)
        {
            if (UpdateInfo.WriteDataCallback != nullptr)
            {
                UpdateInfo.WriteDataCallback(pDst, DstRowStride, DstDepthStride, UpdateInfo.DstBox, UpdateInfo.pWriteDataCallbackUserData);
            }
            else if (UpdateInfo.pSrcData != nullptr)
            {
                const TEXTURE_FORMAT Format = UpdateInfo.pDstTexture != nullptr ?
                    UpdateInfo.pDstTexture->GetDesc().Format :
                    UpdateInfo.Format;
                VERIFY_EXPR(Format != TEX_FORMAT_UNKNOWN);

                const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Format);
                const Uint32                NumRows    = AlignUp(UpdateInfo.DstBox.Height(), FmtAttribs.BlockHeight) / FmtAttribs.BlockHeight;
                for (Uint32 DepthSlice = 0; DepthSlice < UpdateInfo.DstBox.Depth(); ++DepthSlice)
                {
                    for (Uint32 row = 0; row < NumRows; ++row)
                    {
                        const size_t SrcRowOffset = static_cast<size_t>(DepthSlice * UpdateInfo.DepthStride + row * UpdateInfo.Stride);
                        const size_t DstRowOffset = static_cast<size_t>(DepthSlice * DstDepthStride + row * DstRowStride);
                        const void*  pSrcRow      = static_cast<const Uint8*>(UpdateInfo.pSrcData) + SrcRowOffset;
                        void*        pDstRow      = pDst + DstRowOffset;
                        std::memcpy(pDstRow, pSrcRow, DstRowSize);
                    }
                }
            }
        }
    }

    PendingTextureOp Op;
    Op.pDstTexture              = UpdateInfo.pDstTexture;
    Op.DstTextureTransitionMode = UpdateInfo.DstTextureTransitionMode;
    if (m_pStagingBuffer)
    {
        Op.CopyTexture = UpdateInfo.CopyTexture;
    }
    else if (m_pStagingAtlas)
    {
        Op.CopyD3D11Texture = UpdateInfo.CopyD3D11Texture;
    }
    Op.pCopyTextureData = UpdateInfo.pCopyTextureData;
    Op.SrcX             = SrcX;
    Op.SrcY             = SrcY;
    Op.SrcOffset        = Offset;
    Op.SrcStride        = static_cast<Uint32>(CopyInfo.RowStride);
    Op.SrcDepthStride   = static_cast<Uint32>(CopyInfo.DepthStride);
    Op.DstMipLevel      = UpdateInfo.DstMipLevel;
    Op.DstSlice         = UpdateInfo.DstSlice;
    Op.DstBox           = UpdateInfo.DstBox;
    if (Op.CopyTexture == nullptr && Op.CopyD3D11Texture == nullptr)
    {
        Op.UploadEnqueued      = UpdateInfo.UploadEnqueued;
        Op.pUploadEnqueuedData = UpdateInfo.pUploadEnqueuedData;
    }
    m_PendingOps.Enqueue(std::move(Op));

    return true;
}

void GPUUploadManagerImpl::Page::UnmapStagingResource(IDeviceContext* pContext)
{
    VERIFY_EXPR(pContext != nullptr);
    VERIFY_EXPR(m_pData != nullptr);
    VERIFY_EXPR((m_pStagingBuffer != nullptr) ^ (m_pStagingAtlas != nullptr));
    if (m_pStagingBuffer)
    {
        pContext->UnmapBuffer(m_pStagingBuffer, MAP_WRITE);
    }
    else if (m_pStagingAtlas)
    {
        m_pStagingAtlas->Unmap(pContext);
    }
    m_pData = nullptr;
}

void GPUUploadManagerImpl::Page::ExecutePendingOps(IDeviceContext* pContext, Uint64 FenceValue)
{
    VERIFY(DbgIsSealed(), "Page must be sealed before executing pending operations");
    VERIFY(DbgGetWriterCount() == 0, "All writers must finish before executing pending operations");

    if (m_pData != nullptr && !m_PersistentMapped)
    {
        UnmapStagingResource(pContext);
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
                    pContext->CopyBuffer(m_pStagingBuffer, BuffOp.SrcOffset, RESOURCE_STATE_TRANSITION_MODE_VERIFY,
                                         BuffOp.pDstBuffer, BuffOp.DstOffset, BuffOp.NumBytes, BuffOp.DstBufferTransitionMode);
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
            else if (TexOp.CopyD3D11Texture)
            {
                TexOp.CopyD3D11Texture(pContext, TexOp.DstMipLevel, TexOp.DstSlice, TexOp.DstBox, m_pStagingAtlas->pTex, TexOp.SrcX, TexOp.SrcY, TexOp.pCopyTextureData);
            }
            else
            {
                if (pContext != nullptr && TexOp.pDstTexture != nullptr)
                {
                    if (m_pStagingBuffer)
                    {
                        pContext->UpdateTexture(TexOp.pDstTexture, TexOp.DstMipLevel, TexOp.DstSlice, TexOp.DstBox, SrcData,
                                                RESOURCE_STATE_TRANSITION_MODE_VERIFY, TexOp.DstTextureTransitionMode);
                    }
                    else if (m_pStagingAtlas)
                    {
                        CopyTextureAttribs CopyAttribs;

                        CopyAttribs.pSrcTexture              = m_pStagingAtlas->pTex;
                        CopyAttribs.pDstTexture              = TexOp.pDstTexture;
                        CopyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_VERIFY;
                        CopyAttribs.DstTextureTransitionMode = TexOp.DstTextureTransitionMode;
                        CopyAttribs.DstMipLevel              = TexOp.DstMipLevel;
                        CopyAttribs.DstSlice                 = TexOp.DstSlice;

                        const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(TexOp.pDstTexture->GetDesc().Format);

                        Box SrcBox;
                        SrcBox.MinX = TexOp.SrcX;
                        SrcBox.MaxX = AlignUp(TexOp.SrcX + TexOp.DstBox.Width(), FmtAttribs.BlockWidth);
                        SrcBox.MinY = TexOp.SrcY;
                        SrcBox.MaxY = AlignUp(TexOp.SrcY + TexOp.DstBox.Height(), FmtAttribs.BlockHeight);

                        CopyAttribs.pSrcBox = &SrcBox;

                        CopyAttribs.DstX = TexOp.DstBox.MinX;
                        CopyAttribs.DstY = TexOp.DstBox.MinY;
                        CopyAttribs.DstZ = TexOp.DstBox.MinZ;

                        pContext->CopyTexture(CopyAttribs);
                    }
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
    if (m_pStagingAtlas)
    {
        m_pStagingAtlas->Reset();
    }

    if (pContext != nullptr && m_pData == nullptr)
    {
        if (m_pStagingBuffer)
        {
            pContext->TransitionResourceState({m_pStagingBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE});
            const MAP_FLAGS MapFlags = m_PersistentMapped ?
                MAP_FLAG_DO_NOT_WAIT :
                MAP_FLAG_NONE;
            pContext->MapBuffer(m_pStagingBuffer, MAP_WRITE, MapFlags, m_pData);
        }
        else if (m_pStagingAtlas)
        {
            m_pData = m_pStagingAtlas->Map(pContext);
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

void GPUUploadManagerImpl::Page::Recycle()
{
    m_pStream->AddFreePage(this);
}

void GPUUploadManagerImpl::Page::ReleaseStagingBuffer(IDeviceContext* pContext)
{
    if (m_pData != nullptr)
    {
        UnmapStagingResource(pContext);
    }
    m_pStagingBuffer.Release();
    m_pStagingAtlas.reset();
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

GPUUploadManagerImpl::Page* GPUUploadManagerImpl::FreePages::Pop(Uint32 RequiredSize, const std::atomic<Uint32>* pScheduleAdmissionState)
{
    Page* P = nullptr;
    {
        std::lock_guard<std::mutex> Guard{m_PagesMtx};
        if (pScheduleAdmissionState != nullptr &&
            (pScheduleAdmissionState->load(std::memory_order_acquire) & GPUUploadManagerImpl::SCHEDULE_COUNT_MASK) != 0)
        {
            return nullptr;
        }

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
                                                 IDeviceContext*       pContext,
                                                 Uint32                PageSize,
                                                 Uint32                MaxPageCount,
                                                 Uint32                InitialPageCount,
                                                 TEXTURE_FORMAT        Format) noexcept :
    m_Mgr{Mgr},
    m_PageSize{Format == TEX_FORMAT_UNKNOWN ? AlignUpToPowerOfTwo(PageSize) : AlignUp(PageSize, 32u)},
    m_MaxPageCount{MaxPageCount},
    m_Format{Format}
{
    if (pContext != nullptr)
    {
        // Create at least one page.
        if (Page* pPage = CreatePage(pContext))
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
        if (Page* pPage = CreatePage(pContext))
        {
            if (pContext != nullptr)
            {
                // If a context is provided, we can immediately map the staging buffer and
                // prepare the page for use, so we push it to the free list.
                m_FreePages.Push(pPage);
            }
            else
            {
                // If no context is provided, the page needs to be mapped in Reset(),
                // so we add it to the list of in-flight pages.
                // Since initial fence value is 0, these pages will become available after first call
                // to RenderThreadUpdate().
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

GPUUploadManagerImpl::UploadStream* GPUUploadManagerImpl::TextureUploadStreams::GetStreamForFormat(IDeviceContext* pContext, TEXTURE_FORMAT Format)
{
    Format = FormatToTypeless(Format);

    {
        std::shared_lock<std::shared_mutex> Lock{m_Mtx};

        auto it = m_StreamsByFormat.find(Format);
        if (it != m_StreamsByFormat.end())
            return it->second;
    }

    std::unique_lock<std::shared_mutex> Lock{m_Mtx};
    if (m_Stopping.load(std::memory_order_acquire))
        return nullptr;

    auto it = m_StreamsByFormat.find(Format);
    if (it != m_StreamsByFormat.end())
        return it->second;

    const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Format);

    const float BPP =
        static_cast<float>(FmtAttribs.GetElementSize()) /
        static_cast<float>(FmtAttribs.BlockWidth * FmtAttribs.BlockHeight);

    float  fPageSize = std::sqrt(static_cast<float>(m_PageSizeInBytes) / BPP);
    Uint32 PageSize  = std::max(static_cast<Uint32>(fPageSize), 64u);
    PageSize         = PageSize < 1024 ? AlignUpToPowerOfTwo(PageSize) : AlignUp(PageSize, 2048u);
    PageSize         = std::min(PageSize, 16384u);

    // Don't create initial pages from worker threads because this requires access to
    // m_InFlightPages, which is not protected by a mutex.
    Uint32 InitialPageCount = pContext != nullptr ? 1 : 0;

    UploadStreamUniquePtr NewStream = std::make_unique<UploadStream>(m_Mgr, pContext, PageSize, m_MaxPageCount, InitialPageCount, Format);

    it = m_StreamsByFormat.emplace(Format, NewStream.get()).first;

    {
        std::lock_guard<std::mutex> NewStreamsLock{m_NewStreamsMtx};
        m_NewStreams.push_back(std::move(NewStream));
        m_HasNewStreams.store(true);
    }

    return it->second;
}

void GPUUploadManagerImpl::TextureUploadStreams::MoveNewStreamsToManager()
{
    if (!m_HasNewStreams.load())
        return;

    std::lock_guard<std::mutex> Lock{m_NewStreamsMtx};
    m_Mgr.m_Streams.reserve(m_Mgr.m_Streams.size() + m_NewStreams.size());
    for (UploadStreamUniquePtr& Stream : m_NewStreams)
    {
        m_Mgr.m_Streams.push_back(std::move(Stream));
    }
    m_NewStreams.clear();
    m_HasNewStreams.store(false);
}

void GPUUploadManagerImpl::TextureUploadStreams::SetStopping()
{
    // Set the stop flag while holding the mutex to ensure that
    // no new streams are added after we set the flag.
    std::unique_lock<std::shared_mutex> Lock{m_Mtx};
    m_Stopping.store(true, std::memory_order_release);
}

GPUUploadManagerImpl::GPUUploadManagerImpl(IReferenceCounters* pRefCounters, const GPUUploadManagerCreateInfo& CI) :
    TBase{pRefCounters},
    m_pDevice{CI.pDevice},
    m_pContext{CI.pContext},
    m_DeviceType{CI.pDevice->GetDeviceInfo().Type},
    m_TextureUpdateOffsetAlignment{m_pDevice->GetAdapterInfo().Buffer.TextureUpdateOffsetAlignment},
    m_TextureUpdateStrideAlignment{m_pDevice->GetAdapterInfo().Buffer.TextureUpdateStrideAlignment}
{
    const Uint32 PageSize = CI.PageSize != 0 ? CI.PageSize : GPUUploadManagerCreateInfo{}.PageSize;
    if (CI.PageSize == 0)
        LOG_ERROR_MESSAGE("GPUUploadManagerCreateInfo::PageSize must not be zero; using the default value ", PageSize);

    const Uint32 LargePageSize = CI.LargePageSize != 0 ? CI.LargePageSize : GPUUploadManagerCreateInfo{}.LargePageSize;
    if (CI.LargePageSize == 0)
        LOG_ERROR_MESSAGE("GPUUploadManagerCreateInfo::LargePageSize must not be zero; using the default value ", LargePageSize);

    m_Streams.reserve(2);
    m_Streams.push_back(std::make_unique<UploadStream>(
        *this,
        CI.pContext,
        PageSize,
        CI.MaxPageCount,
        CI.InitialPageCount));
    m_pNormalStream = m_Streams.back().get();

    m_Streams.push_back(std::make_unique<UploadStream>(
        *this,
        CI.pContext,
        std::max(LargePageSize, PageSize * 2),
        CI.MaxLargePageCount,
        CI.InitialLargePageCount));
    m_pLargeStream = m_Streams.back().get();

    FenceDesc Desc;
    Desc.Name = "GPU upload manager fence";
    Desc.Type = FENCE_TYPE_CPU_WAIT_ONLY;
    m_pDevice->CreateFence(Desc, &m_pFence);
    VERIFY_EXPR(m_pFence != nullptr);

    if (m_DeviceType == RENDER_DEVICE_TYPE_D3D11)
    {
        m_pTextureStreams = std::make_unique<TextureUploadStreams>(
            *this,
            m_pLargeStream->GetPageSize(),
            CI.MaxLargePageCount);
    }
}

void GPUUploadManagerImpl::UploadStream::ReleaseStagingBuffers(IDeviceContext* pContext)
{
    for (auto& it : m_Pages)
    {
        it.second->ReleaseStagingBuffer(pContext);
    }
}

void GPUUploadManagerImpl::UploadStream::SignalStop()
{
    m_PageRotatedSignal.RequestStop();
}

bool GPUUploadManagerImpl::TryBeginScheduleUpdate() noexcept
{
    Uint32 State = m_ScheduleAdmissionState.load(std::memory_order_acquire);
    for (;;)
    {
        if ((State & SCHEDULE_STOP_BIT) != 0)
            return false;

        VERIFY((State & SCHEDULE_COUNT_MASK) != SCHEDULE_COUNT_MASK, "Too many active GPU upload schedule calls");

        // Atomically register this update only if Stop() has not claimed the stop bit.
        // Splitting this into a load of m_Stopping followed by a separate counter increment
        // would leave a window where Stop()/destruction can observe zero active updates while
        // this thread is already on its way into stream scheduling.
        if (m_ScheduleAdmissionState.compare_exchange_weak(
                State,
                State + 1,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
            return true;
        }
    }
}

void GPUUploadManagerImpl::EndScheduleUpdate() noexcept
{
    const Uint32 PrevState = m_ScheduleAdmissionState.fetch_sub(1, std::memory_order_acq_rel);
    VERIFY((PrevState & SCHEDULE_COUNT_MASK) != 0, "GPU upload schedule-call count underflow");

    if ((PrevState & SCHEDULE_STOP_BIT) != 0 &&
        (PrevState & SCHEDULE_COUNT_MASK) == 1)
    {
        m_LastRunningThreadFinishedSignal.Trigger();
    }
}

bool GPUUploadManagerImpl::SetStopping() noexcept
{
    // From this point on, new schedule calls fail admission. Calls that already incremented
    // the counter are allowed to return through their normal path, and Stop() can wait
    // for the last one using m_LastRunningThreadFinishedSignal.
    const Uint32 PrevState = m_ScheduleAdmissionState.fetch_or(SCHEDULE_STOP_BIT, std::memory_order_acq_rel);
    return (PrevState & SCHEDULE_STOP_BIT) == 0;
}

GPUUploadManagerImpl::ScheduleUpdateGuard::ScheduleUpdateGuard(GPUUploadManagerImpl& Mgr) noexcept
{
    if (Mgr.TryBeginScheduleUpdate())
        m_pMgr = &Mgr;
}

GPUUploadManagerImpl::ScheduleUpdateGuard::~ScheduleUpdateGuard()
{
    if (m_pMgr != nullptr)
        m_pMgr->EndScheduleUpdate();
}

void GPUUploadManagerImpl::Stop(IDeviceContext* pContext)
{
    if (!SetStopping())
        return;

    m_Stopping.store(true, std::memory_order_release);

    if (pContext != nullptr)
    {
        if (!m_pContext)
            m_pContext = pContext;
        else
            DEV_CHECK_ERR(pContext == m_pContext, "The context provided to Stop must be the same as the one used to create the GPUUploadManagerImpl");
    }
    else
    {
        pContext = m_pContext;
    }

    if (m_pTextureStreams)
    {
        m_pTextureStreams->SetStopping();
        // After the stopping flag is set, no new streams can be added.
        m_pTextureStreams->MoveNewStreamsToManager();
    }

    for (UploadStreamUniquePtr& Stream : m_Streams)
    {
        Stream->SignalStop();
    }

    if ((m_ScheduleAdmissionState.load(std::memory_order_acquire) & SCHEDULE_COUNT_MASK) != 0)
    {
        m_LastRunningThreadFinishedSignal.Wait();
    }

    // Once all admitted ScheduleBufferUpdate()/ScheduleTextureUpdate() calls have returned,
    // it is safe to unmap and release staging resources here. Do this in Stop() instead of
    // the destructor so the application controls the thread/phase where the manager's device
    // context is touched. Keep streams and pages alive until destruction, because pending
    // operations still own callback payloads that must be released during teardown.
    for (UploadStreamUniquePtr& Stream : m_Streams)
    {
        Stream->ReleaseStagingBuffers(pContext);
    }
}

GPUUploadManagerImpl::~GPUUploadManagerImpl()
{
    Stop(m_pContext);

    // Pending page pointers are owned by the streams below. The manager is terminally
    // destroyed, so discard the queue nodes before destroying the pages.
    Page* pPage = nullptr;
    while (m_PendingPages.Dequeue(pPage))
    {}
}

void GPUUploadManagerImpl::RenderThreadUpdate(IDeviceContext* pContext)
{
    DEV_CHECK_ERR(pContext != nullptr, "A valid context must be provided to RenderThreadUpdate");
    if (m_Stopping.load(std::memory_order_acquire))
    {
        DEV_ERROR("GPU upload manager has been stopped");
        return;
    }

    if (!m_pContext)
    {
        // If no context was provided at creation, we can accept any context in RenderThreadUpdate, but it must be the same across calls.
        m_pContext = pContext;
    }
    else
    {
        DEV_CHECK_ERR(pContext == m_pContext, "The context provided to RenderThreadUpdate must be the same as the one used to create the GPUUploadManagerImpl");
    }

    if (m_pTextureStreams)
    {
        m_pTextureStreams->MoveNewStreamsToManager();
    }

    // 1. Reclaim completed pages to make them available.
    ReclaimCompletedPages(pContext);

    // 2. Add free pages to accommodate pending updates.
    for (UploadStreamUniquePtr& Stream : m_Streams)
    {
        Stream->AddFreePages(pContext);
    }

    // 3. Seal and swap the current page.
    for (UploadStreamUniquePtr& Stream : m_Streams)
    {
        Stream->SealAndSwapCurrentPage(pContext);
    }

    // 4. Process pending pages and move them to in-fligt list.
    ProcessPendingPages(pContext);

    // 5. Lastly, process pages to release. Do this at the very end so that newly available free pages
    //    first get a chance to be used for pending updates.
    for (UploadStreamUniquePtr& Stream : m_Streams)
    {
        Stream->ProcessPagesToRelease(pContext);
    }

    pContext->EnqueueSignal(m_pFence, m_NextFenceValue++);

    for (UploadStreamUniquePtr& Stream : m_Streams)
    {
        Stream->SignalPageRotated();
    }
}

GPUUploadManagerImpl::UploadStream& GPUUploadManagerImpl::GetStreamForUpdateSize(Uint32 UpdateSize)
{
    const Uint32 LargeUpdateThreshold = m_pNormalStream->GetPageSize();
    return UpdateSize > LargeUpdateThreshold ? *m_pLargeStream : *m_pNormalStream;
}

bool GPUUploadManagerImpl::UploadStream::ScheduleUpdate(IDeviceContext* pContext,
                                                        Uint32          UpdateSize,
                                                        const void*     pUpdateInfo,
                                                        bool            ScheduleUpdate(Page::Writer& Writer, const void* pUpdateInfo))
{
    DEV_CHECK_ERR(pContext == nullptr || pContext == m_Mgr.m_pContext,
                  "If a context is provided to ScheduleBufferUpdate/ScheduleTextureUpdate, it must be the same as the "
                  "one used to create the GPUUploadManagerImpl");

    bool IsFirstAttempt  = true;
    bool AbortUpdate     = false;
    bool UpdateScheduled = false;

    auto UpdatePendingSizeAndTryRotate = [&](Page* P) {
        Uint64 PageEpoch = m_PageRotatedSignal.CurrentEpoch();
        // Note that for texture pages, UpdateSize is the texture dimension.
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

        UpdateScheduled = ScheduleUpdate(Writer, pUpdateInfo);
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
    return UpdateScheduled;
}

bool GPUUploadManagerImpl::ScheduleBufferUpdate(const ScheduleBufferUpdateInfo& UpdateInfo)
{
    BufferUpdateCancelGuard CancelGuard{UpdateInfo};

    ScheduleUpdateGuard ScheduleGuard{*this};
    if (!ScheduleGuard)
    {
        // Worker-thread scheduling may race with Stop(). Quietly cancel the update
        // through the guard so callback-owned user data is released.
        return false;
    }

    if (!ValidateBufferUpdate(UpdateInfo))
        return false;

    if (UpdateInfo.CopyBuffer != nullptr && UpdateInfo.UploadEnqueued != nullptr)
    {
        LOG_WARNING_MESSAGE("ScheduleBufferUpdateInfo::UploadEnqueued is ignored when CopyBuffer is provided.");
    }

    UploadStream& Stream = GetStreamForUpdateSize(UpdateInfo.NumBytes);

    const bool Scheduled = Stream.ScheduleUpdate(
        UpdateInfo.pContext, UpdateInfo.NumBytes, &UpdateInfo,
        [](Page::Writer& Writer, const void* pUpdateData) {
            const ScheduleBufferUpdateInfo* pBufferUpdateInfo = static_cast<const ScheduleBufferUpdateInfo*>(pUpdateData);
            return Writer.ScheduleBufferUpdate(*pBufferUpdateInfo);
        });
    if (Scheduled)
    {
        CancelGuard.Disarm();
    }

    return Scheduled;
}

bool GPUUploadManagerImpl::ScheduleTextureUpdate(const ScheduleTextureUpdateInfo& UpdateInfo)
{
    const bool               UseD3D11TextureCallback = m_DeviceType == RENDER_DEVICE_TYPE_D3D11;
    TextureUpdateCancelGuard CancelGuard{UpdateInfo, UseD3D11TextureCallback};

    ScheduleUpdateGuard ScheduleGuard{*this};
    if (!ScheduleGuard)
    {
        // Worker-thread scheduling may race with Stop(). Quietly cancel the update
        // through the guard so callback-owned user data is released.
        return false;
    }

    const bool HasCopyCallback =
        UseD3D11TextureCallback ?
        UpdateInfo.CopyD3D11Texture != nullptr :
        UpdateInfo.CopyTexture != nullptr;
    if (HasCopyCallback && UpdateInfo.UploadEnqueued != nullptr)
    {
        LOG_WARNING_MESSAGE("ScheduleTextureUpdateInfo::UploadEnqueued is ignored when a copy callback is provided.");
    }

    const TEXTURE_FORMAT Format = UpdateInfo.pDstTexture != nullptr ?
        UpdateInfo.pDstTexture->GetDesc().Format :
        UpdateInfo.Format;
    if (Format == TEX_FORMAT_UNKNOWN)
    {
        LOG_ERROR_MESSAGE("ScheduleTextureUpdate() requires pDstTexture or a valid ScheduleTextureUpdateInfo::Format");
        return false;
    }

    if (!ValidateTextureUpdate(UpdateInfo, Format, HasCopyCallback, m_DeviceType))
        return false;

    struct ScheduleUpdateData
    {
        const ScheduleTextureUpdateInfo& UpdateInfo;
        const BufferToTextureCopyInfo    CopyInfo;
        const Uint32                     OffsetAlignment;
    };
    ScheduleUpdateData UpdateData{
        UpdateInfo,
        !UseD3D11TextureCallback ?
            GetBufferToTextureCopyInfo(Format, UpdateInfo.DstBox, m_TextureUpdateStrideAlignment) :
            BufferToTextureCopyInfo{},
        !UseD3D11TextureCallback ?
            m_TextureUpdateOffsetAlignment :
            0,
    };

    UploadStream* pStream = m_pTextureStreams ?
        m_pTextureStreams->GetStreamForFormat(UpdateData.UpdateInfo.pContext, Format) :
        &GetStreamForUpdateSize(static_cast<Uint32>(UpdateData.CopyInfo.MemorySize));
    if (pStream == nullptr)
    {
        // GetStreamForFormat can return null if the manager is stopping.
        return false;
    }

    // For texture updates, use the maximum upload region size as the update size.
    const Uint32 UpdateSize = m_pTextureStreams ?
        std::max(UpdateInfo.DstBox.Width(), UpdateInfo.DstBox.Height()) :
        static_cast<Uint32>(UpdateData.CopyInfo.MemorySize);

    const bool Scheduled = pStream->ScheduleUpdate(
        UpdateInfo.pContext, UpdateSize, &UpdateData,
        [](Page::Writer& Writer, const void* pData) {
            const ScheduleUpdateData* pUpdateData = static_cast<const ScheduleUpdateData*>(pData);
            return Writer.ScheduleTextureUpdate(pUpdateData->UpdateInfo, pUpdateData->CopyInfo, pUpdateData->OffsetAlignment);
        });
    if (Scheduled)
    {
        CancelGuard.Disarm();
    }

    return Scheduled;
}

GPUUploadManagerImpl::Page* GPUUploadManagerImpl::UploadStream::CreatePage(IDeviceContext* pContext, Uint32 RequiredSize, bool AllowOverLimit)
{
    if (!AllowOverLimit)
    {
        const Uint32 MaxExistingPageSize = m_PageSizeToCount.empty() ? 0 : m_PageSizeToCount.rbegin()->first;
        if (m_MaxPageCount != 0 && m_Pages.size() >= m_MaxPageCount && RequiredSize <= MaxExistingPageSize)
        {
            return nullptr;
        }
    }

    Uint32 PageSize = m_PageSize;
    while (PageSize < RequiredSize)
        PageSize *= 2;

    std::unique_ptr<Page> NewPage = m_Format != TEX_FORMAT_UNKNOWN ?
        std::make_unique<Page>(this, m_Mgr.m_pDevice, PageSize, m_Format) :
        std::make_unique<Page>(this, m_Mgr.m_pDevice, PageSize);

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
    // Allow going over the max page count when rotating the page from the render thread to
    // prevent deadlock in ScheduleUpdate.
    bool AllowOverLimit = pContext != nullptr;
    // Grab a free page.
    Page* Fresh = AcquireFreePage(pContext, RequiredSize, AllowOverLimit);
    if (!Fresh)
        return false;

    Page* Cur = ExpectedCurrent;
    if (!m_pCurrentPage.compare_exchange_strong(Cur, Fresh, std::memory_order_acq_rel))
    {
        // Lost the race. Fresh was unsealed by AcquireFreePage(), so a stale
        // writer may have acquired it before the CAS failed. Return it to the
        // free list only if sealing observes no active writers; otherwise the
        // last writer will recycle the empty page through TryEnqueuePage().
        if (Fresh->TrySeal() == Page::SealStatus::Ready)
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
            P->Recycle();
        }
        m_TmpPages.clear();
    }
}


void GPUUploadManagerImpl::UploadStream::AddFreePages(IDeviceContext* pContext)
{
    VERIFY_EXPR(pContext != nullptr);

    const Uint32 TotalPendingSize = m_TotalPendingUpdateSize.exchange(0, std::memory_order_acq_rel);
    const Uint32 MinimalPageCount = m_Format != TEX_FORMAT_UNKNOWN ?
        (TotalPendingSize > 0 ? 1 : 0) : // For texture streams, TotalPendingSize is a sum of linear update region dimensions, which
                                         // is not informative for determining the number of pages needed, so we just add 1 page
                                         // if there are pending updates.
        std::max((TotalPendingSize + m_PageSize - 1) / m_PageSize, 1u);

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

    VERIFY_EXPR(pContext != nullptr);
    while (m_Pages.size() > m_MaxPageCount)
    {
        // Pop the smallest free page and release it until we are within the limit.
        // The running-update counter is checked under the free-list lock to avoid
        // ABA issue in ScheduleBufferUpdate/ScheduleTextureUpdate:
        // * T1:
        //      Page* P = m_pCurrentPage.load(std::memory_order_acquire);
        // * T1 gets descheduled
        // * Render thread frees the page.
        // * T1 resumes and crashes at
        //      Page::Writer Writer = P->TryBeginWriting();
        if (Page* pPage = m_FreePages.Pop(0, &m_Mgr.m_ScheduleAdmissionState))
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

GPUUploadManagerImpl::Page* GPUUploadManagerImpl::UploadStream::AcquireFreePage(IDeviceContext* pContext,
                                                                                Uint32          RequiredSize,
                                                                                bool            AllowOverLimit)
{
    // For texture pages, all sizes are texture dimensions.
    Uint32 MaxPendingUpdateSize = std::max(m_MaxPendingUpdateSize.load(std::memory_order_acquire), RequiredSize);

    Page* P = m_FreePages.Pop(MaxPendingUpdateSize);
    if (P == nullptr && pContext != nullptr)
    {
        P = CreatePage(pContext, MaxPendingUpdateSize, AllowOverLimit);
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

    Stats.Format                     = m_Format;
    Stats.PageSize                   = m_PageSize;
    Stats.NumPages                   = static_cast<Uint32>(m_Pages.size());
    Stats.NumFreePages               = static_cast<Uint32>(m_FreePages.Size());
    Stats.PeakNumPages               = m_PeakPageCount;
    Stats.PeakTotalPendingUpdateSize = m_PeakTotalPendingUpdateSize;
    Stats.PeakUpdateSize             = m_PeakUpdateSize.load(std::memory_order_relaxed);
    Stats.NumBuckets                 = static_cast<Uint32>(m_BucketInfo.size());
    Stats.pBucketInfo                = m_BucketInfo.data();
}

void GPUUploadManagerImpl::GetStats(GPUUploadManagerStats& Stats)
{
    if (m_Stopping.load(std::memory_order_acquire))
    {
        DEV_ERROR("GPU upload manager has been stopped");
        Stats = GPUUploadManagerStats{};
        return;
    }

    m_StreamStats.resize(m_Streams.size());
    for (size_t i = 0; i < m_Streams.size(); ++i)
    {
        m_Streams[i]->GetStats(m_StreamStats[i]);
    }

    Stats.pStreamStats = m_StreamStats.data();
    Stats.NumStreams   = static_cast<Uint32>(m_StreamStats.size());

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
        ss << "Stream ";
        if (StreamStats.Format == TEX_FORMAT_UNKNOWN)
        {
            ss << GetMemorySizeString(StreamStats.PageSize) << std::endl;
        }
        else
        {
            const TextureFormatAttribs& FmtAttribs      = GetTextureFormatAttribs(StreamStats.Format);
            const Uint32                PageSizeInBytes = StreamStats.PageSize * StreamStats.PageSize * FmtAttribs.GetElementSize() / (FmtAttribs.BlockWidth * FmtAttribs.BlockHeight);
            ss << GetTextureFormatAttribs(StreamStats.Format).Name << ' ' << StreamStats.PageSize << 'x' << StreamStats.PageSize
               << " (" << GetMemorySizeString(PageSizeInBytes) << ')' << std::endl;
        }
        ss << "  NumPages: " << StreamStats.NumPages << std::endl;

        for (Uint32 i = 0; i < StreamStats.NumBuckets; ++i)
        {
            ss << "      ";
            const GPUUploadManagerBucketInfo& BucketInfo = StreamStats.pBucketInfo[i];
            if (StreamStats.Format == TEX_FORMAT_UNKNOWN)
            {
                ss << GetMemorySizeString(BucketInfo.PageSize);
            }
            else
            {
                ss << BucketInfo.PageSize << 'x' << BucketInfo.PageSize;
            }
            ss << ": ";
            for (Uint32 j = 0; j < BucketInfo.NumPages; ++j)
            {
                ss << '#';
            }
            ss << " " << std::to_string(BucketInfo.NumPages) << std::endl;
        }

        ss << "  NumFreePages: " << StreamStats.NumFreePages << std::endl
           << "  PeakNumPages: " << StreamStats.PeakNumPages << std::endl;

        if (StreamStats.Format == TEX_FORMAT_UNKNOWN)
        {
            ss << "  PeakTotalPendingUpdateSize: " << GetMemorySizeString(StreamStats.PeakTotalPendingUpdateSize) << std::endl
               << "  PeakUpdateSize: " << GetMemorySizeString(StreamStats.PeakUpdateSize) << std::endl;
        }
        else
        {
            ss << "  PeakUpdateDim: " << StreamStats.PeakUpdateSize << std::endl;
        }
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
