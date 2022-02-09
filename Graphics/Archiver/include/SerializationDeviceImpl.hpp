/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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
#include "RenderDeviceBase.hpp"

namespace Diligent
{

class SerializationDeviceImpl;
class SerializableShaderImpl;
class SerializableRenderPassImpl;
class SerializableResourceSignatureImpl;
class SerializableObjectStub
{};

struct SerializationEngineImplTraits
{
    using RenderDeviceInterface              = ISerializationDevice;
    using DeviceContextInterface             = IDeviceContext;
    using PipelineStateInterface             = IPipelineState;
    using ShaderResourceBindingInterface     = IShaderResourceBinding;
    using BufferInterface                    = IBuffer;
    using BufferViewInterface                = IBufferView;
    using TextureInterface                   = ITexture;
    using TextureViewInterface               = ITextureView;
    using ShaderInterface                    = IShader;
    using SamplerInterface                   = ISampler;
    using FenceInterface                     = IFence;
    using QueryInterface                     = IQuery;
    using RenderPassInterface                = IRenderPass;
    using FramebufferInterface               = IFramebuffer;
    using CommandListInterface               = ICommandList;
    using BottomLevelASInterface             = IBottomLevelAS;
    using TopLevelASInterface                = ITopLevelAS;
    using ShaderBindingTableInterface        = IShaderBindingTable;
    using PipelineResourceSignatureInterface = IPipelineResourceSignature;
    using CommandQueueInterface              = ICommandQueue;
    using DeviceMemoryInterface              = IDeviceMemory;
    using PipelineStateCacheInterface        = IPipelineStateCache;
    using DeviceObjectArchiveInterface       = IDeviceObjectArchive;

    using RenderDeviceImplType              = SerializationDeviceImpl;
    using DeviceContextImplType             = IDeviceContext;
    using PipelineStateImplType             = SerializableObjectStub;
    using ShaderResourceBindingImplType     = SerializableObjectStub;
    using BufferImplType                    = SerializableObjectStub;
    using BufferViewImplType                = SerializableObjectStub;
    using TextureImplType                   = SerializableObjectStub;
    using TextureViewImplType               = SerializableObjectStub;
    using ShaderImplType                    = SerializableShaderImpl;
    using SamplerImplType                   = SerializableObjectStub;
    using FenceImplType                     = SerializableObjectStub;
    using QueryImplType                     = SerializableObjectStub;
    using RenderPassImplType                = SerializableRenderPassImpl;
    using FramebufferImplType               = SerializableObjectStub;
    using CommandListImplType               = SerializableObjectStub;
    using BottomLevelASImplType             = SerializableObjectStub;
    using TopLevelASImplType                = SerializableObjectStub;
    using ShaderBindingTableImplType        = SerializableObjectStub;
    using PipelineResourceSignatureImplType = SerializableResourceSignatureImpl;
    using DeviceMemoryImplType              = SerializableObjectStub;
    using PipelineStateCacheImplType        = SerializableObjectStub;
    using DeviceObjectArchiveImplType       = SerializableObjectStub;
};

class SerializationDeviceImpl final : public RenderDeviceBase<SerializationEngineImplTraits>
{
public:
    using TBase = RenderDeviceBase<SerializationEngineImplTraits>;

    SerializationDeviceImpl(IReferenceCounters* pRefCounters, const SerializationDeviceCreateInfo& CreateInfo);
    ~SerializationDeviceImpl();

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

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
                                                     IRenderPass**         ppRenderPass) override final;

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

    virtual void DILIGENT_CALL_TYPE CreateShader(const ShaderCreateInfo&   ShaderCI,
                                                 ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags,
                                                 IShader**                 ppShader) override final;

    virtual void DILIGENT_CALL_TYPE CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                                    const ResourceSignatureArchiveInfo&  ArchiveInfo,
                                                                    IPipelineResourceSignature**         ppSignature) override final;

    void CreateSerializableResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                             const ResourceSignatureArchiveInfo&  ArchiveInfo,
                                             SHADER_TYPE                          ShaderStages,
                                             SerializableResourceSignatureImpl**  ppSignature);

    void CreateSerializableResourceSignature(SerializableResourceSignatureImpl** ppSignature, const char* Name);

    virtual void DILIGENT_CALL_TYPE GetPipelineResourceBindings(const PipelineResourceBindingAttribs& Attribs,
                                                                Uint32&                               NumBindings,
                                                                const PipelineResourceBinding*&       pBindings) override final;

    struct D3D11Properties
    {
        Uint32 FeatureLevel = 0;
    };

    struct D3D12Properties
    {
        IDXCompiler* pDxCompiler = nullptr;
        Version      ShaderVersion;
    };

    struct VkProperties
    {
        IDXCompiler* pDxCompiler     = nullptr;
        Uint32       VkVersion       = 0;
        bool         SupportsSpirv14 = false;
    };

    struct MtlProperties
    {
        const char* CompileOptionsMacOS = nullptr;
        const char* CompileOptionsIOS   = nullptr;
        const char* MslPreprocessorCmd  = nullptr;

        const Uint32 MaxBufferFunctionArgumets = 31;
    };

    const D3D11Properties& GetD3D11Properties() { return m_D3D11Props; }
    const D3D12Properties& GetD3D12Properties() { return m_D3D12Props; }
    const VkProperties&    GetVkProperties() { return m_VkProps; }
    const MtlProperties&   GetMtlProperties() { return m_MtlProps; }

    ARCHIVE_DEVICE_DATA_FLAGS GetValidDeviceFlags() const
    {
        return m_ValidDeviceFlags;
    }

    SerializationDeviceImpl* GetDevice() { return this; }

protected:
    static PipelineResourceBinding ResDescToPipelineResBinding(const PipelineResourceDesc& ResDesc, SHADER_TYPE Stages, Uint32 Register, Uint32 Space);

private:
    static void GetPipelineResourceBindingsD3D11(const PipelineResourceBindingAttribs& Attribs,
                                                 std::vector<PipelineResourceBinding>& ResourceBindings);
    static void GetPipelineResourceBindingsD3D12(const PipelineResourceBindingAttribs& Attribs,
                                                 std::vector<PipelineResourceBinding>& ResourceBindings);
    static void GetPipelineResourceBindingsGL(const PipelineResourceBindingAttribs& Attribs,
                                              std::vector<PipelineResourceBinding>& ResourceBindings);
    static void GetPipelineResourceBindingsVk(const PipelineResourceBindingAttribs& Attribs,
                                              std::vector<PipelineResourceBinding>& ResourceBindings);
    static void GetPipelineResourceBindingsMtl(const PipelineResourceBindingAttribs& Attribs,
                                               std::vector<PipelineResourceBinding>& ResourceBindings,
                                               const Uint32                          MaxBufferArgs);

    virtual void TestTextureFormat(TEXTURE_FORMAT TexFormat) override final {}

    ARCHIVE_DEVICE_DATA_FLAGS m_ValidDeviceFlags = ARCHIVE_DEVICE_DATA_FLAG_NONE;

    std::unique_ptr<IDXCompiler> m_pDxCompiler;
    std::unique_ptr<IDXCompiler> m_pVkDxCompiler;

    D3D11Properties m_D3D11Props;
    D3D12Properties m_D3D12Props;
    VkProperties    m_VkProps;
    MtlProperties   m_MtlProps;

    // Metal
    String m_MtlCompileOptionsMacOS;
    String m_MtlCompileOptionsiOS;
    String m_MslPreprocessorCmd;

    std::vector<PipelineResourceBinding> m_ResourceBindings;
};

} // namespace Diligent
