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

namespace Diligent
{


bool ArchiverImpl::ShaderKey::operator==(const ShaderKey& Rhs) const
{
    return Data.Size == Rhs.Data.Size &&
        std::memcmp(Data.Ptr, Rhs.Data.Ptr, Data.Size) == 0;
}

size_t ArchiverImpl::ShaderKeyHash::operator()(const ShaderKey& Key) const
{
    size_t Hash = 0;
    HashCombine(Hash, Key.Data.Size);

    if (Key.Data.Size % 4 == 0)
    {
        VERIFY_EXPR(reinterpret_cast<size_t>(Key.Data.Ptr) % 4 == 0);

        const Uint32* Ptr   = static_cast<Uint32*>(Key.Data.Ptr);
        const size_t  Count = Key.Data.Size / 4;

        for (Uint32 i = 0; i < Count; ++i)
            HashCombine(Hash, Ptr[i]);
    }
    else
    {
        const Uint8* Ptr = static_cast<Uint8*>(Key.Data.Ptr);
        for (Uint32 i = 0; i < Key.Data.Size; ++i)
            HashCombine(Hash, Ptr[i]);
    }
    return Hash;
}


ArchiverImpl::ArchiverImpl(IReferenceCounters* pRefCounters, SerializationDeviceImpl* pDevice) :
    TBase{pRefCounters},
    m_pSerializationDevice{pDevice}
{}

ArchiverImpl::~ArchiverImpl()
{
}

template <typename DataType>
void ArchiverImpl::InitNamedResourceArrayHeader(std::vector<Uint8>&                         ChunkData,
                                                const std::unordered_map<String, DataType>& Map,
                                                Uint32*&                                    DataSizeArray,
                                                Uint32*&                                    DataOffsetArray)
{
    VERIFY_EXPR(!Map.empty());

    const Uint32 Count = static_cast<Uint32>(Map.size());
    Uint32       Size  = sizeof(NamedResourceArrayHeader);
    Size += sizeof(Uint32) * Count; // NameLength
    Size += sizeof(Uint32) * Count; // ***DataSize
    Size += sizeof(Uint32) * Count; // ***DataOffset

    for (auto& NameAndData : Map)
    {
        Size += static_cast<Uint32>(NameAndData.first.size());
    }

    VERIFY_EXPR(ChunkData.empty());
    ChunkData.resize(Size);

    auto& Header = *reinterpret_cast<NamedResourceArrayHeader*>(&ChunkData[0]);
    Header.Count = Count;

    size_t OffsetInHeader  = sizeof(Header);
    auto*  NameLengthArray = reinterpret_cast<Uint32*>(&ChunkData[OffsetInHeader]);
    OffsetInHeader += sizeof(*NameLengthArray) * Header.Count;
    VERIFY_EXPR(reinterpret_cast<size_t>(NameLengthArray) % alignof(decltype(*NameLengthArray)) == 0);

    DataSizeArray = reinterpret_cast<Uint32*>(&ChunkData[OffsetInHeader]);
    OffsetInHeader += sizeof(*DataSizeArray) * Header.Count;
    VERIFY_EXPR(reinterpret_cast<size_t>(DataSizeArray) % alignof(decltype(*DataSizeArray)) == 0);

    DataOffsetArray = reinterpret_cast<Uint32*>(&ChunkData[OffsetInHeader]);
    OffsetInHeader += sizeof(*DataOffsetArray) * Header.Count;
    VERIFY_EXPR(reinterpret_cast<size_t>(DataOffsetArray) % alignof(decltype(*DataOffsetArray)) == 0);

    char* NameDataPtr = reinterpret_cast<char*>(&ChunkData[OffsetInHeader]);

    Uint32 i = 0;
    for (auto& NameAndData : Map)
    {
        const auto& Name = NameAndData.first;

        NameLengthArray[i] = static_cast<Uint32>(Name.size());
        DataSizeArray[i]   = StaticCast<Uint32>(NameAndData.second.GetSharedData().Size);
        DataOffsetArray[i] = 0; // will be initialized later
        std::memcpy(NameDataPtr, Name.c_str(), Name.size());
        NameDataPtr += Name.size();
        ++i;
    }

    VERIFY_EXPR(static_cast<void*>(NameDataPtr) == ChunkData.data() + ChunkData.size());
}

Bool ArchiverImpl::SerializeToBlob(IDataBlob** ppBlob)
{
    DEV_CHECK_ERR(ppBlob != nullptr, "ppBlob must not be null");
    if (ppBlob == nullptr)
        return false;

    *ppBlob = nullptr;

    RefCntAutoPtr<DataBlobImpl>     pDataBlob{MakeNewRCObj<DataBlobImpl>{}(0)};
    RefCntAutoPtr<MemoryFileStream> pMemStream{MakeNewRCObj<MemoryFileStream>{}(pDataBlob)};

    if (!SerializeToStream(pMemStream))
        return false;

    pDataBlob->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ppBlob));
    return true;
}

