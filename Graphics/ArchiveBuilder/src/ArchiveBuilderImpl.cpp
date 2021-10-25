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

#include "ArchiveBuilderImpl.hpp"
#include "BasicMath.hpp"
#include "PlatformMisc.hpp"
#include "DataBlobImpl.hpp"
#include "MemoryFileStream.hpp"

#if VULKAN_SUPPORTED
#    include "VulkanUtilities/VulkanHeaders.h"
#    include "RenderDeviceVkImpl.hpp"
#    include "PipelineResourceSignatureVkImpl.hpp"
#    include "DeviceObjectArchiveVkImpl.hpp"
#endif
#if D3D12_SUPPORTED
#    include "../../GraphicsEngineD3D12/include/pch.h"
#    include "RenderDeviceD3D12Impl.hpp"
#    include "PipelineResourceSignatureD3D12Impl.hpp"
#    include "DeviceObjectArchiveD3D12Impl.hpp"
#endif

namespace Diligent
{
namespace
{

bool operator==(const PipelineResourceDesc& Lhs, const PipelineResourceDesc& Rhs)
{
    VERIFY_EXPR(Lhs.Name != nullptr || Rhs.Name != nullptr);

    // clang-format off
    return Lhs.ShaderStages == Rhs.ShaderStages &&
           Lhs.ArraySize    == Rhs.ArraySize    &&
           Lhs.ResourceType == Rhs.ResourceType &&
           Lhs.VarType      == Rhs.VarType      &&
           Lhs.Flags        == Rhs.Flags        &&
           std::strcmp(Lhs.Name, Rhs.Name) == 0;
    // clang-format on
}

bool operator==(const ImmutableSamplerDesc& Lhs, const ImmutableSamplerDesc& Rhs)
{
    VERIFY_EXPR(Lhs.SamplerOrTextureName != nullptr || Rhs.SamplerOrTextureName != nullptr);
    VERIFY_EXPR(Lhs.Desc.Name != nullptr || Rhs.Desc.Name != nullptr);

    // clang-format off
    return Lhs.ShaderStages == Rhs.ShaderStages           &&
           Lhs.Desc         == Rhs.Desc                   &&
           std::strcmp(Lhs.Desc.Name, Rhs.Desc.Name) == 0 &&
           std::strcmp(Lhs.SamplerOrTextureName, Rhs.SamplerOrTextureName) == 0;
    // clang-format on
}

bool operator==(const PipelineResourceSignatureDesc& Lhs, const PipelineResourceSignatureDesc& Rhs)
{
    // clang-format off
    if (Lhs.NumResources               != Rhs.NumResources         ||
        Lhs.NumImmutableSamplers       != Rhs.NumImmutableSamplers ||
        Lhs.BindingIndex               != Rhs.BindingIndex         ||
        Lhs.UseCombinedTextureSamplers != Rhs.UseCombinedTextureSamplers)
        return false;
    // clang-format on

    if (Lhs.UseCombinedTextureSamplers)
    {
        VERIFY_EXPR(Lhs.CombinedSamplerSuffix != nullptr || Rhs.CombinedSamplerSuffix != nullptr);
        if (std::strcmp(Lhs.CombinedSamplerSuffix, Rhs.CombinedSamplerSuffix) != 0)
            return false;
    }

    // ignore SRBAllocationGranularity

    for (Uint32 r = 0; r < Lhs.NumResources; ++r)
    {
        if (!(Lhs.Resources[r] == Rhs.Resources[r]))
            return false;
    }
    for (Uint32 s = 0; s < Lhs.NumImmutableSamplers; ++s)
    {
        if (!(Lhs.ImmutableSamplers[s] == Rhs.ImmutableSamplers[s]))
            return false;
    }
    return true;
}

bool operator==(const PipelineResourceSignatureSerializedData& Lhs, const PipelineResourceSignatureSerializedData& Rhs)
{
    // clang-format off
    return Lhs.ShaderStages          == Rhs.ShaderStages          &&
           Lhs.StaticResShaderStages == Rhs.StaticResShaderStages &&
           Lhs.PipelineType          == Rhs.PipelineType          &&
           Lhs.StaticResStageIndex   == Rhs.StaticResStageIndex;
    // clang-format on
}

template <SerializerMode Mode>
using SerializerImpl = DeviceObjectArchiveBase::SerializerImpl<Mode>;


template <typename TSerializedMem>
void CopyPipelineResourceSignatureDesc(const PipelineResourceSignatureDesc&           SrcDesc,
                                       const PipelineResourceSignatureSerializedData& SrcSerialized,
                                       PipelineResourceSignatureDesc*&                pDstDesc,
                                       PipelineResourceSignatureSerializedData*&      pDstSerialized,
                                       TSerializedMem&                                DescPtr,
                                       TSerializedMem&                                SerializedPtr)
{
    auto& RawMemAllocator = GetRawAllocator();

    // AZ TODO: duplicated code
    FixedLinearAllocator Alloc{RawMemAllocator};
    Alloc.AddSpace<PipelineResourceSignatureDesc>();
    Alloc.AddSpace<PipelineResourceDesc>(SrcDesc.NumResources);
    Alloc.AddSpace<ImmutableSamplerDesc>(SrcDesc.NumImmutableSamplers);

    for (Uint32 r = 0; r < SrcDesc.NumResources; ++r)
    {
        Alloc.AddSpaceForString(SrcDesc.Resources[r].Name);
    }
    for (Uint32 s = 0; s < SrcDesc.NumImmutableSamplers; ++s)
    {
        Alloc.AddSpaceForString(SrcDesc.ImmutableSamplers[s].SamplerOrTextureName);
        Alloc.AddSpaceForString(SrcDesc.ImmutableSamplers[s].Desc.Name);
    }

    if (SrcDesc.UseCombinedTextureSamplers)
        Alloc.AddSpaceForString(SrcDesc.CombinedSamplerSuffix);

    Alloc.AddSpace<PipelineResourceSignatureSerializedData>();

    Alloc.Reserve();

    pDstDesc             = Alloc.Copy(SrcDesc);
    auto& DstDesc        = *pDstDesc;
    auto* pResources     = Alloc.CopyArray<PipelineResourceDesc>(SrcDesc.Resources, SrcDesc.NumResources);
    auto* pImtblSamplers = Alloc.CopyArray<ImmutableSamplerDesc>(SrcDesc.ImmutableSamplers, SrcDesc.NumImmutableSamplers);

    for (Uint32 r = 0; r < SrcDesc.NumResources; ++r)
    {
        pResources[r].Name = Alloc.CopyString(SrcDesc.Resources[r].Name);
    }
    for (Uint32 s = 0; s < SrcDesc.NumImmutableSamplers; ++s)
    {
        pImtblSamplers[s].SamplerOrTextureName = Alloc.CopyString(SrcDesc.ImmutableSamplers[s].SamplerOrTextureName);
        pImtblSamplers[s].Desc.Name            = Alloc.CopyString(SrcDesc.ImmutableSamplers[s].Desc.Name);
    }

    DstDesc.Name                  = "";
    DstDesc.Resources             = pResources;
    DstDesc.ImmutableSamplers     = pImtblSamplers;
    DstDesc.CombinedSamplerSuffix = SrcDesc.UseCombinedTextureSamplers ? Alloc.CopyString(SrcDesc.CombinedSamplerSuffix) : nullptr;

    pDstSerialized = Alloc.Copy(SrcSerialized);
    DescPtr        = TSerializedMem{Alloc.ReleaseOwnership(), Alloc.GetReservedSize()};

    Serializer<SerializerMode::Measure> MeasureSer;
    SerializerImpl<SerializerMode::Measure>::SerializePRS(MeasureSer, SrcDesc, SrcSerialized, nullptr);

    const size_t SerSize = MeasureSer.GetSize(nullptr);
    void*        SerPtr  = ALLOCATE_RAW(RawMemAllocator, "", SerSize);

    Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
    SerializerImpl<SerializerMode::Write>::SerializePRS(Ser, SrcDesc, SrcSerialized, nullptr);
    VERIFY_EXPR(Ser.IsEnd());

    SerializedPtr = TSerializedMem{SerPtr, SerSize};
}

#if VULKAN_SUPPORTED
template <typename TSerializedMem>
void CopyPRSSerializedDataVk(const PipelineResourceSignatureVkImpl::SerializedData& SrcSerialized, TSerializedMem& SerializedPtr)
{
    Serializer<SerializerMode::Measure> MeasureSer;
    DeviceObjectArchiveVkImpl::SerializerVkImpl<SerializerMode::Measure>::SerializePRS(MeasureSer, SrcSerialized, nullptr);

    const size_t SerSize = MeasureSer.GetSize(nullptr);
    void*        SerPtr  = ALLOCATE_RAW(GetRawAllocator(), "", SerSize);

    Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
    DeviceObjectArchiveVkImpl::SerializerVkImpl<SerializerMode::Write>::SerializePRS(Ser, SrcSerialized, nullptr);
    VERIFY_EXPR(Ser.IsEnd());

    SerializedPtr = TSerializedMem{SerPtr, SerSize};
}
#endif

#if D3D12_SUPPORTED
template <typename TSerializedMem>
void CopyPRSSerializedDataD3D12(const PipelineResourceSignatureD3D12Impl::SerializedData& SrcSerialized, TSerializedMem& SerializedPtr)
{
    Serializer<SerializerMode::Measure> MeasureSer;
    DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<SerializerMode::Measure>::SerializePRS(MeasureSer, SrcSerialized, nullptr);

    const size_t SerSize = MeasureSer.GetSize(nullptr);
    void*        SerPtr  = ALLOCATE_RAW(GetRawAllocator(), "", SerSize);

    Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
    DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<SerializerMode::Write>::SerializePRS(Ser, SrcSerialized, nullptr);
    VERIFY_EXPR(Ser.IsEnd());

    SerializedPtr = TSerializedMem{SerPtr, SerSize};
}
#endif

template <typename NamedResourceArrayHeader, typename DataType>
void InitNamedResourceArrayHeader(std::vector<Uint8>&                         ChunkData,
                                  const std::unordered_map<String, DataType>& Map,
                                  Uint32*&                                    DataSizeArray,
                                  Uint32*&                                    DataOffsetArray)
{
    const Uint32 Count = static_cast<Uint32>(Map.size());
    Uint32       Size  = sizeof(NamedResourceArrayHeader);
    Size += sizeof(Uint32) * Count; // NameLength
    Size += sizeof(Uint32) * Count; // PRSDataSize
    Size += sizeof(Uint32) * Count; // PRSDataOffset

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
        DataSizeArray[i]   = StaticCast<Uint32>(NameAndData.second.PerDeviceData[RENDER_DEVICE_TYPE_UNDEFINED].Size);
        DataOffsetArray[i] = 0; // will be initialized later
        std::memcpy(NameDataPtr, Name.c_str(), Name.size());
        NameDataPtr += Name.size();
        ++i;
    }

