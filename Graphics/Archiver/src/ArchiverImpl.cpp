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

#include <bitset>

#include "ShaderToolsCommon.hpp"
#include "PipelineStateBase.hpp"

namespace Diligent
{

ArchiverImpl::ArchiverImpl(IReferenceCounters* pRefCounters, SerializationDeviceImpl* pDevice) :
    TBase{pRefCounters},
    m_pSerializationDevice{pDevice}
{}

ArchiverImpl::~ArchiverImpl()
{
}

template <typename MapType>
ArchiverImpl::TChunkData ArchiverImpl::InitNamedResourceArrayHeader(const MapType& Map,
                                                                    Uint32*&       DataSizeArray,
                                                                    Uint32*&       DataOffsetArray)
{
    ArchiverImpl::TChunkData ChunkData{GetRawAllocator()};

    VERIFY_EXPR(!Map.empty());

    const Uint32 Count = static_cast<Uint32>(Map.size());

    ChunkData.AddSpace<NamedResourceArrayHeader>();
    ChunkData.AddSpace<Uint32>(Count); // NameLength
    ChunkData.AddSpace<Uint32>(Count); // ***DataSize
    ChunkData.AddSpace<Uint32>(Count); // ***DataOffset

    for (const auto& NameAndData : Map)
        ChunkData.AddSpaceForString(NameAndData.first.GetStr());

    ChunkData.Reserve();

    auto& Header = *ChunkData.Construct<NamedResourceArrayHeader>();
    Header.Count = Count;

    auto* NameLengthArray = ChunkData.Allocate<Uint32>(Count);
    DataSizeArray         = ChunkData.Allocate<Uint32>(Count);
    DataOffsetArray       = ChunkData.ConstructArray<Uint32>(Count, 0u); // will be initialized later

    Uint32 i = 0;
    for (const auto& NameAndData : Map)
    {
        const auto* Name    = NameAndData.first.GetStr();
        const auto  NameLen = strlen(Name);

        auto* pStr = ChunkData.CopyString(Name, NameLen);
        (void)pStr;

        NameLengthArray[i] = StaticCast<Uint32>(NameLen + 1);
        DataSizeArray[i]   = StaticCast<Uint32>(NameAndData.second.GetSharedData().Size);
        ++i;
    }

    return ChunkData;
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

template <SerializerMode Mode>
void ArchiverImpl::SerializeDebugInfo(Serializer<Mode>& Ser) const
{
    Uint32 APIVersion = DILIGENT_API_VERSION;
    Ser(APIVersion);

    const char* GitHash = nullptr;
#ifdef DILIGENT_CORE_COMMIT_HASH
    GitHash = DILIGENT_CORE_COMMIT_HASH;
#endif
    Ser(GitHash);
}

void ArchiverImpl::ReserveSpace(size_t& SharedDataSize, std::array<size_t, DeviceDataCount>& PerDeviceDataSize) const
{
    // Reserve space for debug info
    {
        Serializer<SerializerMode::Measure> MeasureSer;
        SerializeDebugInfo(MeasureSer);
        SharedDataSize += MeasureSer.GetSize(nullptr);
    }

    // Reserve space for pipeline resource signatures
    for (const auto& PRS : m_PRSMap)
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
            if (Shaders.List.empty())
                continue;

            HasShaders = true;
            Dst += Shaders.List.size() * sizeof(FileOffsetAndSize);
            for (auto& Sh : Shaders.List)
            {
                Dst += Sh.Ptr->Size;
            }
        }
        if (HasShaders)
            SharedDataSize += sizeof(ShadersDataHeader);
    }

    // Reserve space for render passes
    for (const auto& RP : m_RPMap)
    {
        SharedDataSize += RP.second.GetSharedData().Size;
    }

    // Reserve space for pipelines
    const auto ReserveSpaceForPSO = [&SharedDataSize, &PerDeviceDataSize](auto& PSOMap) //
    {
        for (auto& PSO : PSOMap)
        {
            SharedDataSize += sizeof(PSODataHeader) + PSO.second.SharedData.Size;

            for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
            {
                auto&       Dst = PerDeviceDataSize[dev];
                const auto& Src = PSO.second.PerDeviceData[dev];
                Dst += Src.Size;
            }
        }
    };
    ReserveSpaceForPSO(m_GraphicsPSOMap);
    ReserveSpaceForPSO(m_ComputePSOMap);
    ReserveSpaceForPSO(m_TilePSOMap);
    ReserveSpaceForPSO(m_RayTracingPSOMap);

