/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#include "pch.h"

#include "TextureWebGPUImpl.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "DeviceContextWebGPUImpl.hpp"
#include "WebGPUTypeConversions.hpp"

namespace Diligent
{

namespace
{

WGPUTextureDescriptor TextureDescToWGPUTextureDescriptor(const TextureDesc&            Desc,
                                                         const RenderDeviceWebGPUImpl* pRenderDevice) noexcept
{
    WGPUTextureDescriptor wgpuTextureDesc{};

    if (Desc.Type == RESOURCE_DIM_TEX_CUBE)
        DEV_CHECK_ERR(Desc.ArraySize == 6, "Cube textures are expected to have exactly 6 array slices");
    if (Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
        DEV_CHECK_ERR(Desc.ArraySize % 6 == 0, "Cube texture arrays are expected to have a number of array slices that is a multiple of 6");

    if (Desc.IsArray())
        wgpuTextureDesc.size.depthOrArrayLayers = Desc.ArraySize;
    else if (Desc.Is3D())
        wgpuTextureDesc.size.depthOrArrayLayers = Desc.Depth;
    else
        wgpuTextureDesc.size.depthOrArrayLayers = 1;

    if (Desc.Is1D())
        wgpuTextureDesc.dimension = WGPUTextureDimension_1D;
    else if (Desc.Is2D())
        wgpuTextureDesc.dimension = WGPUTextureDimension_2D;
    else if (Desc.Is3D())
        wgpuTextureDesc.dimension = WGPUTextureDimension_3D;
    else
        LOG_ERROR_AND_THROW("Unknown texture type");

    wgpuTextureDesc.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;
    if (Desc.BindFlags & (BIND_RENDER_TARGET | BIND_DEPTH_STENCIL))
        wgpuTextureDesc.usage |= WGPUTextureUsage_RenderAttachment;
    if (Desc.BindFlags & BIND_UNORDERED_ACCESS || Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS)
        wgpuTextureDesc.usage |= WGPUTextureUsage_StorageBinding;
    if (Desc.BindFlags & BIND_SHADER_RESOURCE)
        wgpuTextureDesc.usage |= WGPUTextureUsage_TextureBinding;

    if (IsSRGBFormat(Desc.Format) && wgpuTextureDesc.usage & WGPUTextureUsage_StorageBinding)
        wgpuTextureDesc.format = TextureFormatToWGPUFormat(SRGBFormatToUnorm(Desc.Format));
    else
        wgpuTextureDesc.format = TextureFormatToWGPUFormat(Desc.Format);

    wgpuTextureDesc.mipLevelCount = Desc.MipLevels;
    wgpuTextureDesc.sampleCount   = Desc.SampleCount;
    wgpuTextureDesc.size.width    = Desc.GetWidth();
    wgpuTextureDesc.size.height   = Desc.GetHeight();
    wgpuTextureDesc.label         = Desc.Name;

    return wgpuTextureDesc;
}

WGPUTextureViewDescriptor TextureViewDescToWGPUTextureViewDescriptor(const TextureDesc&            TexDesc,
                                                                     TextureViewDesc&              ViewDesc,
                                                                     const RenderDeviceWebGPUImpl* pRenderDevice) noexcept
{
    if (ViewDesc.Format == TEX_FORMAT_UNKNOWN)
        ViewDesc.Format = TexDesc.Format;

    WGPUTextureViewDescriptor wgpuTextureViewDesc{};
    wgpuTextureViewDesc.dimension     = ResourceDimensionToWGPUTextureViewDimension(ViewDesc.TextureDim);
    wgpuTextureViewDesc.baseMipLevel  = ViewDesc.MostDetailedMip;
    wgpuTextureViewDesc.mipLevelCount = ViewDesc.NumMipLevels;

    if (TexDesc.IsArray())
    {
        wgpuTextureViewDesc.baseArrayLayer  = ViewDesc.FirstArraySlice;
        wgpuTextureViewDesc.arrayLayerCount = ViewDesc.NumArraySlices;
    }
    else
    {
        wgpuTextureViewDesc.baseArrayLayer  = 0;
        wgpuTextureViewDesc.arrayLayerCount = 1;
    }

    const auto& FmtAttribs = GetTextureFormatAttribs(ViewDesc.Format);

    if (ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL)
    {
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH)
            wgpuTextureViewDesc.aspect = WGPUTextureAspect_DepthOnly;
        else if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
            wgpuTextureViewDesc.aspect = WGPUTextureAspect_All;
        else
            UNEXPECTED("Unexpected component type for a depth-stencil view format");
    }
    else
    {
        wgpuTextureViewDesc.aspect = WGPUTextureAspect_All;
    }

    return wgpuTextureViewDesc;
}

Uint64 WebGPUGetTextureLocationOffset(const TextureDesc& TexDesc,
                                      Uint32             ArraySlice,
                                      Uint32             MipLevel,
                                      Uint32             BlockHeight,
                                      Uint32             ByteRawStride)
{
    VERIFY_EXPR(TexDesc.MipLevels > 0 && TexDesc.GetArraySize() > 0 && TexDesc.Width > 0 && TexDesc.Height > 0 && TexDesc.Format != TEX_FORMAT_UNKNOWN);
    VERIFY_EXPR(ArraySlice < TexDesc.GetArraySize() && MipLevel < TexDesc.MipLevels || ArraySlice == TexDesc.GetArraySize() && MipLevel == 0);

    Uint64 Offset = 0;
    if (ArraySlice > 0)
    {
        Uint64 ArraySliceSize = 0;
        for (Uint32 MipIdx = 0; MipIdx < TexDesc.MipLevels; ++MipIdx)
        {
            const auto MipInfo        = GetMipLevelProperties(TexDesc, MipIdx);
            const auto DepthSliceSize = AlignUp(MipInfo.RowSize, ByteRawStride) * (MipInfo.StorageHeight / BlockHeight);
            ArraySliceSize += DepthSliceSize * MipInfo.Depth;
        }

        Offset = ArraySliceSize;
        if (TexDesc.IsArray())
            Offset *= ArraySlice;
    }

    for (Uint32 MipIdx = 0; MipIdx < MipLevel; ++MipIdx)
    {
        const auto MipInfo        = GetMipLevelProperties(TexDesc, MipIdx);
        const auto DepthSliceSize = AlignUp(MipInfo.RowSize, ByteRawStride) * (MipInfo.StorageHeight / BlockHeight);
        Offset += DepthSliceSize * MipInfo.Depth;
    }

    return Offset;
}

} // namespace

TextureWebGPUImpl::TextureWebGPUImpl(IReferenceCounters*        pRefCounters,
                                     FixedBlockMemoryAllocator& TexViewObjAllocator,
                                     RenderDeviceWebGPUImpl*    pDevice,
                                     const TextureDesc&         Desc,
                                     const TextureData*         pInitData) :
    // clang-format off
    TTextureBase
    {
        pRefCounters,
        TexViewObjAllocator,
        pDevice,
        Desc
    }
// clang-format on
{
    if (m_Desc.Usage == USAGE_IMMUTABLE && (pInitData == nullptr || pInitData->pSubResources == nullptr))
        LOG_ERROR_AND_THROW("Immutable textures must be initialized with data at creation time: pInitData can't be null");

    const auto& FmtAttribs          = GetTextureFormatAttribs(m_Desc.Format);
    const auto  IsInitializeTexture = (pInitData != nullptr && pInitData->pSubResources != nullptr && pInitData->NumSubresources > 0);

    if (m_Desc.Usage == USAGE_IMMUTABLE || m_Desc.Usage == USAGE_DEFAULT || m_Desc.Usage == USAGE_DYNAMIC)
    {
        WGPUTextureDescriptor wgpuTextureDesc = TextureDescToWGPUTextureDescriptor(m_Desc, pDevice);
        m_wgpuTexture.Reset(wgpuDeviceCreateTexture(pDevice->GetWebGPUDevice(), &wgpuTextureDesc));
        if (!m_wgpuTexture)
            LOG_ERROR_AND_THROW("Failed to create WebGPU texture ", " '", m_Desc.Name ? m_Desc.Name : "", '\'');

        if (IsInitializeTexture)
        {
            WGPUBufferDescriptor wgpuBufferDesc{};
            wgpuBufferDesc.usage            = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
            wgpuBufferDesc.size             = WebGPUGetTextureLocationOffset(m_Desc, m_Desc.GetArraySize(), 0, FmtAttribs.BlockHeight, CopyTextureRawStride);
            wgpuBufferDesc.mappedAtCreation = true;

            WebGPUBufferWrapper wgpuUploadBuffer{wgpuDeviceCreateBuffer(pDevice->GetWebGPUDevice(), &wgpuBufferDesc)};
            if (!wgpuUploadBuffer)
                LOG_ERROR_AND_THROW("Failed to create WebGPU texture upload buffer");

            auto* pUploadData = static_cast<uint8_t*>(wgpuBufferGetMappedRange(wgpuUploadBuffer.Get(), 0, StaticCast<size_t>(wgpuBufferDesc.size)));

            WGPUCommandEncoderDescriptor wgpuEncoderDesc{};
            WGPUCommandEncoder           wgpuCmdEncoder = wgpuDeviceCreateCommandEncoder(pDevice->GetWebGPUDevice(), &wgpuEncoderDesc);

            Uint32 CurrSubRes = 0;
            for (Uint32 LayerIdx = 0; LayerIdx < m_Desc.GetArraySize(); ++LayerIdx)
            {
                for (Uint32 MipIdx = 0; MipIdx < m_Desc.MipLevels; ++MipIdx)
                {
                    const auto  MipProps   = GetMipLevelProperties(m_Desc, MipIdx);
                    const auto& SubResData = pInitData->pSubResources[CurrSubRes++];

                    const auto DstSubResOffset = WebGPUGetTextureLocationOffset(m_Desc, LayerIdx, MipIdx, FmtAttribs.BlockHeight, CopyTextureRawStride);
                    const auto DstRawStride    = AlignUp(MipProps.RowSize, CopyTextureRawStride);
                    const auto DstDepthStride  = DstRawStride * (MipProps.StorageHeight / FmtAttribs.BlockHeight);

                    CopyTextureSubresource(SubResData,
                                           MipProps.StorageHeight / FmtAttribs.BlockHeight, // NumRows
                                           MipProps.Depth,
                                           MipProps.RowSize,
                                           pUploadData + DstSubResOffset,
                                           DstRawStride,  // DstRowStride
                                           DstDepthStride // DstDepthStride
                    );

                    WGPUImageCopyBuffer wgpuSourceCopyInfo{};
                    wgpuSourceCopyInfo.layout.offset       = DstSubResOffset;
                    wgpuSourceCopyInfo.layout.bytesPerRow  = static_cast<Uint32>(DstRawStride);
                    wgpuSourceCopyInfo.layout.rowsPerImage = static_cast<Uint32>(DstDepthStride / DstRawStride);
                    wgpuSourceCopyInfo.buffer              = wgpuUploadBuffer.Get();

                    WGPUImageCopyTexture wgpuDestinationCopyInfo{};
                    wgpuDestinationCopyInfo.texture  = m_wgpuTexture.Get();
                    wgpuDestinationCopyInfo.mipLevel = MipIdx;
                    wgpuDestinationCopyInfo.origin   = {0, 0, LayerIdx};
                    wgpuDestinationCopyInfo.aspect   = WGPUTextureAspect_All;

                    WGPUExtent3D wgpuCopySize{};
                    wgpuCopySize.width              = MipProps.LogicalWidth;
                    wgpuCopySize.height             = MipProps.LogicalHeight;
                    wgpuCopySize.depthOrArrayLayers = MipProps.Depth;

                    wgpuCommandEncoderCopyBufferToTexture(wgpuCmdEncoder, &wgpuSourceCopyInfo, &wgpuDestinationCopyInfo, &wgpuCopySize);
                }
            }

            wgpuBufferUnmap(wgpuUploadBuffer.Get());

            WGPUCommandBufferDescriptor wgpuCmdBufferDesc{};
            WGPUCommandBuffer           wgpuCmdBuffer = wgpuCommandEncoderFinish(wgpuCmdEncoder, &wgpuCmdBufferDesc);
            wgpuQueueSubmit(wgpuDeviceGetQueue(pDevice->GetWebGPUDevice()), 1, &wgpuCmdBuffer);
        }
    }
    else if (m_Desc.Usage == USAGE_STAGING)
    {
        String StagingBufferName = "Staging buffer for '";
        StagingBufferName += m_Desc.Name;
        StagingBufferName += '\'';

        WGPUBufferDescriptor wgpuBufferDesc{};
        wgpuBufferDesc.size  = GetStagingTextureSubresourceOffset(m_Desc, m_Desc.GetArraySize(), 0, StagingDataAlignment);
        wgpuBufferDesc.label = StagingBufferName.c_str();
        m_MappedData.resize(StaticCast<size_t>(wgpuBufferDesc.size));

        if (m_Desc.CPUAccessFlags & CPU_ACCESS_READ)
        {
            DEV_CHECK_ERR(!IsInitializeTexture, "Readback textures should not be initialized with data");
            wgpuBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
            SetState(RESOURCE_STATE_COPY_DEST);
        }
        else if (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE)
        {
            wgpuBufferDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
            SetState(RESOURCE_STATE_COPY_SOURCE);
        }
        else
            UNEXPECTED("Unexpected CPU access");

        m_wgpuStagingBuffer.Reset(wgpuDeviceCreateBuffer(pDevice->GetWebGPUDevice(), &wgpuBufferDesc));
        if (!m_wgpuStagingBuffer)
            LOG_ERROR_AND_THROW("Failed to create WebGPU buffer ", " '", StagingBufferName, '\'');

        if (IsInitializeTexture)
        {
            auto* const pStagingData = static_cast<Uint8*>(wgpuBufferGetMappedRange(m_wgpuStagingBuffer.Get(), 0, StaticCast<size_t>(wgpuBufferDesc.size)));

            Uint32 CurrSubRes = 0;
            for (Uint32 LayerIdx = 0; LayerIdx < m_Desc.GetArraySize(); ++LayerIdx)
            {
                for (Uint32 MipIdx = 0; MipIdx < m_Desc.MipLevels; ++MipIdx)
                {
                    const auto  MipProps        = GetMipLevelProperties(m_Desc, MipIdx);
                    const auto& SubResData      = pInitData->pSubResources[CurrSubRes++];
                    const auto  DstSubResOffset = GetStagingTextureSubresourceOffset(m_Desc, LayerIdx, MipIdx, StagingDataAlignment);

                    CopyTextureSubresource(SubResData,
                                           MipProps.StorageHeight / FmtAttribs.BlockHeight, // NumRows
                                           MipProps.Depth,
                                           MipProps.RowSize,
                                           pStagingData + DstSubResOffset,
                                           MipProps.RowSize,       // DstRowStride
                                           MipProps.DepthSliceSize // DstDepthStride
                    );
                }
            }

            wgpuBufferUnmap(m_wgpuStagingBuffer.Get());
        }
    }
    else
    {
        UNSUPPORTED("Unsupported usage ", GetUsageString(m_Desc.Usage));
    }

    SetState(RESOURCE_STATE_UNDEFINED);
}

TextureWebGPUImpl::TextureWebGPUImpl(IReferenceCounters*        pRefCounters,
                                     FixedBlockMemoryAllocator& TexViewObjAllocator,
                                     RenderDeviceWebGPUImpl*    pDevice,
                                     const TextureDesc&         Desc,
                                     RESOURCE_STATE             InitialState,
                                     WGPUTexture                wgpuTextureHandle) :
    TTextureBase{pRefCounters, TexViewObjAllocator, pDevice, Desc},
    m_wgpuTexture{wgpuTextureHandle, {true}}
{
    SetState(InitialState);
}

Uint64 TextureWebGPUImpl::GetNativeHandle()
{
    return reinterpret_cast<Uint64>(GetWebGPUTexture());
}

WGPUTexture TextureWebGPUImpl::GetWebGPUTexture() const
{
    return m_wgpuTexture.Get();
}

WGPUBuffer TextureWebGPUImpl::GetWebGPUStagingBuffer() const
{
    return m_wgpuStagingBuffer.Get();
}

void* TextureWebGPUImpl::Map(MAP_TYPE MapType, Uint32 MapFlags)
{
    VERIFY(m_Desc.Usage == USAGE_STAGING, "Map working only for staging buffers");

    if (MapType == MAP_READ)
    {
        struct CallbackCaptureData
        {
            TextureWebGPUImpl* pTexture;
            bool               IsMapped;
        } CallbackCapture{this, false};

        auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
            if (MapStatus == WGPUBufferMapAsyncStatus_Success)
            {
                auto*       pCaptureData = static_cast<CallbackCaptureData*>(pUserData);
                auto*       pTexture     = pCaptureData->pTexture;
                const auto  DataSize     = pTexture->m_MappedData.size();
                const auto* pData        = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(pTexture->m_wgpuStagingBuffer.Get(), 0, DataSize));
                VERIFY_EXPR(pUserData != nullptr);
                memcpy(pTexture->m_MappedData.data(), pData, DataSize);
                wgpuBufferUnmap(pTexture->m_wgpuStagingBuffer.Get());
                pCaptureData->IsMapped = true;
            }
            else
            {
                DEV_ERROR("Failed wgpuBufferMapAsync: ", MapStatus);
            }
        };

