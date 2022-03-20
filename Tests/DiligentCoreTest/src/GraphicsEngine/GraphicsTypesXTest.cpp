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

TEST(GraphicsTypesXTest, SubpassDescX)
{
    constexpr AttachmentReference   Inputs[]        = {{2, RESOURCE_STATE_SHADER_RESOURCE}, {4, RESOURCE_STATE_SHADER_RESOURCE}};
    constexpr AttachmentReference   RenderTargets[] = {{1, RESOURCE_STATE_RENDER_TARGET}, {2, RESOURCE_STATE_RENDER_TARGET}};
    constexpr AttachmentReference   Resovles[]      = {{3, RESOURCE_STATE_RESOLVE_DEST}, {4, RESOURCE_STATE_RESOLVE_DEST}};
    constexpr AttachmentReference   DepthStencil    = {5, RESOURCE_STATE_DEPTH_WRITE};
    constexpr Uint32                Preserves[]     = {1, 3, 5};
    constexpr ShadingRateAttachment ShadingRate     = {{6, RESOURCE_STATE_SHADING_RATE}, 128, 256};

    SubpassDesc Ref;
    Ref.InputAttachmentCount        = _countof(Inputs);
    Ref.pInputAttachments           = Inputs;
    Ref.RenderTargetAttachmentCount = _countof(RenderTargets);
    Ref.pRenderTargetAttachments    = RenderTargets;
    Ref.pResolveAttachments         = Resovles;
    Ref.PreserveAttachmentCount     = _countof(Preserves);
    Ref.pPreserveAttachments        = Preserves;
    Ref.pDepthStencilAttachment     = &DepthStencil;
    Ref.pShadingRateAttachment      = &ShadingRate;

    {
        SubpassDescX DescX{Ref};
        EXPECT_TRUE(DescX == Ref);

        SubpassDescX DescX2{DescX};
        EXPECT_TRUE(DescX2 == Ref);
        EXPECT_TRUE(DescX2 == DescX);

        SubpassDescX DescX3;
        EXPECT_TRUE(DescX3 != Ref);
        EXPECT_TRUE(DescX3 != DescX);

        DescX3 = DescX;
        EXPECT_TRUE(DescX3 == Ref);
        EXPECT_TRUE(DescX3 == DescX);

        SubpassDescX DescX4{std::move(DescX3)};
        EXPECT_TRUE(DescX4 == Ref);
        EXPECT_TRUE(DescX4 == DescX);

        DescX3 = std::move(DescX4);
        EXPECT_TRUE(DescX3 == Ref);
        EXPECT_TRUE(DescX3 == DescX);

        SubpassDescX DescX5;
        DescX5 = DescX;
        EXPECT_TRUE(DescX5 == Ref);
        EXPECT_TRUE(DescX5 == DescX);

        DescX5.Reset();
        EXPECT_TRUE(DescX5 == SubpassDesc{});
    }

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
        EXPECT_TRUE(DescX == Ref);

        DescX.ClearRenderTargets();
        Ref.RenderTargetAttachmentCount = 0;
        Ref.pRenderTargetAttachments    = nullptr;
        Ref.pResolveAttachments         = nullptr;
        EXPECT_TRUE(DescX == Ref);

        Ref.RenderTargetAttachmentCount = _countof(RenderTargets);
        Ref.pRenderTargetAttachments    = RenderTargets;
        DescX
            .AddRenderTarget(RenderTargets[0])
            .AddRenderTarget(RenderTargets[1]);
        EXPECT_TRUE(DescX == Ref);

        constexpr AttachmentReference Resovles2[] = {{ATTACHMENT_UNUSED, RESOURCE_STATE_UNKNOWN}, {4, RESOURCE_STATE_RESOLVE_DEST}};
        Ref.pResolveAttachments                   = Resovles2;
        DescX.ClearRenderTargets();
        DescX
            .AddRenderTarget(RenderTargets[0])
            .AddRenderTarget(RenderTargets[1], &Resovles2[1]);
        EXPECT_TRUE(DescX == Ref);

        DescX.ClearInputs();
        Ref.InputAttachmentCount = 0;
        Ref.pInputAttachments    = nullptr;
        EXPECT_TRUE(DescX == Ref);

        DescX.ClearPreserves();
        Ref.PreserveAttachmentCount = 0;
        Ref.pPreserveAttachments    = nullptr;
        EXPECT_TRUE(DescX == Ref);

        DescX.SetDepthStencil(nullptr);
        Ref.pDepthStencilAttachment = nullptr;
        EXPECT_TRUE(DescX == Ref);

        DescX.SetShadingRate(nullptr);
        Ref.pShadingRateAttachment = nullptr;
        EXPECT_TRUE(DescX == Ref);
    }
}

} // namespace
