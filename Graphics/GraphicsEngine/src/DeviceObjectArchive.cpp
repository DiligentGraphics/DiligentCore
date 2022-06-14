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


#if 0
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
#endif

DeviceObjectArchive::DeviceObjectArchive(IArchive* pArchive) noexcept(false)
{
    if (pArchive == nullptr)
        LOG_ERROR_AND_THROW("pArchive must not be null");

    m_pRawData = DataBlobImpl::Create(StaticCast<size_t>(pArchive->GetSize()));
    pArchive->Read(0, pArchive->GetSize(), m_pRawData->GetDataPtr());
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
    return "TBD";

#if 0
    std::stringstream Output;
    Output << "Archive contents:\n";

    constexpr char SeparatorLine[] = "------------------\n";
    constexpr char Ident1[]        = "  ";
    constexpr char Ident2[]        = "    ";

    // Print header
    {
        Output << "Header\n"
               << Ident1 << "version: " << HeaderVersion << '\n';
    }

    auto GetNumFieldWidth = [](Uint32 Number) //
    {
        size_t Width = 1;
        for (; Number >= 10; Number /= 10)
            ++Width;
        return Width;
    };

    // Print chunks, for example:
    //   Chunks
    //     Debug Info:          [104; 135) -  31 bytes
    //     Resource Signatures: [135; 189) -  54 bytes
    //     Compute Pipelines:   [189; 243) -  54 bytes
    //     Shaders:             [243; 299) -  56 bytes
    {
        size_t MaxChunkNameLen = 0;
        Uint32 MaxOffset       = 0;
        for (auto& Chunk : m_Chunks)
        {
            MaxChunkNameLen = std::max(MaxChunkNameLen, strlen(ResourceGroupTypeToString(Chunk.Type)));
            MaxOffset       = std::max(MaxOffset, Chunk.Offset + Chunk.Size);
        }
        const auto OffsetFieldW = GetNumFieldWidth(MaxOffset);

        Output << SeparatorLine
               << "Chunks\n";
        for (auto& Chunk : m_Chunks)
        {
            Output << Ident1 << std::setw(MaxChunkNameLen + 2) << std::left << std::string{ResourceGroupTypeToString(Chunk.Type)} + ": "
                   << "[" << std::setw(OffsetFieldW) << std::right << Chunk.Offset << "; "
                   << std::setw(OffsetFieldW) << (Chunk.Offset + Chunk.Size)
                   << ") - " << std::setw(OffsetFieldW) << Chunk.Size << " bytes\n";
        }
    }

    std::vector<Uint8> Temp;

    // Print debug info, for example:
    //   Debug info
    //     APIVersion: 252002
    //     GitHash:    API252002-15-g127edf0+
    {
        for (auto& Chunk : m_Chunks)
        {
            if (Chunk.Type != ResourceGroupType::DebugInfo)
                continue;

            Temp.resize(Chunk.Size);
            if (m_CommonData.Read(Chunk.Offset, Temp.size(), Temp.data()))
            {
                Serializer<SerializerMode::Read> Ser{SerializedData{Temp.data(), Temp.size()}};

                Uint32      APIVersion = 0;
                const char* GitHash    = nullptr;
                Ser(APIVersion, GitHash);

                Output << SeparatorLine
                       << "Debug info\n"
                       << Ident1 << "APIVersion: " << APIVersion << '\n'
                       << Ident1 << "GitHash:    " << GitHash << "\n";
            }
            break;
        }
    }

    constexpr char CommonDataName[] = "Common";

    static const auto MaxDevNameLen = [CommonDataName]() {
        size_t MaxLen = strlen(CommonDataName);
        for (Uint32 i = 0; i < static_cast<Uint32>(DeviceType::Count); ++i)
        {
            MaxLen = std::max(MaxLen, strlen(ArchiveDeviceTypeToString(i)));
        }
        return MaxLen;
    }();


    // Print archive blocks, for example:
    //   Blocks
    //     Common          - 639 bytes
    //     OpenGL          -  88 bytes
    //     Direct3D11      - 144 bytes
    //     Direct3D12      - 160 bytes
    //     Vulkan          - 168 bytes
    //     Metal for MacOS - none
    //     Metal for iOS   - none
    {
        Uint32 MaxBlockSize = m_CommonData.Size;
        for (Uint32 dev = 0; dev < static_cast<Uint32>(DeviceType::Count); ++dev)
            MaxBlockSize = std::max(MaxBlockSize, m_DeviceSpecific[dev].Size);
        const auto BlockSizeFieldW = GetNumFieldWidth(MaxBlockSize);

        Output << SeparatorLine
               << "Blocks\n"
               << Ident1 << std::setw(MaxDevNameLen) << std::setfill(' ') << std::left << CommonDataName
               << " - " << std::setw(BlockSizeFieldW) << std::right << m_CommonData.Size << " bytes\n";

        for (Uint32 dev = 0; dev < static_cast<Uint32>(DeviceType::Count); ++dev)
        {
            const auto& Block   = m_DeviceSpecific[dev];
            const auto* DevName = ArchiveDeviceTypeToString(dev);

            Output << Ident1 << std::setw(MaxDevNameLen) << std::setfill(' ') << std::left << DevName;

            if (!Block.IsValid())
                Output << " - none\n";
            else
                Output << " - " << std::setw(BlockSizeFieldW) << std::right << Block.Size << " bytes\n";
        }
    }


    const auto LoadResource = [&](const ArchiveRegion& Region) //
    {
        Temp.clear();

        // ignore m_CommonData.Offset
        if (Region.Offset + Region.Size > m_CommonData.Size)
            return false;

        Temp.resize(Region.Size);
        return m_CommonData.Read(Region.Offset, Temp.size(), Temp.data());
    };

    // Print Signatures and Pipelines, for example:
    //   Resource Signatures
    //     ResourceSignatureTest
    //       Common          - [211; 519)
    //       OpenGL          - [  0;  52)
    //       Direct3D11      - [  0;  96)
    //       Direct3D12      - [  0; 104)
    //       Vulkan          - [  0; 108)
    //       Metal for MacOS - none
    //       Metal for iOS   - none
    {
        const auto PrintResources = [&](const NameToArchiveRegionMap& ResMap, const char* ResTypeName) //
        {
            if (ResMap.empty())
                return;

            Output << SeparatorLine
                   << ResTypeName
                   << "\n";

            for (const auto& it : ResMap)
            {
                const auto& Region = it.second;

                Output << Ident1 << it.first.GetStr();

                DataHeaderBase Header{ResourceGroupType::Undefined};
                if (LoadResource(Region) &&
                    Temp.size() >= sizeof(Header))
                {
                    memcpy(&Header, Temp.data(), sizeof(Header));
                    auto MaxOffset = Region.Offset + Region.Size;
                    for (Uint32 dev = 0; dev < Header.DeviceSpecificDataSize.size(); ++dev)
                    {
                        const auto Size   = Header.DeviceSpecificDataSize[dev];
                        const auto Offset = Header.DeviceSpecificDataOffset[dev];
                        if (Offset != DataHeaderBase::InvalidOffset)
                            MaxOffset = std::max(MaxOffset, Offset + Size);
                    }
                    const auto OffsetFieldW = GetNumFieldWidth(MaxOffset);

                    Output << '\n';

                    // Common data & header
                    Output << Ident2 << std::setw(MaxDevNameLen) << std::left << CommonDataName
                           << " - [" << std::setw(OffsetFieldW) << std::right << Region.Offset << "; "
                           << std::setw(OffsetFieldW) << std::right << (Region.Offset + Region.Size) << ")\n";

                    for (Uint32 dev = 0; dev < Header.DeviceSpecificDataSize.size(); ++dev)
                    {
                        const auto  Size    = Header.DeviceSpecificDataSize[dev];
                        const auto  Offset  = Header.DeviceSpecificDataOffset[dev];
                        const auto& Block   = m_DeviceSpecific[dev];
                        const char* DevName = ArchiveDeviceTypeToString(dev);

                        Output << Ident2 << std::setw(MaxDevNameLen) << std::left << DevName;
                        if (Size == 0 || Offset == DataHeaderBase::InvalidOffset || !Block.IsValid())
                            Output << " - none\n";
                        else
                            Output << " - [" << std::setw(OffsetFieldW) << std::right << Offset
                                   << "; " << std::setw(OffsetFieldW) << std::right << (Offset + Size) << ")\n";
                    }
                }
                else
                {
                    Output << " - invalid data\n";
                }
            }
        };

        static_assert(static_cast<Uint32>(ResourceGroupType::Count) == 9, "Please handle the new chunk type below");
        // clang-format off
        PrintResources(m_ResMap.Sign,     "Resource Signatures");
        PrintResources(m_ResMap.GraphPSO, "Graphics Pipeline States");
        PrintResources(m_ResMap.CompPSO,  "Compute Pipeline States");
        PrintResources(m_ResMap.RayTrPSO, "Ray-tracing Pipeline States");
        PrintResources(m_ResMap.TilePSO,  "Tile Pipeline States");
        // clang-format on
    }

    // Render passes, for example:
    //   Render Passes
    //     RenderPassTest
    //       Common - [2191; 2258)
    {
        if (!m_ResMap.RenderPass.empty())
        {
            Output << SeparatorLine
                   << "Render Passes\n";

            for (const auto& it : m_ResMap.RenderPass)
            {
                const auto& Region = it.second;
                Output << Ident1 << it.first.GetStr();

                if (LoadResource(Region))
                {
                    Output << '\n'
                           << Ident2 << CommonDataName << " - [" << Region.Offset << "; " << (Region.Offset + Region.Size) << ")\n";
                }
                else
                    Output << " - invalid data\n";
            }
        }
    }

    // Shaders
    {
        for (auto& Chunk : m_Chunks)
        {
            if (Chunk.Type != ResourceGroupType::Shaders)
                continue;

            ShadersDataHeader Header;
            VERIFY_EXPR(sizeof(Header) == Chunk.Size);

            if (m_CommonData.Read(Chunk.Offset, sizeof(Header), &Header))
            {
                Output << SeparatorLine
                       << "Shaders\n";
                for (Uint32 dev = 0; dev < Header.DeviceSpecificDataSize.size(); ++dev)
                {
                    const auto  Size    = Header.DeviceSpecificDataSize[dev];
                    const auto  Offset  = Header.DeviceSpecificDataOffset[dev];
                    const auto& Block   = m_DeviceSpecific[dev];
                    const char* DevName = ArchiveDeviceTypeToString(dev);

                    Output << Ident1 << std::setw(MaxDevNameLen) << std::setfill(' ') << std::left << DevName;

                    if (Size == 0 || Offset == DataHeaderBase::InvalidOffset || !Block.IsValid())
                        Output << " - none\n";
                    else
                    {
                        Output << " - list range: [" << Offset << "; " << (Offset + Size) << "), count: " << (Size / sizeof(ArchiveRegion));

                        // Calculate data size
                        std::vector<ArchiveRegion> OffsetAndSizeArray(Size / sizeof(ArchiveRegion));
                        if (Block.Read(Offset, OffsetAndSizeArray.size() * sizeof(OffsetAndSizeArray[0]), OffsetAndSizeArray.data()))
                        {
                            Uint32 MinOffset = ~0u;
                            Uint32 MaxOffset = 0;
                            for (auto& OffsetAndSize : OffsetAndSizeArray)
                            {
                                MinOffset = std::min(MinOffset, OffsetAndSize.Offset);
                                MaxOffset = std::max(MaxOffset, OffsetAndSize.Offset + OffsetAndSize.Size);
                            }
                            Output << "; data range: [" << MinOffset << "; " << MaxOffset << ") - " << (MaxOffset - MinOffset) << " bytes";
                        }
                        Output << "\n";
                    }
                }
            }

            break;
        }
    }

    return Output.str();
#endif
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
