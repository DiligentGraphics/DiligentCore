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
#include <vector>

#include "RenderPassVkImpl.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "VulkanTypeConversions.hpp"

namespace Diligent
{

RenderPassVkImpl::RenderPassVkImpl(IReferenceCounters*   pRefCounters,
                                   RenderDeviceVkImpl*   pDevice,
                                   const RenderPassDesc& Desc,
                                   bool                  IsDeviceInternal) :
    TRenderPassBase{pRefCounters, pDevice, Desc, IsDeviceInternal}
{
    const auto& LogicalDevice         = pDevice->GetLogicalDevice();
    const auto& ExtFeats              = LogicalDevice.GetEnabledExtFeatures();
    const bool  ShadingRateEnabled    = ExtFeats.ShadingRate.attachmentFragmentShadingRate != VK_FALSE;
    const bool  FragDensityMapEnabled = ExtFeats.FragmentDensityMap.fragmentDensityMap != VK_FALSE;

    VkRenderPassCreateInfo2 RenderPassCI{};
    RenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
    RenderPassCI.pNext = nullptr;
    RenderPassCI.flags = 0;

    std::vector<VkAttachmentDescription2> vkAttachments(m_Desc.AttachmentCount);
    for (Uint32 i = 0; i < m_Desc.AttachmentCount; ++i)
    {
        const auto& Attachment      = m_Desc.pAttachments[i];
        auto&       vkAttachment    = vkAttachments[i];
        vkAttachment.sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
        vkAttachment.pNext          = nullptr;
        vkAttachment.flags          = 0;
        vkAttachment.format         = TexFormatToVkFormat(Attachment.Format);
        vkAttachment.samples        = static_cast<VkSampleCountFlagBits>(Attachment.SampleCount);
        vkAttachment.loadOp         = AttachmentLoadOpToVkAttachmentLoadOp(Attachment.LoadOp);
        vkAttachment.storeOp        = AttachmentStoreOpToVkAttachmentStoreOp(Attachment.StoreOp);
        vkAttachment.stencilLoadOp  = AttachmentLoadOpToVkAttachmentLoadOp(Attachment.StencilLoadOp);
        vkAttachment.stencilStoreOp = AttachmentStoreOpToVkAttachmentStoreOp(Attachment.StencilStoreOp);
        vkAttachment.initialLayout  = ResourceStateToVkImageLayout(Attachment.InitialState, /*IsInsideRenderPass = */ false, FragDensityMapEnabled);
        vkAttachment.finalLayout    = ResourceStateToVkImageLayout(Attachment.FinalState, /*IsInsideRenderPass = */ true, FragDensityMapEnabled);
    }
    RenderPassCI.attachmentCount = Desc.AttachmentCount;
    RenderPassCI.pAttachments    = vkAttachments.data();

    Uint32 TotalAttachmentReferencesCount   = 0;
    Uint32 TotalPreserveAttachmentsCount    = 0;
    Uint32 TotalShadingRateAttachmentsCount = 0;
    for (Uint32 i = 0; i < m_Desc.SubpassCount; ++i)
    {
        const auto& Subpass = m_Desc.pSubpasses[i];
        TotalAttachmentReferencesCount += Subpass.InputAttachmentCount;
        TotalAttachmentReferencesCount += Subpass.RenderTargetAttachmentCount;
        if (Subpass.pResolveAttachments != nullptr)
            TotalAttachmentReferencesCount += Subpass.RenderTargetAttachmentCount;
        if (Subpass.pDepthStencilAttachment != nullptr)
            TotalAttachmentReferencesCount += 1;
        if (Subpass.pShadingRateAttachment != nullptr && ShadingRateEnabled)
            TotalShadingRateAttachmentsCount += 1;
        TotalPreserveAttachmentsCount += Subpass.PreserveAttachmentCount;
    }

    std::vector<VkAttachmentReference2>                 vkAttachmentReferences(TotalAttachmentReferencesCount + TotalShadingRateAttachmentsCount);
    std::vector<Uint32>                                 vkPreserveAttachments(TotalPreserveAttachmentsCount);
    std::vector<VkFragmentShadingRateAttachmentInfoKHR> vkShadingRate{TotalShadingRateAttachmentsCount};
    const ShadingRateAttachment*                        pMainSRA = nullptr;

    Uint32 CurrAttachmentReferenceInd = 0;
    Uint32 CurrPreserveAttachmentInd  = 0;

    std::vector<VkSubpassDescription2> vkSubpasses{Desc.SubpassCount};
    for (Uint32 i = 0, SRInd = 0; i < m_Desc.SubpassCount; ++i)
    {
        const auto&  SubpassDesc   = m_Desc.pSubpasses[i];
        auto&        vkSubpass     = vkSubpasses[i];
        const void** ppSubpassNext = &vkSubpass.pNext;

        vkSubpass.sType                = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        vkSubpass.flags                = 0;
        vkSubpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vkSubpass.inputAttachmentCount = SubpassDesc.InputAttachmentCount;

        auto ConvertAttachmentReferences = [&](Uint32 NumAttachments, const AttachmentReference* pSrcAttachments, VkImageAspectFlags AspectMask) //
        {
            auto* pCurrVkAttachmentReference = &vkAttachmentReferences[CurrAttachmentReferenceInd];
            for (Uint32 attachment = 0; attachment < NumAttachments; ++attachment, ++CurrAttachmentReferenceInd)
            {
                const auto& SrcAttachmnetRef = pSrcAttachments[attachment];
                auto&       DstAttachmnetRef = vkAttachmentReferences[CurrAttachmentReferenceInd];

                DstAttachmnetRef.sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
                DstAttachmnetRef.pNext      = nullptr;
                DstAttachmnetRef.attachment = SrcAttachmnetRef.AttachmentIndex;
                DstAttachmnetRef.layout     = ResourceStateToVkImageLayout(SrcAttachmnetRef.State, /*IsInsideRenderPass = */ true, FragDensityMapEnabled);
                DstAttachmnetRef.aspectMask = AspectMask;
            }
            return pCurrVkAttachmentReference;
        };

        if (SubpassDesc.InputAttachmentCount != 0)
        {
            vkSubpass.pInputAttachments = ConvertAttachmentReferences(SubpassDesc.InputAttachmentCount, SubpassDesc.pInputAttachments, VK_IMAGE_ASPECT_COLOR_BIT);
        }

        vkSubpass.colorAttachmentCount = SubpassDesc.RenderTargetAttachmentCount;
        if (SubpassDesc.RenderTargetAttachmentCount != 0)
        {
            vkSubpass.pColorAttachments = ConvertAttachmentReferences(SubpassDesc.RenderTargetAttachmentCount, SubpassDesc.pRenderTargetAttachments, VK_IMAGE_ASPECT_COLOR_BIT);
            if (SubpassDesc.pResolveAttachments != nullptr)
            {
                vkSubpass.pResolveAttachments = ConvertAttachmentReferences(SubpassDesc.RenderTargetAttachmentCount, SubpassDesc.pResolveAttachments, VK_IMAGE_ASPECT_COLOR_BIT);
            }
        }

        if (SubpassDesc.pDepthStencilAttachment != nullptr)
        {
            vkSubpass.pDepthStencilAttachment = ConvertAttachmentReferences(1, SubpassDesc.pDepthStencilAttachment, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        }

        vkSubpass.preserveAttachmentCount = SubpassDesc.PreserveAttachmentCount;
        if (SubpassDesc.PreserveAttachmentCount != 0)
        {
            vkSubpass.pPreserveAttachments = &vkPreserveAttachments[CurrPreserveAttachmentInd];
            for (Uint32 prsv_attachment = 0; prsv_attachment < SubpassDesc.PreserveAttachmentCount; ++prsv_attachment, ++CurrPreserveAttachmentInd)
            {
                vkPreserveAttachments[CurrPreserveAttachmentInd] = SubpassDesc.pPreserveAttachments[prsv_attachment];
            }
        }

        if (SubpassDesc.pShadingRateAttachment != nullptr)
        {
            if (ShadingRateEnabled)
            {
                const auto& SRAttachment   = *SubpassDesc.pShadingRateAttachment;
                auto&       vkSRAttachment = vkShadingRate[SRInd++];

                *ppSubpassNext = &vkSRAttachment;
                ppSubpassNext  = &vkSRAttachment.pNext;

                vkSRAttachment.sType                          = VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR;
                vkSRAttachment.pFragmentShadingRateAttachment = ConvertAttachmentReferences(1, &SRAttachment.Attachment, VK_IMAGE_ASPECT_COLOR_BIT);
                vkSRAttachment.shadingRateAttachmentTexelSize = {SRAttachment.TileSize[0], SRAttachment.TileSize[1]};
            }
            else
            {
                VERIFY_EXPR(FragDensityMapEnabled);
                pMainSRA = pMainSRA ? pMainSRA : SubpassDesc.pShadingRateAttachment;
            }
        }

        *ppSubpassNext = nullptr;
    }

    if (FragDensityMapEnabled && pMainSRA != nullptr)
    {
        for (Uint32 i = 0; i < m_Desc.SubpassCount; ++i)
        {
            const auto& SubpassDesc = m_Desc.pSubpasses[i];

            if (SubpassDesc.pShadingRateAttachment == nullptr)
                LOG_ERROR_AND_THROW("Vk_EXT_fragment_density_map extension requires that shading rate attachment is specified for all subpasses");

            if (*pMainSRA != *SubpassDesc.pShadingRateAttachment)
                LOG_ERROR_AND_THROW("Vk_EXT_fragment_density_map extension requires that shading rate attachment is the same for all subpasses");
        }
    }

    VERIFY_EXPR(CurrAttachmentReferenceInd == vkAttachmentReferences.size());
    VERIFY_EXPR(CurrPreserveAttachmentInd == vkPreserveAttachments.size());
    RenderPassCI.subpassCount = Desc.SubpassCount;
    RenderPassCI.pSubpasses   = vkSubpasses.data();

    std::vector<VkSubpassDependency2> vkDependencies(Desc.DependencyCount);
    for (Uint32 i = 0; i < Desc.DependencyCount; ++i)
    {
        const auto& DependencyDesc = m_Desc.pDependencies[i];
        auto&       vkDependency   = vkDependencies[i];
        vkDependency.sType         = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        vkDependency.pNext         = nullptr;
        vkDependency.srcSubpass    = DependencyDesc.SrcSubpass;
        vkDependency.dstSubpass    = DependencyDesc.DstSubpass;
        vkDependency.srcStageMask  = DependencyDesc.SrcStageMask;
        vkDependency.dstStageMask  = DependencyDesc.DstStageMask;
        vkDependency.srcAccessMask = DependencyDesc.SrcAccessMask;
        vkDependency.dstAccessMask = DependencyDesc.DstAccessMask;

        // VK_DEPENDENCY_BY_REGION_BIT specifies that dependencies will be framebuffer-local.
        // Framebuffer-local dependencies are more optimal for most architectures; particularly
        // tile-based architectures - which can keep framebuffer-regions entirely in on-chip registers
        // and thus avoid external bandwidth across such a dependency. Including a framebuffer-global
        // dependency in your rendering will usually force all implementations to flush data to memory,
        // or to a higher level cache, breaking any potential locality optimizations.
        vkDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // For multiview
        vkDependency.viewOffset = 0;
    }
    RenderPassCI.dependencyCount = Desc.DependencyCount;
    RenderPassCI.pDependencies   = vkDependencies.data();

    // For multiview
    RenderPassCI.correlatedViewMaskCount = 0;
    RenderPassCI.pCorrelatedViewMasks    = nullptr;

    // Enable fragment density map
    VkRenderPassFragmentDensityMapCreateInfoEXT FragDensityMapCI{};
    if (FragDensityMapEnabled && pMainSRA != nullptr)
    {
        RenderPassCI.pNext     = &FragDensityMapCI;
        FragDensityMapCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
        FragDensityMapCI.pNext = nullptr;

        FragDensityMapCI.fragmentDensityMapAttachment.attachment = pMainSRA->Attachment.AttachmentIndex;
        FragDensityMapCI.fragmentDensityMapAttachment.layout     = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
    }

    m_VkRenderPass = LogicalDevice.CreateRenderPass(RenderPassCI, Desc.Name);
    if (!m_VkRenderPass)
    {
        LOG_ERROR_AND_THROW("Failed to create Vulkan render pass");
    }
}

RenderPassVkImpl::~RenderPassVkImpl()
{
    m_pDevice->SafeReleaseDeviceObject(std::move(m_VkRenderPass), ~Uint64{0});
    Destruct();
}

} // namespace Diligent
