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

#include "ArchiveRepacker.hpp"

#include <set>

namespace Diligent
{

ArchiveRepacker::ArchiveRepacker(IArchive* pArchive) :
    m_pArchive{std::make_unique<DeviceObjectArchive>(pArchive)}
{
    //Print();
}

void ArchiveRepacker::RemoveDeviceData(DeviceType Dev) noexcept(false)
{
    auto& CommonData     = m_pArchive->m_CommonData;
    auto& DeviceSpecific = m_pArchive->m_DeviceSpecific;

    DeviceSpecific[static_cast<size_t>(Dev)] = ArchiveBlock{};

    ArchiveBlock NewCommonBlock = CommonData;
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
            Header.DeviceSpecificDataOffset[static_cast<size_t>(Dev)] = InvalidOffset;

            // Update header
            NewCommonBlock.Write(Res.second.Offset, sizeof(Header), &Header);
        }
    };

    // Remove device specific data offset
    static_assert(static_cast<Uint32>(ChunkType::Count) == 9, "Please handle the new chunk type below");
    const auto& ResMap = m_pArchive->GetResourceMap();
    // clang-format off
    UpdateResources(ResMap.Sign,     ChunkType::ResourceSignature);
    UpdateResources(ResMap.GraphPSO, ChunkType::GraphicsPipelineStates);
    UpdateResources(ResMap.CompPSO,  ChunkType::ComputePipelineStates);
    UpdateResources(ResMap.TilePSO,  ChunkType::TilePipelineStates);
    UpdateResources(ResMap.RayTrPSO, ChunkType::RayTracingPipelineStates);
    // clang-format on

    // Ignore render passes

    // Patch shader chunk
    for (auto& Chunk : m_pArchive->GetChunks())
    {
        if (Chunk.Type == ChunkType::Shaders)
        {
            ShadersDataHeader Header;
            VERIFY_EXPR(sizeof(Header) == Chunk.Size);

            if (NewCommonBlock.Read(Chunk.Offset, sizeof(Header), &Header))
            {
                VERIFY_EXPR(Header.Type == ChunkType::Shaders);

                Header.DeviceSpecificDataSize[static_cast<size_t>(Dev)]   = 0;
                Header.DeviceSpecificDataOffset[static_cast<size_t>(Dev)] = InvalidOffset;

                // Update header
                NewCommonBlock.Write(Chunk.Offset, sizeof(Header), &Header);
            }
            break;
        }
    }

    CommonData = std::move(NewCommonBlock);

    VERIFY_EXPR(Validate());
}