void ArchiverImpl::ReserveSpace(size_t& SharedDataSize, std::array<size_t, DeviceDataCount>& PerDeviceDataSize) const
{
    // Reserve space for pipeline resource signatures
    for (auto& PRS : m_PRSMap)
    {
        SharedDataSize += sizeof(PRSDataHeader) + PRS.second.GetSharedData().Size;

        for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
        {
            auto&       Dst = PerDeviceDataSize[dev];
            const auto& Src = PRS.second.GetDeviceData(dev);
            Dst += Src.Size;
        }
    }

    // Reserve space for shaders
    {
        bool HasShaders = false;
        for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
        {
            const auto& Shaders = m_Shaders[dev];
            auto&       Dst     = PerDeviceDataSize[dev];
            if (Shaders.Map.empty())
                continue;

            HasShaders = true;
            Dst += Shaders.Map.size() * sizeof(FileOffsetAndSize);
            for (auto& Sh : Shaders.Map)
            {
                Dst += Sh.first.Data.Size;
            }
        }
        if (HasShaders)
            SharedDataSize += sizeof(ShadersDataHeader);
    }

    // Reserve space for render passes
    for (auto& RP : m_RPMap)
    {
        SharedDataSize += RP.second.GetSharedData().Size;
    }

    // Reserve space for graphics pipelines
    for (auto& PSO : m_GraphicsPSOMap)
    {
        SharedDataSize += sizeof(PSODataHeader) + PSO.second.SharedData.Size;

        for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
        {
            auto&       Dst = PerDeviceDataSize[dev];
            const auto& Src = PSO.second.PerDeviceData[dev];
            Dst += Src.Size;
        }
    }

    static_assert(ChunkCount == 8, "Reserve space for new chunk type");
}

void ArchiverImpl::WriteResourceSignatureData(PendingData& Pending) const
{
    if (m_PRSMap.empty())
        return;

    const auto ChunkInd        = static_cast<Uint32>(ChunkType::ResourceSignature);
    auto&      Chunk           = Pending.ChunkData[ChunkInd];
    auto&      DataOffsetArray = Pending.DataOffsetArrayPerChunk[ChunkInd];
    Uint32*    DataSizeArray   = nullptr;
    InitNamedResourceArrayHeader(Chunk, m_PRSMap, DataSizeArray, DataOffsetArray);
    Pending.ResourceCountPerChunk[ChunkInd] = StaticCast<Uint32>(m_PRSMap.size());

    Uint32 j = 0;
    for (auto& PRS : m_PRSMap)
    {
        PRSDataHeader* pHeader = nullptr;

        // Write shared data
        {
            const auto& Src     = PRS.second.GetSharedData();
            auto&       Dst     = Pending.SharedData;
            auto        Offset  = Dst.size();
            const auto  NewSize = Offset + sizeof(*pHeader) + Src.Size;
            VERIFY_EXPR(NewSize <= Dst.capacity());
            Dst.resize(NewSize);

            pHeader       = reinterpret_cast<decltype(pHeader)>(&Dst[Offset]);
            pHeader->Type = ChunkType::ResourceSignature;
            // DeviceSpecificDataSize & DeviceSpecificDataOffset will be initialized later
            pHeader->InitOffsets();

            DataOffsetArray[j] = StaticCast<Uint32>(Offset);
            Offset += sizeof(*pHeader);

            // Copy PipelineResourceSignatureDesc & PipelineResourceSignatureSerializedData
            std::memcpy(&Dst[Offset], Src.Ptr, Src.Size);
        }

        for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
        {
            const auto& Src = PRS.second.GetDeviceData(dev);
            if (!Src)
                continue;

            auto&      Dst     = Pending.PerDeviceData[dev];
            const auto Offset  = Dst.size();
            const auto NewSize = Offset + Src.Size;
            VERIFY_EXPR(NewSize <= Dst.capacity());
            Dst.resize(NewSize);

            pHeader->SetSize(static_cast<DeviceType>(dev), StaticCast<Uint32>(Src.Size));
            pHeader->SetOffset(static_cast<DeviceType>(dev), StaticCast<Uint32>(Offset));
            std::memcpy(&Dst[Offset], Src.Ptr, Src.Size);
        }
        DataSizeArray[j] += sizeof(*pHeader);
        ++j;
    }
}

