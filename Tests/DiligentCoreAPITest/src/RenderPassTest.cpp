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

TEST(RenderPassTest, CreateRenderPassAndFramebuffer)
{
    auto* pDevice  = TestingEnvironment::GetInstance()->GetDevice();
    auto* pContext = TestingEnvironment::GetInstance()->GetDeviceContext();

    RenderPassAttachmentDesc Attachments[6];
    Attachments[0].Format       = TEX_FORMAT_RGBA8_UNORM;
    Attachments[0].SampleCount  = 4;
    Attachments[0].InitialState = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[0].FinalState   = RESOURCE_STATE_RENDER_TARGET;
    Attachments[0].LoadOp       = ATTACHMENT_LOAD_OP_LOAD;
    Attachments[0].StoreOp      = ATTACHMENT_STORE_OP_STORE;

    Attachments[1].Format       = TEX_FORMAT_RGBA8_UNORM;
    Attachments[1].SampleCount  = 4;
    Attachments[1].InitialState = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[1].FinalState   = RESOURCE_STATE_RENDER_TARGET;
    Attachments[1].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].StoreOp      = ATTACHMENT_STORE_OP_DISCARD;

    Attachments[2].Format       = TEX_FORMAT_RGBA8_UNORM;
    Attachments[2].SampleCount  = 1;
    Attachments[2].InitialState = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[2].FinalState   = RESOURCE_STATE_RENDER_TARGET;
    Attachments[2].LoadOp       = ATTACHMENT_LOAD_OP_DISCARD;
    Attachments[2].StoreOp      = ATTACHMENT_STORE_OP_STORE;

    Attachments[3].Format         = TEX_FORMAT_D32_FLOAT_S8X24_UINT;
    Attachments[3].SampleCount    = 4;
    Attachments[3].InitialState   = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[3].FinalState     = RESOURCE_STATE_DEPTH_WRITE;
    Attachments[3].LoadOp         = ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[3].StoreOp        = ATTACHMENT_STORE_OP_DISCARD;
    Attachments[3].StencilLoadOp  = ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[3].StencilStoreOp = ATTACHMENT_STORE_OP_DISCARD;

    Attachments[4].Format       = TEX_FORMAT_RGBA32_FLOAT;
    Attachments[4].SampleCount  = 1;
    Attachments[4].InitialState = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[4].FinalState   = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[4].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[4].StoreOp      = ATTACHMENT_STORE_OP_STORE;

    Attachments[5].Format       = TEX_FORMAT_RGBA8_UNORM;
    Attachments[5].SampleCount  = 1;
    Attachments[5].InitialState = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[5].FinalState   = RESOURCE_STATE_SHADER_RESOURCE;
    Attachments[5].LoadOp       = ATTACHMENT_LOAD_OP_LOAD;
    Attachments[5].StoreOp      = ATTACHMENT_STORE_OP_STORE;

    SubpassDesc Subpasses[2];

    // clang-format off
    AttachmentReference RTAttachmentRefs0[] = 
    {
        {0, RESOURCE_STATE_RENDER_TARGET},
        {1, RESOURCE_STATE_RENDER_TARGET}
    };
    AttachmentReference RslvAttachmentRefs0[] = 
    {
        {ATTACHMENT_UNUSED, RESOURCE_STATE_RENDER_TARGET},
        {2, RESOURCE_STATE_RENDER_TARGET}
    };
    // clang-format on
    AttachmentReference DSAttachmentRef0{3, RESOURCE_STATE_DEPTH_WRITE};
    Subpasses[0].RenderTargetAttachmentCount = _countof(RTAttachmentRefs0);
    Subpasses[0].pRenderTargetAttachments    = RTAttachmentRefs0;
    Subpasses[0].pResolveAttachments         = RslvAttachmentRefs0;
    Subpasses[0].pDepthStencilAttachment     = &DSAttachmentRef0;

    // clang-format off
    AttachmentReference RTAttachmentRefs1[] = 
    {
        {4, RESOURCE_STATE_RENDER_TARGET}
    };
    AttachmentReference InptAttachmentRefs1[] = 
    {
        {2, RESOURCE_STATE_INPUT_ATTACHMENT},
        {5, RESOURCE_STATE_INPUT_ATTACHMENT}
    };
    Uint32 PrsvAttachmentRefs1[] =
    {
        0
    };
    // clang-format on
    Subpasses[1].InputAttachmentCount        = _countof(InptAttachmentRefs1);
    Subpasses[1].pInputAttachments           = InptAttachmentRefs1;
    Subpasses[1].RenderTargetAttachmentCount = _countof(RTAttachmentRefs1);
    Subpasses[1].pRenderTargetAttachments    = RTAttachmentRefs1;
    Subpasses[1].PreserveAttachmentCount     = _countof(PrsvAttachmentRefs1);
    Subpasses[1].pPreserveAttachments        = PrsvAttachmentRefs1;

    SubpassDependencyDesc Dependencies[2] = {};
    Dependencies[0].SrcSubpass            = 0;
    Dependencies[0].DstSubpass            = 1;
    Dependencies[0].SrcStageMask          = PIPELINE_STAGE_FLAG_VERTEX_SHADER;
    Dependencies[0].DstStageMask          = PIPELINE_STAGE_FLAG_PIXEL_SHADER;
    Dependencies[0].SrcAccessMask         = ACCESS_FLAG_SHADER_WRITE;
    Dependencies[0].DstAccessMask         = ACCESS_FLAG_SHADER_READ;

    Dependencies[1].SrcSubpass    = 0;
    Dependencies[1].DstSubpass    = 1;
    Dependencies[1].SrcStageMask  = PIPELINE_STAGE_FLAG_VERTEX_INPUT;
    Dependencies[1].DstStageMask  = PIPELINE_STAGE_FLAG_PIXEL_SHADER;
    Dependencies[1].SrcAccessMask = ACCESS_FLAG_INDEX_READ;
    Dependencies[1].DstAccessMask = ACCESS_FLAG_SHADER_READ;


    RenderPassDesc RPDesc;
    RPDesc.Name            = "Test render pass";
    RPDesc.AttachmentCount = _countof(Attachments);
    RPDesc.pAttachments    = Attachments;
    RPDesc.SubpassCount    = _countof(Subpasses);
    RPDesc.pSubpasses      = Subpasses;
    RPDesc.DependencyCount = _countof(Dependencies);
    RPDesc.pDependencies   = Dependencies;

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
        std::string Name = "Test framebuffer attachment ";
        Name += std::to_string(i);
        TexDesc.Name        = Name.c_str();
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

        if (i == 2 || i == 5)
            TexDesc.BindFlags |= BIND_INPUT_ATTACHMENT;

        const auto InitialState = Attachments[i].InitialState;
        if (InitialState == RESOURCE_STATE_SHADER_RESOURCE)
            TexDesc.BindFlags |= BIND_SHADER_RESOURCE;

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

    BeginRenderPassAttribs RPBeginInfo;
    RPBeginInfo.pRenderPass  = pRenderPass;
    RPBeginInfo.pFramebuffer = pFramebuffer;
    OptimizedClearValue ClearValues[5];
    RPBeginInfo.pClearValues        = ClearValues;
    RPBeginInfo.ClearValueCount     = _countof(ClearValues);
    RPBeginInfo.StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    pContext->BeginRenderPass(RPBeginInfo);
    pContext->NextSubpass();
    pContext->EndRenderPass(true);
}

} // namespace
