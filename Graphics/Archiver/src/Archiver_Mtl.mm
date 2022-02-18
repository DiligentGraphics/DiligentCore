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

#include <thread>
#include <sstream>
#include <filesystem>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "RenderDeviceMtlImpl.hpp"
#include "PipelineResourceSignatureMtlImpl.hpp"
#include "PipelineStateMtlImpl.hpp"
#include "ShaderMtlImpl.hpp"
#include "DeviceObjectArchiveMtlImpl.hpp"
#include "SerializedPipelineStateImpl.hpp"

#include "spirv_msl.hpp"

namespace filesystem = std::__fs::filesystem;

namespace Diligent
{

template <>
struct SerializedResourceSignatureImpl::SignatureTraits<PipelineResourceSignatureMtlImpl>
{
    static constexpr DeviceType Type = DeviceType::Metal_MacOS;

    template <SerializerMode Mode>
    using PRSSerializerType = PRSSerializerMtl<Mode>;
};

namespace
{

std::string GetTmpFolder()
{
    const auto ProcId   = getpid();
    const auto ThreadId = std::this_thread::get_id();
    std::stringstream FolderPath;
    FolderPath << filesystem::temp_directory_path().c_str()
               << "/DiligentArchiver-"
               << ProcId << '-'
               << ThreadId << '/';
    return FolderPath.str();
}

struct ShaderStageInfoMtl
{
    ShaderStageInfoMtl() {}

    ShaderStageInfoMtl(const SerializedShaderImpl* _pShader) :
        Type{_pShader->GetDesc().ShaderType},
        pShader{_pShader}
    {}

    // Needed only for ray tracing
    void Append(const SerializedShaderImpl*) {}

    constexpr Uint32 Count() const { return 1; }

    SHADER_TYPE                 Type    = SHADER_TYPE_UNKNOWN;
    const SerializedShaderImpl* pShader = nullptr;
};

#ifdef DILIGENT_DEBUG
inline SHADER_TYPE GetShaderStageType(const ShaderStageInfoMtl& Stage)
{
    return Stage.Type;
}
#endif

} // namespace


template <typename CreateInfoType>
void SerializedPipelineStateImpl::PatchShadersMtl(const CreateInfoType& CreateInfo, DeviceType DevType) noexcept(false)
{
    VERIFY_EXPR(DevType == DeviceType::Metal_MacOS || DevType == DeviceType::Metal_iOS);

    std::vector<ShaderStageInfoMtl> ShaderStages;
    SHADER_TYPE                     ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateMtlImpl::ExtractShaders<SerializedShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    std::vector<const SPIRVShaderResources*> StageResources{ShaderStages.size()};
    for (size_t i = 0; i < StageResources.size(); ++i)
    {
        StageResources[i] = ShaderStages[i].pShader->GetMtlShaderSPIRVResources();
    }

    auto** ppSignatures    = CreateInfo.ppResourceSignatures;
    auto   SignaturesCount = CreateInfo.ResourceSignaturesCount;

    IPipelineResourceSignature* DefaultSignatures[1] = {};
    if (CreateInfo.ResourceSignaturesCount == 0)
    {
        CreateDefaultResourceSignature<PipelineStateMtlImpl, PipelineResourceSignatureMtlImpl>(DevType, CreateInfo.PSODesc, ActiveShaderStages, StageResources);

        DefaultSignatures[0] = m_pDefaultSignature;
        SignaturesCount      = 1;
        ppSignatures         = DefaultSignatures;
    }

    {
        // Sort signatures by binding index.
        // Note that SignaturesCount will be overwritten with the maximum binding index.
        SignatureArray<PipelineResourceSignatureMtlImpl> Signatures      = {};
        SortResourceSignatures(ppSignatures, SignaturesCount, Signatures, SignaturesCount, DevType);

        std::array<MtlResourceCounters, MAX_RESOURCE_SIGNATURES> BaseBindings{};
        MtlResourceCounters                                      CurrBindings{};
        for (Uint32 s = 0; s < SignaturesCount; ++s)
        {
            BaseBindings[s] = CurrBindings;
            const auto& pSignature = Signatures[s];
            if (pSignature != nullptr)
                pSignature->ShiftBindings(CurrBindings);
        }

        VERIFY_EXPR(m_Data.Shaders[static_cast<size_t>(DevType)].empty());
        for (size_t j = 0; j < ShaderStages.size(); ++j)
        {
            const auto& Stage = ShaderStages[j];
            // Note that patched shader data contains some extra information
            // besides the byte code itself.
            const auto ShaderData = Stage.pShader->PatchShaderMtl(
                CreateInfo.PSODesc.Name, Signatures.data(), BaseBindings.data(), SignaturesCount, DevType); // May throw

            auto ShaderCI           = Stage.pShader->GetCreateInfo();
            ShaderCI.Source         = nullptr;
            ShaderCI.FilePath       = nullptr;
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_MTLB;
            ShaderCI.ByteCode       = ShaderData.Ptr();
            ShaderCI.ByteCodeSize   = ShaderData.Size();
            SerializeShaderCreateInfo(DevType, ShaderCI);
        }
        VERIFY_EXPR(m_Data.Shaders[static_cast<size_t>(DevType)].size() == ShaderStages.size());
    }
}

INSTANTIATE_PATCH_SHADER_METHODS(PatchShadersMtl, DeviceType DevType)
INSTANTIATE_DEVICE_SIGNATURE_METHODS(PipelineResourceSignatureMtlImpl)


static_assert(std::is_same<MtlArchiverResourceCounters, MtlResourceCounters>::value,
              "MtlArchiverResourceCounters and MtlResourceCounters must be same types");

struct SerializedShaderImpl::CompiledShaderMtlImpl final : CompiledShader
{
    String                                      MslSource;
    std::vector<uint32_t>                       SPIRV;
    std::unique_ptr<SPIRVShaderResources>       SPIRVResources;
    MtlFunctionArguments::BufferTypeInfoMapType BufferTypeInfoMap;
};

void SerializedShaderImpl::CreateShaderMtl(ShaderCreateInfo ShaderCI, String& CompilationLog)
{
    auto* pShaderMtl = new CompiledShaderMtlImpl{};
    m_pShaderMtl.reset(pShaderMtl);

    RefCntAutoPtr<IDataBlob> pLog;
    ShaderCI.ppCompilerOutput = pLog.RawDblPtr();

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
}

const SPIRVShaderResources* SerializedShaderImpl::GetMtlShaderSPIRVResources() const
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
        const auto GroupSize = SPIRVResources ?
            SPIRVResources->GetComputeGroupSize() :
            std::array<Uint32,3>{};

