/*
 *  Copyright 2025 Diligent Graphics LLC
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

#include "VulkanUtilities/RenderingInfoWrapper.hpp"

#include "BasicMath.hpp"
#include "PlatformMisc.hpp"

namespace VulkanUtilities
{

RenderingInfoWrapper::RenderingInfoWrapper(size_t   Hash,
                                           uint32_t ColorAttachmentCount,
                                           bool     UseDepthAttachment,
                                           bool     UseStencilAttachment) :
    m_RI{},
    m_Hash{Hash}
{
    m_RI.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    m_RI.pNext                = nullptr;
    m_RI.flags                = 0;
    m_RI.colorAttachmentCount = ColorAttachmentCount;

    const uint32_t TotalAttachmentCount = ColorAttachmentCount + (UseDepthAttachment ? 1u : 0u) + (UseStencilAttachment ? 1u : 0u);
    if (TotalAttachmentCount > 0)
    {
        m_Attachments = std::make_unique<VkRenderingAttachmentInfo[]>(TotalAttachmentCount);
        for (size_t i = 0; i < TotalAttachmentCount; ++i)
            m_Attachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    }
    uint32_t AttachmentInd = 0;
    if (ColorAttachmentCount > 0)
    {
        m_RI.pColorAttachments = &m_Attachments[AttachmentInd];
        AttachmentInd += ColorAttachmentCount;
    }
    if (UseDepthAttachment)
    {
        m_DepthAttachmentIndex = AttachmentInd;
        m_RI.pDepthAttachment  = &m_Attachments[AttachmentInd];
        ++AttachmentInd;
    }
    if (UseStencilAttachment)
    {
        m_StencilAttachmentIndex = AttachmentInd;
        m_RI.pStencilAttachment  = &m_Attachments[AttachmentInd];
        ++AttachmentInd;
    }
    VERIFY_EXPR(AttachmentInd == TotalAttachmentCount);
}

VkRenderingFragmentShadingRateAttachmentInfoKHR& RenderingInfoWrapper::GetShadingRateAttachment()
{
    if (!m_ShadingRateAttachment)
    {
        m_ShadingRateAttachment        = std::make_unique<VkRenderingFragmentShadingRateAttachmentInfoKHR>();
        m_ShadingRateAttachment->sType = VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR;

        m_RI.pNext = m_ShadingRateAttachment.get();
    }

    return *m_ShadingRateAttachment;
}

void RenderingInfoWrapper::ResetClears()
{
    while (m_AttachmentClearMask != 0)
    {
        uint32_t Bit = Diligent::ExtractLSB(m_AttachmentClearMask);
        uint32_t Idx = Diligent::PlatformMisc::GetLSB(Bit);
        VERIFY_EXPR(Idx < m_RI.colorAttachmentCount + (m_RI.pDepthAttachment != nullptr ? 1 : 0) + (m_RI.pStencilAttachment != nullptr ? 1 : 0));
        m_Attachments[Idx].loadOp     = VK_ATTACHMENT_LOAD_OP_LOAD;
        m_Attachments[Idx].clearValue = {};
    }
}

} // namespace VulkanUtilities
