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
#include "ArchiverImpl_Inc.hpp"

#include "VulkanUtilities/VulkanHeaders.h"
#include "RenderDeviceVkImpl.hpp"
#include "PipelineResourceSignatureVkImpl.hpp"
#include "PipelineStateVkImpl.hpp"
#include "ShaderVkImpl.hpp"
#include "DeviceObjectArchiveVkImpl.hpp"

namespace Diligent
{

template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersVk(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS)
{
    struct ShaderStageInfoVk : PipelineStateVkImpl::ShaderStageInfo
    {
        ShaderStageInfoVk() :
            ShaderStageInfo{} {}

        ShaderStageInfoVk(const SerializableShaderImpl* pShader) :
            ShaderStageInfo{pShader->GetShaderVk()},
            Serializable{pShader}
        {}

        void Append(const SerializableShaderImpl* pShader)
        {
            ShaderStageInfo::Append(pShader->GetShaderVk());
            Serializable.push_back(pShader);
        }

        std::vector<const SerializableShaderImpl*> Serializable;
    };

    TShaderIndices ShaderIndices;

    std::vector<ShaderStageInfoVk> ShaderStages;
    SHADER_TYPE                    ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateVkImpl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    PipelineStateVkImpl::TShaderStages ShaderStagesVk{ShaderStages.size()};
    for (size_t i = 0; i < ShaderStagesVk.size(); ++i)
    {
        auto& Src   = ShaderStages[i];
        auto& Dst   = ShaderStagesVk[i];
        Dst.Type    = Src.Type;
        Dst.Shaders = std::move(Src.Shaders);
        Dst.SPIRVs  = std::move(Src.SPIRVs);
    }

    IPipelineResourceSignature* DefaultSignatures[1] = {};
    if (CreateInfo.ResourceSignaturesCount == 0)
    {
        if (!CreateDefaultResourceSignature(
                DefPRS, [&]() {
                    std::vector<PipelineResourceDesc> Resources;
                    std::vector<ImmutableSamplerDesc> ImmutableSamplers;

                    auto SignDesc = PipelineStateVkImpl::GetDefaultResourceSignatureDesc(
                        ShaderStagesVk, CreateInfo.PSODesc.ResourceLayout, "Default resource signature", Resources, ImmutableSamplers);
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
        SignatureArray<PipelineResourceSignatureVkImpl> Signatures      = {};
        Uint32                                          SignaturesCount = 0;
        SortResourceSignatures(CreateInfo, Signatures, SignaturesCount);

        // Same as PipelineLayoutVk::Create()
        PipelineStateVkImpl::TBindIndexToDescSetIndex BindIndexToDescSetIndex = {};
        Uint32                                        DescSetLayoutCount      = 0;
        for (Uint32 i = 0; i < SignaturesCount; ++i)
        {
            const auto& pSignature = Signatures[i];
            if (pSignature == nullptr)
                continue;

            VERIFY_EXPR(pSignature->GetDesc().BindingIndex == i);
            BindIndexToDescSetIndex[i] = StaticCast<PipelineStateVkImpl::TBindIndexToDescSetIndex::value_type>(DescSetLayoutCount);

            for (auto SetId : {PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_STATIC_MUTABLE, PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_DYNAMIC})
            {
                if (pSignature->GetDescriptorSetSize(SetId) != ~0u)
                    ++DescSetLayoutCount;
            }
        }
        VERIFY_EXPR(DescSetLayoutCount <= MAX_RESOURCE_SIGNATURES * 2);
        VERIFY_EXPR(DescSetLayoutCount >= CreateInfo.ResourceSignaturesCount);

        PipelineStateVkImpl::RemapShaderResources(ShaderStagesVk,
                                                  Signatures.data(),
                                                  SignaturesCount,
                                                  BindIndexToDescSetIndex,
                                                  true); // bStripReflection
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to remap shader resources in Vulkan shaders");
        return false;
    }

    for (size_t j = 0; j < ShaderStagesVk.size(); ++j)
    {
        const auto& Stage = ShaderStagesVk[j];
        for (size_t i = 0; i < Stage.Count(); ++i)
        {
            const auto& CI    = ShaderStages[j].Serializable[i]->GetCreateInfo();
            const auto& SPIRV = Stage.SPIRVs[i];

            SerializeShaderBytecode(ShaderIndices, DeviceType::Vulkan, CI, SPIRV.data(), SPIRV.size() * sizeof(SPIRV[0]));
        }
    }

    // AZ TODO: map ray tracing shaders to shader indices

    SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[static_cast<Uint32>(DeviceType::Vulkan)]);
    return true;
}

template bool ArchiverImpl::PatchShadersVk<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersVk<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersVk<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersVk<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);

} // namespace Diligent
