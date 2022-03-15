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
#include "FileSystem.hpp"
#include "FileWrapper.hpp"
#include "DataBlobImpl.hpp"

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


template <SerializerMode Mode>
void SerializeMSLData(Serializer<Mode>&                       Ser,
                      const SHADER_TYPE                       ShaderType,
                      const ParsedMSLInfo::BufferInfoMapType& BufferInfoMap,
                      const ComputeGroupSizeType&             ComputeGroupSize)
{
    // Same as DeviceObjectArchiveMtlImpl::UnpackShader
    
    const auto Count = static_cast<Uint32>(BufferInfoMap.size());
    Ser(Count);
    for (auto& it : BufferInfoMap)
    {
        const auto* Name    = it.first.c_str();
        const auto* AltName = it.second.AltName.c_str();
        const auto  Space   = it.second.Space;
        Ser(Name, AltName, Space);
    }

    if (ShaderType == SHADER_TYPE_COMPUTE)
    {
        Ser(ComputeGroupSize);
    }
}

struct TmpDirRemover
{
    explicit TmpDirRemover(const std::string& _Path) noexcept :
        Path{_Path}
    {}

    ~TmpDirRemover()
    {
        if (!Path.empty())
        {
            filesystem::remove_all(Path.c_str());
        }
    }
private:
    const std::string Path;
};

#define LOG_ERRNO_MESSAGE(...)                                           \
    do                                                                   \
    {                                                                    \
        char ErrorStr[512];                                              \
        strerror_r(errno, ErrorStr, sizeof(ErrorStr));                   \
        LOG_ERROR_MESSAGE(__VA_ARGS__, " Error description: ", ErrorStr);\
    } while (false)

bool SaveMslToFile(const std::string& MslSource, const std::string& MetalFile)
{
    auto* File = fopen(MetalFile.c_str(), "wb");
    if (File == nullptr)
    {
        LOG_ERRNO_MESSAGE("failed to open file '", MetalFile,"' to save Metal shader source.");
        return false;
    }
    
    if (fwrite(MslSource.c_str(), sizeof(MslSource[0]) * MslSource.size(), 1, File) != 1)
    {
        LOG_ERRNO_MESSAGE("failed to save Metal shader source to file '", MetalFile, '\'');
        return false;
    }
    
    fclose(File);
    
    return true;
}

// Runs a custom MSL processing command
bool PreprocessMsl(const std::string& MslPreprocessorCmd, const std::string& MetalFile)
{
    auto cmd{MslPreprocessorCmd};
    cmd += " \"";
    cmd += MetalFile;
    cmd += '\"';
    FILE* Pipe = FileSystem::popen(cmd.c_str(), "r");
    if (Pipe == nullptr)
    {
        LOG_ERRNO_MESSAGE("failed to run command-line Metal shader compiler with command line \"", cmd, '\"');
        return false;
    }
    
    char Output[512];
    while (fgets(Output, _countof(Output), Pipe) != nullptr)
        printf("%s", Output);

    auto status = FileSystem::pclose(Pipe);
    if (status != 0)
    {
        // errno is not useful
        LOG_ERROR_MESSAGE("failed to close msl preprocessor process (error code: ", status, ").");
        return false;
    }
    
    return true;
}

// Compiles MSL into a metal library using xcrun
// https://developer.apple.com/documentation/metal/libraries/generating_and_loading_a_metal_library_symbol_file
bool CompileMsl(const std::string& CompileOptions,
                const std::string& MetalFile,
                const std::string& MetalLibFile) noexcept(false)
{
    String cmd{"xcrun "};
    cmd += CompileOptions;
    cmd += " \"" + MetalFile + "\" -o \"" + MetalLibFile + '\"';

    FILE* Pipe = FileSystem::popen(cmd.c_str(), "r");
    if (Pipe == nullptr)
    {
        LOG_ERRNO_MESSAGE("failed to compile MSL source with command line \"", cmd, '\"');
        return false;
    }
    
    char Output[512];
    while (fgets(Output, _countof(Output), Pipe) != nullptr)
        printf("%s", Output);

    auto status = FileSystem::pclose(Pipe);
    if (status != 0)
    {
        // errno is not useful
        LOG_ERROR_MESSAGE("failed to close xcrun process (error code: ", status, ").");
        return false;
    }
    
    return true;
}

