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

DeviceObjectArchiveBase::DeviceObjectArchiveBase(IReferenceCounters* pRefCounters, IArchive* pSource, DeviceType DevType) :
    TObjectBase{pRefCounters},
    m_pSource{pSource},
    m_DevType{DevType}
{
    if (m_pSource == nullptr)
        LOG_ERROR_AND_THROW("pSource must not be null");

    // Read header
    ArchiveHeader Header{};
    {
        if (!m_pSource->Read(0, sizeof(Header), &Header))
        {
            LOG_ERROR_AND_THROW("Failed to read archive header");
        }
        if (Header.MagicNumber != HeaderMagicNumber)
        {
            LOG_ERROR_AND_THROW("Archive header magic number is incorrect");
        }
        if (Header.Version != HeaderVersion)
        {
            LOG_ERROR_AND_THROW("Archive header version (", Header.Version, ") is not supported, expected (", HeaderVersion, ")");
        }

        m_BaseOffsets = Header.BlockBaseOffsets;
    }

    // Read chunks
    std::vector<ChunkHeader> Chunks{Header.NumChunks};
    if (!m_pSource->Read(sizeof(Header), sizeof(Chunks[0]) * Chunks.size(), Chunks.data()))
    {
        LOG_ERROR_AND_THROW("Failed to read chunk headers");
    }

    std::bitset<static_cast<Uint32>(ChunkType::Count)> ProcessedBits{};
    for (const auto& Chunk : Chunks)
    {
        if (ProcessedBits[static_cast<Uint32>(Chunk.Type)])
        {
            LOG_ERROR_AND_THROW("Multiple chunks with the same types are not allowed");
        }
        ProcessedBits[static_cast<Uint32>(Chunk.Type)] = true;

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

void DeviceObjectArchiveBase::ReadArchiveDebugInfo(const ChunkHeader& Chunk) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::ArchiveDebugInfo);

    std::vector<Uint8> Data; // AZ TODO: optimize
    Data.resize(Chunk.Size);

    if (!m_pSource->Read(Chunk.Offset, Data.size(), Data.data()))
    {
        LOG_ERROR_AND_THROW("Failed to read archive debug info");
    }

    Serializer<SerializerMode::Read> Ser{Data.data(), Data.size()};

    const char* GitHash = nullptr;
    Ser(GitHash);

    VERIFY_EXPR(Ser.IsEnd());
    m_DebugInfo.GitHash = String{GitHash};
}

template <typename ResType>
void DeviceObjectArchiveBase::ReadNamedResources(const ChunkHeader& Chunk, TNameOffsetMap<ResType>& NameAndOffset, std::mutex& Guard) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::ResourceSignature ||
                Chunk.Type == ChunkType::GraphicsPipelineStates ||
                Chunk.Type == ChunkType::ComputePipelineStates ||
                Chunk.Type == ChunkType::RayTracingPipelineStates ||
                Chunk.Type == ChunkType::RenderPass);

    std::vector<Uint8> Data; // AZ TODO: optimize
    Data.resize(Chunk.Size);

    if (!m_pSource->Read(Chunk.Offset, Data.size(), Data.data()))
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
        if (DataOffsetArray[i] + DataSizeArray[i] > m_pSource->GetSize())
        {
            LOG_ERROR_AND_THROW("Failed to read archive data");
        }

        bool Inserted = NameAndOffset.emplace(String{NameDataPtr + Offset, NameLengthArray[i]}, FileOffsetAndSize{DataOffsetArray[i], DataSizeArray[i]}).second;
        DEV_CHECK_ERR(Inserted, "Each name in the resource names array must be unique");
        Offset += NameLengthArray[i];
    }
}

