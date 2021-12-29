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
#include <vector>
#include <unordered_set>
#include <algorithm>

#include "DeviceObjectArchiveBase.hpp"
#include "DebugUtilities.hpp"
#include "PSOSerializer.hpp"

namespace Diligent
{

DeviceObjectArchiveBase::DeviceObjectArchiveBase(IReferenceCounters* pRefCounters, IArchive* pArchive, DeviceType DevType) :
    TObjectBase{pRefCounters},
    m_pArchive{pArchive},
    m_DevType{DevType}
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
            LOG_ERROR_AND_THROW("Multiple chunks with the same types are not allowed");
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
            case ChunkType::Shaders:                  ReadShaders         (Chunk);                     break;
            // clang-format on
            default:
                LOG_ERROR_AND_THROW("Unknown chunk type (", static_cast<Uint32>(Chunk.Type), ")");
        }
    }
}

DeviceObjectArchiveBase::BlockOffsetType DeviceObjectArchiveBase::GetBlockOffsetType() const
{
    static_assert(static_cast<size_t>(DeviceType::Count) == 6, "Please handle the new device type below");
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

    std::vector<Uint8> Data(Chunk.Size);
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
void DeviceObjectArchiveBase::ReadNamedResources(const ChunkHeader& Chunk, OffsetSizeAndResourceMap<ResType>& NameAndOffset) noexcept(false)
{
    ReadNamedResources(m_pArchive, Chunk,
                       [&NameAndOffset](const char* Name, Uint32 Offset, Uint32 Size) //
                       {
                           NameAndOffset.Insert(Name, Offset, Size);
                       });
}

void DeviceObjectArchiveBase::ReadShaders(const ChunkHeader& Chunk) noexcept(false)
{
    VERIFY_EXPR(Chunk.Type == ChunkType::Shaders);
    VERIFY_EXPR(Chunk.Size == sizeof(ShadersDataHeader));

    ShadersDataHeader Header;
    if (!m_pArchive->Read(Chunk.Offset, sizeof(Header), &Header))
    {
        LOG_ERROR_AND_THROW("Failed to read indexed resources info from the archive");
    }

    void*  pData    = nullptr;
    size_t DataSize = 0;

    DynamicLinearAllocator Allocator{GetRawAllocator()};
    if (!GetDeviceSpecificData(Header, Allocator, "Shader list", GetBlockOffsetType(), pData, DataSize))
        return;

    VERIFY_EXPR(DataSize % sizeof(FileOffsetAndSize) == 0);
    const size_t Count = DataSize / sizeof(FileOffsetAndSize);

    const auto* pFileOffsetAndSize = reinterpret_cast<const FileOffsetAndSize*>(pData);

    std::unique_lock<std::mutex> WriteLock{m_ShadersGuard};
    m_Shaders.reserve(Count);
    for (Uint32 i = 0; i < Count; ++i)
        m_Shaders.emplace_back(pFileOffsetAndSize[i]);
}

template <typename ResType, typename FnType>
bool DeviceObjectArchiveBase::LoadResourceData(OffsetSizeAndResourceMap<ResType>& ResourceMap,
                                               const char*                        ResourceName,
                                               DynamicLinearAllocator&            Allocator,
                                               const char*                        ResTypeName,
                                               const FnType&                      Fn)
{
    const char* StoredResourceName = nullptr;

    const auto OffsetAndSize = ResourceMap.GetOffsetAndSize(ResourceName, StoredResourceName);
    if (OffsetAndSize == FileOffsetAndSize::Invalid())
    {
        LOG_ERROR_MESSAGE(ResTypeName, " with name '", ResourceName, "' is not present in the archive");
        return false;
    }
    VERIFY_EXPR(StoredResourceName != nullptr && StoredResourceName != ResourceName && strcmp(ResourceName, StoredResourceName) == 0);

    const auto DataSize = OffsetAndSize.Size;
    void*      pData    = Allocator.Allocate(DataSize, DataPtrAlign);
    if (!m_pArchive->Read(OffsetAndSize.Offset, DataSize, pData))
    {
        LOG_ERROR_MESSAGE("Failed to read ", ResTypeName, " with name '", ResourceName, "' data from the archive");
        return false;
    }

    Serializer<SerializerMode::Read> Ser{pData, DataSize};
    return Fn(StoredResourceName, Ser);
}

template <typename HeaderType>
bool DeviceObjectArchiveBase::GetDeviceSpecificData(const HeaderType&       Header,
                                                    DynamicLinearAllocator& Allocator,
                                                    const char*             ResTypeName,
                                                    BlockOffsetType         BlockType,
                                                    void*&                  pData,
                                                    size_t&                 Size)
{
    pData = nullptr;
    Size  = 0;

    const auto BaseOffset  = m_BaseOffsets[static_cast<Uint32>(BlockType)];
    const auto ArchiveSize = m_pArchive->GetSize();
    if (BaseOffset > ArchiveSize)
    {
        LOG_ERROR_MESSAGE("Required block does not exist in archive");
        return false;
    }
    if (Header.GetSize(m_DevType) == 0)
    {
        LOG_ERROR_MESSAGE("Device specific data is not specified for ", ResTypeName);
        return false;
    }
    if (BaseOffset + Header.GetEndOffset(m_DevType) > ArchiveSize)
    {
        LOG_ERROR_MESSAGE("Invalid offset in the archive");
        return false;
    }

    Size  = Header.GetSize(m_DevType);
    pData = Allocator.Allocate(Size, DataPtrAlign);
    if (!m_pArchive->Read(BaseOffset + Header.GetOffset(m_DevType), Size, pData))
    {
        LOG_ERROR_MESSAGE("Failed to read resource-specific data");
        return false;
    }

    return true;
}

// Instantiation is required by UnpackResourceSignatureImpl
template bool DeviceObjectArchiveBase::GetDeviceSpecificData<DeviceObjectArchiveBase::PRSDataHeader>(
    const PRSDataHeader&    Header,
    DynamicLinearAllocator& Allocator,
    const char*             ResTypeName,
    BlockOffsetType         BlockType,
    void*&                  pData,
    size_t&                 Size);

bool DeviceObjectArchiveBase::ReadPRSData(const char* Name, PRSData& PRS)
{
    return LoadResourceData(
        m_PRSMap, Name, PRS.Allocator,
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

            PSOSerializer<SerializerMode::Read>::SerializePRSDesc(Ser, PRS.Desc, PRS.Serialized, &PRS.Allocator);
            VERIFY_EXPR(Ser.IsEnd());
            return true;
        });
}

