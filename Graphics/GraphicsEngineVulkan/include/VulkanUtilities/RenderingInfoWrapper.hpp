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

#pragma once

#include <memory>

#include "VulkanHeaders.h"
#include "DebugUtilities.hpp"

namespace VulkanUtilities
{

class RenderingInfoWrapper
{
public:
    RenderingInfoWrapper(size_t   Hash,
                         uint32_t ColorAttachmentCount,
                         bool     UseDepthAttachment,
                         bool     UseStencilAttachment);

    // clang-format off
    RenderingInfoWrapper           (const RenderingInfoWrapper&) = delete;
    RenderingInfoWrapper           (RenderingInfoWrapper&&)      = delete;
    RenderingInfoWrapper& operator=(const RenderingInfoWrapper&) = delete;
    RenderingInfoWrapper& operator=(RenderingInfoWrapper&&)      = delete;
    // clang-format on

    operator const VkRenderingInfoKHR&() const { return m_RI; }

    const VkRenderingInfoKHR& Get() const { return m_RI; }

    size_t GetHash() const { return m_Hash; }

    RenderingInfoWrapper& SetFlags(VkRenderingFlagsKHR flags)
    {
        m_RI.flags = flags;
        return *this;
    }

    RenderingInfoWrapper& SetRenderArea(const VkRect2D& renderArea)
    {
        m_RI.renderArea = renderArea;
        return *this;
    }

    RenderingInfoWrapper& SetLayerCount(uint32_t layerCount)
    {
        m_RI.layerCount = layerCount;
        return *this;
    }

    RenderingInfoWrapper& SetViewMask(uint32_t viewMask)
    {
        m_RI.viewMask = viewMask;
        return *this;
    }

    VkRenderingAttachmentInfoKHR& GetColorAttachment(uint32_t Index)
    {
        VERIFY_EXPR(Index < m_RI.colorAttachmentCount);
        return m_Attachments[Index];
    }

    VkRenderingAttachmentInfoKHR& GetDepthAttachment()
    {
        VERIFY_EXPR(m_RI.pDepthAttachment != nullptr && m_DepthAttachmentIndex != ~0u);
        return m_Attachments[m_DepthAttachmentIndex];
    }

    VkRenderingAttachmentInfoKHR& GetStencilAttachment()
    {
        VERIFY_EXPR(m_RI.pStencilAttachment != nullptr && m_StencilAttachmentIndex != ~0u);
        return m_Attachments[m_StencilAttachmentIndex];
    }

    VkRenderingFragmentShadingRateAttachmentInfoKHR& GetShadingRateAttachment();


    void SetColorAttachmentClearValue(uint32_t Index, const VkClearColorValue& ClearValue)
    {
        VERIFY_EXPR(Index < m_RI.colorAttachmentCount);
        m_Attachments[Index].clearValue.color = ClearValue;
        m_Attachments[Index].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        m_AttachmentClearMask |= 1u << Index;
    }

    void SetDepthAttachmentClearValue(float Depth)
    {
        VERIFY_EXPR(m_RI.pDepthAttachment != nullptr && m_DepthAttachmentIndex != ~0u);
        m_Attachments[m_DepthAttachmentIndex].clearValue.depthStencil.depth = Depth;
        m_Attachments[m_DepthAttachmentIndex].loadOp                        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        m_AttachmentClearMask |= 1u << m_DepthAttachmentIndex;
    }

    void SetStencilAttachmentClearValue(uint32_t Stencil)
    {
        VERIFY_EXPR(m_RI.pStencilAttachment != nullptr && m_StencilAttachmentIndex != ~0u);
        m_Attachments[m_StencilAttachmentIndex].clearValue.depthStencil.stencil = Stencil;
        m_Attachments[m_StencilAttachmentIndex].loadOp                          = VK_ATTACHMENT_LOAD_OP_CLEAR;
        m_AttachmentClearMask |= 1u << m_StencilAttachmentIndex;
    }

    void ResetClears();
    bool HasClears() const { return m_AttachmentClearMask != 0; }

private:
    VkRenderingInfoKHR m_RI;

    const size_t m_Hash = 0;

    std::unique_ptr<VkRenderingAttachmentInfoKHR[]>                  m_Attachments;
    std::unique_ptr<VkRenderingFragmentShadingRateAttachmentInfoKHR> m_ShadingRateAttachment;

    uint32_t m_DepthAttachmentIndex   = ~0u;
    uint32_t m_StencilAttachmentIndex = ~0u;
    uint32_t m_AttachmentClearMask    = 0;
};

} // namespace VulkanUtilities
