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

#include "DeviceObjectArchive.hpp"

#include <bitset>
#include <unordered_set>
#include <algorithm>
#include <set>
#include <algorithm>
#include <sstream>

#include "DebugUtilities.hpp"
#include "PSOSerializer.hpp"
#include "DataBlobImpl.hpp"

namespace Diligent
{

DeviceObjectArchive::DeviceObjectArchive() noexcept
{
}

void DeviceObjectArchive::Deserialize(const void* pData, size_t Size) noexcept(false)
{
    Serializer<SerializerMode::Read> Reader{SerializedData{const_cast<void*>(pData), Size}};

    ArchiveHeader Header;
    if (!Reader(Header.MagicNumber, Header.Version, Header.APIVersion, Header.GitHash))
        LOG_ERROR_AND_THROW("Failed to read archive header");

    if (Header.MagicNumber != HeaderMagicNumber)
        LOG_ERROR_AND_THROW("Invalid archive header");

    if (Header.Version != ArchiveVersion)
        LOG_ERROR_AND_THROW("Unsupported archive version: ", Header.Version, ". Expected version: ", Uint32{ArchiveVersion});

    Uint32 NumResources = 0;
    if (!Reader(NumResources))
        LOG_ERROR_AND_THROW("Failed to read the number of named resources");

    for (Uint32 res = 0; res < NumResources; ++res)
    {
        const char*  Name    = nullptr;
        ResourceType ResType = ResourceType::Undefined;
        if (!Reader(ResType, Name))
            LOG_ERROR_AND_THROW("Failed to read resource name");
        VERIFY_EXPR(Name != nullptr);

        auto& ResData = m_NamedResources[NamedResourceKey{ResType, Name}];

        if (!Reader.Serialize(ResData.Common))
            LOG_ERROR_AND_THROW("Failed to read common data of resource '", Name, "'.");

        for (auto& DevData : ResData.DeviceSpecific)
        {
            if (!Reader.Serialize(DevData))
                LOG_ERROR_AND_THROW("Failed to read device-specific data of resource '", Name, "'.");
        }
    }

    for (auto& Shaders : m_DeviceShaders)
    {
        Uint32 NumShaders = 0;
        if (!Reader(NumShaders))
            LOG_ERROR_AND_THROW("Failed to read the number of shaders");
        Shaders.resize(NumShaders);
        for (auto& Shader : Shaders)
        {
            if (!Reader.Serialize(Shader))
                LOG_ERROR_AND_THROW("Failed to read shader data");
        }
    }
}

void DeviceObjectArchive::Serialize(IDataBlob** ppDataBlob) const
{
    DEV_CHECK_ERR(ppDataBlob != nullptr, "Pointer to the data blob object must not be null");
    DEV_CHECK_ERR(*ppDataBlob == nullptr, "Data blob object must be null");

    auto SerializeThis = [this](auto& Ser) {
        ArchiveHeader Header;
        Ser(Header.MagicNumber, Header.Version, Header.APIVersion);

        const char* GitHash = nullptr;
#ifdef DILIGENT_CORE_COMMIT_HASH
        GitHash = DILIGENT_CORE_COMMIT_HASH;
#endif
        Ser(GitHash);

        Uint32 NumResources = static_cast<Uint32>(m_NamedResources.size());
        Ser(NumResources);

        for (const auto& res_it : m_NamedResources)
        {
            const auto* Name    = res_it.first.GetName();
            const auto  ResType = res_it.first.GetType();
            Ser(ResType, Name);
            Ser.Serialize(res_it.second.Common);
            for (const auto& DevData : res_it.second.DeviceSpecific)
                Ser.Serialize(DevData);
        }

        for (auto& Shaders : m_DeviceShaders)
        {
            Uint32 NumShaders = static_cast<Uint32>(Shaders.size());
            Ser(NumShaders);
            for (const auto& Shader : Shaders)
                Ser.Serialize(Shader);
        }
    };

    Serializer<SerializerMode::Measure> Measurer;
    SerializeThis(Measurer);

    auto SerializedData = Measurer.AllocateData(GetRawAllocator());

    Serializer<SerializerMode::Write> Writer{SerializedData};
    SerializeThis(Writer);
    VERIFY_EXPR(Writer.IsEnded());

    auto pDataBlob = DataBlobImpl::Create(SerializedData.Size(), SerializedData.Ptr());
    *ppDataBlob    = pDataBlob.Detach();
}


namespace
{

const char* ArchiveDeviceTypeToString(Uint32 dev)
{
    using DeviceType = DeviceObjectArchive::DeviceType;
    static_assert(static_cast<Uint32>(DeviceType::Count) == 6, "Please handle the new archive device type below");
    switch (static_cast<DeviceType>(dev))
    {
        // clang-format off
        case DeviceType::OpenGL:      return "OpenGL";
        case DeviceType::Direct3D11:  return "Direct3D11";
        case DeviceType::Direct3D12:  return "Direct3D12";
        case DeviceType::Vulkan:      return "Vulkan";
        case DeviceType::Metal_iOS:   return "Metal for iOS";
        case DeviceType::Metal_MacOS: return "Metal for MacOS";
        // clang-format on
        default: return "unknown";
    }
}

} // namespace


DeviceObjectArchive::DeviceObjectArchive(IDataBlob* pArchive) noexcept(false) :
    m_pRawData{pArchive}
{
    if (!m_pRawData)
        LOG_ERROR_AND_THROW("pArchive must not be null");

    Deserialize(m_pRawData->GetConstDataPtr(), StaticCast<size_t>(pArchive->GetSize()));
}

DeviceObjectArchive::DeviceType DeviceObjectArchive::RenderDeviceTypeToArchiveDeviceType(RENDER_DEVICE_TYPE Type)
{
    static_assert(RENDER_DEVICE_TYPE_COUNT == 7, "Did you add a new render device type? Please handle it here.");
    switch (Type)
    {
        // clang-format off
        case RENDER_DEVICE_TYPE_D3D11:  return DeviceObjectArchive::DeviceType::Direct3D11;
        case RENDER_DEVICE_TYPE_D3D12:  return DeviceObjectArchive::DeviceType::Direct3D12;
        case RENDER_DEVICE_TYPE_GL:     return DeviceObjectArchive::DeviceType::OpenGL;
        case RENDER_DEVICE_TYPE_GLES:   return DeviceObjectArchive::DeviceType::OpenGL;
        case RENDER_DEVICE_TYPE_VULKAN: return DeviceObjectArchive::DeviceType::Vulkan;
#if PLATFORM_MACOS
        case RENDER_DEVICE_TYPE_METAL:  return DeviceObjectArchive::DeviceType::Metal_MacOS;
#elif PLATFORM_IOS || PLATFORM_TVOS
        case RENDER_DEVICE_TYPE_METAL:  return DeviceObjectArchive::DeviceType::Metal_iOS;
#endif
        // clang-format on
        default:
            UNEXPECTED("Unexpected device type");
            return DeviceObjectArchive::DeviceType::Count;
    }
}

const char* DeviceObjectArchive::ResourceTypeToString(ResourceType Type)
{
    static_assert(static_cast<size_t>(ResourceType::Count) == 7, "Please handle the new chunk type below");
    switch (Type)
    {
        // clang-format off
        case ResourceType::Undefined:          return "Undefined";
        case ResourceType::ResourceSignature:  return "Resource Signatures";
        case ResourceType::GraphicsPipeline:   return "Graphics Pipelines";
        case ResourceType::ComputePipeline:    return "Compute Pipelines";
        case ResourceType::RayTracingPipeline: return "Ray-Tracing Pipelines";
        case ResourceType::TilePipeline:       return "Tile Pipelines";
        case ResourceType::RenderPass:         return "Render Passes";
        // clang-format on
        default:
            UNEXPECTED("Unexpected chunk type");
            return "";
    }
}

const SerializedData& DeviceObjectArchive::GetDeviceSpecificData(ResourceType Type,
                                                                 const char*  Name,
                                                                 DeviceType   DevType) const noexcept
{
    auto it = m_NamedResources.find(NamedResourceKey{Type, Name});
    if (it == m_NamedResources.end())
    {
        ;
        LOG_ERROR_MESSAGE("Resource '", Name, "' is not present in the archive");
        static const SerializedData NullData;
        return NullData;
    }
    VERIFY_EXPR(SafeStrEqual(Name, it->first.GetName()));
    return it->second.DeviceSpecific[static_cast<size_t>(DevType)];
}

std::string DeviceObjectArchive::ToString() const
{
    std::stringstream Output;
    Output << "Archive contents:\n";

    constexpr char SeparatorLine[] = "------------------\n";
    constexpr char Ident1[]        = "  ";
    constexpr char Ident2[]        = "    ";

    // Print header
    {
        Output << "Header\n"
               << Ident1 << "version: " << ArchiveVersion << '\n';
    }

    constexpr char CommonDataName[] = "Common";

    auto GetNumFieldWidth = [](auto Number) //
    {
        size_t Width = 1;
        for (; Number >= 10; Number /= 10)
            ++Width;
        return Width;
    };

    // Print resources, e.g.:
    //
    //   ------------------
    //   Resource Signatures (1)
    //     Test PRS
    //       Common     1015 bytes
    //       OpenGL      729 bytes
    //       Direct3D11  384 bytes
    //       Direct3D12  504 bytes
    //       Vulkan      881 bytes
    {
        std::array<std::vector<std::reference_wrapper<const decltype(m_NamedResources)::value_type>>, static_cast<size_t>(ResourceType::Count)> ResourcesByType;
        for (const auto& it : m_NamedResources)
        {
            ResourcesByType[static_cast<size_t>(it.first.GetType())].emplace_back(it);
        }
        for (const auto& Resources : ResourcesByType)
        {
            if (Resources.empty())
                continue;

            const auto ResType = Resources.front().get().first.GetType();
            Output << SeparatorLine
                   << ResourceTypeToString(ResType) << " (" << Resources.size() << ")\n";

            for (const auto& res_ref : Resources)
            {
                const auto& it = res_ref.get();
                Output << Ident1 << it.first.GetName() << '\n';
                const auto& Res = it.second;

                auto   MaxSize       = Res.Common.Size();
                size_t MaxDevNameLen = strlen(CommonDataName);
                for (Uint32 i = 0; i < Res.DeviceSpecific.size(); ++i)
                {
                    const auto DevDataSize = Res.DeviceSpecific[i].Size();

                    MaxSize = std::max(MaxSize, DevDataSize);
                    if (DevDataSize != 0)
                        MaxDevNameLen = std::max(MaxDevNameLen, strlen(ArchiveDeviceTypeToString(i)));
                }
                const auto SizeFieldW = GetNumFieldWidth(MaxSize);

                Output << Ident2 << std::setw(MaxDevNameLen) << std::left << CommonDataName << ' '
                       << std::setw(SizeFieldW) << std::right << Res.Common.Size() << " bytes\n";

                for (Uint32 i = 0; i < Res.DeviceSpecific.size(); ++i)
                {
                    const auto DevDataSize = Res.DeviceSpecific[i].Size();
                    if (DevDataSize > 0)
                    {
                        Output << Ident2 << std::setw(MaxDevNameLen) << std::left << ArchiveDeviceTypeToString(i) << ' '
                               << std::setw(SizeFieldW) << std::right << DevDataSize << " bytes\n";
                    }
                }
            }
        }
    }

    // Print shaders, e.g.
    //
    //   ------------------
    //   Shaders
    //     OpenGL(2)
    //       [0] 4020 bytes
    //       [1] 4020 bytes
    //     Vulkan(2)
    //       [0] 8364 bytes
    //       [1] 7380 bytes
    {
        bool HasShaders = false;
        for (const auto& Shaders : m_DeviceShaders)
        {
            if (!Shaders.empty())
                HasShaders = true;
        }

        if (HasShaders)
        {
            Output << SeparatorLine
                   << "Shaders\n";

            for (Uint32 dev = 0; dev < m_DeviceShaders.size(); ++dev)
            {
                const auto& Shaders = m_DeviceShaders[dev];
                if (Shaders.empty())
                    continue;
                Output << Ident1 << ArchiveDeviceTypeToString(dev) << '(' << Shaders.size() << ")\n";

                size_t MaxSize = 0;
                for (const auto& Shader : Shaders)
                    MaxSize = std::max(MaxSize, Shader.Size());
                const auto IdxFieldW  = GetNumFieldWidth(Shaders.size());
                const auto SizeFieldW = GetNumFieldWidth(MaxSize);
                for (Uint32 idx = 0; idx < Shaders.size(); ++idx)
                {
                    Output << Ident2 << '[' << std::setw(IdxFieldW) << std::right << idx << "] "
                           << std::setw(SizeFieldW) << std::right << Shaders[idx].Size() << " bytes\n";
                }
            }
        }
    }

    return Output.str();
}


void DeviceObjectArchive::RemoveDeviceData(DeviceType Dev) noexcept(false)
{
    for (auto& res_it : m_NamedResources)
        res_it.second.DeviceSpecific[static_cast<size_t>(Dev)] = {};

    m_DeviceShaders[static_cast<size_t>(Dev)].clear();
}

void DeviceObjectArchive::AppendDeviceData(const DeviceObjectArchive& Src, DeviceType Dev) noexcept(false)
{
    auto& Allocator = GetRawAllocator();
    for (auto& dst_res_it : m_NamedResources)
    {
        auto& DstData = dst_res_it.second.DeviceSpecific[static_cast<size_t>(Dev)];
        // Clear dst device data to make sure we don't have invalid shader indices
        DstData = {};

        auto src_res_it = Src.m_NamedResources.find(dst_res_it.first);
        if (src_res_it == Src.m_NamedResources.end())
            continue;

        const auto& SrcData{src_res_it->second.DeviceSpecific[static_cast<size_t>(Dev)]};
        // Always copy src data even if it is empty
        dst_res_it.second.DeviceSpecific[static_cast<size_t>(Dev)] = SrcData.MakeCopy(Allocator);
    }

    // Copy all shaders
    const auto& SrcShaders = Src.m_DeviceShaders[static_cast<size_t>(Dev)];
    auto&       DstShaders = m_DeviceShaders[static_cast<size_t>(Dev)];
    DstShaders.clear();
    for (const auto& SrcShader : SrcShaders)
        DstShaders.emplace_back(SrcShader.MakeCopy(Allocator));
}

void DeviceObjectArchive::Serialize(IFileStream* pStream) const noexcept(false)
{
    DEV_CHECK_ERR(pStream != nullptr, "File stream must not be null");
    RefCntAutoPtr<IDataBlob> pDataBlob;
    Serialize(&pDataBlob);
    VERIFY_EXPR(pDataBlob);
    pStream->Write(pDataBlob->GetConstDataPtr(), pDataBlob->GetSize());
}

} // namespace Diligent
