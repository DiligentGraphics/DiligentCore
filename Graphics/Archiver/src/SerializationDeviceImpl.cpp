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

#include "ArchiverImpl.hpp"
#include "SerializationDeviceImpl.hpp"
#include "GLSLangUtils.hpp"
#include "SerializableShaderImpl.hpp"
#include "SerializableRenderPassImpl.hpp"
#include "SerializableResourceSignatureImpl.hpp"
#include "EngineMemory.h"

namespace Diligent
{

static constexpr ARCHIVE_DEVICE_DATA_FLAGS GetSupportedDeviceFlags()
{
    ARCHIVE_DEVICE_DATA_FLAGS Flags = ARCHIVE_DEVICE_DATA_FLAG_NONE;
#if GL_SUPPORTED
    Flags = Flags | ARCHIVE_DEVICE_DATA_FLAG_GL;
#endif
#if GLES_SUPPORTED
    Flags = Flags | ARCHIVE_DEVICE_DATA_FLAG_GLES;
#endif
#if D3D11_SUPPORTED
    Flags = Flags | ARCHIVE_DEVICE_DATA_FLAG_D3D11;
#endif
#if D3D12_SUPPORTED
    Flags = Flags | ARCHIVE_DEVICE_DATA_FLAG_D3D12;
#endif
#if VULKAN_SUPPORTED
    Flags = Flags | ARCHIVE_DEVICE_DATA_FLAG_VULKAN;
#endif
#if METAL_SUPPORTED
    Flags = Flags | ARCHIVE_DEVICE_DATA_FLAG_METAL_MACOS;
    Flags = Flags | ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS;
#endif
    return Flags;
}

SerializationDeviceImpl::SerializationDeviceImpl(IReferenceCounters* pRefCounters, const SerializationDeviceCreateInfo& CreateInfo) :
    TBase{pRefCounters},
    m_DeviceInfo{CreateInfo.DeviceInfo},
    m_AdapterInfo{CreateInfo.AdapterInfo}
{
#if !DILIGENT_NO_GLSLANG
    GLSLangUtils::InitializeGlslang();
#endif

    m_ValidDeviceFlags = GetSupportedDeviceFlags();

    if (m_ValidDeviceFlags & ARCHIVE_DEVICE_DATA_FLAG_D3D11)
    {
        m_D3D11Props.FeatureLevel = (CreateInfo.D3D11.FeatureLevel.Major << 12u) | (CreateInfo.D3D11.FeatureLevel.Minor << 8u);
    }

    if (m_ValidDeviceFlags & ARCHIVE_DEVICE_DATA_FLAG_D3D12)
    {
        m_pDxCompiler              = CreateDXCompiler(DXCompilerTarget::Direct3D12, 0, CreateInfo.D3D12.DxCompilerPath);
        m_D3D12Props.pDxCompiler   = m_pDxCompiler.get();
        m_D3D12Props.ShaderVersion = CreateInfo.D3D12.ShaderVersion;
    }

    if (m_ValidDeviceFlags & ARCHIVE_DEVICE_DATA_FLAG_VULKAN)
    {
        const auto& ApiVersion    = CreateInfo.Vulkan.ApiVersion;
        m_VkProps.VkVersion       = (ApiVersion.Major << 22u) | (ApiVersion.Minor << 12u);
        m_pVkDxCompiler           = CreateDXCompiler(DXCompilerTarget::Vulkan, m_VkProps.VkVersion, CreateInfo.Vulkan.DxCompilerPath);
        m_VkProps.pDxCompiler     = m_pVkDxCompiler.get();
        m_VkProps.SupportsSpirv14 = ApiVersion >= Version{1, 2} || CreateInfo.Vulkan.SupportsSpirv14;
    }

    if (m_ValidDeviceFlags & ARCHIVE_DEVICE_DATA_FLAG_METAL_MACOS)
    {
        const auto* CompileOptionsMacOS = CreateInfo.Metal.CompileOptionsMacOS;
        if (CompileOptionsMacOS != nullptr && CompileOptionsMacOS[0] != '\0')
        {
            m_MtlCompileOptionsMacOS       = CompileOptionsMacOS;
            m_MtlProps.CompileOptionsMacOS = m_MtlCompileOptionsMacOS.c_str();
        }
        else
        {
            LOG_WARNING_MESSAGE("CreateInfo.Metal.CompileOptionsMacOS is null or empty. Compilation for MacOS will be disabled.");
            m_ValidDeviceFlags &= ~ARCHIVE_DEVICE_DATA_FLAG_METAL_MACOS;
        }
    }

    if (m_ValidDeviceFlags & ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS)
    {
        const auto* CompileOptionsiOS = CreateInfo.Metal.CompileOptionsiOS;
        if (CompileOptionsiOS != nullptr && CompileOptionsiOS[0] != '\0')
        {
            m_MtlCompileOptionsiOS       = CompileOptionsiOS;
            m_MtlProps.CompileOptionsIOS = m_MtlCompileOptionsiOS.c_str();
        }
        else
        {
            LOG_WARNING_MESSAGE("CreateInfo.Metal.CompileOptionsiOS is null or empty. Compilation for iOS will be disabled.");
            m_ValidDeviceFlags &= ~ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS;
        }
    }

    if (m_ValidDeviceFlags & (ARCHIVE_DEVICE_DATA_FLAG_METAL_MACOS | ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS))
    {
        const auto* MslPreprocessorCmd = CreateInfo.Metal.MslPreprocessorCmd;
        if (MslPreprocessorCmd != nullptr && MslPreprocessorCmd[0] != '\0')
        {
            m_MslPreprocessorCmd          = CreateInfo.Metal.MslPreprocessorCmd;
            m_MtlProps.MslPreprocessorCmd = m_MslPreprocessorCmd.c_str();
        }
    }
}

void DILIGENT_CALL_TYPE SerializationDeviceImpl::QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)
{
    if (ppInterface == nullptr)
        return;
    if (IID == IID_SerializationDevice || IID == IID_RenderDevice)
    {
        *ppInterface = this;
        (*ppInterface)->AddRef();
    }
    else
    {
        TBase::QueryInterface(IID, ppInterface);
    }
}