bool DeviceObjectArchiveBase::ReadRPData(const char* Name, RPData& RP)
{
    return LoadResourceData(
        m_RenderPassMap, Name, RP.Allocator,
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

            PSOSerializer<SerializerMode::Read>::SerializeRenderPassDesc(Ser, RP.Desc, &RP.Allocator);
            VERIFY_EXPR(Ser.IsEnd());
            return true;
        });
}


template <typename CreateInfoType>
void DeviceObjectArchiveBase::PSOData<CreateInfoType>::Deserialize(Serializer<SerializerMode::Read>& Ser)
{
    PSOSerializer<SerializerMode::Read>::SerializePSOCreateInfo(Ser, CreateInfo, PRSNames, &Allocator);
}

template <>
void DeviceObjectArchiveBase::PSOData<GraphicsPipelineStateCreateInfo>::Deserialize(Serializer<SerializerMode::Read>& Ser)
{
    PSOSerializer<SerializerMode::Read>::SerializePSOCreateInfo(Ser, CreateInfo, PRSNames, &Allocator, RenderPassName);
}

template <>
void DeviceObjectArchiveBase::PSOData<RayTracingPipelineStateCreateInfo>::Deserialize(Serializer<SerializerMode::Read>& Ser)
{
    auto RemapShaders = [](Uint32& InIndex, IShader*& outShader) {
        outShader = BitCast<IShader*>(size_t{InIndex});
    };
    PSOSerializer<SerializerMode::Read>::SerializePSOCreateInfo(Ser, CreateInfo, PRSNames, &Allocator, RemapShaders);
}

