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

#include "RenderDeviceWebGPUImpl.hpp"
#include "DeviceContextWebGPUImpl.hpp"
#include "RenderPassWebGPUImpl.hpp"
#include "TextureWebGPUImpl.hpp"
#include "BufferWebGPUImpl.hpp"
#include "PipelineStateWebGPUImpl.hpp"
#include "PipelineResourceSignatureWebGPUImpl.hpp"
#include "PipelineResourceAttribsWebGPU.hpp"
#include "ShaderResourceCacheWebGPU.hpp"
#include "RenderPassWebGPUImpl.hpp"
#include "ShaderWebGPUImpl.hpp"
#include "FramebufferWebGPUImpl.hpp"
#include "SamplerWebGPUImpl.hpp"
#include "FenceWebGPUImpl.hpp"
#include "QueryWebGPUImpl.hpp"
#include "QueueSignalPoolWebGPU.hpp"
#include "AttachmentCleanerWebGPU.hpp"

namespace Diligent
{

class BottomLevelASWebGPUImpl
{};

class TopLevelASWebGPUImpl
{};

class ShaderBindingTableWebGPUImpl
{};

class DeviceMemoryWebGPUImpl
{};

static void DebugMessengerCallback(WGPUErrorType MessageType, const char* Message, void* pUserData)
{
    if (Message != nullptr)
        LOG_DEBUG_MESSAGE(DEBUG_MESSAGE_SEVERITY_ERROR, "WebGPU: ", Message);
}

RenderDeviceWebGPUImpl::RenderDeviceWebGPUImpl(IReferenceCounters*           pRefCounters,
                                               IMemoryAllocator&             RawMemAllocator,
                                               IEngineFactory*               pEngineFactory,
                                               const EngineWebGPUCreateInfo& EngineCI,
                                               const GraphicsAdapterInfo&    AdapterInfo,
                                               WGPUInstance                  wgpuInstance,
                                               WGPUAdapter                   wgpuAdapter,
                                               WGPUDevice                    wgpuDevice) :

    // clang-format off
    TRenderDeviceBase
    {
        pRefCounters,
        RawMemAllocator,
        pEngineFactory,
        EngineCI,
        AdapterInfo
    },
    m_wgpuInstance(wgpuInstance),
    m_wgpuAdapter{wgpuAdapter},
    m_wgpuDevice{wgpuDevice}
// clang-format on
{
    wgpuDeviceSetUncapturedErrorCallback(m_wgpuDevice.Get(), DebugMessengerCallback, nullptr);

    m_DeviceInfo.Type     = RENDER_DEVICE_TYPE_WEBGPU;
    m_DeviceInfo.Features = EnableDeviceFeatures(m_AdapterInfo.Features, EngineCI.Features);
    m_pQueueSignalPool.reset(new QueueSignalPoolWebGPU{this, EngineCI.QueueSignalPoolSize});
    m_pMemoryManager.reset(new SharedMemoryManagerWebGPU{m_wgpuDevice.Get(), EngineCI.DynamicHeapPageSize});
    m_pAttachmentCleaner.reset(new AttachmentCleanerWebGPU{m_wgpuDevice.Get()});
}

RenderDeviceWebGPUImpl::~RenderDeviceWebGPUImpl() = default;

void RenderDeviceWebGPUImpl::CreateBuffer(const BufferDesc& BuffDesc,
                                          const BufferData* pBuffData,
                                          IBuffer**         ppBuffer)
{
    CreateBufferImpl(ppBuffer, BuffDesc, pBuffData);
}

void RenderDeviceWebGPUImpl::CreateTexture(const TextureDesc& TexDesc,
                                           const TextureData* pData,
                                           ITexture**         ppTexture)
{
    CreateTextureImpl(ppTexture, TexDesc, pData);
}

void RenderDeviceWebGPUImpl::CreateSampler(const SamplerDesc& SamplerDesc,
                                           ISampler**         ppSampler)
{
    CreateSamplerImpl(ppSampler, SamplerDesc);
}

void RenderDeviceWebGPUImpl::CreateShader(const ShaderCreateInfo& ShaderCI,
                                          IShader**               ppShader,
                                          IDataBlob**             ppCompilerOutput)
{
    const ShaderWebGPUImpl::CreateInfo wgpuShaderCI{
        GetDeviceInfo(),
        GetAdapterInfo(),
        ppCompilerOutput,
        m_pShaderCompilationThreadPool,
    };
    CreateShaderImpl(ppShader, ShaderCI, wgpuShaderCI);
}

void RenderDeviceWebGPUImpl::CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                             IPipelineResourceSignature**         ppSignature)
{
    CreatePipelineResourceSignature(Desc, ppSignature, SHADER_TYPE_UNKNOWN, false);
}

void RenderDeviceWebGPUImpl::CreateGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
                                                         IPipelineState**                       ppPipelineState)
{
    CreatePipelineStateImpl(ppPipelineState, PSOCreateInfo);
}

void RenderDeviceWebGPUImpl::CreateComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo,
                                                        IPipelineState**                      ppPipelineState)
{
    CreatePipelineStateImpl(ppPipelineState, PSOCreateInfo);
}

void RenderDeviceWebGPUImpl::CreateRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
                                                           IPipelineState**                         ppPipelineState)
{
    UNSUPPORTED("Ray tracing is not supported in WebGPU");
    *ppPipelineState = nullptr;
}

void RenderDeviceWebGPUImpl::CreateFence(const FenceDesc& Desc,
                                         IFence**         ppFence)
{
    CreateFenceImpl(ppFence, Desc);
}

