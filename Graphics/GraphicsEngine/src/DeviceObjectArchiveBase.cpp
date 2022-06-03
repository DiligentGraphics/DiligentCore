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

#include "DeviceObjectArchiveBase.hpp"

#include <bitset>
#include <vector>
#include <unordered_set>
#include <algorithm>

#include "DebugUtilities.hpp"
#include "PSOSerializer.hpp"

namespace Diligent
{

template <typename ResType>
void DeviceObjectArchiveBase::OffsetSizeAndResourceMap<ResType>::Insert(const char* Name, Uint32 Offset, Uint32 Size)
{
    std::unique_lock<std::mutex> Lock{m_Mtx};

    bool Inserted = m_Map.emplace(HashMapStringKey{Name, true}, FileOffsetAndSize{Offset, Size}).second;
    DEV_CHECK_ERR(Inserted, "Each name in the resource map must be unique");
}


template <typename ResType>
DeviceObjectArchiveBase::FileOffsetAndSize DeviceObjectArchiveBase::OffsetSizeAndResourceMap<ResType>::GetOffsetAndSize(const char* Name, const char*& StoredNamePtr)
{
    std::unique_lock<std::mutex> Lock{m_Mtx};

    auto it = m_Map.find(Name);
    if (it != m_Map.end())
    {
        StoredNamePtr = it->first.GetStr();
        return it->second;
    }
    else
    {
        StoredNamePtr = nullptr;
        return FileOffsetAndSize::Invalid();
    }
}

template <typename ResType>
bool DeviceObjectArchiveBase::OffsetSizeAndResourceMap<ResType>::GetResource(const char* Name, ResType** ppResource)
{
    VERIFY_EXPR(Name != nullptr);
    VERIFY_EXPR(ppResource != nullptr && *ppResource == nullptr);
    *ppResource = nullptr;

    std::unique_lock<std::mutex> Lock{m_Mtx};

    auto it = m_Map.find(Name);
    if (it == m_Map.end())
        return false;

    auto Ptr = it->second.pRes.Lock();
    if (Ptr == nullptr)
        return false;

    *ppResource = Ptr.Detach();
    return true;
}

template <typename ResType>
void DeviceObjectArchiveBase::OffsetSizeAndResourceMap<ResType>::SetResource(const char* Name, ResType* pResource)
{
    VERIFY_EXPR(Name != nullptr && Name[0] != '\0');
    VERIFY_EXPR(pResource != nullptr);

    std::unique_lock<std::mutex> Lock{m_Mtx};

    auto it = m_Map.find(Name);
    if (it == m_Map.end())
        return;

    if (it->second.pRes.IsValid())
        return;

    it->second.pRes = pResource;
}

template <typename ResType>
void DeviceObjectArchiveBase::OffsetSizeAndResourceMap<ResType>::ReleaseResources()
{
    std::unique_lock<std::mutex> Lock{m_Mtx};
    for (auto& it : m_Map)
        it.second.pRes.Release();
}

// Instantiation is required by UnpackResourceSignatureImpl
template class DeviceObjectArchiveBase::OffsetSizeAndResourceMap<IPipelineResourceSignature>;

DeviceObjectArchiveBase::DeviceObjectArchiveBase(IReferenceCounters* pRefCounters, IArchive* pArchive) :
    TObjectBase{pRefCounters},
    m_pArchive{pArchive}
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
        if (Header.Version != HeaderVersion)
        {
            LOG_ERROR_AND_THROW("Archive version (", Header.Version, ") is not supported; expected version: ", Uint32{HeaderVersion}, ".");
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
            LOG_ERROR_AND_THROW("Multiple chunks with the same type are not allowed");
        }
        ProcessedBits[static_cast<size_t>(Chunk.Type)] = true;

        static_assert(static_cast<size_t>(ChunkType::Count) == 9, "Please handle the new chunk type below");
        switch (Chunk.Type)
        {
            // clang-format off
            case ChunkType::ArchiveDebugInfo:         ReadArchiveDebugInfo(Chunk);                     break;
            case ChunkType::ResourceSignature:        ReadNamedResources  (Chunk, m_PRSMap);           break;
            case ChunkType::GraphicsPipelineStates:   ReadNamedResources  (Chunk, m_GraphicsPSOMap);   break;
            case ChunkType::ComputePipelineStates:    ReadNamedResources  (Chunk, m_ComputePSOMap);    break;
            case ChunkType::RayTracingPipelineStates: ReadNamedResources  (Chunk, m_RayTracingPSOMap); break;
            case ChunkType::TilePipelineStates:       ReadNamedResources  (Chunk, m_TilePSOMap);       break;
            case ChunkType::RenderPass:               ReadNamedResources  (Chunk, m_RenderPassMap);    break;
            case ChunkType::Shaders:                  ReadShadersHeader   (Chunk);                     break;
            // clang-format on
            default:
                LOG_ERROR_AND_THROW("Unknown chunk type (", static_cast<Uint32>(Chunk.Type), ")");
        }
    }
}

