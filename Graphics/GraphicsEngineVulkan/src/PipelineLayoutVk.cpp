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

#include "PipelineLayoutVk.hpp"
#include "ShaderVkImpl.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"
#include "TextureVkImpl.hpp"
#include "BufferVkImpl.hpp"
#include "VulkanTypeConversions.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

PipelineLayoutVk::PipelineLayoutVk() :
    m_DynamicOffsetCount{0},
    m_SignatureCount{0},
    m_DescrSetCount{0}
{
}

void PipelineLayoutVk::Release(RenderDeviceVkImpl* pDeviceVk, Uint64 CommandQueueMask)
{
    pDeviceVk->SafeReleaseDeviceObject(std::move(m_VkPipelineLayout), CommandQueueMask);
}

void PipelineLayoutVk::Create(RenderDeviceVkImpl* pDeviceVk, IPipelineResourceSignature** ppSignatures, Uint32 SignatureCount)
{
    VERIFY(m_DynamicOffsetCount == 0 && m_SignatureCount == 0 && m_DescrSetCount == 0,
           "pipeline layout is already initialized");

    for (Uint32 i = 0; i < SignatureCount; ++i)
    {
        auto* pSignature = ValidatedCast<PipelineResourceSignatureVkImpl>(ppSignatures[i]);
        if (!pSignature)
            LOG_ERROR_AND_THROW("pipeline resource signature (", i, ") must not be null");

        const Uint32 Index = pSignature->GetDesc().BindingIndex;

        if (Index >= m_Signatures.size())
            LOG_ERROR_AND_THROW("Pipeline resource signature '", pSignature->GetDesc().Name, "' index (", Uint32{Index}, ") must be less than (", m_Signatures.size(), ").");

        if (m_Signatures[Index] != nullptr)
            LOG_ERROR_AND_THROW("Pipeline resource signature '", pSignature->GetDesc().Name, "' with index (", Uint32{Index}, ") overrides another resource signature '", m_Signatures[Index]->GetDesc().Name, "'.");

        m_SignatureCount    = std::max(m_SignatureCount, Index + 1);
        m_Signatures[Index] = pSignature;
    }

    auto* pEmptySign = ValidatedCast<PipelineResourceSignatureVkImpl>(pDeviceVk->GetEmptySignature());

    for (Uint32 i = 0; i < m_SignatureCount; ++i)
    {
        if (m_Signatures[i] == nullptr)
            m_Signatures[i] = pEmptySign;
    }

    std::array<VkDescriptorSetLayout, MAX_RESOURCE_SIGNATURES * 2> DescSetLayouts;

    Uint32 DescSetLayoutCount = 0;
    Uint32 DynamicOffsetCount = 0;
#ifdef DILIGENT_DEBUG
    Uint32 DynamicUniformBufferCount = 0;
    Uint32 DynamicStorageBufferCount = 0;
#endif

    for (Uint32 i = 0; i < m_SignatureCount; ++i)
    {
        const auto& pSignature = m_Signatures[i];
        VERIFY_EXPR(pSignature != nullptr);

        m_DescSetOffset[i] = static_cast<Uint8>(DescSetLayoutCount);
        m_DynBufOffset[i]  = static_cast<Uint16>(DynamicOffsetCount);

        auto StaticDSLayout  = pSignature->GetStaticVkDescriptorSetLayout();
        auto DynamicDSLayout = pSignature->GetDynamicVkDescriptorSetLayout();

        if (StaticDSLayout) DescSetLayouts[DescSetLayoutCount++] = StaticDSLayout;
        if (DynamicDSLayout) DescSetLayouts[DescSetLayoutCount++] = DynamicDSLayout;

        DynamicOffsetCount += pSignature->GetDynamicBufferCount();

#ifdef DILIGENT_DEBUG
        for (Uint32 r = 0, ResCount = pSignature->GetTotalResourceCount(); r < ResCount; ++r)
        {
            const auto& Attr = pSignature->GetAttribs(r);

            if (Attr.Type == DescriptorType::UniformBufferDynamic)
            {
                ++DynamicUniformBufferCount;
            }
            else if (Attr.Type == DescriptorType::StorageBufferDynamic ||
                     Attr.Type == DescriptorType::StorageBufferDynamic_ReadOnly)
            {
                ++DynamicStorageBufferCount;
            }
        }
#endif
    }

    VkPipelineLayoutCreateInfo PipelineLayoutCI = {};

    PipelineLayoutCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutCI.pNext                  = nullptr;
    PipelineLayoutCI.flags                  = 0; // reserved for future use
    PipelineLayoutCI.setLayoutCount         = DescSetLayoutCount;
    PipelineLayoutCI.pSetLayouts            = DescSetLayoutCount ? DescSetLayouts.data() : nullptr;
    PipelineLayoutCI.pushConstantRangeCount = 0;
    PipelineLayoutCI.pPushConstantRanges    = nullptr;
    m_VkPipelineLayout                      = pDeviceVk->GetLogicalDevice().CreatePipelineLayout(PipelineLayoutCI);

    const auto& Limits = pDeviceVk->GetPhysicalDevice().GetProperties().limits;
    VERIFY_EXPR(DescSetLayoutCount <= Limits.maxBoundDescriptorSets);
    VERIFY_EXPR(DescSetLayoutCount <= MAX_RESOURCE_SIGNATURES * 2);

#ifdef DILIGENT_DEBUG
    VERIFY_EXPR(DynamicUniformBufferCount <= Limits.maxDescriptorSetUniformBuffersDynamic);
    VERIFY_EXPR(DynamicStorageBufferCount <= Limits.maxDescriptorSetStorageBuffersDynamic);
#endif

    m_DescrSetCount      = static_cast<Uint8>(DescSetLayoutCount);
    m_DynamicOffsetCount = DynamicOffsetCount;
}

size_t PipelineLayoutVk::GetHash() const
{
    size_t hash = 0;
    HashCombine(hash, m_SignatureCount);
    for (Uint32 i = 0; i < m_SignatureCount; ++i)
    {
        VERIFY_EXPR(m_Signatures[i] != nullptr);
        HashCombine(hash, m_Signatures[i]->GetHash());
    }
    return hash;
}

bool PipelineLayoutVk::GetResourceInfo(const char* Name, SHADER_TYPE Stage, ResourceInfo& Info) const
{
    // AZ TODO: optimize
    for (Uint32 i = 0, DSCount = GetSignatureCount(); i < DSCount; ++i)
    {
        auto* pSignature = GetSignature(i);
        VERIFY_EXPR(pSignature != nullptr);

        for (Uint32 r = 0, ResCount = pSignature->GetTotalResourceCount(); r < ResCount; ++r)
        {
            const auto& Res  = pSignature->GetResource(r);
            const auto& Attr = pSignature->GetAttribs(r);

            if ((Res.ShaderStages & Stage) && strcmp(Res.Name, Name) == 0)
            {
                Info.Type          = Res.ResourceType;
                Info.BindingIndex  = static_cast<Uint16>(Attr.BindingIndex);
                Info.DescrSetIndex = m_DescSetOffset[i] + Attr.DescrSet;
                return true;
            }
        }
    }
    return false;
}

} // namespace Diligent
