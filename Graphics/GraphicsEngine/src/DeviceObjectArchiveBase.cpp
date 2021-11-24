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

DeviceObjectArchiveBase::DeviceType DeviceObjectArchiveBase::RenderDeviceTypeToArchiveDeviceType(RENDER_DEVICE_TYPE RenderDeviceType)
{
    static_assert(RENDER_DEVICE_TYPE_COUNT == 7, "Please handle the new device type below");
    switch (RenderDeviceType)
    {
        case RENDER_DEVICE_TYPE_UNDEFINED:
            UNEXPECTED("Render device type is undefined");
            return DeviceType::Count;

        case RENDER_DEVICE_TYPE_D3D11:
            return DeviceType::Direct3D11;

        case RENDER_DEVICE_TYPE_D3D12:
            return DeviceType::Direct3D12;

        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            return DeviceType::OpenGL;

        case RENDER_DEVICE_TYPE_VULKAN:
            return DeviceType::Vulkan;

        case RENDER_DEVICE_TYPE_METAL:
            return DeviceType::Metal_iOS;

        case RENDER_DEVICE_TYPE_COUNT:
            return DeviceType::Count;

        default:
            UNEXPECTED("Unexpected device type");
            return DeviceType::Count;
    }
}

DeviceObjectArchiveBase::DeviceObjectArchiveBase(IReferenceCounters* pRefCounters, IArchive* pArchive, DeviceType DevType) :
    TObjectBase{pRefCounters},
    m_pArchive{pArchive},
    m_DevType{DevType},
    m_StringAllocator{GetRawAllocator(), 4 << 20}
{
    if (m_pArchive == nullptr)
        LOG_ERROR_AND_THROW("pSource must not be null");

    // Read header
    ArchiveHeader Header{};
    {
        if (!m_pArchive->Read(0, sizeof(Header), &Header))
        {
            LOG_ERROR_AND_THROW("Failed to read archive header");
        }
        if (Header.MagicNumber != HeaderMagicNumber)
        {
            LOG_ERROR_AND_THROW("Archive header magic number is incorrect");
        }
        if (Header.Version != GetHeaderVersion())
        {
            LOG_ERROR_AND_THROW("Archive version (", Header.Version, ") is not supported; expected version: ", GetHeaderVersion(), ".");
        }

        m_BaseOffsets = Header.BlockBaseOffsets;
    }

    // Read chunks
    std::vector<ChunkHeader> Chunks{Header.NumChunks};
    if (!m_pArchive->Read(sizeof(Header), sizeof(Chunks[0]) * Chunks.size(), Chunks.data()))
    {
        LOG_ERROR_AND_THROW("Failed to read chunk headers");
    }

    std::bitset<static_cast<size_t>(ChunkType::Count)> ProcessedBits{};
    for (const auto& Chunk : Chunks)
    {
        if (ProcessedBits[static_cast<size_t>(Chunk.Type)])
        {
            LOG_ERROR_AND_THROW("Multiple chunks with the same types are not allowed");
        }
        ProcessedBits[static_cast<size_t>(Chunk.Type)] = true;

        switch (Chunk.Type)
        {
            // clang-format off
            case ChunkType::ArchiveDebugInfo:         ReadArchiveDebugInfo(Chunk);                                              break;
            case ChunkType::ResourceSignature:        ReadNamedResources(Chunk,   m_PRSMap,           m_PRSMapGuard);           break;
            case ChunkType::GraphicsPipelineStates:   ReadNamedResources(Chunk,   m_GraphicsPSOMap,   m_GraphicsPSOMapGuard);   break;
            case ChunkType::ComputePipelineStates:    ReadNamedResources(Chunk,   m_ComputePSOMap,    m_ComputePSOMapGuard);    break;
            case ChunkType::RayTracingPipelineStates: ReadNamedResources(Chunk,   m_RayTracingPSOMap, m_RayTracingPSOMapGuard); break;
            case ChunkType::RenderPass:               ReadNamedResources(Chunk,   m_RenderPassMap,    m_RenderPassMapGuard);    break;
            case ChunkType::Shaders:                  ReadIndexedResources(Chunk, m_Shaders,          m_ShadersGuard);          break;
            // clang-format on
            default:
                LOG_ERROR_AND_THROW("Unknown chunk type (", static_cast<Uint32>(Chunk.Type), ")");
        }
    }
}

DeviceObjectArchiveBase::BlockOffsetType DeviceObjectArchiveBase::GetBlockOffsetType() const
{
    switch (m_DevType)
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

void DeviceObjectArchiveBase::ReadArchiveDebugInfo(const ChunkHeader& Chunk) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::ArchiveDebugInfo);

    std::vector<Uint8> Data(Chunk.Size); // AZ TODO: optimize
    if (!m_pArchive->Read(Chunk.Offset, Data.size(), Data.data()))
    {
        LOG_ERROR_AND_THROW("Failed to read archive debug info");
    }

    Serializer<SerializerMode::Read> Ser{Data.data(), Data.size()};

    Ser(m_DebugInfo.APIVersion);

    const char* GitHash = nullptr;
    Ser(GitHash);

    VERIFY_EXPR(Ser.IsEnd());
    m_DebugInfo.GitHash = String{GitHash};

    if (m_DebugInfo.APIVersion != DILIGENT_API_VERSION)
        LOG_INFO_MESSAGE("Archive was created with Engine API version (", m_DebugInfo.APIVersion, ") but is used with (", DILIGENT_API_VERSION, ")");
#ifdef DILIGENT_CORE_COMMIT_HASH
    if (m_DebugInfo.GitHash != DILIGENT_CORE_COMMIT_HASH)
        LOG_INFO_MESSAGE("Archive was built with Diligent Core git hash '", m_DebugInfo.GitHash, "' but is used with '", DILIGENT_CORE_COMMIT_HASH, "'.");
#endif
}

template <typename ResType>
void DeviceObjectArchiveBase::ReadNamedResources(const ChunkHeader& Chunk, TNameOffsetMap<ResType>& NameAndOffset, std::mutex& Guard) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::ResourceSignature ||
                Chunk.Type == ChunkType::GraphicsPipelineStates ||
                Chunk.Type == ChunkType::ComputePipelineStates ||
                Chunk.Type == ChunkType::RayTracingPipelineStates ||
                Chunk.Type == ChunkType::RenderPass);

    std::vector<Uint8> Data(Chunk.Size); // AZ TODO: optimize
    if (!m_pArchive->Read(Chunk.Offset, Data.size(), Data.data()))
    {
        LOG_ERROR_AND_THROW("Failed to read resource list from archive");
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

    std::unique_lock<std::mutex> WriteLock{Guard};

    // Read names
    Uint32 Offset = 0;
    for (Uint32 i = 0; i < Header.Count; ++i)
    {
        if (Offset + NameLengthArray[i] > Data.size())
        {
            LOG_ERROR_AND_THROW("Failed to read archive data");
        }
        if (DataOffsetArray[i] + DataSizeArray[i] > m_pArchive->GetSize())
        {
            LOG_ERROR_AND_THROW("Failed to read archive data");
        }
        VERIFY_EXPR(strlen(NameDataPtr + Offset) + 1 == NameLengthArray[i]);

        const char* ResName  = m_StringAllocator.CopyString(NameDataPtr + Offset);
        bool        Inserted = NameAndOffset.emplace(HashMapStringKey{ResName}, FileOffsetAndSize{DataOffsetArray[i], DataSizeArray[i]}).second;
        DEV_CHECK_ERR(Inserted, "Each name in the resource names array must be unique");
        Offset += NameLengthArray[i];
    }
}