void DeviceObjectArchiveBase::ReadIndexedResources(const ChunkHeader& Chunk, TResourceOffsetAndSize& Resources, std::mutex& Guard) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::Shaders);
    VERIFY_EXPR(Chunk.Size == sizeof(ShadersDataHeader));

    ShadersDataHeader Header;
    if (!m_pSource->Read(Chunk.Offset, sizeof(Header), &Header))
    {
        LOG_ERROR_AND_THROW("Failed to read indexed resources info from archive");
    }

    DynamicLinearAllocator Allocator{GetRawAllocator()};
    LoadDeviceSpecificData(
        Header,
        Allocator,
        "Shader list",
        static_cast<BlockOffsetType>(m_DevType), // AZ TODO
        [&](const void* pData, size_t DataSize)  //
        {
            // AZ TODO: reuse allocated data
            VERIFY_EXPR(DataSize % sizeof(TResourceOffsetAndSize::value_type) == 0);
            const size_t Count = DataSize / sizeof(TResourceOffsetAndSize::value_type);

            std::unique_lock<std::mutex> WriteLock{Guard};
            Resources.resize(Count);
            std::memcpy(Resources.data(), pData, Resources.size() * sizeof(Resources[0]));
        });
}

template <typename ResType, typename FnType>
bool DeviceObjectArchiveBase::LoadResourceData(const TNameOffsetMap<ResType>& NameAndOffset,
                                               std::mutex&                    Guard,
                                               const String&                  ResourceName,
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
            LOG_ERROR_MESSAGE(ResTypeName, " with name '", ResourceName, "' is not present in archive");
            return false;
        }
        OffsetAndSize = Iter->second;
        ResName       = Iter->first.c_str();
    }

    const auto DataSize = OffsetAndSize.Size;
    void*      pData    = Allocator.Allocate(DataSize, DataPtrAlign);
    if (!m_pSource->Read(OffsetAndSize.Offset, DataSize, pData))
    {
        LOG_ERROR_MESSAGE("Failed to read ", ResTypeName, " with name '", ResourceName, "' data from archive");
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
    const auto BaseOffset = m_BaseOffsets[static_cast<Uint32>(BlockType)];
    if (BaseOffset > m_pSource->GetSize())
    {
        LOG_ERROR_MESSAGE("Required block is not exists in archive");
        return;
    }
    if (Header.GetSize(m_DevType) == 0)
    {
        LOG_ERROR_MESSAGE("Device specific data is not specified for ", ResTypeName);
        return;
    }
    if (BaseOffset + Header.GetEndOffset(m_DevType) > m_pSource->GetSize())
    {
        LOG_ERROR_MESSAGE("Invalid offset in archive");
        return;
    }

    const auto DataSize = Header.GetSize(m_DevType);
    auto*      pData    = Allocator.Allocate(DataSize, DataPtrAlign);
    if (!m_pSource->Read(BaseOffset + Header.GetOffset(m_DevType), DataSize, pData))
    {
        LOG_ERROR_MESSAGE("Failed to read resource signature data");
        return;
    }

    return Fn(pData, DataSize);
}

