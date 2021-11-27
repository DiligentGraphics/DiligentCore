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
#include "Archiver_Inc.hpp"

#include "RenderDeviceMtlImpl.hpp"
#include "PipelineResourceSignatureMtlImpl.hpp"
#include "PipelineStateMtlImpl.hpp"
#include "ShaderMtlImpl.hpp"
#include "DeviceObjectArchiveMtlImpl.hpp"

#include "spirv_msl.hpp"

namespace Diligent
{

template <>
struct SerializableResourceSignatureImpl::SignatureTraits<PipelineResourceSignatureMtlImpl>
{
    static constexpr DeviceType Type = DeviceType::Metal_iOS;

    template <SerializerMode Mode>
    using PSOSerializerType = PSOSerializerMtl<Mode>;
};

namespace
{

struct ShaderStageInfoMtl
{
    ShaderStageInfoMtl() {}

    ShaderStageInfoMtl(const SerializableShaderImpl* _pShader) :
        Type{_pShader->GetDesc().ShaderType},
        pShader{_pShader}
    {}

    // Needed only for ray tracing
    void Append(const SerializableShaderImpl*) {}

    Uint32 Count() const { return 1; }

    SHADER_TYPE                   Type    = SHADER_TYPE_UNKNOWN;
    const SerializableShaderImpl* pShader = nullptr;

    SHADER_TYPE GetShaderStageType(const ShaderStageInfoMtl& Stage)
    {
        return Stage.Type;
    }
};

void VerifyResourceMerge(const char*                       PSOName,
                         const SPIRVShaderResourceAttribs& ExistingRes,
                         const SPIRVShaderResourceAttribs& NewResAttribs)
{
#define LOG_RESOURCE_MERGE_ERROR_AND_THROW(PropertyName)                                                  \
    LOG_ERROR_AND_THROW("Shader variable '", NewResAttribs.Name,                                          \
                        "' is shared between multiple shaders in pipeline '", (PSOName ? PSOName : ""),   \
                        "', but its " PropertyName " varies. A variable shared between multiple shaders " \
                        "must be defined identically in all shaders. Either use separate variables for "  \
                        "different shader stages, change resource name or make sure that " PropertyName " is consistent.");

    if (ExistingRes.Type != NewResAttribs.Type)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("type");

    if (ExistingRes.ResourceDim != NewResAttribs.ResourceDim)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("resource dimension");

    if (ExistingRes.ArraySize != NewResAttribs.ArraySize)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("array size");

    if (ExistingRes.IsMS != NewResAttribs.IsMS)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("multisample state");
#undef LOG_RESOURCE_MERGE_ERROR_AND_THROW
}

} // namespace


template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersMtl(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS)
{
    std::vector<ShaderStageInfoMtl> ShaderStages;
    SHADER_TYPE                     ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateMtlImpl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);
    