SerializationDeviceImpl::~SerializationDeviceImpl()
{
#if !DILIGENT_NO_GLSLANG
    GLSLangUtils::FinalizeGlslang();
#endif
}

void SerializationDeviceImpl::CreateShader(const ShaderCreateInfo&   ShaderCI,
                                           ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags,
                                           IShader**                 ppShader)
{
    DEV_CHECK_ERR(ppShader != nullptr, "ppShader must not be null");
    if (!ppShader)
        return;

    *ppShader = nullptr;
    try
    {
        auto& RawMemAllocator = GetRawAllocator();
        auto* pShaderImpl     = NEW_RC_OBJ(RawMemAllocator, "Shader instance", SerializableShaderImpl)(this, ShaderCI, DeviceFlags);
        pShaderImpl->QueryInterface(IID_Shader, reinterpret_cast<IObject**>(ppShader));
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to create the shader");
    }
}

void SerializationDeviceImpl::CreateRenderPass(const RenderPassDesc& Desc, IRenderPass** ppRenderPass)
{
    DEV_CHECK_ERR(ppRenderPass != nullptr, "ppRenderPass must not be null");
    if (!ppRenderPass)
        return;

    *ppRenderPass = nullptr;
    try
    {
        auto& RawMemAllocator = GetRawAllocator();
        auto* pRenderPassImpl = NEW_RC_OBJ(RawMemAllocator, "Render pass instance", SerializableRenderPassImpl)(this, Desc);
        pRenderPassImpl->QueryInterface(IID_RenderPass, reinterpret_cast<IObject**>(ppRenderPass));
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to create the render pass");
    }
}