bool DeviceObjectArchiveBase::ReadPRSData(const String& Name, PRSData& PRS)
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
                LOG_ERROR_MESSAGE("Invalid PRS header in archive");
                return false;
            }

            SerializerImpl<SerializerMode::Read>::SerializePRS(Ser, PRS.Desc, PRS.Serialized, &PRS.Allocator);
            VERIFY_EXPR(Ser.IsEnd());
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadRPData(const String& Name, RPData& RP)
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
                LOG_ERROR_MESSAGE("Invalid render pass header in archive");
                return false;
            }

            SerializerImpl<SerializerMode::Read>::SerializeRenderPass(Ser, RP.Desc, &RP.Allocator);
            VERIFY_EXPR(Ser.IsEnd());
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadGraphicsPSOData(const String& Name, PSOData<GraphicsPipelineStateCreateInfo>& PSO)
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
                LOG_ERROR_MESSAGE("Invalid graphics pipeline header in archive");
                return false;
            }

            SerializerImpl<SerializerMode::Read>::SerializeGraphicsPSO(Ser, PSO.CreateInfo, PSO.PRSNames, PSO.RenderPassName, &PSO.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            // AZ TODO: required only if PSO has resource signatures
            PSO.CreateInfo.Flags |= PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES;
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadComputePSOData(const String& Name, PSOData<ComputePipelineStateCreateInfo>& PSO)
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
                LOG_ERROR_MESSAGE("Invalid compute pipeline header in archive");
                return false;
            }

            SerializerImpl<SerializerMode::Read>::SerializeComputePSO(Ser, PSO.CreateInfo, PSO.PRSNames, &PSO.Allocator);
            VERIFY_EXPR(Ser.IsEnd());
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadRayTracingPSOData(const String& Name, PSOData<RayTracingPipelineStateCreateInfo>& PSO)
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
                LOG_ERROR_MESSAGE("Invalid ray tracing pipeline header in archive");
                return false;
            }

            SerializerImpl<SerializerMode::Read>::SerializeRayTracingPSO(Ser, PSO.CreateInfo, PSO.PRSNames, &PSO.Allocator);
            VERIFY_EXPR(Ser.IsEnd());
            return true;
        });
}

template <typename ResType>
bool DeviceObjectArchiveBase::GetCachedResource(const String& Name, TNameOffsetMap<ResType>& Cache, std::mutex& Guard, ResType*& pResource)
{
    std::unique_lock<std::mutex> ReadLock{Guard};

    pResource = nullptr;

    auto Iter = Cache.find(Name);
    if (Iter == Cache.end())
        return false;

    auto Ptr = Iter->second.Cache.Lock();
    if (Ptr == nullptr)
        return false;

    pResource = Ptr.Detach();
    return true;
}

template <typename ResType>
void DeviceObjectArchiveBase::CacheResource(const String& Name, TNameOffsetMap<ResType>& Cache, std::mutex& Guard, ResType* pResource)
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

bool DeviceObjectArchiveBase::GetCachedPRS(const String& Name, IPipelineResourceSignature*& pSignature)
{
    return GetCachedResource(Name, m_PRSMap, m_PRSMapGuard, pSignature);
}

void DeviceObjectArchiveBase::CachePRSResource(const String& Name, IPipelineResourceSignature* pSignature)
{
    return CacheResource(Name, m_PRSMap, m_PRSMapGuard, pSignature);
}

bool DeviceObjectArchiveBase::GetCachedGraphicsPSO(const String& Name, IPipelineState*& pPSO)
{
    return GetCachedResource(Name, m_GraphicsPSOMap, m_GraphicsPSOMapGuard, pPSO);
}

void DeviceObjectArchiveBase::CacheGraphicsPSOResource(const String& Name, IPipelineState* pPSO)
{
    return CacheResource(Name, m_GraphicsPSOMap, m_GraphicsPSOMapGuard, pPSO);
}

bool DeviceObjectArchiveBase::GetCachedComputePSO(const String& Name, IPipelineState*& pPSO)
{
    return GetCachedResource(Name, m_ComputePSOMap, m_ComputePSOMapGuard, pPSO);
}

void DeviceObjectArchiveBase::CacheComputePSOResource(const String& Name, IPipelineState* pPSO)
{
    return CacheResource(Name, m_ComputePSOMap, m_ComputePSOMapGuard, pPSO);
}

bool DeviceObjectArchiveBase::GetCachedRayTracingPSO(const String& Name, IPipelineState*& pPSO)
{
    return GetCachedResource(Name, m_RayTracingPSOMap, m_RayTracingPSOMapGuard, pPSO);
}

void DeviceObjectArchiveBase::CacheRayTracingPSOResource(const String& Name, IPipelineState* pPSO)
{
    return CacheResource(Name, m_RayTracingPSOMap, m_RayTracingPSOMapGuard, pPSO);
}