RefCntAutoPtr<DataBlobImpl> ReadFile(const char* FilePath)
{
    FileWrapper File{FilePath, EFileAccessMode::Read};
    if (!File)
    {
        LOG_ERRNO_MESSAGE("Failed to open file '", FilePath, "'.");
        return {};
    }
    
    auto pFileData = DataBlobImpl::Create();
    if (!File->Read(pFileData))
    {
        LOG_ERRNO_MESSAGE("Failed to read '", FilePath, "'.");
        return {};
    }
    
    return pFileData;
}

#undef LOG_ERRNO_MESSAGE


struct CompiledShaderMtl final : SerializedShaderImpl::CompiledShader
{
    CompiledShaderMtl(
        IReferenceCounters*                           pRefCounters,
        const ShaderCreateInfo&                       ShaderCI,
        const RenderDeviceInfo&                       DeviceInfo,
        const GraphicsAdapterInfo&                    AdapterInfo,
        const SerializationDeviceImpl::MtlProperties& MtlProps)
    {
        MSLData = ShaderMtlImpl::PrepareMSLData(ShaderCI, DeviceInfo, AdapterInfo); // may throw exception

        if (!MtlProps.MslPreprocessorCmd.empty())
        {
            const auto TmpFolder = GetTmpFolder();
            filesystem::create_directories(TmpFolder);
            TmpDirRemover DirRemover{TmpFolder};
            
            const auto MetalFile = TmpFolder + ShaderCI.Desc.Name + ".metal";
        
            // Save MSL source to a file
            if (!SaveMslToFile(MSLData.Source, MetalFile))
                LOG_ERROR_AND_THROW("Failed to save MSL source to a temp file for shader '", ShaderCI.Desc.Name,"'.");

            // Run the preprocessor
            if (!PreprocessMsl(MtlProps.MslPreprocessorCmd, MetalFile))
                LOG_ERROR_AND_THROW("Failed to preprocess MSL source for shader '", ShaderCI.Desc.Name,"'.");
            
            // Read processed MSL source back
            auto pProcessedMsl = ReadFile(MetalFile.c_str());
            if (!pProcessedMsl)
                LOG_ERROR_AND_THROW("Failed to read preprocessed MSL source for shader '", ShaderCI.Desc.Name,"'.");

            const auto* pMslStr = pProcessedMsl->GetConstDataPtr<char>();
            MSLData.Source = {pMslStr, pMslStr + pProcessedMsl->GetSize()};
        }
        
        ParsedMsl = ShaderMtlImpl::ParseMSL(MSLData);
    }
    
    MSLParseData  MSLData;
    ParsedMSLInfo ParsedMsl;
};

inline const ParsedMSLInfo* GetParsedMsl(const SerializedShaderImpl* pShader, SerializedShaderImpl::DeviceType Type)
{
    const auto* pCompiledShaderMtl = pShader->GetShader<const CompiledShaderMtl>(Type);
    return pCompiledShaderMtl != nullptr ? &pCompiledShaderMtl->ParsedMsl : nullptr;
}


struct CompileMtlShaderAttribs
{
    const SerializedShaderImpl::DeviceType        DevType;
    const SerializationDeviceImpl::MtlProperties& MtlProps;

    const SerializedShaderImpl* pSerializedShader;
    
    const char*         PSOName;
    const std::string&  DumpFolder;
    
    const SignatureArray<PipelineResourceSignatureMtlImpl>& Signatures;
    const Uint32                                            SignatureCount;

    const std::array<MtlResourceCounters, MAX_RESOURCE_SIGNATURES>& BaseBindings;
};

