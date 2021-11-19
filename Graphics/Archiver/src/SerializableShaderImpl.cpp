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

#include "SerializableShaderImpl.hpp"
#include "FixedLinearAllocator.hpp"
#include "EngineMemory.h"
#include "DataBlobImpl.hpp"
#include "PlatformMisc.hpp"
#include "BasicMath.hpp"

#if D3D11_SUPPORTED
#    include "../../GraphicsEngineD3D11/include/pch.h"
#    include "RenderDeviceD3D11Impl.hpp"
#    include "ShaderD3D11Impl.hpp"
#endif
#if D3D12_SUPPORTED
#    include "../../GraphicsEngineD3D12/include/pch.h"
#    include "RenderDeviceD3D12Impl.hpp"
#    include "ShaderD3D12Impl.hpp"
#endif
#if GL_SUPPORTED || GLES_SUPPORTED
#    include "../../GraphicsEngineOpenGL/include/pch.h"
#    include "RenderDeviceGLImpl.hpp"
#    include "ShaderGLImpl.hpp"
#endif
#if VULKAN_SUPPORTED
#    include "VulkanUtilities/VulkanHeaders.h"
#    include "RenderDeviceVkImpl.hpp"
#    include "ShaderVkImpl.hpp"
#endif

namespace Diligent
{
namespace
{

template <typename ShaderType, typename... ArgTypes>
void CreateShader(std::unique_ptr<ShaderType>& pShader, String& CompilationLog, const char* DeviceTypeName, IReferenceCounters* pRefCounters, ShaderCreateInfo& ShaderCI, const ArgTypes&... Args)
{
    // Mem leak when used RefCntAutoPtr
    IDataBlob* pLog           = nullptr;
    ShaderCI.ppCompilerOutput = &pLog;
    try
    {
        pShader.reset(new ShaderType{pRefCounters, ShaderCI, Args...});
    }
    catch (...)
    {
        if (pLog && pLog->GetConstDataPtr())
        {
            CompilationLog += "Failed to compile ";
            CompilationLog += DeviceTypeName;
            CompilationLog += " shader:\n";
            CompilationLog += static_cast<const char*>(pLog->GetConstDataPtr());
        }
    }

    if (pLog)
        pLog->Release();
}

} // namespace

#if D3D11_SUPPORTED
struct SerializableShaderImpl::CompiledShaderD3D11
{
    ShaderD3D11Impl ShaderD3D11;

    CompiledShaderD3D11(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, const ShaderD3D11Impl::CreateInfo& D3D11ShaderCI) :
        ShaderD3D11{pRefCounters, nullptr, ShaderCI, D3D11ShaderCI, true}
    {}
};

ShaderD3D11Impl* SerializableShaderImpl::GetShaderD3D11() const
{
    return m_pShaderD3D11 ? &m_pShaderD3D11->ShaderD3D11 : nullptr;
}
#endif // D3D11_SUPPORTED


#if D3D12_SUPPORTED
struct SerializableShaderImpl::CompiledShaderD3D12
{
    ShaderD3D12Impl ShaderD3D12;

    CompiledShaderD3D12(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, const ShaderD3D12Impl::CreateInfo& D3D12ShaderCI) :
        ShaderD3D12{pRefCounters, nullptr, ShaderCI, D3D12ShaderCI, true}
    {}
};

const ShaderD3D12Impl* SerializableShaderImpl::GetShaderD3D12() const
{
    return m_pShaderD3D12 ? &m_pShaderD3D12->ShaderD3D12 : nullptr;
}
#endif // D3D12_SUPPORTED


#if VULKAN_SUPPORTED
struct SerializableShaderImpl::CompiledShaderVk
{
    ShaderVkImpl ShaderVk;

