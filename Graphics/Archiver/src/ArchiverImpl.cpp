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

#include "ArchiverImpl.hpp"
#include "Archiver_Inc.hpp"

#include <bitset>

#include "ShaderToolsCommon.hpp"
#include "PipelineStateBase.hpp"
#include "PSOSerializer.hpp"
#include "DataBlobImpl.hpp"
#include "MemoryFileStream.hpp"

namespace Diligent
{

DeviceObjectArchiveBase::DeviceType ArchiveDeviceDataFlagToArchiveDeviceType(ARCHIVE_DEVICE_DATA_FLAGS DataTypeFlag);

ArchiverImpl::ArchiverImpl(IReferenceCounters* pRefCounters, SerializationDeviceImpl* pDevice) :
    TBase{pRefCounters},
    m_pSerializationDevice{pDevice}
{}

ArchiverImpl::~ArchiverImpl()
{
}

template <typename MapType>
Uint32* ArchiverImpl::InitNamedResourceArrayHeader(ChunkType      Type,
                                                   const MapType& Map,
                                                   PendingData&   Pending)
{
    VERIFY_EXPR(!Map.empty());

    const auto ChunkInd = static_cast<size_t>(Type);

    auto& DataOffsetArray = Pending.DataOffsetArrayPerChunk[ChunkInd];
    auto& ChunkData       = Pending.ChunkData[ChunkInd];
    auto& Count           = Pending.ResourceCountPerChunk[ChunkInd];

    Count = StaticCast<Uint32>(Map.size());

    ChunkData = TDataElement{GetRawAllocator()};
    ChunkData.AddSpace<NamedResourceArrayHeader>();
    ChunkData.AddSpace<Uint32>(Count); // NameLength
    ChunkData.AddSpace<Uint32>(Count); // ***DataSize
    ChunkData.AddSpace<Uint32>(Count); // ***DataOffset

    for (const auto& NameAndData : Map)
        ChunkData.AddSpaceForString(NameAndData.first.GetStr());

    ChunkData.Reserve();

    auto& Header = *ChunkData.Construct<NamedResourceArrayHeader>(Count);
    VERIFY_EXPR(Header.Count == Count);

    auto* NameLengthArray = ChunkData.ConstructArray<Uint32>(Count);
    auto* DataSizeArray   = ChunkData.ConstructArray<Uint32>(Count);
    DataOffsetArray       = ChunkData.ConstructArray<Uint32>(Count); // will be initialized later

    Uint32 i = 0;
    for (const auto& NameAndData : Map)
    {
        const auto* Name    = NameAndData.first.GetStr();
        const auto  NameLen = strlen(Name);

        auto* pStr = ChunkData.CopyString(Name, NameLen);
        (void)pStr;

        NameLengthArray[i] = StaticCast<Uint32>(NameLen + 1);
        DataSizeArray[i]   = StaticCast<Uint32>(NameAndData.second.GetCommonData().Size());
        ++i;
    }

    return DataSizeArray;
}

Bool ArchiverImpl::SerializeToBlob(IDataBlob** ppBlob)
{
    DEV_CHECK_ERR(ppBlob != nullptr, "ppBlob must not be null");
    if (ppBlob == nullptr)
        return false;

    *ppBlob = nullptr;

    auto pDataBlob  = DataBlobImpl::Create(0);
    auto pMemStream = MemoryFileStream::Create(pDataBlob);

    if (!SerializeToStream(pMemStream))
        return false;

    pDataBlob->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ppBlob));
    return true;
}