        wgpuBufferMapAsync(m_wgpuStagingBuffer.Get(), WGPUMapMode_Read, 0, m_MappedData.size(), MapAsyncCallback, &CallbackCapture);
        while (!CallbackCapture.IsMapped)
            m_pDevice->PollEvents(false);

        m_MapState = TextureMapState::Read;
        return m_MappedData.data();
    }
    else if (MapType == MAP_WRITE)
    {
        m_MapState = TextureMapState::Write;
        return m_MappedData.data();
    }
    else if (MapType == MAP_READ_WRITE)
    {
        LOG_ERROR("MAP_READ_WRITE is not supported in WebGPU backend");
    }
    else
    {
        UNEXPECTED("Unknown map type");
    }
    return nullptr;
}

void TextureWebGPUImpl::Unmap()
{
    VERIFY(m_Desc.Usage == USAGE_STAGING, "Unmap working only for staging buffers");

    if (m_MapState == TextureMapState::Read)
    {
        // Nothing to do
    }
    else if (m_MapState == TextureMapState::Write)
    {
        struct CallbackCaptureData
        {
            TextureWebGPUImpl* pTexture;
            bool               IsMapped;
        } CallbackCapture{this, false};

        auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
            if (MapStatus == WGPUBufferMapAsyncStatus_Success)
            {

                auto*       pCaptureData = static_cast<CallbackCaptureData*>(pUserData);
                auto*       pTexture     = pCaptureData->pTexture;
                const auto  DataSize     = pTexture->m_MappedData.size();
                auto* const pData        = static_cast<uint8_t*>(wgpuBufferGetMappedRange(pTexture->m_wgpuStagingBuffer.Get(), 0, DataSize));
                VERIFY_EXPR(pUserData != nullptr);
                memcpy(pData, pTexture->m_MappedData.data(), DataSize);
                wgpuBufferUnmap(pTexture->m_wgpuStagingBuffer.Get());
                pCaptureData->IsMapped = true;
            }
            else
            {
                DEV_ERROR("Failed wgpuBufferMapAsync: ", MapStatus);
            }
        };

        wgpuBufferMapAsync(m_wgpuStagingBuffer.Get(), WGPUMapMode_Write, 0, m_MappedData.size(), MapAsyncCallback, &CallbackCapture);
        while (!CallbackCapture.IsMapped)
            m_pDevice->PollEvents(false);
    }
    else
    {
        UNEXPECTED("Call TextureWebGPUImpl::Map method before TextureWebGPUImpl::Unmap");
    }
    m_MapState = TextureMapState::None;
}

