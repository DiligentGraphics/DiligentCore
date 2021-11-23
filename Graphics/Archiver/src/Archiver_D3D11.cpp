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

#include "../../GraphicsEngineD3D11/include/pch.h"
#include "RenderDeviceD3D11Impl.hpp"
#include "PipelineResourceSignatureD3D11Impl.hpp"
#include "PipelineStateD3D11Impl.hpp"
#include "ShaderD3D11Impl.hpp"
#include "DeviceObjectArchiveD3D11Impl.hpp"

namespace Diligent
{

template <>
struct SerializableResourceSignatureImpl::SignatureTraits<PipelineResourceSignatureD3D11Impl>
{
    static constexpr DeviceType Type = DeviceType::Direct3D11;
    using SerializedDataType         = PipelineResourceSignatureSerializedDataD3D11;

    template <SerializerMode Mode>
    using PSOSerializerType = PSOSerializerD3D11<Mode>;
};

namespace
{

struct ShaderStageInfoD3D11
{
    ShaderStageInfoD3D11() {}

    ShaderStageInfoD3D11(const SerializableShaderImpl* _pShader) :
        Type{_pShader->GetDesc().ShaderType},
        pShader{_pShader->GetShaderD3D11()},
        pSerializable{_pShader}
    {}

    // Needed only for ray tracing
    void Append(const SerializableShaderImpl*) {}

    Uint32 Count() const { return 1; }