    IPipelineResourceSignature* DefaultSignatures[1] = {};
    if (CreateInfo.ResourceSignaturesCount == 0)
    {
        try
        {
            std::vector<PipelineResourceDesc> Resources;
            std::vector<ImmutableSamplerDesc> ImmutableSamplers;
            PipelineResourceSignatureDesc     SignDesc;
            const auto&                       ResourceLayout = CreateInfo.PSODesc.ResourceLayout;
        
            std::unordered_map<ShaderResourceHashKey, const SPIRVShaderResourceAttribs&, ShaderResourceHashKey::Hasher> UniqueResources;

            for (auto& Stage : ShaderStages)
            {
                const auto* pSPIRVResources = Stage.pShader->GetMtlShaderSPIRVResources();
                if (pSPIRVResources == nullptr)
                    continue;

                pSPIRVResources->ProcessResources(
                    [&](const SPIRVShaderResourceAttribs& Attribs, Uint32)
                    {
                        const char* const SamplerSuffix =
                            (pSPIRVResources->IsUsingCombinedSamplers() && Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler) ?
                            pSPIRVResources->GetCombinedSamplerSuffix() :
                            nullptr;

                        const auto VarDesc = FindPipelineResourceLayoutVariable(ResourceLayout, Attribs.Name, Stage.Type, SamplerSuffix);
                        // Note that Attribs.Name != VarDesc.Name for combined samplers
                        const auto it_assigned = UniqueResources.emplace(ShaderResourceHashKey{Attribs.Name, VarDesc.ShaderStages}, Attribs);
                        if (it_assigned.second)
                        {
                            if (Attribs.ArraySize == 0)
                            {
                                LOG_ERROR_AND_THROW("Resource '", Attribs.Name, "' in shader '", Stage.pShader->GetDesc().Name, "' is a runtime-sized array. ",
                                                    "You must use explicit resource signature to specify the array size.");
                            }

                            const auto ResType = SPIRVShaderResourceAttribs::GetShaderResourceType(Attribs.Type);
                            const auto Flags   = SPIRVShaderResourceAttribs::GetPipelineResourceFlags(Attribs.Type) | ShaderVariableFlagsToPipelineResourceFlags(VarDesc.Flags);
                            Resources.emplace_back(VarDesc.ShaderStages, Attribs.Name, Attribs.ArraySize, ResType, VarDesc.Type, Flags);
                        }
                        else
                        {
                            VerifyResourceMerge("", it_assigned.first->second, Attribs);
                        }
                    });

                // Merge combined sampler suffixes
                if (pSPIRVResources->IsUsingCombinedSamplers() && pSPIRVResources->GetNumSepSmplrs() > 0)
                {
                    if (SignDesc.CombinedSamplerSuffix != nullptr)
                    {
                        if (strcmp(SignDesc.CombinedSamplerSuffix, pSPIRVResources->GetCombinedSamplerSuffix()) != 0)
                            LOG_ERROR_AND_THROW("CombinedSamplerSuffix is not compatible between shaders");
                    }
                    else
                    {
                        SignDesc.CombinedSamplerSuffix = pSPIRVResources->GetCombinedSamplerSuffix();
                    }
                }
            }

            SignDesc.Name                       = DefPRS.UniqueName.c_str();
            SignDesc.NumResources               = static_cast<Uint32>(Resources.size());
            SignDesc.Resources                  = SignDesc.NumResources > 0 ? Resources.data() : nullptr;
            SignDesc.NumImmutableSamplers       = ResourceLayout.NumImmutableSamplers;
            SignDesc.ImmutableSamplers          = ResourceLayout.ImmutableSamplers;
            SignDesc.BindingIndex               = 0;
            SignDesc.UseCombinedTextureSamplers = SignDesc.CombinedSamplerSuffix != nullptr;

            RefCntAutoPtr<IPipelineResourceSignature> pDefaultPRS;
            m_pSerializationDevice->CreatePipelineResourceSignature(SignDesc, DefPRS.DeviceFlags, ActiveShaderStages, &pDefaultPRS);
            if (pDefaultPRS == nullptr)
                return false;

            if (DefPRS.pPRS)
            {
                if (!(DefPRS.pPRS.RawPtr<SerializableResourceSignatureImpl>()->IsCompatible(*pDefaultPRS.RawPtr<SerializableResourceSignatureImpl>(), DefPRS.DeviceFlags)))
                {
                    LOG_ERROR_MESSAGE("Default signatures does not match between different backends");
                    return false;
                }
                pDefaultPRS = DefPRS.pPRS;
            }
            else
            {
                if (!CachePipelineResourceSignature(pDefaultPRS))
                    return false;
                DefPRS.pPRS = pDefaultPRS;
            }
        }
        catch (...)
        {
            LOG_ERROR_MESSAGE("Failed to create default resource signature");
            return false;
        }

        DefaultSignatures[0]               = DefPRS.pPRS;
        CreateInfo.ResourceSignaturesCount = 1;
        CreateInfo.ppResourceSignatures    = DefaultSignatures;
        CreateInfo.PSODesc.ResourceLayout  = {};
    }
    
