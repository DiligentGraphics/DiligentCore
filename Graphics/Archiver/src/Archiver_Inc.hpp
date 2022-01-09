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

namespace Diligent
{
namespace
{

template <typename SignatureType>
using SignatureArray = std::array<RefCntAutoPtr<SignatureType>, MAX_RESOURCE_SIGNATURES>;

template <typename SignatureType>
static void SortResourceSignatures(IPipelineResourceSignature**   ppSrcSignatures,
                                   Uint32                         SrcSignaturesCount,
                                   SignatureArray<SignatureType>& SortedSignatures,
                                   Uint32&                        SortedSignaturesCount)
{
    for (Uint32 i = 0; i < SrcSignaturesCount; ++i)
    {
        const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(ppSrcSignatures[i]);
        VERIFY_EXPR(pSerPRS != nullptr);

        const auto& Desc = pSerPRS->GetDesc();

        VERIFY(!SortedSignatures[Desc.BindingIndex], "Multiple signatures use the same binding index (", Desc.BindingIndex, ").");
        SortedSignatures[Desc.BindingIndex] = pSerPRS->template GetSignature<SignatureType>();

        SortedSignaturesCount = std::max(SortedSignaturesCount, Uint32{Desc.BindingIndex} + 1u);
    }
}

} // namespace


template <typename PipelineStateImplType, typename ShaderStagesArrayType, typename... ExtraArgsType>
bool ArchiverImpl::CreateDefaultResourceSignature(DefaultPRSInfo&              DefPRS,
                                                  const PipelineStateDesc&     PSODesc,
                                                  SHADER_TYPE                  ActiveShaderStages,
                                                  const ShaderStagesArrayType& ShaderStages,
                                                  const ExtraArgsType&... ExtraArgs)
{
    try
    {
        auto SignDesc = PipelineStateImplType::GetDefaultResourceSignatureDesc(ShaderStages, PSODesc.Name, PSODesc.ResourceLayout, PSODesc.SRBAllocationGranularity, ExtraArgs...);
        SignDesc.SetName(DefPRS.UniqueName.c_str());

        RefCntAutoPtr<IPipelineResourceSignature> pDefaultPRS;
        m_pSerializationDevice->CreatePipelineResourceSignature(SignDesc, DefPRS.DeviceFlags, ActiveShaderStages, &pDefaultPRS);
        if (pDefaultPRS == nullptr)
            return false;

        if (DefPRS.pPRS)
        {
            if (!(DefPRS.pPRS.RawPtr<SerializableResourceSignatureImpl>()->IsCompatible(*pDefaultPRS.RawPtr<SerializableResourceSignatureImpl>(), DefPRS.DeviceFlags)))
            {
                LOG_ERROR_MESSAGE("Default signatures do not match between different backends");
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
    return true;
}


template <typename ShaderType, typename... ArgTypes>
void SerializableShaderImpl::CreateShader(DeviceType          Type,
                                          String&             CompilationLog,
                                          const char*         DeviceTypeName,
                                          IReferenceCounters* pRefCounters,
                                          ShaderCreateInfo&   ShaderCI,
                                          const ArgTypes&... Args)
{
    // Mem leak when used RefCntAutoPtr
    IDataBlob* pLog           = nullptr;
    ShaderCI.ppCompilerOutput = &pLog;
    try
    {
        m_Shaders[static_cast<size_t>(Type)] = std::make_unique<ShaderType>(pRefCounters, ShaderCI, Args...);
    }
    catch (...)
    {
        if (pLog && pLog->GetConstDataPtr())
        {
            CompilationLog += "Failed to compile ";
            CompilationLog += DeviceTypeName;
            CompilationLog += " shader:\n";
            CompilationLog += static_cast<const char*>(pLog->GetConstDataPtr());
        }
    }

    if (pLog)
        pLog->Release();
}


template <typename ImplType>
struct SerializableResourceSignatureImpl::TPRS final : PRSWapperBase
{
    ImplType PRS;

    TPRS(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& SignatureDesc, SHADER_TYPE ShaderStages) :
        PRS{pRefCounters, nullptr, SignatureDesc, ShaderStages, true /*Pretend device internal to allow null device*/}
    {}

    IPipelineResourceSignature* GetPRS() override { return &PRS; }
};


template <typename SignatureImplType>
void SerializableResourceSignatureImpl::CreateSignature(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages)
{
    using Traits                = SignatureTraits<SignatureImplType>;
    using MeasureSerializerType = typename Traits::template PSOSerializerType<SerializerMode::Measure>;
    using WriteSerializerType   = typename Traits::template PSOSerializerType<SerializerMode::Write>;

    auto PRSWrpr = std::make_unique<TPRS<SignatureImplType>>(pRefCounters, Desc, ShaderStages);
    AddPRSDesc(PRSWrpr->PRS.GetDesc());

    const auto InternalData = PRSWrpr->PRS.GetInternalData();

    {
        Serializer<SerializerMode::Measure> MeasureSer;
        MeasureSerializerType::SerializePRSInternalData(MeasureSer, InternalData, nullptr);

        PRSWrpr->Mem = SerializedMemory{MeasureSer.GetSize(nullptr)};
    }

    {
        Serializer<SerializerMode::Write> Ser{PRSWrpr->Mem.Ptr(), PRSWrpr->Mem.Size()};
        WriteSerializerType::SerializePRSInternalData(Ser, InternalData, nullptr);
        VERIFY_EXPR(Ser.IsEnd());
    }

    m_pPRSWrappers[static_cast<size_t>(Traits::Type)] = std::move(PRSWrpr);
}


using RayTracingShaderMap = std::unordered_map<const IShader*, /*Index in TShaderIndices*/ Uint32>;

void ExtractShadersD3D12(const RayTracingPipelineStateCreateInfo& CreateInfo, RayTracingShaderMap& ShaderMap);
void ExtractShadersVk(const RayTracingPipelineStateCreateInfo& CreateInfo, RayTracingShaderMap& ShaderMap);

template <typename ShaderStage>
void ExtractRayTracingShaders(const std::vector<ShaderStage>& ShaderStages, RayTracingShaderMap& ShaderMap)
{
    Uint32 ShaderIndex = 0;
    for (auto& Stage : ShaderStages)
    {
        for (auto* pShader : Stage.Serializable)
        {
            if (ShaderMap.emplace(pShader, ShaderIndex).second)
                ++ShaderIndex;
        }
    }
}

} // namespace Diligent