void RenderDeviceWebGPUImpl::CreateQuery(const QueryDesc& Desc,
                                         IQuery**         ppQuery)
{
    CreateQueryImpl(ppQuery, Desc);
}

void RenderDeviceWebGPUImpl::CreateRenderPass(const RenderPassDesc& Desc,
                                              IRenderPass**         ppRenderPass)
{
    CreateRenderPassImpl(ppRenderPass, Desc);
}

void RenderDeviceWebGPUImpl::CreateFramebuffer(const FramebufferDesc& Desc,
                                               IFramebuffer**         ppFramebuffer)
{
    CreateFramebufferImpl(ppFramebuffer, Desc);
}

void RenderDeviceWebGPUImpl::CreateBLAS(const BottomLevelASDesc& Desc,
                                        IBottomLevelAS**         ppBLAS)
{
    UNSUPPORTED("CreateBLAS is not supported in WebGPU");
    *ppBLAS = nullptr;
}

void RenderDeviceWebGPUImpl::CreateTLAS(const TopLevelASDesc& Desc,
                                        ITopLevelAS**         ppTLAS)
{
    UNSUPPORTED("CreateTLAS is not supported in WebGPU");
    *ppTLAS = nullptr;
}

void RenderDeviceWebGPUImpl::CreateSBT(const ShaderBindingTableDesc& Desc,
                                       IShaderBindingTable**         ppSBT)
{
    UNSUPPORTED("CreateSBT is not supported in WebGPU");
    *ppSBT = nullptr;
}

void RenderDeviceWebGPUImpl::CreateDeviceMemory(const DeviceMemoryCreateInfo& CreateInfo,
                                                IDeviceMemory**               ppMemory)
{
    UNSUPPORTED("CreateDeviceMemory is not supported in WebGPU");
    *ppMemory = nullptr;
}

void RenderDeviceWebGPUImpl::CreatePipelineStateCache(const PipelineStateCacheCreateInfo& CreateInfo,
                                                      IPipelineStateCache**               ppPSOCache)
{
    UNSUPPORTED("CreatePipelineStateCache is not supported in WebGPU");
    *ppPSOCache = nullptr;
}

SparseTextureFormatInfo RenderDeviceWebGPUImpl::GetSparseTextureFormatInfo(TEXTURE_FORMAT     TexFormat,
                                                                           RESOURCE_DIMENSION Dimension,
                                                                           Uint32             SampleCount) const
{
    UNSUPPORTED("GetSparseTextureFormatInfo is not supported in WebGPU");
    return {};
}

WGPUInstance RenderDeviceWebGPUImpl::GetWebGPUInstance() const
{
    return m_wgpuInstance.Get();
}

WGPUAdapter RenderDeviceWebGPUImpl::GetWebGPUAdapter() const
{
    return m_wgpuAdapter.Get();
}

WGPUDevice RenderDeviceWebGPUImpl::GetWebGPUDevice() const
{
    return m_wgpuDevice.Get();
}

void RenderDeviceWebGPUImpl::IdleGPU()
{
    // TODO How to wait GPU in Web?
    PollEvents(true);
}

void RenderDeviceWebGPUImpl::CreateTextureFromWebGPUTexture(WGPUTexture        wgpuTexture,
                                                            const TextureDesc& TexDesc,
                                                            RESOURCE_STATE     InitialState,
                                                            ITexture**         ppTexture)
{
    CreateTextureImpl(ppTexture, TexDesc, InitialState, wgpuTexture);
}

void RenderDeviceWebGPUImpl::CreateBufferFromWebGPUBuffer(WGPUBuffer        wgpuBuffer,
                                                          const BufferDesc& BuffDesc,
                                                          RESOURCE_STATE    InitialState,
                                                          IBuffer**         ppBuffer)
{
    CreateBufferImpl(ppBuffer, BuffDesc, InitialState, wgpuBuffer);
}

void RenderDeviceWebGPUImpl::CreatePipelineResourceSignature(const PipelineResourceSignatureDesc&               Desc,
                                                             const PipelineResourceSignatureInternalDataWebGPU& InternalData,
                                                             IPipelineResourceSignature**                       ppSignature)
{

    CreatePipelineResourceSignatureImpl(ppSignature, Desc, InternalData);
}

void RenderDeviceWebGPUImpl::CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                             IPipelineResourceSignature**         ppSignature,
                                                             SHADER_TYPE                          ShaderStages,
                                                             bool                                 IsDeviceInternal)
{
    CreatePipelineResourceSignatureImpl(ppSignature, Desc, ShaderStages, IsDeviceInternal);
}

QueueSignalPoolWebGPU& RenderDeviceWebGPUImpl::GetQueueSignalPool() const
{
    return *m_pQueueSignalPool.get();
}

AttachmentCleanerWebGPU& RenderDeviceWebGPUImpl::GetAttachmentCleaner() const
{
    return *m_pAttachmentCleaner.get();
}

SharedMemoryManagerWebGPU::Page RenderDeviceWebGPUImpl::GetSharedMemoryPage(Uint64 Size)
{
    return m_pMemoryManager->GetPage(Size);
}

void RenderDeviceWebGPUImpl::PollEvents(bool YieldToWebBrowser)
{
#if PLATFORM_EMSCRIPTEN
    if (YieldToWebBrowser)
        emscripten_sleep(1);
#else
    (void)YieldToWebBrowser;
    wgpuDeviceTick(m_wgpuDevice.Get());
#endif
}

void RenderDeviceWebGPUImpl::TestTextureFormat(TEXTURE_FORMAT TexFormat)
{
    UNSUPPORTED("TestTextureFormat is not supported in WebGPU");
}

} // namespace Diligent