void ArchiveRepacker::AppendDeviceData(const ArchiveRepacker& Src, DeviceType Dev) noexcept(false)
{
    auto& CommonData     = m_pArchive->m_CommonData;
    auto& DeviceSpecific = m_pArchive->m_DeviceSpecific;

    if (!Src.m_pArchive->m_CommonData.IsValid())
        LOG_ERROR_AND_THROW("Common data block is not present");

    if (!Src.m_pArchive->m_DeviceSpecific[static_cast<size_t>(Dev)].IsValid())
        LOG_ERROR_AND_THROW("Can not append device specific block - block is not present");

    ArchiveBlock NewCommonBlock = CommonData;
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
            if (!LoadResource(TempDst, DstRes, NewCommonBlock) || !LoadResource(TempSrc, SrcRes, Src.m_pArchive->m_CommonData))
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
            const auto& SrcBlock  = Src.m_pArchive->m_DeviceSpecific[static_cast<Uint32>(Dev)];

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
    const auto& SrcResMap = Src.m_pArchive->GetResourceMap();
    const auto& ResMap    = m_pArchive->GetResourceMap();
    // clang-format off
    CmpAndUpdateResources(ResMap.Sign,     SrcResMap.Sign,     ChunkType::ResourceSignature,        "ResourceSignature");
    CmpAndUpdateResources(ResMap.GraphPSO, SrcResMap.GraphPSO, ChunkType::GraphicsPipelineStates,   "GraphicsPipelineState");
    CmpAndUpdateResources(ResMap.CompPSO,  SrcResMap.CompPSO,  ChunkType::ComputePipelineStates,    "ComputePipelineState");
    CmpAndUpdateResources(ResMap.TilePSO,  SrcResMap.TilePSO,  ChunkType::TilePipelineStates,       "TilePipelineState");
    CmpAndUpdateResources(ResMap.RayTrPSO, SrcResMap.RayTrPSO, ChunkType::RayTracingPipelineStates, "RayTracingPipelineState");
    // clang-format on

    // Compare render passes
    {
        if (ResMap.RenderPass.size() != SrcResMap.RenderPass.size())
            LOG_ERROR_AND_THROW("Number of RenderPass resources in source and destination archive does not match");

        for (auto& DstRes : ResMap.RenderPass)
        {
            auto Iter = SrcResMap.RenderPass.find(DstRes.first);
            if (Iter == SrcResMap.RenderPass.end())
                LOG_ERROR_AND_THROW("RenderPass '", DstRes.first.GetStr(), "' is not found");

            const auto& SrcRes = *Iter;
            if (!LoadResource(TempDst, DstRes, NewCommonBlock) || !LoadResource(TempSrc, SrcRes, Src.m_pArchive->m_CommonData))
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
        if (ReadShaderHeader(DstHeader, DstHeaderOffset, m_pArchive->GetChunks(), CommonData))
        {
            ShadersDataHeader SrcHeader;
            Uint32            SrcHeaderOffset = 0;
            if (!ReadShaderHeader(SrcHeader, SrcHeaderOffset, Src.m_pArchive->GetChunks(), Src.m_pArchive->m_CommonData))
                LOG_ERROR_AND_THROW("Failed to find shaders in source archive");

            const auto  SrcSize   = SrcHeader.DeviceSpecificDataSize[static_cast<Uint32>(Dev)];
            const auto  SrcOffset = SrcHeader.DeviceSpecificDataOffset[static_cast<Uint32>(Dev)];
            const auto& SrcBlock  = Src.m_pArchive->m_DeviceSpecific[static_cast<Uint32>(Dev)];

            // ignore Block.Offset
            if (SrcOffset > SrcBlock.Size || SrcOffset + SrcSize > SrcBlock.Size)
                LOG_ERROR_AND_THROW("Source device specific data for Shaders is out of block range");

            DstHeader.DeviceSpecificDataSize[static_cast<Uint32>(Dev)]   = SrcSize;
            DstHeader.DeviceSpecificDataOffset[static_cast<Uint32>(Dev)] = SrcOffset;

            // Update header
            NewCommonBlock.Write(DstHeaderOffset, sizeof(DstHeader), &DstHeader);
        }
    }

    CommonData = std::move(NewCommonBlock);

    DeviceSpecific[static_cast<Uint32>(Dev)] = Src.m_pArchive->m_DeviceSpecific[static_cast<Uint32>(Dev)];

    VERIFY_EXPR(Validate());
}

void ArchiveRepacker::Serialize(IFileStream* pStream) noexcept(false)
{
    auto& CommonData     = m_pArchive->m_CommonData;
    auto& DeviceSpecific = m_pArchive->m_DeviceSpecific;

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
    Header.NumChunks   = StaticCast<Uint32>(m_pArchive->GetChunks().size());

    size_t Offset = CommonData.Size;
    for (size_t dev = 0; dev < DeviceSpecific.size(); ++dev)
    {
        const auto& Block = DeviceSpecific[dev];

        if (Block.IsValid())
        {
            Header.BlockBaseOffsets[dev] = StaticCast<Uint32>(Offset);
            Offset += Block.Size;
        }
        else
            Header.BlockBaseOffsets[dev] = InvalidOffset;
    }

    pStream->Write(&Header, sizeof(Header));
    CopyToStream(CommonData, sizeof(Header));

    for (size_t dev = 0; dev < DeviceSpecific.size(); ++dev)
    {
        const auto& Block = DeviceSpecific[dev];
        if (Block.IsValid())
        {
            const size_t Pos = pStream->GetSize();
            VERIFY_EXPR(Header.BlockBaseOffsets[dev] == Pos);
            CopyToStream(Block, 0);
        }
    }

    VERIFY_EXPR(Offset == pStream->GetSize());
}

bool ArchiveRepacker::Validate() const
{
    return m_pArchive->Validate();
}

void ArchiveRepacker::Print() const
{
    LOG_INFO_MESSAGE(m_pArchive->ToString());
}

} // namespace Diligent
