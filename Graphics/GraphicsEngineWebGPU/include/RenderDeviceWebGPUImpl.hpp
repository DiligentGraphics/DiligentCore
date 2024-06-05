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

#pragma once

/// \file
/// Declaration of Diligent::RenderDeviceWebGPUImpl class

#include "EngineWebGPUImplTraits.hpp"
#include "RenderDeviceBase.hpp"
#include "RenderDeviceWebGPU.h"
#include "WebGPUObjectWrappers.hpp"
#include "ShaderWebGPUImpl.hpp"
#include "SharedMemoryManagerWebGPU.hpp"

namespace Diligent
{

using QueueSignalPoolWebGPUPtr     = std::unique_ptr<class QueueSignalPoolWebGPU>;
using QueryManagerWebGPUPtr        = std::unique_ptr<class QueryManagerWebGPU>;
using AttachmentCleanerWebGPUPtr   = std::unique_ptr<class AttachmentCleanerWebGPU>;
using SharedMemoryManagerWebGPUPtr = std::unique_ptr<class SharedMemoryManagerWebGPU>;

/// Render device implementation in WebGPU backend.
class RenderDeviceWebGPUImpl final : public RenderDeviceBase<EngineWebGPUImplTraits>
{
public:
    using TRenderDeviceBase = RenderDeviceBase<EngineWebGPUImplTraits>;

    RenderDeviceWebGPUImpl(IReferenceCounters*           pRefCounters,
                           IMemoryAllocator&             RawMemAllocator,
                           IEngineFactory*               pEngineFactory,
                           const EngineWebGPUCreateInfo& EngineCI,
                           const GraphicsAdapterInfo&    AdapterInfo,
                           WGPUInstance                  wgpuInstance,
                           WGPUAdapter                   wgpuAdapter,
                           WGPUDevice                    wgpuDevice) noexcept(false);

    ~RenderDeviceWebGPUImpl() override;

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_RenderDeviceWebGPU, TRenderDeviceBase)