template <>
const DeviceObjectArchiveBase::ChunkType DeviceObjectArchiveBase::PSOData<GraphicsPipelineStateCreateInfo>::ExpectedChunkType = DeviceObjectArchiveBase::ChunkType::GraphicsPipelineStates;
template <>
const DeviceObjectArchiveBase::ChunkType DeviceObjectArchiveBase::PSOData<ComputePipelineStateCreateInfo>::ExpectedChunkType = DeviceObjectArchiveBase::ChunkType::ComputePipelineStates;
template <>
const DeviceObjectArchiveBase::ChunkType DeviceObjectArchiveBase::PSOData<TilePipelineStateCreateInfo>::ExpectedChunkType = DeviceObjectArchiveBase::ChunkType::TilePipelineStates;
template <>
const DeviceObjectArchiveBase::ChunkType DeviceObjectArchiveBase::PSOData<RayTracingPipelineStateCreateInfo>::ExpectedChunkType = DeviceObjectArchiveBase::ChunkType::RayTracingPipelineStates;

template <typename PSOHashMapType, typename PSOCreateInfoType>
bool DeviceObjectArchiveBase::ReadPSOData(const char*                 Name,
                                          PSOHashMapType&             PSOMap,
                                          const char*                 ResTypeName,
                                          PSOData<PSOCreateInfoType>& PSO)
{
    return LoadResourceData(
        PSOMap, Name, PSO.Allocator, ResTypeName,
        [&](const char* Name, Serializer<SerializerMode::Read>& Ser) -> bool //
        {
            PSO.CreateInfo.PSODesc.Name = Name;

            PSO.pHeader = Ser.Cast<PSODataHeader>();
            if (PSO.pHeader->Type != PSO.ExpectedChunkType)
            {
                LOG_ERROR_MESSAGE("Invalid ", ResTypeName, " header in the archive");
                return false;
            }

            PSO.Deserialize(Ser);
            VERIFY_EXPR(Ser.IsEnd());

            PSO.CreateInfo.Flags |= PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES;

            if (PSO.CreateInfo.ResourceSignaturesCount == 0)
            {
                PSO.CreateInfo.ResourceSignaturesCount = 1;
                PSO.CreateInfo.Flags |= PSO_CREATE_FLAG_IMPLICIT_SIGNATURE0;
            }

            return true;
        });
}


template <>
bool DeviceObjectArchiveBase::UnpackPSORenderPass<GraphicsPipelineStateCreateInfo>(PSOData<GraphicsPipelineStateCreateInfo>& PSO, IRenderDevice* pRenderDevice)
{
    VERIFY_EXPR(pRenderDevice != nullptr);
    if (PSO.RenderPassName == nullptr || *PSO.RenderPassName == 0)
        return true;

    RefCntAutoPtr<IRenderPass> pRenderPass;
    UnpackRenderPass(RenderPassUnpackInfo{pRenderDevice, this, PSO.RenderPassName}, &pRenderPass);
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
        ResourceSignatureUnpackInfo UnpackInfo{pRenderDevice, this, PSO.PRSNames[i]};
        UnpackInfo.SRBAllocationGranularity = PSO.CreateInfo.PSODesc.SRBAllocationGranularity;

        auto pSignature = UnpackResourceSignature(UnpackInfo, (PSO.CreateInfo.Flags & PSO_CREATE_FLAG_IMPLICIT_SIGNATURE0) != 0);
        if (!pSignature)
            return false;

        ppResourceSignatures[i] = pSignature;
        PSO.Objects.emplace_back(std::move(pSignature));
    }
    return true;
}