        Ser(GroupSize);
    }
}
} // namespace

SerializedData SerializedShaderImpl::PatchShaderMtl(const char*                                            PSOName,
                                                    const RefCntAutoPtr<PipelineResourceSignatureMtlImpl>* pSignatures,
                                                    const MtlResourceCounters*                             pBaseBindings,
                                                    const Uint32                                           SignatureCount,
                                                    DeviceType                                             DevType) const noexcept(false)
{
    VERIFY_EXPR(SignatureCount > 0);
    VERIFY_EXPR(pSignatures != nullptr);
    VERIFY_EXPR(pBaseBindings != nullptr);

    const auto TmpFolder = GetTmpFolder();
    filesystem::create_directories(TmpFolder.c_str());

    struct TmpDirRemover
    {
        explicit TmpDirRemover(const std::string& _Path) noexcept :
            Path{_Path}
        {}

        ~TmpDirRemover()
        {
            filesystem::remove_all(Path.c_str());
        }
    private:
        const std::string Path;
    };
    TmpDirRemover DirRemover{TmpFolder};

    const auto MetalFile    = TmpFolder + "Shader.metal";
    const auto MetalLibFile = TmpFolder + "Shader.metallib";

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

#define LOG_PATCH_SHADER_ERROR_AND_THROW(...)         \
    do                                                \
    {                                                 \
        char ErrorStr[512];                           \
        strerror_r(errno, ErrorStr, sizeof(ErrorStr));\
        LOG_ERROR_AND_THROW("Failed to patch shader '", GetDesc().Name, "' for PSO '", PSOName, "': ", ##__VA_ARGS__, " Error description: ", ErrorStr);\
    } while (false)

    // Save to 'Shader.metal'
    {
        FILE* File = fopen(MetalFile.c_str(), "wb");
        if (File == nullptr)
            LOG_PATCH_SHADER_ERROR_AND_THROW("failed to open temp file to save Metal shader source.");

        if (fwrite(MslSource.c_str(), sizeof(MslSource[0]) * MslSource.size(), 1, File) != 1)
            LOG_PATCH_SHADER_ERROR_AND_THROW("failed to save Metal shader source to a temp file.");

        fclose(File);
    }

    const auto& MtlProps = m_pDevice->GetMtlProperties();
    // Run user-defined MSL preprocessor
    if (MtlProps.MslPreprocessorCmd != nullptr)
    {
        String cmd{MtlProps.MslPreprocessorCmd};
        cmd += ' ';
        cmd += MetalFile;
        FILE* File = popen(cmd.c_str(), "r");
        if (File == nullptr)
            LOG_PATCH_SHADER_ERROR_AND_THROW("failed to run command-line Metal shader compiler.");

        char Output[512];
        while (fgets(Output, _countof(Output), File) != nullptr)
            printf("%s", Output);

        auto status = pclose(File);
        if (status == -1)
            LOG_PATCH_SHADER_ERROR_AND_THROW("failed to close msl preprocessor process.");
    }

    // https://developer.apple.com/documentation/metal/libraries/generating_and_loading_a_metal_library_symbol_file

    // Compile MSL to Metal library
    {
        String cmd{"xcrun "};
        cmd += (DevType == DeviceType::Metal_MacOS ? MtlProps.CompileOptionsMacOS : MtlProps.CompileOptionsIOS);
        cmd += " " + MetalFile + " -o " + MetalLibFile;

        FILE* File = popen(cmd.c_str(), "r");
        if (File == nullptr)
            LOG_PATCH_SHADER_ERROR_AND_THROW("failed to compile MSL source.");

        char Output[512];
        while (fgets(Output, _countof(Output), File) != nullptr)
            printf("%s", Output);

        auto status = pclose(File);
        if (status == -1)
            LOG_PATCH_SHADER_ERROR_AND_THROW("failed to close xcrun process");
    }

    // Read 'Shader.metallib'
    std::vector<Uint8> ByteCode;
    {
        FILE* File = fopen(MetalLibFile.c_str(), "rb");
        if (File == nullptr)
            LOG_PATCH_SHADER_ERROR_AND_THROW("failed to read shader library");

        fseek(File, 0, SEEK_END);
        const auto BytecodeSize = static_cast<size_t>(ftell(File));
        fseek(File, 0, SEEK_SET);
        ByteCode.resize(BytecodeSize);
        if (fread(ByteCode.data(), BytecodeSize, 1, File) != 1)
            ByteCode.clear();

        fclose(File);
    }

    if (ByteCode.empty())
        LOG_PATCH_SHADER_ERROR_AND_THROW("failed to load Metal shader library");

#undef LOG_PATCH_SHADER_ERROR_AND_THROW

    auto SerializeShaderData = [&](auto& Ser){
        Ser.SerializeBytes(ByteCode.data(), ByteCode.size() * sizeof(ByteCode[0]));
        SerializeBufferTypeInfoMapAndComputeGroupSize(Ser, BufferTypeInfoMap, GetDesc().ShaderType, pShaderMtl->SPIRVResources);
    };

    SerializedData ShaderData;
    {
        Serializer<SerializerMode::Measure> Ser;
        SerializeShaderData(Ser);
        ShaderData = Ser.AllocateData(GetRawAllocator());
    }

    {
        Serializer<SerializerMode::Write> Ser{ShaderData};
        SerializeShaderData(Ser);
        VERIFY_EXPR(Ser.IsEnded());
    }

    return ShaderData;
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
        const auto* pSerPRS = ClassPtrCast<SerializedResourceSignatureImpl>(Info.ppResourceSignatures[i]);
        const auto& Desc    = pSerPRS->GetDesc();

        Signatures[Desc.BindingIndex] = pSerPRS->GetDeviceSignature<PipelineResourceSignatureMtlImpl>(DeviceObjectArchiveBase::DeviceType::Metal_MacOS);
        SignaturesCount               = std::max(SignaturesCount, static_cast<Uint32>(Desc.BindingIndex) + 1);
    }

    const auto          ShaderStages        = (Info.ShaderStages == SHADER_TYPE_UNKNOWN ? static_cast<SHADER_TYPE>(~0u) : Info.ShaderStages);
    constexpr auto      SupportedStagesMask = (SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL | SHADER_TYPE_COMPUTE | SHADER_TYPE_TILE);
    MtlResourceCounters BaseBindings{};

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
