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

#include "DebugUtilities.hpp"
#include "PSOSerializer.hpp"

namespace Diligent
{


bool DeviceObjectArchive::ArchiveBlock::LoadToMemory()
{
    if (!IsValid())
        return false;

    Memory.resize(Size);
    if (!pArchive->Read(Offset, Memory.size(), Memory.data()))
    {
        Memory.clear();
        return false;
    }
    return true;
}

bool DeviceObjectArchive::ArchiveBlock::Read(Uint64 OffsetInBlock, Uint64 DataSize, void* pData) const
{
    if (!IsValid())
        return false;

    if (!Memory.empty())
    {
        if (OffsetInBlock < Memory.size() && OffsetInBlock + DataSize <= Memory.size())
        {
            memcpy(pData, &Memory[StaticCast<size_t>(OffsetInBlock)], StaticCast<size_t>(DataSize));
            return true;
        }
        return false;
    }

    auto* Archive = pArchive.RawPtr<IArchive>();
    if (Archive && Archive->Read(Offset + OffsetInBlock, DataSize, pData))
        return true;

    return false;
}

bool DeviceObjectArchive::ArchiveBlock::Write(Uint64 OffsetInBlock, Uint64 DataSize, const void* pData)
{
    if (!IsValid())
        return false;

    if (!Memory.empty())
    {
        if (OffsetInBlock < Memory.size() && OffsetInBlock + DataSize <= Memory.size())
        {
            memcpy(&Memory[static_cast<size_t>(OffsetInBlock)], pData, static_cast<size_t>(DataSize));
            return true;
        }
    }

    // can not write to archive
    return false;
}

template <>
const DeviceObjectArchive::NameToArchiveRegionMap& DeviceObjectArchive::NamedResourcesMap::GetPsoMap<GraphicsPipelineStateCreateInfo>() const
{
    return GraphPSO;
}
template <>
const DeviceObjectArchive::NameToArchiveRegionMap& DeviceObjectArchive::NamedResourcesMap::GetPsoMap<ComputePipelineStateCreateInfo>() const
{
    return CompPSO;
}
template <>
const DeviceObjectArchive::NameToArchiveRegionMap& DeviceObjectArchive::NamedResourcesMap::GetPsoMap<TilePipelineStateCreateInfo>() const
{
    return TilePSO;
}
template <>
const DeviceObjectArchive::NameToArchiveRegionMap& DeviceObjectArchive::NamedResourcesMap::GetPsoMap<RayTracingPipelineStateCreateInfo>() const
{
    return RayTrPSO;
}

namespace
{

void ReadNamedResourceRegions(IArchive*                                    pArchive,
                              const DeviceObjectArchive::ChunkHeader&      Chunk,
                              DeviceObjectArchive::NameToArchiveRegionMap& NameToRegion) noexcept(false)
{
    using NamedResourceArrayHeader = DeviceObjectArchive::NamedResourceArrayHeader;
    using ArchiveRegion            = DeviceObjectArchive::ArchiveRegion;

    VERIFY_EXPR(Chunk.Type == DeviceObjectArchive::ChunkType::ResourceSignature ||
                Chunk.Type == DeviceObjectArchive::ChunkType::GraphicsPipelineStates ||
                Chunk.Type == DeviceObjectArchive::ChunkType::ComputePipelineStates ||
                Chunk.Type == DeviceObjectArchive::ChunkType::RayTracingPipelineStates ||
                Chunk.Type == DeviceObjectArchive::ChunkType::TilePipelineStates ||
                Chunk.Type == DeviceObjectArchive::ChunkType::RenderPass);

    std::vector<Uint8> Data(Chunk.Size);
    if (!pArchive->Read(Chunk.Offset, Data.size(), Data.data()))
    {
        LOG_ERROR_AND_THROW("Failed to read resource list from archive");
    }

    FixedLinearAllocator InPlaceAlloc{Data.data(), Data.size()};

    const auto& Header          = *InPlaceAlloc.Allocate<NamedResourceArrayHeader>();
    const auto* NameLengthArray = InPlaceAlloc.Allocate<Uint32>(Header.Count);
    const auto* DataSizeArray   = InPlaceAlloc.Allocate<Uint32>(Header.Count);
    const auto* DataOffsetArray = InPlaceAlloc.Allocate<Uint32>(Header.Count);

    // Read names
    for (Uint32 i = 0; i < Header.Count; ++i)
    {
        if (InPlaceAlloc.GetCurrentSize() + NameLengthArray[i] > Data.size())
        {
            LOG_ERROR_AND_THROW("Failed to read archive data");
        }
        if (size_t{DataOffsetArray[i]} + size_t{DataSizeArray[i]} > pArchive->GetSize())
        {
            LOG_ERROR_AND_THROW("Failed to read archive data");
        }
        const auto* Name = InPlaceAlloc.Allocate<char>(NameLengthArray[i]);
        VERIFY_EXPR(strlen(Name) + 1 == NameLengthArray[i]);

        // Make string copy
        bool Inserted = NameToRegion.emplace(HashMapStringKey{Name, true}, ArchiveRegion{DataOffsetArray[i], DataSizeArray[i]}).second;
        VERIFY(Inserted, "Each resource name in the archive map must be unique");
    }
}

void ReadArchiveDebugInfo(IArchive*                               pArchive,
                          const DeviceObjectArchive::ChunkHeader& Chunk,
                          DeviceObjectArchive::ArchiveDebugInfo&  DebugInfo) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == DeviceObjectArchive::ChunkType::ArchiveDebugInfo);