bool DeviceObjectArchiveBase::GetCachedRP(const String& Name, IRenderPass*& pRP)
{
    return GetCachedResource(Name, m_RenderPassMap, m_RenderPassMapGuard, pRP);
}

void DeviceObjectArchiveBase::CacheRPResource(const String& Name, IRenderPass* pRP)
{
    return CacheResource(Name, m_RenderPassMap, m_RenderPassMapGuard, pRP);
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

    if (PSO.CreateInfo.pVS != nullptr)
        PSO.CreateInfo.pVS->Release();

    if (PSO.CreateInfo.pPS != nullptr)
        PSO.CreateInfo.pPS->Release();

    if (PSO.CreateInfo.pDS != nullptr)
        PSO.CreateInfo.pDS->Release();

    if (PSO.CreateInfo.pHS != nullptr)
        PSO.CreateInfo.pHS->Release();

    if (PSO.CreateInfo.pGS != nullptr)
        PSO.CreateInfo.pGS->Release();

    if (PSO.CreateInfo.pAS != nullptr)
        PSO.CreateInfo.pAS->Release();

    if (PSO.CreateInfo.pMS != nullptr)
        PSO.CreateInfo.pMS->Release();
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

    if (PSO.CreateInfo.pCS != nullptr)
        PSO.CreateInfo.pCS->Release();
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

    // AZ TODO: release shaders
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
        return true;

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

bool DeviceObjectArchiveBase::LoadShaders(Serializer<SerializerMode::Read>&    Ser,
                                          IRenderDevice*                       pDevice,
                                          std::vector<RefCntAutoPtr<IShader>>& Shaders)
{
    DynamicLinearAllocator Allocator{GetRawAllocator()};

    ShaderIndexArray ShaderIndices;
    SerializerImpl<SerializerMode::Read>::SerializeShaders(Ser, ShaderIndices, &Allocator);

    Shaders.resize(ShaderIndices.Count);

    const auto BaseOffset = m_BaseOffsets[static_cast<Uint32>(static_cast<BlockOffsetType>(m_DevType))]; // AZ TODO

    std::unique_lock<std::mutex> ReadLock{m_ShadersGuard};

    for (Uint32 i = 0; i < ShaderIndices.Count; ++i)
    {
        const Uint32 Idx = ShaderIndices.pIndices[i];
        if (Idx >= m_Shaders.size())
            return false;

        const auto& OffsetAndSize = m_Shaders[Idx];
        void*       pData         = Allocator.Allocate(OffsetAndSize.Size, DataPtrAlign);

        if (!m_pSource->Read(BaseOffset + OffsetAndSize.Offset, OffsetAndSize.Size, pData))
            return false;

        Serializer<SerializerMode::Read> Ser2{pData, OffsetAndSize.Size};
        ShaderCreateInfo                 ShaderCI;
        Ser2(ShaderCI.Desc.ShaderType, ShaderCI.EntryPoint, ShaderCI.SourceLanguage, ShaderCI.ShaderCompiler);

        if (m_DevType == DeviceType::OpenGL)
        {
            ShaderCI.Source                     = static_cast<const Char*>(Ser2.GetCurrentPtr());
            ShaderCI.SourceLength               = Ser2.GetRemainSize();
            ShaderCI.UseCombinedTextureSamplers = true;

            VERIFY_EXPR(ShaderCI.SourceLength == strlen(ShaderCI.Source) + 1);
        }
        else
        {
            VERIFY_EXPR(ShaderCI.SourceLanguage == SHADER_SOURCE_LANGUAGE_DEFAULT);
            VERIFY_EXPR(ShaderCI.ShaderCompiler == SHADER_COMPILER_DEFAULT);

            ShaderCI.ByteCode     = Ser2.GetCurrentPtr();
            ShaderCI.ByteCodeSize = Ser2.GetRemainSize();
        }

        pDevice->CreateShader(ShaderCI, &Shaders[i]);
        if (!Shaders[i])
            return false;

        // AZ TODO: cache shaders ?
    }
    return true;
}

void DeviceObjectArchiveBase::UnpackGraphicsPSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO)
{
    VERIFY_EXPR(DeArchiveInfo.pDevice != nullptr);

    if (GetCachedGraphicsPSO(DeArchiveInfo.Name, pPSO))
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
        static_cast<BlockOffsetType>(m_DevType), // AZ TODO
        [&](void* pData, size_t DataSize)        //
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
                    case SHADER_TYPE_VERTEX:        PSO.CreateInfo.pVS = Shader; Shader->AddRef(); break;
                    case SHADER_TYPE_PIXEL:         PSO.CreateInfo.pPS = Shader; Shader->AddRef(); break;
                    case SHADER_TYPE_GEOMETRY:      PSO.CreateInfo.pGS = Shader; Shader->AddRef(); break;
                    case SHADER_TYPE_HULL:          PSO.CreateInfo.pHS = Shader; Shader->AddRef(); break; 
                    case SHADER_TYPE_DOMAIN:        PSO.CreateInfo.pDS = Shader; Shader->AddRef(); break;
                    case SHADER_TYPE_AMPLIFICATION: PSO.CreateInfo.pAS = Shader; Shader->AddRef(); break;
                    case SHADER_TYPE_MESH:          PSO.CreateInfo.pMS = Shader; Shader->AddRef(); break;
                    // clang-format on
                    default:
                        LOG_ERROR_MESSAGE("Unsupported shader type for graphics pipeline");
                        return;
                }
            }

            DeArchiveInfo.pDevice->CreateGraphicsPipelineState(PSO.CreateInfo, &pPSO);
            CacheGraphicsPSOResource(DeArchiveInfo.Name, pPSO);
        });
}