    TShaderIndices ShaderIndicesMacOS;
    TShaderIndices ShaderIndicesiOS;
    const bool     SerializeMacOS = m_pSerializationDevice->MtlCompileForMacOS();
    const bool     SerializeiOS   = m_pSerializationDevice->MtlCompileForiOS();

    try
    {
        SignatureArray<PipelineResourceSignatureMtlImpl> Signatures      = {};
        Uint32                                           SignaturesCount = 0;
        SortResourceSignatures(CreateInfo, Signatures, SignaturesCount);

        std::array<MtlResourceCounters, MAX_RESOURCE_SIGNATURES> BaseBindings{};
        MtlResourceCounters                                      CurrBindings{};
        for (Uint32 s = 0; s < SignaturesCount; ++s)
        {
            BaseBindings[s] = CurrBindings;
            const auto& pSignature = Signatures[s];
            if (pSignature != nullptr)
                pSignature->ShiftBindings(CurrBindings);
        }

        for (size_t j = 0; j < ShaderStages.size(); ++j)
        {
            const auto& Stage = ShaderStages[j];
            if (SerializeiOS)
            {
                SerializedMemory PatchedBytecode = Stage.pShader->PatchShaderMtl(Signatures.data(), BaseBindings.data(), SignaturesCount, /*IsForMacOS*/false); // throw exception
                SerializeShaderBytecode(ShaderIndicesiOS, DeviceType::Metal_iOS, Stage.pShader->GetCreateInfo(), PatchedBytecode.Ptr(), PatchedBytecode.Size());
            }
            if (SerializeMacOS)
            {
                SerializedMemory PatchedBytecode = Stage.pShader->PatchShaderMtl(Signatures.data(), BaseBindings.data(), SignaturesCount, /*IsForMacOS*/true); // throw exception
                SerializeShaderBytecode(ShaderIndicesMacOS, DeviceType::Metal_MacOS, Stage.pShader->GetCreateInfo(), PatchedBytecode.Ptr(), PatchedBytecode.Size());
            }
        }
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to compile Metal shaders");
        return false;
    }
    
    if (SerializeiOS)
        Data.PerDeviceData[static_cast<Uint32>(DeviceType::Metal_iOS)] = SerializeShadersForPSO(ShaderIndicesiOS);
    if (SerializeMacOS)
        Data.PerDeviceData[static_cast<Uint32>(DeviceType::Metal_MacOS)] = SerializeShadersForPSO(ShaderIndicesMacOS);
    return true;
}

template bool ArchiverImpl::PatchShadersMtl<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersMtl<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersMtl<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersMtl<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);


static_assert(std::is_same<MtlArchiverResourceCounters, MtlResourceCounters>::value,
              "MtlArchiverResourceCounters and MtlResourceCounters must be same types");

struct SerializableShaderImpl::CompiledShaderMtlImpl final : ICompiledShader
{
    String                                      MslSource;
    std::vector<uint32_t>                       SPIRV;
    std::unique_ptr<SPIRVShaderResources>       SPIRVResources;
    MtlFunctionArguments::BufferTypeInfoMapType BufferTypeInfoMap;
};

void SerializableShaderImpl::CreateShaderMtl(ShaderCreateInfo& ShaderCI, String& CompilationLog)
{
    auto* pShaderMtl = new CompiledShaderMtlImpl{};
    m_pShaderMtl.reset(pShaderMtl);

    // Mem leak when used RefCntAutoPtr
    IDataBlob* pLog           = nullptr;
    ShaderCI.ppCompilerOutput = &pLog;

    // Convert HLSL/GLSL/SPIRV to MSL
    try
    {
        ShaderMtlImpl::ConvertToMSL(ShaderCI,
                                    m_pDevice->GetDeviceInfo(),
                                    m_pDevice->GetAdapterInfo(),
                                    pShaderMtl->MslSource,
                                    pShaderMtl->SPIRV,
                                    pShaderMtl->SPIRVResources,
                                    pShaderMtl->BufferTypeInfoMap); // may throw exception
    }
    catch (...)
    {
        if (pLog && pLog->GetConstDataPtr())
        {
            CompilationLog += "Failed to compile Metal shader:\n";
            CompilationLog += static_cast<const char*>(pLog->GetConstDataPtr());
        }
    }

    if (pLog)
        pLog->Release();
}

