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

#include "pch.h"
#include <array>

#include "RenderPassVkImpl.hpp"
#include "VulkanTypeConversions.hpp"

namespace Diligent
{

RenderPassVkImpl::RenderPassVkImpl(IReferenceCounters*   pRefCounters,
                                   RenderDeviceVkImpl*   pDevice,
                                   const RenderPassDesc& Desc) :
    TRenderPassBase{pRefCounters, pDevice, Desc}
{
    VkRenderPassCreateInfo RenderPassCI = {};

    RenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassCI.pNext = nullptr;
    RenderPassCI.flags = 0;

    std::array<VkAttachmentDescription, MAX_RENDER_TARGETS + 1> vkAttachments = {};
    for (Uint32 i = 0; i < m_Desc.AttachmentCount; ++i)
    {
        const auto& Attachment      = m_Desc.pAttachments[i];
        auto&       vkAttachment    = vkAttachments[i];
        vkAttachment.flags          = 0;
        vkAttachment.format         = TexFormatToVkFormat(Attachment.Format);
        vkAttachment.samples        = static_cast<VkSampleCountFlagBits>(0x01 << Attachment.SampleCount);
        vkAttachment.loadOp         = AttachmentLoadOpToVkAttachmentLoadOp(Attachment.LoadOp);
        vkAttachment.storeOp        = AttachmentStoreOpToVkAttachmentStoreOp(Attachment.StoreOp);
        vkAttachment.stencilLoadOp  = AttachmentLoadOpToVkAttachmentLoadOp(Attachment.StencilLoadOp);
        vkAttachment.stencilStoreOp = AttachmentStoreOpToVkAttachmentStoreOp(Attachment.StencilStoreOp);
        vkAttachment.initialLayout  = ResourceStateToVkImageLayout(Attachment.InitialState);
        vkAttachment.finalLayout    = ResourceStateToVkImageLayout(Attachment.FinalState);
    }
    RenderPassCI.attachmentCount = Desc.AttachmentCount;
    RenderPassCI.pAttachments    = vkAttachments.data();


    RenderPassCI.subpassCount = Desc.SubpassCount;
    RenderPassCI.pSubpasses;

    RenderPassCI.dependencyCount = Desc.DependencyCount;
    RenderPassCI.pDependencies;
}

RenderPassVkImpl::~RenderPassVkImpl()
{
}

} // namespace Diligent