DeviceObjectArchiveBase::DeviceType DeviceObjectArchiveBase::RenderDeviceTypeToArchiveDeviceType(RENDER_DEVICE_TYPE Type)
{
    static_assert(RENDER_DEVICE_TYPE_COUNT == 7, "Did you add a new render device type? Please handle it here.");
    switch (Type)
    {
        // clang-format off
        case RENDER_DEVICE_TYPE_D3D11:  return DeviceObjectArchiveBase::DeviceType::Direct3D11;
        case RENDER_DEVICE_TYPE_D3D12:  return DeviceObjectArchiveBase::DeviceType::Direct3D12;
        case RENDER_DEVICE_TYPE_GL:     return DeviceObjectArchiveBase::DeviceType::OpenGL;
        case RENDER_DEVICE_TYPE_GLES:   return DeviceObjectArchiveBase::DeviceType::OpenGL;
        case RENDER_DEVICE_TYPE_VULKAN: return DeviceObjectArchiveBase::DeviceType::Vulkan;
#if PLATFORM_MACOS
        case RENDER_DEVICE_TYPE_METAL:  return DeviceObjectArchiveBase::DeviceType::Metal_MacOS;
#elif PLATFORM_IOS || PLATFORM_TVOS
        case RENDER_DEVICE_TYPE_METAL:  return DeviceObjectArchiveBase::DeviceType::Metal_iOS;
#endif
        // clang-format on
        default:
            UNEXPECTED("Unexpected device type");
            return DeviceObjectArchiveBase::DeviceType::Count;
    }
}

DeviceObjectArchiveBase::DeviceType DeviceObjectArchiveBase::GetArchiveDeviceType(const IRenderDevice* pDevice)
{
    VERIFY_EXPR(pDevice != nullptr);
    const auto Type = pDevice->GetDeviceInfo().Type;
    return RenderDeviceTypeToArchiveDeviceType(Type);
}

DeviceObjectArchiveBase::BlockOffsetType DeviceObjectArchiveBase::GetBlockOffsetType(DeviceType DevType)
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

const char* DeviceObjectArchiveBase::ChunkTypeToResName(ChunkType Type)
{
    switch (Type)
    {
        // clang-format off
        case ChunkType::Undefined:                return "Undefined";
        case ChunkType::ArchiveDebugInfo:         return "Debug Info";
        case ChunkType::ResourceSignature:        return "Resource Signature";
        case ChunkType::GraphicsPipelineStates:   return "Graphics Pipeline";
        case ChunkType::ComputePipelineStates:    return "Compute Pipeline";
        case ChunkType::RayTracingPipelineStates: return "Ray-Tracing Pipeline";
        case ChunkType::TilePipelineStates:       return "Tile Pipeline";
        case ChunkType::RenderPass:               return "Render Pass";
        case ChunkType::Shaders:                  return "Shader";
        // clang-format on
        default:
            UNEXPECTED("Unexpected chunk type");
            return "";
    }
}

void DeviceObjectArchiveBase::ReadArchiveDebugInfo(const ChunkHeader& Chunk) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::ArchiveDebugInfo);

    SerializedData Data{Chunk.Size, GetRawAllocator()};
    if (!m_pArchive->Read(Chunk.Offset, Data.Size(), Data.Ptr()))
    {
        LOG_ERROR_AND_THROW("Failed to read archive debug info");
    }

    Serializer<SerializerMode::Read> Ser{Data};

    Ser(m_DebugInfo.APIVersion);

    const char* GitHash = nullptr;
    Ser(GitHash);

    VERIFY_EXPR(Ser.IsEnded());
    m_DebugInfo.GitHash = String{GitHash};

    if (m_DebugInfo.APIVersion != DILIGENT_API_VERSION)
        LOG_INFO_MESSAGE("Archive was created with Engine API version (", m_DebugInfo.APIVersion, ") but is used with (", DILIGENT_API_VERSION, ")");
#ifdef DILIGENT_CORE_COMMIT_HASH
    if (m_DebugInfo.GitHash != DILIGENT_CORE_COMMIT_HASH)
        LOG_INFO_MESSAGE("Archive was built with Diligent Core git hash '", m_DebugInfo.GitHash, "' but is used with '", DILIGENT_CORE_COMMIT_HASH, "'.");
#endif
}

template <typename ResType>
void DeviceObjectArchiveBase::ReadNamedResources(const ChunkHeader& Chunk, OffsetSizeAndResourceMap<ResType>& NameAndOffset) noexcept(false)
{
    ReadNamedResources(m_pArchive, Chunk,
                       [&NameAndOffset](const char* Name, Uint32 Offset, Uint32 Size) //
                       {
                           NameAndOffset.Insert(Name, Offset, Size);
                       });
}

