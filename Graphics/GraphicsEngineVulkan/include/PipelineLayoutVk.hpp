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
class PipelineLayoutVk final : public ObjectBase<IObject>
{
public:
    PipelineLayoutVk(IReferenceCounters* pRefCounters, RenderDeviceVkImpl* pDeviceVk, IPipelineResourceSignature** ppSignatures, Uint32 SignatureCount);
    ~PipelineLayoutVk();

    using ObjectBase<IObject>::Release;

    void Finalize();

    size_t GetHash() const;

    VkPipelineLayout GetVkPipelineLayout() const { return m_VkPipelineLayout; }
    PIPELINE_TYPE    GetPipelineType() const { return m_PipelineType; }
    Uint32           GetSignatureCount() const { return m_SignatureCount; }
    Uint32           GetDescriptorSetCount() const { return m_DescrSetCount; }
    Uint32           GetDynamicOffsetCount() const { return m_DynamicOffsetCount; }

    IPipelineResourceSignature* GetSignature(Uint32 index) const
    {
        VERIFY_EXPR(index < m_SignatureCount);
        return m_Signatures[index].RawPtr<IPipelineResourceSignature>();
    }

    bool HasSignature(IPipelineResourceSignature* pPRS) const
    {
        Uint32 Index = pPRS->GetDesc().BindingIndex;
        return Index < m_SignatureCount && m_Signatures[Index].RawPtr() == pPRS;
    }

    Uint32 GetDescrSetIndex(IPipelineResourceSignature* pPRS) const
    {
        Uint32 Index = pPRS->GetDesc().BindingIndex;
        return Index < m_SignatureCount ? m_DescSetOffset[Index] : ~0u;
    }

    Uint32 GetDynamicBufferOffset(IPipelineResourceSignature* pPRS) const
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

    using DescriptorSetBindInfo = PipelineResourceSignatureVkImpl::DescriptorSetBindInfo;

    // Computes dynamic offsets and binds descriptor sets
    __forceinline void BindDescriptorSetsWithDynamicOffsets(VulkanUtilities::VulkanCommandBuffer& CmdBuffer,
                                                            Uint32                                CtxId,
                                                            DeviceContextVkImpl*                  pCtxVkImpl,
                                                            DescriptorSetBindInfo&                BindInfo) const;

private:
    VulkanUtilities::PipelineLayoutWrapper m_VkPipelineLayout;

    RenderDeviceVkImpl* m_pDeviceVk;

    // AZ TODO: pack bits
    Uint16 m_DynamicOffsetCount = 0;
    Uint8  m_SignatureCount     = 0;
    Uint8  m_DescrSetCount      = 0;

    PIPELINE_TYPE m_PipelineType = PIPELINE_TYPE(0xFF);

    using SignatureArray     = std::array<RefCntAutoPtr<PipelineResourceSignatureVkImpl>, MAX_RESOURCE_SIGNATURES>;
    using DescSetOffsetArray = std::array<Uint8, MAX_RESOURCE_SIGNATURES>;
    using DynBufOffsetArray  = std::array<Uint16, MAX_RESOURCE_SIGNATURES>;

    SignatureArray     m_Signatures;
    DescSetOffsetArray m_DescSetOffset = {};
    DynBufOffsetArray  m_DynBufOffset  = {};
};


__forceinline void PipelineLayoutVk::BindDescriptorSetsWithDynamicOffsets(VulkanUtilities::VulkanCommandBuffer& CmdBuffer,
                                                                          Uint32                                CtxId,
                                                                          DeviceContextVkImpl*                  pCtxVkImpl,
                                                                          DescriptorSetBindInfo&                BindInfo) const
{
    /*
    VERIFY(BindInfo.pDbgPipelineLayout != nullptr, "Pipeline layout is not initialized, which most likely means that CommitShaderResources() has never been called");
    VERIFY(BindInfo.pDbgPipelineLayout->IsSameAs(*this), "Inconsistent pipeline layout");
    VERIFY(BindInfo.DynamicOffsetCount > 0, "This function should only be called for pipelines that contain dynamic descriptors");
    VERIFY_EXPR(BindInfo.pResourceCache != nullptr);

#ifdef DILIGENT_DEBUG
    Uint32 TotalDynamicDescriptors = 0;
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
         VarType <= SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
         VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        const auto& Set = m_LayoutMgr.GetDescriptorSet(VarType);
        TotalDynamicDescriptors += Set.NumDynamicDescriptors;
    }
    VERIFY(BindInfo.DynamicOffsetCount == TotalDynamicDescriptors, "Incosistent dynamic buffer size");
    VERIFY_EXPR(BindInfo.DynamicOffsets.size() >= BindInfo.DynamicOffsetCount);
#endif

    auto NumOffsetsWritten = BindInfo.pResourceCache->GetDynamicBufferOffsets(CtxId, pCtxVkImpl, BindInfo.DynamicOffsets);
    VERIFY_EXPR(NumOffsetsWritten == BindInfo.DynamicOffsetCount);
    (void)NumOffsetsWritten;

    // Note that there is one global dynamic buffer from which all dynamic resources are suballocated in Vulkan back-end,
    // and this buffer is not resizable, so the buffer handle can never change.

    // vkCmdBindDescriptorSets causes the sets numbered [firstSet .. firstSet+descriptorSetCount-1] to use the
    // bindings stored in pDescriptorSets[0 .. descriptorSetCount-1] for subsequent rendering commands
    // (either compute or graphics, according to the pipelineBindPoint). Any bindings that were previously
    // applied via these sets are no longer valid (13.2.5)
    CmdBuffer.BindDescriptorSets(BindInfo.BindPoint,
                                 m_LayoutMgr.GetVkPipelineLayout(),
                                 0, // First set
                                 BindInfo.SetCout,
                                 BindInfo.vkSets.data(), // BindInfo.vkSets is never empty
                                 // dynamicOffsetCount must equal the total number of dynamic descriptors in the sets being bound (13.2.5)
                                 BindInfo.DynamicOffsetCount,
                                 BindInfo.DynamicOffsets.data());

    BindInfo.DynamicDescriptorsBound = true;
*/
}

} // namespace Diligent