void ArchiverImpl::WriteRenderPassData(PendingData& Pending) const
{
    if (m_RPMap.empty())
        return;

    const auto ChunkInd        = static_cast<Uint32>(ChunkType::RenderPass);
    auto&      Chunk           = Pending.ChunkData[ChunkInd];
    auto&      DataOffsetArray = Pending.DataOffsetArrayPerChunk[ChunkInd];
    Uint32*    DataSizeArray   = nullptr;
    InitNamedResourceArrayHeader(Chunk, m_RPMap, DataSizeArray, DataOffsetArray);
    Pending.ResourceCountPerChunk[ChunkInd] = StaticCast<Uint32>(m_RPMap.size());

    Uint32 j = 0;
    for (auto& RP : m_RPMap)
    {
        RPDataHeader* pHeader = nullptr;

        // Write shared data
        {
            const auto& Src     = RP.second.GetSharedData();
            auto&       Dst     = Pending.SharedData;
            auto        Offset  = Dst.size();
            const auto  NewSize = Offset + sizeof(RPDataHeader) + Src.Size;
            VERIFY_EXPR(NewSize <= Dst.capacity());
            Dst.resize(NewSize);

            pHeader       = reinterpret_cast<RPDataHeader*>(&Dst[Offset]);
            pHeader->Type = ChunkType::RenderPass;

            DataOffsetArray[j] = StaticCast<Uint32>(Offset);
            Offset += sizeof(*pHeader);

            // Copy PipelineResourceSignatureDesc & PipelineResourceSignatureSerializedData
            std::memcpy(&Dst[Offset], Src.Ptr, Src.Size);
        }
        DataSizeArray[j] += sizeof(RPDataHeader);
        ++j;
    }
}

void ArchiverImpl::WriteGraphicsPSOData(PendingData& Pending) const
{
    if (m_GraphicsPSOMap.empty())
        return;

    const auto ChunkInd        = static_cast<Uint32>(ChunkType::GraphicsPipelineStates);
    auto&      Chunk           = Pending.ChunkData[ChunkInd];
    auto&      DataOffsetArray = Pending.DataOffsetArrayPerChunk[ChunkInd];
    Uint32*    DataSizeArray   = nullptr;
    InitNamedResourceArrayHeader(Chunk, m_GraphicsPSOMap, DataSizeArray, DataOffsetArray);
    Pending.ResourceCountPerChunk[ChunkInd] = StaticCast<Uint32>(m_GraphicsPSOMap.size());

    Uint32 j = 0;
    for (auto& PSO : m_GraphicsPSOMap)
    {
        PSODataHeader* pHeader = nullptr;

        // write shared data
        {
            const auto& Src     = PSO.second.SharedData;
            auto&       Dst     = Pending.SharedData;
            auto        Offset  = Dst.size();
            const auto  NewSize = Offset + sizeof(*pHeader) + Src.Size;
            VERIFY_EXPR(NewSize <= Dst.capacity());
            Dst.resize(NewSize);

            pHeader       = reinterpret_cast<decltype(pHeader)>(&Dst[Offset]);
            pHeader->Type = ChunkType::GraphicsPipelineStates;
            // DeviceSpecificDataSize & DeviceSpecificDataOffset will be initialized later
            pHeader->InitOffsets();

            DataOffsetArray[j] = StaticCast<Uint32>(Offset);
            Offset += sizeof(*pHeader);

            // Copy GraphicsPipelineStateCreateInfo
            std::memcpy(&Dst[Offset], Src.Ptr, Src.Size);
        }

        for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
        {
            const auto& Src = PSO.second.PerDeviceData[dev];
            if (!Src)
                continue;

            auto&      Dst     = Pending.PerDeviceData[dev];
            const auto Offset  = Dst.size();
            const auto NewSize = Offset + Src.Size;
            VERIFY_EXPR(NewSize <= Dst.capacity());
            Dst.resize(NewSize);

            pHeader->SetSize(static_cast<DeviceType>(dev), StaticCast<Uint32>(Src.Size));
            pHeader->SetOffset(static_cast<DeviceType>(dev), StaticCast<Uint32>(Offset));
            std::memcpy(&Dst[Offset], Src.Ptr, Src.Size);
        }
        DataSizeArray[j] += sizeof(*pHeader);
        ++j;
    }
}

