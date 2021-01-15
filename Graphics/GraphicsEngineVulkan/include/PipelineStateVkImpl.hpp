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
/// Declaration of Diligent::PipelineStateVkImpl class

#include <array>

#include "RenderDeviceVk.h"
#include "PipelineStateVk.h"
#include "PipelineStateBase.hpp"
#include "ShaderVariableVk.hpp"
#include "FixedBlockMemoryAllocator.hpp"
#include "SRBMemoryAllocator.hpp"
#include "VulkanUtilities/VulkanObjectWrappers.hpp"
#include "VulkanUtilities/VulkanCommandBuffer.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "PipelineLayoutVk.hpp"

namespace Diligent
{

class FixedBlockMemoryAllocator;
class ShaderVariableManagerVk;
class ShaderVkImpl;

/// Pipeline state object implementation in Vulkan backend.
class PipelineStateVkImpl final : public PipelineStateBase<IPipelineStateVk, RenderDeviceVkImpl>
{
public:
    using TPipelineStateBase = PipelineStateBase<IPipelineStateVk, RenderDeviceVkImpl>;

    PipelineStateVkImpl(IReferenceCounters* pRefCounters, RenderDeviceVkImpl* pDeviceVk, const GraphicsPipelineStateCreateInfo& CreateInfo);
    PipelineStateVkImpl(IReferenceCounters* pRefCounters, RenderDeviceVkImpl* pDeviceVk, const ComputePipelineStateCreateInfo& CreateInfo);
    PipelineStateVkImpl(IReferenceCounters* pRefCounters, RenderDeviceVkImpl* pDeviceVk, const RayTracingPipelineStateCreateInfo& CreateInfo);
    ~PipelineStateVkImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_PipelineStateVk, TPipelineStateBase)

    /// Implementation of IPipelineState::IsCompatibleWith() in Vulkan backend.
    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineState* pPSO) const override final;

    /// Implementation of IPipelineStateVk::GetRenderPass().
    virtual IRenderPassVk* DILIGENT_CALL_TYPE GetRenderPass() const override final { return m_pRenderPass.RawPtr<IRenderPassVk>(); }

    /// Implementation of IPipelineStateVk::GetVkPipeline().
    virtual VkPipeline DILIGENT_CALL_TYPE GetVkPipeline() const override final { return m_Pipeline; }

    /// Implementation of IPipelineState::BindStaticResources() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE BindStaticResources(Uint32 ShaderFlags, IResourceMapping* pResourceMapping, Uint32 Flags) override final;

    /// Implementation of IPipelineState::GetResourceSignatureCount() in Vulkan backend.
    virtual Uint32 DILIGENT_CALL_TYPE GetResourceSignatureCount() const override final { return m_PipelineLayout->GetSignatureCount(); }

    /// Implementation of IPipelineState::GetResourceSignature() in Vulkan backend.
    virtual IPipelineResourceSignature* DILIGENT_CALL_TYPE GetResourceSignature(Uint32 Index) const override final { return m_PipelineLayout->GetSignature(Index); }

    __forceinline void BindDescriptorSetsWithDynamicOffsets(VulkanUtilities::VulkanCommandBuffer&    CmdBuffer,
                                                            Uint32                                   CtxId,
                                                            DeviceContextVkImpl*                     pCtxVkImpl,
                                                            PipelineLayoutVk::DescriptorSetBindInfo& BindInfo)
    {
        m_PipelineLayout->BindDescriptorSetsWithDynamicOffsets(CmdBuffer, CtxId, pCtxVkImpl, BindInfo);
    }

    const PipelineLayoutVk& GetPipelineLayout() const { return *m_PipelineLayout; }

    static RenderPassDesc GetImplicitRenderPassDesc(Uint32                                                        NumRenderTargets,
                                                    const TEXTURE_FORMAT                                          RTVFormats[],
                                                    TEXTURE_FORMAT                                                DSVFormat,
                                                    Uint8                                                         SampleCount,
                                                    std::array<RenderPassAttachmentDesc, MAX_RENDER_TARGETS + 1>& Attachments,
                                                    std::array<AttachmentReference, MAX_RENDER_TARGETS + 1>&      AttachmentReferences,
                                                    SubpassDesc&                                                  SubpassDesc);


    void InitializeStaticSRBResources(ShaderResourceCacheVk& ResourceCache) const;

    bool IsIncompatibleWith(IPipelineResourceSignature* pPRS) const
    {
        return !m_PipelineLayout->HasSignature(pPRS);
    }

    struct ShaderStageInfo
    {
        ShaderStageInfo() {}
        ShaderStageInfo(const ShaderVkImpl* pShader);

        void   Append(const ShaderVkImpl* pShader);
        size_t Count() const;

        // Shader stage type. All shaders in the stage must have the same type.
        SHADER_TYPE Type = SHADER_TYPE_UNKNOWN;

        std::vector<const ShaderVkImpl*>   Shaders;
        std::vector<std::vector<uint32_t>> SPIRVs;
    };
    using TShaderStages = std::vector<ShaderStageInfo>;

private:
    template <typename PSOCreateInfoType>
    TShaderStages InitInternalObjects(const PSOCreateInfoType&                           CreateInfo,
                                      std::vector<VkPipelineShaderStageCreateInfo>&      vkShaderStages,
                                      std::vector<VulkanUtilities::ShaderModuleWrapper>& ShaderModules);

    void InitPipelineLayout(const PipelineStateCreateInfo& CreateInfo,
                            TShaderStages&                 ShaderStages);

    void InitResourceLayouts(const PipelineStateCreateInfo& CreateInfo,
                             TShaderStages&                 ShaderStages);

    void Destruct();

    void* m_pRawMem = nullptr; // AZ TODO: remove

    VulkanUtilities::PipelineWrapper m_Pipeline;
    RefCntAutoPtr<PipelineLayoutVk>  m_PipelineLayout;

    bool m_HasStaticResources    = false;
    bool m_HasNonStaticResources = false;
};

} // namespace Diligent
