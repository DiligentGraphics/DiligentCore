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

PipelineLayoutVk::PipelineLayoutVk(IReferenceCounters* pRefCounters, RenderDeviceVkImpl* pDeviceVk, IPipelineResourceSignature** ppSignatures, Uint32 SignatureCount) :
    ObjectBase<IObject>{pRefCounters},
    m_pDeviceVk{pDeviceVk},
    m_SignatureCount{0}
{
    for (Uint32 i = 0; i < SignatureCount; ++i)
    {
        auto* pSign = ValidatedCast<PipelineResourceSignatureVkImpl>(ppSignatures[i]);
        if (pSign)
        {
            const Uint8 Index = pSign->GetDesc().BindingIndex;

            if (Index >= m_Signatures.size())
                LOG_ERROR_AND_THROW("Pipeline resource signature '", pSign->GetDesc().Name, "' index (", Uint32{Index}, ") must be less than (", m_Signatures.size(), ").");

            if (m_Signatures[Index] != nullptr)
                LOG_ERROR_AND_THROW("Pipeline resource signature '", pSign->GetDesc().Name, "' with index (", Uint32{Index}, ") overrides another resource signature '", m_Signatures[Index]->GetDesc().Name, "'.");

            m_SignatureCount    = std::max(m_SignatureCount, Uint8(Index + 1));
            m_Signatures[Index] = pSign;

            if (m_PipelineType <= PIPELINE_TYPE_LAST)
            {
                if (m_PipelineType != pSign->GetPipelineType())
                {
                    LOG_ERROR_AND_THROW("Pipeline resource signature '", pSign->GetDesc().Name, "' with index (", Uint32{Index}, ") has different pipeline type (",
                                        GetPipelineTypeString(pSign->GetPipelineType()), ") than other (", GetPipelineTypeString(m_PipelineType), ").");
                }
            }
            else
                m_PipelineType = pSign->GetPipelineType();
        }
    }
}

PipelineLayoutVk::~PipelineLayoutVk()
{
    m_pDeviceVk->GetPipelineLayoutCache().OnDestroyLayout(this);
    m_pDeviceVk->SafeReleaseDeviceObject(std::move(m_VkPipelineLayout), ~0ull);
}

void PipelineLayoutVk::Finalize()
{
    std::array<VkDescriptorSetLayout, MAX_RESOURCE_SIGNATURES * 2> DescSetLayouts;

    Uint32 DescSetLayoutCount = 0;
    Uint32 DynamicBufferCount = 0;

    for (Uint32 i = 0; i < m_SignatureCount; ++i)
    {
        if (m_Signatures[i] == nullptr)
            continue;

        m_DescSetOffset[i] = static_cast<Uint8>(DescSetLayoutCount);
        m_DynBufOffset[i]  = static_cast<Uint16>(DynamicBufferCount);

        auto StaticDSLayout  = m_Signatures[i]->GetStaticVkDescriptorSetLayout();
        auto DynamicDSLayout = m_Signatures[i]->GetDynamicVkDescriptorSetLayout();

        if (StaticDSLayout) DescSetLayouts[DescSetLayoutCount++] = StaticDSLayout;
        if (DynamicDSLayout) DescSetLayouts[DescSetLayoutCount++] = DynamicDSLayout;

        DynamicBufferCount += m_Signatures[i]->GetDynamicBufferCount();
    }

    VkPipelineLayoutCreateInfo PipelineLayoutCI = {};

    PipelineLayoutCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutCI.pNext                  = nullptr;
    PipelineLayoutCI.flags                  = 0; // reserved for future use
    PipelineLayoutCI.setLayoutCount         = DescSetLayoutCount;
    PipelineLayoutCI.pSetLayouts            = DescSetLayoutCount ? DescSetLayouts.data() : nullptr;
    PipelineLayoutCI.pushConstantRangeCount = 0;
    PipelineLayoutCI.pPushConstantRanges    = nullptr;
    m_VkPipelineLayout                      = m_pDeviceVk->GetLogicalDevice().CreatePipelineLayout(PipelineLayoutCI);

    const auto& Limits = m_pDeviceVk->GetPhysicalDevice().GetProperties().limits;
    VERIFY_EXPR(DescSetLayoutCount <= Limits.maxBoundDescriptorSets);
    VERIFY_EXPR(DynamicBufferCount <= (Limits.maxDescriptorSetUniformBuffersDynamic + Limits.maxDescriptorSetStorageBuffersDynamic)); // AZ TODO

    m_DescrSetCount      = static_cast<Uint8>(DescSetLayoutCount);
    m_DynamicOffsetCount = static_cast<Uint16>(DynamicBufferCount);
}

size_t PipelineLayoutVk::GetHash() const
{
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

bool PipelineLayoutVk::GetResourceInfo(const char* Name, SHADER_TYPE Stage, ResourceInfo& Info) const
{
    // AZ TODO: optimize
    for (Uint32 i = 0, Count = GetSignatureCount(); i < Count; ++i)
    {
        auto* pSign = GetSignature(i);
        if (pSign)
        {
            auto&  Desc            = pSign->GetDesc();
            Uint32 BindingCount[2] = {};

            for (Uint32 j = 0; j < Desc.NumResources; ++j)
            {
                auto&  Res     = Desc.Resources[j];
                Uint16 DSIndex = (Res.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC ? 1 : 0);

                if ((Res.ShaderStages & Stage) && strcmp(Res.Name, Name) == 0)
                {
                    Info.Type          = Res.ResourceType;
                    Info.BindingIndex  = static_cast<Uint16>(BindingCount[DSIndex]);
                    Info.DescrSetIndex = m_DescSetOffset[i] + DSIndex;
                    return true;
                }
                BindingCount[DSIndex] += Res.ArraySize;
            }
        }
    }
    return false;
}

} // namespace Diligent