    VERIFY_EXPR(static_cast<void*>(NameDataPtr) == ChunkData.data() + ChunkData.size());
}

} // namespace


ArchiveBuilderImpl::TSerializedMem::~TSerializedMem()
{
    if (Ptr)
    {
        auto& RawMemAllocator = GetRawAllocator();
        RawMemAllocator.Free(Ptr);
    }
}

ArchiveBuilderImpl::TSerializedMem& ArchiveBuilderImpl::TSerializedMem::operator=(TSerializedMem&& Rhs)
{
    if (Ptr)
    {
        auto& RawMemAllocator = GetRawAllocator();
        RawMemAllocator.Free(Ptr);
    }

    Ptr  = Rhs.Ptr;
    Size = Rhs.Size;

    Rhs.Ptr  = nullptr;
    Rhs.Size = 0;
    return *this;
}


ArchiveBuilderImpl::ArchiveBuilderImpl(IReferenceCounters* pRefCounters) :
    TBase{pRefCounters}
{}

ArchiveBuilderImpl::~ArchiveBuilderImpl()
{
}

Bool ArchiveBuilderImpl::SerializeToBlob(IDataBlob** ppBlob)
{
    DEV_CHECK_ERR(ppBlob != nullptr, "ppBlob must not be null");
    if (ppBlob == nullptr)
        return false;

    *ppBlob = nullptr;

    RefCntAutoPtr<DataBlobImpl>     pDataBlob(MakeNewRCObj<DataBlobImpl>()(0));
    RefCntAutoPtr<MemoryFileStream> pMemStream(MakeNewRCObj<MemoryFileStream>()(pDataBlob));

    if (!SerializeToStream(pMemStream))
        return false;

    pDataBlob->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ppBlob));
    return true;
}