void DeviceObjectArchiveBase::UnpackComputePSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO)
{
    VERIFY_EXPR(DeArchiveInfo.pDevice != nullptr);

    if (GetCachedComputePSO(DeArchiveInfo.Name, pPSO))
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
        static_cast<BlockOffsetType>(m_DevType), // AZ TODO
        [&](void* pData, size_t DataSize)        //
        {
            Serializer<SerializerMode::Read> Ser{pData, DataSize};
            // AZ TODO

            DeArchiveInfo.pDevice->CreateComputePipelineState(PSO.CreateInfo, &pPSO);
            CacheComputePSOResource(DeArchiveInfo.Name, pPSO);
        });
}

void DeviceObjectArchiveBase::UnpackRayTracingPSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO)
{
    VERIFY_EXPR(DeArchiveInfo.pDevice != nullptr);

    if (GetCachedRayTracingPSO(DeArchiveInfo.Name, pPSO))
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
        static_cast<BlockOffsetType>(m_DevType), // AZ TODO
        [&](void* pData, size_t DataSize)        //
        {
            Serializer<SerializerMode::Read> Ser{pData, DataSize};
            // AZ TODO

            DeArchiveInfo.pDevice->CreateRayTracingPipelineState(PSO.CreateInfo, &pPSO);
            CacheRayTracingPSOResource(DeArchiveInfo.Name, pPSO);
        });
}

void DeviceObjectArchiveBase::UnpackRenderPass(const RenderPassUnpackInfo& DeArchiveInfo, IRenderPass*& pRP)
{
    VERIFY_EXPR(DeArchiveInfo.pDevice != nullptr);

    if (GetCachedRP(DeArchiveInfo.Name, pRP))
        return;

    RPData RP{GetRawAllocator()};
    if (!ReadRPData(DeArchiveInfo.Name, RP))
        return;

    DeArchiveInfo.pDevice->CreateRenderPass(RP.Desc, &pRP);
    CacheRPResource(DeArchiveInfo.Name, pRP);
}