void ArchiverImpl::ReserveSpace(PendingData& Pending) const
{
    auto& CommonData    = Pending.CommonData;
    auto& PerDeviceData = Pending.PerDeviceData;

    CommonData = TDataElement{GetRawAllocator()};
    for (auto& DeviceData : PerDeviceData)
        DeviceData = TDataElement{GetRawAllocator()};

    // Reserve space for shaders
    for (Uint32 type = 0; type < DeviceDataCount; ++type)
    {
        const auto& Shaders = m_Shaders[type];
        if (Shaders.List.empty())
            continue;

        auto& DeviceData = PerDeviceData[type];
        DeviceData.AddSpace<FileOffsetAndSize>(Shaders.List.size());
        for (const auto& Sh : Shaders.List)
        {
            DeviceData.AddSpace(Sh.Data->Size());
        }
    }

    // Reserve space for pipeline resource signatures
    for (const auto& PRS : m_PRSMap)
    {
        CommonData.AddSpace<PRSDataHeader>();
        CommonData.AddSpace(PRS.second.GetCommonData().Size());

        for (Uint32 type = 0; type < DeviceDataCount; ++type)
        {
            const auto& Src = PRS.second.GetDeviceData(static_cast<DeviceType>(type));
            PerDeviceData[type].AddSpace(Src.Size());
        }
    }

    // Reserve space for render passes
    for (const auto& RP : m_RPMap)
    {
        CommonData.AddSpace<RPDataHeader>();
        CommonData.AddSpace(RP.second.GetCommonData().Size());
    }

    // Reserve space for pipelines
    const auto ReserveSpaceForPSO = [&CommonData, &PerDeviceData](auto& PSOMap) //
    {
        for (const auto& PSO : PSOMap)
        {
            CommonData.AddSpace<PSODataHeader>();
            CommonData.AddSpace(PSO.second.CommonData.Size());

            for (Uint32 type = 0; type < DeviceDataCount; ++type)
            {
                const auto& Src = PSO.second.PerDeviceData[type];
                PerDeviceData[type].AddSpace(Src.Size());
            }
        }
    };
    ReserveSpaceForPSO(m_GraphicsPSOMap);
    ReserveSpaceForPSO(m_ComputePSOMap);
    ReserveSpaceForPSO(m_TilePSOMap);
    ReserveSpaceForPSO(m_RayTracingPSOMap);

    static_assert(ChunkCount == 9, "Reserve space for new chunk type");

    Pending.CommonData.Reserve();
    for (auto& DeviceData : Pending.PerDeviceData)
        DeviceData.Reserve();
}

void ArchiverImpl::WriteDebugInfo(PendingData& Pending) const
{
    auto& Chunk = Pending.ChunkData[static_cast<size_t>(ChunkType::ArchiveDebugInfo)];

    auto SerializeDebugInfo = [](auto& Ser) //
    {
        Uint32 APIVersion = DILIGENT_API_VERSION;
        Ser(APIVersion);

        const char* GitHash = nullptr;
#ifdef DILIGENT_CORE_COMMIT_HASH
        GitHash = DILIGENT_CORE_COMMIT_HASH;
#endif
        Ser(GitHash);
    };

    Serializer<SerializerMode::Measure> MeasureSer;
    SerializeDebugInfo(MeasureSer);

    VERIFY_EXPR(Chunk.IsEmpty());
    const auto Size = MeasureSer.GetSize();
    if (Size == 0)
        return;

    Chunk = TDataElement{GetRawAllocator()};
    Chunk.AddSpace(Size);
    Chunk.Reserve();
    Serializer<SerializerMode::Write> Ser{SerializedData{Chunk.Allocate(Size), Size}};
    SerializeDebugInfo(Ser);
}

template <typename HeaderType>
HeaderType* WriteHeader(ArchiverImpl::ChunkType     Type,
                        const SerializedData&       SrcData,
                        ArchiverImpl::TDataElement& DstChunk,
                        Uint32&                     DstOffset,
                        Uint32&                     DstArraySize)
{
    auto* pHeader = DstChunk.Construct<HeaderType>(Type);
    VERIFY_EXPR(pHeader->Type == Type);
    DstOffset = StaticCast<Uint32>(reinterpret_cast<const Uint8*>(pHeader) - DstChunk.GetDataPtr<const Uint8>());
    // DeviceSpecificDataSize & DeviceSpecificDataOffset will be initialized later

    DstChunk.Copy(SrcData.Ptr(), SrcData.Size());
    DstArraySize += sizeof(*pHeader);

    return pHeader;
}