template PipelineResourceSignatureMtlImpl* SerializableResourceSignatureImpl::GetSignature<PipelineResourceSignatureMtlImpl>() const;

const SPIRVShaderResources* SerializableShaderImpl::GetMtlShaderSPIRVResources() const
{
    auto* pShaderMtl = static_cast<const CompiledShaderMtlImpl*>(m_pShaderMtl.get());
    return pShaderMtl && pShaderMtl->SPIRVResources ? pShaderMtl->SPIRVResources.get() : nullptr;
}

namespace
{
template <SerializerMode Mode>
void SerializeBufferTypeInfoMapAndComputeGroupSize(Serializer<Mode>                                  &Ser,
                                                   const MtlFunctionArguments::BufferTypeInfoMapType &BufferTypeInfoMap,
                                                   const SHADER_TYPE                                  ShaderType,
                                                   const std::unique_ptr<SPIRVShaderResources>       &SPIRVResources)
{
    const auto Count = static_cast<Uint32>(BufferTypeInfoMap.size());
    Ser(Count);
    
    for (auto& BuffInfo : BufferTypeInfoMap)
    {
        const char* Name = BuffInfo.second.Name.c_str();
        Ser(BuffInfo.first, Name, BuffInfo.second.ArraySize, BuffInfo.second.ResourceType);
    }

    if (ShaderType == SHADER_TYPE_COMPUTE)
    {
        std::array<Uint32,3> GroupSize = {};
        if (SPIRVResources)
            GroupSize = SPIRVResources->GetComputeGroupSize();
        
        Ser(GroupSize);
    }
}
} // namespace

