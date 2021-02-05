/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include "pch.h"

#include <algorithm>
#include <limits>

#include "PipelineLayoutVk.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "VulkanTypeConversions.hpp"
#include "StringTools.hpp"

namespace Diligent
{

PipelineLayoutVk::PipelineLayoutVk()
{
    m_FirstDescrSetIndex.fill(std::numeric_limits<FirstDescrSetIndexArrayType::value_type>::max());
}

PipelineLayoutVk::~PipelineLayoutVk()
{
    VERIFY(!m_VkPipelineLayout, "Pipeline layout have not been released!");
}

void PipelineLayoutVk::Release(RenderDeviceVkImpl* pDeviceVk, Uint64 CommandQueueMask)
{
    if (m_VkPipelineLayout)
    {
        pDeviceVk->SafeReleaseDeviceObject(std::move(m_VkPipelineLayout), CommandQueueMask);
    }
}

void PipelineLayoutVk::Create(RenderDeviceVkImpl* pDeviceVk, PIPELINE_TYPE PipelineType, IPipelineResourceSignature** ppSignatures, Uint32 SignatureCount)
{
    VERIFY(m_SignatureCount == 0 && m_DescrSetCount == 0 && !m_VkPipelineLayout,
           "This pipeline layout is already initialized");

    for (Uint32 i = 0; i < SignatureCount; ++i)
    {
        auto* pSignature = ValidatedCast<PipelineResourceSignatureVkImpl>(ppSignatures[i]);
        VERIFY(pSignature != nullptr, "Pipeline resource signature at index ", i, " is null. This error should've been caught by ValidatePipelineResourceSignatures.");

        const Uint8 Index = pSignature->GetDesc().BindingIndex;

#ifdef DILIGENT_DEBUG
        VERIFY(Index < m_Signatures.size(),
               "Pipeline resource signature specifies binding index ", Uint32{Index}, " that exceeds the limit (", m_Signatures.size() - 1,
               "). This error should've been caught by ValidatePipelineResourceSignatureDesc.");

        VERIFY(m_Signatures[Index] == nullptr,
               "Pipeline resource signature '", pSignature->GetDesc().Name, "' at index ", Uint32{Index},
               " conflicts with another resource signature '", m_Signatures[Index]->GetDesc().Name,
               "' that uses the same index. This error should've been caught by ValidatePipelineResourceSignatures.");

        for (Uint32 s = 0, StageCount = pSignature->GetNumActiveShaderStages(); s < StageCount; ++s)
        {
            const auto ShaderType = pSignature->GetActiveShaderStageType(s);
            VERIFY(IsConsistentShaderType(ShaderType, PipelineType),
                   "Pipeline resource signature '", pSignature->GetDesc().Name, "' at index ", Uint32{Index},
                   " has shader stage '", GetShaderTypeLiteralName(ShaderType), "' that is not compatible with pipeline type '",
                   GetPipelineTypeString(PipelineType), "'.");
        }
#endif

        m_SignatureCount    = std::max<Uint8>(m_SignatureCount, Index + 1);
        m_Signatures[Index] = pSignature;
    }

    std::array<VkDescriptorSetLayout, MAX_RESOURCE_SIGNATURES * PipelineResourceSignatureVkImpl::MAX_DESCRIPTOR_SETS> DescSetLayouts;

    Uint32 DescSetLayoutCount        = 0;
    Uint32 DynamicUniformBufferCount = 0;
    Uint32 DynamicStorageBufferCount = 0;

    for (Uint32 i = 0; i < m_SignatureCount; ++i)
    {
        const auto& pSignature = m_Signatures[i];
        if (pSignature == nullptr)
            continue;

        VERIFY(DescSetLayoutCount <= std::numeric_limits<FirstDescrSetIndexArrayType::value_type>::max(),
               "Descriptor set layout count (", DescSetLayoutCount, ") exceeds the maximum representable value");
        m_FirstDescrSetIndex[i] = static_cast<FirstDescrSetIndexArrayType::value_type>(DescSetLayoutCount);

        for (auto SetId : {PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_STATIC_MUTABLE, PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_DYNAMIC})
        {
            if (pSignature->HasDescriptorSet(SetId))
                DescSetLayouts[DescSetLayoutCount++] = pSignature->GetVkDescriptorSetLayout(SetId);
        }

        DynamicUniformBufferCount += pSignature->GetDynamicUniformBufferCount();
        DynamicStorageBufferCount += pSignature->GetDynamicStorageBufferCount();
    }
    VERIFY_EXPR(DescSetLayoutCount <= MAX_RESOURCE_SIGNATURES * 2);

    const auto& Limits = pDeviceVk->GetPhysicalDevice().GetProperties().limits;
    if (DescSetLayoutCount > Limits.maxBoundDescriptorSets)
    {
        LOG_ERROR_AND_THROW("The total number of descriptor sets (", DescSetLayoutCount,
                            ") used by the pipeline layout exceeds device limit (", Limits.maxBoundDescriptorSets, ")");
    }

    if (DynamicUniformBufferCount > Limits.maxDescriptorSetUniformBuffersDynamic)
    {
        LOG_ERROR_AND_THROW("The number of dynamic uniform buffers  (", DynamicUniformBufferCount,
                            ") used by the pipeline layout exceeds device limit (", Limits.maxDescriptorSetUniformBuffersDynamic, ")");
    }

    if (DynamicStorageBufferCount > Limits.maxDescriptorSetStorageBuffersDynamic)
    {
        LOG_ERROR_AND_THROW("The number of dynamic storage buffers (", DynamicStorageBufferCount,
                            ") used by the pipeline layout exceeds device limit (", Limits.maxDescriptorSetStorageBuffersDynamic, ")");
    }

    VERIFY(m_DescrSetCount <= std::numeric_limits<decltype(m_DescrSetCount)>::max(),
           "Descriptor set count (", DescSetLayoutCount, ") exceeds the maximum representable value");

    VkPipelineLayoutCreateInfo PipelineLayoutCI = {};

    PipelineLayoutCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutCI.pNext                  = nullptr;
    PipelineLayoutCI.flags                  = 0; // reserved for future use
    PipelineLayoutCI.setLayoutCount         = DescSetLayoutCount;
    PipelineLayoutCI.pSetLayouts            = DescSetLayoutCount ? DescSetLayouts.data() : nullptr;
    PipelineLayoutCI.pushConstantRangeCount = 0;
    PipelineLayoutCI.pPushConstantRanges    = nullptr;
    m_VkPipelineLayout                      = pDeviceVk->GetLogicalDevice().CreatePipelineLayout(PipelineLayoutCI);

    m_DescrSetCount = static_cast<Uint8>(DescSetLayoutCount);
}

size_t PipelineLayoutVk::GetHash() const
{
    if (m_SignatureCount == 0)
        return 0;

    size_t hash = 0;
    HashCombine(hash, m_SignatureCount);
    for (Uint32 i = 0; i < m_SignatureCount; ++i)
    {
        if (m_Signatures[i] != nullptr)
            HashCombine(hash, m_Signatures[i]->GetHash());
        else
            HashCombine(hash, 0);
    }
    return hash;
}

PipelineLayoutVk::ResourceInfo PipelineLayoutVk::GetResourceInfo(const char* Name, SHADER_TYPE Stage) const
{
    ResourceInfo Info;
    for (Uint32 sign = 0, SignCount = GetSignatureCount(); sign < SignCount && !Info; ++sign)
    {
        auto* const pSignature = GetSignature(sign);
        if (pSignature == nullptr)
            continue;

        for (Uint32 r = 0, ResCount = pSignature->GetTotalResourceCount(); r < ResCount; ++r)
        {
            const auto& ResDesc = pSignature->GetResourceDesc(r);
            const auto& Attr    = pSignature->GetResourceAttribs(r);

            if ((ResDesc.ShaderStages & Stage) != 0 && strcmp(ResDesc.Name, Name) == 0)
            {
                Info.Signature     = pSignature;
                Info.Type          = ResDesc.ResourceType;
                Info.ResIndex      = r;
                Info.BindingIndex  = Attr.BindingIndex;
                Info.DescrSetIndex = m_FirstDescrSetIndex[sign] + Attr.DescrSet;
                break;
            }
        }
    }
    return Info;
}

PipelineLayoutVk::ResourceInfo PipelineLayoutVk::GetImmutableSamplerInfo(const char* Name, SHADER_TYPE Stage) const
{
    ResourceInfo Info;
    for (Uint32 sign = 0, SignCount = GetSignatureCount(); sign < SignCount && !Info; ++sign)
    {
        auto* const pSignature = GetSignature(sign);
        if (pSignature == nullptr)
            continue;

        for (Uint32 s = 0, SampCount = pSignature->GetImmutableSamplerCount(); s < SampCount; ++s)
        {
            const auto& Desc = pSignature->GetImmutableSamplerDesc(s);
            const auto& Attr = pSignature->GetImmutableSamplerAttribs(s);

            if (Attr.Ptr && (Desc.ShaderStages & Stage) != 0 && StreqSuff(Name, Desc.SamplerOrTextureName, pSignature->GetCombinedSamplerSuffix()))
            {
                Info.Signature     = pSignature;
                Info.Type          = SHADER_RESOURCE_TYPE_SAMPLER;
                Info.BindingIndex  = Attr.BindingIndex;
                Info.DescrSetIndex = m_FirstDescrSetIndex[sign] + Attr.DescrSet;
                break;
            }
        }
    }
    return Info;
}

} // namespace Diligent