RefCntAutoPtr<IShader> DeviceObjectArchiveBase::UnpackShader(Serializer<SerializerMode::Read>& Ser,
                                                             ShaderCreateInfo&                 ShaderCI,
                                                             IRenderDevice*                    pDevice)
{
    VERIFY_EXPR(ShaderCI.SourceLanguage == SHADER_SOURCE_LANGUAGE_DEFAULT);
    VERIFY_EXPR(ShaderCI.ShaderCompiler == SHADER_COMPILER_DEFAULT);

    ShaderCI.ByteCode     = Ser.GetCurrentPtr();
    ShaderCI.ByteCodeSize = Ser.GetRemainSize();

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
                                               IRenderDevice*           pDevice,
                                               const char*              PipelineTypeName)
{
    void*  pShaderData    = nullptr;
    size_t ShaderDataSize = 0;

    if (!GetDeviceSpecificData(*PSO.pHeader, PSO.Allocator, PipelineTypeName, GetBlockOffsetType(), pShaderData, ShaderDataSize))
        return false;

    Serializer<SerializerMode::Read> Ser{pShaderData, ShaderDataSize};

    const Uint64 BaseOffset = m_BaseOffsets[static_cast<size_t>(GetBlockOffsetType())];
    if (BaseOffset > m_pArchive->GetSize())
    {
        LOG_ERROR_MESSAGE("Required block does not exist in archive");
        return false;
    }

    DynamicLinearAllocator Allocator{GetRawAllocator()};

    ShaderIndexArray ShaderIndices;
    PSOSerializer<SerializerMode::Read>::SerializeShaders(Ser, ShaderIndices, &Allocator);
    VERIFY_EXPR(Ser.IsEnd());

    PSO.Shaders.resize(ShaderIndices.Count);
    for (Uint32 i = 0; i < ShaderIndices.Count; ++i)
    {
        auto& pShader{PSO.Shaders[i]};

        const Uint32 Idx = ShaderIndices.pIndices[i];

        FileOffsetAndSize OffsetAndSize;
        {
            std::unique_lock<std::mutex> ReadLock{m_ShadersGuard};

            if (Idx >= m_Shaders.size())
                return false;

            // Try to get cached shader
            pShader = m_Shaders[Idx].pRes;
            if (pShader)
                continue;

            OffsetAndSize = m_Shaders[Idx];
        }

        void* pData = Allocator.Allocate(OffsetAndSize.Size, DataPtrAlign);

        if (!m_pArchive->Read(BaseOffset + OffsetAndSize.Offset, OffsetAndSize.Size, pData))
            return false;

        {
            Serializer<SerializerMode::Read> ShaderSer{pData, OffsetAndSize.Size};
            ShaderCreateInfo                 ShaderCI;
            ShaderSer(ShaderCI.Desc.ShaderType, ShaderCI.EntryPoint, ShaderCI.SourceLanguage, ShaderCI.ShaderCompiler);

            ShaderCI.CompileFlags |= SHADER_COMPILE_FLAG_SKIP_REFLECTION;

            pShader = UnpackShader(ShaderSer, ShaderCI, pDevice);
            if (!pShader)
                return false;
        }

        // Add to cache
        {
            std::unique_lock<std::mutex> WriteLock{m_ShadersGuard};
            m_Shaders[Idx].pRes = pShader;
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

    if (!(ResourceLayout == CreateInfo.PSODesc.ResourceLayout))
    {
        LOG_ERROR_MESSAGE("Modifying resource layout is not allowed");
        return false;
    }

    if (!std::equal(pSignatures.begin(), pSignatures.end(), CreateInfo.ppResourceSignatures, CreateInfo.ppResourceSignatures + CreateInfo.ResourceSignaturesCount))
    {
        LOG_ERROR_MESSAGE("Modifying resource signatures is not allowed");
        return false;
    }

    return true;
}

template <typename CreateInfoType>
void DeviceObjectArchiveBase::UnpackPipelineStateImpl(const PipelineStateUnpackInfo&            UnpackInfo,
                                                      IPipelineState**                          ppPSO,
                                                      OffsetSizeAndResourceMap<IPipelineState>& PSOMap,
                                                      const char*                               PipelineTypeName)
{
    VERIFY_EXPR(UnpackInfo.pArchive == nullptr || UnpackInfo.pArchive == this);
    VERIFY_EXPR(UnpackInfo.pDevice != nullptr);

    if (UnpackInfo.ModifyPipelineStateCreateInfo == nullptr && PSOMap.GetResource(UnpackInfo.Name, ppPSO))
        return;

    PSOData<CreateInfoType> PSO{GetRawAllocator()};
    if (!ReadPSOData(UnpackInfo.Name, PSOMap, PipelineTypeName, PSO))
        return;

    if (!UnpackPSORenderPass(PSO, UnpackInfo.pDevice))
        return;

    if (!UnpackPSOSignatures(PSO, UnpackInfo.pDevice))
        return;

    if (!UnpackPSOShaders(PSO, UnpackInfo.pDevice, PipelineTypeName))
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
    UnpackPipelineStateImpl<GraphicsPipelineStateCreateInfo>(UnpackInfo, ppPSO, m_GraphicsPSOMap, "Graphics Pipeline");
}

void DeviceObjectArchiveBase::UnpackComputePSO(const PipelineStateUnpackInfo& UnpackInfo, IPipelineState** ppPSO)
{
    UnpackPipelineStateImpl<ComputePipelineStateCreateInfo>(UnpackInfo, ppPSO, m_ComputePSOMap, "Compute Pipeline");
}

void DeviceObjectArchiveBase::UnpackTilePSO(const PipelineStateUnpackInfo& UnpackInfo, IPipelineState** ppPSO)
{
    UnpackPipelineStateImpl<TilePipelineStateCreateInfo>(UnpackInfo, ppPSO, m_TilePSOMap, "Tile Pipeline");
}

void DeviceObjectArchiveBase::UnpackRayTracingPSO(const PipelineStateUnpackInfo& UnpackInfo, IPipelineState** ppPSO)
{
    UnpackPipelineStateImpl<RayTracingPipelineStateCreateInfo>(UnpackInfo, ppPSO, m_RayTracingPSOMap, "Ray Tracing Pipeline");
}

void DeviceObjectArchiveBase::UnpackRenderPass(const RenderPassUnpackInfo& UnpackInfo, IRenderPass** ppRP)
{
    VERIFY_EXPR(UnpackInfo.pArchive == nullptr || UnpackInfo.pArchive == this);
    VERIFY_EXPR(UnpackInfo.pDevice != nullptr);

    if (UnpackInfo.ModifyRenderPassDesc == nullptr && m_RenderPassMap.GetResource(UnpackInfo.Name, ppRP))
        return;

    RPData RP{GetRawAllocator()};
    if (!ReadRPData(UnpackInfo.Name, RP))
        return;

    if (UnpackInfo.ModifyRenderPassDesc != nullptr)
        UnpackInfo.ModifyRenderPassDesc(RP.Desc, UnpackInfo.pUserData);

    UnpackInfo.pDevice->CreateRenderPass(RP.Desc, ppRP);

    if (UnpackInfo.ModifyRenderPassDesc == nullptr)
        m_RenderPassMap.SetResource(UnpackInfo.Name, *ppRP);
}

void DeviceObjectArchiveBase::ClearResourceCache()
{
    std::unique_lock<std::mutex> WriteLock{m_ShadersGuard};

    for (auto& Shader : m_Shaders)
    {
        Shader.pRes.Release();
    }
}

} // namespace Diligent
