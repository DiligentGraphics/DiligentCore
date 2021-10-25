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

#include <bitset>
#include "DeviceObjectArchiveBase.hpp"
#include "DebugUtilities.hpp"

namespace Diligent
{

Bool DeviceObjectArchiveBase::Deserialize(IArchiveSource* pSource)
{
    Clear();

    DEV_CHECK_ERR(pSource != nullptr, "pSource must not be null");
    if (pSource == nullptr)
        return false;

    std::unique_lock<std::shared_mutex> WriteLock{m_Guard};

    m_pSource = pSource;

    // Read header
    ArchiveHeader Header{};
    {
        if (!m_pSource->Read(0, &Header, sizeof(Header)))
        {
            LOG_ERROR_MESSAGE("Failed to read archive header");
            return false;
        }
        if (Header.MagicNumber != HeaderMagicNumber)
        {
            LOG_ERROR_MESSAGE("Archive header magic number is incorrect");
            return false;
        }
        if (Header.Version != HeaderVersion)
        {
            LOG_ERROR_MESSAGE("Archive header version (", Header.Version, ") is not supported, expected (", HeaderVersion, ")");
            return false;
        }
    }

    // Read chunks
    std::vector<ChunkHeader> Chunks{Header.NumChunks};
    if (!m_pSource->Read(sizeof(Header), Chunks.data(), sizeof(Chunks[0]) * Chunks.size()))
    {
        LOG_ERROR_MESSAGE("Failed to read chunk headers");
        return false;
    }

    std::bitset<Uint32{ChunkType::Count}> ProcessedBits{};
    for (const auto& Chunk : Chunks)
    {
        if (ProcessedBits[Uint32{Chunk.Type}])
        {
            LOG_ERROR_MESSAGE("Multiple chunks with the same types are not allowed");
            return false;
        }
        ProcessedBits[Uint32{Chunk.Type}] = true;

        switch (Chunk.Type)
        {
            // clang-format off
            case ChunkType::ArchiveDebugInfo:         ReadArchiveDebugInfo(Chunk);                   break;
            case ChunkType::ResourceSignature:        ReadNamedResources(Chunk, m_PRSMap);           break;
            case ChunkType::GraphicsPipelineStates:   ReadNamedResources(Chunk, m_GraphicsPSOMap);   break;
            case ChunkType::ComputePipelineStates:    ReadNamedResources(Chunk, m_ComputePSOMap);    break;
            case ChunkType::RayTracingPipelineStates: ReadNamedResources(Chunk, m_RayTracingPSOMap); break;
            // clang-format on
            default:
                LOG_ERROR_MESSAGE("Unknown chunk type (", static_cast<Uint32>(Chunk.Type), ")");
                return false;
        }
    }

    return true;
}

void DeviceObjectArchiveBase::Clear()
{
    std::unique_lock<std::shared_mutex> WriteLock{m_Guard};

    m_DebugInfo = {};
    m_pSource   = {};
    m_PRSMap.clear();
    m_GraphicsPSOMap.clear();
    m_ComputePSOMap.clear();
    m_RayTracingPSOMap.clear();
}

void DeviceObjectArchiveBase::ReadArchiveDebugInfo(const ChunkHeader& Chunk)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::ArchiveDebugInfo);

    std::vector<Uint8> Data; // AZ TODO: optimize
    Data.resize(Chunk.Size);

    if (!m_pSource->Read(Chunk.Offset, Data.data(), Data.size()))
    {
        LOG_ERROR_MESSAGE("Failed to read archive debug info");
        return;
    }

    Serializer<SerializerMode::Read> Ser{Data.data(), Data.size()};

    const char* GitHash = nullptr;
    Ser(GitHash);

    VERIFY_EXPR(Ser.IsEnd());
    m_DebugInfo.GitHash = String{GitHash};
}

