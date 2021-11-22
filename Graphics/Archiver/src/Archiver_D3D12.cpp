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

#include "../../GraphicsEngineD3D12/include/pch.h"
#include "RenderDeviceD3D12Impl.hpp"
#include "PipelineResourceSignatureD3D12Impl.hpp"
#include "PipelineStateD3D12Impl.hpp"
#include "ShaderD3D12Impl.hpp"
#include "DeviceObjectArchiveD3D12Impl.hpp"

namespace Diligent
{

template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersD3D12(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS)
{
    struct ShaderStageInfoD3D12 : PipelineStateD3D12Impl::ShaderStageInfo
    {
        ShaderStageInfoD3D12() :
            ShaderStageInfo{} {}

        ShaderStageInfoD3D12(const SerializableShaderImpl* pShader) :
            ShaderStageInfo{pShader->GetShaderD3D12()},
            Serializable{pShader}
        {}

        void Append(const SerializableShaderImpl* pShader)
        {
            ShaderStageInfo::Append(pShader->GetShaderD3D12());
            Serializable.push_back(pShader);
        }

        std::vector<const SerializableShaderImpl*> Serializable;
    };

    TShaderIndices ShaderIndices;

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

    IPipelineResourceSignature* DefaultSignatures[1] = {};
    if (CreateInfo.ResourceSignaturesCount == 0)
    {
        if (!CreateDefaultResourceSignature(
                DefPRS, [&]() {
                    std::vector<PipelineResourceDesc> Resources;
                    std::vector<ImmutableSamplerDesc> ImmutableSamplers;

                    auto SignDesc = PipelineStateD3D12Impl::GetDefaultResourceSignatureDesc(
                        ShaderStagesD3D12, CreateInfo.PSODesc.ResourceLayout, "Default resource signature", nullptr, Resources, ImmutableSamplers);
                    SignDesc.Name = DefPRS.UniqueName.c_str();

                    RefCntAutoPtr<IPipelineResourceSignature> pDefaultPRS;
                    m_pSerializationDevice->CreatePipelineResourceSignature(SignDesc, DefPRS.DeviceFlags, ActiveShaderStages, &pDefaultPRS);
                    return pDefaultPRS;
                }))
        {
            return false;
        }

        DefaultSignatures[0]               = DefPRS.pPRS;
        CreateInfo.ResourceSignaturesCount = 1;
        CreateInfo.ppResourceSignatures    = DefaultSignatures;
        CreateInfo.PSODesc.ResourceLayout  = {};
    }

    try
    {
        SignatureArray<PipelineResourceSignatureD3D12Impl> Signatures      = {};
        Uint32                                             SignaturesCount = 0;
        SortResourceSignatures(CreateInfo, Signatures, SignaturesCount);

        RootSignatureD3D12 RootSig{nullptr, nullptr, Signatures.data(), SignaturesCount, 0};
        PipelineStateD3D12Impl::RemapShaderResources(ShaderStagesD3D12,
                                                     Signatures.data(),
                                                     SignaturesCount,
                                                     RootSig,
                                                     m_pSerializationDevice->GetDxCompilerForDirect3D12());
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to remap shader resources in Direct3D12 shaders");
        return false;
    }

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

    // AZ TODO: map ray tracing shaders to shader indices

    SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[static_cast<Uint32>(DeviceType::Direct3D12)]);
    return true;
}

template bool ArchiverImpl::PatchShadersD3D12<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersD3D12<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersD3D12<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersD3D12<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);


struct SerializableShaderImpl::CompiledShaderD3D12 : ICompiledShader
{
    ShaderD3D12Impl ShaderD3D12;

    CompiledShaderD3D12(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, const ShaderD3D12Impl::CreateInfo& D3D12ShaderCI) :
        ShaderD3D12{pRefCounters, nullptr, ShaderCI, D3D12ShaderCI, true}
    {}
};

const ShaderD3D12Impl* SerializableShaderImpl::GetShaderD3D12() const
{
    return m_pShaderD3D12 ? &reinterpret_cast<CompiledShaderD3D12*>(m_pShaderD3D12.get())->ShaderD3D12 : nullptr;
}

void SerializableShaderImpl::CreateShaderD3D12(IReferenceCounters* pRefCounters, ShaderCreateInfo& ShaderCI, String& CompilationLog)
{
    const ShaderD3D12Impl::CreateInfo D3D12ShaderCI{
        m_pDevice->GetDxCompilerForDirect3D12(),
        m_pDevice->GetDeviceInfo(),
        m_pDevice->GetAdapterInfo(),
        m_pDevice->GetD3D12ShaderVersion() //
    };
    CreateShader<CompiledShaderD3D12>(m_pShaderD3D12, CompilationLog, "Direct3D12", pRefCounters, ShaderCI, D3D12ShaderCI);
}


PipelineResourceSignatureD3D12Impl* SerializableResourceSignatureImpl::GetSignatureD3D12() const
{
    return m_pPRSD3D12 ? m_pPRSD3D12->GetPRS<PipelineResourceSignatureD3D12Impl>() : nullptr;
}

const SerializedMemory* SerializableResourceSignatureImpl::GetSerializedMemoryD3D12() const
{
    return m_pPRSD3D12 ? m_pPRSD3D12->GetMem() : nullptr;
}

void SerializableResourceSignatureImpl::CreatePRSD3D12(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages)
{
    auto pPRSD3D12 = std::make_unique<TPRS<PipelineResourceSignatureD3D12Impl>>(pRefCounters, Desc, ShaderStages);

    PipelineResourceSignatureSerializedDataD3D12 SerializedData;
    pPRSD3D12->PRS.Serialize(SerializedData);
    AddPRSDesc(pPRSD3D12->PRS.GetDesc(), SerializedData);
    CopyPRSSerializedData<PSOSerializerD3D12>(SerializedData, pPRSD3D12->Mem);

    m_pPRSD3D12.reset(pPRSD3D12.release());
}


void SerializationDeviceImpl::GetPipelineResourceBindingsD3D12(const PipelineResourceBindingAttribs& Info,
                                                               std::vector<PipelineResourceBinding>& ResourceBindings)
{
    const auto ShaderStages = (Info.ShaderStages == SHADER_TYPE_UNKNOWN ? static_cast<SHADER_TYPE>(~0u) : Info.ShaderStages);

    SignatureArray<PipelineResourceSignatureD3D12Impl> Signatures      = {};
    Uint32                                             SignaturesCount = 0;
    SortResourceSignatures(Info, Signatures, SignaturesCount);

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

            PipelineResourceBinding Dst{};
            Dst.Name         = ResDesc.Name;
            Dst.ResourceType = ResDesc.ResourceType;
            Dst.Register     = ResAttr.Register;
            Dst.Space        = StaticCast<Uint16>(BaseRegisterSpace + ResAttr.Space);
            Dst.ArraySize    = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY) == 0 ? ResDesc.ArraySize : RuntimeArray;
            Dst.ShaderStages = ResDesc.ShaderStages;
            ResourceBindings.push_back(Dst);
        }
    }
}

} // namespace Diligent