    static_assert(ChunkCount == 9, "Reserve space for new chunk type");
}

void ArchiverImpl::WriteDebugInfo(PendingData& Pending) const
{
    const auto ChunkInd = static_cast<Uint32>(ChunkType::ArchiveDebugInfo);
    auto&      Chunk    = Pending.ChunkData[ChunkInd];

    Serializer<SerializerMode::Measure> MeasureSer;
    SerializeDebugInfo(MeasureSer);

    VERIFY_EXPR(Chunk.IsEmpty());
    const auto Size = MeasureSer.GetSize(nullptr);
    if (Size == 0)
        return;

    Chunk = FixedLinearAllocator{GetRawAllocator()};
    Chunk.AddSpace(Size);
    Chunk.Reserve();
    Serializer<SerializerMode::Write> Ser{Chunk.Allocate(Size), Size};
    SerializeDebugInfo(Ser);
}

void ArchiverImpl::WriteResourceSignatureData(PendingData& Pending) const
{
    if (m_PRSMap.empty())
        return;

    const auto ChunkInd        = static_cast<Uint32>(ChunkType::ResourceSignature);
    auto&      DataOffsetArray = Pending.DataOffsetArrayPerChunk[ChunkInd];
    Uint32*    DataSizeArray   = nullptr;

    Pending.ChunkData[ChunkInd]             = InitNamedResourceArrayHeader(m_PRSMap, DataSizeArray, DataOffsetArray);
    Pending.ResourceCountPerChunk[ChunkInd] = StaticCast<Uint32>(m_PRSMap.size());

    Uint32 j = 0;
    for (const auto& PRS : m_PRSMap)
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
    auto&      DataOffsetArray = Pending.DataOffsetArrayPerChunk[ChunkInd];
    Uint32*    DataSizeArray   = nullptr;

    Pending.ChunkData[ChunkInd]             = InitNamedResourceArrayHeader(m_RPMap, DataSizeArray, DataOffsetArray);
    Pending.ResourceCountPerChunk[ChunkInd] = StaticCast<Uint32>(m_RPMap.size());

    Uint32 j = 0;
    for (const auto& RP : m_RPMap)
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

template <typename PSOType>
void ArchiverImpl::WritePSOData(PendingData& Pending, TNamedObjectHashMap<PSOType>& PSOMap, ChunkType PSOChunkType) const
{
    if (PSOMap.empty())
        return;

    const auto ChunkInd        = static_cast<Uint32>(PSOChunkType);
    auto&      DataOffsetArray = Pending.DataOffsetArrayPerChunk[ChunkInd];
    Uint32*    DataSizeArray   = nullptr;

    Pending.ChunkData[ChunkInd]             = InitNamedResourceArrayHeader(PSOMap, DataSizeArray, DataOffsetArray);
    Pending.ResourceCountPerChunk[ChunkInd] = StaticCast<Uint32>(PSOMap.size());

    Uint32 j = 0;
    for (auto& PSO : PSOMap)
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
            pHeader->Type = PSOChunkType;
            // DeviceSpecificDataSize & DeviceSpecificDataOffset will be initialized later
            pHeader->InitOffsets();

            DataOffsetArray[j] = StaticCast<Uint32>(Offset);
            Offset += sizeof(*pHeader);

            // Copy ***PipelineStateCreateInfo
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
            if (!m_Shaders[dev].List.empty())
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
        VERIFY_EXPR(Chunk.IsEmpty());
        Chunk = FixedLinearAllocator{GetRawAllocator()};
        Chunk.AddSpace<ShadersDataHeader>();
        Chunk.Reserve();

        auto* pHeader = Chunk.Construct<ShadersDataHeader>();
        pHeader->Type = ChunkType::Shaders;
        pHeader->InitOffsets();
        DataSizeArray   = pHeader->DeviceSpecificDataSize.data();
        DataOffsetArray = pHeader->DeviceSpecificDataOffset.data();

        Pending.ResourceCountPerChunk[ChunkInd] = DeviceDataCount;
    }

    for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
    {
        const auto& Shaders = m_Shaders[dev];
        auto&       Dst     = Pending.PerDeviceData[dev];

        if (Shaders.List.empty())
            continue;

        VERIFY(Dst.empty(), "Shaders must be written first");

        // write shared data
        FileOffsetAndSize* pOffsetAndSize = nullptr;
        {
            const auto Offset  = Dst.size();
            const auto Size    = Shaders.List.size() * sizeof(FileOffsetAndSize);
            const auto NewSize = Offset + Size;
            VERIFY_EXPR(NewSize <= Dst.capacity());
            Dst.resize(NewSize);
            pOffsetAndSize = reinterpret_cast<FileOffsetAndSize*>(&Dst[Offset]);
            VERIFY_EXPR(reinterpret_cast<size_t>(pOffsetAndSize) % alignof(FileOffsetAndSize) == 0);

            DataOffsetArray[dev] = StaticCast<Uint32>(Offset);
            DataSizeArray[dev]   = StaticCast<Uint32>(Size);
        }

        for (auto& Sh : Shaders.List)
        {
            const auto& Src     = *Sh.Ptr;
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
        NumChunks += (Chunk.IsEmpty() ? 0 : 1);
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
        if (ChunkData[i].IsEmpty())
            continue;

        CurrChunkPtr->Type   = static_cast<ChunkType>(i);
        CurrChunkPtr->Size   = StaticCast<Uint32>(ChunkData[i].GetCurrentSize());
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
        if (Chunk.IsEmpty())
            continue;

        pStream->Write(Chunk.GetDataPtr(), Chunk.GetCurrentSize());
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

    static_assert(ChunkCount == 9, "Write data for new chunk type");
    WriteDebugInfo(Pending);
    WriteShaderData(Pending);
    WriteResourceSignatureData(Pending);
    WriteRenderPassData(Pending);
    WritePSOData(Pending, m_GraphicsPSOMap, ChunkType::GraphicsPipelineStates);
    WritePSOData(Pending, m_ComputePSOMap, ChunkType::ComputePipelineStates);
    WritePSOData(Pending, m_TilePSOMap, ChunkType::TilePipelineStates);
    WritePSOData(Pending, m_RayTracingPSOMap, ChunkType::RayTracingPipelineStates);

    UpdateOffsetsInArchive(Pending);
    WritePendingDataToStream(Pending, pStream);

    return true;
}


const SerializedMemory& ArchiverImpl::PRSData::GetSharedData() const
{
    return pPRS->GetSharedSerializedMemory();
}

const SerializedMemory& ArchiverImpl::PRSData::GetDeviceData(Uint32 Idx) const
{
    const auto* pMem = pPRS->GetSerializedMemory(static_cast<DeviceType>(Idx));
    if (pMem != nullptr)
        return *pMem;

    static const SerializedMemory NullMem;
    return NullMem;
}

bool ArchiverImpl::AddPipelineResourceSignature(IPipelineResourceSignature* pPRS)
{
    DEV_CHECK_ERR(pPRS != nullptr, "pPRS must not be null");
    if (pPRS == nullptr)
        return false;

    auto* pPRSImpl        = ClassPtrCast<SerializableResourceSignatureImpl>(pPRS);
    auto  IterAndInserted = m_PRSMap.emplace(HashMapStringKey{pPRSImpl->GetDesc().Name, true}, PRSData{});

    if (!IterAndInserted.second)
    {
        if (IterAndInserted.first->second.pPRS != pPRSImpl)
        {
            LOG_ERROR_MESSAGE("Pipeline resource signature must have unique name");
            return false;
        }
        else
            return true;
    }

    m_PRSCache.insert(RefCntAutoPtr<SerializableResourceSignatureImpl>{pPRSImpl});

    IterAndInserted.first->second.pPRS = pPRSImpl;
    return true;
}

bool ArchiverImpl::CachePipelineResourceSignature(RefCntAutoPtr<IPipelineResourceSignature>& pPRS)
{
    auto* pPRSImpl        = pPRS.RawPtr<SerializableResourceSignatureImpl>();
    auto  IterAndInserted = m_PRSCache.insert(RefCntAutoPtr<SerializableResourceSignatureImpl>{pPRSImpl});

    // Found same PRS in cache
    if (!IterAndInserted.second)
    {
        pPRS     = *IterAndInserted.first;
        pPRSImpl = pPRS.RawPtr<SerializableResourceSignatureImpl>();

#ifdef DILIGENT_DEBUG
        auto Iter = m_PRSMap.find(pPRSImpl->GetDesc().Name);
        VERIFY_EXPR(Iter != m_PRSMap.end());
        VERIFY_EXPR(Iter->second.pPRS == pPRSImpl);
#endif
        return true;
    }

    return AddPipelineResourceSignature(pPRS);
}

Bool ArchiverImpl::AddPipelineResourceSignature(const PipelineResourceSignatureDesc& SignatureDesc,
                                                const ResourceSignatureArchiveInfo&  ArchiveInfo)
{
    RefCntAutoPtr<IPipelineResourceSignature> pPRS;
    m_pSerializationDevice->CreatePipelineResourceSignature(SignatureDesc, ArchiveInfo.DeviceFlags, &pPRS);
    if (!pPRS)
        return false;

    return AddPipelineResourceSignature(pPRS);
}

String ArchiverImpl::UniquePRSName()
{
    String       PRSName = "Default PRS - ";
    const size_t Pos     = PRSName.length();

    // AZ TODO: optimize (binary search?)
    for (Uint32 Index = 0; Index < 10000; ++Index)
    {
        PRSName.resize(Pos);
        PRSName += std::to_string(Index);

        if (m_PRSMap.find(PRSName.c_str()) == m_PRSMap.end())
            return PRSName;
    }
    return "";
}


const SerializedMemory& ArchiverImpl::RPData::GetSharedData() const
{
    return pRP->GetSharedSerializedMemory();
}

void ArchiverImpl::SerializeShaderBytecode(TShaderIndices& ShaderIndices, DeviceType DevType, const ShaderCreateInfo& CI, const void* Bytecode, size_t BytecodeSize)
{
    auto&                        Shaders         = m_Shaders[static_cast<Uint32>(DevType)];
    auto&                        RawMemAllocator = GetRawAllocator();
    const SHADER_SOURCE_LANGUAGE SourceLanguage  = SHADER_SOURCE_LANGUAGE_DEFAULT;
    const SHADER_COMPILER        ShaderCompiler  = SHADER_COMPILER_DEFAULT;

    Serializer<SerializerMode::Measure> MeasureSer;
    MeasureSer(CI.Desc.ShaderType, CI.EntryPoint, SourceLanguage, ShaderCompiler);

    const auto   Size   = MeasureSer.GetSize(nullptr) + BytecodeSize;
    void*        Ptr    = ALLOCATE_RAW(RawMemAllocator, "", Size);
    const Uint8* pBytes = static_cast<const Uint8*>(Bytecode);

    Serializer<SerializerMode::Write> Ser{Ptr, Size};
    Ser(CI.Desc.ShaderType, CI.EntryPoint, SourceLanguage, ShaderCompiler);

    for (size_t s = 0; s < BytecodeSize; ++s)
        Ser(pBytes[s]);

    VERIFY_EXPR(Ser.IsEnd());

    ShaderKey Key{std::make_shared<SerializedMemory>(Ptr, Size)};

    auto IterAndInserted = Shaders.Map.emplace(Key, Shaders.List.size());
    auto Iter            = IterAndInserted.first;
    if (IterAndInserted.second)
    {
        VERIFY_EXPR(Shaders.List.size() == Iter->second);
        Shaders.List.push_back(Key);
    }
    ShaderIndices.push_back(StaticCast<Uint32>(Iter->second));
}

void ArchiverImpl::SerializeShaderSource(TShaderIndices& ShaderIndices, DeviceType DevType, const ShaderCreateInfo& CI)
{
    auto& Shaders         = m_Shaders[static_cast<Uint32>(DevType)];
    auto& RawMemAllocator = GetRawAllocator();

    VERIFY_EXPR(CI.SourceLength > 0);

    String Source{CI.Source, CI.SourceLength};
    if (CI.Macros == nullptr)
    {
        DEV_CHECK_ERR(CI.SourceLanguage != SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM, "Shader macros are ignored when compiling GLSL verbatim in OpenGL backend");
        AppendShaderMacros(Source, CI.Macros);
    }

    Serializer<SerializerMode::Measure> MeasureSer;
    MeasureSer(CI.Desc.ShaderType, CI.EntryPoint, CI.SourceLanguage, CI.ShaderCompiler, CI.UseCombinedTextureSamplers, CI.CombinedSamplerSuffix);

    const auto   BytecodeSize = (Source.size() + 1) * sizeof(Source[0]);
    const auto   Size         = MeasureSer.GetSize(nullptr) + BytecodeSize;
    void*        Ptr          = ALLOCATE_RAW(RawMemAllocator, "", Size);
    const Uint8* pBytes       = reinterpret_cast<const Uint8*>(Source.c_str());

    Serializer<SerializerMode::Write> Ser{Ptr, Size};
    Ser(CI.Desc.ShaderType, CI.EntryPoint, CI.SourceLanguage, CI.ShaderCompiler, CI.UseCombinedTextureSamplers, CI.CombinedSamplerSuffix);

    for (size_t s = 0; s < BytecodeSize; ++s)
        Ser(pBytes[s]);

    VERIFY_EXPR(Ser.IsEnd());

    ShaderKey Key{std::make_shared<SerializedMemory>(Ptr, Size)};

    auto IterAndInserted = Shaders.Map.emplace(Key, Shaders.List.size());
    auto Iter            = IterAndInserted.first;
    if (IterAndInserted.second)
    {
        VERIFY_EXPR(Shaders.List.size() == Iter->second);
        Shaders.List.push_back(Key);
    }
    ShaderIndices.push_back(StaticCast<Uint32>(Iter->second));
}

void ArchiverImpl::SerializeShadersForPSO(const TShaderIndices& ShaderIndices, SerializedMemory& DeviceData) const
{
    auto& RawMemAllocator = GetRawAllocator();

    ShaderIndexArray Indices{ShaderIndices.data(), static_cast<Uint32>(ShaderIndices.size())};

    Serializer<SerializerMode::Measure> MeasureSer;
    PSOSerializer<SerializerMode::Measure>::SerializeShaders(MeasureSer, Indices, nullptr);

    const size_t SerSize = MeasureSer.GetSize(nullptr);
    void*        SerPtr  = ALLOCATE_RAW(RawMemAllocator, "", SerSize);

    Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
    PSOSerializer<SerializerMode::Write>::SerializeShaders(Ser, Indices, nullptr);
    VERIFY_EXPR(Ser.IsEnd());

    DeviceData = SerializedMemory{SerPtr, SerSize};
}

bool ArchiverImpl::AddRenderPass(IRenderPass* pRP)
{
    DEV_CHECK_ERR(pRP != nullptr, "pRP must not be null");
    if (pRP == nullptr)
        return false;

    auto* pRPImpl         = ClassPtrCast<SerializableRenderPassImpl>(pRP);
    auto  IterAndInserted = m_RPMap.emplace(HashMapStringKey{pRPImpl->GetDesc().Name, true}, RPData{});
    if (!IterAndInserted.second)
    {
        if (IterAndInserted.first->second.pRP != pRPImpl)
        {
            LOG_ERROR_MESSAGE("Render pass must have unique name");
            return false;
        }
        else
            return true;
    }

    IterAndInserted.first->second.pRP = pRPImpl;
    return true;
}

namespace
{

template <SerializerMode Mode>
void SerializerPSOImpl(Serializer<Mode>&                                 Ser,
                       const GraphicsPipelineStateCreateInfo&            PSOCreateInfo,
                       std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    const char* RPName = PSOCreateInfo.GraphicsPipeline.pRenderPass != nullptr ? PSOCreateInfo.GraphicsPipeline.pRenderPass->GetDesc().Name : "";
    PSOSerializer<Mode>::SerializeGraphicsPSO(Ser, PSOCreateInfo, PRSNames, RPName, nullptr);
}

template <SerializerMode Mode>
void SerializerPSOImpl(Serializer<Mode>&                                 Ser,
                       const ComputePipelineStateCreateInfo&             PSOCreateInfo,
                       std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    PSOSerializer<Mode>::SerializeComputePSO(Ser, PSOCreateInfo, PRSNames, nullptr);
}

template <SerializerMode Mode>
void SerializerPSOImpl(Serializer<Mode>&                                 Ser,
                       const TilePipelineStateCreateInfo&                PSOCreateInfo,
                       std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    PSOSerializer<Mode>::SerializeTilePSO(Ser, PSOCreateInfo, PRSNames, nullptr);
}

template <SerializerMode Mode>
void SerializerPSOImpl(Serializer<Mode>&                                 Ser,
                       const RayTracingPipelineStateCreateInfo&          PSOCreateInfo,
                       std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    PSOSerializer<Mode>::SerializeRayTracingPSO(Ser, PSOCreateInfo, PRSNames, nullptr);
}

#define LOG_PSO_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of PSO is invalid: ", ##__VA_ARGS__)
#define VERIFY_PSO(Expr, ...)                     \
    do                                            \
    {                                             \
        if (!(Expr))                              \
        {                                         \
            LOG_PSO_ERROR_AND_THROW(__VA_ARGS__); \
        }                                         \
    } while (false)

template <typename PRSMapType>
void ValidatePipelineStateArchiveInfo(const PipelineStateCreateInfo&  PSOCreateInfo,
                                      const PipelineStateArchiveInfo& ArchiveInfo,
                                      const PRSMapType&               PRSMap,
                                      const RENDER_DEVICE_TYPE_FLAGS  ValidDeviceFlags) noexcept(false)
{
    VERIFY_PSO(ArchiveInfo.DeviceFlags != RENDER_DEVICE_TYPE_FLAG_NONE, "At least one bit must be set in DeviceFlags");
    VERIFY_PSO((ArchiveInfo.DeviceFlags & ValidDeviceFlags) == ArchiveInfo.DeviceFlags, "DeviceFlags contain unsupported device type");

    VERIFY_PSO(PSOCreateInfo.PSODesc.Name != nullptr, "Pipeline name in PSOCreateInfo.PSODesc.Name must not be null");
    VERIFY_PSO((PSOCreateInfo.ResourceSignaturesCount != 0) == (PSOCreateInfo.ppResourceSignatures != nullptr),
               "ppResourceSignatures must not be null if ResourceSignaturesCount is not zero");

    std::bitset<MAX_RESOURCE_SIGNATURES> PRSExists{0};
    for (Uint32 i = 0; i < PSOCreateInfo.ResourceSignaturesCount; ++i)
    {
        VERIFY_PSO(PSOCreateInfo.ppResourceSignatures[i] != nullptr, "ppResourceSignatures[", i, "] must not be null");

        const auto& Desc = PSOCreateInfo.ppResourceSignatures[i]->GetDesc();
        VERIFY_EXPR(Desc.BindingIndex < PRSExists.size());

        VERIFY_PSO(!PRSExists[Desc.BindingIndex], "PRS binding index must be unique");
        PRSExists[Desc.BindingIndex] = true;
    }
}

} // namespace

template <typename CreateInfoType>
bool ArchiverImpl::SerializePSO(TNamedObjectHashMap<TPSOData<CreateInfoType>>& PSOMap,
                                const CreateInfoType&                          InPSOCreateInfo,
                                const PipelineStateArchiveInfo&                ArchiveInfo) noexcept
{
    CreateInfoType PSOCreateInfo = InPSOCreateInfo;
    try
    {
        ValidatePipelineStateArchiveInfo(PSOCreateInfo, ArchiveInfo, m_PRSMap, m_pSerializationDevice->GetValidDeviceFlags());
        ValidatePSOCreateInfo(m_pSerializationDevice->GetDevice(), PSOCreateInfo);
    }
    catch (...)
    {
        return false;
    }

    auto IterAndInserted = PSOMap.emplace(HashMapStringKey{PSOCreateInfo.PSODesc.Name, true}, TPSOData<CreateInfoType>{});
    if (!IterAndInserted.second)
    {
        LOG_ERROR_MESSAGE("Pipeline must have unique name");
        return false;
    }

    auto&      Data            = IterAndInserted.first->second;
    auto&      RawMemAllocator = GetRawAllocator();
    const bool UseDefaultPRS   = (PSOCreateInfo.ResourceSignaturesCount == 0);

    DefaultPRSInfo DefPRS;
    if (UseDefaultPRS)
    {
        DefPRS.DeviceFlags = ArchiveInfo.DeviceFlags;
        DefPRS.UniqueName  = UniquePRSName();
    }

    for (auto DeviceBits = ArchiveInfo.DeviceFlags; DeviceBits != 0;)
    {
        const auto Type = static_cast<RENDER_DEVICE_TYPE>(PlatformMisc::GetLSB(ExtractLSB(DeviceBits)));

        static_assert(RENDER_DEVICE_TYPE_COUNT == 7, "Please update the switch below to handle the new render device type");
        switch (Type)
        {
#if D3D11_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D11:
                if (!PatchShadersD3D11(PSOCreateInfo, Data, DefPRS))
                    return false;
                break;
#endif
#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
                if (!PatchShadersD3D12(PSOCreateInfo, Data, DefPRS))
                    return false;
                break;
#endif
#if GL_SUPPORTED || GLES_SUPPORTED
            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
                if (!PatchShadersGL(PSOCreateInfo, Data, DefPRS))
                    return false;
                break;
#endif
#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                if (!PatchShadersVk(PSOCreateInfo, Data, DefPRS))
                    return false;
                break;
#endif
#if METAL_SUPPORTED
            case RENDER_DEVICE_TYPE_METAL:
                if (!PatchShadersMtl(PSOCreateInfo, Data, DefPRS))
                    return false;
                break;
#endif
            case RENDER_DEVICE_TYPE_UNDEFINED:
            case RENDER_DEVICE_TYPE_COUNT:
            default:
                LOG_ERROR_MESSAGE("Unexpected render device type");
                break;
        }
        if (UseDefaultPRS)
        {
            PSOCreateInfo.ResourceSignaturesCount = 0;
            PSOCreateInfo.ppResourceSignatures    = nullptr;
            PSOCreateInfo.PSODesc.ResourceLayout  = InPSOCreateInfo.PSODesc.ResourceLayout;
        }
    }

    if (!Data.SharedData)
    {
        IPipelineResourceSignature* DefaultSignatures[1] = {};
        if (UseDefaultPRS)
        {
            DefaultSignatures[0]                  = DefPRS.pPRS;
            PSOCreateInfo.ResourceSignaturesCount = 1;
            PSOCreateInfo.ppResourceSignatures    = DefaultSignatures;
        }
        VERIFY_EXPR(PSOCreateInfo.ResourceSignaturesCount != 0);

        TPRSNames PRSNames = {};
        for (Uint32 i = 0; i < PSOCreateInfo.ResourceSignaturesCount; ++i)
        {
            if (!AddPipelineResourceSignature(PSOCreateInfo.ppResourceSignatures[i]))
                return false;
            PRSNames[i] = PSOCreateInfo.ppResourceSignatures[i]->GetDesc().Name;
        }

        Serializer<SerializerMode::Measure> MeasureSer;
        SerializerPSOImpl(MeasureSer, PSOCreateInfo, PRSNames);

        const size_t SerSize = MeasureSer.GetSize(nullptr);
        void*        SerPtr  = ALLOCATE_RAW(RawMemAllocator, "", SerSize);

        Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
        SerializerPSOImpl(Ser, PSOCreateInfo, PRSNames);
        VERIFY_EXPR(Ser.IsEnd());

        Data.SharedData = SerializedMemory{SerPtr, SerSize};
    }
    return true;
}

Bool ArchiverImpl::AddGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
                                            const PipelineStateArchiveInfo&        ArchiveInfo)
{
    if (PSOCreateInfo.GraphicsPipeline.pRenderPass != nullptr)
    {
        if (!AddRenderPass(PSOCreateInfo.GraphicsPipeline.pRenderPass))
            return false;
    }

    return SerializePSO(m_GraphicsPSOMap, PSOCreateInfo, ArchiveInfo);
}

Bool ArchiverImpl::AddComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo,
                                           const PipelineStateArchiveInfo&       ArchiveInfo)
{
    return SerializePSO(m_ComputePSOMap, PSOCreateInfo, ArchiveInfo);
}

Bool ArchiverImpl::AddRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
                                              const PipelineStateArchiveInfo&          ArchiveInfo)
{
    return SerializePSO(m_RayTracingPSOMap, PSOCreateInfo, ArchiveInfo);
}

Bool ArchiverImpl::AddTilePipelineState(const TilePipelineStateCreateInfo& PSOCreateInfo,
                                        const PipelineStateArchiveInfo&    ArchiveInfo)
{
    return SerializePSO(m_TilePSOMap, PSOCreateInfo, ArchiveInfo);
}

} // namespace Diligent
