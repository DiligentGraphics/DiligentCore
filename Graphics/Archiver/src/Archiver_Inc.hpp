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

#include <memory>
#include <array>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "PSOSerializer.hpp"

namespace Diligent
{

namespace
{

template <typename SignatureType>
using SignatureArray = std::array<RefCntAutoPtr<SignatureType>, MAX_RESOURCE_SIGNATURES>;

template <typename SignatureType>
void SortResourceSignatures(IPipelineResourceSignature**                  ppSrcSignatures,
                            Uint32                                        SrcSignaturesCount,
                            SignatureArray<SignatureType>&                SortedSignatures,
                            Uint32&                                       SortedSignaturesCount,
                            SerializableResourceSignatureImpl::DeviceType Type)
{
    for (Uint32 i = 0; i < SrcSignaturesCount; ++i)
    {
        const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(ppSrcSignatures[i]);
        VERIFY_EXPR(pSerPRS != nullptr);

        const auto& Desc = pSerPRS->GetDesc();

        VERIFY(!SortedSignatures[Desc.BindingIndex], "Multiple signatures use the same binding index (", Desc.BindingIndex, ").");
        SortedSignatures[Desc.BindingIndex] = pSerPRS->template GetDeviceSignature<SignatureType>(Type);

        SortedSignaturesCount = std::max(SortedSignaturesCount, Uint32{Desc.BindingIndex} + 1u);
    }
}

template <typename SignatureType>
void SortResourceSignatures(IPipelineResourceSignature**   ppSrcSignatures,
                            Uint32                         SrcSignaturesCount,
                            SignatureArray<SignatureType>& SortedSignatures,
                            Uint32&                        SortedSignaturesCount)
{
    constexpr auto Type = SerializableResourceSignatureImpl::SignatureTraits<SignatureType>::Type;
    SortResourceSignatures<SignatureType>(ppSrcSignatures, SrcSignaturesCount,
                                          SortedSignatures, SortedSignaturesCount,
                                          Type);
}

} // namespace


template <typename PipelineStateImplType, typename SignatureImplType, typename ShaderStagesArrayType, typename... ExtraArgsType>
bool ArchiverImpl::CreateDefaultResourceSignature(DeviceType                                        Type,
                                                  RefCntAutoPtr<SerializableResourceSignatureImpl>& pSignature,
                                                  const PipelineStateDesc&                          PSODesc,
                                                  SHADER_TYPE                                       ActiveShaderStageFlags,
                                                  const ShaderStagesArrayType&                      ShaderStages,
                                                  const ExtraArgsType&... ExtraArgs)
{
    try
    {
        auto SignDesc = PipelineStateImplType::GetDefaultResourceSignatureDesc(ShaderStages, PSODesc.Name, PSODesc.ResourceLayout, PSODesc.SRBAllocationGranularity, ExtraArgs...);

        if (!pSignature)
        {
            // Get unique name that is not yet present in the cache
            const auto UniqueName = GetDefaultPRSName(PSODesc.Name);
            SignDesc.SetName(UniqueName.c_str());

            // Create empty serializable signature
            m_pSerializationDevice->CreateSerializableResourceSignature(&pSignature, UniqueName.c_str());
            if (!pSignature)
                return false;

            // Even though we are not going to reuse the default PRS, we need to add it to the cache
            // to make its name unavailable for future signatures
            if (!CachePipelineResourceSignature(pSignature))
            {
                UNEXPECTED("Failed to add default signature '", UniqueName, "' to the cache. This should've never happened as we generated the unique name.");
                return false;
            }
        }
        else
        {
            // Override the name to make sure it is consistent for all devices
            SignDesc.SetName(pSignature->GetName());
        }

        pSignature->CreateDeviceSignature<SignatureImplType>(Type, SignDesc, ActiveShaderStageFlags);
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
void SerializableResourceSignatureImpl::CreateDeviceSignature(DeviceType                           Type,
                                                              const PipelineResourceSignatureDesc& Desc,
                                                              SHADER_TYPE                          ShaderStages)
{
    using Traits                = SignatureTraits<SignatureImplType>;
    using MeasureSerializerType = typename Traits::template PSOSerializerType<SerializerMode::Measure>;
    using WriteSerializerType   = typename Traits::template PSOSerializerType<SerializerMode::Write>;

    VERIFY_EXPR(Type == Traits::Type || (Type == DeviceType::Metal_iOS && Traits::Type == DeviceType::Metal_MacOS));
    VERIFY(!m_pDeviceSignatures[static_cast<size_t>(Type)], "Signature for this device type has already been initialized");

    auto  pDeviceSignature = std::make_unique<TPRS<SignatureImplType>>(GetReferenceCounters(), Desc, ShaderStages);
    auto& DeviceSignature  = *pDeviceSignature;
    // We must initialize at least one device signature before calling InitCommonData()
    m_pDeviceSignatures[static_cast<size_t>(Type)] = std::move(pDeviceSignature);

    const auto& SignDesc = DeviceSignature.GetPRS()->GetDesc();
    InitCommonData(SignDesc);

    const auto InternalData = DeviceSignature.PRS.GetInternalData();

    const auto& CommonDesc = GetDesc();

    // Check if the device signature description differs from common description
    bool SpecialDesc = !(CommonDesc == SignDesc);

    {
        Serializer<SerializerMode::Measure> MeasureSer;

        MeasureSer(SpecialDesc);
        if (SpecialDesc)
            PSOSerializer<SerializerMode::Measure>::SerializePRSDesc(MeasureSer, SignDesc, nullptr);

        MeasureSerializerType::SerializePRSInternalData(MeasureSer, InternalData, nullptr);

        DeviceSignature.Mem = SerializedMemory{MeasureSer.GetSize(nullptr)};
    }

    {
        Serializer<SerializerMode::Write> Ser{DeviceSignature.Mem.Ptr(), DeviceSignature.Mem.Size()};

        Ser(SpecialDesc);
        if (SpecialDesc)
            PSOSerializer<SerializerMode::Write>::SerializePRSDesc(Ser, SignDesc, nullptr);

        WriteSerializerType::SerializePRSInternalData(Ser, InternalData, nullptr);

        VERIFY_EXPR(Ser.IsEnd());
    }
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
