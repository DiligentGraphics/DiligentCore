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

#include "../../GraphicsEngineD3D12/include/pch.h"
#include "RenderDeviceD3D12Impl.hpp"
#include "PipelineResourceSignatureD3D12Impl.hpp"
#include "PipelineStateD3D12Impl.hpp"
#include "ShaderD3D12Impl.hpp"
#include "DeviceObjectArchiveD3D12Impl.hpp"

namespace Diligent
{
namespace
{

struct CompiledShaderD3D12 : SerializableShaderImpl::ICompiledShader
{
    ShaderD3D12Impl ShaderD3D12;

    CompiledShaderD3D12(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, const ShaderD3D12Impl::CreateInfo& D3D12ShaderCI) :
        ShaderD3D12{pRefCounters, nullptr, ShaderCI, D3D12ShaderCI, true}
    {}
};

inline const ShaderD3D12Impl* GetShaderD3D12(const SerializableShaderImpl* pShader)
{
    const auto* pCompiledShaderD3D12 = pShader->GetShader<const CompiledShaderD3D12>(DeviceObjectArchiveBase::DeviceType::Direct3D12);
    return pCompiledShaderD3D12 != nullptr ? &pCompiledShaderD3D12->ShaderD3D12 : nullptr;
}

struct ShaderStageInfoD3D12 : PipelineStateD3D12Impl::ShaderStageInfo
{
    ShaderStageInfoD3D12() :
        ShaderStageInfo{} {}

    ShaderStageInfoD3D12(const SerializableShaderImpl* pShader) :
        ShaderStageInfo{GetShaderD3D12(pShader)},
        Serializable{pShader}
    {}

    void Append(const SerializableShaderImpl* pShader)
    {
        ShaderStageInfo::Append(GetShaderD3D12(pShader));
        Serializable.push_back(pShader);
    }

    std::vector<const SerializableShaderImpl*> Serializable;
};
} // namespace

template <>
struct SerializableResourceSignatureImpl::SignatureTraits<PipelineResourceSignatureD3D12Impl>
{
    static constexpr DeviceType Type = DeviceType::Direct3D12;