    SerializedData Data{Chunk.Size, GetRawAllocator()};
    if (!pArchive->Read(Chunk.Offset, Data.Size(), Data.Ptr()))
    {
        LOG_ERROR_AND_THROW("Failed to read archive debug info");
    }

    Serializer<SerializerMode::Read> Ser{Data};

    Ser(DebugInfo.APIVersion);

    const char* GitHash = nullptr;
    Ser(GitHash);

    VERIFY_EXPR(Ser.IsEnded());
    DebugInfo.GitHash = String{GitHash};

    if (DebugInfo.APIVersion != DILIGENT_API_VERSION)
        LOG_INFO_MESSAGE("Archive was created with Engine API version (", DebugInfo.APIVersion, ") but is used with (", DILIGENT_API_VERSION, ")");
#ifdef DILIGENT_CORE_COMMIT_HASH
    if (DebugInfo.GitHash != DILIGENT_CORE_COMMIT_HASH)
        LOG_INFO_MESSAGE("Archive was built with Diligent Core git hash '", DebugInfo.GitHash, "' but is used with '", DILIGENT_CORE_COMMIT_HASH, "'.");
#endif
}

void ReadShadersHeader(IArchive*                               pArchive,
                       const DeviceObjectArchive::ChunkHeader& Chunk,
                       DeviceObjectArchive::ShadersDataHeader& ShadersHeader) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == DeviceObjectArchive::ChunkType::Shaders);
    VERIFY_EXPR(Chunk.Size == sizeof(ShadersHeader));

    if (!pArchive->Read(Chunk.Offset, sizeof(ShadersHeader), &ShadersHeader))
    {
        LOG_ERROR_AND_THROW("Failed to read shaders data header from the archive");
    }
}

