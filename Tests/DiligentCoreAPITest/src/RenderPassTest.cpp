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

#include <algorithm>

#include "TestingEnvironment.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(RenderDeviceTest, CreateRenderPass)
{
    auto* pDevice = TestingEnvironment::GetInstance()->GetDevice();

    RenderPassAttachmentDesc Attachments[5];
    Attachments[0].Format       = TEX_FORMAT_RGBA8_UNORM;
    Attachments[0].SampleCount  = 4;
    Attachments[0].InitialState = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[0].FinalState   = RESOURCE_STATE_RENDER_TARGET;
    Attachments[0].LoadOp       = ATTACHMENT_LOAD_OP_LOAD;
    Attachments[0].StoreOp      = ATTACHMENT_STORE_OP_STORE;

    Attachments[1].Format         = TEX_FORMAT_D32_FLOAT_S8X24_UINT;
    Attachments[1].SampleCount    = 4;
    Attachments[1].InitialState   = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[1].FinalState     = RESOURCE_STATE_DEPTH_WRITE;
    Attachments[1].LoadOp         = ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].StoreOp        = ATTACHMENT_STORE_OP_DONT_CARE;
    Attachments[1].StencilLoadOp  = ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].StencilStoreOp = ATTACHMENT_STORE_OP_DONT_CARE;

    Attachments[2].Format       = TEX_FORMAT_RGBA8_UNORM;
    Attachments[2].SampleCount  = 1;
    Attachments[2].InitialState = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[2].FinalState   = RESOURCE_STATE_RENDER_TARGET;
    Attachments[2].LoadOp       = ATTACHMENT_LOAD_OP_DONT_CARE;
    Attachments[2].StoreOp      = ATTACHMENT_STORE_OP_STORE;

    Attachments[3].Format       = TEX_FORMAT_RGBA32_FLOAT;
    Attachments[3].SampleCount  = 1;
    Attachments[3].InitialState = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[3].FinalState   = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[3].LoadOp       = ATTACHMENT_LOAD_OP_LOAD;
    Attachments[3].StoreOp      = ATTACHMENT_STORE_OP_STORE;

    Attachments[4].Format       = TEX_FORMAT_RGBA8_UNORM;
    Attachments[4].SampleCount  = 1;
    Attachments[4].InitialState = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[4].FinalState   = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[4].LoadOp       = ATTACHMENT_LOAD_OP_LOAD;
    Attachments[4].StoreOp      = ATTACHMENT_STORE_OP_STORE;

    SubpassDesc Subpasses[2] = {};

    Subpasses[0].InputAttachmentCount = 1;
    AttachmentReference InputAttachmentRef{3, RESOURCE_STATE_SHADER_RESOURCE};
    Subpasses[0].pInputAttachments           = &InputAttachmentRef;
    Subpasses[0].RenderTargetAttachmentCount = 1;
    AttachmentReference RTAttachmentRef{0, RESOURCE_STATE_RENDER_TARGET};
    Subpasses[0].pRenderTargetAttachments = &RTAttachmentRef;
    AttachmentReference ResolveAttachmentRef{2, RESOURCE_STATE_RESOLVE_DEST};
    Subpasses[0].pResolveAttachments = &ResolveAttachmentRef;
    AttachmentReference DSAttachmentRef{1, RESOURCE_STATE_DEPTH_WRITE};
    Subpasses[0].pDepthStencilAttachment = &DSAttachmentRef;
    Uint32 PreserveAttachment            = 4;
    Subpasses[0].PreserveAttachmentCount = 1;
    Subpasses[0].pPreserveAttachments    = &PreserveAttachment;

    Subpasses[1].InputAttachmentCount = 1;
    AttachmentReference InputAttachment2Ref{2, RESOURCE_STATE_SHADER_RESOURCE};
    Subpasses[1].pInputAttachments           = &InputAttachment2Ref;
    Subpasses[1].RenderTargetAttachmentCount = 1;
    AttachmentReference RT2AttachmentRef{4, RESOURCE_STATE_RENDER_TARGET};
    Subpasses[1].pRenderTargetAttachments = &RT2AttachmentRef;

    RenderPassDesc RPDesc;
    RPDesc.Name            = "Test renader pass";
    RPDesc.AttachmentCount = _countof(Attachments);
    RPDesc.pAttachments    = Attachments;
    RPDesc.SubpassCount    = _countof(Subpasses);
    RPDesc.pSubpasses      = Subpasses;

    RefCntAutoPtr<IRenderPass> pRenderPass;
    pDevice->CreateRenderPass(RPDesc, &pRenderPass);
    ASSERT_NE(pRenderPass, nullptr);

    const auto& RPDesc2 = pRenderPass->GetDesc();
    EXPECT_EQ(RPDesc.AttachmentCount, RPDesc2.AttachmentCount);
    for (Uint32 i = 0; i < std::min(RPDesc.AttachmentCount, RPDesc2.AttachmentCount); ++i)
        EXPECT_EQ(RPDesc.pAttachments[i], RPDesc2.pAttachments[i]);

    EXPECT_EQ(RPDesc.SubpassCount, RPDesc2.SubpassCount);
    for (Uint32 i = 0; i < std::min(RPDesc.SubpassCount, RPDesc2.SubpassCount); ++i)
        EXPECT_EQ(RPDesc.pSubpasses[i], RPDesc2.pSubpasses[i]);

    EXPECT_EQ(RPDesc.DependencyCount, RPDesc2.DependencyCount);
    //for (Uint32 i = 0; i < std::min(RPDesc.DependencyCount, RPDesc2.DependencyCount); ++i)
    //    EXPECT_EQ(RPDesc.pDependencies[i], RPDesc2.pDependencies[i]);

    RefCntAutoPtr<ITexture> pTextures[_countof(Attachments)];
    ITextureView*           pTexViews[_countof(Attachments)] = {};
    for (Uint32 i = 0; i < _countof(pTextures); ++i)
    {
        TextureDesc TexDesc;
        TexDesc.Name        = "Test framebuffer attachment";
        TexDesc.Type        = RESOURCE_DIM_TEX_2D;
        TexDesc.Format      = Attachments[i].Format;
        TexDesc.Width       = 1024;
        TexDesc.Height      = 1024;
        TexDesc.SampleCount = Attachments[i].SampleCount;

        const auto FmtAttribs = pDevice->GetTextureFormatInfo(TexDesc.Format);
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH ||
            FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
            TexDesc.BindFlags = BIND_DEPTH_STENCIL;
        else
            TexDesc.BindFlags = BIND_RENDER_TARGET;

        if (i == 2 || i == 3)
            TexDesc.BindFlags |= BIND_INPUT_ATTACHMENT;

        pDevice->CreateTexture(TexDesc, nullptr, &pTextures[i]);

        if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH ||
            FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
            pTexViews[i] = pTextures[i]->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
        else
            pTexViews[i] = pTextures[i]->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    }

    FramebufferDesc FBDesc;
    FBDesc.Name            = "Test framebuffer";
    FBDesc.pRenderPass     = pRenderPass;
    FBDesc.AttachmentCount = _countof(Attachments);
    FBDesc.ppAttachments   = pTexViews;
    RefCntAutoPtr<IFramebuffer> pFramebuffer;
    pDevice->CreateFramebuffer(FBDesc, &pFramebuffer);
    ASSERT_TRUE(pFramebuffer);

    const auto& FBDesc2 = pFramebuffer->GetDesc();
    EXPECT_EQ(FBDesc2.AttachmentCount, FBDesc.AttachmentCount);
    for (Uint32 i = 0; i < std::min(FBDesc.AttachmentCount, FBDesc2.AttachmentCount); ++i)
        EXPECT_EQ(FBDesc2.ppAttachments[i], FBDesc.ppAttachments[i]);
}

} // namespace
