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

#include "ShaderWebGPUImpl.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "GLSLUtils.hpp"
#include "ShaderToolsCommon.hpp"
#include "WGSLUtils.hpp"

#if !DILIGENT_NO_GLSLANG
#    include "GLSLangUtils.hpp"
#endif

#if !DILIGENT_NO_HLSL
#    include "SPIRVTools.hpp"
#endif

namespace Diligent
{

constexpr INTERFACE_ID ShaderWebGPUImpl::IID_InternalImpl;

namespace
{

constexpr char SPIRVDefine[] =
    "#ifndef WEBGPU\n"
    "#   define WEBGPU 1\n"
    "#endif\n";

std::vector<uint32_t> CompileShaderGLSLang(const ShaderCreateInfo&             ShaderCI,
                                           const ShaderWebGPUImpl::CreateInfo& WebGPUShaderCI)
{
    std::vector<uint32_t> SPIRV;

#if DILIGENT_NO_GLSLANG
    LOG_ERROR_AND_THROW("Diligent engine was not linked with glslang, use precompiled SPIRV bytecode.");
#else
    SPIRV = GLSLangUtils::HLSLtoSPIRV(ShaderCI, GLSLangUtils::SpirvVersion::Vk100, SPIRVDefine, WebGPUShaderCI.ppCompilerOutput);
#endif

    return SPIRV;
}

} // namespace

ShaderWebGPUImpl::ShaderWebGPUImpl(IReferenceCounters*     pRefCounters,
                                   RenderDeviceWebGPUImpl* pDeviceWebGPU,
                                   const ShaderCreateInfo& ShaderCI,
                                   const CreateInfo&       WebGPUShaderCI,
                                   bool                    IsDeviceInternal) :
    // clang-format off
    TShaderBase
    {
        pRefCounters,
        pDeviceWebGPU,
        ShaderCI.Desc,
        WebGPUShaderCI.DeviceInfo,
        WebGPUShaderCI.AdapterInfo,
        IsDeviceInternal
    }
// clang-format on
{
    m_Status.store(SHADER_STATUS_COMPILING);
    if (ShaderCI.Source != nullptr || ShaderCI.FilePath != nullptr)
    {
        DEV_CHECK_ERR(ShaderCI.ByteCode == nullptr, "'ByteCode' must be null when shader is created from source code or a file");
        switch (ShaderCI.ShaderCompiler)
        {

            case SHADER_COMPILER_DEFAULT:
            case SHADER_COMPILER_GLSLANG:
                m_SPIRV = CompileShaderGLSLang(ShaderCI, WebGPUShaderCI);
                break;

            default:
                LOG_ERROR_AND_THROW("Unsupported shader compiler");
        }

        if (m_SPIRV.empty())
        {
            LOG_ERROR_AND_THROW("Failed to compile shader '", m_Desc.Name, '\'');
        }
    }
    else if (ShaderCI.ByteCode != nullptr)
    {
        DEV_CHECK_ERR(ShaderCI.ByteCodeSize != 0, "ByteCodeSize must not be 0");
        DEV_CHECK_ERR(ShaderCI.ByteCodeSize % 4 == 0, "Byte code size (", ShaderCI.ByteCodeSize, ") is not multiple of 4");
        m_SPIRV.resize(ShaderCI.ByteCodeSize / 4);
        memcpy(m_SPIRV.data(), ShaderCI.ByteCode, ShaderCI.ByteCodeSize);
    }
    else
    {
        LOG_ERROR_AND_THROW("Shader source must be provided through one of the 'Source', 'FilePath' or 'ByteCode' members");
    }

    // We cannot create shader module here because resource bindings are assigned when
    // pipeline state is created

    // Load shader resources
    if ((ShaderCI.CompileFlags & SHADER_COMPILE_FLAG_SKIP_REFLECTION) == 0)
    {
        auto& Allocator        = GetRawAllocator();
        auto* pRawMem          = ALLOCATE(Allocator, "Memory for SPIRVShaderResources", SPIRVShaderResources, 1);
        auto  LoadShaderInputs = m_Desc.ShaderType == SHADER_TYPE_VERTEX;
        auto* pResources       = new (pRawMem) SPIRVShaderResources //
            {
                Allocator,
                m_SPIRV,
                m_Desc,
                m_Desc.UseCombinedTextureSamplers ? m_Desc.CombinedSamplerSuffix : nullptr,
                LoadShaderInputs,
                ShaderCI.LoadConstantBufferReflection,
                m_EntryPoint //
            };
        VERIFY_EXPR(ShaderCI.ByteCode != nullptr || m_EntryPoint == ShaderCI.EntryPoint);
        m_pShaderResources.reset(pResources, STDDeleterRawMem<SPIRVShaderResources>(Allocator));

        if (LoadShaderInputs && m_pShaderResources->IsHLSLSource())
        {
            // TODO
            // MapHLSLVertexShaderInputs();
        }
    }
    else
    {
        m_EntryPoint = ShaderCI.EntryPoint;
    }

    m_WGSL = ConvertSPIRVtoWGSL(m_SPIRV);
    m_Status.store(SHADER_STATUS_READY);
}

ShaderWebGPUImpl::~ShaderWebGPUImpl()
{
    GetStatus(/*WaitForCompletion = */ true);
}

Uint32 ShaderWebGPUImpl::GetResourceCount() const
{
    DEV_CHECK_ERR(!IsCompiling(), "Shader resources are not available until the shader is compiled. Use GetStatus() to check the shader status.");
    return m_pShaderResources ? m_pShaderResources->GetTotalResources() : 0;
}

void ShaderWebGPUImpl::GetResourceDesc(Uint32 Index, ShaderResourceDesc& ResourceDesc) const
{
    DEV_CHECK_ERR(!IsCompiling(), "Shader resources are not available until the shader is compiled. Use GetStatus() to check the shader status.");

    auto ResCount = GetResourceCount();
    DEV_CHECK_ERR(Index < ResCount, "Resource index (", Index, ") is out of range");
    if (Index < ResCount)
    {
        const auto& SPIRVResource = m_pShaderResources->GetResource(Index);
        ResourceDesc              = SPIRVResource.GetResourceDesc();
    }
}

const ShaderCodeBufferDesc* ShaderWebGPUImpl::GetConstantBufferDesc(Uint32 Index) const
{
    DEV_CHECK_ERR(!IsCompiling(), "Shader resources are not available until the shader is compiled. Use GetStatus() to check the shader status.");

    auto ResCount = GetResourceCount();
    if (Index >= ResCount)
    {
        UNEXPECTED("Resource index (", Index, ") is out of range");
        return nullptr;
    }

    // Uniform buffers always go first in the list of resources
    return m_pShaderResources->GetUniformBufferDesc(Index);
}

void ShaderWebGPUImpl::GetBytecode(const void** ppBytecode, Uint64& Size) const
{
    DEV_CHECK_ERR(!IsCompiling(), "WGSL is not available until the shader is compiled. Use GetStatus() to check the shader status.");
    *ppBytecode = m_WGSL.data();
    Size        = m_WGSL.size();
}

const std::vector<uint32_t>& ShaderWebGPUImpl::GetSPIRV() const
{
    DEV_CHECK_ERR(!IsCompiling(), "SPIRV bytecode is not available until the shader is compiled. Use GetStatus() to check the shader status.");
    return m_SPIRV;
}

const std::string& ShaderWebGPUImpl::GetWGSL() const
{
    DEV_CHECK_ERR(!IsCompiling(), "WGSL is not available until the shader is compiled. Use GetStatus() to check the shader status.");
    return m_WGSL;
}

const char* ShaderWebGPUImpl::GetEntryPoint() const
{
    DEV_CHECK_ERR(!IsCompiling(), "Shader resources are not available until the shader is compiled. Use GetStatus() to check the shader status.");
    return m_EntryPoint.c_str();
}

} // namespace Diligent