void DeviceObjectArchiveBase::ReadShadersHeader(const ChunkHeader& Chunk) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::Shaders);
    VERIFY_EXPR(Chunk.Size == sizeof(m_ShadersHeader));

    if (!m_pArchive->Read(Chunk.Offset, sizeof(m_ShadersHeader), &m_ShadersHeader))
    {
        LOG_ERROR_AND_THROW("Failed to read indexed resources info from the archive");
    }
}

DeviceObjectArchiveBase::ShaderDeviceInfo& DeviceObjectArchiveBase::GetShaderDeviceInfo(DeviceType DevType, DynamicLinearAllocator& Allocator) noexcept(false)
{
    auto& ShaderInfo = m_ShaderInfo[static_cast<size_t>(DevType)];

    {
        std::unique_lock<std::mutex> Lock{ShaderInfo.Mtx};
        if (!ShaderInfo.OffsetsAndCache.empty())
            return ShaderInfo;
    }

    if (const auto ShaderData = GetDeviceSpecificData(DevType, m_ShadersHeader, Allocator, "Shader list"))
    {
        VERIFY_EXPR(ShaderData.Size() % sizeof(FileOffsetAndSize) == 0);
        const size_t Count = ShaderData.Size() / sizeof(FileOffsetAndSize);

        const auto* pFileOffsetAndSize = ShaderData.Ptr<const FileOffsetAndSize>();

        std::unique_lock<std::mutex> WriteLock{ShaderInfo.Mtx};
        ShaderInfo.OffsetsAndCache.reserve(Count);
        for (Uint32 i = 0; i < Count; ++i)
            ShaderInfo.OffsetsAndCache.emplace_back(pFileOffsetAndSize[i]);
    }

    return ShaderInfo;
}

template <typename ResType, typename ReourceDataType>
bool DeviceObjectArchiveBase::LoadResourceData(OffsetSizeAndResourceMap<ResType>& ResourceMap,
                                               const char*                        ResourceName,
                                               ReourceDataType&                   ResData)
{
    const char* StoredResourceName = nullptr;

    const auto OffsetAndSize = ResourceMap.GetOffsetAndSize(ResourceName, StoredResourceName);
    if (OffsetAndSize == FileOffsetAndSize::Invalid())
    {
        LOG_ERROR_MESSAGE(ChunkTypeToResName(ResData.ExpectedChunkType), " with name '", ResourceName, "' is not present in the archive");
        return false;
    }
    VERIFY_EXPR(StoredResourceName != nullptr && StoredResourceName != ResourceName && strcmp(ResourceName, StoredResourceName) == 0);

    const auto DataSize = OffsetAndSize.Size;
    void*      pData    = ResData.Allocator.Allocate(DataSize, DataPtrAlign);
    if (!m_pArchive->Read(OffsetAndSize.Offset, DataSize, pData))
    {
        LOG_ERROR_MESSAGE("Failed to read ", ChunkTypeToResName(ResData.ExpectedChunkType), " with name '", ResourceName, "' data from the archive");
        return false;
    }

    Serializer<SerializerMode::Read> Ser{SerializedData{pData, DataSize}};

    using HeaderType = typename std::remove_reference<decltype(*ResData.pHeader)>::type;
    ResData.pHeader  = Ser.Cast<HeaderType>();
    if (ResData.pHeader->Type != ResData.ExpectedChunkType)
    {
        LOG_ERROR_MESSAGE("Invalid chunk header: ", ChunkTypeToResName(ResData.pHeader->Type),
                          "; expected: ", ChunkTypeToResName(ResData.ExpectedChunkType), ".");
        return false;
    }

    auto Res = ResData.Deserialize(StoredResourceName, Ser);
    VERIFY_EXPR(Ser.IsEnded());
    return Res;
}

// Instantiation is required by UnpackResourceSignatureImpl
template bool DeviceObjectArchiveBase::LoadResourceData<IPipelineResourceSignature, DeviceObjectArchiveBase::PRSData>(
    OffsetSizeAndResourceMap<IPipelineResourceSignature>& ResourceMap,
    const char*                                           ResourceName,
    PRSData&                                              ResData);

