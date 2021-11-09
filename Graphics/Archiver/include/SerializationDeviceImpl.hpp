/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "RenderDevice.h"
#include "SerializationDevice.h"
#include "ObjectBase.hpp"
#include "DXCompiler.hpp"

#if D3D11_SUPPORTED
#    include "../../GraphicsEngineD3D11/include/pch.h"
#endif


namespace Diligent
{

class DummyRenderDevice;
class SerializableShaderImpl;
class SerializableRenderPassImpl;
class SerializableResourceSignatureImpl;

struct SerializationEngineImplTraits
{
    using RenderDeviceInterface              = IRenderDevice;
    using ShaderInterface                    = IShader;
    using RenderPassInterface                = IRenderPass;
    using PipelineResourceSignatureInterface = IPipelineResourceSignature;

    using RenderDeviceImplType              = DummyRenderDevice;
    using ShaderImplType                    = SerializableShaderImpl;
    using RenderPassImplType                = SerializableRenderPassImpl;
    using PipelineResourceSignatureImplType = SerializableResourceSignatureImpl;
};


class DummyRenderDevice final : public ObjectBase<IRenderDevice>
{
public:
    using TBase = ObjectBase<IRenderDevice>;

    explicit DummyRenderDevice(IReferenceCounters* pRefCounters);
    ~DummyRenderDevice();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_RenderDevice, TBase)

    /// Implementation of IRenderDevice::CreateGraphicsPipelineState().
    virtual void DILIGENT_CALL_TYPE CreateGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo, IPipelineState** ppPipelineState) override final {}

    /// Implementation of IRenderDevice::CreateComputePipelineState().
    virtual void DILIGENT_CALL_TYPE CreateComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo, IPipelineState** ppPipelineState) override final {}

    /// Implementation of IRenderDevice::CreateRayTracingPipelineState().
    virtual void DILIGENT_CALL_TYPE CreateRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo, IPipelineState** ppPipelineState) override final {}

    /// Implementation of IRenderDevice::CreateTilePipelineState().
    virtual void DILIGENT_CALL_TYPE CreateTilePipelineState(const TilePipelineStateCreateInfo& PSOCreateInfo, IPipelineState** ppPipelineState) override final {}

    /// Implementation of IRenderDevice::CreateBuffer().
    virtual void DILIGENT_CALL_TYPE CreateBuffer(const BufferDesc& BuffDesc,
                                                 const BufferData* pBuffData,
                                                 IBuffer**         ppBuffer) override final {}

    /// Implementation of IRenderDevice::CreateShader().
    virtual void DILIGENT_CALL_TYPE CreateShader(const ShaderCreateInfo& ShaderCreateInfo, IShader** ppShader) override final {}

    /// Implementation of IRenderDevice::CreateTexture().
    virtual void DILIGENT_CALL_TYPE CreateTexture(const TextureDesc& TexDesc,
                                                  const TextureData* pData,
                                                  ITexture**         ppTexture) override final {}

    /// Implementation of IRenderDevice::CreateSampler().
    virtual void DILIGENT_CALL_TYPE CreateSampler(const SamplerDesc& SamplerDesc, ISampler** ppSampler) override final {}

    /// Implementation of IRenderDevice::CreateFence().
    virtual void DILIGENT_CALL_TYPE CreateFence(const FenceDesc& Desc, IFence** ppFence) override final {}

    /// Implementation of IRenderDevice::CreateQuery().
    virtual void DILIGENT_CALL_TYPE CreateQuery(const QueryDesc& Desc, IQuery** ppQuery) override final {}

    /// Implementation of IRenderDevice::CreateRenderPass().
    virtual void DILIGENT_CALL_TYPE CreateRenderPass(const RenderPassDesc& Desc,
                                                     IRenderPass**         ppRenderPass) override final {}

    /// Implementation of IRenderDevice::CreateFramebuffer().
    virtual void DILIGENT_CALL_TYPE CreateFramebuffer(const FramebufferDesc& Desc,
                                                      IFramebuffer**         ppFramebuffer) override final {}

    /// Implementation of IRenderDevice::CreateBLAS().
    virtual void DILIGENT_CALL_TYPE CreateBLAS(const BottomLevelASDesc& Desc,
                                               IBottomLevelAS**         ppBLAS) override final {}

    /// Implementation of IRenderDevice::CreateTLAS().
    virtual void DILIGENT_CALL_TYPE CreateTLAS(const TopLevelASDesc& Desc,
                                               ITopLevelAS**         ppTLAS) override final {}

    /// Implementation of IRenderDevice::CreateSBT().
    virtual void DILIGENT_CALL_TYPE CreateSBT(const ShaderBindingTableDesc& Desc,
                                              IShaderBindingTable**         ppSBT) override final {}

    /// Implementation of IRenderDevice::CreatePipelineResourceSignature().
    virtual void DILIGENT_CALL_TYPE CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                                    IPipelineResourceSignature**         ppSignature) override final {}

    /// Implementation of IRenderDevice::CreateDeviceMemory().
    virtual void DILIGENT_CALL_TYPE CreateDeviceMemory(const DeviceMemoryCreateInfo& CreateInfo,
                                                       IDeviceMemory**               ppMemory) override final {}

    /// Implementation of IRenderDevice::CreatePipelineStateCache().
    virtual void DILIGENT_CALL_TYPE CreatePipelineStateCache(const PipelineStateCacheCreateInfo& CreateInfo,
                                                             IPipelineStateCache**               ppPSOCache) override final {}

    /// Implementation of IRenderDevice::CreateResourceMapping().
    virtual void DILIGENT_CALL_TYPE CreateResourceMapping(const ResourceMappingDesc& MappingDesc,
                                                          IResourceMapping**         ppMapping) override final {}

    /// Implementation of IRenderDevice::IdleGPU().
    virtual void DILIGENT_CALL_TYPE IdleGPU() override final {}

    /// Implementation of IRenderDevice::ReleaseStaleResources().
    virtual void DILIGENT_CALL_TYPE ReleaseStaleResources(bool ForceRelease) override final {}

    /// Implementation of IRenderDevice::GetSparseTextureFormatInfo().
    virtual SparseTextureFormatInfo DILIGENT_CALL_TYPE GetSparseTextureFormatInfo(TEXTURE_FORMAT     TexFormat,
                                                                                  RESOURCE_DIMENSION Dimension,
                                                                                  Uint32             SampleCount) const override final
    {
        return SparseTextureFormatInfo{};
    }

    /// Implementation of IRenderDevice::GetDeviceInfo().
    virtual const RenderDeviceInfo& DILIGENT_CALL_TYPE GetDeviceInfo() const override final { return m_DeviceInfo; }

    /// Implementation of IRenderDevice::GetAdapterInfo().
    virtual const GraphicsAdapterInfo& DILIGENT_CALL_TYPE GetAdapterInfo() const override final { return m_AdapterInfo; }

    /// Implementation of IRenderDevice::GetTextureFormatInfo().
    virtual const TextureFormatInfo& DILIGENT_CALL_TYPE GetTextureFormatInfo(TEXTURE_FORMAT TexFormat) override final
    {
        static const TextureFormatInfo FmtInfo = {};
        return FmtInfo;
    }

    /// Implementation of IRenderDevice::GetTextureFormatInfoExt().
    virtual const TextureFormatInfoExt& DILIGENT_CALL_TYPE GetTextureFormatInfoExt(TEXTURE_FORMAT TexFormat) override final
    {
        static const TextureFormatInfoExt FmtInfo = {};
        return FmtInfo;
    }

    /// Implementation of IRenderDevice::GetEngineFactory().
    virtual IEngineFactory* DILIGENT_CALL_TYPE GetEngineFactory() const override final { return nullptr; }