    template <SerializerMode Mode>
    using PSOSerializerType = PSOSerializerD3D12<Mode>;
};

template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersD3D12(const CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS)
{
    std::vector<ShaderStageInfoD3D12> ShaderStages;
    SHADER_TYPE                       ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateD3D12Impl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    PipelineStateD3D12Impl::TShaderStages ShaderStagesD3D12{ShaderStages.size()};
    for (size_t i = 0; i < ShaderStagesD3D12.size(); ++i)
    {
        auto& Src     = ShaderStages[i];
        auto& Dst     = ShaderStagesD3D12[i];
        Dst.Type      = Src.Type;
        Dst.Shaders   = std::move(Src.Shaders);
        Dst.ByteCodes = std::move(Src.ByteCodes);
    }

    auto** ppSignatures    = CreateInfo.ppResourceSignatures;
    auto   SignaturesCount = CreateInfo.ResourceSignaturesCount;

    IPipelineResourceSignature* DefaultSignatures[1] = {};
    if (CreateInfo.ResourceSignaturesCount == 0)
    {
        if (!CreateDefaultResourceSignature<PipelineStateD3D12Impl>(DefPRS, CreateInfo.PSODesc, ActiveShaderStages, ShaderStagesD3D12, nullptr))
            return false;

        DefaultSignatures[0] = DefPRS.pPRS;
        SignaturesCount      = 1;
        ppSignatures         = DefaultSignatures;
    }

    try
    {
        // Sort signatures by binding index.
        // Note that SignaturesCount will be overwritten with the maximum binding index.
        SignatureArray<PipelineResourceSignatureD3D12Impl> Signatures = {};
        SortResourceSignatures(ppSignatures, SignaturesCount, Signatures, SignaturesCount);

        RootSignatureD3D12 RootSig{nullptr, nullptr, Signatures.data(), SignaturesCount, 0};
        PipelineStateD3D12Impl::RemapShaderResources(ShaderStagesD3D12,
                                                     Signatures.data(),
                                                     SignaturesCount,
                                                     RootSig,
                                                     m_pSerializationDevice->GetD3D12Properties().pDxCompiler);
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to remap shader resources in Direct3D12 shaders");
        return false;
    }

    TShaderIndices ShaderIndices;
    for (size_t j = 0; j < ShaderStagesD3D12.size(); ++j)
    {
        const auto& Stage = ShaderStagesD3D12[j];
        for (size_t i = 0; i < Stage.Count(); ++i)
        {
            const auto& CI        = ShaderStages[j].Serializable[i]->GetCreateInfo();
            const auto& pBytecode = Stage.ByteCodes[i];

            SerializeShaderBytecode(ShaderIndices, DeviceType::Direct3D12, CI, pBytecode->GetBufferPointer(), pBytecode->GetBufferSize());
        }
    }

    Data.PerDeviceData[static_cast<size_t>(DeviceType::Direct3D12)] = SerializeShadersForPSO(ShaderIndices);
    return true;
}

template bool ArchiverImpl::PatchShadersD3D12<GraphicsPipelineStateCreateInfo>(const GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersD3D12<ComputePipelineStateCreateInfo>(const ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersD3D12<TilePipelineStateCreateInfo>(const TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersD3D12<RayTracingPipelineStateCreateInfo>(const RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);


void SerializableShaderImpl::CreateShaderD3D12(IReferenceCounters* pRefCounters, ShaderCreateInfo& ShaderCI, String& CompilationLog)
{
    const auto& D3D12Props  = m_pDevice->GetD3D12Properties();
    const auto& DeviceInfo  = m_pDevice->GetDeviceInfo();
    const auto& AdapterInfo = m_pDevice->GetAdapterInfo();

    const ShaderD3D12Impl::CreateInfo D3D12ShaderCI{
        D3D12Props.pDxCompiler,
        DeviceInfo,
        AdapterInfo,
        D3D12Props.ShaderVersion //
    };
    CreateShader<CompiledShaderD3D12>(DeviceType::Direct3D12, CompilationLog, "Direct3D12", pRefCounters, ShaderCI, D3D12ShaderCI);
}


template PipelineResourceSignatureD3D12Impl* SerializableResourceSignatureImpl::GetSignature<PipelineResourceSignatureD3D12Impl>() const;

void SerializableResourceSignatureImpl::CreatePRSD3D12(IReferenceCounters*                  pRefCounters,
                                                       const PipelineResourceSignatureDesc& Desc,
                                                       SHADER_TYPE                          ShaderStages)
{
    CreateSignature<PipelineResourceSignatureD3D12Impl>(pRefCounters, Desc, ShaderStages);
}


void SerializationDeviceImpl::GetPipelineResourceBindingsD3D12(const PipelineResourceBindingAttribs& Info,
                                                               std::vector<PipelineResourceBinding>& ResourceBindings)
{
    const auto ShaderStages = (Info.ShaderStages == SHADER_TYPE_UNKNOWN ? static_cast<SHADER_TYPE>(~0u) : Info.ShaderStages);

    SignatureArray<PipelineResourceSignatureD3D12Impl> Signatures      = {};
    Uint32                                             SignaturesCount = 0;
    SortResourceSignatures(Info.ppResourceSignatures, Info.ResourceSignaturesCount, Signatures, SignaturesCount);

    RootSignatureD3D12 RootSig{nullptr, nullptr, Signatures.data(), SignaturesCount, 0};
    const bool         HasSpaces = RootSig.GetTotalSpaces() > 1;

    for (Uint32 sign = 0; sign < SignaturesCount; ++sign)
    {
        const auto& pSignature = Signatures[sign];
        if (pSignature == nullptr)
            continue;

        const auto BaseRegisterSpace = RootSig.GetBaseRegisterSpace(sign);

        for (Uint32 r = 0; r < pSignature->GetTotalResourceCount(); ++r)
        {
            const auto& ResDesc = pSignature->GetResourceDesc(r);
            const auto& ResAttr = pSignature->GetResourceAttribs(r);
            if ((ResDesc.ShaderStages & ShaderStages) == 0)
                continue;

            ResourceBindings.push_back(ResDescToPipelineResBinding(ResDesc, ResDesc.ShaderStages, ResAttr.Register, BaseRegisterSpace + ResAttr.Space));
        }
    }
}

void ExtractShadersD3D12(const RayTracingPipelineStateCreateInfo& CreateInfo, RayTracingShaderMap& ShaderMap)
{
    std::vector<ShaderStageInfoD3D12> ShaderStages;
    SHADER_TYPE                       ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateD3D12Impl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    ExtractRayTracingShaders(ShaderStages, ShaderMap);
}

} // namespace Diligent