void DeviceObjectArchiveBase::ReadNamedResources(const ChunkHeader& Chunk, TNameOffsetMap& NameAndOffset)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::ResourceSignature ||
                Chunk.Type == ChunkType::GraphicsPipelineStates ||
                Chunk.Type == ChunkType::ComputePipelineStates ||
                Chunk.Type == ChunkType::RayTracingPipelineStates);

    std::vector<Uint8> Data; // AZ TODO: optimize
    Data.resize(Chunk.Size);

    if (!m_pSource->Read(Chunk.Offset, Data.data(), Data.size()))
    {
        LOG_ERROR_MESSAGE("Failed to read resource list from archive");
        return;
    }

    const auto& Header         = *reinterpret_cast<const NamedResourceArrayHeader*>(Data.data());
    size_t      OffsetInHeader = sizeof(Header);

    const auto* NameLengthArray = reinterpret_cast<const Uint32*>(&Data[OffsetInHeader]);
    OffsetInHeader += sizeof(*NameLengthArray) * Header.Count;
    VERIFY_EXPR(reinterpret_cast<size_t>(NameLengthArray) % alignof(decltype(*NameLengthArray)) == 0);

    const auto* DataSizeArray = reinterpret_cast<const Uint32*>(&Data[OffsetInHeader]);
    OffsetInHeader += sizeof(*DataSizeArray) * Header.Count;
    VERIFY_EXPR(reinterpret_cast<size_t>(DataSizeArray) % alignof(decltype(*DataSizeArray)) == 0);

    const auto* DataOffsetArray = reinterpret_cast<const Uint32*>(&Data[OffsetInHeader]);
    OffsetInHeader += sizeof(*DataOffsetArray) * Header.Count;
    VERIFY_EXPR(reinterpret_cast<size_t>(DataOffsetArray) % alignof(decltype(*DataOffsetArray)) == 0);

    const char* NameDataPtr = reinterpret_cast<char*>(&Data[OffsetInHeader]);

    // Read names
    Uint32 Offset = 0;
    for (Uint32 i = 0; i < Header.Count; ++i)
    {
        if (Offset + NameLengthArray[i] > Data.size())
        {
            LOG_ERROR_MESSAGE("Failed to read archive data");
            return;
        }

        if (DataOffsetArray[i] + DataSizeArray[i] >= m_pSource->GetSize())
        {
            LOG_ERROR_MESSAGE("Failed to read archive data");
            return;
        }

        bool Inserted = m_PRSMap.emplace(String{NameDataPtr + Offset, NameLengthArray[i]}, FileOffsetAndSize{DataOffsetArray[i], DataSizeArray[i]}).second;
        DEV_CHECK_ERR(Inserted, "Each name in the resource names array must be unique");
        Offset += NameLengthArray[i];
    }
}

bool DeviceObjectArchiveBase::ReadPRSData(const String& Name, PRSData& PRS)
{
    //std::shared_lock ReadLock{m_Guard};

    auto Iter = m_PRSMap.find(Name);
    if (Iter == m_PRSMap.end())
    {
        LOG_ERROR_MESSAGE("Resource signature '", Name, "' is not present in archive");
        return false;
    }

    const auto DataSize = Iter->second.Size;
    void*      pData    = PRS.Allocator.Allocate(DataSize, DataPtrAlign);
    if (!m_pSource->Read(Iter->second.Offset, pData, DataSize))
    {
        LOG_ERROR_MESSAGE("Failed to read PRS '", Name, "' data from archive");
        return false;
    }

    Serializer<SerializerMode::Read> Ser{pData, DataSize};

    PRS.pHeader = Ser.Cast<PRSDataHeader>();
    if (PRS.pHeader->Type != ChunkType::ResourceSignature)
    {
        LOG_ERROR_MESSAGE("Invalid PRS header in archive");
        return false;
    }

    PRS.Desc.Name = Iter->first.c_str();
    SerializerImpl<SerializerMode::Read>::SerializePRS(Ser, PRS.Desc, PRS.Serialized, &PRS.Allocator);

    VERIFY_EXPR(Ser.IsEnd());
    return true;
}

bool DeviceObjectArchiveBase::ReadGraphicsPSOData(const String& Name, PSOData<GraphicsPipelineStateCreateInfo>& PSO)
{
    //std::shared_lock ReadLock{m_Guard};

    auto Iter = m_GraphicsPSOMap.find(Name);
    if (Iter == m_GraphicsPSOMap.end())
    {
        LOG_ERROR_MESSAGE("Graphics pipeline '", Name, "' is not present in archive");
        return false;
    }

    const auto DataSize = Iter->second.Size;
    void*      pData    = PSO.Allocator.Allocate(DataSize, DataPtrAlign);
    if (!m_pSource->Read(Iter->second.Offset, pData, DataSize))
    {
        LOG_ERROR_MESSAGE("Failed to read graphics pipeline '", Name, "' data from archive");
        return false;
    }

    Serializer<SerializerMode::Read> Ser{pData, DataSize};

    PSO.pHeader = Ser.Cast<PSODataHeader>();
    if (PSO.pHeader->Type != ChunkType::GraphicsPipelineStates)
    {
        LOG_ERROR_MESSAGE("Invalid graphics pipeline header in archive");
        return false;
    }

    PSO.CreateInfo.PSODesc.Name = Iter->first.c_str();
    SerializerImpl<SerializerMode::Read>::SerializeGraphicsPSO(Ser, PSO.CreateInfo, &PSO.Allocator);

    VERIFY_EXPR(Ser.IsEnd());
    return true;
}