void DeviceObjectArchiveBase::UnpackResourceSignatureImpl(const ResourceSignatureUnpackInfo& DeArchiveInfo,
                                                          IPipelineResourceSignature*&       pSignature,
                                                          const CreateSignatureType&         CreateSignature)
{
    if (GetCachedPRS(DeArchiveInfo.Name, pSignature))
        return;

    PRSData PRS{GetRawAllocator()};
    if (!ReadPRSData(DeArchiveInfo.Name, PRS))
        return;

    PRS.Desc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;

    LoadDeviceSpecificData(
        *PRS.pHeader,
        PRS.Allocator,
        "Resource signature",
        static_cast<BlockOffsetType>(m_DevType), // AZ TODO
        [&](void* pData, size_t DataSize)        //
        {
            Serializer<SerializerMode::Read> Ser{pData, DataSize};

            CreateSignature(PRS, Ser, pSignature);
            CachePRSResource(DeArchiveInfo.Name, pSignature);
        });
}

template <SerializerMode Mode>
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeImmutableSampler(
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
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializePRS(
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

    auto* pResources = ArraySerializerHelper<Mode>::Create(Desc.Resources, Desc.NumResources, Allocator);
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

    auto* pImmutableSamplers = ArraySerializerHelper<Mode>::Create(Desc.ImmutableSamplers, Desc.NumImmutableSamplers, Allocator);
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
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializePSO(
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

    //   Serialize PipelineResourceLayoutDesc
    {
        auto& ResLayout = CreateInfo.PSODesc.ResourceLayout;
        Ser(ResLayout.DefaultVariableType,
            ResLayout.DefaultVariableMergeStages,
            ResLayout.NumVariables,
            ResLayout.NumImmutableSamplers);

        auto* pVariables = ArraySerializerHelper<Mode>::Create(ResLayout.Variables, ResLayout.NumVariables, Allocator);
        for (Uint32 i = 0; i < ResLayout.NumVariables; ++i)
        {
            // Serialize ShaderResourceVariableDesc
            auto& Var = pVariables[i];
            Ser(Var.ShaderStages,
                Var.Name,
                Var.Type,
                Var.Flags);
        }
        auto* pImmutableSamplers = ArraySerializerHelper<Mode>::Create(ResLayout.ImmutableSamplers, ResLayout.NumImmutableSamplers, Allocator);
        for (Uint32 i = 0; i < ResLayout.NumImmutableSamplers; ++i)
        {
            // Serialize ImmutableSamplerDesc
            auto& SampDesc = pImmutableSamplers[i];
            SerializeImmutableSampler(Ser, SampDesc);
        }
    }

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(ShaderResourceVariableDesc) == 24, "Did you add a new member to ShaderResourceVariableDesc? Please add serialization here.");
    static_assert(sizeof(PipelineStateCreateInfo) == 96, "Did you add a new member to PipelineStateCreateInfo? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeGraphicsPSO(
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
        auto* pLayoutElements = ArraySerializerHelper<Mode>::Create(InputLayout.LayoutElements, InputLayout.NumElements, Allocator);
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
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeComputePSO(
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
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeTilePSO(
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
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeRayTracingPSO(
    Serializer<Mode>&                         Ser,
    TQual<RayTracingPipelineStateCreateInfo>& CreateInfo,
    TQual<TPRSNames>&                         PRSNames,
    DynamicLinearAllocator*                   Allocator)
{
    SerializePSO(Ser, CreateInfo, PRSNames, Allocator);

    // AZ TODO: read RayTracingPipelineStateCreateInfo

    // skip NodeMask
    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(RayTracingPipelineStateCreateInfo) == 168, "Did you add a new member to RayTracingPipelineStateCreateInfo? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeRenderPass(
    Serializer<Mode>&       Ser,
    TQual<RenderPassDesc>&  RPDesc,
    DynamicLinearAllocator* Allocator)
{
    // Serialize RenderPassDesc
    Ser(RPDesc.AttachmentCount,
        RPDesc.SubpassCount,
        RPDesc.DependencyCount);

    auto* pAttachments = ArraySerializerHelper<Mode>::Create(RPDesc.pAttachments, RPDesc.AttachmentCount, Allocator);
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

    auto* pSubpasses = ArraySerializerHelper<Mode>::Create(RPDesc.pSubpasses, RPDesc.SubpassCount, Allocator);
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

        auto* pInputAttachments = ArraySerializerHelper<Mode>::Create(Subpass.pInputAttachments, Subpass.InputAttachmentCount, Allocator);
        for (Uint32 j = 0; j < Subpass.InputAttachmentCount; ++j)
        {
            auto& InputAttach = pInputAttachments[j];
            Ser(InputAttach.AttachmentIndex,
                InputAttach.State);
        }

        auto* pRenderTargetAttachments = ArraySerializerHelper<Mode>::Create(Subpass.pRenderTargetAttachments, Subpass.RenderTargetAttachmentCount, Allocator);
        for (Uint32 j = 0; j < Subpass.RenderTargetAttachmentCount; ++j)
        {
            auto& RTAttach = pRenderTargetAttachments[j];
            Ser(RTAttach.AttachmentIndex,
                RTAttach.State);
        }

        auto* pPreserveAttachments = ArraySerializerHelper<Mode>::Create(Subpass.pPreserveAttachments, Subpass.PreserveAttachmentCount, Allocator);
        for (Uint32 j = 0; j < Subpass.PreserveAttachmentCount; ++j)
        {
            auto& Attach = pPreserveAttachments[j];
            Ser(Attach);
        }

        if (HasResolveAttachments)
        {
            auto* pResolveAttachments = ArraySerializerHelper<Mode>::Create(Subpass.pResolveAttachments, Subpass.RenderTargetAttachmentCount, Allocator);
            for (Uint32 j = 0; j < Subpass.RenderTargetAttachmentCount; ++j)
            {
                auto& ResAttach = pResolveAttachments[j];
                Ser(ResAttach.AttachmentIndex,
                    ResAttach.State);
            }
        }
        if (HasDepthStencilAttachment)
        {
            auto* pDepthStencilAttachment = ArraySerializerHelper<Mode>::Create(Subpass.pDepthStencilAttachment, 1, Allocator);
            Ser(pDepthStencilAttachment->AttachmentIndex,
                pDepthStencilAttachment->State);
        }
        if (HasShadingRateAttachment)
        {
            auto* pShadingRateAttachment = ArraySerializerHelper<Mode>::Create(Subpass.pShadingRateAttachment, 1, Allocator);
            Ser(pShadingRateAttachment->Attachment.AttachmentIndex,
                pShadingRateAttachment->Attachment.State,
                pShadingRateAttachment->TileSize);
        }
    }

    auto* pDependencies = ArraySerializerHelper<Mode>::Create(RPDesc.pDependencies, RPDesc.DependencyCount, Allocator);
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
void DeviceObjectArchiveBase::SerializerImpl<Mode>::SerializeShaders(
    Serializer<Mode>&        Ser,
    TQual<ShaderIndexArray>& Shaders,
    DynamicLinearAllocator*  Allocator)
{
    Ser(Shaders.Count);

    auto* pIndices = ArraySerializerHelper<Mode>::Create(Shaders.pIndices, Shaders.Count, Allocator);
    for (Uint32 i = 0; i < Shaders.Count; ++i)
        Ser(pIndices[i]);
}

template struct DeviceObjectArchiveBase::SerializerImpl<SerializerMode::Read>;
template struct DeviceObjectArchiveBase::SerializerImpl<SerializerMode::Write>;
template struct DeviceObjectArchiveBase::SerializerImpl<SerializerMode::Measure>;


} // namespace Diligent