    SHADER_TYPE                   Type          = SHADER_TYPE_UNKNOWN;
    ShaderD3D11Impl*              pShader       = nullptr;
    const SerializableShaderImpl* pSerializable = nullptr;
};

inline SHADER_TYPE GetShaderStageType(const ShaderStageInfoD3D11& Stage) { return Stage.Type; }

template <typename CreateInfoType>
void InitD3D11ShaderResourceCounters(const CreateInfoType& CreateInfo, D3D11ShaderResourceCounters& ResCounters)
{}

void InitD3D11ShaderResourceCounters(const GraphicsPipelineStateCreateInfo& CreateInfo, D3D11ShaderResourceCounters& ResCounters)
{
    VERIFY_EXPR(CreateInfo.PSODesc.IsAnyGraphicsPipeline());

    // In Direct3D11, UAVs use the same register space as render targets
    ResCounters[D3D11_RESOURCE_RANGE_UAV][PSInd] = CreateInfo.GraphicsPipeline.NumRenderTargets;
}

} // namespace


template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersD3D11(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS)
{
    TShaderIndices ShaderIndices;

    std::vector<ShaderStageInfoD3D11> ShaderStages;
    SHADER_TYPE                       ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateD3D11Impl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    std::vector<ShaderD3D11Impl*>  ShadersD3D11{ShaderStages.size()};
    std::vector<CComPtr<ID3DBlob>> ShaderBytecode{ShaderStages.size()};
    for (size_t i = 0; i < ShadersD3D11.size(); ++i)
    {
        auto& Src = ShaderStages[i];
        auto& Dst = ShadersD3D11[i];
        Dst       = Src.pShader;
    }

    IPipelineResourceSignature* DefaultSignatures[1] = {};
    if (CreateInfo.ResourceSignaturesCount == 0)
    {
        if (!CreateDefaultResourceSignature(
                DefPRS, [&]() {
                    std::vector<PipelineResourceDesc> Resources;
                    std::vector<ImmutableSamplerDesc> ImmutableSamplers;

                    auto SignDesc = PipelineStateD3D11Impl::GetDefaultResourceSignatureDesc(
                        ShadersD3D11, CreateInfo.PSODesc.ResourceLayout, "Default resource signature", Resources, ImmutableSamplers);
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
        SignatureArray<PipelineResourceSignatureD3D11Impl> Signatures      = {};
        Uint32                                             SignaturesCount = 0;
        SortResourceSignatures(CreateInfo, Signatures, SignaturesCount);

        D3D11ShaderResourceCounters ResCounters = {};
        InitD3D11ShaderResourceCounters(CreateInfo, ResCounters);
        std::array<D3D11ShaderResourceCounters, MAX_RESOURCE_SIGNATURES> BaseBindings = {};
        for (Uint32 i = 0; i < SignaturesCount; ++i)
        {
            const PipelineResourceSignatureD3D11Impl* const pSignature = Signatures[i];
            if (pSignature == nullptr)
                continue;

            BaseBindings[i] = ResCounters;
            pSignature->ShiftBindings(ResCounters);
        }

        PipelineStateD3D11Impl::RemapShaderResources(
            ShadersD3D11,
            Signatures.data(),
            SignaturesCount,
            BaseBindings.data(),
            [&ShaderBytecode](size_t ShaderIdx, ShaderD3D11Impl* pShader, ID3DBlob* pPatchedBytecode) //
            {
                ShaderBytecode[ShaderIdx] = pPatchedBytecode;
            });
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to remap shader resources in Direct3D11 shaders");
        return false;
    }

    for (size_t i = 0; i < ShadersD3D11.size(); ++i)
    {
        const auto& CI        = ShaderStages[i].pSerializable->GetCreateInfo();
        const auto& pBytecode = ShaderBytecode[i];

        SerializeShaderBytecode(ShaderIndices, DeviceType::Direct3D11, CI, pBytecode->GetBufferPointer(), pBytecode->GetBufferSize());
    }
    SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[static_cast<Uint32>(DeviceType::Direct3D11)]);
    return true;
}

template bool ArchiverImpl::PatchShadersD3D11<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersD3D11<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersD3D11<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersD3D11<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);


struct SerializableShaderImpl::CompiledShaderD3D11 : ICompiledShader
{
    ShaderD3D11Impl ShaderD3D11;

    CompiledShaderD3D11(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, const ShaderD3D11Impl::CreateInfo& D3D11ShaderCI) :
        ShaderD3D11{pRefCounters, nullptr, ShaderCI, D3D11ShaderCI, true}
    {}
};

ShaderD3D11Impl* SerializableShaderImpl::GetShaderD3D11() const
{
    return m_pShaderD3D11 ? &reinterpret_cast<CompiledShaderD3D11*>(m_pShaderD3D11.get())->ShaderD3D11 : nullptr;
}

void SerializableShaderImpl::CreateShaderD3D11(IReferenceCounters* pRefCounters, ShaderCreateInfo& ShaderCI, String& CompilationLog)
{
    const ShaderD3D11Impl::CreateInfo D3D11ShaderCI{
        m_pDevice->GetDeviceInfo(),
        m_pDevice->GetAdapterInfo(),
        static_cast<D3D_FEATURE_LEVEL>(m_pDevice->GetD3D11FeatureLevel()) //
    };
    CreateShader<CompiledShaderD3D11>(m_pShaderD3D11, CompilationLog, "Direct3D11", pRefCounters, ShaderCI, D3D11ShaderCI);
}


template PipelineResourceSignatureD3D11Impl* SerializableResourceSignatureImpl::GetSignature<PipelineResourceSignatureD3D11Impl>() const;

void SerializableResourceSignatureImpl::CreatePRSD3D11(IReferenceCounters*                  pRefCounters,
                                                       const PipelineResourceSignatureDesc& Desc,
                                                       SHADER_TYPE                          ShaderStages)
{
    CreateSignature<PipelineResourceSignatureD3D11Impl>(pRefCounters, Desc, ShaderStages);
}

void SerializationDeviceImpl::GetPipelineResourceBindingsD3D11(const PipelineResourceBindingAttribs& Info,
                                                               std::vector<PipelineResourceBinding>& ResourceBindings)
{
    const auto            ShaderStages        = (Info.ShaderStages == SHADER_TYPE_UNKNOWN ? static_cast<SHADER_TYPE>(~0u) : Info.ShaderStages);
    constexpr SHADER_TYPE SupportedStagesMask = (SHADER_TYPE_ALL_GRAPHICS | SHADER_TYPE_COMPUTE);

    SignatureArray<PipelineResourceSignatureD3D11Impl> Signatures      = {};
    Uint32                                             SignaturesCount = 0;
    SortResourceSignatures(Info, Signatures, SignaturesCount);

    D3D11ShaderResourceCounters BaseBindings = {};
    // In Direct3D11, UAVs use the same register space as render targets
    BaseBindings[D3D11_RESOURCE_RANGE_UAV][PSInd] = Info.NumRenderTargets;

    for (Uint32 sign = 0; sign < SignaturesCount; ++sign)
    {
        const PipelineResourceSignatureD3D11Impl* const pSignature = Signatures[sign];
        if (pSignature == nullptr)
            continue;

        for (Uint32 r = 0; r < pSignature->GetTotalResourceCount(); ++r)
        {
            const auto& ResDesc = pSignature->GetResourceDesc(r);
            const auto& ResAttr = pSignature->GetResourceAttribs(r);
            const auto  Range   = PipelineResourceSignatureD3D11Impl::ShaderResourceTypeToRange(ResDesc.ResourceType);

            for (auto Stages = ShaderStages & SupportedStagesMask; Stages != 0;)
            {
                const auto ShaderStage = ExtractLSB(Stages);
                const auto ShaderInd   = GetShaderTypeIndex(ShaderStage);

                if ((ResDesc.ShaderStages & ShaderStage) == 0)
                    continue;

                VERIFY_EXPR(ResAttr.BindPoints.IsStageActive(ShaderInd));
                const auto Binding = Uint32{BaseBindings[Range][ShaderInd]} + Uint32{ResAttr.BindPoints[ShaderInd]};

                PipelineResourceBinding Dst{};
                Dst.Name         = ResDesc.Name;
                Dst.ResourceType = ResDesc.ResourceType;
                Dst.Register     = Binding;
                Dst.Space        = 0;
                Dst.ArraySize    = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY) == 0 ? ResDesc.ArraySize : RuntimeArray;
                Dst.ShaderStages = ShaderStage;
                ResourceBindings.push_back(Dst);
            }
        }

        for (Uint32 samp = 0; samp < pSignature->GetImmutableSamplerCount(); ++samp)
        {
            const auto& ImtblSam = pSignature->GetImmutableSamplerDesc(samp);
            const auto& SampAttr = pSignature->GetImmutableSamplerAttribs(samp);
            const auto  Range    = D3D11_RESOURCE_RANGE_SAMPLER;

            for (auto Stages = ShaderStages & SupportedStagesMask; Stages != 0;)
            {
                const auto ShaderStage = ExtractLSB(Stages);
                const auto ShaderInd   = GetShaderTypeIndex(ShaderStage);

                if ((ImtblSam.ShaderStages & ShaderStage) == 0)
                    continue;

                VERIFY_EXPR(SampAttr.BindPoints.IsStageActive(ShaderInd));
                const auto Binding = Uint32{BaseBindings[Range][ShaderInd]} + Uint32{SampAttr.BindPoints[ShaderInd]};

                PipelineResourceBinding Dst{};
                Dst.Name         = ImtblSam.SamplerOrTextureName;
                Dst.ResourceType = SHADER_RESOURCE_TYPE_SAMPLER;
                Dst.Register     = Binding;
                Dst.Space        = 0;
                Dst.ArraySize    = SampAttr.ArraySize;
                Dst.ShaderStages = ShaderStage;
                ResourceBindings.push_back(Dst);
            }
        }
        pSignature->ShiftBindings(BaseBindings);
    }
}

} // namespace Diligent
