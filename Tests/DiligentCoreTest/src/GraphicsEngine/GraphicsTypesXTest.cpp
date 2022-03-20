/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include "GraphicsTypesX.hpp"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

template <typename TypeX, typename Type>
void TestCtorsAndAssignments(const Type& Ref)
{
    TypeX DescX{Ref};
    EXPECT_TRUE(DescX == Ref);

    TypeX DescX2{DescX};
    EXPECT_TRUE(DescX2 == Ref);
    EXPECT_TRUE(DescX2 == DescX);

    TypeX DescX3;
    EXPECT_TRUE(DescX3 != Ref);
    EXPECT_TRUE(DescX3 != DescX);

    DescX3 = DescX;
    EXPECT_TRUE(DescX3 == Ref);
    EXPECT_TRUE(DescX3 == DescX);

    TypeX DescX4{std::move(DescX3)};
    EXPECT_TRUE(DescX4 == Ref);
    EXPECT_TRUE(DescX4 == DescX);

    DescX3 = std::move(DescX4);
    EXPECT_TRUE(DescX3 == Ref);
    EXPECT_TRUE(DescX3 == DescX);

    TypeX DescX5;
    DescX5 = DescX;
    EXPECT_TRUE(DescX5 == Ref);
    EXPECT_TRUE(DescX5 == DescX);

    DescX5.Clear();
    EXPECT_TRUE(DescX5 == Type{});
}

TEST(GraphicsTypesXTest, SubpassDescX)
{
    constexpr AttachmentReference   Inputs[]        = {{2, RESOURCE_STATE_SHADER_RESOURCE}, {4, RESOURCE_STATE_SHADER_RESOURCE}};
    constexpr AttachmentReference   RenderTargets[] = {{1, RESOURCE_STATE_RENDER_TARGET}, {2, RESOURCE_STATE_RENDER_TARGET}};
    constexpr AttachmentReference   Resovles[]      = {{3, RESOURCE_STATE_RESOLVE_DEST}, {4, RESOURCE_STATE_RESOLVE_DEST}};
    constexpr AttachmentReference   DepthStencil    = {5, RESOURCE_STATE_DEPTH_WRITE};
    constexpr Uint32                Preserves[]     = {1, 3, 5};
    constexpr ShadingRateAttachment ShadingRate     = {{6, RESOURCE_STATE_SHADING_RATE}, 128, 256};

    SubpassDesc Ref;
    Ref.InputAttachmentCount = _countof(Inputs);
    Ref.pInputAttachments    = Inputs;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    Ref.RenderTargetAttachmentCount = _countof(RenderTargets);
    Ref.pRenderTargetAttachments    = RenderTargets;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    Ref.pResolveAttachments = Resovles;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    Ref.PreserveAttachmentCount = _countof(Preserves);
    Ref.pPreserveAttachments    = Preserves;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    Ref.pDepthStencilAttachment = &DepthStencil;
    Ref.pShadingRateAttachment  = &ShadingRate;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    {
        SubpassDescX DescX;
        DescX
            .AddInput(Inputs[0])
            .AddInput(Inputs[1])
            .AddRenderTarget(RenderTargets[0], &Resovles[0])
            .AddRenderTarget(RenderTargets[1], &Resovles[1])
            .SetDepthStencil(&DepthStencil)
            .SetShadingRate(&ShadingRate)
            .AddPreserve(Preserves[0])
            .AddPreserve(Preserves[1])
            .AddPreserve(Preserves[2]);
        EXPECT_EQ(DescX, Ref);

        DescX.ClearRenderTargets();
        Ref.RenderTargetAttachmentCount = 0;
        Ref.pRenderTargetAttachments    = nullptr;
        Ref.pResolveAttachments         = nullptr;
        EXPECT_EQ(DescX, Ref);

        Ref.RenderTargetAttachmentCount = _countof(RenderTargets);
        Ref.pRenderTargetAttachments    = RenderTargets;
        DescX
            .AddRenderTarget(RenderTargets[0])
            .AddRenderTarget(RenderTargets[1]);
        EXPECT_EQ(DescX, Ref);

        constexpr AttachmentReference Resovles2[] = {{ATTACHMENT_UNUSED, RESOURCE_STATE_UNKNOWN}, {4, RESOURCE_STATE_RESOLVE_DEST}};
        Ref.pResolveAttachments                   = Resovles2;
        DescX.ClearRenderTargets();
        DescX
            .AddRenderTarget(RenderTargets[0])
            .AddRenderTarget(RenderTargets[1], &Resovles2[1]);
        EXPECT_EQ(DescX, Ref);

        DescX.ClearInputs();
        Ref.InputAttachmentCount = 0;
        Ref.pInputAttachments    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearPreserves();
        Ref.PreserveAttachmentCount = 0;
        Ref.pPreserveAttachments    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.SetDepthStencil(nullptr);
        Ref.pDepthStencilAttachment = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.SetShadingRate(nullptr);
        Ref.pShadingRateAttachment = nullptr;
        EXPECT_EQ(DescX, Ref);
    }
}