Bool ArchiveBuilderImpl::SerializeToStream(IFileStream* pStream)
{
    DEV_CHECK_ERR(pStream != nullptr, "pStream must not be null");
    if (pStream == nullptr)
        return false;

    std::vector<Uint8>                                               HeaderData;                            // ArchiveHeader, ChunkHeader[]
    std::array<std::vector<Uint8>, Uint32{ChunkType::Count}>         ChunkData;                             // NamedResourceArrayHeader
    std::array<std::vector<Uint8>, Uint32{RENDER_DEVICE_TYPE_COUNT}> ArchiveData;                           // ***DataHeader, device specific data
    std::array<Uint32*, Uint32{ChunkType::Count}>                    DataOffsetArrayPerChunk          = {}; // pointer to NamedResourceArrayHeader::DataOffset - offsets to ***DataHeader
    std::array<Uint32, Uint32{ChunkType::Count}>                     ResourceCountPerChunk            = {};
    std::array<std::vector<Uint32*>, Uint32{ChunkType::Count}>       DeviceSpecificDataOffsetPerChunk = {};

    // Reserve space
    {
        std::array<size_t, Uint32{RENDER_DEVICE_TYPE_COUNT}> ArchiveDataSize = {};

        // Reserve space for pipeline resource signatures
        for (auto& PRS : m_PRSMap)
        {
            for (Uint32 dev = 0; dev < RENDER_DEVICE_TYPE_COUNT; ++dev)
            {
                auto&       Dst = ArchiveDataSize[dev];
                const auto& Src = PRS.second.PerDeviceData[dev];
                Dst += (dev == 0 ? sizeof(PRSDataHeader) : 0) + Src.Size;
            }
        }

        for (Uint32 dev = 0; dev < RENDER_DEVICE_TYPE_COUNT; ++dev)
        {
            ArchiveData[dev].reserve(ArchiveDataSize[dev]);
        }
    }

    // Write pipeline resource signatures
    {
        const auto ChunkInd        = Uint32{ChunkType::ResourceSignature};
        auto&      Chunk           = ChunkData[ChunkInd];
        auto&      DataOffsetArray = DataOffsetArrayPerChunk[ChunkInd];
        Uint32*    DataSizeArray   = nullptr;
        InitNamedResourceArrayHeader<NamedResourceArrayHeader>(Chunk, m_PRSMap, DataSizeArray, DataOffsetArray);

        ResourceCountPerChunk[ChunkInd] = StaticCast<Uint32>(m_PRSMap.size());
        auto& DeviceSpecificDataOffset  = DeviceSpecificDataOffsetPerChunk[ChunkInd];
        DeviceSpecificDataOffset.resize(ResourceCountPerChunk[ChunkInd]);

        Uint32 j = 0;
        for (auto& PRS : m_PRSMap)
        {
            PRSDataHeader* pHeader = nullptr;

            // Write shared data
            {
                const auto& Src     = PRS.second.PerDeviceData[RENDER_DEVICE_TYPE_UNDEFINED];
                auto&       Dst     = ArchiveData[RENDER_DEVICE_TYPE_UNDEFINED];
                auto        Offset  = Dst.size();
                const auto  NewSize = Offset + sizeof(PRSDataHeader) + Src.Size;
                VERIFY_EXPR(NewSize <= Dst.capacity());
                Dst.resize(NewSize);

                pHeader       = reinterpret_cast<PRSDataHeader*>(&Dst[Offset]);
                pHeader->Type = ChunkType::ResourceSignature;
                // DeviceSpecificDataSize & DeviceSpecificDataOffset will be initialized later
                std::memset(pHeader->DeviceSpecificDataOffset, 0xFF, sizeof(pHeader->DeviceSpecificDataOffset));

                Offset += sizeof(*pHeader);

                // Copy PipelineResourceSignatureDesc & PipelineResourceSignatureSerializedData
                std::memcpy(&Dst[Offset], Src.Ptr, Src.Size);
            }

            for (Uint32 dev = 1; dev < RENDER_DEVICE_TYPE_COUNT; ++dev)
            {
                const auto& Src = PRS.second.PerDeviceData[dev];
                if (!Src)
                    continue;

                auto&      Dst     = ArchiveData[dev];
                const auto OldSize = Dst.size();
                const auto NewSize = OldSize + Src.Size;
                VERIFY_EXPR(NewSize <= Dst.capacity());
                Dst.resize(NewSize);

                pHeader->DeviceSpecificDataSize[dev]   = StaticCast<Uint32>(Src.Size);
                pHeader->DeviceSpecificDataOffset[dev] = StaticCast<Uint32>(OldSize);
                std::memcpy(&Dst[OldSize], Src.Ptr, Src.Size);
            }
            DataSizeArray[j] += sizeof(PRSDataHeader);
            DeviceSpecificDataOffset[j] = pHeader->DeviceSpecificDataOffset;
            ++j;
        }
    }

    // Update file offsets
    size_t OffsetInFile = 0;
    {
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

        for (Uint32 dev = 0; dev < RENDER_DEVICE_TYPE_COUNT; ++dev)
        {
            for (Uint32 i = 0; i < NumChunks; ++i)
            {
                const auto& Chunk    = ChunkPtr[i];
                const auto  ChunkInd = Uint32{Chunk.Type};
                const auto  Count    = ResourceCountPerChunk[ChunkInd];

                for (Uint32 j = 0; j < Count; ++j)
                {
                    Uint32* Offset = nullptr;
                    if (dev == 0)
                    {
                        // Update offsets to the ***DataHeader
                        Offset = &DataOffsetArrayPerChunk[ChunkInd][j];
                    }
                    else
                    {
                        // Update offsets to the device specific data
                        Offset = &DeviceSpecificDataOffsetPerChunk[ChunkInd][j][dev];
                    }
                    *Offset = *Offset == ~0u ? 0u : StaticCast<Uint32>(*Offset + OffsetInFile);
                }
            }
            OffsetInFile += ArchiveData[dev].size();
        }
    }

    // Write to stream
    {
        const size_t InitialSize = pStream->GetSize();
        pStream->Write(HeaderData.data(), HeaderData.size());

        for (auto& Chunk : ChunkData)
        {
            if (Chunk.empty())
                continue;

            pStream->Write(Chunk.data(), Chunk.size());
        }

        for (auto& DevData : ArchiveData)
        {
            if (DevData.empty())
                continue;

            pStream->Write(DevData.data(), DevData.size());
        }

        VERIFY_EXPR(InitialSize + pStream->GetSize() == OffsetInFile);
    }
    return true;
}