void SerializationDeviceImpl::CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                              ARCHIVE_DEVICE_DATA_FLAGS            DeviceFlags,
                                                              IPipelineResourceSignature**         ppSignature)
{
    CreateSerializableResourceSignature(Desc, DeviceFlags, SHADER_TYPE_UNKNOWN, reinterpret_cast<SerializableResourceSignatureImpl**>(ppSignature));
}

void SerializationDeviceImpl::CreateSerializableResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                                  ARCHIVE_DEVICE_DATA_FLAGS            DeviceFlags,
                                                                  SHADER_TYPE                          ShaderStages,
                                                                  SerializableResourceSignatureImpl**  ppSignature)
{
    DEV_CHECK_ERR(ppSignature != nullptr, "ppSignature must not be null");
    if (!ppSignature)
        return;

    *ppSignature = nullptr;
    try
    {
        auto& RawMemAllocator = GetRawAllocator();
        auto* pSignatureImpl  = NEW_RC_OBJ(RawMemAllocator, "Pipeline resource signature instance", SerializableResourceSignatureImpl)(this, Desc, DeviceFlags, ShaderStages);
        pSignatureImpl->QueryInterface(IID_PipelineResourceSignature, reinterpret_cast<IObject**>(ppSignature));
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to create the resource signature");
    }
}

void SerializationDeviceImpl::CreateSerializableResourceSignature(SerializableResourceSignatureImpl** ppSignature, const char* Name)
{
    auto& RawMemAllocator = GetRawAllocator();
    auto* pSignatureImpl  = NEW_RC_OBJ(RawMemAllocator, "Pipeline resource signature instance", SerializableResourceSignatureImpl)(Name);
    pSignatureImpl->QueryInterface(IID_PipelineResourceSignature, reinterpret_cast<IObject**>(ppSignature));
}

void SerializationDeviceImpl::GetPipelineResourceBindings(const PipelineResourceBindingAttribs& Info,
                                                          Uint32&                               NumBindings,
                                                          const PipelineResourceBinding*&       pBindings)
{
    NumBindings = 0;
    pBindings   = nullptr;
    m_ResourceBindings.clear();

    switch (Info.DeviceType)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
            GetPipelineResourceBindingsD3D11(Info, m_ResourceBindings);
            break;
#endif
#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
            GetPipelineResourceBindingsD3D12(Info, m_ResourceBindings);
            break;
#endif
#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            GetPipelineResourceBindingsGL(Info, m_ResourceBindings);
            break;
#endif
#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
            GetPipelineResourceBindingsVk(Info, m_ResourceBindings);
            break;
#endif
#if METAL_SUPPORTED
        case RENDER_DEVICE_TYPE_METAL:
            GetPipelineResourceBindingsMtl(Info, m_ResourceBindings, m_MtlProps.MaxBufferFunctionArgumets);
            break;
#endif
        case RENDER_DEVICE_TYPE_UNDEFINED:
        case RENDER_DEVICE_TYPE_COUNT:
        default:
            return;
    }

    NumBindings = static_cast<Uint32>(m_ResourceBindings.size());
    pBindings   = m_ResourceBindings.data();
}

PipelineResourceBinding SerializationDeviceImpl::ResDescToPipelineResBinding(const PipelineResourceDesc& ResDesc,
                                                                             SHADER_TYPE                 Stages,
                                                                             Uint32                      Register,
                                                                             Uint32                      Space)
{
    PipelineResourceBinding BindigDesc;
    BindigDesc.Name         = ResDesc.Name;
    BindigDesc.ResourceType = ResDesc.ResourceType;
    BindigDesc.Register     = Register;
    BindigDesc.Space        = StaticCast<Uint16>(Space);
    BindigDesc.ArraySize    = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY) == 0 ? ResDesc.ArraySize : 0;
    BindigDesc.ShaderStages = Stages;
    return BindigDesc;
}

} // namespace Diligent