void DeviceObjectArchiveBase::ReadIndexedResources(const ChunkHeader& Chunk, TShaderOffsetAndCache& Resources, std::mutex& Guard) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::Shaders);
    VERIFY_EXPR(Chunk.Size == sizeof(ShadersDataHeader));

    ShadersDataHeader Header;
    if (!m_pArchive->Read(Chunk.Offset, sizeof(Header), &Header))
    {
        LOG_ERROR_AND_THROW("Failed to read indexed resources info from the archive");
    }

    DynamicLinearAllocator Allocator{GetRawAllocator()};
    LoadDeviceSpecificData(
        Header,
        Allocator,
        "Shader list",
        GetBlockOffsetType(),
        [&](const void* pData, size_t DataSize) //
        {
            VERIFY_EXPR(DataSize % sizeof(FileOffsetAndSize) == 0);
            const size_t Count = DataSize / sizeof(FileOffsetAndSize);

            const auto* pFileOffsetAndSize = reinterpret_cast<const FileOffsetAndSize*>(pData);

            std::unique_lock<std::mutex> WriteLock{Guard};
            Resources.resize(Count);

            for (Uint32 i = 0; i < Count; ++i)
                Resources[i] = pFileOffsetAndSize[i];
        });
}

template <typename ResType, typename FnType>
bool DeviceObjectArchiveBase::LoadResourceData(const TNameOffsetMap<ResType>& NameAndOffset,
                                               std::mutex&                    Guard,
                                               const char*                    ResourceName,
                                               DynamicLinearAllocator&        Allocator,
                                               const char*                    ResTypeName,
                                               const FnType&                  Fn)
{
    FileOffsetAndSize OffsetAndSize;
    const char*       ResName = nullptr;
    {
        std::unique_lock<std::mutex> ReadLock{Guard};

        auto Iter = NameAndOffset.find(ResourceName);
        if (Iter == NameAndOffset.end())
        {
            LOG_ERROR_MESSAGE(ResTypeName, " with name '", ResourceName, "' is not present in the archive");
            return false;
        }
        OffsetAndSize = Iter->second;
        ResName       = Iter->first.GetStr();
    }

    const auto DataSize = OffsetAndSize.Size;
    void*      pData    = Allocator.Allocate(DataSize, DataPtrAlign);
    if (!m_pArchive->Read(OffsetAndSize.Offset, DataSize, pData))
    {
        LOG_ERROR_MESSAGE("Failed to read ", ResTypeName, " with name '", ResourceName, "' data from the archive");
        return false;
    }

    Serializer<SerializerMode::Read> Ser{pData, DataSize};
    return Fn(ResName, Ser);
}

template <typename HeaderType, typename FnType>
void DeviceObjectArchiveBase::LoadDeviceSpecificData(const HeaderType&       Header,
                                                     DynamicLinearAllocator& Allocator,
                                                     const char*             ResTypeName,
                                                     BlockOffsetType         BlockType,
                                                     const FnType&           Fn)
{
    const auto BaseOffset  = m_BaseOffsets[static_cast<Uint32>(BlockType)];
    const auto ArchiveSize = m_pArchive->GetSize();
    if (BaseOffset > ArchiveSize)
    {
        LOG_ERROR_MESSAGE("Required block does not exists in archive");
        return;
    }
    if (Header.GetSize(m_DevType) == 0)
    {
        LOG_ERROR_MESSAGE("Device specific data is not specified for ", ResTypeName);
        return;
    }
    if (BaseOffset + Header.GetEndOffset(m_DevType) > ArchiveSize)
    {
        LOG_ERROR_MESSAGE("Invalid offset in the archive");
        return;
    }

    const auto DataSize = Header.GetSize(m_DevType);
    auto*      pData    = Allocator.Allocate(DataSize, DataPtrAlign);
    if (!m_pArchive->Read(BaseOffset + Header.GetOffset(m_DevType), DataSize, pData))
    {
        LOG_ERROR_MESSAGE("Failed to read resource-specific data");
        return;
    }

    return Fn(pData, DataSize);
}