Bool ArchiveBuilderImpl::ArchiveGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
                                                      const PipelineStateArchiveInfo&        ArchiveInfo)
{
    // AZ TODO
    return false;
}

Bool ArchiveBuilderImpl::ArchiveComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo,
                                                     const PipelineStateArchiveInfo&       ArchiveInfo)
{
    // AZ TODO
    return false;
}

Bool ArchiveBuilderImpl::ArchiveRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
                                                        const PipelineStateArchiveInfo&          ArchiveInfo)
{
    // AZ TODO
    return false;
}

Bool ArchiveBuilderImpl::ArchivePipelineResourceSignature(const PipelineResourceSignatureDesc& SignatureDesc,
                                                          const ResourceSignatureArchiveInfo&  ArchiveInfo)
{
    DEV_CHECK_ERR(ArchiveInfo.DeviceBits != 0, "At least one bit must be set in DeviceBits");
    DEV_CHECK_ERR(SignatureDesc.Name != nullptr, "Name must not be null");

    auto IterAndInserted = m_PRSMap.emplace(String{SignatureDesc.Name}, PRSData{});
    if (!IterAndInserted.second)
    {
        LOG_ERROR_MESSAGE("Pipeline resource signature must have unique name");
        return false;
    }

    PRSData&   Data       = IterAndInserted.first->second;
    const auto AddPRSDesc = [&Data](const PipelineResourceSignatureDesc& Desc, const PipelineResourceSignatureSerializedData& Serialized) //
    {
        auto& DescData = Data.DescMem;
        if (DescData)
        {
            VERIFY_EXPR(Data.pDesc != nullptr);
            VERIFY_EXPR(Data.pSerialized != nullptr);

            if (!(*Data.pDesc == Desc) || !(*Data.pSerialized == Serialized))
            {
                LOG_ERROR_MESSAGE("Pipeline resource signature description is not the same for different backends");
                return false;
            }
            return true;
        }
        else
        {
            CopyPipelineResourceSignatureDesc(Desc, Serialized, Data.pDesc, Data.pSerialized, DescData, Data.PerDeviceData[RENDER_DEVICE_TYPE_UNDEFINED]);
            return true;
        }
    };

    for (Uint32 Bits = ArchiveInfo.DeviceBits; Bits != 0;)
    {
        const auto Type = static_cast<RENDER_DEVICE_TYPE>(PlatformMisc::GetLSB(ExtractLSB(Bits)));

        static_assert(RENDER_DEVICE_TYPE_COUNT == 7, "Please update the switch below to handle the new render device type");
        switch (Type)
        {
#if D3D11_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D11:
                // AZ TODO
                break;
#endif

#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
            {
                PipelineResourceSignatureD3D12Impl Temp{nullptr, nullptr, SignatureDesc, SHADER_TYPE_UNKNOWN, true};

                PipelineResourceSignatureD3D12Impl::SerializedData SerializedData;
                Temp.Serialize(SerializedData);

                if (!AddPRSDesc(Temp.GetDesc(), SerializedData.Base))
                    return false;

                CopyPRSSerializedDataD3D12(SerializedData, Data.PerDeviceData[Type]);
                break;
            }
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
                // AZ TODO
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
            {
                PipelineResourceSignatureVkImpl Temp{nullptr, nullptr, SignatureDesc, SHADER_TYPE_UNKNOWN, true};

                PipelineResourceSignatureVkImpl::SerializedData SerializedData;
                Temp.Serialize(SerializedData);

                if (!AddPRSDesc(Temp.GetDesc(), SerializedData.Base))
                    return false;

                CopyPRSSerializedDataVk(SerializedData, Data.PerDeviceData[Type]);
                break;
            }
#endif

#if METAL_SUPPORTED
            case RENDER_DEVICE_TYPE_METAL:
                // AZ TODO
                break;
#endif

            case RENDER_DEVICE_TYPE_UNDEFINED:
            case RENDER_DEVICE_TYPE_COUNT:
            default:
                LOG_ERROR_MESSAGE("Unexpected render device type");
                return false;
        }
    }

    return true;
}

} // namespace Diligent