SerializedMemory SerializableShaderImpl::PatchShaderMtl(const RefCntAutoPtr<PipelineResourceSignatureMtlImpl>* pSignatures,
                                                        const MtlResourceCounters*                             pBaseBindings,
                                                        const Uint32                                           SignatureCount,
                                                        const bool                                             IsForMacOS) const noexcept(false)
{
    VERIFY_EXPR(SignatureCount > 0);
    VERIFY_EXPR(pSignatures != nullptr);
    VERIFY_EXPR(pBaseBindings != nullptr);

    const auto RemoveTempFiles = []() {
        std::remove("Shader.metal");
        std::remove("Shader.air");
        std::remove("Shader.metallib");
        //std::remove("Shader.metallibsym");
    };
    RemoveTempFiles();

    auto*  pShaderMtl = static_cast<const CompiledShaderMtlImpl*>(m_pShaderMtl.get());
    String MslSource  = pShaderMtl->MslSource;
    MtlFunctionArguments::BufferTypeInfoMapType BufferTypeInfoMap;

    if (!pShaderMtl->SPIRV.empty())
    {
        try
        {
            // Shader can be patched as SPIRV
            VERIFY_EXPR(pShaderMtl->SPIRVResources != nullptr);
            ShaderMtlImpl::MtlResourceRemappingVectorType ResRemapping;

            PipelineStateMtlImpl::RemapShaderResources(pShaderMtl->SPIRV,
                                                       *pShaderMtl->SPIRVResources,
                                                       ResRemapping,
                                                       pSignatures,
                                                       SignatureCount,
                                                       pBaseBindings,
                                                       GetDesc(),
                                                       ""); // may throw exception

            MslSource = ShaderMtlImpl::SPIRVtoMSL(pShaderMtl->SPIRV,
                                                  GetCreateInfo(),
                                                  &ResRemapping,
                                                  BufferTypeInfoMap); // may throw exception

            VERIFY_EXPR(BufferTypeInfoMap.size() == pShaderMtl->BufferTypeInfoMap.size());
        }
        catch (...)
        {
            LOG_ERROR_AND_THROW("Failed to patch Metal shader");
        }
    }

    // Save to 'Shader.metal'
    {
        FILE* File = fopen("Shader.metal", "wb");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to save Metal shader source");

        fwrite(MslSource.c_str(), sizeof(MslSource[0]), MslSource.size(), File);
        fclose(File);
    }

    // Run user-defined MSL preprocessor
    if (!m_pDevice->GetMslPreprocessorCmd().empty())
    {
        FILE* File = popen((m_pDevice->GetMslPreprocessorCmd() + " Shader.metal").c_str(), "r");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to run command line Metal shader compiler");

        char Output[512];
        while (fgets(Output, _countof(Output), File) != nullptr)
            printf("%s", Output);

        auto status = pclose(File);
        if (status == -1)
            LOG_ERROR_MESSAGE("Failed to close process");
    }

    // https://developer.apple.com/documentation/metal/libraries/generating_and_loading_a_metal_library_symbol_file

    // Compile MSL to AIR file
    {
        String cmd{"xcrun "};
        cmd += (IsForMacOS ? m_pDevice->GetMtlCompileOptionsMacOS() : m_pDevice->GetMtlCompileOptionsiOS());
        cmd += " -c Shader.metal -o Shader.air";

        FILE* File = popen(cmd.c_str(), "r");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to compile MSL to AIR");

        char Output[512];
        while (fgets(Output, _countof(Output), File) != nullptr)
            printf("%s", Output);

        auto status = pclose(File);
        if (status == -1)
            LOG_ERROR_MESSAGE("Failed to close process");
    }

    // Generate a Metal library
    {
        String cmd{"xcrun "};
        cmd += (IsForMacOS ? m_pDevice->GetMtlLinkOptionsMacOS() : m_pDevice->GetMtlLinkOptionsiOS());
        cmd += " -o Shader.metallib Shader.air";

        FILE* File = popen(cmd.c_str(), "r");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to generate Metal library");

        char Output[512];
        while (fgets(Output, _countof(Output), File) != nullptr)
            printf("%s", Output);

        auto status = pclose(File);
        if (status == -1)
            LOG_ERROR_MESSAGE("Failed to close process");
    }
    
    size_t BytecodeOffset = 0;
    {
        Serializer<SerializerMode::Measure> MeasureSer;
        SerializeBufferTypeInfoMapAndComputeGroupSize(MeasureSer, BufferTypeInfoMap, GetDesc().ShaderType, pShaderMtl->SPIRVResources);
        BytecodeOffset = MeasureSer.GetSize(nullptr);
    }

    // Read 'Shader.metallib'
    auto&            RawAllocator = GetRawAllocator();
    SerializedMemory Bytecode;
    size_t           BytecodeSize = 0;
    {
        FILE* File = fopen("Shader.metallib", "rb");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to read shader library");

        fseek(File, 0, SEEK_END);
        long size = ftell(File);
        fseek(File, 0, SEEK_SET);

        Bytecode = SerializedMemory{ALLOCATE_RAW(RawAllocator, "", BytecodeOffset + size), BytecodeOffset + size, &RawAllocator};
        BytecodeSize = fread(&static_cast<Uint8*>(Bytecode.Ptr())[BytecodeOffset], 1, size, File);

        fclose(File);
    }

    RemoveTempFiles();

    if (BytecodeSize == 0)
        LOG_ERROR_AND_THROW("Metal shader library is empty");
    
    Serializer<SerializerMode::Write> Ser{Bytecode.Ptr(), BytecodeOffset};
    SerializeBufferTypeInfoMapAndComputeGroupSize(Ser, BufferTypeInfoMap, GetDesc().ShaderType, pShaderMtl->SPIRVResources);
    VERIFY_EXPR(Ser.IsEnd());

    return Bytecode;
}