bool DeviceObjectArchiveBase::ReadPRSData(const char* Name, PRSData& PRS)
{
    return LoadResourceData(
        m_PRSMap, m_PRSMapGuard, Name, PRS.Allocator,
        "Resource signature",
        [&PRS](const char* Name, Serializer<SerializerMode::Read>& Ser) -> bool //
        {
            PRS.Desc.Name = Name;
            PRS.pHeader   = Ser.Cast<PRSDataHeader>();
            if (PRS.pHeader->Type != ChunkType::ResourceSignature)
            {
                LOG_ERROR_MESSAGE("Invalid PRS header in the archive");
                return false;
            }

            PSOSerializer<SerializerMode::Read>::SerializePRS(Ser, PRS.Desc, PRS.Serialized, &PRS.Allocator);
            VERIFY_EXPR(Ser.IsEnd());
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadRPData(const char* Name, RPData& RP)
{
    return LoadResourceData(
        m_RenderPassMap, m_RenderPassMapGuard, Name, RP.Allocator,
        "Render pass",
        [&RP](const char* Name, Serializer<SerializerMode::Read>& Ser) -> bool //
        {
            RP.Desc.Name = Name;
            RP.pHeader   = Ser.Cast<RPDataHeader>();
            if (RP.pHeader->Type != ChunkType::RenderPass)
            {
                LOG_ERROR_MESSAGE("Invalid render pass header in the archive");
                return false;
            }

            PSOSerializer<SerializerMode::Read>::SerializeRenderPass(Ser, RP.Desc, &RP.Allocator);
            VERIFY_EXPR(Ser.IsEnd());
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadGraphicsPSOData(const char* Name, PSOData<GraphicsPipelineStateCreateInfo>& PSO)
{
    return LoadResourceData(
        m_GraphicsPSOMap, m_GraphicsPSOMapGuard, Name, PSO.Allocator,
        "Graphics pipeline",
        [&PSO](const char* Name, Serializer<SerializerMode::Read>& Ser) -> bool //
        {
            PSO.CreateInfo.PSODesc.Name = Name;
            PSO.pHeader                 = Ser.Cast<PSODataHeader>();
            if (PSO.pHeader->Type != ChunkType::GraphicsPipelineStates)
            {
                LOG_ERROR_MESSAGE("Invalid graphics pipeline header in the archive");
                return false;
            }

            PSOSerializer<SerializerMode::Read>::SerializeGraphicsPSO(Ser, PSO.CreateInfo, PSO.PRSNames, PSO.RenderPassName, &PSO.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            PSO.CreateInfo.Flags |= PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES;
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadComputePSOData(const char* Name, PSOData<ComputePipelineStateCreateInfo>& PSO)
{
    return LoadResourceData(
        m_ComputePSOMap, m_ComputePSOMapGuard, Name, PSO.Allocator,
        "Compute pipeline",
        [&PSO](const char* Name, Serializer<SerializerMode::Read>& Ser) -> bool //
        {
            PSO.CreateInfo.PSODesc.Name = Name;
            PSO.pHeader                 = Ser.Cast<PSODataHeader>();
            if (PSO.pHeader->Type != ChunkType::ComputePipelineStates)
            {
                LOG_ERROR_MESSAGE("Invalid compute pipeline header in the archive");
                return false;
            }

            PSOSerializer<SerializerMode::Read>::SerializeComputePSO(Ser, PSO.CreateInfo, PSO.PRSNames, &PSO.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            PSO.CreateInfo.Flags |= PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES;
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadTilePSOData(const char* Name, PSOData<TilePipelineStateCreateInfo>& PSO)
{
    return LoadResourceData(
        m_TilePSOMap, m_TilePSOMapGuard, Name, PSO.Allocator,
        "Tile pipeline",
        [&PSO](const char* Name, Serializer<SerializerMode::Read>& Ser) -> bool //
        {
            PSO.CreateInfo.PSODesc.Name = Name;
            PSO.pHeader                 = Ser.Cast<PSODataHeader>();
            if (PSO.pHeader->Type != ChunkType::ComputePipelineStates)
            {
                LOG_ERROR_MESSAGE("Invalid tile pipeline header in the archive");
                return false;
            }

            PSOSerializer<SerializerMode::Read>::SerializeTilePSO(Ser, PSO.CreateInfo, PSO.PRSNames, &PSO.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            PSO.CreateInfo.Flags |= PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES;
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadRayTracingPSOData(const char* Name, PSOData<RayTracingPipelineStateCreateInfo>& PSO)
{
    return LoadResourceData(
        m_RayTracingPSOMap, m_RayTracingPSOMapGuard, Name, PSO.Allocator,
        "Ray tracing pipeline",
        [&PSO](const char* Name, Serializer<SerializerMode::Read>& Ser) -> bool //
        {
            PSO.CreateInfo.PSODesc.Name = Name;
            PSO.pHeader                 = Ser.Cast<PSODataHeader>();
            if (PSO.pHeader->Type != ChunkType::RayTracingPipelineStates)
            {
                LOG_ERROR_MESSAGE("Invalid ray tracing pipeline header in the archive");
                return false;
            }

            auto RemapShaders = [](Uint32& InIndex, IShader*& outShader) {
                outShader = BitCast<IShader*>(size_t{InIndex});
            };

            PSOSerializer<SerializerMode::Read>::SerializeRayTracingPSO(Ser, PSO.CreateInfo, PSO.PRSNames, RemapShaders, &PSO.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            PSO.CreateInfo.Flags |= PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES;
            return true;
        });
}

template <typename ResType>
bool DeviceObjectArchiveBase::GetCachedResource(const char* Name, TNameOffsetMapAndWeakCache<ResType>& Cache, std::mutex& Guard, ResType** ppResource)
{
    std::unique_lock<std::mutex> ReadLock{Guard};

    VERIFY_EXPR(ppResource != nullptr);
    *ppResource = nullptr;

    auto Iter = Cache.find(Name);
    if (Iter == Cache.end())
        return false;

    auto Ptr = Iter->second.Cache.Lock();
    if (Ptr == nullptr)
        return false;

    *ppResource = Ptr.Detach();
    return true;
}

template <typename ResType>
void DeviceObjectArchiveBase::CacheResource(const char* Name, TNameOffsetMapAndWeakCache<ResType>& Cache, std::mutex& Guard, ResType* pResource)
{
    VERIFY_EXPR(pResource != nullptr);

    std::unique_lock<std::mutex> WriteLock{Guard};

    auto Iter = Cache.find(Name);
    if (Iter == Cache.end())
        return;

    auto Ptr = Iter->second.Cache.Lock();
    if (Ptr != nullptr)
        return;

    Iter->second.Cache = pResource;
}

template <typename ResType>
bool DeviceObjectArchiveBase::GetCachedResource(const char* Name, TNameOffsetMapAndStrongCache<ResType>& Cache, std::mutex& Guard, ResType** ppResource)
{
    std::unique_lock<std::mutex> ReadLock{Guard};

    VERIFY_EXPR(ppResource != nullptr);
    *ppResource = nullptr;

    auto Iter = Cache.find(Name);
    if (Iter == Cache.end())
        return false;

    if (Iter->second.Cache == nullptr)
        return false;

    *ppResource = Iter->second.Cache;
    *ppResource->AddRef();
    return true;
}

template <typename ResType>
void DeviceObjectArchiveBase::CacheResource(const char* Name, TNameOffsetMapAndStrongCache<ResType>& Cache, std::mutex& Guard, ResType* pResource)
{
    VERIFY_EXPR(pResource != nullptr);

    std::unique_lock<std::mutex> WriteLock{Guard};

    auto Iter = Cache.find(Name);
    if (Iter == Cache.end())
        return;

    if (Iter->second.Cache != nullptr)
        return;

    Iter->second.Cache = pResource;
}

template <>
inline DeviceObjectArchiveBase::ReleaseTempResourceRefs<GraphicsPipelineStateCreateInfo>::~ReleaseTempResourceRefs()
{
    if (PSO.CreateInfo.ppResourceSignatures != nullptr)
    {
        for (Uint32 i = 0; i < PSO.CreateInfo.ResourceSignaturesCount; ++i)
        {
            if (PSO.CreateInfo.ppResourceSignatures[i] != nullptr)
                PSO.CreateInfo.ppResourceSignatures[i]->Release();
        }
    }

    if (PSO.CreateInfo.GraphicsPipeline.pRenderPass != nullptr)
        PSO.CreateInfo.GraphicsPipeline.pRenderPass->Release();
}

template <>
inline DeviceObjectArchiveBase::ReleaseTempResourceRefs<ComputePipelineStateCreateInfo>::~ReleaseTempResourceRefs()
{
    if (PSO.CreateInfo.ppResourceSignatures != nullptr)
    {
        for (Uint32 i = 0; i < PSO.CreateInfo.ResourceSignaturesCount; ++i)
        {
            if (PSO.CreateInfo.ppResourceSignatures[i] != nullptr)
                PSO.CreateInfo.ppResourceSignatures[i]->Release();
        }
    }
}

template <>
inline DeviceObjectArchiveBase::ReleaseTempResourceRefs<TilePipelineStateCreateInfo>::~ReleaseTempResourceRefs()
{
    if (PSO.CreateInfo.ppResourceSignatures != nullptr)
    {
        for (Uint32 i = 0; i < PSO.CreateInfo.ResourceSignaturesCount; ++i)
        {
            if (PSO.CreateInfo.ppResourceSignatures[i] != nullptr)
                PSO.CreateInfo.ppResourceSignatures[i]->Release();
        }
    }
}

template <>
inline DeviceObjectArchiveBase::ReleaseTempResourceRefs<RayTracingPipelineStateCreateInfo>::~ReleaseTempResourceRefs()
{
    if (PSO.CreateInfo.ppResourceSignatures != nullptr)
    {
        for (Uint32 i = 0; i < PSO.CreateInfo.ResourceSignaturesCount; ++i)
        {
            if (PSO.CreateInfo.ppResourceSignatures[i] != nullptr)
                PSO.CreateInfo.ppResourceSignatures[i]->Release();
        }
    }
}


bool DeviceObjectArchiveBase::CreateRenderPass(PSOData<GraphicsPipelineStateCreateInfo>& PSO, IRenderDevice* pRenderDevice)
{
    VERIFY_EXPR(pRenderDevice != nullptr);
    if (PSO.RenderPassName == nullptr || *PSO.RenderPassName == 0)
        return true;

    RenderPassUnpackInfo UnpackInfo;
    UnpackInfo.Name    = PSO.RenderPassName;
    UnpackInfo.pDevice = pRenderDevice;

    IRenderPass* pRP = nullptr;
    UnpackRenderPass(UnpackInfo, pRP); // Reference released in ~ReleaseTempResourceRefs()
    if (pRP == nullptr)
        return false;

    PSO.CreateInfo.GraphicsPipeline.pRenderPass = pRP;
    return true;
}

template <typename CreateInfoType>
bool DeviceObjectArchiveBase::CreateResourceSignatures(PSOData<CreateInfoType>& PSO, IRenderDevice* pRenderDevice)
{
    if (PSO.CreateInfo.ResourceSignaturesCount == 0)
    {
        UNEXPECTED("PSO must have at least one resource signature");
        return true;
    }
    auto* ppResourceSignatures = PSO.Allocator.template Allocate<IPipelineResourceSignature*>(PSO.CreateInfo.ResourceSignaturesCount);

    ResourceSignatureUnpackInfo UnpackInfo;
    UnpackInfo.SRBAllocationGranularity = DefaultSRBAllocationGranularity;
    UnpackInfo.pDevice                  = pRenderDevice;

    PSO.CreateInfo.ppResourceSignatures = ppResourceSignatures;
    for (Uint32 i = 0; i < PSO.CreateInfo.ResourceSignaturesCount; ++i)
    {
        UnpackInfo.Name = PSO.PRSNames[i];
        UnpackResourceSignature(UnpackInfo, ppResourceSignatures[i]); // Reference released in ~ReleaseTempResourceRefs()
        if (ppResourceSignatures[i] == nullptr)
            return false;
    }
    return true;
}

void DeviceObjectArchiveBase::ReadAndCreateShader(Serializer<SerializerMode::Read>& Ser, ShaderCreateInfo& ShaderCI, IRenderDevice* pDevice, IShader** ppShader)
{
    VERIFY_EXPR(ShaderCI.SourceLanguage == SHADER_SOURCE_LANGUAGE_DEFAULT);
    VERIFY_EXPR(ShaderCI.ShaderCompiler == SHADER_COMPILER_DEFAULT);

    ShaderCI.ByteCode     = Ser.GetCurrentPtr();
    ShaderCI.ByteCodeSize = Ser.GetRemainSize();

    pDevice->CreateShader(ShaderCI, ppShader);
}

bool DeviceObjectArchiveBase::LoadShaders(Serializer<SerializerMode::Read>&    Ser,
                                          IRenderDevice*                       pDevice,
                                          std::vector<RefCntAutoPtr<IShader>>& Shaders)
{
    const auto BaseOffset = m_BaseOffsets[static_cast<Uint32>(GetBlockOffsetType())];
    if (BaseOffset > m_pArchive->GetSize())
    {
        LOG_ERROR_MESSAGE("Required block does not exists in archive");
        return false;
    }

    DynamicLinearAllocator Allocator{GetRawAllocator()};

    ShaderIndexArray ShaderIndices;
    PSOSerializer<SerializerMode::Read>::SerializeShaders(Ser, ShaderIndices, &Allocator);

    Shaders.resize(ShaderIndices.Count);

    for (Uint32 i = 0; i < ShaderIndices.Count; ++i)
    {
        const Uint32 Idx = ShaderIndices.pIndices[i];

        FileOffsetAndSize OffsetAndSize;
        {
            std::unique_lock<std::mutex> ReadLock{m_ShadersGuard};

            if (Idx >= m_Shaders.size())
                return false;

            // Try to get cached shader
            if (m_Shaders[Idx].Cache)
            {
                Shaders[i] = m_Shaders[Idx].Cache;
                continue;
            }

            OffsetAndSize = m_Shaders[Idx];
        }

        void* pData = Allocator.Allocate(OffsetAndSize.Size, DataPtrAlign);

        if (!m_pArchive->Read(BaseOffset + OffsetAndSize.Offset, OffsetAndSize.Size, pData))
            return false;

        Serializer<SerializerMode::Read> Ser2{pData, OffsetAndSize.Size};
        ShaderCreateInfo                 ShaderCI;
        Ser2(ShaderCI.Desc.ShaderType, ShaderCI.EntryPoint, ShaderCI.SourceLanguage, ShaderCI.ShaderCompiler);

        ShaderCI.CompileFlags |= SHADER_COMPILE_FLAG_SKIP_REFLECTION;

        RefCntAutoPtr<IShader> pShader;
        ReadAndCreateShader(Ser2, ShaderCI, pDevice, &pShader);
        if (!pShader)
            return false;

        Shaders[i] = pShader;

        // Add to cache
        {
            std::unique_lock<std::mutex> WriteLock{m_ShadersGuard};
            m_Shaders[Idx].Cache = pShader;
        }
    }
    return true;
}

void DeviceObjectArchiveBase::UnpackGraphicsPSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO)
{
    VERIFY_EXPR(DeArchiveInfo.pDevice != nullptr);

    const bool HasOverrideFlags = DeArchiveInfo.OverrideFlags != PSO_UNPACK_OVERRIDE_FLAG_NONE;
    DEV_CHECK_ERR(!HasOverrideFlags || DeArchiveInfo.pGraphicsPipelineDesc != nullptr, "pGraphicsPipelineDesc must not be null");

    if (!HasOverrideFlags && GetCachedResource(DeArchiveInfo.Name, m_GraphicsPSOMap, m_GraphicsPSOMapGuard, &pPSO))
        return;

    PSOData<GraphicsPipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadGraphicsPSOData(DeArchiveInfo.Name, PSO))
        return;

    ReleaseTempResourceRefs<GraphicsPipelineStateCreateInfo> ReleaseRefs{PSO};

    if (!CreateRenderPass(PSO, DeArchiveInfo.pDevice))
        return;

    if (!CreateResourceSignatures(PSO, DeArchiveInfo.pDevice))
        return;

    PSO.CreateInfo.PSODesc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;
    PSO.CreateInfo.PSODesc.ImmediateContextMask     = DeArchiveInfo.ImmediateContextMask;

    LoadDeviceSpecificData(
        *PSO.pHeader,
        PSO.Allocator,
        "Graphics pipeline",
        GetBlockOffsetType(),
        [&](void* pData, size_t DataSize) //
        {
            Serializer<SerializerMode::Read> Ser{pData, DataSize};

            std::vector<RefCntAutoPtr<IShader>> Shaders;
            if (!LoadShaders(Ser, DeArchiveInfo.pDevice, Shaders))
                return;

            for (auto& Shader : Shaders)
            {
                switch (Shader->GetDesc().ShaderType)
                {
                    // clang-format off
                    case SHADER_TYPE_VERTEX:        PSO.CreateInfo.pVS = Shader; break;
                    case SHADER_TYPE_PIXEL:         PSO.CreateInfo.pPS = Shader; break;
                    case SHADER_TYPE_GEOMETRY:      PSO.CreateInfo.pGS = Shader; break;
                    case SHADER_TYPE_HULL:          PSO.CreateInfo.pHS = Shader; break; 
                    case SHADER_TYPE_DOMAIN:        PSO.CreateInfo.pDS = Shader; break;
                    case SHADER_TYPE_AMPLIFICATION: PSO.CreateInfo.pAS = Shader; break;
                    case SHADER_TYPE_MESH:          PSO.CreateInfo.pMS = Shader; break;
                    // clang-format on
                    default:
                        LOG_ERROR_MESSAGE("Unsupported shader type for graphics pipeline");
                        return;
                }
            }

            for (auto OverrideFlags = DeArchiveInfo.OverrideFlags; OverrideFlags != 0;)
            {
                auto OverrideFlag = ExtractLSB(OverrideFlags);

                static_assert(PSO_UNPACK_OVERRIDE_FLAG_LAST == (1u << 12), "Please update the switch below to handle the new PSO unpack override flag");
                switch (OverrideFlag)
                {
                    case PSO_UNPACK_OVERRIDE_FLAG_NAME:
                        PSO.CreateInfo.PSODesc.Name = "AZ TODO";
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_RASTERIZER:
                        PSO.CreateInfo.GraphicsPipeline.RasterizerDesc = DeArchiveInfo.pGraphicsPipelineDesc->RasterizerDesc;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_BLEND_STATE:
                        PSO.CreateInfo.GraphicsPipeline.BlendDesc = DeArchiveInfo.pGraphicsPipelineDesc->BlendDesc;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_SAMPLE_MASK:
                        PSO.CreateInfo.GraphicsPipeline.SampleMask = DeArchiveInfo.pGraphicsPipelineDesc->SampleMask;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_DEPTH_STENCIL_DESC:
                        PSO.CreateInfo.GraphicsPipeline.DepthStencilDesc = DeArchiveInfo.pGraphicsPipelineDesc->DepthStencilDesc;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_INPUT_LAYOUT:
                        PSO.CreateInfo.GraphicsPipeline.InputLayout = DeArchiveInfo.pGraphicsPipelineDesc->InputLayout;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_PRIMITIVE_TOPOLOGY:
                        PSO.CreateInfo.GraphicsPipeline.PrimitiveTopology = DeArchiveInfo.pGraphicsPipelineDesc->PrimitiveTopology;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_NUM_VIEWPORTS:
                        PSO.CreateInfo.GraphicsPipeline.NumViewports = DeArchiveInfo.pGraphicsPipelineDesc->NumViewports;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_RENDER_TARGETS:
                        PSO.CreateInfo.GraphicsPipeline.NumRenderTargets = DeArchiveInfo.pGraphicsPipelineDesc->NumRenderTargets;
                        memcpy(PSO.CreateInfo.GraphicsPipeline.RTVFormats, DeArchiveInfo.pGraphicsPipelineDesc->RTVFormats, sizeof(PSO.CreateInfo.GraphicsPipeline.RTVFormats));
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_RENDER_PASS:
                        PSO.CreateInfo.GraphicsPipeline.pRenderPass  = DeArchiveInfo.pGraphicsPipelineDesc->pRenderPass;
                        PSO.CreateInfo.GraphicsPipeline.SubpassIndex = DeArchiveInfo.pGraphicsPipelineDesc->SubpassIndex;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_SHADING_RATE:
                        PSO.CreateInfo.GraphicsPipeline.ShadingRateFlags = DeArchiveInfo.pGraphicsPipelineDesc->ShadingRateFlags;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_DEPTH_STENCIL_TARGET:
                        PSO.CreateInfo.GraphicsPipeline.DSVFormat = DeArchiveInfo.pGraphicsPipelineDesc->DSVFormat;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_SAMPLE_DESC:
                        PSO.CreateInfo.GraphicsPipeline.SmplDesc = DeArchiveInfo.pGraphicsPipelineDesc->SmplDesc;
                        break;
                    default:
                        UNEXPECTED("Unexpected PSO unpack override flag");
                        break;
                }
            }

            VERIFY_EXPR(Ser.IsEnd());

            DeArchiveInfo.pDevice->CreateGraphicsPipelineState(PSO.CreateInfo, &pPSO);
            if (!HasOverrideFlags)
                CacheResource(DeArchiveInfo.Name, m_GraphicsPSOMap, m_GraphicsPSOMapGuard, pPSO);
        });
}

void DeviceObjectArchiveBase::UnpackComputePSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO)
{
    VERIFY_EXPR(DeArchiveInfo.pDevice != nullptr);

    const bool HasOverrideFlags = DeArchiveInfo.OverrideFlags != PSO_UNPACK_OVERRIDE_FLAG_NONE;
    DEV_CHECK_ERR(!HasOverrideFlags, "Override flags are not supported for Compute PSO");

    if (GetCachedResource(DeArchiveInfo.Name, m_ComputePSOMap, m_ComputePSOMapGuard, &pPSO))
        return;

    PSOData<ComputePipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadComputePSOData(DeArchiveInfo.Name, PSO))
        return;

    ReleaseTempResourceRefs<ComputePipelineStateCreateInfo> ReleaseRefs{PSO};

    if (!CreateResourceSignatures(PSO, DeArchiveInfo.pDevice))
        return;

    PSO.CreateInfo.PSODesc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;
    PSO.CreateInfo.PSODesc.ImmediateContextMask     = DeArchiveInfo.ImmediateContextMask;

    LoadDeviceSpecificData(
        *PSO.pHeader,
        PSO.Allocator,
        "Compute pipeline",
        GetBlockOffsetType(),
        [&](void* pData, size_t DataSize) //
        {
            Serializer<SerializerMode::Read> Ser{pData, DataSize};

            std::vector<RefCntAutoPtr<IShader>> Shaders;
            if (!LoadShaders(Ser, DeArchiveInfo.pDevice, Shaders))
                return;

            if (Shaders.size() != 1 || Shaders[0]->GetDesc().ShaderType != SHADER_TYPE_COMPUTE)
                return;

            PSO.CreateInfo.pCS = Shaders[0];

            VERIFY_EXPR(Ser.IsEnd());

            DeArchiveInfo.pDevice->CreateComputePipelineState(PSO.CreateInfo, &pPSO);
            if (!HasOverrideFlags)
                CacheResource(DeArchiveInfo.Name, m_ComputePSOMap, m_ComputePSOMapGuard, pPSO);
        });
}

void DeviceObjectArchiveBase::UnpackTilePSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO)
{
    VERIFY_EXPR(DeArchiveInfo.pDevice != nullptr);

    const bool HasOverrideFlags = DeArchiveInfo.OverrideFlags != PSO_UNPACK_OVERRIDE_FLAG_NONE;
    DEV_CHECK_ERR(!HasOverrideFlags || DeArchiveInfo.pTilePipelineDesc != nullptr, "pTilePipelineDesc must not be null");

    if (!HasOverrideFlags && GetCachedResource(DeArchiveInfo.Name, m_TilePSOMap, m_TilePSOMapGuard, &pPSO))
        return;

    PSOData<TilePipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadTilePSOData(DeArchiveInfo.Name, PSO))
        return;

    ReleaseTempResourceRefs<TilePipelineStateCreateInfo> ReleaseRefs{PSO};

    if (!CreateResourceSignatures(PSO, DeArchiveInfo.pDevice))
        return;

    PSO.CreateInfo.PSODesc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;
    PSO.CreateInfo.PSODesc.ImmediateContextMask     = DeArchiveInfo.ImmediateContextMask;

    LoadDeviceSpecificData(
        *PSO.pHeader,
        PSO.Allocator,
        "Tile pipeline",
        GetBlockOffsetType(),
        [&](void* pData, size_t DataSize) //
        {
            Serializer<SerializerMode::Read> Ser{pData, DataSize};

            std::vector<RefCntAutoPtr<IShader>> Shaders;
            if (!LoadShaders(Ser, DeArchiveInfo.pDevice, Shaders))
                return;

            if (Shaders.size() != 1 || Shaders[0]->GetDesc().ShaderType != SHADER_TYPE_TILE)
                return;

            PSO.CreateInfo.pTS = Shaders[0];

            for (auto OverrideFlags = DeArchiveInfo.OverrideFlags; OverrideFlags != 0;)
            {
                auto OverrideFlag = ExtractLSB(OverrideFlags);

                static_assert(PSO_UNPACK_OVERRIDE_FLAG_LAST == (1u << 12), "Please update the switch below to handle the new PSO unpack override flag");
                switch (OverrideFlag)
                {
                    case PSO_UNPACK_OVERRIDE_FLAG_NAME:
                        PSO.CreateInfo.PSODesc.Name = "AZ TODO";
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_RASTERIZER:
                        PSO.CreateInfo.TilePipeline.SampleCount = DeArchiveInfo.pTilePipelineDesc->SampleCount;
                        break;
                    case PSO_UNPACK_OVERRIDE_FLAG_RENDER_TARGETS:
                        PSO.CreateInfo.TilePipeline.NumRenderTargets = DeArchiveInfo.pTilePipelineDesc->NumRenderTargets;
                        memcpy(PSO.CreateInfo.TilePipeline.RTVFormats, DeArchiveInfo.pTilePipelineDesc->RTVFormats, sizeof(PSO.CreateInfo.TilePipeline.RTVFormats));
                        break;
                    default:
                        UNEXPECTED("Unexpected PSO unpack override flag");
                        break;
                }
            }

            VERIFY_EXPR(Ser.IsEnd());

            DeArchiveInfo.pDevice->CreateTilePipelineState(PSO.CreateInfo, &pPSO);
            if (!HasOverrideFlags)
                CacheResource(DeArchiveInfo.Name, m_TilePSOMap, m_TilePSOMapGuard, pPSO);
        });
}

void DeviceObjectArchiveBase::UnpackRayTracingPSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO)
{
    VERIFY_EXPR(DeArchiveInfo.pDevice != nullptr);

    const bool HasOverrideFlags = DeArchiveInfo.OverrideFlags != PSO_UNPACK_OVERRIDE_FLAG_NONE;
    DEV_CHECK_ERR(!HasOverrideFlags, "Override flags are not supported for Ray tracing PSO");

    if (GetCachedResource(DeArchiveInfo.Name, m_RayTracingPSOMap, m_RayTracingPSOMapGuard, &pPSO))
        return;

    PSOData<RayTracingPipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadRayTracingPSOData(DeArchiveInfo.Name, PSO))
        return;

    ReleaseTempResourceRefs<RayTracingPipelineStateCreateInfo> ReleaseRefs{PSO};

    if (!CreateResourceSignatures(PSO, DeArchiveInfo.pDevice))
        return;

    PSO.CreateInfo.PSODesc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;
    PSO.CreateInfo.PSODesc.ImmediateContextMask     = DeArchiveInfo.ImmediateContextMask;

    LoadDeviceSpecificData(
        *PSO.pHeader,
        PSO.Allocator,
        "Ray tracing pipeline",
        GetBlockOffsetType(),
        [&](void* pData, size_t DataSize) //
        {
            Serializer<SerializerMode::Read> Ser{pData, DataSize};

            std::vector<RefCntAutoPtr<IShader>> Shaders;
            if (!LoadShaders(Ser, DeArchiveInfo.pDevice, Shaders))
                return;

            auto RemapShader = [&Shaders](IShader* const& inoutShader) //
            {
                size_t ShaderIndex = BitCast<size_t>(inoutShader);
                if (ShaderIndex < Shaders.size())
                    const_cast<IShader*&>(inoutShader) = Shaders[ShaderIndex];
                else
                {
                    VERIFY(ShaderIndex == ~0u, "Failed to remap shader");
                    const_cast<IShader*&>(inoutShader) = nullptr;
                }
            };

            // Set shaders to CreateInfo
            for (Uint32 i = 0; i < PSO.CreateInfo.GeneralShaderCount; ++i)
            {
                RemapShader(PSO.CreateInfo.pGeneralShaders[i].pShader);
            }
            for (Uint32 i = 0; i < PSO.CreateInfo.TriangleHitShaderCount; ++i)
            {
                RemapShader(PSO.CreateInfo.pTriangleHitShaders[i].pClosestHitShader);
                RemapShader(PSO.CreateInfo.pTriangleHitShaders[i].pAnyHitShader);
            }
            for (Uint32 i = 0; i < PSO.CreateInfo.ProceduralHitShaderCount; ++i)
            {
                RemapShader(PSO.CreateInfo.pProceduralHitShaders[i].pIntersectionShader);
                RemapShader(PSO.CreateInfo.pProceduralHitShaders[i].pClosestHitShader);
                RemapShader(PSO.CreateInfo.pProceduralHitShaders[i].pAnyHitShader);
            }

            VERIFY_EXPR(Ser.IsEnd());

            DeArchiveInfo.pDevice->CreateRayTracingPipelineState(PSO.CreateInfo, &pPSO);
            if (!HasOverrideFlags)
                CacheResource(DeArchiveInfo.Name, m_RayTracingPSOMap, m_RayTracingPSOMapGuard, pPSO);
        });
}

void DeviceObjectArchiveBase::UnpackRenderPass(const RenderPassUnpackInfo& DeArchiveInfo, IRenderPass*& pRP)
{
    VERIFY_EXPR(DeArchiveInfo.pDevice != nullptr);

    const bool OverrideAttachments = DeArchiveInfo.AttachmentCount != 0;

    if (!OverrideAttachments && GetCachedResource(DeArchiveInfo.Name, m_RenderPassMap, m_RenderPassMapGuard, &pRP))
        return;

    RPData RP{GetRawAllocator()};
    if (!ReadRPData(DeArchiveInfo.Name, RP))
        return;

    if (OverrideAttachments)
    {
        auto* pAttachments   = RP.Allocator.CopyArray(RP.Desc.pAttachments, RP.Desc.AttachmentCount);
        RP.Desc.pAttachments = pAttachments;

        for (Uint32 i = 0; i < DeArchiveInfo.AttachmentCount; ++i)
        {
            const auto& Override = DeArchiveInfo.pAttachments[i];
            auto&       Dst      = pAttachments[Override.AttachmentIndex];

            for (auto OverrideFlags = Override.OverrideFlags; OverrideFlags != 0;)
            {
                const auto OverrideFlag = ExtractLSB(OverrideFlags);

                static_assert(RP_UNPACK_OVERRIDE_FLAG_LAST == (1u << 7), "Please update the switch below to handle the new RP unpack override flag");
                switch (OverrideFlag)
                {
                    // clang-format off
                    case RP_UNPACK_OVERRIDE_FLAG_FORMAT:           Dst.Format         = Override.AttachmentDesc.Format;         break;
                    case RP_UNPACK_OVERRIDE_FLAG_SAMPLE_COUNT:     Dst.SampleCount    = Override.AttachmentDesc.SampleCount;    break;
                    case RP_UNPACK_OVERRIDE_FLAG_LOAD_OP:          Dst.LoadOp         = Override.AttachmentDesc.LoadOp;         break;
                    case RP_UNPACK_OVERRIDE_FLAG_STORE_OP:         Dst.StoreOp        = Override.AttachmentDesc.StoreOp;        break;
                    case RP_UNPACK_OVERRIDE_FLAG_STENCIL_LOAD_OP:  Dst.StencilLoadOp  = Override.AttachmentDesc.StencilLoadOp;  break;
                    case RP_UNPACK_OVERRIDE_FLAG_STENCIL_STORE_OP: Dst.StencilStoreOp = Override.AttachmentDesc.StencilStoreOp; break;
                    case RP_UNPACK_OVERRIDE_FLAG_INITIAL_STATE:    Dst.InitialState   = Override.AttachmentDesc.InitialState;   break;
                    case RP_UNPACK_OVERRIDE_FLAG_FINAL_STATE:      Dst.FinalState     = Override.AttachmentDesc.FinalState;     break;
                        // clang-format on
                    default:
                        UNEXPECTED("Unexpected RP unpack override flag");
                        break;
                }
            }
        }
    }

    DeArchiveInfo.pDevice->CreateRenderPass(RP.Desc, &pRP);
    if (!OverrideAttachments)
        CacheResource(DeArchiveInfo.Name, m_RenderPassMap, m_RenderPassMapGuard, pRP);
}

void DeviceObjectArchiveBase::UnpackResourceSignatureImpl(const ResourceSignatureUnpackInfo& DeArchiveInfo,
                                                          IPipelineResourceSignature*&       pSignature,
                                                          const CreateSignatureType&         CreateSignature)
{
    if (GetCachedResource(DeArchiveInfo.Name, m_PRSMap, m_PRSMapGuard, &pSignature))
        return;

    PRSData PRS{GetRawAllocator()};
    if (!ReadPRSData(DeArchiveInfo.Name, PRS))
        return;

    PRS.Desc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;

    LoadDeviceSpecificData(
        *PRS.pHeader,
        PRS.Allocator,
        "Resource signature",
        GetBlockOffsetType(),
        [&](void* pData, size_t DataSize) //
        {
            Serializer<SerializerMode::Read> Ser{pData, DataSize};

            CreateSignature(PRS, Ser, pSignature);
            CacheResource(DeArchiveInfo.Name, m_PRSMap, m_PRSMapGuard, pSignature);
        });
}

void DeviceObjectArchiveBase::ClearResourceCache()
{
    std::unique_lock<std::mutex> WriteLock{m_ShadersGuard};

    for (auto& Shader : m_Shaders)
    {
        Shader.Cache.Release();
    }
}


template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeImmutableSampler(
    Serializer<Mode>&            Ser,
    TQual<ImmutableSamplerDesc>& SampDesc)
{
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

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(ImmutableSamplerDesc) == 72, "Did you add a new member to ImmutableSamplerDesc? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializePRS(
    Serializer<Mode>&                               Ser,
    TQual<PipelineResourceSignatureDesc>&           Desc,
    TQual<PipelineResourceSignatureSerializedData>& Serialized,
    DynamicLinearAllocator*                         Allocator)
{
    // Serialize PipelineResourceSignatureDesc
    Ser(Desc.NumResources,
        Desc.NumImmutableSamplers,
        Desc.BindingIndex,
        Desc.UseCombinedTextureSamplers,
        Desc.CombinedSamplerSuffix);
    // skip Name
    // skip SRBAllocationGranularity

    auto* pResources = PSOSerializer_ArrayHelper<Mode>::Create(Desc.Resources, Desc.NumResources, Allocator);
    for (Uint32 r = 0; r < Desc.NumResources; ++r)
    {
        // Serialize PipelineResourceDesc
        auto& ResDesc = pResources[r];
        Ser(ResDesc.Name, // AZ TODO: global cache for names ?
            ResDesc.ShaderStages,
            ResDesc.ArraySize,
            ResDesc.ResourceType,
            ResDesc.VarType,
            ResDesc.Flags);
    }

    auto* pImmutableSamplers = PSOSerializer_ArrayHelper<Mode>::Create(Desc.ImmutableSamplers, Desc.NumImmutableSamplers, Allocator);
    for (Uint32 s = 0; s < Desc.NumImmutableSamplers; ++s)
    {
        // Serialize ImmutableSamplerDesc
        auto& SampDesc = pImmutableSamplers[s];
        SerializeImmutableSampler(Ser, SampDesc);
    }

    // Serialize PipelineResourceSignatureSerializedData
    Ser(Serialized.ShaderStages,
        Serialized.StaticResShaderStages,
        Serialized.PipelineType,
        Serialized.StaticResStageIndex);

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(PipelineResourceSignatureDesc) == 56, "Did you add a new member to PipelineResourceSignatureDesc? Please add serialization here.");
    static_assert(sizeof(PipelineResourceDesc) == 24, "Did you add a new member to PipelineResourceDesc? Please add serialization here.");
    static_assert(sizeof(PipelineResourceSignatureSerializedData) == 16, "Did you add a new member to PipelineResourceSignatureSerializedData? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializePSO(
    Serializer<Mode>&               Ser,
    TQual<PipelineStateCreateInfo>& CreateInfo,
    TQual<TPRSNames>&               PRSNames,
    DynamicLinearAllocator*         Allocator)
{
    // Serialize PipelineStateCreateInfo
    //   Serialize PipelineStateDesc
    Ser(CreateInfo.PSODesc.PipelineType);
    Ser(CreateInfo.ResourceSignaturesCount,
        CreateInfo.Flags);
    // skip SRBAllocationGranularity
    // skip ImmediateContextMask
    // skip pPSOCache

    // instead of ppResourceSignatures
    for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
    {
        Ser(PRSNames[i]);
    }

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(ShaderResourceVariableDesc) == 24, "Did you add a new member to ShaderResourceVariableDesc? Please add serialization here.");
    static_assert(sizeof(PipelineStateCreateInfo) == 96, "Did you add a new member to PipelineStateCreateInfo? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeGraphicsPSO(
    Serializer<Mode>&                       Ser,
    TQual<GraphicsPipelineStateCreateInfo>& CreateInfo,
    TQual<TPRSNames>&                       PRSNames,
    TQual<const char*>&                     RenderPassName,
    DynamicLinearAllocator*                 Allocator)
{
    SerializePSO(Ser, CreateInfo, PRSNames, Allocator);

    // Serialize GraphicsPipelineDesc
    Ser(CreateInfo.GraphicsPipeline.BlendDesc,
        CreateInfo.GraphicsPipeline.SampleMask,
        CreateInfo.GraphicsPipeline.RasterizerDesc,
        CreateInfo.GraphicsPipeline.DepthStencilDesc);
    //   Serialize InputLayoutDesc
    {
        auto& InputLayout = CreateInfo.GraphicsPipeline.InputLayout;
        Ser(InputLayout.NumElements);
        auto* pLayoutElements = PSOSerializer_ArrayHelper<Mode>::Create(InputLayout.LayoutElements, InputLayout.NumElements, Allocator);
        for (Uint32 i = 0; i < InputLayout.NumElements; ++i)
        {
            // Serialize LayoutElement
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
        CreateInfo.GraphicsPipeline.SmplDesc,
        RenderPassName); // for CreateInfo.GraphicsPipeline.pRenderPass

    // skip NodeMask
    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(GraphicsPipelineStateCreateInfo) == 344, "Did you add a new member to GraphicsPipelineStateCreateInfo? Please add serialization here.");
    static_assert(sizeof(LayoutElement) == 40, "Did you add a new member to LayoutElement? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeComputePSO(
    Serializer<Mode>&                      Ser,
    TQual<ComputePipelineStateCreateInfo>& CreateInfo,
    TQual<TPRSNames>&                      PRSNames,
    DynamicLinearAllocator*                Allocator)
{
    SerializePSO(Ser, CreateInfo, PRSNames, Allocator);

    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(ComputePipelineStateCreateInfo) == 104, "Did you add a new member to ComputePipelineStateCreateInfo? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeTilePSO(
    Serializer<Mode>&                   Ser,
    TQual<TilePipelineStateCreateInfo>& CreateInfo,
    TQual<TPRSNames>&                   PRSNames,
    DynamicLinearAllocator*             Allocator)
{
    SerializePSO(Ser, CreateInfo, PRSNames, Allocator);

    // AZ TODO: read TilePipelineStateCreateInfo

    // skip NodeMask
    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(TilePipelineStateCreateInfo) == 128, "Did you add a new member to TilePipelineStateCreateInfo? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeRayTracingPSO(
    Serializer<Mode>&                                     Ser,
    TQual<RayTracingPipelineStateCreateInfo>&             CreateInfo,
    TQual<TPRSNames>&                                     PRSNames,
    const std::function<void(Uint32&, TQual<IShader*>&)>& ShaderToIndex,
    DynamicLinearAllocator*                               Allocator)
{
    const bool IsReading = (Allocator != nullptr);
    const bool IsWriting = !IsReading;

    SerializePSO(Ser, CreateInfo, PRSNames, Allocator);

    // Serialize RayTracingPipelineDesc
    Ser(CreateInfo.RayTracingPipeline.ShaderRecordSize,
        CreateInfo.RayTracingPipeline.MaxRecursionDepth);

    // Serialize RayTracingPipelineStateCreateInfo
    Ser(CreateInfo.pShaderRecordName,
        CreateInfo.MaxAttributeSize,
        CreateInfo.MaxPayloadSize);

    //  Serialize RayTracingGeneralShaderGroup
    {
        Ser(CreateInfo.GeneralShaderCount);
        auto* pGeneralShaders = PSOSerializer_ArrayHelper<Mode>::Create(CreateInfo.pGeneralShaders, CreateInfo.GeneralShaderCount, Allocator);
        for (Uint32 i = 0; i < CreateInfo.GeneralShaderCount; ++i)
        {
            auto&  Group       = pGeneralShaders[i];
            Uint32 ShaderIndex = ~0u;
            if (IsWriting)
            {
                ShaderToIndex(ShaderIndex, Group.pShader);
            }
            Ser(Group.Name, ShaderIndex);
            VERIFY_EXPR(ShaderIndex != ~0u);
            if (IsReading)
            {
                ShaderToIndex(ShaderIndex, Group.pShader);
            }
        }
    }
    //  Serialize RayTracingTriangleHitShaderGroup
    {
        Ser(CreateInfo.TriangleHitShaderCount);
        auto* pTriangleHitShaders = PSOSerializer_ArrayHelper<Mode>::Create(CreateInfo.pTriangleHitShaders, CreateInfo.TriangleHitShaderCount, Allocator);
        for (Uint32 i = 0; i < CreateInfo.TriangleHitShaderCount; ++i)
        {
            auto&  Group                 = pTriangleHitShaders[i];
            Uint32 ClosestHitShaderIndex = ~0u;
            Uint32 AnyHitShaderIndex     = ~0u;
            if (IsWriting)
            {
                ShaderToIndex(ClosestHitShaderIndex, Group.pClosestHitShader);
                ShaderToIndex(AnyHitShaderIndex, Group.pAnyHitShader);
            }
            Ser(Group.Name, ClosestHitShaderIndex, AnyHitShaderIndex);
            VERIFY_EXPR(ClosestHitShaderIndex != ~0u);
            if (IsReading)
            {
                ShaderToIndex(ClosestHitShaderIndex, Group.pClosestHitShader);
                ShaderToIndex(AnyHitShaderIndex, Group.pAnyHitShader);
            }
        }
    }
    //  Serialize RayTracingProceduralHitShaderGroup
    {
        Ser(CreateInfo.ProceduralHitShaderCount);
        auto* pProceduralHitShaders = PSOSerializer_ArrayHelper<Mode>::Create(CreateInfo.pProceduralHitShaders, CreateInfo.ProceduralHitShaderCount, Allocator);
        for (Uint32 i = 0; i < CreateInfo.ProceduralHitShaderCount; ++i)
        {
            auto&  Group                   = pProceduralHitShaders[i];
            Uint32 IntersectionShaderIndex = ~0u;
            Uint32 ClosestHitShaderIndex   = ~0u;
            Uint32 AnyHitShaderIndex       = ~0u;
            if (IsWriting)
            {
                ShaderToIndex(IntersectionShaderIndex, Group.pIntersectionShader);
                ShaderToIndex(ClosestHitShaderIndex, Group.pClosestHitShader);
                ShaderToIndex(AnyHitShaderIndex, Group.pAnyHitShader);
            }
            Ser(Group.Name, IntersectionShaderIndex, ClosestHitShaderIndex, AnyHitShaderIndex);
            VERIFY_EXPR(IntersectionShaderIndex != ~0u);
            if (IsReading)
            {
                ShaderToIndex(IntersectionShaderIndex, Group.pIntersectionShader);
                ShaderToIndex(ClosestHitShaderIndex, Group.pClosestHitShader);
                ShaderToIndex(AnyHitShaderIndex, Group.pAnyHitShader);
            }
        }
    }

    // skip NodeMask
    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(RayTracingPipelineStateCreateInfo) == 168, "Did you add a new member to RayTracingPipelineStateCreateInfo? Please add serialization here.");
    static_assert(sizeof(RayTracingGeneralShaderGroup) == 16, "Did you add a new member to RayTracingGeneralShaderGroup? Please add serialization here.");
    static_assert(sizeof(RayTracingTriangleHitShaderGroup) == 24, "Did you add a new member to RayTracingTriangleHitShaderGroup? Please add serialization here.");
    static_assert(sizeof(RayTracingProceduralHitShaderGroup) == 32, "Did you add a new member to RayTracingProceduralHitShaderGroup? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeRenderPass(
    Serializer<Mode>&       Ser,
    TQual<RenderPassDesc>&  RPDesc,
    DynamicLinearAllocator* Allocator)
{
    // Serialize RenderPassDesc
    Ser(RPDesc.AttachmentCount,
        RPDesc.SubpassCount,
        RPDesc.DependencyCount);

    auto* pAttachments = PSOSerializer_ArrayHelper<Mode>::Create(RPDesc.pAttachments, RPDesc.AttachmentCount, Allocator);
    for (Uint32 i = 0; i < RPDesc.AttachmentCount; ++i)
    {
        // Serialize RenderPassAttachmentDesc
        auto& Attachment = pAttachments[i];
        Ser(Attachment.Format,
            Attachment.SampleCount,
            Attachment.LoadOp,
            Attachment.StoreOp,
            Attachment.StencilLoadOp,
            Attachment.StencilStoreOp,
            Attachment.InitialState,
            Attachment.FinalState);
    }

    auto* pSubpasses = PSOSerializer_ArrayHelper<Mode>::Create(RPDesc.pSubpasses, RPDesc.SubpassCount, Allocator);
    for (Uint32 i = 0; i < RPDesc.SubpassCount; ++i)
    {
        // Serialize SubpassDesc
        auto& Subpass                   = pSubpasses[i];
        bool  HasResolveAttachments     = Subpass.pResolveAttachments != nullptr;
        bool  HasDepthStencilAttachment = Subpass.pDepthStencilAttachment != nullptr;
        bool  HasShadingRateAttachment  = Subpass.pShadingRateAttachment != nullptr;

        Ser(Subpass.InputAttachmentCount,
            Subpass.RenderTargetAttachmentCount,
            Subpass.PreserveAttachmentCount,
            HasResolveAttachments,
            HasDepthStencilAttachment,
            HasShadingRateAttachment);

        auto* pInputAttachments = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pInputAttachments, Subpass.InputAttachmentCount, Allocator);
        for (Uint32 j = 0; j < Subpass.InputAttachmentCount; ++j)
        {
            auto& InputAttach = pInputAttachments[j];
            Ser(InputAttach.AttachmentIndex,
                InputAttach.State);
        }

        auto* pRenderTargetAttachments = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pRenderTargetAttachments, Subpass.RenderTargetAttachmentCount, Allocator);
        for (Uint32 j = 0; j < Subpass.RenderTargetAttachmentCount; ++j)
        {
            auto& RTAttach = pRenderTargetAttachments[j];
            Ser(RTAttach.AttachmentIndex,
                RTAttach.State);
        }

        auto* pPreserveAttachments = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pPreserveAttachments, Subpass.PreserveAttachmentCount, Allocator);
        for (Uint32 j = 0; j < Subpass.PreserveAttachmentCount; ++j)
        {
            auto& Attach = pPreserveAttachments[j];
            Ser(Attach);
        }

        if (HasResolveAttachments)
        {
            auto* pResolveAttachments = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pResolveAttachments, Subpass.RenderTargetAttachmentCount, Allocator);
            for (Uint32 j = 0; j < Subpass.RenderTargetAttachmentCount; ++j)
            {
                auto& ResAttach = pResolveAttachments[j];
                Ser(ResAttach.AttachmentIndex,
                    ResAttach.State);
            }
        }
        if (HasDepthStencilAttachment)
        {
            auto* pDepthStencilAttachment = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pDepthStencilAttachment, 1, Allocator);
            Ser(pDepthStencilAttachment->AttachmentIndex,
                pDepthStencilAttachment->State);
        }
        if (HasShadingRateAttachment)
        {
            auto* pShadingRateAttachment = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pShadingRateAttachment, 1, Allocator);
            Ser(pShadingRateAttachment->Attachment.AttachmentIndex,
                pShadingRateAttachment->Attachment.State,
                pShadingRateAttachment->TileSize);
        }
    }

    auto* pDependencies = PSOSerializer_ArrayHelper<Mode>::Create(RPDesc.pDependencies, RPDesc.DependencyCount, Allocator);
    for (Uint32 i = 0; i < RPDesc.DependencyCount; ++i)
    {
        // Serialize SubpassDependencyDesc
        auto& Dep = pDependencies[i];
        Ser(Dep.SrcSubpass,
            Dep.DstSubpass,
            Dep.SrcStageMask,
            Dep.DstStageMask,
            Dep.SrcAccessMask,
            Dep.DstAccessMask);
    }

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(RenderPassDesc) == 56, "Did you add a new member to RenderPassDesc? Please add serialization here.");
    static_assert(sizeof(RenderPassAttachmentDesc) == 16, "Did you add a new member to RenderPassAttachmentDesc? Please add serialization here.");
    static_assert(sizeof(SubpassDesc) == 72, "Did you add a new member to SubpassDesc? Please add serialization here.");
    static_assert(sizeof(SubpassDependencyDesc) == 24, "Did you add a new member to SubpassDependencyDesc? Please add serialization here.");
    static_assert(sizeof(ShadingRateAttachment) == 16, "Did you add a new member to ShadingRateAttachment? Please add serialization here.");
    static_assert(sizeof(AttachmentReference) == 8, "Did you add a new member to AttachmentReference? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeShaders(
    Serializer<Mode>&        Ser,
    TQual<ShaderIndexArray>& Shaders,
    DynamicLinearAllocator*  Allocator)
{
    Ser(Shaders.Count);

    auto* pIndices = PSOSerializer_ArrayHelper<Mode>::Create(Shaders.pIndices, Shaders.Count, Allocator);
    for (Uint32 i = 0; i < Shaders.Count; ++i)
        Ser(pIndices[i]);
}

template struct PSOSerializer<SerializerMode::Read>;
template struct PSOSerializer<SerializerMode::Write>;
template struct PSOSerializer<SerializerMode::Measure>;


} // namespace Diligent