const char* ArchiveDeviceTypeToString(Uint32 dev)
{
    using DeviceType = DeviceObjectArchive::DeviceType;
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

DeviceObjectArchive::DeviceObjectArchive(IArchive* pArchive) noexcept(false) :
    m_pArchive{pArchive}
{
    if (pArchive == nullptr)
        LOG_ERROR_AND_THROW("pArchive must not be null");

    // Read header
    ArchiveHeader Header{};
    {
        if (!pArchive->Read(0, sizeof(Header), &Header))
        {
            LOG_ERROR_AND_THROW("Failed to read archive header");
        }
        if (Header.MagicNumber != HeaderMagicNumber)
        {
            LOG_ERROR_AND_THROW("Archive header magic number is incorrect");
        }
        if (Header.Version != HeaderVersion)
        {
            LOG_ERROR_AND_THROW("Archive version (", Header.Version, ") is not supported; expected version: ", Uint32{HeaderVersion}, ".");
        }

        m_BaseOffsets = Header.BlockBaseOffsets;
    }

    // Read chunks
    m_Chunks.resize(Header.NumChunks);
    if (!pArchive->Read(sizeof(Header), sizeof(m_Chunks[0]) * m_Chunks.size(), m_Chunks.data()))
    {
        LOG_ERROR_AND_THROW("Failed to read chunk headers");
    }

    std::bitset<static_cast<size_t>(ChunkType::Count)> ProcessedBits{};
    for (const auto& Chunk : m_Chunks)
    {
        if (ProcessedBits[static_cast<size_t>(Chunk.Type)])
        {
            LOG_ERROR_AND_THROW("Multiple chunks with the same type are not allowed");
        }
        ProcessedBits[static_cast<size_t>(Chunk.Type)] = true;

        static_assert(static_cast<size_t>(ChunkType::Count) == 9, "Please handle the new chunk type below");
        switch (Chunk.Type)
        {
            // clang-format off
            case ChunkType::ArchiveDebugInfo:         ReadArchiveDebugInfo    (pArchive, Chunk, m_DebugInfo);         break;
            case ChunkType::ResourceSignature:        ReadNamedResourceRegions(pArchive, Chunk, m_ResMap.Sign);       break;
            case ChunkType::GraphicsPipelineStates:   ReadNamedResourceRegions(pArchive, Chunk, m_ResMap.GraphPSO);   break;
            case ChunkType::ComputePipelineStates:    ReadNamedResourceRegions(pArchive, Chunk, m_ResMap.CompPSO);    break;
            case ChunkType::RayTracingPipelineStates: ReadNamedResourceRegions(pArchive, Chunk, m_ResMap.RayTrPSO);   break;
            case ChunkType::TilePipelineStates:       ReadNamedResourceRegions(pArchive, Chunk, m_ResMap.TilePSO);    break;
            case ChunkType::RenderPass:               ReadNamedResourceRegions(pArchive, Chunk, m_ResMap.RenderPass); break;
            case ChunkType::Shaders:                  ReadShadersHeader       (pArchive, Chunk, m_ShadersHeader);     break;
            // clang-format on
            default:
                LOG_ERROR_AND_THROW("Unknown chunk type (", static_cast<Uint32>(Chunk.Type), ")");
        }
    }

    // Calculate device-specific block sizes
    {
        std::set<Uint32> SortedOffsets;

        const Uint32 ArchiveSize = StaticCast<Uint32>(pArchive->GetSize());
        for (auto BaseOffset : m_BaseOffsets)
        {
            if (BaseOffset == DataHeaderBase::InvalidOffset)
                continue;
            VERIFY_EXPR(BaseOffset < ArchiveSize);
            SortedOffsets.insert(BaseOffset);
        }
        // TODO: using the archive size as last offset is incorrect as
        //       there may be some additional data past the archive data.
        SortedOffsets.insert(ArchiveSize);

        for (size_t i = 0; i < static_cast<size_t>(BlockOffsetType::Count); ++i)
        {
            const auto BaseOffset = m_BaseOffsets[i];
            if (BaseOffset == DataHeaderBase::InvalidOffset)
                continue;
            auto next_offset = SortedOffsets.find(BaseOffset);
            VERIFY_EXPR(next_offset != SortedOffsets.end());
            ++next_offset;
            VERIFY_EXPR(next_offset != SortedOffsets.end());
            Uint32 BlockSize    = *next_offset - BaseOffset;
            m_DeviceSpecific[i] = ArchiveBlock{pArchive, BaseOffset, BlockSize};
        }

        m_CommonData = ArchiveBlock{pArchive, 0, *SortedOffsets.begin()};
        VERIFY_EXPR(m_CommonData.IsValid());
    }

    VERIFY_EXPR(Validate());
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

DeviceObjectArchive::BlockOffsetType DeviceObjectArchive::GetBlockOffsetType(DeviceType DevType)
{
    static_assert(static_cast<size_t>(DeviceType::Count) == 6, "Please handle the new device type below");
    switch (DevType)
    {
        // clang-format off
        case DeviceType::OpenGL:      return BlockOffsetType::OpenGL;
        case DeviceType::Direct3D11:  return BlockOffsetType::Direct3D11;
        case DeviceType::Direct3D12:  return BlockOffsetType::Direct3D12;
        case DeviceType::Vulkan:      return BlockOffsetType::Vulkan;
        case DeviceType::Metal_iOS:   return BlockOffsetType::Metal_iOS;
        case DeviceType::Metal_MacOS: return BlockOffsetType::Metal_MacOS;
        // clang-format on
        default:
            UNEXPECTED("Unexpected device type");
            return BlockOffsetType::Count;
    }
}

const char* DeviceObjectArchive::ChunkTypeToResName(ChunkType Type)
{
    static_assert(static_cast<size_t>(ChunkType::Count) == 9, "Please handle the new chunk type below");
    switch (Type)
    {
        // clang-format off
        case ChunkType::Undefined:                return "Undefined";
        case ChunkType::ArchiveDebugInfo:         return "Debug Info";
        case ChunkType::ResourceSignature:        return "Resource Signatures";
        case ChunkType::GraphicsPipelineStates:   return "Graphics Pipelines";
        case ChunkType::ComputePipelineStates:    return "Compute Pipelines";
        case ChunkType::RayTracingPipelineStates: return "Ray-Tracing Pipelines";
        case ChunkType::TilePipelineStates:       return "Tile Pipelines";
        case ChunkType::RenderPass:               return "Render Passes";
        case ChunkType::Shaders:                  return "Shaders";
        // clang-format on
        default:
            UNEXPECTED("Unexpected chunk type");
            return "";
    }
}


const std::vector<DeviceObjectArchive::ArchiveRegion>& DeviceObjectArchive::GetShaderRegions(DeviceType DevType, DynamicLinearAllocator& Allocator) noexcept
{
    auto& RegionsInfo = m_ShaderRegions[static_cast<size_t>(DevType)];

    {
        std::unique_lock<std::mutex> Lock{RegionsInfo.Mtx};
        if (!RegionsInfo.Regions.empty())
            return RegionsInfo.Regions;
    }

    if (const auto ShaderData = GetDeviceSpecificData(DevType, m_ShadersHeader, Allocator, ChunkType::Shaders))
    {
        VERIFY_EXPR(ShaderData.Size() % sizeof(ArchiveRegion) == 0);
        const size_t Count = ShaderData.Size() / sizeof(ArchiveRegion);

        const auto* pSrcRegions = ShaderData.Ptr<const ArchiveRegion>();

        std::unique_lock<std::mutex> WriteLock{RegionsInfo.Mtx};
        RegionsInfo.Regions.reserve(Count);
        for (Uint32 i = 0; i < Count; ++i)
            RegionsInfo.Regions.emplace_back(pSrcRegions[i]);
    }

    return RegionsInfo.Regions;
}

SerializedData DeviceObjectArchive::GetDeviceSpecificData(DeviceType              DevType,
                                                          const DataHeaderBase&   Header,
                                                          DynamicLinearAllocator& Allocator,
                                                          ChunkType               ExpectedChunkType) noexcept
{
    const char*  ChunkName   = ChunkTypeToResName(ExpectedChunkType);
    const auto   BlockType   = GetBlockOffsetType(DevType);
    const Uint64 BaseOffset  = m_BaseOffsets[static_cast<size_t>(BlockType)];
    const auto   ArchiveSize = m_pArchive->GetSize();
    if (BaseOffset > ArchiveSize)
    {
        LOG_ERROR_MESSAGE(ChunkName, " chunk is not present in the archive");
        return {};
    }
    if (Header.GetSize(DevType) == 0)
    {
        LOG_ERROR_MESSAGE("Device-specific data is missing for ", ChunkName);
        return {};
    }
    if (BaseOffset + Header.GetEndOffset(DevType) > ArchiveSize)
    {
        LOG_ERROR_MESSAGE("Invalid offset in the archive for ", ChunkName);
        return {};
    }

    auto const  Size  = Header.GetSize(DevType);
    auto* const pData = Allocator.Allocate(Size, DataPtrAlign);
    if (!m_pArchive->Read(BaseOffset + Header.GetOffset(DevType), Size, pData))
    {
        LOG_ERROR_MESSAGE("Failed to read resource-specific data");
        return {};
    }

    return {pData, Size};
}


bool DeviceObjectArchive::Validate() const
{
#define VALIDATE_RES(...) \
    IsValid = false;      \
    LOG_INFO_MESSAGE(ResTypeName, " '", Res.first.GetStr(), "': ", __VA_ARGS__);

    std::vector<Uint8> Temp;
    bool               IsValid = true;

    const auto ValidateResource = [&](const NameToArchiveRegionMap::value_type& Res, const char* ResTypeName) //
    {
        Temp.clear();

        // ignore m_CommonData.Offset
        if (Res.second.Offset > m_CommonData.Size || Res.second.Offset + Res.second.Size > m_CommonData.Size)
        {
            VALIDATE_RES("common data in range [", Res.second.Offset, "; ", Res.second.Offset + Res.second.Size,
                         "] is out of common block size (", m_CommonData.Size, ")");
            return false;
        }

        Temp.resize(Res.second.Size);
        if (!m_CommonData.Read(Res.second.Offset, Temp.size(), Temp.data()))
        {
            VALIDATE_RES("failed to read data from archive");
            return false;
        }
        return true;
    };

    const auto ValidateResources = [&](const NameToArchiveRegionMap& ResMap, ChunkType chunkType, const char* ResTypeName) //
    {
        for (auto& Res : ResMap)
        {
            if (!ValidateResource(Res, ResTypeName))
                continue;

            DataHeaderBase Header{ChunkType::Undefined};
            if (Temp.size() < sizeof(Header))
            {
                VALIDATE_RES("resource data is too small to store header - archive corrupted");
                continue;
            }

            memcpy(&Header, Temp.data(), sizeof(Header));
            if (Header.Type != chunkType)
            {
                VALIDATE_RES("invalid chunk type");
                continue;
            }

            for (Uint32 dev = 0; dev < Header.DeviceSpecificDataSize.size(); ++dev)
            {
                const auto  Size   = Header.DeviceSpecificDataSize[dev];
                const auto  Offset = Header.DeviceSpecificDataOffset[dev];
                const auto& Block  = m_DeviceSpecific[dev];

                if (Size == 0 && Offset == DataHeaderBase::InvalidOffset)
                    continue;

                if (Block.IsValid())
                {
                    // ignore Block.Offset
                    if (Offset > Block.Size || Offset + Size > Block.Size)
                    {
                        VALIDATE_RES(ArchiveDeviceTypeToString(dev), " specific data is out of block size (", Block.Size, ")");
                        continue;
                    }
                }
                else
                {
                    VALIDATE_RES(ArchiveDeviceTypeToString(dev), " specific data block is not present, but resource requires that data");
                    continue;
                }
            }
        }
    };

    static_assert(static_cast<Uint32>(ChunkType::Count) == 9, "Please handle the new chunk type below");
    // clang-format off
    ValidateResources(m_ResMap.Sign,     ChunkType::ResourceSignature,        "ResourceSignature");
    ValidateResources(m_ResMap.GraphPSO, ChunkType::GraphicsPipelineStates,   "GraphicsPipelineState");
    ValidateResources(m_ResMap.CompPSO,  ChunkType::ComputePipelineStates,    "ComputePipelineState");
    ValidateResources(m_ResMap.RayTrPSO, ChunkType::RayTracingPipelineStates, "RayTracingPipelineState");
    ValidateResources(m_ResMap.TilePSO,  ChunkType::TilePipelineStates,       "TilePipelineState");
    // clang-format on

    // Validate render passes
    {
        const char* ResTypeName = "RenderPass";
        for (auto& Res : m_ResMap.RenderPass)
        {
            if (!ValidateResource(Res, ResTypeName))
                continue;

            RPDataHeader Header{ChunkType::RenderPass};
            if (Temp.size() < sizeof(Header))
            {
                VALIDATE_RES("resource data is too small to store header - archive corrupted");
                continue;
            }

            memcpy(&Header, Temp.data(), sizeof(Header));
            if (Header.Type != ChunkType::RenderPass)
            {
                VALIDATE_RES("invalid chunk type");
                continue;
            }
        }
    }

    // Validate shaders
    for (const auto& Chunk : m_Chunks)
    {
        if (Chunk.Type == ChunkType::Shaders)
        {
            ShadersDataHeader Header;
            VERIFY_EXPR(sizeof(Header) == Chunk.Size);

            if (m_CommonData.Read(Chunk.Offset, sizeof(Header), &Header))
            {
                if (Header.Type != ChunkType::Shaders)
                {
                    LOG_INFO_MESSAGE("Invalid shaders header");
                    IsValid = false;
                    break;
                }

                for (Uint32 dev = 0; dev < Header.DeviceSpecificDataSize.size(); ++dev)
                {
                    const auto  Size   = Header.DeviceSpecificDataSize[dev];
                    const auto  Offset = Header.DeviceSpecificDataOffset[dev];
                    const auto& Block  = m_DeviceSpecific[dev];

                    if (Size == 0 && Offset == DataHeaderBase::InvalidOffset)
                        continue;

                    if (Block.IsValid())
                    {
                        // ignore Block.Offset
                        if (Offset > Block.Size || Offset + Size > Block.Size)
                        {
                            LOG_INFO_MESSAGE(ArchiveDeviceTypeToString(dev), " specific data for shaders is out of block size (", Block.Size, ")");
                            IsValid = false;
                            continue;
                        }
                    }
                    else
                    {
                        LOG_INFO_MESSAGE(ArchiveDeviceTypeToString(dev), " specific data for shaders block is not present, but resource requires that data");
                        IsValid = false;
                        continue;
                    }
                }
            }
            break;
        }
    }

    return IsValid;
}

std::string DeviceObjectArchive::ToString() const
{
    std::vector<Uint8> Temp;
    String             Output           = "Archive content:\n";
    size_t             MaxDevNameLen    = 0;
    const char         CommonDataName[] = "Common";

    for (Uint32 i = 0, Cnt = static_cast<Uint32>(DeviceType::Count); i < Cnt; ++i)
    {
        MaxDevNameLen = std::max(MaxDevNameLen, strlen(ArchiveDeviceTypeToString(i)));
    }

    const auto LoadResource = [&](const NameToArchiveRegionMap::value_type& Res) //
    {
        Temp.clear();

        // ignore m_CommonData.Offset
        if (Res.second.Offset > m_CommonData.Size || Res.second.Offset + Res.second.Size > m_CommonData.Size)
            return false;

        Temp.resize(Res.second.Size);
        return m_CommonData.Read(Res.second.Offset, Temp.size(), Temp.data());
    };

    const auto PrintResources = [&](const NameToArchiveRegionMap& ResMap, const char* ResTypeName) //
    {
        if (ResMap.empty())
            return;

        Output += "------------------\n";
        Output += ResTypeName;
        Output += "\n";

        String Log;
        for (auto& Res : ResMap)
        {
            Log.clear();
            Log += "  ";
            Log += Res.first.GetStr();

            DataHeaderBase Header{ChunkType::Undefined};
            if (LoadResource(Res) &&
                Temp.size() >= sizeof(Header))
            {
                memcpy(&Header, Temp.data(), sizeof(Header));
                Log += '\n';

                // Common data & header
                {
                    Log += "    ";
                    Log += CommonDataName;
                    for (size_t i = strlen(CommonDataName); i < MaxDevNameLen; ++i)
                        Log += ' ';
                    Log += " - [" + std::to_string(Res.second.Offset) + "; " + std::to_string(Res.second.Offset + Res.second.Size) + "]\n";
                }

                for (Uint32 dev = 0; dev < Header.DeviceSpecificDataSize.size(); ++dev)
                {
                    const auto  Size    = Header.DeviceSpecificDataSize[dev];
                    const auto  Offset  = Header.DeviceSpecificDataOffset[dev];
                    const auto& Block   = m_DeviceSpecific[dev];
                    const char* DevName = ArchiveDeviceTypeToString(dev);

                    Log += "    ";
                    Log += DevName;
                    for (size_t i = strlen(DevName); i < MaxDevNameLen; ++i)
                        Log += ' ';

                    if (Size == 0 || Offset == DataHeaderBase::InvalidOffset || !Block.IsValid())
                        Log += " - none\n";
                    else
                        Log += " - [" + std::to_string(Offset) + "; " + std::to_string(Offset + Size) + "]\n";
                }

                Output += Log;
                continue;
            }

            Log += " - invalid data\n";
            Output += Log;
        }
    };

    // Print header
    {
        Output += "Header\n";
        Output += "  version: " + std::to_string(HeaderVersion) + "\n";
    }

    // Print chunks
    {
        const auto ChunkTypeToString = [](ChunkType Type) //
        {
            static_assert(static_cast<Uint32>(ChunkType::Count) == 9, "Please handle the new chunk type below");
            switch (Type)
            {
                // clang-format off
                case ChunkType::ArchiveDebugInfo:         return "ArchiveDebugInfo";
                case ChunkType::ResourceSignature:        return "ResourceSignature";
                case ChunkType::GraphicsPipelineStates:   return "GraphicsPipelineStates";
                case ChunkType::ComputePipelineStates:    return "ComputePipelineStates";
                case ChunkType::RayTracingPipelineStates: return "RayTracingPipelineStates";
                case ChunkType::TilePipelineStates:       return "TilePipelineStates";
                case ChunkType::RenderPass:               return "RenderPass";
                case ChunkType::Shaders:                  return "Shaders";
                default:                                  return "unknown";
                    // clang-format on
            }
        };

        Output += "------------------\nChunks\n";
        for (auto& Chunk : m_Chunks)
        {
            Output += String{"  "} + ChunkTypeToString(Chunk.Type) + ", range: [" + std::to_string(Chunk.Offset) + "; " + std::to_string(Chunk.Offset + Chunk.Size) + "]\n";
        }
    }

    // Print debug info
    {
        for (auto& Chunk : m_Chunks)
        {
            if (Chunk.Type == ChunkType::ArchiveDebugInfo)
            {
                Temp.resize(Chunk.Size);
                if (m_CommonData.Read(Chunk.Offset, Temp.size(), Temp.data()))
                {
                    Serializer<SerializerMode::Read> Ser{SerializedData{Temp.data(), Temp.size()}};

                    Uint32      APIVersion = 0;
                    const char* GitHash    = nullptr;
                    Ser(APIVersion, GitHash);

                    Output += "------------------\nDebug info";
                    Output += "\n  APIVersion: " + std::to_string(APIVersion);
                    Output += "\n  GitHash:    " + String{GitHash} + "\n";
                }
                break;
            }
        }
    }

    // Print archive blocks
    {
        Output += "------------------\nBlocks\n";
        Output += "  ";
        Output += CommonDataName;
        for (size_t i = strlen(CommonDataName); i < MaxDevNameLen; ++i)
            Output += ' ';
        Output += " - " + std::to_string(m_CommonData.Size) + " bytes\n";

        for (Uint32 dev = 0, Cnt = static_cast<Uint32>(DeviceType::Count); dev < Cnt; ++dev)
        {
            const auto& Block   = m_DeviceSpecific[dev];
            const char* DevName = ArchiveDeviceTypeToString(dev);

            Output += "  ";
            Output += DevName;
            for (size_t i = strlen(DevName); i < MaxDevNameLen; ++i)
                Output += ' ';

            if (!Block.IsValid())
                Output += " - none\n";
            else
                Output += " - " + std::to_string(Block.Size) + " bytes\n";
        }
    }

    // Print resources
    {
        static_assert(static_cast<Uint32>(ChunkType::Count) == 9, "Please handle the new chunk type below");
        // clang-format off
        PrintResources(m_ResMap.Sign,     "ResourceSignature");
        PrintResources(m_ResMap.GraphPSO, "GraphicsPipelineState");
        PrintResources(m_ResMap.CompPSO,  "ComputePipelineState");
        PrintResources(m_ResMap.RayTrPSO, "RayTracingPipelineState");
        PrintResources(m_ResMap.TilePSO,  "TilePipelineState");
        // clang-format on

        if (!m_ResMap.RenderPass.empty())
        {
            Output += "------------------\nRenderPass\n";

            String Log;
            for (auto& Res : m_ResMap.RenderPass)
            {
                Log.clear();
                Log += "  ";
                Log += Res.first.GetStr();

                if (LoadResource(Res))
                {
                    Log += '\n';

                    // Common data & header
                    {
                        Log += "    ";
                        Log += CommonDataName;
                        Log += " - [" + std::to_string(Res.second.Offset) + "; " + std::to_string(Res.second.Offset + Res.second.Size) + "]\n";
                    }
                }
                else
                    Log += " - invalid data\n";
                Output += Log;
            }
        }

        // Print shaders
        for (auto& Chunk : m_Chunks)
        {
            if (Chunk.Type == ChunkType::Shaders)
            {
                ShadersDataHeader Header;
                VERIFY_EXPR(sizeof(Header) == Chunk.Size);

                if (m_CommonData.Read(Chunk.Offset, sizeof(Header), &Header))
                {
                    Output += "------------------\nShaders\n";
                    for (Uint32 dev = 0; dev < Header.DeviceSpecificDataSize.size(); ++dev)
                    {
                        const auto  Size    = Header.DeviceSpecificDataSize[dev];
                        const auto  Offset  = Header.DeviceSpecificDataOffset[dev];
                        const auto& Block   = m_DeviceSpecific[dev];
                        const char* DevName = ArchiveDeviceTypeToString(dev);

                        Output += "  ";
                        Output += DevName;
                        for (size_t i = strlen(DevName); i < MaxDevNameLen; ++i)
                            Output += ' ';

                        if (Size == 0 || Offset == DataHeaderBase::InvalidOffset || !Block.IsValid())
                            Output += " - none\n";
                        else
                        {
                            Output += " - list range: [" + std::to_string(Offset) + "; " + std::to_string(Offset + Size) + "], count: " + std::to_string(Size / sizeof(ArchiveRegion));

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
                                Output += ", data range: [" + std::to_string(MinOffset) + "; " + std::to_string(MaxOffset) + "]";
                            }
                            Output += "\n";
                        }
                    }
                }
                break;
            }
        }
    }

    return Output;
}


void DeviceObjectArchive::RemoveDeviceData(DeviceType Dev) noexcept(false)
{
    m_DeviceSpecific[static_cast<size_t>(Dev)] = ArchiveBlock{};

    ArchiveBlock NewCommonBlock = m_CommonData;
    if (!NewCommonBlock.LoadToMemory())
        LOG_ERROR_AND_THROW("Failed to load common block");

    std::vector<Uint8> Temp;

    const auto UpdateResources = [&](const NameToArchiveRegionMap& ResMap, ChunkType chunkType) //
    {
        for (auto& Res : ResMap)
        {
            Temp.resize(Res.second.Size);
            if (!NewCommonBlock.Read(Res.second.Offset, Temp.size(), Temp.data()))
                continue;

            DataHeaderBase Header{ChunkType::Undefined};
            if (Temp.size() < sizeof(Header))
                continue;

            memcpy(&Header, Temp.data(), sizeof(Header));
            if (Header.Type != chunkType)
                continue;

            Header.DeviceSpecificDataSize[static_cast<size_t>(Dev)]   = 0;
            Header.DeviceSpecificDataOffset[static_cast<size_t>(Dev)] = DataHeaderBase::InvalidOffset;

            // Update header
            NewCommonBlock.Write(Res.second.Offset, sizeof(Header), &Header);
        }
    };

    // Remove device specific data offset
    static_assert(static_cast<Uint32>(ChunkType::Count) == 9, "Please handle the new chunk type below");
    // clang-format off
    UpdateResources(m_ResMap.Sign,     ChunkType::ResourceSignature);
    UpdateResources(m_ResMap.GraphPSO, ChunkType::GraphicsPipelineStates);
    UpdateResources(m_ResMap.CompPSO,  ChunkType::ComputePipelineStates);
    UpdateResources(m_ResMap.TilePSO,  ChunkType::TilePipelineStates);
    UpdateResources(m_ResMap.RayTrPSO, ChunkType::RayTracingPipelineStates);
    // clang-format on

    // Ignore render passes

    // Patch shader chunk
    for (auto& Chunk : m_Chunks)
    {
        if (Chunk.Type == ChunkType::Shaders)
        {
            ShadersDataHeader Header;
            VERIFY_EXPR(sizeof(Header) == Chunk.Size);

            if (NewCommonBlock.Read(Chunk.Offset, sizeof(Header), &Header))
            {
                VERIFY_EXPR(Header.Type == ChunkType::Shaders);

                Header.DeviceSpecificDataSize[static_cast<size_t>(Dev)]   = 0;
                Header.DeviceSpecificDataOffset[static_cast<size_t>(Dev)] = DataHeaderBase::InvalidOffset;

                // Update header
                NewCommonBlock.Write(Chunk.Offset, sizeof(Header), &Header);
            }
            break;
        }
    }

    m_CommonData = std::move(NewCommonBlock);

    VERIFY_EXPR(Validate());
}


void DeviceObjectArchive::AppendDeviceData(const DeviceObjectArchive& Src, DeviceType Dev) noexcept(false)
{
    if (!Src.m_CommonData.IsValid())
        LOG_ERROR_AND_THROW("Common data block is not present");

    if (!Src.m_DeviceSpecific[static_cast<size_t>(Dev)].IsValid())
        LOG_ERROR_AND_THROW("Can not append device specific block - block is not present");

    ArchiveBlock NewCommonBlock = m_CommonData;
    if (!NewCommonBlock.LoadToMemory())
        LOG_ERROR_AND_THROW("Failed to load common block in destination archive");

    const auto LoadResource = [](std::vector<Uint8>& Data, const NameToArchiveRegionMap::value_type& Res, const ArchiveBlock& Block) //
    {
        Data.clear();

        // ignore Block.Offset
        if (Res.second.Offset > Block.Size || Res.second.Offset + Res.second.Size > Block.Size)
            return false;

        Data.resize(Res.second.Size);
        return Block.Read(Res.second.Offset, Data.size(), Data.data());
    };

    std::vector<Uint8> TempSrc, TempDst;

    const auto CmpAndUpdateResources = [&](const NameToArchiveRegionMap& DstResMap, const NameToArchiveRegionMap& SrcResMap, ChunkType chunkType, const char* ResTypeName) //
    {
        if (DstResMap.size() != SrcResMap.size())
            LOG_ERROR_AND_THROW("Number of ", ResTypeName, " resources in source and destination archive does not match");

        for (auto& DstRes : DstResMap)
        {
            auto Iter = SrcResMap.find(DstRes.first);
            if (Iter == SrcResMap.end())
                LOG_ERROR_AND_THROW(ResTypeName, " '", DstRes.first.GetStr(), "' is not found");

            const auto& SrcRes = *Iter;
            if (!LoadResource(TempDst, DstRes, NewCommonBlock) || !LoadResource(TempSrc, SrcRes, Src.m_CommonData))
                LOG_ERROR_AND_THROW("Failed to load ", ResTypeName, " '", DstRes.first.GetStr(), "' common data");

            if (TempSrc.size() != TempDst.size())
                LOG_ERROR_AND_THROW(ResTypeName, " '", DstRes.first.GetStr(), "' common data size must match");

            DataHeaderBase SrcHeader{ChunkType::Undefined};
            DataHeaderBase DstHeader{ChunkType::Undefined};
            if (TempSrc.size() < sizeof(SrcHeader) || TempDst.size() < sizeof(DstHeader))
                LOG_ERROR_AND_THROW(ResTypeName, " '", DstRes.first.GetStr(), "' data size is too small to have header");

            if (memcmp(&TempSrc[sizeof(SrcHeader)], &TempDst[sizeof(DstHeader)], TempDst.size() - sizeof(DstHeader)) != 0)
                LOG_ERROR_AND_THROW(ResTypeName, " '", DstRes.first.GetStr(), "' common data must match");

            memcpy(&SrcHeader, TempSrc.data(), sizeof(SrcHeader));
            memcpy(&DstHeader, TempDst.data(), sizeof(DstHeader));

            if (SrcHeader.Type != chunkType || DstHeader.Type != chunkType)
                LOG_ERROR_AND_THROW(ResTypeName, " '", DstRes.first.GetStr(), "' header chunk type is invalid");

            const auto  SrcSize   = SrcHeader.DeviceSpecificDataSize[static_cast<Uint32>(Dev)];
            const auto  SrcOffset = SrcHeader.DeviceSpecificDataOffset[static_cast<Uint32>(Dev)];
            const auto& SrcBlock  = Src.m_DeviceSpecific[static_cast<Uint32>(Dev)];

            // ignore Block.Offset
            if (SrcOffset > SrcBlock.Size || SrcOffset + SrcSize > SrcBlock.Size)
                LOG_ERROR_AND_THROW("Source device specific data for ", ResTypeName, " '", DstRes.first.GetStr(), "' is out of block range");

            DstHeader.DeviceSpecificDataSize[static_cast<Uint32>(Dev)]   = SrcSize;
            DstHeader.DeviceSpecificDataOffset[static_cast<Uint32>(Dev)] = SrcOffset;

            // Update header
            NewCommonBlock.Write(DstRes.second.Offset, sizeof(DstHeader), &DstHeader);
        }
    };

    static_assert(static_cast<Uint32>(ChunkType::Count) == 9, "Please handle the new chunk type below");
    const auto& SrcResMap = Src.GetResourceMap();
    // clang-format off
    CmpAndUpdateResources(m_ResMap.Sign,     SrcResMap.Sign,     ChunkType::ResourceSignature,        "ResourceSignature");
    CmpAndUpdateResources(m_ResMap.GraphPSO, SrcResMap.GraphPSO, ChunkType::GraphicsPipelineStates,   "GraphicsPipelineState");
    CmpAndUpdateResources(m_ResMap.CompPSO,  SrcResMap.CompPSO,  ChunkType::ComputePipelineStates,    "ComputePipelineState");
    CmpAndUpdateResources(m_ResMap.TilePSO,  SrcResMap.TilePSO,  ChunkType::TilePipelineStates,       "TilePipelineState");
    CmpAndUpdateResources(m_ResMap.RayTrPSO, SrcResMap.RayTrPSO, ChunkType::RayTracingPipelineStates, "RayTracingPipelineState");
    // clang-format on

    // Compare render passes
    {
        if (m_ResMap.RenderPass.size() != SrcResMap.RenderPass.size())
            LOG_ERROR_AND_THROW("Number of RenderPass resources in source and destination archive does not match");

        for (auto& DstRes : m_ResMap.RenderPass)
        {
            auto Iter = SrcResMap.RenderPass.find(DstRes.first);
            if (Iter == SrcResMap.RenderPass.end())
                LOG_ERROR_AND_THROW("RenderPass '", DstRes.first.GetStr(), "' is not found");

            const auto& SrcRes = *Iter;
            if (!LoadResource(TempDst, DstRes, NewCommonBlock) || !LoadResource(TempSrc, SrcRes, Src.m_CommonData))
                LOG_ERROR_AND_THROW("Failed to load RenderPass '", DstRes.first.GetStr(), "' common data");

            if (TempSrc != TempDst)
                LOG_ERROR_AND_THROW("RenderPass '", DstRes.first.GetStr(), "' common data must match");
        }
    }

    // Update shader device specific offsets
    {
        const auto ReadShaderHeader = [](ShadersDataHeader& Header, Uint32& HeaderOffset, const std::vector<ChunkHeader>& Chunks, const ArchiveBlock& Block) //
        {
            HeaderOffset = 0;
            for (auto& Chunk : Chunks)
            {
                if (Chunk.Type == ChunkType::Shaders)
                {
                    if (sizeof(Header) != Chunk.Size)
                        LOG_ERROR_AND_THROW("Invalid chunk size for ShadersDataHeader");

                    if (!Block.Read(Chunk.Offset, sizeof(Header), &Header))
                        LOG_ERROR_AND_THROW("Failed to read ShadersDataHeader");

                    if (Header.Type != ChunkType::Shaders)
                        LOG_ERROR_AND_THROW("Invalid chunk type for ShadersDataHeader");

                    HeaderOffset = Chunk.Offset;
                    return true;
                }
            }
            return false;
        };

        ShadersDataHeader DstHeader;
        Uint32            DstHeaderOffset = 0;
        if (ReadShaderHeader(DstHeader, DstHeaderOffset, m_Chunks, m_CommonData))
        {
            ShadersDataHeader SrcHeader;
            Uint32            SrcHeaderOffset = 0;
            if (!ReadShaderHeader(SrcHeader, SrcHeaderOffset, Src.GetChunks(), Src.m_CommonData))
                LOG_ERROR_AND_THROW("Failed to find shaders in source archive");

            const auto  SrcSize   = SrcHeader.DeviceSpecificDataSize[static_cast<Uint32>(Dev)];
            const auto  SrcOffset = SrcHeader.DeviceSpecificDataOffset[static_cast<Uint32>(Dev)];
            const auto& SrcBlock  = Src.m_DeviceSpecific[static_cast<Uint32>(Dev)];

            // ignore Block.Offset
            if (SrcOffset > SrcBlock.Size || SrcOffset + SrcSize > SrcBlock.Size)
                LOG_ERROR_AND_THROW("Source device specific data for Shaders is out of block range");

            DstHeader.DeviceSpecificDataSize[static_cast<Uint32>(Dev)]   = SrcSize;
            DstHeader.DeviceSpecificDataOffset[static_cast<Uint32>(Dev)] = SrcOffset;

            // Update header
            NewCommonBlock.Write(DstHeaderOffset, sizeof(DstHeader), &DstHeader);
        }
    }

    m_CommonData = std::move(NewCommonBlock);

    m_DeviceSpecific[static_cast<Uint32>(Dev)] = Src.m_DeviceSpecific[static_cast<Uint32>(Dev)];

    VERIFY_EXPR(Validate());
}

void DeviceObjectArchive::Serialize(IFileStream* pStream) const noexcept(false)
{
    std::vector<Uint8> Temp;

    const auto CopyToStream = [&pStream, &Temp](const ArchiveBlock& Block, Uint32 Offset) //
    {
        Temp.resize(Block.Size - Offset);

        if (!Block.Read(Offset, Temp.size(), Temp.data()))
            LOG_ERROR_AND_THROW("Failed to read block from archive");

        if (!pStream->Write(Temp.data(), Temp.size()))
            LOG_ERROR_AND_THROW("Failed to store block");
    };

    ArchiveHeader Header;
    Header.MagicNumber = HeaderMagicNumber;
    Header.Version     = HeaderVersion;
    Header.NumChunks   = StaticCast<Uint32>(m_Chunks.size());

    size_t Offset = m_CommonData.Size;
    for (size_t dev = 0; dev < m_DeviceSpecific.size(); ++dev)
    {
        const auto& Block = m_DeviceSpecific[dev];

        if (Block.IsValid())
        {
            Header.BlockBaseOffsets[dev] = StaticCast<Uint32>(Offset);
            Offset += Block.Size;
        }
        else
            Header.BlockBaseOffsets[dev] = DataHeaderBase::InvalidOffset;
    }

    pStream->Write(&Header, sizeof(Header));
    CopyToStream(m_CommonData, sizeof(Header));

    for (size_t dev = 0; dev < m_DeviceSpecific.size(); ++dev)
    {
        const auto& Block = m_DeviceSpecific[dev];
        if (Block.IsValid())
        {
            const size_t Pos = pStream->GetSize();
            VERIFY_EXPR(Header.BlockBaseOffsets[dev] == Pos);
            CopyToStream(Block, 0);
        }
    }

    VERIFY_EXPR(Offset == pStream->GetSize());
}

} // namespace Diligent