void TextureWebGPUImpl::CreateViewInternal(const TextureViewDesc& ViewDesc, ITextureView** ppView, bool bIsDefaultView)
{

    VERIFY(ppView != nullptr, "View pointer address is null");
    if (!ppView) return;
    VERIFY(*ppView == nullptr, "Overwriting reference to existing object may cause memory leaks");

    *ppView = nullptr;

    try
    {
        auto& TexViewAllocator = m_pDevice->GetTexViewObjAllocator();
        VERIFY(&TexViewAllocator == &m_dbgTexViewObjAllocator, "Texture view allocator does not match allocator provided during texture initialization");

        auto UpdatedViewDesc = ViewDesc;
        ValidatedAndCorrectTextureViewDesc(m_Desc, UpdatedViewDesc);
        auto wgpuTextureViewDesc = TextureViewDescToWGPUTextureViewDescriptor(m_Desc, UpdatedViewDesc, m_pDevice);

        WebGPUTextureViewWrapper wgpuTextureView{wgpuTextureCreateView(m_wgpuTexture.Get(), &wgpuTextureViewDesc)};
        if (!wgpuTextureView)
            LOG_ERROR_AND_THROW("Failed to create WebGPU texture view ", " '", ViewDesc.Name ? ViewDesc.Name : "", '\'');

        std::vector<WebGPUTextureViewWrapper> wgpuTextureMipSRVs;
        std::vector<WebGPUTextureViewWrapper> wgpuTextureMipUAVs;
        if (UpdatedViewDesc.Flags & TEXTURE_VIEW_FLAG_ALLOW_MIP_MAP_GENERATION)
        {
            VERIFY_EXPR((m_Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS) != 0 && m_Desc.Is2D());

            for (Uint32 MipLevel = 0; MipLevel < m_Desc.MipLevels; ++MipLevel)
            {
                TextureViewDesc TexMipSRVDesc = UpdatedViewDesc;
                TexMipSRVDesc.TextureDim      = RESOURCE_DIM_TEX_2D_ARRAY;
                TexMipSRVDesc.ViewType        = TEXTURE_VIEW_UNORDERED_ACCESS;
                TexMipSRVDesc.MostDetailedMip = MipLevel;
                TexMipSRVDesc.NumMipLevels    = 1;

                auto wgpuTextureViewDescSRV = TextureViewDescToWGPUTextureViewDescriptor(m_Desc, TexMipSRVDesc, m_pDevice);
                wgpuTextureMipSRVs.emplace_back(wgpuTextureCreateView(m_wgpuTexture.Get(), &wgpuTextureViewDescSRV));

                if (!wgpuTextureMipSRVs.back())
                    LOG_ERROR_AND_THROW("Failed to create WebGPU texture view ", " '", ViewDesc.Name ? ViewDesc.Name : "", '\'');
            }

            for (Uint32 MipLevel = 0; MipLevel < m_Desc.MipLevels; ++MipLevel)
            {
                TextureViewDesc TexMipUAVDesc = UpdatedViewDesc;
                TexMipUAVDesc.TextureDim      = RESOURCE_DIM_TEX_2D_ARRAY;
                TexMipUAVDesc.ViewType        = TEXTURE_VIEW_UNORDERED_ACCESS;
                TexMipUAVDesc.MostDetailedMip = MipLevel;
                TexMipUAVDesc.NumMipLevels    = 1;
                TexMipUAVDesc.Format          = SRGBFormatToUnorm(TexMipUAVDesc.Format);

                auto wgpuTextureViewDescUAV = TextureViewDescToWGPUTextureViewDescriptor(m_Desc, TexMipUAVDesc, m_pDevice);
                wgpuTextureMipUAVs.emplace_back(wgpuTextureCreateView(m_wgpuTexture.Get(), &wgpuTextureViewDescUAV));

                if (!wgpuTextureMipUAVs.back())
                    LOG_ERROR_AND_THROW("Failed to create WebGPU texture view ", " '", ViewDesc.Name ? ViewDesc.Name : "", '\'');
            }
        }

        const auto pViewWebGPU = NEW_RC_OBJ(TexViewAllocator, "TextureViewWebGPUImpl instance", TextureViewWebGPUImpl, bIsDefaultView ? this : nullptr)(
            GetDevice(), UpdatedViewDesc, this, std::move(wgpuTextureView), std::move(wgpuTextureMipSRVs), std::move(wgpuTextureMipUAVs), bIsDefaultView);
        VERIFY(pViewWebGPU->GetDesc().ViewType == ViewDesc.ViewType, "Incorrect view type");

        if (bIsDefaultView)
            *ppView = pViewWebGPU;
        else
            pViewWebGPU->QueryInterface(IID_TextureView, reinterpret_cast<IObject**>(ppView));
    }
    catch (const std::runtime_error&)
    {
        const auto* ViewTypeName = GetTexViewTypeLiteralName(ViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", ViewDesc.Name ? ViewDesc.Name : "", "\" (", ViewTypeName, ") for texture \"", m_Desc.Name ? m_Desc.Name : "", "\"");
    }
}

} // namespace Diligent
