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

namespace Diligent
{

class SerializationDeviceImpl;
class SerializableShaderImpl;
class SerializableRenderPassImpl;
class SerializableResourceSignatureImpl;

struct SerializationEngineImplTraits
{
    using RenderDeviceInterface              = IRenderDevice;
    using ShaderInterface                    = IShader;
    using RenderPassInterface                = IRenderPass;
    using PipelineResourceSignatureInterface = IPipelineResourceSignature;

    using RenderDeviceImplType              = SerializationDeviceImpl;
    using ShaderImplType                    = SerializableShaderImpl;
    using RenderPassImplType                = SerializableRenderPassImpl;
    using PipelineResourceSignatureImplType = SerializableResourceSignatureImpl;
};

class SerializationDeviceImpl final : public ObjectBase<ISerializationDevice>
{
public:
    using TBase = ObjectBase<ISerializationDevice>;

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

    virtual void DILIGENT_CALL_TYPE CreateShader(const ShaderCreateInfo&   ShaderCI,
                                                 ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags,
                                                 IShader**                 ppShader) override final;

    virtual void DILIGENT_CALL_TYPE CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                                    ARCHIVE_DEVICE_DATA_FLAGS            DeviceFlags,
                                                                    IPipelineResourceSignature**         ppSignature) override final;

    void CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                         ARCHIVE_DEVICE_DATA_FLAGS            DeviceFlags,
                                         SHADER_TYPE                          ShaderStages,
                                         IPipelineResourceSignature**         ppSignature);

    virtual void DILIGENT_CALL_TYPE GetPipelineResourceBindings(const PipelineResourceBindingAttribs& Attribs,
                                                                Uint32&                               NumBindings,
                                                                const PipelineResourceBinding*&       pBindings) override final;

#if D3D11_SUPPORTED
    Uint32 GetD3D11FeatureLevel() const
    {
        return (m_D3D11FeatureLevel.Major << 12) | (m_D3D11FeatureLevel.Minor << 8);
    }
#endif


#if D3D12_SUPPORTED
    IDXCompiler* GetDxCompilerForDirect3D12() const
    {
        return m_pDxCompiler.get();
    }

    ShaderVersion GetD3D12ShaderVersion() const
    {
        return m_D3D12ShaderVersion;
    }
#endif

#if VULKAN_SUPPORTED
    IDXCompiler* GetDxCompilerForVulkan() const
    {
        return m_pVkDxCompiler.get();
    }

    Uint32 GetVkVersion() const { return (m_VkVersion.Major << 22) | (m_VkVersion.Minor << 12); }
    bool   HasSpirv14() const { return m_VkSupportedSpirv14; }
#endif

#if METAL_SUPPORTED
    const String& GetMslPreprocessorCmd() const
    {
        return m_MslPreprocessorCmd;
    }
    Uint32 MtlMaxBufferFunctionArgumets() const
    {
        return 31;
    }

    // MacOS
    bool MtlCompileForMacOS() const
    {
        return m_MtlCompileForMacOS;
    }
    const String& GetMtlCompileOptionsMacOS() const
    {
        VERIFY_EXPR(m_MtlCompileForMacOS);
        return m_MtlCompileOptionsMacOS;
    }
    const String& GetMtlLinkOptionsMacOS() const
    {
        VERIFY_EXPR(m_MtlCompileForMacOS);
        return m_MtlLinkOptionsMacOS;
    }

    // iOS
    bool MtlCompileForiOS() const
    {
        return m_MtlCompileForiOS;
    }
    const String& GetMtlCompileOptionsiOS() const
    {
        VERIFY_EXPR(m_MtlCompileForiOS);
        return m_MtlCompileOptionsiOS;
    }
    const String& GetMtlLinkOptionsiOS() const
    {
        VERIFY_EXPR(m_MtlCompileForiOS);
        return m_MtlLinkOptionsiOS;
    }
#endif

    ARCHIVE_DEVICE_DATA_FLAGS GetValidDeviceFlags() const
    {
        return m_DeviceFlags;
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

    ARCHIVE_DEVICE_DATA_FLAGS m_DeviceFlags = ARCHIVE_DEVICE_DATA_FLAG_NONE;
    const RenderDeviceInfo    m_DeviceInfo;
    const GraphicsAdapterInfo m_AdapterInfo;

    // D3D11
    Version m_D3D11FeatureLevel{11, 0};

    // D3D12
    std::unique_ptr<IDXCompiler> m_pDxCompiler;
    Version                      m_D3D12ShaderVersion{6, 5};

    // Vulkan
    std::unique_ptr<IDXCompiler> m_pVkDxCompiler;
    Version                      m_VkVersion{1, 0};
    bool                         m_VkSupportedSpirv14 = false;

    // Metal
    String m_MtlCompileOptionsMacOS;
    String m_MtlCompileOptionsiOS;
    String m_MtlLinkOptionsMacOS;
    String m_MtlLinkOptionsiOS;
    String m_MslPreprocessorCmd;
    bool   m_MtlCompileForMacOS = false;
    bool   m_MtlCompileForiOS   = false;

    std::vector<PipelineResourceBinding> m_ResourceBindings;
};

} // namespace Diligent