bool DeviceObjectArchiveBase::ReadComputePSOData(const String& Name, PSOData<ComputePipelineStateCreateInfo>& PSO)
{
    //std::shared_lock ReadLock{m_Guard};

    // AZ TODO

    return true;
}

bool DeviceObjectArchiveBase::ReadRayTracingPSOData(const String& Name, PSOData<RayTracingPipelineStateCreateInfo>& PSO)
{
    //std::shared_lock ReadLock{m_Guard};

    // AZ TODO: not needed yet

    return true;
}


template <SerializerMode Mode>
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializePRS(
    Serializer<Mode>&                               Ser,
    TQual<PipelineResourceSignatureDesc>&           Desc,
    TQual<PipelineResourceSignatureSerializedData>& Serialized,
    DynamicLinearAllocator*                         Allocator)
{
    // Read PipelineResourceSignatureDesc
    Ser(Desc.NumResources,
        Desc.NumImmutableSamplers,
        Desc.BindingIndex,
        Desc.UseCombinedTextureSamplers,
        Desc.CombinedSamplerSuffix);
    // skip Name
    // skip SRBAllocationGranularity

    auto* pResources = ArraySerializerHelper<Mode>::Create(Desc.Resources, Desc.NumResources, Allocator);
    for (Uint32 r = 0; r < Desc.NumResources; ++r)
    {
        auto& ResDesc = pResources[r];
        Ser(ResDesc.Name, // AZ TODO: global cache for names ?
            ResDesc.ShaderStages,
            ResDesc.ArraySize,
            ResDesc.ResourceType,
            ResDesc.VarType,
            ResDesc.Flags);
    }

    auto* pImmutableSamplers = ArraySerializerHelper<Mode>::Create(Desc.ImmutableSamplers, Desc.NumImmutableSamplers, Allocator);
    for (Uint32 s = 0; s < Desc.NumImmutableSamplers; ++s)
    {
        auto& SampDesc = pImmutableSamplers[s];
        Ser(SampDesc.SamplerOrTextureName, // AZ TODO: global cache for names ?
            SampDesc.ShaderStages,
            SampDesc.Desc.Name,
            SampDesc.Desc.MinFilter,
            SampDesc.Desc.MagFilter,
            SampDesc.Desc.MipFilter,
            SampDesc.Desc.AddressU,
            SampDesc.Desc.AddressV,
            SampDesc.Desc.AddressW,
            SampDesc.Desc.Flags,
            SampDesc.Desc.MipLODBias,
            SampDesc.Desc.MaxAnisotropy,
            SampDesc.Desc.ComparisonFunc,
            SampDesc.Desc.BorderColor,
            SampDesc.Desc.MinLOD,
            SampDesc.Desc.MaxLOD);
    }

    // Read PipelineResourceSignatureSerializedData
    Ser(Serialized.ShaderStages,
        Serialized.StaticResShaderStages,
        Serialized.PipelineType,
        Serialized.StaticResStageIndex);

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(PipelineResourceSignatureDesc) == 56, "Did you add a new member to PipelineResourceSignatureDesc? Please add serialization here.");
    static_assert(sizeof(PipelineResourceDesc) == 24, "Did you add a new member to PipelineResourceDesc? Please add serialization here.");
    static_assert(sizeof(ImmutableSamplerDesc) == 72, "Did you add a new member to ImmutableSamplerDesc? Please add serialization here.");
    static_assert(sizeof(PipelineResourceSignatureSerializedData) == 16, "Did you add a new member to PipelineResourceSignatureSerializedData? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeGraphicsPSO(
    Serializer<Mode>&                       Ser,
    TQual<GraphicsPipelineStateCreateInfo>& CreateInfo,
    DynamicLinearAllocator*                 Allocator)
{
    // Read PipelineStateCreateInfo
    //   Read PipelineStateDesc
    Ser(CreateInfo.PSODesc.PipelineType,
        CreateInfo.Flags);
    // skip SRBAllocationGranularity
    // skip ImmediateContextMask
    // skip PipelineResourceLayoutDesc
    // skip ppResourceSignatures, ResourceSignaturesCount, pPSOCache
    // AZ TODO: PRS names ?

    // Read GraphicsPipelineDesc
    Ser(CreateInfo.GraphicsPipeline.BlendDesc,
        CreateInfo.GraphicsPipeline.SampleMask,
        CreateInfo.GraphicsPipeline.RasterizerDesc,
        CreateInfo.GraphicsPipeline.DepthStencilDesc);
    //   Read InputLayoutDesc
    {
        auto& InputLayout = CreateInfo.GraphicsPipeline.InputLayout;
        Ser(InputLayout.NumElements);
        auto* pLayoutElements = ArraySerializerHelper<Mode>::Create(InputLayout.LayoutElements, InputLayout.NumElements, Allocator);
        for (Uint32 i = 0; i < InputLayout.NumElements; ++i)
        {
            // Read LayoutElement
            auto& Elem = pLayoutElements[i];
            Ser(Elem.HLSLSemantic, // AZ TODO: global cache for names ?
                Elem.InputIndex,
                Elem.BufferSlot,
                Elem.NumComponents,
                Elem.ValueType,
                Elem.IsNormalized,
                Elem.RelativeOffset,
                Elem.Stride,
                Elem.Frequency,
                Elem.InstanceDataStepRate);
        }
    }
    Ser(CreateInfo.GraphicsPipeline.PrimitiveTopology,
        CreateInfo.GraphicsPipeline.NumViewports,
        CreateInfo.GraphicsPipeline.NumRenderTargets,
        CreateInfo.GraphicsPipeline.SubpassIndex,
        CreateInfo.GraphicsPipeline.ShadingRateFlags,
        CreateInfo.GraphicsPipeline.RTVFormats,
        CreateInfo.GraphicsPipeline.DSVFormat,
        CreateInfo.GraphicsPipeline.SmplDesc);
    // skip pRenderPass
    // skip NodeMask
    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(GraphicsPipelineStateCreateInfo) == 344, "Did you add a new member to GraphicsPipelineStateCreateInfo? Please add serialization here.");
    static_assert(sizeof(LayoutElement) == 40, "Did you add a new member to LayoutElement? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeComputePSO(
    Serializer<Mode>&                      Ser,
    TQual<ComputePipelineStateCreateInfo>& CreateInfo,
    DynamicLinearAllocator*                Allocator)
{
    // Read PipelineStateCreateInfo
    //   Read PipelineStateDesc
    Ser(CreateInfo.PSODesc.PipelineType,
        CreateInfo.Flags);
    // skip SRBAllocationGranularity
    // skip ImmediateContextMask
    // skip PipelineResourceLayoutDesc
    // skip ppResourceSignatures, ResourceSignaturesCount, pPSOCache
    // AZ TODO: PRS names ?

    // AZ TODO: read ComputePipelineStateCreateInfo

    // skip NodeMask
    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(ComputePipelineStateCreateInfo) == 104, "Did you add a new member to ComputePipelineStateCreateInfo? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeRayTracingPSO(
    Serializer<Mode>&                         Ser,
    TQual<RayTracingPipelineStateCreateInfo>& CreateInfo,
    DynamicLinearAllocator*                   Allocator)
{
    // Read PipelineStateCreateInfo
    //   Read PipelineStateDesc
    Ser(CreateInfo.PSODesc.PipelineType,
        CreateInfo.Flags);
    // skip SRBAllocationGranularity
    // skip ImmediateContextMask
    // skip PipelineResourceLayoutDesc
    // skip ppResourceSignatures, ResourceSignaturesCount, pPSOCache
    // AZ TODO: PRS names ?

    // AZ TODO: read RayTracingPipelineStateCreateInfo

    // skip NodeMask
    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(RayTracingPipelineStateCreateInfo) == 168, "Did you add a new member to RayTracingPipelineStateCreateInfo? Please add serialization here.");
#endif
}

template struct DeviceObjectArchiveBase::SerializerImpl<SerializerMode::Read>;
template struct DeviceObjectArchiveBase::SerializerImpl<SerializerMode::Write>;
template struct DeviceObjectArchiveBase::SerializerImpl<SerializerMode::Measure>;


} // namespace Diligent
