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

#pragma once

/// \file
/// Declaration of Diligent::PipelineLayoutVk class
#include <array>

#include "PipelineResourceSignatureVkImpl.hpp"
#include "VulkanUtilities/VulkanObjectWrappers.hpp"

namespace Diligent
{

class ShaderResourceCacheVk;

/// Implementation of the Diligent::PipelineLayoutVk class
class PipelineLayoutVk
{
public:
    PipelineLayoutVk();
    ~PipelineLayoutVk();

    void Create(RenderDeviceVkImpl* pDeviceVk, PIPELINE_TYPE PipelineType, IPipelineResourceSignature** ppSignatures, Uint32 SignatureCount);
    void Release(RenderDeviceVkImpl* pDeviceVkImpl, Uint64 CommandQueueMask);

    size_t GetHash() const;

    VkPipelineLayout GetVkPipelineLayout() const { return m_VkPipelineLayout; }
    Uint32           GetSignatureCount() const { return m_SignatureCount; }

    PipelineResourceSignatureVkImpl* GetSignature(Uint32 index) const
    {
        VERIFY_EXPR(index < m_SignatureCount);
        return m_Signatures[index].RawPtr<PipelineResourceSignatureVkImpl>();
    }

    // Returns the index of the first descriptor set used by the given resource signature
    Uint32 GetFirstDescrSetIndex(const PipelineResourceSignatureVkImpl* pPRS) const
    {
        VERIFY_EXPR(pPRS != nullptr);
        Uint32 Index = pPRS->GetDesc().BindingIndex;

        VERIFY_EXPR(Index < m_SignatureCount);
        VERIFY_EXPR(m_Signatures[Index] != nullptr);
        VERIFY_EXPR(!m_Signatures[Index]->IsIncompatibleWith(*pPRS));

        return m_FirstDescrSetIndex[Index];
    }

    struct ResourceInfo
    {
        PipelineResourceSignatureVkImpl* Signature = nullptr;
        SHADER_RESOURCE_TYPE             Type      = SHADER_RESOURCE_TYPE_UNKNOWN;
        // Index in m_Desc.Resources for a resource, or ~0U for an immutable sampler.
        Uint32 ResIndex      = ~0U;
        Uint32 DescrSetIndex = ~0U;
        Uint32 BindingIndex  = ~0U;

        operator bool() const
        {
            return Signature != nullptr && Type != SHADER_RESOURCE_TYPE_UNKNOWN;
        }
    };
    ResourceInfo GetResourceInfo(const char* Name, SHADER_TYPE Stage) const;
    ResourceInfo GetImmutableSamplerInfo(const char* Name, SHADER_TYPE Stage) const;

private:
    using SignatureArray              = std::array<RefCntAutoPtr<PipelineResourceSignatureVkImpl>, MAX_RESOURCE_SIGNATURES>;
    using FirstDescrSetIndexArrayType = std::array<Uint8, MAX_RESOURCE_SIGNATURES>;

    VulkanUtilities::PipelineLayoutWrapper m_VkPipelineLayout;

    // Index of the first descriptor set, for every resource signature.
    FirstDescrSetIndexArrayType m_FirstDescrSetIndex = {};

    // The number of resource signatures used by this pipeline layout
    // (Maximum is MAX_RESOURCE_SIGNATURES)
    Uint8 m_SignatureCount = 0;

    // The total number of descriptor sets used by this pipeline layout
    // (Maximum is MAX_RESOURCE_SIGNATURES * 2)
    Uint8 m_DescrSetCount = 0;

    SignatureArray m_Signatures;
};

} // namespace Diligent