SerializedData CompileMtlShader(const CompileMtlShaderAttribs& Attribs) noexcept(false)
{
    using DeviceType = SerializedShaderImpl::DeviceType;
    
    VERIFY_EXPR(Attribs.SignatureCount > 0);
    const auto* PSOName = Attribs.PSOName != nullptr ? Attribs.PSOName : "<unknown>";
    
    const auto& ShDesc     = Attribs.pSerializedShader->GetDesc();
    const auto* ShaderName = ShDesc.Name;
    VERIFY_EXPR(ShaderName != nullptr);

    const std::string WorkingFolder =
        [&](){
            if (Attribs.DumpFolder.empty())
                return GetTmpFolder();

            auto Folder = Attribs.DumpFolder;
            if (Folder.back() != FileSystem::GetSlashSymbol())
                Folder += FileSystem::GetSlashSymbol();

            return Folder;
        }();
    filesystem::create_directories(WorkingFolder);

    TmpDirRemover DirRemover{Attribs.DumpFolder.empty() ? WorkingFolder : ""};

    const auto MetalFile    = WorkingFolder + ShaderName + ".metal";
    const auto MetalLibFile = WorkingFolder + ShaderName + ".metallib";
    
    const auto* pCompiledShader = Attribs.pSerializedShader->GetShader<const CompiledShaderMtl>(Attribs.DevType);

    const auto& MslData   = pCompiledShader->MSLData;
    const auto& ParsedMsl = pCompiledShader->ParsedMsl;

#define LOG_PATCH_SHADER_ERROR_AND_THROW(...)\
    LOG_ERROR_AND_THROW("Failed to patch shader '", ShaderName, "' for PSO '", PSOName, "': ", ##__VA_ARGS__)
    
    std::string MslSource;
    if (ParsedMsl.pParser != nullptr)
    {
        const auto ResRemapping = PipelineStateMtlImpl::GetResourceMap(
            ParsedMsl,
            Attribs.Signatures.data(),
            Attribs.SignatureCount,
            Attribs.BaseBindings.data(),
            ShDesc,
            PSOName); // may throw exception

        MslSource = ParsedMsl.pParser->RemapResources(ResRemapping);
        if (MslSource.empty())
            LOG_ERROR_AND_THROW("Failed to remap MSL resources");
    }
    else
    {
        MslSource = MslData.Source;
    }
    
    if (!SaveMslToFile(MslSource, MetalFile))
        LOG_PATCH_SHADER_ERROR_AND_THROW("Failed to save MSL source to a temp file.");

    // Compile MSL to Metal library
    const auto& CompileOptions = Attribs.DevType == DeviceType::Metal_MacOS ?
        Attribs.MtlProps.CompileOptionsMacOS :
        Attribs.MtlProps.CompileOptionsIOS;
    if (!CompileMsl(CompileOptions, MetalFile, MetalLibFile))
        LOG_PATCH_SHADER_ERROR_AND_THROW("Failed to create metal library.");

    // Read the bytecode from metal library
    auto pByteCode = ReadFile(MetalLibFile.c_str());
    if (!pByteCode)
        LOG_PATCH_SHADER_ERROR_AND_THROW("Failed to read Metal shader library.");

#undef LOG_PATCH_SHADER_ERROR_AND_THROW

    auto SerializeShaderData = [&](auto& Ser){
        Ser.SerializeBytes(pByteCode->GetConstDataPtr(), pByteCode->GetSize());
        SerializeMSLData(Ser, ShDesc.ShaderType, ParsedMsl.BufferInfoMap, MslData.ComputeGroupSize);
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

} // namespace

template <typename CreateInfoType>
void SerializedPipelineStateImpl::PatchShadersMtl(const CreateInfoType& CreateInfo, DeviceType DevType, const std::string& DumpDir) noexcept(false)
{
    VERIFY_EXPR(DevType == DeviceType::Metal_MacOS || DevType == DeviceType::Metal_iOS);

    std::vector<ShaderStageInfoMtl> ShaderStages;
    SHADER_TYPE                     ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateMtlImpl::ExtractShaders<SerializedShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    std::vector<const ParsedMSLInfo*> StageResources{ShaderStages.size()};
    for (size_t i = 0; i < StageResources.size(); ++i)
    {
        StageResources[i] = GetParsedMsl(ShaderStages[i].pShader, DevType);
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
            const auto ShaderData = CompileMtlShader({
                DevType,
                m_pSerializationDevice->GetMtlProperties(),
                Stage.pShader,
                CreateInfo.PSODesc.Name,
                DumpDir,
                Signatures,
                SignaturesCount,
                BaseBindings,
                }); // May throw

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

INSTANTIATE_PATCH_SHADER_METHODS(PatchShadersMtl, DeviceType DevType, const std::string& DumpDir)
INSTANTIATE_DEVICE_SIGNATURE_METHODS(PipelineResourceSignatureMtlImpl)

void SerializedShaderImpl::CreateShaderMtl(const ShaderCreateInfo& ShaderCI, DeviceType Type) noexcept(false)
{
    const auto& DeviceInfo  = m_pDevice->GetDeviceInfo();
    const auto& AdapterInfo = m_pDevice->GetAdapterInfo();
    const auto& MtlProps    = m_pDevice->GetMtlProperties();
    CreateShader<CompiledShaderMtl>(Type, nullptr, ShaderCI, DeviceInfo, AdapterInfo, MtlProps);
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
