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

#include "SerializableShaderImpl.hpp"
#include "FixedLinearAllocator.hpp"
#include "EngineMemory.h"
#include "DataBlobImpl.hpp"
#include "PlatformMisc.hpp"
#include "BasicMath.hpp"

namespace Diligent
{

SerializableShaderImpl::SerializableShaderImpl(IReferenceCounters*       pRefCounters,
                                               SerializationDeviceImpl*  pDevice,
                                               const ShaderCreateInfo&   ShaderCI,
                                               ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags) :
    TBase{pRefCounters},
    m_pDevice{pDevice},
    m_CreateInfo{ShaderCI}
{
    if ((DeviceFlags & m_pDevice->GetValidDeviceFlags()) != DeviceFlags)
    {
        LOG_ERROR_AND_THROW("DeviceFlags contain unsupported device type");
    }

    if (ShaderCI.CompileFlags & SHADER_COMPILE_FLAG_SKIP_REFLECTION)
    {
        LOG_ERROR_AND_THROW("Serialized shader must not contain SHADER_COMPILE_FLAG_SKIP_REFLECTION flag");
    }

    CopyShaderCreateInfo(ShaderCI);

    String CompilationLog;

    while (DeviceFlags != ARCHIVE_DEVICE_DATA_FLAG_NONE)
    {
        const auto Flag = ExtractLSB(DeviceFlags);

        static_assert(ARCHIVE_DEVICE_DATA_FLAG_LAST == ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS, "Please update the switch below to handle the new device data type");
        switch (Flag)
        {
#if D3D11_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_D3D11:
                CreateShaderD3D11(pRefCounters, ShaderCI, CompilationLog);
                break;
#endif

#if D3D12_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_D3D12:
                CreateShaderD3D12(pRefCounters, ShaderCI, CompilationLog);
                break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_GL:
            case ARCHIVE_DEVICE_DATA_FLAG_GLES:
                CreateShaderGL(pRefCounters, ShaderCI, CompilationLog, Flag == ARCHIVE_DEVICE_DATA_FLAG_GL ? RENDER_DEVICE_TYPE_GL : RENDER_DEVICE_TYPE_GLES);
                break;
#endif

#if VULKAN_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_VULKAN:
                CreateShaderVk(pRefCounters, ShaderCI, CompilationLog);
                break;
#endif

#if METAL_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_METAL_MACOS:
            case ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS:
                CreateShaderMtl(ShaderCI, CompilationLog);
                break;
#endif

            case ARCHIVE_DEVICE_DATA_FLAG_NONE:
                UNEXPECTED("ARCHIVE_DEVICE_DATA_FLAG_NONE(0) should never occur");
                break;

            default:
                LOG_ERROR_MESSAGE("Unexpected render device type");
                break;
        }
    }

    if (!CompilationLog.empty())
    {
        if (ShaderCI.ppCompilerOutput)
        {
            auto* pLogBlob = MakeNewRCObj<DataBlobImpl>{}(CompilationLog.size() + 1);
            std::memcpy(pLogBlob->GetDataPtr(), CompilationLog.c_str(), CompilationLog.size() + 1);
            pLogBlob->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ShaderCI.ppCompilerOutput));
        }
        LOG_ERROR_AND_THROW("Shader '", (ShaderCI.Desc.Name ? ShaderCI.Desc.Name : ""), "' compilation failed for some backends");
    }
}

void SerializableShaderImpl::CopyShaderCreateInfo(const ShaderCreateInfo& ShaderCI) noexcept(false)
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

        if (pSourceStream == nullptr)
            LOG_ERROR_AND_THROW("Failed to open shader source file ", ShaderCI.FilePath);

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
        Allocator.AddSpace<ShaderMacro>(MacroCount + 1);

        for (Uint32 i = 0; i < MacroCount; ++i)
        {
            Allocator.AddSpaceForString(ShaderCI.Macros[i].Name);
            Allocator.AddSpaceForString(ShaderCI.Macros[i].Definition);
        }
    }

    Allocator.Reserve();

    m_pRawMemory = decltype(m_pRawMemory){Allocator.ReleaseOwnership(), STDDeleterRawMem<void>{RawAllocator}};

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
            auto*        pSource = static_cast<Char*>(Allocator.Allocate(Size, alignof(Char)));
            std::memcpy(pSource, SourceCode, Size);
            pSource[SourceCodeLen]    = '\0';
            m_CreateInfo.SourceLength = SourceCodeLen;
            m_CreateInfo.Source       = pSource;
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
        auto* pMacros       = Allocator.Allocate<ShaderMacro>(MacroCount + 1);
        m_CreateInfo.Macros = pMacros;
        for (auto* Macro = ShaderCI.Macros; Macro->Name != nullptr && Macro->Definition != nullptr; ++Macro, ++pMacros)
        {
            pMacros->Name       = Allocator.CopyString(Macro->Name);
            pMacros->Definition = Allocator.CopyString(Macro->Definition);
        }
        pMacros->Name       = nullptr;
        pMacros->Definition = nullptr;
    }
}

SerializableShaderImpl::~SerializableShaderImpl()
{}

} // namespace Diligent