void ArchiverImpl::WriteShaderData(PendingData& Pending) const
{
    {
        bool HasShaders = false;
        for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
        {
            if (!m_Shaders[dev].Map.empty())
                HasShaders = true;
        }
        if (!HasShaders)
            return;
    }

    const auto ChunkInd        = static_cast<Uint32>(ChunkType::Shaders);
    auto&      Chunk           = Pending.ChunkData[ChunkInd];
    Uint32*    DataOffsetArray = nullptr; // Pending.DataOffsetArrayPerChunk[ChunkInd];
    Uint32*    DataSizeArray   = nullptr;
    {
        VERIFY_EXPR(Chunk.empty());
        Chunk.resize(sizeof(ShadersDataHeader));

        auto* pHeader = reinterpret_cast<ShadersDataHeader*>(Chunk.data());
        pHeader->Type = ChunkType::Shaders;
        pHeader->InitOffsets();
        DataSizeArray   = pHeader->m_DeviceSpecificDataSize.data();
        DataOffsetArray = pHeader->m_DeviceSpecificDataOffset.data();

        Pending.ResourceCountPerChunk[ChunkInd] = DeviceDataCount;
    }

    for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
    {
        const auto& Shaders = m_Shaders[dev];
        auto&       Dst     = Pending.PerDeviceData[dev];

        if (Shaders.Map.empty())
            continue;

        VERIFY(Dst.empty(), "Shaders must be written first");

        // write shared data
        FileOffsetAndSize* pOffsetAndSize = nullptr;
        {
            const auto Offset  = Dst.size();
            const auto Size    = Shaders.Map.size() * sizeof(FileOffsetAndSize);
            const auto NewSize = Offset + Size;
            VERIFY_EXPR(NewSize <= Dst.capacity());
            Dst.resize(NewSize);
            pOffsetAndSize = reinterpret_cast<FileOffsetAndSize*>(&Dst[Offset]);
            VERIFY_EXPR(reinterpret_cast<size_t>(pOffsetAndSize) % alignof(FileOffsetAndSize) == 0);

            DataOffsetArray[dev] = StaticCast<Uint32>(Offset);
            DataSizeArray[dev]   = StaticCast<Uint32>(Size);
        }

        for (auto& Sh : Shaders.Map)
        {
            const auto& Src     = Sh.first.Data;
            const auto  Offset  = Dst.size();
            const auto  NewSize = Offset + Src.Size;
            VERIFY_EXPR(NewSize <= Dst.capacity());
            Dst.resize(NewSize);
            std::memcpy(&Dst[Offset], Src.Ptr, Src.Size);

            pOffsetAndSize->Offset = StaticCast<Uint32>(Offset);
            pOffsetAndSize->Size   = StaticCast<Uint32>(Src.Size);
            ++pOffsetAndSize;
        }
    }
}