template <typename HeaderType>
void WritePerDeviceData(HeaderType&                 Header,
                        ArchiverImpl::DeviceType    Type,
                        const SerializedData&       SrcData,
                        ArchiverImpl::TDataElement& DstChunk)
{
    if (!SrcData)
        return;

    auto* const pDst   = DstChunk.Copy(SrcData.Ptr(), SrcData.Size());
    const auto  Offset = reinterpret_cast<const Uint8*>(pDst) - DstChunk.GetDataPtr<const Uint8>();
    Header.SetSize(Type, StaticCast<Uint32>(SrcData.Size()));
    Header.SetOffset(Type, StaticCast<Uint32>(Offset));
}

template <typename DataHeaderType, typename MapType, typename WritePerDeviceDataType>
void ArchiverImpl::WriteDeviceObjectData(ChunkType Type, PendingData& Pending, MapType& ObjectMap, WritePerDeviceDataType WriteDeviceData) const
{
    if (ObjectMap.empty())
        return;

    auto* DataSizeArray   = InitNamedResourceArrayHeader(Type, ObjectMap, Pending);
    auto* DataOffsetArray = Pending.DataOffsetArrayPerChunk[static_cast<size_t>(Type)];

    Uint32 j = 0;
    for (auto& Obj : ObjectMap)
    {
        auto* pHeader = WriteHeader<DataHeaderType>(Type, Obj.second.GetCommonData(), Pending.CommonData,
                                                    DataOffsetArray[j], DataSizeArray[j]);

        for (Uint32 type = 0; type < DeviceDataCount; ++type)
        {
            WriteDeviceData(*pHeader, static_cast<DeviceType>(type), Obj.second);
        }
        ++j;
    }
}