void SerializableResourceSignatureImpl::CreatePRSMtl(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages)
{
    CreateSignature<PipelineResourceSignatureMtlImpl>(pRefCounters, Desc, ShaderStages);
}


void SerializationDeviceImpl::GetPipelineResourceBindingsMtl(const PipelineResourceBindingAttribs& Info,
                                                             std::vector<PipelineResourceBinding>& ResourceBindings,
                                                             const Uint32                          MaxBufferArgs)
{
    ResourceBindings.clear();

    std::array<RefCntAutoPtr<PipelineResourceSignatureMtlImpl>, MAX_RESOURCE_SIGNATURES> Signatures = {};

    Uint32 SignaturesCount = 0;
    for (Uint32 i = 0; i < Info.ResourceSignaturesCount; ++i)
    {
        const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(Info.ppResourceSignatures[i]);
        const auto& Desc    = pSerPRS->GetDesc();

        Signatures[Desc.BindingIndex] = pSerPRS->GetSignature<PipelineResourceSignatureMtlImpl>();
        SignaturesCount               = std::max(SignaturesCount, static_cast<Uint32>(Desc.BindingIndex) + 1);
    }

    const auto            ShaderStages        = (Info.ShaderStages == SHADER_TYPE_UNKNOWN ? static_cast<SHADER_TYPE>(~0u) : Info.ShaderStages);
    constexpr SHADER_TYPE SupportedStagesMask = (SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL | SHADER_TYPE_COMPUTE | SHADER_TYPE_TILE);
    MtlResourceCounters   BaseBindings{};

    for (Uint32 sign = 0; sign < SignaturesCount; ++sign)
    {
        const auto& pSignature = Signatures[sign];
        if (pSignature == nullptr)
            continue;

        for (Uint32 r = 0; r < pSignature->GetTotalResourceCount(); ++r)
        {
            const auto& ResDesc = pSignature->GetResourceDesc(r);
            const auto& ResAttr = pSignature->GetResourceAttribs(r);
            const auto  Range   = PipelineResourceDescToMtlResourceRange(ResDesc);

            for (auto Stages = ShaderStages & SupportedStagesMask; Stages != 0;)
            {
                const auto ShaderStage = ExtractLSB(Stages);
                const auto ShaderInd   = MtlResourceBindIndices::ShaderTypeToIndex(ShaderStage);
                DEV_CHECK_ERR(ShaderInd < MtlResourceBindIndices::NumShaderTypes,
                              "Unsupported shader stage (", GetShaderTypeLiteralName(ShaderStage), ") for Metal backend");

                if ((ResDesc.ShaderStages & ShaderStage) == 0)
                    continue;

                ResourceBindings.push_back(ResDescToPipelineResBinding(ResDesc, ShaderStage, BaseBindings[ShaderInd][Range] + ResAttr.BindIndices[ShaderInd], 0 /*space*/));
            }
        }
        pSignature->ShiftBindings(BaseBindings);
    }

    // Add vertex buffer bindings.
    // Same as DeviceContextMtlImpl::CommitVertexBuffers()
    if ((Info.ShaderStages & SHADER_TYPE_VERTEX) != 0 && Info.NumVertexBuffers > 0)
    {
        DEV_CHECK_ERR(Info.VertexBufferNames != nullptr, "VertexBufferNames must not be null");

        const auto BaseSlot = MaxBufferArgs - Info.NumVertexBuffers;
        for (Uint32 i = 0; i < Info.NumVertexBuffers; ++i)
        {
            PipelineResourceBinding Dst{};
            Dst.Name         = Info.VertexBufferNames[i];
            Dst.ResourceType = SHADER_RESOURCE_TYPE_BUFFER_SRV;
            Dst.Register     = BaseSlot + i;
            Dst.Space        = 0;
            Dst.ArraySize    = 1;
            Dst.ShaderStages = SHADER_TYPE_VERTEX;
            ResourceBindings.push_back(Dst);
        }
    }
}

} // namespace Diligent