void ArchiverImpl::UpdateOffsetsInArchive(PendingData& Pending) const
{
    auto& ChunkData    = Pending.ChunkData;
    auto& HeaderData   = Pending.HeaderData;
    auto& OffsetInFile = Pending.OffsetInFile;

    Uint32 NumChunks = 0;
    for (auto& Chunk : ChunkData)
    {
        NumChunks += (Chunk.empty() ? 0 : 1);
    }

    HeaderData.resize(sizeof(ArchiveHeader) + sizeof(ChunkHeader) * NumChunks);
    auto&       FileHeader = *reinterpret_cast<ArchiveHeader*>(&HeaderData[0]);
    auto* const ChunkPtr   = reinterpret_cast<ChunkHeader*>(&HeaderData[sizeof(ArchiveHeader)]);

    FileHeader.MagicNumber = DeviceObjectArchiveBase::HeaderMagicNumber;
    FileHeader.Version     = DeviceObjectArchiveBase::HeaderVersion;
    FileHeader.NumChunks   = NumChunks;

    // Update offsets to the NamedResourceArrayHeader
    OffsetInFile       = HeaderData.size();
    auto* CurrChunkPtr = ChunkPtr;
    for (Uint32 i = 0; i < ChunkData.size(); ++i)
    {
        if (ChunkData[i].empty())
            continue;

        CurrChunkPtr->Type   = static_cast<ChunkType>(i);
        CurrChunkPtr->Size   = StaticCast<Uint32>(ChunkData[i].size());
        CurrChunkPtr->Offset = StaticCast<Uint32>(OffsetInFile);

        OffsetInFile += CurrChunkPtr->Size;
        ++CurrChunkPtr;
    }

    // Shared data
    {
        for (Uint32 i = 0; i < NumChunks; ++i)
        {
            const auto& Chunk    = ChunkPtr[i];
            const auto  ChunkInd = static_cast<Uint32>(Chunk.Type);
            const auto  Count    = Pending.ResourceCountPerChunk[ChunkInd];

            for (Uint32 j = 0; j < Count; ++j)
            {
                // Update offsets to the ***DataHeader
                if (Pending.DataOffsetArrayPerChunk[ChunkInd] != nullptr)
                {
                    Uint32& Offset = Pending.DataOffsetArrayPerChunk[ChunkInd][j];
                    Offset         = (Offset == InvalidOffset ? InvalidOffset : StaticCast<Uint32>(Offset + OffsetInFile));
                }
            }
        }

        OffsetInFile += Pending.SharedData.size();
    }

    // Device specific data
    for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
    {
        if (Pending.PerDeviceData[dev].empty())
        {
            FileHeader.BlockBaseOffsets[dev] = InvalidOffset;
        }
        else
        {
            FileHeader.BlockBaseOffsets[dev] = StaticCast<Uint32>(OffsetInFile);
            OffsetInFile += Pending.PerDeviceData[dev].size();
        }
    }
}

void ArchiverImpl::WritePendingDataToStream(const PendingData& Pending, IFileStream* pStream) const
{
    const size_t InitialSize = pStream->GetSize();
    pStream->Write(Pending.HeaderData.data(), Pending.HeaderData.size());

    for (auto& Chunk : Pending.ChunkData)
    {
        if (Chunk.empty())
            continue;

        pStream->Write(Chunk.data(), Chunk.size());
    }

    pStream->Write(Pending.SharedData.data(), Pending.SharedData.size());

    for (auto& DevData : Pending.PerDeviceData)
    {
        if (DevData.empty())
            continue;

        pStream->Write(DevData.data(), DevData.size());
    }

    VERIFY_EXPR(InitialSize + pStream->GetSize() == Pending.OffsetInFile);
}

Bool ArchiverImpl::SerializeToStream(IFileStream* pStream)
{
    DEV_CHECK_ERR(pStream != nullptr, "pStream must not be null");
    if (pStream == nullptr)
        return false;

    PendingData Pending;

    // Reserve space
    {
        size_t                              SharedDataSIze  = 0;
        std::array<size_t, DeviceDataCount> ArchiveDataSize = {};

        ReserveSpace(SharedDataSIze, ArchiveDataSize);

        Pending.SharedData.reserve(SharedDataSIze);
        for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
            Pending.PerDeviceData[dev].reserve(ArchiveDataSize[dev]);
    }

    static_assert(ChunkCount == 8, "Write data for new chunk type");
    WriteShaderData(Pending);
    WriteResourceSignatureData(Pending);
    WriteRenderPassData(Pending);
    WriteGraphicsPSOData(Pending);

    UpdateOffsetsInArchive(Pending);
    WritePendingDataToStream(Pending, pStream);

    return true;
}

} // namespace Diligent