void ArchiverImpl::WriteShaderData(PendingData& Pending) const
{
    {
        bool HasShaders = false;
        for (Uint32 type = 0; type < DeviceDataCount && !HasShaders; ++type)
            HasShaders = !m_Shaders[type].List.empty();
        if (!HasShaders)
            return;
    }

    const auto ChunkInd        = static_cast<Uint32>(ChunkType::Shaders);
    auto&      Chunk           = Pending.ChunkData[ChunkInd];
    Uint32*    DataOffsetArray = nullptr; // Pending.DataOffsetArrayPerChunk[ChunkInd];
    Uint32*    DataSizeArray   = nullptr;
    {
        VERIFY_EXPR(Chunk.IsEmpty());
        Chunk = TDataElement{GetRawAllocator()};
        Chunk.AddSpace<ShadersDataHeader>();
        Chunk.Reserve();

        auto& Header    = *Chunk.Construct<ShadersDataHeader>(ChunkType::Shaders);
        DataSizeArray   = Header.DeviceSpecificDataSize.data();
        DataOffsetArray = Header.DeviceSpecificDataOffset.data();

        Pending.ResourceCountPerChunk[ChunkInd] = DeviceDataCount;
    }

    for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
    {
        const auto& Shaders = m_Shaders[dev];
        auto&       Dst     = Pending.PerDeviceData[dev];

        if (Shaders.List.empty())
            continue;

        VERIFY(Dst.GetCurrentSize() == 0, "Shaders must be written first");

        // Write common data
        auto* pOffsetAndSize = Dst.ConstructArray<FileOffsetAndSize>(Shaders.List.size());
        DataOffsetArray[dev] = StaticCast<Uint32>(reinterpret_cast<const Uint8*>(pOffsetAndSize) - Dst.GetDataPtr<const Uint8>());
        DataSizeArray[dev]   = StaticCast<Uint32>(sizeof(FileOffsetAndSize) * Shaders.List.size());

        for (auto& Sh : Shaders.List)
        {
            const auto& Src  = *Sh.Data;
            auto* const pDst = Dst.Copy(Src.Ptr(), Src.Size());

            pOffsetAndSize->Offset = StaticCast<Uint32>(reinterpret_cast<const Uint8*>(pDst) - Dst.GetDataPtr<const Uint8>());
            pOffsetAndSize->Size   = StaticCast<Uint32>(Src.Size());
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

    HeaderData = TDataElement{GetRawAllocator()};
    HeaderData.AddSpace<ArchiveHeader>();
    HeaderData.AddSpace<ChunkHeader>(NumChunks);
    HeaderData.Reserve();
    auto&       FileHeader   = *HeaderData.Construct<ArchiveHeader>();
    auto* const ChunkHeaders = HeaderData.ConstructArray<ChunkHeader>(NumChunks);

    FileHeader.MagicNumber = DeviceObjectArchiveBase::HeaderMagicNumber;
    FileHeader.Version     = DeviceObjectArchiveBase::HeaderVersion;
    FileHeader.NumChunks   = NumChunks;

    // Update offsets to the NamedResourceArrayHeader
    OffsetInFile    = HeaderData.GetCurrentSize();
    size_t ChunkIdx = 0;
    for (Uint32 i = 0; i < ChunkData.size(); ++i)
    {
        if (ChunkData[i].IsEmpty())
            continue;

        auto& ChunkHdr  = ChunkHeaders[ChunkIdx++];
        ChunkHdr.Type   = static_cast<ChunkType>(i);
        ChunkHdr.Size   = StaticCast<Uint32>(ChunkData[i].GetCurrentSize());
        ChunkHdr.Offset = StaticCast<Uint32>(OffsetInFile);

        OffsetInFile += ChunkHdr.Size;
    }

    // Common data
    {
        for (Uint32 i = 0; i < NumChunks; ++i)
        {
            const auto& ChunkHdr = ChunkHeaders[i];
            VERIFY_EXPR(ChunkHdr.Size > 0);
            const auto ChunkInd = static_cast<Uint32>(ChunkHdr.Type);
            const auto Count    = Pending.ResourceCountPerChunk[ChunkInd];

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

        if (!Pending.CommonData.IsEmpty())
            OffsetInFile += Pending.CommonData.GetCurrentSize();
    }

    // Device specific data
    for (Uint32 dev = 0; dev < DeviceDataCount; ++dev)
    {
        if (Pending.PerDeviceData[dev].IsEmpty())
        {
            FileHeader.BlockBaseOffsets[dev] = InvalidOffset;
        }
        else
        {
            FileHeader.BlockBaseOffsets[dev] = StaticCast<Uint32>(OffsetInFile);
            OffsetInFile += Pending.PerDeviceData[dev].GetCurrentSize();
        }
    }
}

void ArchiverImpl::WritePendingDataToStream(const PendingData& Pending, IFileStream* pStream) const
{
    const size_t InitialSize = pStream->GetSize();
    pStream->Write(Pending.HeaderData.GetDataPtr(), Pending.HeaderData.GetCurrentSize());

    for (auto& Chunk : Pending.ChunkData)
    {
        if (Chunk.IsEmpty())
            continue;

        pStream->Write(Chunk.GetDataPtr(), Chunk.GetCurrentSize());
    }

    if (!Pending.CommonData.IsEmpty())
        pStream->Write(Pending.CommonData.GetDataPtr(), Pending.CommonData.GetCurrentSize());

    for (auto& DevData : Pending.PerDeviceData)
    {
        if (DevData.IsEmpty())
            continue;

        pStream->Write(DevData.GetDataPtr(), DevData.GetCurrentSize());
    }

    VERIFY_EXPR(InitialSize + pStream->GetSize() == Pending.OffsetInFile);
}

Bool ArchiverImpl::SerializeToStream(IFileStream* pStream)
{
    DEV_CHECK_ERR(pStream != nullptr, "pStream must not be null");
    if (pStream == nullptr)
        return false;

    PendingData Pending;
    ReserveSpace(Pending);
    WriteDebugInfo(Pending);
    WriteShaderData(Pending);

    auto WritePRSPerDeviceData = [&Pending](PRSDataHeader& Header, DeviceType Type, const PRSData& Src) //
    {
        WritePerDeviceData(Header, Type, Src.GetDeviceData(Type), Pending.PerDeviceData[static_cast<size_t>(Type)]);
    };
    WriteDeviceObjectData<PRSDataHeader>(ChunkType::ResourceSignature, Pending, m_PRSMap, WritePRSPerDeviceData);

    WriteDeviceObjectData<RPDataHeader>(ChunkType::RenderPass, Pending, m_RPMap, [](RPDataHeader& Header, DeviceType Type, const RPData&) {});

    auto WritePSOPerDeviceData = [&Pending](PSODataHeader& Header, DeviceType Type, const auto& Src) //
    {
        WritePerDeviceData(Header, Type, Src.PerDeviceData[static_cast<size_t>(Type)], Pending.PerDeviceData[static_cast<size_t>(Type)]);
    };
    WriteDeviceObjectData<PSODataHeader>(ChunkType::GraphicsPipelineStates, Pending, m_GraphicsPSOMap, WritePSOPerDeviceData);
    WriteDeviceObjectData<PSODataHeader>(ChunkType::ComputePipelineStates, Pending, m_ComputePSOMap, WritePSOPerDeviceData);
    WriteDeviceObjectData<PSODataHeader>(ChunkType::TilePipelineStates, Pending, m_TilePSOMap, WritePSOPerDeviceData);
    WriteDeviceObjectData<PSODataHeader>(ChunkType::RayTracingPipelineStates, Pending, m_RayTracingPSOMap, WritePSOPerDeviceData);
    static_assert(ChunkCount == 9, "Write data for new chunk type");

    UpdateOffsetsInArchive(Pending);
    WritePendingDataToStream(Pending, pStream);

    return true;
}


const SerializedData& ArchiverImpl::PRSData::GetCommonData() const
{
    return pPRS->GetCommonData();
}

const SerializedData& ArchiverImpl::PRSData::GetDeviceData(DeviceType Type) const
{
    const auto* pMem = pPRS->GetDeviceData(Type);
    if (pMem != nullptr)
        return *pMem;

    static const SerializedData NullData;
    return NullData;
}

bool ArchiverImpl::AddPipelineResourceSignature(IPipelineResourceSignature* pPRS)
{
    DEV_CHECK_ERR(pPRS != nullptr, "pPRS must not be null");
    if (pPRS == nullptr)
        return false;

    auto* const pPRSImpl = ClassPtrCast<SerializableResourceSignatureImpl>(pPRS);
    const auto* Name     = pPRSImpl->GetName();

    auto IterAndInserted = m_PRSMap.emplace(HashMapStringKey{Name, true}, PRSData{pPRSImpl});
    if (!IterAndInserted.second)
    {
        if (IterAndInserted.first->second.pPRS != pPRSImpl)
        {
            LOG_ERROR_MESSAGE("Pipeline resource signature with name '", Name, "' is already present in the archive. All signature names must be unique.");
            return false;
        }
        else
            return true;
    }

    m_PRSCache.insert(RefCntAutoPtr<SerializableResourceSignatureImpl>{pPRSImpl});

    return true;
}

bool ArchiverImpl::CachePipelineResourceSignature(RefCntAutoPtr<SerializableResourceSignatureImpl>& pPRS)
{
    VERIFY_EXPR(pPRS);
    auto IterAndInserted = m_PRSCache.insert(pPRS);

    // Found same PRS in the cache
    if (!IterAndInserted.second)
    {
        pPRS = *IterAndInserted.first;

#ifdef DILIGENT_DEBUG
        auto Iter = m_PRSMap.find(pPRS->GetName());
        VERIFY_EXPR(Iter != m_PRSMap.end());
        VERIFY_EXPR(Iter->second.pPRS == pPRS);
#endif
        return true;
    }

    return AddPipelineResourceSignature(pPRS);
}

String ArchiverImpl::GetDefaultPRSName(const char* PSOName) const
{
    VERIFY_EXPR(PSOName != nullptr);
    const String PRSName0 = String{"Default Signature of PSO '"} + PSOName + '\'';
    for (Uint32 Index = 0;; ++Index)
    {
        auto PRSName = Index == 0 ? PRSName0 : PRSName0 + std::to_string(Index);
        if (m_PRSMap.find(PRSName.c_str()) == m_PRSMap.end())
            return PRSName;
    }
}


const SerializedData& ArchiverImpl::RPData::GetCommonData() const
{
    return pRP->GetCommonData();
}

void ArchiverImpl::SerializeShaderBytecode(TShaderIndices&         ShaderIndices,
                                           DeviceType              DevType,
                                           const ShaderCreateInfo& CI,
                                           const void*             Bytecode,
                                           size_t                  BytecodeSize)
{
    auto& Shaders{m_Shaders[static_cast<Uint32>(DevType)]};

    constexpr SHADER_SOURCE_LANGUAGE SourceLanguage = SHADER_SOURCE_LANGUAGE_DEFAULT;
    constexpr SHADER_COMPILER        ShaderCompiler = SHADER_COMPILER_DEFAULT;

    Serializer<SerializerMode::Measure> MeasureSer;
    MeasureSer(CI.Desc.ShaderType, CI.EntryPoint, SourceLanguage, ShaderCompiler);

    const auto   Size   = MeasureSer.GetSize() + BytecodeSize;
    const Uint8* pBytes = static_cast<const Uint8*>(Bytecode);

    ShaderKey Key{std::make_shared<SerializedData>(Size, GetRawAllocator())};

    Serializer<SerializerMode::Write> Ser{*Key.Data};
    Ser(CI.Desc.ShaderType, CI.EntryPoint, SourceLanguage, ShaderCompiler);

    for (size_t s = 0; s < BytecodeSize; ++s)
        Ser(pBytes[s]);

    VERIFY_EXPR(Ser.IsEnded());


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
    auto& Shaders = m_Shaders[static_cast<Uint32>(DevType)];

    VERIFY_EXPR(CI.SourceLength > 0);

    String Source;
    if (CI.Macros != nullptr)
    {
        DEV_CHECK_ERR(CI.SourceLanguage != SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM, "Shader macros are ignored when compiling GLSL verbatim in OpenGL backend");
        AppendShaderMacros(Source, CI.Macros);
    }
    Source.append(CI.Source, CI.SourceLength);

    Serializer<SerializerMode::Measure> MeasureSer;
    MeasureSer(CI.Desc.ShaderType, CI.EntryPoint, CI.SourceLanguage, CI.ShaderCompiler, CI.UseCombinedTextureSamplers, CI.CombinedSamplerSuffix);

    const auto   BytecodeSize = (Source.size() + 1) * sizeof(Source[0]);
    const auto   Size         = MeasureSer.GetSize() + BytecodeSize;
    const Uint8* pBytes       = reinterpret_cast<const Uint8*>(Source.c_str());

    ShaderKey Key{std::make_shared<SerializedData>(Size, GetRawAllocator())};

    Serializer<SerializerMode::Write> Ser{*Key.Data};
    Ser(CI.Desc.ShaderType, CI.EntryPoint, CI.SourceLanguage, CI.ShaderCompiler, CI.UseCombinedTextureSamplers, CI.CombinedSamplerSuffix);

    for (size_t s = 0; s < BytecodeSize; ++s)
        Ser(pBytes[s]);

    VERIFY_EXPR(Ser.IsEnded());

    auto IterAndInserted = Shaders.Map.emplace(Key, Shaders.List.size());
    auto Iter            = IterAndInserted.first;
    if (IterAndInserted.second)
    {
        VERIFY_EXPR(Shaders.List.size() == Iter->second);
        Shaders.List.push_back(Key);
    }
    ShaderIndices.push_back(StaticCast<Uint32>(Iter->second));
}

SerializedData ArchiverImpl::SerializeShadersForPSO(const TShaderIndices& ShaderIndices) const
{
    ShaderIndexArray Indices{ShaderIndices.data(), static_cast<Uint32>(ShaderIndices.size())};

    Serializer<SerializerMode::Measure> MeasureSer;
    PSOSerializer<SerializerMode::Measure>::SerializeShaders(MeasureSer, Indices, nullptr);

    SerializedData DeviceData{MeasureSer.GetSize(), GetRawAllocator()};

    Serializer<SerializerMode::Write> Ser{DeviceData};
    PSOSerializer<SerializerMode::Write>::SerializeShaders(Ser, Indices, nullptr);
    VERIFY_EXPR(Ser.IsEnded());

    return DeviceData;
}

bool ArchiverImpl::AddRenderPass(IRenderPass* pRP)
{
    DEV_CHECK_ERR(pRP != nullptr, "pRP must not be null");
    if (pRP == nullptr)
        return false;

    auto* pRPImpl         = ClassPtrCast<SerializableRenderPassImpl>(pRP);
    auto  IterAndInserted = m_RPMap.emplace(HashMapStringKey{pRPImpl->GetDesc().Name, true}, RPData{pRPImpl});
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
    PSOSerializer<Mode>::SerializeCreateInfo(Ser, PSOCreateInfo, PRSNames, nullptr, RPName);
}

template <SerializerMode Mode>
void SerializerPSOImpl(Serializer<Mode>&                                 Ser,
                       const ComputePipelineStateCreateInfo&             PSOCreateInfo,
                       std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    PSOSerializer<Mode>::SerializeCreateInfo(Ser, PSOCreateInfo, PRSNames, nullptr);
}

template <SerializerMode Mode>
void SerializerPSOImpl(Serializer<Mode>&                                 Ser,
                       const TilePipelineStateCreateInfo&                PSOCreateInfo,
                       std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    PSOSerializer<Mode>::SerializeCreateInfo(Ser, PSOCreateInfo, PRSNames, nullptr);
}

template <SerializerMode Mode>
void SerializerPSOImpl(Serializer<Mode>&                                 Ser,
                       const RayTracingPipelineStateCreateInfo&          PSOCreateInfo,
                       std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    RayTracingShaderMap ShaderMapVk;
    RayTracingShaderMap ShaderMapD3D12;
#if VULKAN_SUPPORTED
    ExtractShadersVk(PSOCreateInfo, ShaderMapVk);
    VERIFY_EXPR(!ShaderMapVk.empty());
#endif
#if D3D12_SUPPORTED
    ExtractShadersD3D12(PSOCreateInfo, ShaderMapD3D12);
    VERIFY_EXPR(!ShaderMapD3D12.empty());
#endif

    VERIFY(ShaderMapVk.empty() || ShaderMapD3D12.empty() || ShaderMapVk == ShaderMapD3D12,
           "Ray tracing shader map must be same for Vulkan and Direct3D12 backends");

    RayTracingShaderMap ShaderMap;
    if (!ShaderMapVk.empty())
        std::swap(ShaderMap, ShaderMapVk);
    else if (!ShaderMapD3D12.empty())
        std::swap(ShaderMap, ShaderMapD3D12);
    else
        return;

    auto RemapShaders = [&ShaderMap](Uint32& outIndex, IShader* const& inShader) //
    {
        auto Iter = ShaderMap.find(inShader);
        if (Iter != ShaderMap.end())
            outIndex = Iter->second;
        else
            outIndex = ~0u;
    };
    PSOSerializer<Mode>::SerializeCreateInfo(Ser, PSOCreateInfo, PRSNames, nullptr, RemapShaders);
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
                                      const ARCHIVE_DEVICE_DATA_FLAGS ValidDeviceFlags) noexcept(false)
{
    VERIFY_PSO(ArchiveInfo.DeviceFlags != ARCHIVE_DEVICE_DATA_FLAG_NONE, "At least one bit must be set in DeviceFlags");
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

    auto& Data{IterAndInserted.first->second};
    Data.AuxData.NoShaderReflection = (ArchiveInfo.PSOFlags & PSO_ARCHIVE_FLAG_STRIP_REFLECTION) != 0;
    for (auto DeviceBits = ArchiveInfo.DeviceFlags; DeviceBits != 0;)
    {
        const auto Flag = ExtractLSB(DeviceBits);

        static_assert(ARCHIVE_DEVICE_DATA_FLAG_LAST == ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS, "Please update the switch below to handle the new data type");
        switch (Flag)
        {
#if D3D11_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_D3D11:
                if (!PatchShadersD3D11(PSOCreateInfo, Data))
                    return false;
                break;
#endif
#if D3D12_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_D3D12:
                if (!PatchShadersD3D12(PSOCreateInfo, Data))
                    return false;
                break;
#endif
#if GL_SUPPORTED || GLES_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_GL:
            case ARCHIVE_DEVICE_DATA_FLAG_GLES:
                if (!PatchShadersGL(PSOCreateInfo, Data))
                    return false;
                break;
#endif
#if VULKAN_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_VULKAN:
                if (!PatchShadersVk(PSOCreateInfo, Data))
                    return false;
                break;
#endif
#if METAL_SUPPORTED
            case ARCHIVE_DEVICE_DATA_FLAG_METAL_MACOS:
            case ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS:
                if (!PatchShadersMtl(PSOCreateInfo, Data, ArchiveDeviceDataFlagToArchiveDeviceType(Flag)))
                    return false;
                break;
#endif
            case ARCHIVE_DEVICE_DATA_FLAG_NONE:
                UNEXPECTED("ARCHIVE_DEVICE_DATA_FLAG_NONE (0) should never occur");
                break;

            default:
                LOG_ERROR_MESSAGE("Unexpected render device type");
                break;
        }
    }

    if (!Data.CommonData)
    {
        auto SignaturesCount = PSOCreateInfo.ResourceSignaturesCount;

        if (SignaturesCount == 0)
        {
#if GL_SUPPORTED || GLES_SUPPORTED
            if (ArchiveInfo.DeviceFlags & (ARCHIVE_DEVICE_DATA_FLAG_GL | ARCHIVE_DEVICE_DATA_FLAG_GLES))
            {
                // We must add empty device signature for OpenGL after all other devices are processed,
                // otherwise this empty description will be used as common signature description.
                if (!PrepareDefaultSignatureGL(PSOCreateInfo, Data))
                    return false;
            }
#endif
        }

        IPipelineResourceSignature* DefaultSignatures[1] = {};
        if (Data.pDefaultSignature)
        {
            DefaultSignatures[0]               = Data.pDefaultSignature;
            PSOCreateInfo.ppResourceSignatures = DefaultSignatures;
            SignaturesCount                    = 1;
        }

        TPRSNames PRSNames = {};
        for (Uint32 i = 0; i < SignaturesCount; ++i)
        {
            if (!AddPipelineResourceSignature(PSOCreateInfo.ppResourceSignatures[i]))
                return false;
            PRSNames[i] = PSOCreateInfo.ppResourceSignatures[i]->GetDesc().Name;
        }

        Serializer<SerializerMode::Measure> MeasureSer;
        SerializerPSOImpl(MeasureSer, PSOCreateInfo, PRSNames);
        PSOSerializer<SerializerMode::Measure>::SerializeAuxData(MeasureSer, Data.AuxData, nullptr);

        Data.CommonData = MeasureSer.AllocateData(GetRawAllocator());
        Serializer<SerializerMode::Write> Ser{Data.CommonData};
        SerializerPSOImpl(Ser, PSOCreateInfo, PRSNames);
        PSOSerializer<SerializerMode::Write>::SerializeAuxData(Ser, Data.AuxData, nullptr);

        VERIFY_EXPR(Ser.IsEnded());
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
