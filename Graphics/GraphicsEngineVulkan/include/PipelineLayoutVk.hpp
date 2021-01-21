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
#include "VulkanUtilities/VulkanLogicalDevice.hpp"
#include "VulkanUtilities/VulkanCommandBuffer.hpp"

namespace Diligent
{

class DeviceContextVkImpl;
class ShaderResourceCacheVk;

/// Implementation of the Diligent::PipelineLayoutVk class
class PipelineLayoutVk
{
public:
    PipelineLayoutVk();

    void Create(RenderDeviceVkImpl* pDeviceVk, IPipelineResourceSignature** ppSignatures, Uint32 SignatureCount);
    void Release(RenderDeviceVkImpl* pDeviceVkImpl, Uint64 CommandQueueMask);

    size_t GetHash() const;

    VkPipelineLayout GetVkPipelineLayout() const { return m_VkPipelineLayout; }
    Uint32           GetSignatureCount() const { return m_SignatureCount; }
    Uint32           GetDescriptorSetCount() const { return m_DescrSetCount; }
    Uint32           GetDynamicOffsetCount() const { return m_DynamicOffsetCount; }

    PipelineResourceSignatureVkImpl* GetSignature(Uint32 index) const
    {
        VERIFY_EXPR(index < m_SignatureCount);
        return m_Signatures[index].RawPtr<PipelineResourceSignatureVkImpl>();
    }

    Uint32 GetDescrSetIndex(const IPipelineResourceSignature* pPRS) const
    {
        Uint32 Index = pPRS->GetDesc().BindingIndex;
        return Index < m_SignatureCount ? m_DescSetOffset[Index] : ~0u;
    }

    Uint32 GetDynamicBufferOffset(const IPipelineResourceSignature* pPRS) const
    {
        Uint32 Index = pPRS->GetDesc().BindingIndex;
        return Index < m_SignatureCount ? m_DynBufOffset[Index] : 0;
    }

    struct ResourceInfo
    {
        SHADER_RESOURCE_TYPE Type          = SHADER_RESOURCE_TYPE_UNKNOWN;
        Uint16               DescrSetIndex = 0;
        Uint16               BindingIndex  = 0;
    };
    bool GetResourceInfo(const char* Name, SHADER_TYPE Stage, ResourceInfo& Info) const;

private:
    VulkanUtilities::PipelineLayoutWrapper m_VkPipelineLayout;

    Uint32 m_DynamicOffsetCount : 25;

    Uint32 m_SignatureCount : 3;
    static_assert(MAX_RESOURCE_SIGNATURES == (1 << 3), "update m_SignatureCount bits count");

    Uint32 m_DescrSetCount : 4;
    static_assert(MAX_RESOURCE_SIGNATURES * 2 == (1 << 4), "update m_DescrSetCount bits count");

    using SignatureArray     = std::array<RefCntAutoPtr<PipelineResourceSignatureVkImpl>, MAX_RESOURCE_SIGNATURES>;
    using DescSetOffsetArray = std::array<Uint8, MAX_RESOURCE_SIGNATURES>;
    using DynBufOffsetArray  = std::array<Uint16, MAX_RESOURCE_SIGNATURES>;

    SignatureArray     m_Signatures;
    DescSetOffsetArray m_DescSetOffset = {};
    DynBufOffsetArray  m_DynBufOffset  = {};
};

} // namespace Diligent
