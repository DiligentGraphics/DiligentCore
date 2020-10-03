/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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
#include "PipelineLayout.hpp"
#include "ShaderResourceLayoutVk.hpp"
#include "ShaderVariableVk.hpp"
#include "FixedBlockMemoryAllocator.hpp"
#include "SRBMemoryAllocator.hpp"
#include "VulkanUtilities/VulkanObjectWrappers.hpp"
#include "VulkanUtilities/VulkanCommandBuffer.hpp"
#include "PipelineLayout.hpp"
#include "RenderDeviceVkImpl.hpp"

namespace Diligent
{

/// Pipeline state object implementation in Vulkan backend.
class PipelineState2VkImpl final : public DeviceObjectBase<IPipelineState2Vk, RenderDeviceVkImpl, PipelineStateDesc>
{
public:
    using TParent = DeviceObjectBase<IPipelineState2Vk, RenderDeviceVkImpl, PipelineStateDesc>;

    PipelineState2VkImpl(IReferenceCounters* pRefCounters, RenderDeviceVkImpl* pDeviceVk, const PipelineStateCreateInfo& CreateInfo);
    ~PipelineState2VkImpl();

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    /// Implementation of IPipelineState2::CreateShaderResourceBinding() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding2** ppShaderResourceBinding, bool InitStaticResources) override final;

    /// Implementation of IPipelineState2::IsCompatibleWith() in Vulkan backend.
    bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineState2* pPSO) const override final;

    /// Implementation of IPipelineState2Vk::GetRenderPass().
    virtual IRenderPassVk* DILIGENT_CALL_TYPE GetRenderPass() const override final { return nullptr; }

    /// Implementation of IPipelineState2Vk::GetVkPipeline().
    virtual VkPipeline DILIGENT_CALL_TYPE GetVkPipeline() const override final { return VK_NULL_HANDLE; }

    /// Implementation of IPipelineState2::BindStaticResources() in Vulkan backend.
    virtual void DILIGENT_CALL_TYPE BindStaticResources(IResourceMapping* pResourceMapping, Uint32 Flags) override final;

    /// Implementation of IPipelineState2::GetStaticVariableCount() in Vulkan backend.
    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount() const override final;

    /// Implementation of IPipelineState2::GetStaticVariableByName() in Vulkan backend.
    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(const Char* Name) override final;

    /// Implementation of IPipelineState2::GetStaticVariableByIndex() in Vulkan backend.
    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(Uint32 Index) override final;

    void CommitAndTransitionShaderResources(IShaderResourceBinding2*               pShaderResourceBinding,
                                            DeviceContextVkImpl*                   pCtxVkImpl,
                                            bool                                   CommitResources,
                                            RESOURCE_STATE_TRANSITION_MODE         StateTransitionMode,
                                            PipelineLayout::DescriptorSetBindInfo* pDescrSetBindInfo) const;

    __forceinline void BindDescriptorSetsWithDynamicOffsets(VulkanUtilities::VulkanCommandBuffer&  CmdBuffer,
                                                            Uint32                                 CtxId,
                                                            DeviceContextVkImpl*                   pCtxVkImpl,
                                                            PipelineLayout::DescriptorSetBindInfo& BindInfo);

    const PipelineLayout& GetPipelineLayout() const;

    const ShaderResourceLayoutVk& GetShaderResLayout(Uint32 ShaderInd) const;

    SRBMemoryAllocator& GetSRBMemoryAllocator();

    static RenderPassDesc GetImplicitRenderPassDesc(Uint32                                                        NumRenderTargets,
                                                    const TEXTURE_FORMAT                                          RTVFormats[],
                                                    TEXTURE_FORMAT                                                DSVFormat,
                                                    Uint8                                                         SampleCount,
                                                    std::array<RenderPassAttachmentDesc, MAX_RENDER_TARGETS + 1>& Attachments,
                                                    std::array<AttachmentReference, MAX_RENDER_TARGETS + 1>&      AttachmentReferences,
                                                    SubpassDesc&                                                  SubpassDesc);


    void InitializeStaticSRBResources(ShaderResourceCacheVk& ResourceCache) const;

private:
};

} // namespace Diligent