    /// Implementation of IRenderDevice::CreateBuffer() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateBuffer(const BufferDesc& BuffDesc,
                                         const BufferData* pBuffData,
                                         IBuffer**         ppBuffer) override;

    /// Implementation of IRenderDevice::CreateShader() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateShader(const ShaderCreateInfo& ShaderCI,
                                         IShader**               ppShader,
                                         IDataBlob**             ppCompilerOutput) override;

    /// Implementation of IRenderDevice::CreateTexture() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateTexture(const TextureDesc& TexDesc,
                                          const TextureData* pData,
                                          ITexture**         ppTexture) override;

    /// Implementation of IRenderDevice::CreateSampler() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateSampler(const SamplerDesc& SamplerDesc,
                                          ISampler**         ppSampler) override;

    /// Implementation of IRenderDevice::CreateGraphicsPipelineState() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
                                                        IPipelineState**                       ppPipelineState) override;

    /// Implementation of IRenderDevice::CreateComputePipelineState() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo,
                                                       IPipelineState**                      ppPipelineState) override;

    /// Implementation of IRenderDevice::CreateRayTracingPipelineState() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
                                                          IPipelineState**                         ppPipelineState) override;

    /// Implementation of IRenderDevice::CreateFence() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateFence(const FenceDesc& Desc,
                                        IFence**         ppFence) override;

    /// Implementation of IRenderDevice::CreateQuery() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateQuery(const QueryDesc& Desc,
                                        IQuery**         ppQuery) override;

    /// Implementation of IRenderDevice::CreateRenderPass() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateRenderPass(const RenderPassDesc& Desc,
                                             IRenderPass**         ppRenderPass) override;

    /// Implementation of IRenderDevice::CreateFramebuffer() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateFramebuffer(const FramebufferDesc& Desc,
                                              IFramebuffer**         ppFramebuffer) override;

    /// Implementation of IRenderDevice::CreateBLAS() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateBLAS(const BottomLevelASDesc& Desc,
                                       IBottomLevelAS**         ppBLAS) override;

    /// Implementation of IRenderDevice::CreateTLAS() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateTLAS(const TopLevelASDesc& Desc,
                                       ITopLevelAS**         ppTLAS) override;

    /// Implementation of IRenderDevice::CreateSBT() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateSBT(const ShaderBindingTableDesc& Desc,
                                      IShaderBindingTable**         ppSBT) override;

    /// Implementation of IRenderDevice::CreatePipelineResourceSignature() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                            IPipelineResourceSignature**         ppSignature) override;

    /// Implementation of IRenderDevice::CreateDeviceMemory() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateDeviceMemory(const DeviceMemoryCreateInfo& CreateInfo,
                                               IDeviceMemory**               ppMemory) override;

    /// Implementation of IRenderDevice::CreatePipelineStateCache() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreatePipelineStateCache(const PipelineStateCacheCreateInfo& CreateInfo,
                                                     IPipelineStateCache**               ppPSOCache) override;

    /// Implementation of IRenderDevice::ReleaseStaleResources() in WebGPU backend.
    void DILIGENT_CALL_TYPE ReleaseStaleResources(bool ForceRelease = false) override {}

    /// Implementation of IRenderDevice::IdleGPU() in WebGPU backend.
    void DILIGENT_CALL_TYPE IdleGPU() override;

    /// Implementation of IRenderDevice::GetSparseTextureFormatInfo() in WebGPU backend.
    SparseTextureFormatInfo DILIGENT_CALL_TYPE GetSparseTextureFormatInfo(TEXTURE_FORMAT     TexFormat,
                                                                          RESOURCE_DIMENSION Dimension,
                                                                          Uint32             SampleCount) const override;

    /// Implementation of IRenderDeviceWebGPU::GetWebGPUInstance() in WebGPU backend.
    WGPUInstance DILIGENT_CALL_TYPE GetWebGPUInstance() const override;

    /// Implementation of IRenderDeviceWebGPU::GetWebGPUAdapter() in WebGPU backend.
    WGPUAdapter DILIGENT_CALL_TYPE GetWebGPUAdapter() const override;

    /// Implementation of IRenderDeviceWebGPU::GetWebGPUDevice() in WebGPU backend.
    WGPUDevice DILIGENT_CALL_TYPE GetWebGPUDevice() const override;

    /// Implementation of IRenderDeviceWebGPU::CreateTextureFromWebGPUTexture() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateTextureFromWebGPUTexture(WGPUTexture        wgpuTexture,
                                                           const TextureDesc& TexDesc,
                                                           RESOURCE_STATE     InitialState,
                                                           ITexture**         ppTexture) override;

    /// Implementation of IRenderDeviceWebGPU::CreateBufferFromWebGPUBuffer() in WebGPU backend.
    void DILIGENT_CALL_TYPE CreateBufferFromWebGPUBuffer(WGPUBuffer        wgpuBuffer,
                                                         const BufferDesc& BuffDesc,
                                                         RESOURCE_STATE    InitialState,
                                                         IBuffer**         ppBuffer) override;

public:
    void CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                         IPipelineResourceSignature**         ppSignature,
                                         SHADER_TYPE                          ShaderStages,
                                         bool                                 IsDeviceInternal);

    void CreatePipelineResourceSignature(const PipelineResourceSignatureDesc&               Desc,
                                         const PipelineResourceSignatureInternalDataWebGPU& InternalData,
                                         IPipelineResourceSignature**                       ppSignature);


    void TransitionResource(TextureWebGPUImpl& Texture,
                            RESOURCE_STATE     NewState,
                            RESOURCE_STATE     OldState            = RESOURCE_STATE_UNKNOWN,
                            bool               UpdateResourceState = true);

    void TransitionResource(BufferWebGPUImpl& Buffer,
                            RESOURCE_STATE    NewState,
                            RESOURCE_STATE    OldState            = RESOURCE_STATE_UNKNOWN,
                            bool              UpdateResourceState = true);

    Uint64 GetCommandQueueCount() const { return 1; }

    Uint64 GetCommandQueueMask() const { return 1; }

    QueueSignalPoolWebGPU& GetQueueSignalPool() const;

    AttachmentCleanerWebGPU& GetAttachmentCleaner() const;

    SharedMemoryManagerWebGPU::Page GetSharedMemoryPage(Uint64 Size);

private:
    void TestTextureFormat(TEXTURE_FORMAT TexFormat) override;

private:
    WebGPUInstanceWrapper        m_wgpuInstance;
    WebGPUAdapterWrapper         m_wgpuAdapter;
    WebGPUDeviceWrapper          m_wgpuDevice;
    QueueSignalPoolWebGPUPtr     m_pQueueSignalPool;
    AttachmentCleanerWebGPUPtr   m_pAttachmentCleaner;
    SharedMemoryManagerWebGPUPtr m_pMemoryManager;

    std::vector<QueryManagerWebGPUPtr> m_QueryMgrs;
};


} // namespace Diligent
