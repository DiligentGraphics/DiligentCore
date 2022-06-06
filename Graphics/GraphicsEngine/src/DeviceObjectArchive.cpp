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

#include "DebugUtilities.hpp"
#include "PSOSerializer.hpp"

namespace Diligent
{


void DeviceObjectArchive::ReadNamedResourceRegions(IArchive*               pArchive,
                                                   const ChunkHeader&      Chunk,
                                                   NameToArchiveRegionMap& NameToRegion) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::ResourceSignature ||
                Chunk.Type == ChunkType::GraphicsPipelineStates ||
                Chunk.Type == ChunkType::ComputePipelineStates ||
                Chunk.Type == ChunkType::RayTracingPipelineStates ||
                Chunk.Type == ChunkType::TilePipelineStates ||
                Chunk.Type == ChunkType::RenderPass);

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

void DeviceObjectArchive::ReadArchiveDebugInfo(IArchive* pArchive, const ChunkHeader& Chunk, ArchiveDebugInfo& DebugInfo) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::ArchiveDebugInfo);

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

void DeviceObjectArchive::ReadShadersHeader(IArchive* pArchive, const ChunkHeader& Chunk, ShadersDataHeader& ShadersHeader) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::Shaders);
    VERIFY_EXPR(Chunk.Size == sizeof(ShadersHeader));

    if (!pArchive->Read(Chunk.Offset, sizeof(ShadersHeader), &ShadersHeader))
    {
        LOG_ERROR_AND_THROW("Failed to read shaders data header from the archive");
    }
}

void DeviceObjectArchive::ReadArchiveIndex(IArchive* pArchive, ArchiveIndex& Index) noexcept(false)
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

        Index.BaseOffsets = Header.BlockBaseOffsets;
    }

    // Read chunks
    Index.Chunks.resize(Header.NumChunks);
    if (!pArchive->Read(sizeof(Header), sizeof(Index.Chunks[0]) * Index.Chunks.size(), Index.Chunks.data()))
    {
        LOG_ERROR_AND_THROW("Failed to read chunk headers");
    }

    std::bitset<static_cast<size_t>(ChunkType::Count)> ProcessedBits{};
    for (const auto& Chunk : Index.Chunks)
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
            case ChunkType::ArchiveDebugInfo:         ReadArchiveDebugInfo    (pArchive, Chunk, Index.DebugInfo);  break;
            case ChunkType::ResourceSignature:        ReadNamedResourceRegions(pArchive, Chunk, Index.Sign);       break;
            case ChunkType::GraphicsPipelineStates:   ReadNamedResourceRegions(pArchive, Chunk, Index.GraphPSO);   break;
            case ChunkType::ComputePipelineStates:    ReadNamedResourceRegions(pArchive, Chunk, Index.CompPSO);    break;
            case ChunkType::RayTracingPipelineStates: ReadNamedResourceRegions(pArchive, Chunk, Index.RayTrPSO);   break;
            case ChunkType::TilePipelineStates:       ReadNamedResourceRegions(pArchive, Chunk, Index.TilePSO);    break;
            case ChunkType::RenderPass:               ReadNamedResourceRegions(pArchive, Chunk, Index.RenderPass); break;
            case ChunkType::Shaders:                  ReadShadersHeader       (pArchive, Chunk, Index.Shaders);    break;
            // clang-format on
            default:
                LOG_ERROR_AND_THROW("Unknown chunk type (", static_cast<Uint32>(Chunk.Type), ")");
        }
    }
}

DeviceObjectArchive::DeviceObjectArchive(IReferenceCounters* pRefCounters, IArchive* pArchive) noexcept(false) :
    TObjectBase{pRefCounters},
    m_pArchive{pArchive}
{
    ReadArchiveIndex(pArchive, m_ArchiveIndex);
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


DeviceObjectArchive::ShaderDeviceInfo& DeviceObjectArchive::GetShaderDeviceInfo(DeviceType DevType, DynamicLinearAllocator& Allocator) noexcept(false)
{
    auto& ShaderInfo = m_ShaderInfo[static_cast<size_t>(DevType)];

    {
        std::unique_lock<std::mutex> Lock{ShaderInfo.Mtx};
        if (!ShaderInfo.Regions.empty())
            return ShaderInfo;
    }

    if (const auto ShaderData = GetDeviceSpecificData(DevType, m_ArchiveIndex.Shaders, Allocator, ChunkType::Shaders))
    {
        VERIFY_EXPR(ShaderData.Size() % sizeof(ArchiveRegion) == 0);
        const size_t Count = ShaderData.Size() / sizeof(ArchiveRegion);

        const auto* pSrcRegions = ShaderData.Ptr<const ArchiveRegion>();

        std::unique_lock<std::mutex> WriteLock{ShaderInfo.Mtx};
        ShaderInfo.Regions.reserve(Count);
        for (Uint32 i = 0; i < Count; ++i)
            ShaderInfo.Regions.emplace_back(pSrcRegions[i]);
        //ShaderInfo.Cache.resize(Count);
    }

    return ShaderInfo;
}

SerializedData DeviceObjectArchive::GetDeviceSpecificData(DeviceType              DevType,
                                                          const DataHeaderBase&   Header,
                                                          DynamicLinearAllocator& Allocator,
                                                          ChunkType               ExpectedChunkType)
{
    const char*  ChunkName   = ChunkTypeToResName(ExpectedChunkType);
    const auto   BlockType   = GetBlockOffsetType(DevType);
    const Uint64 BaseOffset  = m_ArchiveIndex.BaseOffsets[static_cast<size_t>(BlockType)];
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

} // namespace Diligent