    CompiledShaderVk(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, const ShaderVkImpl::CreateInfo& VkShaderCI) :
        ShaderVk{pRefCounters, nullptr, ShaderCI, VkShaderCI, true}
    {}
};

const ShaderVkImpl* SerializableShaderImpl::GetShaderVk() const
{
    return m_pShaderVk ? &m_pShaderVk->ShaderVk : nullptr;
}
#endif // VULKAN_SUPPORTED


SerializableShaderImpl::SerializableShaderImpl(IReferenceCounters*      pRefCounters,
                                               SerializationDeviceImpl* pDevice,
                                               const ShaderCreateInfo&  InShaderCI,
                                               Uint32                   DeviceBits) :
    TBase{pRefCounters},
    m_CreateInfo{InShaderCI}
{
    if ((DeviceBits & pDevice->GetValidDeviceBits()) != DeviceBits)
    {
        LOG_ERROR_AND_THROW("DeviceBits contains unsupported device type");
    }

    CopyShaderCreateInfo(InShaderCI);

    auto   ShaderCI = m_CreateInfo;
    String CompilationLog;

    for (Uint32 Bits = DeviceBits; Bits != 0;)
    {
        const auto Type = static_cast<RENDER_DEVICE_TYPE>(PlatformMisc::GetLSB(ExtractLSB(Bits)));

        static_assert(RENDER_DEVICE_TYPE_COUNT == 7, "Please update the switch below to handle the new render device type");
        switch (Type)
        {
#if D3D11_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D11:
            {
                const ShaderD3D11Impl::CreateInfo D3D11ShaderCI{
                    pDevice->GetDeviceInfo(),
                    pDevice->GetAdapterInfo(),
                    static_cast<D3D_FEATURE_LEVEL>(pDevice->GetD3D11FeatureLevel()) //
                };
                CreateShader(m_pShaderD3D11, CompilationLog, "Direct3D11", pRefCounters, ShaderCI, D3D11ShaderCI);
                break;
            }
#endif

#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
            {
                const ShaderD3D12Impl::CreateInfo D3D12ShaderCI{
                    pDevice->GetDxCompilerForDirect3D12(),
                    pDevice->GetDeviceInfo(),
                    pDevice->GetAdapterInfo(),
                    pDevice->GetD3D12ShaderVersion() //
                };
                CreateShader(m_pShaderD3D12, CompilationLog, "Direct3D12", pRefCounters, ShaderCI, D3D12ShaderCI);
                break;
            }
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
                // shader compilation is not supported for OpenGL, use GetCreateInfo() to get source
                // AZ TODO: validate source using glslang
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
            {
                const ShaderVkImpl::CreateInfo VkShaderCI{
                    pDevice->GetDxCompilerForVulkan(),
                    pDevice->GetDeviceInfo(),
                    pDevice->GetAdapterInfo(),
                    pDevice->GetVkVersion(),
                    pDevice->HasSpirv14() //
                };
                CreateShader(m_pShaderVk, CompilationLog, "Vulkan", pRefCounters, ShaderCI, VkShaderCI);
                break;
            }
#endif

#if METAL_SUPPORTED
            case RENDER_DEVICE_TYPE_METAL:
                CompileShaderMtl(pDevice, ShaderCI);
                break;
#endif

            case RENDER_DEVICE_TYPE_UNDEFINED:
            case RENDER_DEVICE_TYPE_COUNT:
            default:
                LOG_ERROR_MESSAGE("Unexpected render device type");
                break;
        }
    }

    if (!CompilationLog.empty())
    {
        if (InShaderCI.ppCompilerOutput)
        {
            auto* pLogBlob = MakeNewRCObj<DataBlobImpl>{}(CompilationLog.size() + 1);
            std::memcpy(pLogBlob->GetDataPtr(), CompilationLog.c_str(), CompilationLog.size() + 1);
            pLogBlob->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(InShaderCI.ppCompilerOutput));
        }
        LOG_ERROR_AND_THROW("Shader '", (InShaderCI.Desc.Name ? InShaderCI.Desc.Name : ""), "' compilation failed for some backends");
    }
}

