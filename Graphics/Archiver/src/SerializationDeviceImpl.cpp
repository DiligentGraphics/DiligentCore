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

#include "ArchiverImpl.hpp"
#include "SerializationDeviceImpl.hpp"
#include "GLSLangUtils.hpp"
#include "SerializableShaderImpl.hpp"
#include "SerializableRenderPassImpl.hpp"
#include "SerializableResourceSignatureImpl.hpp"
#include "EngineMemory.h"

namespace Diligent
{
namespace
{
static constexpr RENDER_DEVICE_TYPE_FLAGS GetSupportedDeviceFlags()
{
    RENDER_DEVICE_TYPE_FLAGS Flags = RENDER_DEVICE_TYPE_FLAG_NONE;
#if D3D11_SUPPORTED
    Flags = Flags | RENDER_DEVICE_TYPE_FLAG_D3D11;
#endif
#if D3D12_SUPPORTED
    Flags = Flags | RENDER_DEVICE_TYPE_FLAG_D3D12;
#endif
#if GL_SUPPORTED
    Flags = Flags | RENDER_DEVICE_TYPE_FLAG_GL;
#endif
#if GLES_SUPPORTED
    Flags = Flags | RENDER_DEVICE_TYPE_FLAG_GLES;
#endif
#if VULKAN_SUPPORTED
    Flags = Flags | RENDER_DEVICE_TYPE_FLAG_VULKAN;
#endif
#if METAL_SUPPORTED
    Flags = Flags | RENDER_DEVICE_TYPE_FLAG_METAL;
#endif
    return Flags;
}

template <typename SignatureType>
using SignatureArray = std::array<RefCntAutoPtr<SignatureType>, MAX_RESOURCE_SIGNATURES>;

template <typename SignatureType>
static void SortResourceSignatures(const PipelineResourceBindingAttribs& Info, SignatureArray<SignatureType>& Signatures, Uint32& SignaturesCount)
{
    for (Uint32 i = 0; i < Info.ResourceSignaturesCount; ++i)
    {
        const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(Info.ppResourceSignatures[i]);
        const auto& Desc    = pSerPRS->GetDesc();

        Signatures[Desc.BindingIndex] = pSerPRS->GetSignature<SignatureType>();
        SignaturesCount               = std::max(SignaturesCount, static_cast<Uint32>(Desc.BindingIndex) + 1);
    }
}
} // namespace


SerializationDeviceImpl::SerializationDeviceImpl(IReferenceCounters* pRefCounters, const SerializationDeviceCreateInfo& CreateInfo) :
    TBase{pRefCounters},
    m_DeviceInfo{CreateInfo.DeviceInfo},
    m_AdapterInfo{CreateInfo.AdapterInfo}
{
#if !DILIGENT_NO_GLSLANG
    GLSLangUtils::InitializeGlslang();
#endif

#if D3D11_SUPPORTED
    m_D3D11FeatureLevel = CreateInfo.D3D11.FeatureLevel;
#endif
#if D3D12_SUPPORTED
    m_D3D12ShaderVersion = CreateInfo.D3D12.ShaderVersion;
    m_pDxCompiler        = CreateDXCompiler(DXCompilerTarget::Direct3D12, 0, CreateInfo.D3D12.DxCompilerPath);
#endif
#if VULKAN_SUPPORTED
    m_VkVersion          = CreateInfo.Vulkan.ApiVersion;
    m_VkSupportedSpirv14 = (m_VkVersion >= Version{1, 2} ? true : CreateInfo.Vulkan.SupportedSpirv14);
    m_pVkDxCompiler      = CreateDXCompiler(DXCompilerTarget::Vulkan, GetVkVersion(), CreateInfo.Vulkan.DxCompilerPath);
#endif
#if METAL_SUPPORTED
    m_MslPreprocessorCmd = CreateInfo.Metal.MslPreprocessorCmd ? CreateInfo.Metal.MslPreprocessorCmd : "";
    if (CreateInfo.Metal.CompileForMacOS)
    {
        m_MtlCompileForMacOS     = true;
        m_MtlCompileOptionsMacOS = CreateInfo.Metal.CompileOptionsMacOS ? CreateInfo.Metal.CompileOptionsMacOS : "";
        m_MtlLinkOptionsMacOS    = CreateInfo.Metal.LinkOptionsMacOS ? CreateInfo.Metal.LinkOptionsMacOS : "";
    }
    if (CreateInfo.Metal.CompileForiOS)
    {
        m_MtlCompileForiOS     = true;
        m_MtlCompileOptionsiOS = CreateInfo.Metal.CompileOptionsiOS ? CreateInfo.Metal.CompileOptionsiOS : "";
        m_MtlLinkOptionsiOS    = CreateInfo.Metal.LinkOptionsiOS ? CreateInfo.Metal.LinkOptionsiOS : "";
    }
#endif
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

RENDER_DEVICE_TYPE_FLAGS SerializationDeviceImpl::GetValidDeviceFlags()
{
    return GetSupportedDeviceFlags();
}

void SerializationDeviceImpl::CreateShader(const ShaderCreateInfo&  ShaderCI,
                                           RENDER_DEVICE_TYPE_FLAGS DeviceFlags,
                                           IShader**                ppShader)
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
                                                              RENDER_DEVICE_TYPE_FLAGS             DeviceFlags,
                                                              IPipelineResourceSignature**         ppSignature)
{
    CreatePipelineResourceSignature(Desc, DeviceFlags, SHADER_TYPE_UNKNOWN, ppSignature);
}

void SerializationDeviceImpl::CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                              RENDER_DEVICE_TYPE_FLAGS             DeviceFlags,
                                                              SHADER_TYPE                          ShaderStages,
                                                              IPipelineResourceSignature**         ppSignature)
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
            GetPipelineResourceBindingsMtl(Info, m_ResourceBindings, MtlMaxBufferFunctionArgumets());
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

} // namespace Diligent
