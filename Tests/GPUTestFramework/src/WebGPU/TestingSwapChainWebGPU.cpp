/*
 *  Copyright 2023-2026 Diligent Graphics LLC
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


#include <atomic>

#include "WebGPU/TestingSwapChainWebGPU.hpp"
#include "WebGPU/TestingEnvironmentWebGPU.hpp"

#include "RenderDeviceWebGPU.h"
#include "DeviceContextWebGPU.h"
#include "TextureWebGPU.h"
#include "Align.hpp"
#include "../../../../Graphics/GraphicsEngineWebGPU/include/WebGPUDefinitions.hpp"

#if PLATFORM_WEB
#    include <emscripten.h>
#endif

namespace Diligent
{

namespace Testing
{

TestingSwapChainWebGPU::TestingSwapChainWebGPU(IReferenceCounters*       pRefCounters,
                                               TestingEnvironmentWebGPU* pEnv,
                                               const SwapChainDesc&      SCDesc) :
    TBase //
    {
        pRefCounters,
        pEnv->GetDevice(),
        pEnv->GetDeviceContext(),
        SCDesc //
    }
{
    RefCntAutoPtr<IRenderDeviceWebGPU> pRenderDeviceWebGPU{m_pDevice, IID_RenderDeviceWebGPU};
    m_wgpuDevice = pRenderDeviceWebGPU->GetWebGPUDevice();
    CreateBackendResources();
}

void TestingSwapChainWebGPU::CreateBackendResources()
{

    WGPUTextureFormat ColorFormat = WGPUTextureFormat_Undefined;
    WGPUTextureFormat DepthFormat = WGPUTextureFormat_Undefined;

    switch (m_SwapChainDesc.ColorBufferFormat)
    {
        case TEX_FORMAT_RGBA8_UNORM:
            ColorFormat = WGPUTextureFormat_RGBA8Unorm;
            break;

        default:
            UNSUPPORTED("Texture format ", GetTextureFormatAttribs(m_SwapChainDesc.ColorBufferFormat).Name, " is not a supported color buffer format");
    }

    switch (m_SwapChainDesc.DepthBufferFormat)
    {
        case TEX_FORMAT_D32_FLOAT:
            DepthFormat = WGPUTextureFormat_Depth32Float;
            break;

        default:
            UNSUPPORTED("Texture format ", GetTextureFormatAttribs(m_SwapChainDesc.DepthBufferFormat).Name, " is not a supported depth buffer format");
    }

    {
        WGPUTextureDescriptor wgpuTextureDesc{};
        wgpuTextureDesc.dimension     = WGPUTextureDimension_2D;
        wgpuTextureDesc.size          = {m_SwapChainDesc.Width, m_SwapChainDesc.Height, 1};
        wgpuTextureDesc.mipLevelCount = 1;
        wgpuTextureDesc.format        = ColorFormat;
        wgpuTextureDesc.usage         = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        wgpuTextureDesc.sampleCount   = 1;

        m_wgpuColorTexture = wgpuDeviceCreateTexture(m_wgpuDevice, &wgpuTextureDesc);
    }

    {
        WGPUTextureDescriptor wgpuTextureDesc{};
        wgpuTextureDesc.dimension     = WGPUTextureDimension_2D;
        wgpuTextureDesc.size          = {m_SwapChainDesc.Width, m_SwapChainDesc.Height, 1};
        wgpuTextureDesc.mipLevelCount = 1;
        wgpuTextureDesc.format        = DepthFormat;
        wgpuTextureDesc.usage         = WGPUTextureUsage_RenderAttachment;
        wgpuTextureDesc.sampleCount   = 1;

        m_wgpuDepthTexture = wgpuDeviceCreateTexture(m_wgpuDevice, &wgpuTextureDesc);
    }

    {
        static constexpr Uint32 CopyBytesPerRowAlignment = 256;
        m_StagingRowPitch                                = AlignUp(m_SwapChainDesc.Width * 4, CopyBytesPerRowAlignment);

        WGPUBufferDescriptor wgpuStagingBufferDesc{};
        wgpuStagingBufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        wgpuStagingBufferDesc.size  = Uint64{m_StagingRowPitch} * m_SwapChainDesc.Height;

        m_wgpuStagingBuffer = wgpuDeviceCreateBuffer(m_wgpuDevice, &wgpuStagingBufferDesc);
    }

    {
        WGPUTextureViewDescriptor wgpuTextureViewDesc{};
        wgpuTextureViewDesc.dimension       = WGPUTextureViewDimension_2D;
        wgpuTextureViewDesc.mipLevelCount   = 1;
        wgpuTextureViewDesc.arrayLayerCount = 1;
        wgpuTextureViewDesc.format          = ColorFormat;

        m_wgpuColorTextureView = wgpuTextureCreateView(m_wgpuColorTexture, &wgpuTextureViewDesc);
    }

    {
        WGPUTextureViewDescriptor wgpuTextureViewDesc{};
        wgpuTextureViewDesc.dimension       = WGPUTextureViewDimension_2D;
        wgpuTextureViewDesc.mipLevelCount   = 1;
        wgpuTextureViewDesc.arrayLayerCount = 1;
        wgpuTextureViewDesc.format          = DepthFormat;

        m_wgpuDepthTextureView = wgpuTextureCreateView(m_wgpuDepthTexture, &wgpuTextureViewDesc);
    }
}

TestingSwapChainWebGPU::~TestingSwapChainWebGPU()
{
    ReleaseBackendResources();
}

void TestingSwapChainWebGPU::ResizeBackendResources()
{
    ReleaseBackendResources();
    CreateBackendResources();
}

void TestingSwapChainWebGPU::ReleaseBackendResources()
{
    if (m_wgpuColorTextureView != nullptr)
        wgpuTextureViewRelease(m_wgpuColorTextureView);
    if (m_wgpuDepthTextureView != nullptr)
        wgpuTextureViewRelease(m_wgpuDepthTextureView);
    if (m_wgpuColorTexture != nullptr)
        wgpuTextureRelease(m_wgpuColorTexture);
    if (m_wgpuDepthTexture != nullptr)
        wgpuTextureRelease(m_wgpuDepthTexture);
    if (m_wgpuStagingBuffer != nullptr)
        wgpuBufferRelease(m_wgpuStagingBuffer);

    m_wgpuColorTexture     = nullptr;
    m_wgpuDepthTexture     = nullptr;
    m_wgpuColorTextureView = nullptr;
    m_wgpuDepthTextureView = nullptr;
    m_wgpuStagingBuffer    = nullptr;
    m_StagingRowPitch      = 0;
}

void TestingSwapChainWebGPU::TakeSnapshot(ITexture* pCopyFrom)
{
    m_ReferenceData.clear();
#if PLATFORM_WEB
    ASSERT_TRUE(emscripten_has_asyncify()) << "WebGPU snapshot readback requires Asyncify or JSPI.";
#endif

    RefCntAutoPtr<IDeviceContextWebGPU> pContextWebGPU{m_pContext, IID_DeviceContextWebGPU};

    WGPUTexture wgpuSrcTexture = m_wgpuColorTexture;
    if (pCopyFrom != nullptr)
    {
        RefCntAutoPtr<ITextureWebGPU> pSrcTextureWebGPU{pCopyFrom, IID_TextureWebGPU};
        VERIFY_EXPR(GetDesc().Width == pSrcTextureWebGPU->GetDesc().Width);
        VERIFY_EXPR(GetDesc().Height == pSrcTextureWebGPU->GetDesc().Height);
        VERIFY_EXPR(GetDesc().ColorBufferFormat == pSrcTextureWebGPU->GetDesc().Format);
        wgpuSrcTexture = pSrcTextureWebGPU->GetWebGPUTexture();
    }

    WGPUCommandEncoderDescriptor wgpuCmdEncoderDesc{};
    WGPUCommandEncoder           wgpuCmdEncoder = wgpuDeviceCreateCommandEncoder(m_wgpuDevice, &wgpuCmdEncoderDesc);

    WGPUTexelCopyTextureInfo wgpuImageCopySrc{};
    wgpuImageCopySrc.mipLevel = 0;
    wgpuImageCopySrc.texture  = wgpuSrcTexture;
    wgpuImageCopySrc.origin   = {0, 0, 0};

    WGPUTexelCopyBufferInfo wgpuImageCopyDst{};
    wgpuImageCopyDst.layout.offset       = 0;
    wgpuImageCopyDst.layout.bytesPerRow  = m_StagingRowPitch;
    wgpuImageCopyDst.layout.rowsPerImage = m_SwapChainDesc.Height;
    wgpuImageCopyDst.buffer              = m_wgpuStagingBuffer;

    WGPUExtent3D wgpuCopySize{};
    wgpuCopySize.width              = m_SwapChainDesc.Width;
    wgpuCopySize.height             = m_SwapChainDesc.Height;
    wgpuCopySize.depthOrArrayLayers = 1;

    wgpuCommandEncoderCopyTextureToBuffer(wgpuCmdEncoder, &wgpuImageCopySrc, &wgpuImageCopyDst, &wgpuCopySize);

    WGPUCommandBufferDescriptor wgpuCmdBufferDesc{};
    WGPUCommandBuffer           wgpuCmdBuffer = wgpuCommandEncoderFinish(wgpuCmdEncoder, &wgpuCmdBufferDesc);

    wgpuQueueSubmit(pContextWebGPU->GetWebGPUQueue(), 1, &wgpuCmdBuffer);
    wgpuCommandEncoderRelease(wgpuCmdEncoder);
    wgpuCommandBufferRelease(wgpuCmdBuffer);

    const size_t DataSize = size_t{m_StagingRowPitch} * m_SwapChainDesc.Height;
    // All WebGPU map results are nonzero. The callback publishes the result on success or failure.
    constexpr WGPUMapAsyncStatus    Pending{};
    std::atomic<WGPUMapAsyncStatus> MapStatus{Pending};
    WGPUBufferMapCallbackInfo       CallbackInfo{};
    CallbackInfo.mode      = DefaultWGPUCallbackMode;
    CallbackInfo.userdata1 = &MapStatus;
    CallbackInfo.callback  = [](WGPUMapAsyncStatus Status, WGPUStringView, void* pUserData, void*) {
        static_cast<std::atomic<WGPUMapAsyncStatus>*>(pUserData)->store(Status, std::memory_order_release);
    };
    wgpuBufferMapAsync(m_wgpuStagingBuffer, WGPUMapMode_Read, 0, DataSize, CallbackInfo);

#if !PLATFORM_WEB
    RefCntAutoPtr<IRenderDeviceWebGPU> pDeviceWebGPU{m_pDevice, IID_RenderDeviceWebGPU};
#endif
    while (MapStatus.load(std::memory_order_acquire) == Pending)
    {
#if PLATFORM_WEB
        emscripten_sleep(1);
#else
        wgpuInstanceProcessEvents(pDeviceWebGPU->GetWebGPUInstance());
#endif
    }
    ASSERT_EQ(MapStatus.load(), WGPUMapAsyncStatus_Success) << "Failed to map the reference staging buffer.";

    const void* pMappedData = wgpuBufferGetConstMappedRange(m_wgpuStagingBuffer, 0, DataSize);
    if (pMappedData != nullptr)
        SetReferenceData(pMappedData, m_StagingRowPitch);
    else
        ADD_FAILURE() << "The reference staging buffer has no mapped data.";
    wgpuBufferUnmap(m_wgpuStagingBuffer);
}

void CreateTestingSwapChainWebGPU(TestingEnvironmentWebGPU* pEnv,
                                  const SwapChainDesc&      SCDesc,
                                  ISwapChain**              ppSwapChain)
{
    TestingSwapChainWebGPU* pTestingSC(MakeNewRCObj<TestingSwapChainWebGPU>()(pEnv, SCDesc));
    pTestingSC->QueryInterface(IID_SwapChain, reinterpret_cast<IObject**>(ppSwapChain));
}

} // namespace Testing

} // namespace Diligent