void SerializableShaderImpl::CopyShaderCreateInfo(const ShaderCreateInfo& ShaderCI)
{
    m_CreateInfo.ppCompilerOutput           = nullptr;
    m_CreateInfo.FilePath                   = nullptr;
    m_CreateInfo.pShaderSourceStreamFactory = nullptr;

    auto&                    RawAllocator = GetRawAllocator();
    FixedLinearAllocator     Allocator{RawAllocator};
    RefCntAutoPtr<IDataBlob> pSourceFileData;

    Allocator.AddSpaceForString(ShaderCI.EntryPoint);
    Allocator.AddSpaceForString(ShaderCI.CombinedSamplerSuffix);
    Allocator.AddSpaceForString(ShaderCI.Desc.Name);

    const auto* SourceCode    = ShaderCI.Source;
    size_t      SourceCodeLen = ShaderCI.SourceLength;

    if (ShaderCI.Source == nullptr &&
        ShaderCI.ByteCode == nullptr &&
        ShaderCI.FilePath != nullptr &&
        ShaderCI.pShaderSourceStreamFactory != nullptr)
    {
        RefCntAutoPtr<IFileStream> pSourceStream;
        ShaderCI.pShaderSourceStreamFactory->CreateInputStream(ShaderCI.FilePath, &pSourceStream);

        pSourceFileData = MakeNewRCObj<DataBlobImpl>{}(0);
        pSourceStream->ReadBlob(pSourceFileData);
        SourceCode    = static_cast<char*>(pSourceFileData->GetDataPtr());
        SourceCodeLen = pSourceFileData->GetSize();
    }

    if (SourceCode)
    {
        if (SourceCodeLen == 0)
            Allocator.AddSpaceForString(SourceCode);
        else
            Allocator.AddSpace<decltype(*SourceCode)>(SourceCodeLen + 1);
    }
    else if (ShaderCI.ByteCode && ShaderCI.ByteCodeSize > 0)
    {
        Allocator.AddSpace(ShaderCI.ByteCodeSize, alignof(Uint32));
    }
    else
    {
        LOG_ERROR_AND_THROW("Shader create info must contains Source, Bytecode or FilePath with pShaderSourceStreamFactory");
    }

    Uint32 MacroCount = 0;
    if (ShaderCI.Macros)
    {
        for (auto* Macro = ShaderCI.Macros; Macro->Name != nullptr && Macro->Definition != nullptr; ++Macro, ++MacroCount)
        {}
        Allocator.AddSpace<ShaderMacro>(MacroCount);

        for (Uint32 i = 0; i < MacroCount; ++i)
        {
            Allocator.AddSpaceForString(ShaderCI.Macros[i].Name);
            Allocator.AddSpaceForString(ShaderCI.Macros[i].Definition);
        }
    }

    Allocator.Reserve();

    m_pRawMemory = decltype(m_pRawMemory){Allocator.ReleaseOwnership(), STDDeleterRawMem<void>{RawAllocator}};

    m_CreateInfo.FilePath              = Allocator.CopyString(ShaderCI.FilePath);
    m_CreateInfo.EntryPoint            = Allocator.CopyString(ShaderCI.EntryPoint);
    m_CreateInfo.CombinedSamplerSuffix = Allocator.CopyString(ShaderCI.CombinedSamplerSuffix);
    m_CreateInfo.Desc.Name             = Allocator.CopyString(ShaderCI.Desc.Name);

    if (m_CreateInfo.Desc.Name == nullptr)
        m_CreateInfo.Desc.Name = "";

    if (SourceCode)
    {
        if (SourceCodeLen == 0)
        {
            m_CreateInfo.Source       = Allocator.CopyString(SourceCode);
            m_CreateInfo.SourceLength = strlen(m_CreateInfo.Source);
        }
        else
        {
            const size_t Size    = sizeof(*SourceCode) * (SourceCodeLen + 1);
            auto*        pSource = Allocator.Allocate<Char>(Size);
            std::memcpy(pSource, SourceCode, Size);
            pSource[m_CreateInfo.SourceLength + 1] = '\0';
            m_CreateInfo.Source                    = pSource;
        }
        VERIFY_EXPR(m_CreateInfo.SourceLength == strlen(m_CreateInfo.Source));
    }

    if (ShaderCI.ByteCode && ShaderCI.ByteCodeSize > 0)
    {
        void* pByteCode = Allocator.Allocate(ShaderCI.ByteCodeSize, alignof(Uint32));
        std::memcpy(pByteCode, ShaderCI.ByteCode, ShaderCI.ByteCodeSize);
        m_CreateInfo.ByteCode = pByteCode;
    }

    if (MacroCount > 0)
    {
        auto* pMacros       = Allocator.Allocate<ShaderMacro>(MacroCount);
        m_CreateInfo.Macros = pMacros;
        for (auto* Macro = ShaderCI.Macros; Macro->Name != nullptr && Macro->Definition != nullptr; ++Macro, ++pMacros)
        {
            pMacros->Name       = Allocator.CopyString(Macro->Name);
            pMacros->Definition = Allocator.CopyString(Macro->Definition);
        }
    }
}

SerializableShaderImpl::~SerializableShaderImpl()
{}

} // namespace Diligent