template <typename HeaderType>
SerializedData DeviceObjectArchiveBase::GetDeviceSpecificData(DeviceType              DevType,
                                                              const HeaderType&       Header,
                                                              DynamicLinearAllocator& Allocator,
                                                              const char*             ResTypeName)
{
    const auto   BlockType   = GetBlockOffsetType(DevType);
    const Uint64 BaseOffset  = m_BaseOffsets[static_cast<size_t>(BlockType)];
    const auto   ArchiveSize = m_pArchive->GetSize();
    if (BaseOffset > ArchiveSize)
    {
        LOG_ERROR_MESSAGE("Required block does not exist in archive");
        return {};
    }
    if (Header.GetSize(DevType) == 0)
    {
        LOG_ERROR_MESSAGE("Device specific data is not specified for ", ResTypeName);
        return {};
    }
    if (BaseOffset + Header.GetEndOffset(DevType) > ArchiveSize)
    {
        LOG_ERROR_MESSAGE("Invalid offset in the archive");
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

// Instantiation is required by UnpackResourceSignatureImpl
template SerializedData DeviceObjectArchiveBase::GetDeviceSpecificData<DeviceObjectArchiveBase::PRSDataHeader>(
    DeviceType              DevType,
    const PRSDataHeader&    Header,
    DynamicLinearAllocator& Allocator,
    const char*             ResTypeName);

bool DeviceObjectArchiveBase::PRSData::Deserialize(const char* Name, Serializer<SerializerMode::Read>& Ser)
{
    Desc.Name = Name;
    PRSSerializer<SerializerMode::Read>::SerializeDesc(Ser, Desc, &Allocator);
    return true;
}

bool DeviceObjectArchiveBase::RPData::Deserialize(const char* Name, Serializer<SerializerMode::Read>& Ser)
{
    Desc.Name = Name;
    RPSerializer<SerializerMode::Read>::SerializeDesc(Ser, Desc, &Allocator);
    return true;
}


template <typename CreateInfoType>
void DeviceObjectArchiveBase::PSOData<CreateInfoType>::DeserializeInternal(Serializer<SerializerMode::Read>& Ser)
{
    PSOSerializer<SerializerMode::Read>::SerializeCreateInfo(Ser, CreateInfo, PRSNames, &Allocator);
}

template <>
void DeviceObjectArchiveBase::PSOData<GraphicsPipelineStateCreateInfo>::DeserializeInternal(Serializer<SerializerMode::Read>& Ser)
{
    PSOSerializer<SerializerMode::Read>::SerializeCreateInfo(Ser, CreateInfo, PRSNames, &Allocator, RenderPassName);
}

template <>
void DeviceObjectArchiveBase::PSOData<RayTracingPipelineStateCreateInfo>::DeserializeInternal(Serializer<SerializerMode::Read>& Ser)
{
    auto RemapShaders = [](Uint32& InIndex, IShader*& outShader) {
        outShader = BitCast<IShader*>(size_t{InIndex});
    };
    PSOSerializer<SerializerMode::Read>::SerializeCreateInfo(Ser, CreateInfo, PRSNames, &Allocator, RemapShaders);
}

template <typename CreateInfoType>
bool DeviceObjectArchiveBase::PSOData<CreateInfoType>::Deserialize(const char* Name, Serializer<SerializerMode::Read>& Ser)
{
    CreateInfo.PSODesc.Name = Name;

    DeserializeInternal(Ser);
    PSOSerializer<SerializerMode::Read>::SerializeAuxData(Ser, AuxData, &Allocator);

    CreateInfo.Flags |= PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES;
    if (AuxData.NoShaderReflection)
        InternalCI.Flags |= PSO_CREATE_INTERNAL_FLAG_NO_SHADER_REFLECTION;

    CreateInfo.pInternalData = &InternalCI;

    if (CreateInfo.ResourceSignaturesCount == 0)
    {
        CreateInfo.ResourceSignaturesCount = 1;
        InternalCI.Flags |= PSO_CREATE_INTERNAL_FLAG_IMPLICIT_SIGNATURE0;
    }

    return true;
}

template <>
const DeviceObjectArchiveBase::ChunkType DeviceObjectArchiveBase::PSOData<GraphicsPipelineStateCreateInfo>::ExpectedChunkType = DeviceObjectArchiveBase::ChunkType::GraphicsPipelineStates;
template <>
const DeviceObjectArchiveBase::ChunkType DeviceObjectArchiveBase::PSOData<ComputePipelineStateCreateInfo>::ExpectedChunkType = DeviceObjectArchiveBase::ChunkType::ComputePipelineStates;
template <>
const DeviceObjectArchiveBase::ChunkType DeviceObjectArchiveBase::PSOData<TilePipelineStateCreateInfo>::ExpectedChunkType = DeviceObjectArchiveBase::ChunkType::TilePipelineStates;
template <>
const DeviceObjectArchiveBase::ChunkType DeviceObjectArchiveBase::PSOData<RayTracingPipelineStateCreateInfo>::ExpectedChunkType = DeviceObjectArchiveBase::ChunkType::RayTracingPipelineStates;


template <>
bool DeviceObjectArchiveBase::UnpackPSORenderPass<GraphicsPipelineStateCreateInfo>(PSOData<GraphicsPipelineStateCreateInfo>& PSO, IRenderDevice* pRenderDevice)
{
    VERIFY_EXPR(pRenderDevice != nullptr);
    if (PSO.RenderPassName == nullptr || *PSO.RenderPassName == 0)
        return true;

    RefCntAutoPtr<IRenderPass> pRenderPass;
    UnpackRenderPass(RenderPassUnpackInfo{pRenderDevice, PSO.RenderPassName}, &pRenderPass);
    if (!pRenderPass)
        return false;

    PSO.CreateInfo.GraphicsPipeline.pRenderPass = pRenderPass;
    PSO.Objects.emplace_back(std::move(pRenderPass));
    return true;
}

template <typename CreateInfoType>
bool DeviceObjectArchiveBase::UnpackPSOSignatures(PSOData<CreateInfoType>& PSO, IRenderDevice* pRenderDevice)
{
    const auto ResourceSignaturesCount = PSO.CreateInfo.ResourceSignaturesCount;
    if (ResourceSignaturesCount == 0)
    {
        UNEXPECTED("PSO must have at least one resource signature (including PSOs that use implicit signature)");
        return true;
    }
    auto* const ppResourceSignatures = PSO.Allocator.template Allocate<IPipelineResourceSignature*>(ResourceSignaturesCount);

    PSO.CreateInfo.ppResourceSignatures = ppResourceSignatures;
    for (Uint32 i = 0; i < ResourceSignaturesCount; ++i)
    {
        ResourceSignatureUnpackInfo UnpackInfo{pRenderDevice, PSO.PRSNames[i]};
        UnpackInfo.SRBAllocationGranularity = PSO.CreateInfo.PSODesc.SRBAllocationGranularity;

        auto pSignature = UnpackResourceSignature(UnpackInfo, (PSO.InternalCI.Flags & PSO_CREATE_INTERNAL_FLAG_IMPLICIT_SIGNATURE0) != 0);
        if (!pSignature)
            return false;

        ppResourceSignatures[i] = pSignature;
        PSO.Objects.emplace_back(std::move(pSignature));
    }
    return true;
}

RefCntAutoPtr<IShader> DeviceObjectArchiveBase::UnpackShader(const ShaderCreateInfo& ShaderCI,
                                                             IRenderDevice*          pDevice)
{
    RefCntAutoPtr<IShader> pShader;
    pDevice->CreateShader(ShaderCI, &pShader);
    return pShader;
}

inline void AssignShader(IShader*& pDstShader, IShader* pSrcShader, SHADER_TYPE ExpectedType)
{
    VERIFY_EXPR(pSrcShader != nullptr);
    VERIFY_EXPR(pSrcShader->GetDesc().ShaderType == ExpectedType);

    VERIFY(pDstShader == nullptr, "Non-null ", GetShaderTypeLiteralName(pDstShader->GetDesc().ShaderType), " has already been assigned. This might be a bug.");
    pDstShader = pSrcShader;
}

template <>
void DeviceObjectArchiveBase::PSOData<GraphicsPipelineStateCreateInfo>::AssignShaders()
{
    for (auto& Shader : Shaders)
    {
        const auto ShaderType = Shader->GetDesc().ShaderType;
        switch (ShaderType)
        {
            // clang-format off
            case SHADER_TYPE_VERTEX:        AssignShader(CreateInfo.pVS, Shader, ShaderType); break;
            case SHADER_TYPE_PIXEL:         AssignShader(CreateInfo.pPS, Shader, ShaderType); break;
            case SHADER_TYPE_GEOMETRY:      AssignShader(CreateInfo.pGS, Shader, ShaderType); break;
            case SHADER_TYPE_HULL:          AssignShader(CreateInfo.pHS, Shader, ShaderType); break; 
            case SHADER_TYPE_DOMAIN:        AssignShader(CreateInfo.pDS, Shader, ShaderType); break;
            case SHADER_TYPE_AMPLIFICATION: AssignShader(CreateInfo.pAS, Shader, ShaderType); break;
            case SHADER_TYPE_MESH:          AssignShader(CreateInfo.pMS, Shader, ShaderType); break;
            // clang-format on
            default:
                LOG_ERROR_MESSAGE("Unsupported shader type for graphics pipeline");
                return;
        }
    }
}

template <>
void DeviceObjectArchiveBase::PSOData<ComputePipelineStateCreateInfo>::AssignShaders()
{
    VERIFY(Shaders.size() == 1, "Compute pipline must have one shader");
    AssignShader(CreateInfo.pCS, Shaders[0], SHADER_TYPE_COMPUTE);
}

template <>
void DeviceObjectArchiveBase::PSOData<TilePipelineStateCreateInfo>::AssignShaders()
{
    VERIFY(Shaders.size() == 1, "Tile pipline must have one shader");
    AssignShader(CreateInfo.pTS, Shaders[0], SHADER_TYPE_TILE);
}

template <>
void DeviceObjectArchiveBase::PSOData<RayTracingPipelineStateCreateInfo>::AssignShaders()
{
    auto RemapShader = [this](IShader* const& inoutShader) //
    {
        auto ShaderIndex = BitCast<size_t>(inoutShader);
        if (ShaderIndex < Shaders.size())
            const_cast<IShader*&>(inoutShader) = Shaders[ShaderIndex];
        else
        {
            VERIFY(ShaderIndex == ~0u, "Failed to remap shader");
            const_cast<IShader*&>(inoutShader) = nullptr;
        }
    };

    for (Uint32 i = 0; i < CreateInfo.GeneralShaderCount; ++i)
    {
        RemapShader(CreateInfo.pGeneralShaders[i].pShader);
    }
    for (Uint32 i = 0; i < CreateInfo.TriangleHitShaderCount; ++i)
    {
        RemapShader(CreateInfo.pTriangleHitShaders[i].pClosestHitShader);
        RemapShader(CreateInfo.pTriangleHitShaders[i].pAnyHitShader);
    }
    for (Uint32 i = 0; i < CreateInfo.ProceduralHitShaderCount; ++i)
    {
        RemapShader(CreateInfo.pProceduralHitShaders[i].pIntersectionShader);
        RemapShader(CreateInfo.pProceduralHitShaders[i].pClosestHitShader);
        RemapShader(CreateInfo.pProceduralHitShaders[i].pAnyHitShader);
    }
}

template <>
void DeviceObjectArchiveBase::PSOData<GraphicsPipelineStateCreateInfo>::CreatePipeline(IRenderDevice* pDevice, IPipelineState** ppPSO)
{
    pDevice->CreateGraphicsPipelineState(CreateInfo, ppPSO);
}

template <>
void DeviceObjectArchiveBase::PSOData<ComputePipelineStateCreateInfo>::CreatePipeline(IRenderDevice* pDevice, IPipelineState** ppPSO)
{
    pDevice->CreateComputePipelineState(CreateInfo, ppPSO);
}

template <>
void DeviceObjectArchiveBase::PSOData<TilePipelineStateCreateInfo>::CreatePipeline(IRenderDevice* pDevice, IPipelineState** ppPSO)
{
    pDevice->CreateTilePipelineState(CreateInfo, ppPSO);
}

template <>
void DeviceObjectArchiveBase::PSOData<RayTracingPipelineStateCreateInfo>::CreatePipeline(IRenderDevice* pDevice, IPipelineState** ppPSO)
{
    pDevice->CreateRayTracingPipelineState(CreateInfo, ppPSO);
}

template <typename CreateInfoType>
bool DeviceObjectArchiveBase::UnpackPSOShaders(PSOData<CreateInfoType>& PSO,
                                               IRenderDevice*           pDevice)
{
    const auto DevType    = GetArchiveDeviceType(pDevice);
    const auto ShaderData = GetDeviceSpecificData(DevType, *PSO.pHeader, PSO.Allocator, ChunkTypeToResName(PSO.ExpectedChunkType));
    if (!ShaderData)
        return false;

    const Uint64 BaseOffset = m_BaseOffsets[static_cast<size_t>(GetBlockOffsetType(DevType))];
    if (BaseOffset > m_pArchive->GetSize())
    {
        LOG_ERROR_MESSAGE("Required block does not exist in archive");
        return false;
    }

    DynamicLinearAllocator Allocator{GetRawAllocator()};
    ShaderIndexArray       ShaderIndices;
    {
        Serializer<SerializerMode::Read> Ser{ShaderData};
        PSOSerializer<SerializerMode::Read>::SerializeShaderIndices(Ser, ShaderIndices, &Allocator);
        VERIFY_EXPR(Ser.IsEnded());
    }

    auto& ShaderInfo = GetShaderDeviceInfo(DevType, Allocator);

    PSO.Shaders.resize(ShaderIndices.Count);
    for (Uint32 i = 0; i < ShaderIndices.Count; ++i)
    {
        auto& pShader{PSO.Shaders[i]};

        const Uint32 Idx = ShaderIndices.pIndices[i];

        FileOffsetAndSize OffsetAndSize;
        {
            std::unique_lock<std::mutex> ReadLock{ShaderInfo.Mtx};

            if (Idx >= ShaderInfo.OffsetsAndCache.size())
                return false;

            // Try to get cached shader
            pShader = ShaderInfo.OffsetsAndCache[Idx].pRes;
            if (pShader)
                continue;

            OffsetAndSize = ShaderInfo.OffsetsAndCache[Idx];
        }

        void* pData = Allocator.Allocate(OffsetAndSize.Size, DataPtrAlign);

        if (!m_pArchive->Read(BaseOffset + OffsetAndSize.Offset, OffsetAndSize.Size, pData))
            return false;

        {
            ShaderCreateInfo ShaderCI;
            {
                Serializer<SerializerMode::Read> ShaderSer{SerializedData{pData, OffsetAndSize.Size}};
                ShaderSerializer<SerializerMode::Read>::SerializeCI(ShaderSer, ShaderCI);
                VERIFY_EXPR(ShaderSer.IsEnded());
            }

            if ((PSO.InternalCI.Flags & PSO_CREATE_INTERNAL_FLAG_NO_SHADER_REFLECTION) != 0)
                ShaderCI.CompileFlags |= SHADER_COMPILE_FLAG_SKIP_REFLECTION;

            pShader = UnpackShader(ShaderCI, pDevice);
            if (!pShader)
                return false;
        }

        // Add to the cache
        {
            std::unique_lock<std::mutex> WriteLock{ShaderInfo.Mtx};
            ShaderInfo.OffsetsAndCache[Idx].pRes = pShader;
        }
    }

    return true;
}

template <typename PSOCreateInfoType>
bool DeviceObjectArchiveBase::ModifyPipelineStateCreateInfo(PSOCreateInfoType&             CreateInfo,
                                                            const PipelineStateUnpackInfo& UnpackInfo)
{
    if (UnpackInfo.ModifyPipelineStateCreateInfo == nullptr)
        return true;

    const auto PipelineType = CreateInfo.PSODesc.PipelineType;

    auto ResourceLayout = CreateInfo.PSODesc.ResourceLayout;

    std::unordered_set<std::string> Strings;

    std::vector<ShaderResourceVariableDesc> Variables{ResourceLayout.Variables, ResourceLayout.Variables + ResourceLayout.NumVariables};
    for (auto& Var : Variables)
        Var.Name = Strings.emplace(Var.Name).first->c_str();

    std::vector<ImmutableSamplerDesc> ImmutableSamplers{ResourceLayout.ImmutableSamplers, ResourceLayout.ImmutableSamplers + ResourceLayout.NumImmutableSamplers};
    for (auto& Sam : ImmutableSamplers)
        Sam.SamplerOrTextureName = Strings.emplace(Sam.SamplerOrTextureName).first->c_str();

    ResourceLayout.Variables         = Variables.data();
    ResourceLayout.ImmutableSamplers = ImmutableSamplers.data();

    std::vector<IPipelineResourceSignature*> pSignatures{CreateInfo.ppResourceSignatures, CreateInfo.ppResourceSignatures + CreateInfo.ResourceSignaturesCount};

    UnpackInfo.ModifyPipelineStateCreateInfo(CreateInfo, UnpackInfo.pUserData);

    if (PipelineType != CreateInfo.PSODesc.PipelineType)
    {
        LOG_ERROR_MESSAGE("Modifying pipeline type is not allowed");
        return false;
    }

    if (!PipelineResourceLayoutDesc::IsEqual(ResourceLayout, CreateInfo.PSODesc.ResourceLayout, /*IgnoreVariables = */ false, /*IgnoreSamplers = */ true))
    {
        LOG_ERROR_MESSAGE("Only immutable sampler descriptions in the pipeline resource layout can be modified");
        return false;
    }

    for (size_t i = 0; i < ResourceLayout.NumImmutableSamplers; ++i)
    {
        // Immutable sampler descriptions can be modified, but shader stages must be the same
        if (ResourceLayout.ImmutableSamplers[i].ShaderStages != CreateInfo.PSODesc.ResourceLayout.ImmutableSamplers[i].ShaderStages)
        {
            LOG_ERROR_MESSAGE("Modifiying immutable sampler shader stages in the resource layout is not allowed");
            return false;
        }
    }

    if (pSignatures.size() != CreateInfo.ResourceSignaturesCount)
    {
        LOG_ERROR_MESSAGE("Changing the number of resource signatures is not allowed");
        return false;
    }

    for (size_t sign = 0; sign < CreateInfo.ResourceSignaturesCount; ++sign)
    {
        const auto* pOrigSign = pSignatures[sign];
        const auto* pNewSign  = CreateInfo.ppResourceSignatures[sign];
        if (pOrigSign == pNewSign)
            continue;
        if ((pOrigSign == nullptr) != (pNewSign == nullptr))
        {
            LOG_ERROR_MESSAGE("Changing non-null resource signature to null and vice versa is not allowed");
            return false;
        }
        if ((pOrigSign == nullptr) || (pNewSign == nullptr))
        {
            // This may never happen, but let's make static analyzers happy
            continue;
        }

        const auto& OrigDesc = pOrigSign->GetDesc();
        const auto& NewDesc  = pNewSign->GetDesc();
        if (!PipelineResourceSignaturesCompatible(OrigDesc, NewDesc, /*IgnoreSamplerDescriptions =*/true))
        {
            LOG_ERROR_MESSAGE("When changing pipeline resource singatures, only immutable sampler descriptions in new signatures are allowed to differ from original");
            return false;
        }
    }

    return true;
}

template <typename CreateInfoType>
void DeviceObjectArchiveBase::UnpackPipelineStateImpl(const PipelineStateUnpackInfo&            UnpackInfo,
                                                      IPipelineState**                          ppPSO,
                                                      OffsetSizeAndResourceMap<IPipelineState>& PSOMap)
{
    VERIFY_EXPR(UnpackInfo.pDevice != nullptr);

    if (UnpackInfo.ModifyPipelineStateCreateInfo == nullptr && PSOMap.GetResource(UnpackInfo.Name, ppPSO))
        return;

    PSOData<CreateInfoType> PSO{GetRawAllocator()};
    if (!LoadResourceData(PSOMap, UnpackInfo.Name, PSO))
        return;

#ifdef DILIGENT_DEVELOPMENT
    if (UnpackInfo.pDevice->GetDeviceInfo().IsD3DDevice())
    {
        // We always have reflection information in Direct3D shaders, so always
        // load it in development build to allow the engine verify bindings.
        PSO.InternalCI.Flags &= ~PSO_CREATE_INTERNAL_FLAG_NO_SHADER_REFLECTION;
    }
#endif

    if (!UnpackPSORenderPass(PSO, UnpackInfo.pDevice))
        return;

    if (!UnpackPSOSignatures(PSO, UnpackInfo.pDevice))
        return;

    if (!UnpackPSOShaders(PSO, UnpackInfo.pDevice))
        return;

    PSO.AssignShaders();

    PSO.CreateInfo.PSODesc.SRBAllocationGranularity = UnpackInfo.SRBAllocationGranularity;
    PSO.CreateInfo.PSODesc.ImmediateContextMask     = UnpackInfo.ImmediateContextMask;
    PSO.CreateInfo.pPSOCache                        = UnpackInfo.pCache;

    if (!ModifyPipelineStateCreateInfo(PSO.CreateInfo, UnpackInfo))
        return;

    PSO.CreatePipeline(UnpackInfo.pDevice, ppPSO);

    if (UnpackInfo.ModifyPipelineStateCreateInfo == nullptr)
        PSOMap.SetResource(UnpackInfo.Name, *ppPSO);
}

void DeviceObjectArchiveBase::UnpackGraphicsPSO(const PipelineStateUnpackInfo& UnpackInfo, IPipelineState** ppPSO)
{
    UnpackPipelineStateImpl<GraphicsPipelineStateCreateInfo>(UnpackInfo, ppPSO, m_GraphicsPSOMap);
}

void DeviceObjectArchiveBase::UnpackComputePSO(const PipelineStateUnpackInfo& UnpackInfo, IPipelineState** ppPSO)
{
    UnpackPipelineStateImpl<ComputePipelineStateCreateInfo>(UnpackInfo, ppPSO, m_ComputePSOMap);
}

void DeviceObjectArchiveBase::UnpackTilePSO(const PipelineStateUnpackInfo& UnpackInfo, IPipelineState** ppPSO)
{
    UnpackPipelineStateImpl<TilePipelineStateCreateInfo>(UnpackInfo, ppPSO, m_TilePSOMap);
}

void DeviceObjectArchiveBase::UnpackRayTracingPSO(const PipelineStateUnpackInfo& UnpackInfo, IPipelineState** ppPSO)
{
    UnpackPipelineStateImpl<RayTracingPipelineStateCreateInfo>(UnpackInfo, ppPSO, m_RayTracingPSOMap);
}

void DeviceObjectArchiveBase::UnpackRenderPass(const RenderPassUnpackInfo& UnpackInfo, IRenderPass** ppRP)
{
    VERIFY_EXPR(UnpackInfo.pDevice != nullptr);

    if (UnpackInfo.ModifyRenderPassDesc == nullptr && m_RenderPassMap.GetResource(UnpackInfo.Name, ppRP))
        return;

    RPData RP{GetRawAllocator()};
    if (!LoadResourceData(m_RenderPassMap, UnpackInfo.Name, RP))
        return;

    if (UnpackInfo.ModifyRenderPassDesc != nullptr)
        UnpackInfo.ModifyRenderPassDesc(RP.Desc, UnpackInfo.pUserData);

    UnpackInfo.pDevice->CreateRenderPass(RP.Desc, ppRP);

    if (UnpackInfo.ModifyRenderPassDesc == nullptr)
        m_RenderPassMap.SetResource(UnpackInfo.Name, *ppRP);
}

void DeviceObjectArchiveBase::ClearResourceCache()
{
    m_PRSMap.ReleaseResources();
    m_GraphicsPSOMap.ReleaseResources();
    m_ComputePSOMap.ReleaseResources();
    m_TilePSOMap.ReleaseResources();
    m_RayTracingPSOMap.ReleaseResources();
    m_RenderPassMap.ReleaseResources();

    {
        for (auto& ShaderInfo : m_ShaderInfo)
            ShaderInfo.OffsetsAndCache.clear();
    }
}

} // namespace Diligent