private:
    RenderDeviceInfo    m_DeviceInfo;
    GraphicsAdapterInfo m_AdapterInfo;
};



class SerializationDeviceImpl final : public ObjectBase<ISerializationDevice>
{
public:
    using TBase = ObjectBase<ISerializationDevice>;

    explicit SerializationDeviceImpl(IReferenceCounters* pRefCounters);
    ~SerializationDeviceImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_SerializationDevice, TBase)

    virtual void DILIGENT_CALL_TYPE CreateShader(const ShaderCreateInfo& ShaderCI,
                                                 Uint32                  DeviceBits,
                                                 IShader**               ppShader) override final;

    virtual void DILIGENT_CALL_TYPE CreateRenderPass(const RenderPassDesc& Desc,
                                                     IRenderPass**         ppRenderPass) override final;

    virtual void DILIGENT_CALL_TYPE CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                                    Uint32                               DeviceBits,
                                                                    IPipelineResourceSignature**         ppSignature) override final;

#if D3D11_SUPPORTED
    D3D_FEATURE_LEVEL GetD3D11FeatureLevel() const
    {
        return D3D_FEATURE_LEVEL_11_1;
    }
#endif


#if D3D12_SUPPORTED
    IDXCompiler* GetDxCompilerForDirect3D12() const
    {
        return m_pDxCompiler.get();
    }

    ShaderVersion GetD3D12ShaderVersion() const
    {
        return ShaderVersion{6, 5};
    }
#endif

#if VULKAN_SUPPORTED
    IDXCompiler* GetDxCompilerForVulkan() const
    {
        return m_pVkDxCompiler.get();
    }

    Uint32 GetVkVersion() const { return /*VK_API_VERSION_1_0*/ (1u << 22) | (0u << 12); } // AZ TODO
    bool   HasSpirv14() const { return false; }                                            // AZ TODO
#endif

    static Uint32 GetValidDeviceBits();

    DummyRenderDevice* GetDevice() { return &m_Device; }

private:
    DummyRenderDevice            m_Device;
    std::unique_ptr<IDXCompiler> m_pDxCompiler;
    std::unique_ptr<IDXCompiler> m_pVkDxCompiler;
};

} // namespace Diligent
