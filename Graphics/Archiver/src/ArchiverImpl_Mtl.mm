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

#include "RenderDeviceMtlImpl.hpp"
#include "PipelineResourceSignatureMtlImpl.hpp"
#include "PipelineStateMtlImpl.hpp"
#include "ShaderMtlImpl.hpp"
#include "DeviceObjectArchiveMtlImpl.hpp"

namespace Diligent
{
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

    friend SHADER_TYPE GetShaderStageType(const ShaderStageInfoMtl& Stage) { return Stage.Type; }
};

template <typename SignatureType>
using SignatureArray = std::array<RefCntAutoPtr<SignatureType>, MAX_RESOURCE_SIGNATURES>;

template <typename CreateInfoType, typename SignatureType>
static void SortResourceSignatures(const CreateInfoType& CreateInfo, SignatureArray<SignatureType>& Signatures, Uint32& SignaturesCount)
{
    for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
    {
        const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(CreateInfo.ppResourceSignatures[i]);
        VERIFY_EXPR(pSerPRS != nullptr);

        const auto& Desc = pSerPRS->GetDesc();

        Signatures[Desc.BindingIndex] = pSerPRS->template GetSignature<SignatureType>();
        SignaturesCount               = std::max(SignaturesCount, static_cast<Uint32>(Desc.BindingIndex) + 1);
    }
}

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
    TShaderIndices ShaderIndices;

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
            m_pSerializationDevice->CreatePipelineResourceSignature(SignDesc, DefPRS.DeviceBits, ActiveShaderStages, &pDefaultPRS);
            if (pDefaultPRS == nullptr)
                return false;

            if (DefPRS.pPRS)
            {
                if (!(DefPRS.pPRS.RawPtr<SerializableResourceSignatureImpl>()->IsCompatible(*pDefaultPRS.RawPtr<SerializableResourceSignatureImpl>(), DefPRS.DeviceBits)))
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
            const auto&      Stage           = ShaderStages[j];
            SerializedMemory PatchedBytecode = Stage.pShader->PatchShaderMtl(Signatures.data(), BaseBindings.data(), SignaturesCount); // throw exception
            SerializeShaderBytecode(ShaderIndices, DeviceType::Metal, Stage.pShader->GetCreateInfo(), PatchedBytecode.Ptr, PatchedBytecode.Size);
        }
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to compile Metal shaders");
        return false;
    }

    SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[static_cast<Uint32>(DeviceType::Metal)]);
    return true;
}

template bool ArchiverImpl::PatchShadersMtl<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersMtl<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersMtl<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersMtl<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);

} // namespace Diligent