TEST(GraphicsTypesXTest, RenderPassDescX)
{
    constexpr RenderPassAttachmentDesc Attachments[] =
        {
            {TEX_FORMAT_RGBA8_UNORM_SRGB, 2},
            {TEX_FORMAT_RGBA32_FLOAT},
            {TEX_FORMAT_R16_UINT},
            {TEX_FORMAT_D32_FLOAT},
        };

    RenderPassDesc Ref;
    Ref.AttachmentCount = _countof(Attachments);
    Ref.pAttachments    = Attachments;
    TestCtorsAndAssignments<RenderPassDescX>(Ref);

    SubpassDescX Subpass0, Subpass1;
    Subpass0
        .AddInput({1, RESOURCE_STATE_SHADER_RESOURCE})
        .AddRenderTarget({2, RESOURCE_STATE_RENDER_TARGET})
        .AddRenderTarget({3, RESOURCE_STATE_RENDER_TARGET})
        .SetDepthStencil({4, RESOURCE_STATE_DEPTH_WRITE});
    Subpass1
        .AddPreserve(5)
        .AddPreserve(6)
        .AddRenderTarget({7, RESOURCE_STATE_RENDER_TARGET})
        .SetShadingRate({{6, RESOURCE_STATE_SHADING_RATE}, 128, 256});

    SubpassDesc Subpasses[] = {Subpass0, Subpass1};
    Ref.SubpassCount        = _countof(Subpasses);
    Ref.pSubpasses          = Subpasses;
    TestCtorsAndAssignments<RenderPassDescX>(Ref);

    constexpr SubpassDependencyDesc Dependecies[] =
        {
            {0, 1, PIPELINE_STAGE_FLAG_DRAW_INDIRECT, PIPELINE_STAGE_FLAG_VERTEX_INPUT, ACCESS_FLAG_INDIRECT_COMMAND_READ, ACCESS_FLAG_INDEX_READ},
            {2, 3, PIPELINE_STAGE_FLAG_VERTEX_SHADER, PIPELINE_STAGE_FLAG_HULL_SHADER, ACCESS_FLAG_VERTEX_READ, ACCESS_FLAG_UNIFORM_READ},
            {4, 5, PIPELINE_STAGE_FLAG_DOMAIN_SHADER, PIPELINE_STAGE_FLAG_GEOMETRY_SHADER, ACCESS_FLAG_SHADER_READ, ACCESS_FLAG_SHADER_WRITE},
        };
    Ref.DependencyCount = _countof(Dependecies);
    Ref.pDependencies   = Dependecies;
    TestCtorsAndAssignments<RenderPassDescX>(Ref);

    {
        RenderPassDescX DescX;
        DescX
            .AddAttachment(Attachments[0])
            .AddAttachment(Attachments[1])
            .AddAttachment(Attachments[2])
            .AddAttachment(Attachments[3])
            .AddSubpass(Subpass0)
            .AddSubpass(Subpass1)
            .AddDependency(Dependecies[0])
            .AddDependency(Dependecies[1])
            .AddDependency(Dependecies[2]);
        EXPECT_EQ(DescX, Ref);

        DescX.ClearAttachments();
        Ref.AttachmentCount = 0;
        Ref.pAttachments    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearSubpasses();
        Ref.SubpassCount = 0;
        Ref.pSubpasses   = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearDependencies();
        Ref.DependencyCount = 0;
        Ref.pDependencies   = nullptr;
        EXPECT_EQ(DescX, Ref);
    }
}


TEST(GraphicsTypesXTest, InputLayoutDescX)
{
    constexpr LayoutElement Elements[] =
        {
            {0, 0, 2, VT_FLOAT32},
            {1, 0, 2, VT_FLOAT32},
            {2, 0, 4, VT_UINT8, True},
        };

    InputLayoutDesc Ref;
    Ref.NumElements    = _countof(Elements);
    Ref.LayoutElements = Elements;
    TestCtorsAndAssignments<InputLayoutDescX>(Ref);

    {
        InputLayoutDescX DescX;
        DescX
            .Add({0, 0, 2, VT_FLOAT32})
            .Add(1, 0, 2, VT_FLOAT32)
            .Add(2, 0, 4, VT_UINT8, True);
        EXPECT_EQ(DescX, Ref);

        DescX.Clear();
        EXPECT_EQ(DescX, InputLayoutDesc{});
    }

    {
        InputLayoutDescX DescX{
            {
                {0, 0, 2, VT_FLOAT32},
                {1, 0, 2, VT_FLOAT32},
                {2, 0, 4, VT_UINT8, True},
            } //
        };
        EXPECT_EQ(DescX, Ref);
    }
}


TEST(GraphicsTypesXTest, FramebufferDescX)
{
    ITextureView* ppAttachments[] = {
        reinterpret_cast<ITextureView*>(uintptr_t{0x1}),
        reinterpret_cast<ITextureView*>(uintptr_t{0x2}),
        reinterpret_cast<ITextureView*>(uintptr_t{0x3}),
    };
    FramebufferDesc Ref;
    Ref.Name            = "Test";
    Ref.pRenderPass     = reinterpret_cast<IRenderPass*>(uintptr_t{0xA});
    Ref.AttachmentCount = _countof(ppAttachments);
    Ref.ppAttachments   = ppAttachments;
    Ref.Width           = 256;
    Ref.Height          = 128;
    Ref.NumArraySlices  = 6;
    TestCtorsAndAssignments<FramebufferDescX>(Ref);

    {
        FramebufferDescX DescX;
        DescX.SetName("Test");
        DescX.pRenderPass    = reinterpret_cast<IRenderPass*>(uintptr_t{0xA});
        DescX.Width          = 256;
        DescX.Height         = 128;
        DescX.NumArraySlices = 6;
        DescX.AddAttachment(ppAttachments[0]);
        DescX.AddAttachment(ppAttachments[1]);
        DescX.AddAttachment(ppAttachments[2]);
        EXPECT_EQ(DescX, Ref);

        DescX.ClearAttachments();
        Ref.AttachmentCount = 0;
        Ref.ppAttachments   = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.Clear();
        EXPECT_EQ(DescX, FramebufferDesc{});
    }
}

} // namespace
