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

#include "FramebufferGLImpl.hpp"
#include "FBOCache.hpp"
#include "TextureViewGLImpl.hpp"

namespace Diligent
{

FramebufferGLImpl::FramebufferGLImpl(IReferenceCounters*    pRefCounters,
                                     RenderDeviceGLImpl*    pDevice,
                                     GLContextState&        CtxState,
                                     const FramebufferDesc& Desc) :
    TFramebufferBase{pRefCounters, pDevice, Desc}
{
    const auto& RPDesc = m_Desc.pRenderPass->GetDesc();
    m_SubpassFramebuffers.reserve(RPDesc.SubpassCount);
    for (Uint32 subpass = 0; subpass < RPDesc.SubpassCount; ++subpass)
    {
        const auto& SubpassDesc = RPDesc.pSubpasses[subpass];

        TextureViewGLImpl* ppRTVs[MAX_RENDER_TARGETS] = {};
        TextureViewGLImpl* pDSV                       = nullptr;

        for (Uint32 rt = 0; rt < SubpassDesc.RenderTargetAttachmentCount; ++rt)
        {
            const auto& RTAttachmentRef = SubpassDesc.pRenderTargetAttachments[rt];
            if (RTAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
            {
                ppRTVs[rt] = ValidatedCast<TextureViewGLImpl>(m_Desc.ppAttachments[RTAttachmentRef.AttachmentIndex]);
            }
        }

        if (SubpassDesc.pDepthStencilAttachment != nullptr && SubpassDesc.pDepthStencilAttachment->AttachmentIndex != ATTACHMENT_UNUSED)
        {
            pDSV = ValidatedCast<TextureViewGLImpl>(m_Desc.ppAttachments[SubpassDesc.pDepthStencilAttachment->AttachmentIndex]);
        }
        auto FBO = FBOCache::CreateFBO(CtxState, SubpassDesc.RenderTargetAttachmentCount, ppRTVs, pDSV);
        m_SubpassFramebuffers.emplace_back(std::move(FBO));
    }
}

FramebufferGLImpl::~FramebufferGLImpl()
{
}

} // namespace Diligent
