/*
 *  Copyright 2019-2026 Diligent Graphics LLC
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

#include <cstring>
#include <stdexcept>
#include <vector>

#include "DynamicTextureArray.hpp"
#include "GPUTestingEnvironment.hpp"
#include "GraphicsAccessories.hpp"
#include "FastRand.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

class DynamicTextureArrayCreateTest : public testing::TestWithParam<std::tuple<USAGE, TEXTURE_FORMAT>>
{
};

TEST_P(DynamicTextureArrayCreateTest, Run)
{
    auto* pEnv     = GPUTestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    if (pDevice->GetDeviceInfo().IsMetalDevice())
        GTEST_SKIP() << "This test is currently disabled on Metal";

    const auto& TestInfo = GetParam();

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureArrayCreateInfo DynTexArrCI;
    DynTexArrCI.NumSlicesInMemoryPage = 2;

    auto& Desc{DynTexArrCI.Desc};
    Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    Desc.BindFlags = BIND_SHADER_RESOURCE;
    Desc.Width     = 1024;
    Desc.Height    = 1024;
    Desc.MipLevels = 0;
    Desc.Usage     = std::get<0>(TestInfo);
    Desc.Format    = std::get<1>(TestInfo);
    Desc.ArraySize = 0;

    if (Desc.Usage == USAGE_SPARSE)
    {
        const auto& DeviceInfo = pDevice->GetDeviceInfo();
        if (!DeviceInfo.Features.SparseResources)
            GTEST_SKIP() << "Sparse resources are not enabled on this device";

        const auto& AdapterInfo = pDevice->GetAdapterInfo();
        if ((AdapterInfo.SparseResources.CapFlags & SPARSE_RESOURCE_CAP_FLAG_TEXTURE_2D_ARRAY_MIP_TAIL) == 0)
            GTEST_SKIP() << "This device does not support sparse texture 2D arrays with mip tails";
    }

    Desc.Name = "Dynamic texture array create test 1";
    {
        auto pDynTexArray = std::make_unique<DynamicTextureArray>(nullptr, DynTexArrCI);
        ASSERT_NE(pDynTexArray, nullptr);

        EXPECT_FALSE(pDynTexArray->PendingUpdate());
        auto* pTexture = pDynTexArray->Update(nullptr, nullptr);
        EXPECT_EQ(pTexture, nullptr);
        EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    }

    Desc.Name      = "Dynamic texture array create test 2";
    Desc.ArraySize = 1;
    {
        auto pDynTexArray = std::make_unique<DynamicTextureArray>(nullptr, DynTexArrCI);
        ASSERT_NE(pDynTexArray, nullptr);

        EXPECT_TRUE(pDynTexArray->PendingUpdate());
        auto* pTexture = pDynTexArray->Update(pDevice, Desc.Usage == USAGE_SPARSE ? pContext : nullptr);
        EXPECT_NE(pTexture, nullptr);
        EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    }

    Desc.Name = "Dynamic texture array create test 3";
    {
        auto pDynTexArray = std::make_unique<DynamicTextureArray>(pDevice, DynTexArrCI);
        ASSERT_NE(pDynTexArray, nullptr);

        EXPECT_EQ(pDynTexArray->PendingUpdate(), Desc.Usage == USAGE_SPARSE);
        auto* pTexture = pDynTexArray->Update(nullptr, Desc.Usage == USAGE_SPARSE ? pContext : nullptr);
        EXPECT_NE(pTexture, nullptr);
        EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    }
}

std::string GetTestName(const testing::TestParamInfo<std::tuple<USAGE, TEXTURE_FORMAT>>& info)
{
    return std::string{GetUsageString(std::get<0>(info.param))} + "__" + GetTextureFormatAttribs(std::get<1>(info.param)).Name;
}

INSTANTIATE_TEST_SUITE_P(DynamicTextureArray,
                         DynamicTextureArrayCreateTest,
                         testing::Combine(
                             testing::Values<USAGE>(USAGE_DEFAULT, USAGE_SPARSE),
                             testing::Values<TEXTURE_FORMAT>(TEX_FORMAT_RGBA8_UNORM_SRGB, TEX_FORMAT_BC1_UNORM_SRGB)),
                         GetTestName); //

TEST(DynamicTextureArray, RejectsUnsupportedUsage)
{
    DynamicTextureArrayCreateInfo CI;
    CI.Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
    CI.Desc.Name      = "Dynamic Texture Array Invalid Usage Test";
    CI.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags = BIND_SHADER_RESOURCE;
    CI.Desc.Width     = 64;
    CI.Desc.Height    = 64;
    CI.Desc.ArraySize = 1;
    CI.Desc.MipLevels = 1;

    TestingEnvironment::ErrorScope ExpectedErrors{
        "DynamicTextureArray only supports USAGE_DEFAULT and USAGE_SPARSE",
        "DynamicTextureArray only supports USAGE_DEFAULT and USAGE_SPARSE",
        "DynamicTextureArray only supports USAGE_DEFAULT and USAGE_SPARSE",
        "DynamicTextureArray only supports USAGE_DEFAULT and USAGE_SPARSE"};

    for (const USAGE Usage : {USAGE_IMMUTABLE, USAGE_DYNAMIC, USAGE_STAGING, USAGE_UNIFIED})
    {
        CI.Desc.Usage           = Usage;
        auto CreateTextureArray = [&]() {
            DynamicTextureArray TextureArray{nullptr, CI};
        };
        EXPECT_THROW(CreateTextureArray(), std::runtime_error);
    }
}

TEST(DynamicTextureArray, SparseUsageRequiresOneImmediateContext)
{
    DynamicTextureArrayCreateInfo CI;
    CI.Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
    CI.Desc.Name      = "Dynamic Texture Array Invalid Context Mask Test";
    CI.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags = BIND_SHADER_RESOURCE;
    CI.Desc.Width     = 64;
    CI.Desc.Height    = 64;
    CI.Desc.ArraySize = 1;
    CI.Desc.MipLevels = 1;
    CI.Desc.Usage     = USAGE_SPARSE;

    TestingEnvironment::ErrorScope ExpectedErrors{
        "Sparse DynamicTextureArray requires exactly one immediate context",
        "Sparse DynamicTextureArray requires exactly one immediate context"};

    for (const Uint64 ImmediateContextMask : {Uint64{0}, Uint64{3}})
    {
        CI.Desc.ImmediateContextMask = ImmediateContextMask;
        auto CreateTextureArray      = [&]() {
            DynamicTextureArray TextureArray{nullptr, CI};
        };
        EXPECT_THROW(CreateTextureArray(), std::runtime_error);
    }
}

TEST(DynamicTextureArray, TextureSRVsFollowBackingTexture)
{
    auto* const pEnv     = GPUTestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto* const pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    if (pDevice->GetDeviceInfo().Features.TextureSubresourceViews != DEVICE_FEATURE_STATE_ENABLED)
        GTEST_SKIP() << "Typed texture views are not supported by this device.";

    DynamicTextureArrayCreateInfo CI;
    CI.Desc.Format    = TEX_FORMAT_RGBA8_TYPELESS;
    CI.Desc.Name      = "Dynamic Texture Array View Test";
    CI.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags = BIND_SHADER_RESOURCE;
    CI.Desc.Width     = 64;
    CI.Desc.Height    = 64;
    CI.Desc.ArraySize = 1;
    CI.Desc.MipLevels = 1;

    DynamicTextureArray TextureArray{nullptr, CI};
    EXPECT_EQ(TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM), nullptr);

    ITexture* const pInitialTexture = TextureArray.Update(pDevice, nullptr);
    ASSERT_NE(pInitialTexture, nullptr);

    RefCntAutoPtr<ITextureView> pInitialLinearView{TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM)};
    RefCntAutoPtr<ITextureView> pInitialSRGBView{TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM_SRGB)};
    ASSERT_NE(pInitialLinearView, nullptr);
    ASSERT_NE(pInitialSRGBView, nullptr);
    EXPECT_EQ(pInitialLinearView, pInitialTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    EXPECT_NE(pInitialLinearView, pInitialSRGBView);
    EXPECT_EQ(pInitialLinearView->GetDesc().Format, TEX_FORMAT_RGBA8_UNORM);
    EXPECT_EQ(pInitialSRGBView->GetDesc().Format, TEX_FORMAT_RGBA8_UNORM_SRGB);
    EXPECT_EQ(pInitialLinearView->GetTexture(), pInitialTexture);
    EXPECT_EQ(pInitialSRGBView->GetTexture(), pInitialTexture);
    EXPECT_EQ(TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UINT), nullptr);

    // Create the replacement texture, but leave the copy from the stale
    // texture pending until a context is provided.
    ITexture* const pResizedTexture = TextureArray.Resize(pDevice, nullptr, 2);
    ASSERT_NE(pResizedTexture, nullptr);
    EXPECT_NE(pResizedTexture, pInitialTexture);
    EXPECT_TRUE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 1u);

    ITextureView* const pResizedLinearView = TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM);
    ITextureView* const pResizedSRGBView   = TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM_SRGB);
    ASSERT_NE(pResizedLinearView, nullptr);
    ASSERT_NE(pResizedSRGBView, nullptr);
    EXPECT_EQ(pResizedLinearView, pResizedTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    EXPECT_NE(pResizedLinearView, pResizedSRGBView);
    EXPECT_NE(pResizedLinearView, pInitialLinearView);
    EXPECT_NE(pResizedSRGBView, pInitialSRGBView);
    EXPECT_EQ(pResizedLinearView->GetDesc().Format, TEX_FORMAT_RGBA8_UNORM);
    EXPECT_EQ(pResizedSRGBView->GetDesc().Format, TEX_FORMAT_RGBA8_UNORM_SRGB);
    EXPECT_EQ(pResizedLinearView->GetTexture(), pResizedTexture);
    EXPECT_EQ(pResizedSRGBView->GetTexture(), pResizedTexture);

    // Repeating the same pending resize must preserve the replacement texture
    // and its additional SRV while committing the copy.
    EXPECT_EQ(TextureArray.Resize(pDevice, pContext, 2), pResizedTexture);
    EXPECT_FALSE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 2u);
    EXPECT_EQ(TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM), pResizedLinearView);
    EXPECT_EQ(TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM_SRGB), pResizedSRGBView);
}

TEST(DynamicTextureArray, PendingResizeRejectsRetargetingAndCancellation)
{
    auto* const pEnv     = GPUTestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto* const pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureArrayCreateInfo CI;
    CI.Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
    CI.Desc.Name      = "Dynamic Texture Array Pending Resize Test";
    CI.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags = BIND_SHADER_RESOURCE;
    CI.Desc.Width     = 64;
    CI.Desc.Height    = 64;
    CI.Desc.ArraySize = 1;
    CI.Desc.MipLevels = 1;

    DynamicTextureArray TextureArray{pDevice, CI};

    ITexture* const pPendingTexture = TextureArray.Resize(pDevice, nullptr, 2);
    ASSERT_NE(pPendingTexture, nullptr);
    EXPECT_TRUE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 1u);
    EXPECT_EQ(pPendingTexture->GetDesc().ArraySize, 2u);

    // A pending content-preserving resize must be committed before it can be
    // retargeted or cancelled.
    {
        TestingEnvironment::ErrorScope ExpectedErrors{
            "A default texture resize is already pending",
            "A default texture resize is already pending"};
        EXPECT_EQ(TextureArray.Resize(pDevice, nullptr, 3), pPendingTexture);
        EXPECT_EQ(TextureArray.Resize(nullptr, nullptr, 1), pPendingTexture);
    }

    EXPECT_TRUE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 1u);
    EXPECT_EQ(TextureArray.GetTexture(), pPendingTexture);
    EXPECT_EQ(pPendingTexture->GetDesc().ArraySize, 2u);

    EXPECT_EQ(TextureArray.Resize(pDevice, pContext, 2), pPendingTexture);
    EXPECT_FALSE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 2u);
}

TEST(DynamicTextureArray, UnallocatedPendingResizeKeepsCommittedTexture)
{
    auto* const pEnv     = GPUTestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto* const pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureArrayCreateInfo CI;
    CI.Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
    CI.Desc.Name      = "Dynamic Texture Array Deferred Resize Test";
    CI.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags = BIND_SHADER_RESOURCE;
    CI.Desc.Width     = 64;
    CI.Desc.Height    = 64;
    CI.Desc.ArraySize = 1;
    CI.Desc.MipLevels = 1;

    DynamicTextureArray TextureArray{pDevice, CI};

    ITexture* const     pInitialTexture = TextureArray.GetTexture();
    ITextureView* const pInitialSRV     = TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM);
    const Uint32        InitialVersion  = TextureArray.GetVersion();
    ASSERT_NE(pInitialTexture, nullptr);
    ASSERT_NE(pInitialSRV, nullptr);

    // Until a device is provided, the committed texture and its views remain
    // usable and the pending request can be changed or cancelled.
    EXPECT_EQ(TextureArray.Resize(nullptr, nullptr, 2), pInitialTexture);
    EXPECT_TRUE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 1u);
    EXPECT_EQ(TextureArray.GetVersion(), InitialVersion);
    EXPECT_EQ(TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM), pInitialSRV);

    EXPECT_EQ(TextureArray.Resize(nullptr, nullptr, 3), pInitialTexture);
    EXPECT_TRUE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 1u);

    EXPECT_EQ(TextureArray.Resize(nullptr, nullptr, 1), pInitialTexture);
    EXPECT_FALSE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 1u);
    EXPECT_EQ(TextureArray.GetVersion(), InitialVersion);

    TextureArray.Resize(nullptr, nullptr, 2);
    ITexture* const pResizedTexture = TextureArray.Resize(pDevice, pContext, 2);
    ASSERT_NE(pResizedTexture, nullptr);
    EXPECT_NE(pResizedTexture, pInitialTexture);
    EXPECT_FALSE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 2u);
    EXPECT_EQ(TextureArray.GetVersion(), InitialVersion + 1);
}

TEST(DynamicTextureArray, RepeatedPendingResizeCanDiscardContent)
{
    auto* const pDevice = GPUTestingEnvironment::GetInstance()->GetDevice();

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureArrayCreateInfo CI;
    CI.Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
    CI.Desc.Name      = "Dynamic Texture Array Pending Discard Test";
    CI.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags = BIND_SHADER_RESOURCE;
    CI.Desc.Width     = 64;
    CI.Desc.Height    = 64;
    CI.Desc.ArraySize = 1;
    CI.Desc.MipLevels = 1;

    DynamicTextureArray TextureArray{pDevice, CI};

    ITexture* const pPendingTexture = TextureArray.Resize(pDevice, nullptr, 2);
    ASSERT_NE(pPendingTexture, nullptr);
    EXPECT_TRUE(TextureArray.PendingUpdate());

    // Repeating the request with DiscardContent releases the copy source and
    // commits the already allocated replacement without requiring a context.
    EXPECT_EQ(TextureArray.Resize(nullptr, nullptr, 2, true), pPendingTexture);
    EXPECT_FALSE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 2u);
}

TEST(DynamicTextureArray, TypedSRGBTextureReturnsSRGBView)
{
    auto* const pDevice = GPUTestingEnvironment::GetInstance()->GetDevice();

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureArrayCreateInfo CI;
    CI.Desc.Format    = TEX_FORMAT_RGBA8_UNORM_SRGB;
    CI.Desc.Name      = "Dynamic Texture Array SRGB View Test";
    CI.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags = BIND_SHADER_RESOURCE;
    CI.Desc.Width     = 64;
    CI.Desc.Height    = 64;
    CI.Desc.ArraySize = 1;
    CI.Desc.MipLevels = 1;

    DynamicTextureArray TextureArray{pDevice, CI};

    ITextureView* const pSRGBView = TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM_SRGB);
    ASSERT_NE(pSRGBView, nullptr);
    EXPECT_EQ(pSRGBView, TextureArray.GetTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    EXPECT_EQ(pSRGBView->GetDesc().Format, TEX_FORMAT_RGBA8_UNORM_SRGB);
    EXPECT_EQ(TextureArray.GetTextureSRV(TEX_FORMAT_RGBA8_UNORM), nullptr);
}

TEST(DynamicTextureArray, ResizeToZeroBeforeInitialization)
{
    DynamicTextureArrayCreateInfo CI;
    CI.Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
    CI.Desc.Name      = "Uninitialized Dynamic Texture Array Resize Test";
    CI.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags = BIND_SHADER_RESOURCE;
    CI.Desc.Width     = 64;
    CI.Desc.Height    = 64;
    CI.Desc.ArraySize = 1;
    CI.Desc.MipLevels = 1;

    DynamicTextureArray TextureArray{nullptr, CI};
    ASSERT_TRUE(TextureArray.PendingUpdate());

    EXPECT_EQ(TextureArray.Resize(nullptr, nullptr, 0), nullptr);
    EXPECT_FALSE(TextureArray.PendingUpdate());
    EXPECT_EQ(TextureArray.GetArraySize(), 0u);
    EXPECT_EQ(TextureArray.GetTexture(), nullptr);
}

class DynamicTextureArrayResizeTest : public testing::TestWithParam<std::tuple<USAGE, TEXTURE_FORMAT>>
{
};

TEST_P(DynamicTextureArrayResizeTest, Run)
{
    auto* pEnv     = GPUTestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    const auto& DeviceInfo = pDevice->GetDeviceInfo();
    const auto& TestInfo   = GetParam();
    const auto  Usage      = std::get<0>(TestInfo);
    const auto  Format     = std::get<1>(TestInfo);

    if (Usage == USAGE_SPARSE)
    {
        if (!DeviceInfo.Features.SparseResources)
            GTEST_SKIP() << "Sparse resources are not enabled on this device";

        const auto& AdapterInfo = pDevice->GetAdapterInfo();
        if ((AdapterInfo.SparseResources.CapFlags & SPARSE_RESOURCE_CAP_FLAG_TEXTURE_2D_ARRAY_MIP_TAIL) == 0)
            GTEST_SKIP() << "This device does not support sparse texture 2D arrays with mip tails";
    }

    if (DeviceInfo.IsMetalDevice())
        GTEST_SKIP() << "This test is currently disabled on Metal";

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureArrayCreateInfo DynTexArrCI;
    DynTexArrCI.NumSlicesInMemoryPage = 2;

    auto& Desc{DynTexArrCI.Desc};
    Desc.Name      = "Dynamic texture array resize test";
    Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    Desc.BindFlags = BIND_SHADER_RESOURCE;
    Desc.Width     = 1024;
    Desc.Height    = 1024;
    Desc.MipLevels = 11;
    Desc.Usage     = Usage;
    Desc.Format    = Format;
    Desc.ArraySize = 0;

    constexpr Uint32 NumTestSlices = 6;

    RefCntAutoPtr<ITexture> pStagingTex;
    {
        auto StagingTexDesc           = Desc;
        StagingTexDesc.Name           = "Dynamice texture array staging texture";
        StagingTexDesc.BindFlags      = BIND_NONE;
        StagingTexDesc.Usage          = USAGE_STAGING;
        StagingTexDesc.CPUAccessFlags = CPU_ACCESS_READ;
        StagingTexDesc.ArraySize      = NumTestSlices;
        pDevice->CreateTexture(StagingTexDesc, nullptr, &pStagingTex);
        ASSERT_NE(pStagingTex, nullptr);
    }

    FastRandInt rnd{0, 0, 255};

    std::vector<std::vector<Uint8>> RefData(NumTestSlices * Desc.MipLevels);
    for (Uint32 slice = 0; slice < NumTestSlices; ++slice)
    {
        for (Uint32 mip = 0; mip < Desc.MipLevels; ++mip)
        {
            auto&      MipData    = RefData[slice * Desc.MipLevels + mip];
            const auto MipAttribs = GetMipLevelProperties(Desc, mip);
            MipData.resize(static_cast<size_t>(MipAttribs.MipSize));
            for (auto& texel : MipData)
                texel = rnd() & 0xFF;
        }
    }

    auto UpdateSlice = [&RefData, &Desc](IDeviceContext* pCtx, ITexture* pTex, Uint32 Slice) //
    {
        for (Uint32 mip = 0; mip < Desc.MipLevels; ++mip)
        {
            const auto& MipData    = RefData[Slice * Desc.MipLevels + mip];
            const auto  MipAttribs = GetMipLevelProperties(Desc, mip);

            TextureSubResData SubResData{MipData.data(), MipAttribs.RowSize};
            pCtx->UpdateTexture(pTex, mip, Slice, Box{0, MipAttribs.LogicalWidth, 0, MipAttribs.LogicalHeight, 0, 1}, SubResData,
                                RESOURCE_STATE_TRANSITION_MODE_NONE, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    };

    auto VerifySlices = [&RefData, &Desc, &pStagingTex, IsGL = DeviceInfo.IsGLDevice()](IDeviceContext* pCtx, ITexture* pSrcTex, Uint32 FirstSlice, Uint32 NumSlices) //
    {
        const auto FmtAttribs = GetTextureFormatAttribs(Desc.Format);
        if (IsGL && FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
        {
            // Copying to compressed staging textures is not supported in GL
            return;
        }

        for (Uint32 slice = FirstSlice; slice < FirstSlice + NumSlices; ++slice)
        {
            for (Uint32 mip = 0; mip < Desc.MipLevels; ++mip)
            {
                CopyTextureAttribs CopyAttribs{pSrcTex, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, pStagingTex, RESOURCE_STATE_TRANSITION_MODE_TRANSITION};
                CopyAttribs.SrcSlice    = slice;
                CopyAttribs.SrcMipLevel = mip;
                CopyAttribs.DstSlice    = slice;
                CopyAttribs.DstMipLevel = mip;
                pCtx->CopyTexture(CopyAttribs);
            }
        }

        pCtx->WaitForIdle();

        for (Uint32 slice = FirstSlice; slice < FirstSlice + NumSlices; ++slice)
        {
            for (Uint32 mip = 0; mip < Desc.MipLevels; ++mip)
            {
                const auto& RefMipData = RefData[slice * Desc.MipLevels + mip];

                MappedTextureSubresource MappedSubres;
                pCtx->MapTextureSubresource(pStagingTex, mip, slice, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, MappedSubres);

                bool       DataOK     = true;
                const auto MipAttribs = GetMipLevelProperties(Desc, mip);
                for (Uint32 row = 0; row < MipAttribs.StorageHeight / FmtAttribs.BlockHeight; ++row)
                {
                    const auto* pSrcRow = reinterpret_cast<const Uint8*>(MappedSubres.pData) + row * MappedSubres.Stride;
                    const auto* pRefRow = &RefMipData[static_cast<size_t>(row * MipAttribs.RowSize)];

                    if (memcmp(pSrcRow, pRefRow, static_cast<size_t>(MipAttribs.RowSize)) != 0)
                        DataOK = false;
                }
                EXPECT_TRUE(DataOK) << "Slice: " << slice << ", Mip: " << mip;

                pCtx->UnmapTextureSubresource(pStagingTex, mip, slice);
            }
        }
    };

    auto pDynTexArray = std::make_unique<DynamicTextureArray>(pDevice, DynTexArrCI);
    ASSERT_NE(pDynTexArray, nullptr);

    pDynTexArray->Resize(pDevice, nullptr, 1);
    EXPECT_EQ(pDynTexArray->PendingUpdate(), Desc.Usage == USAGE_SPARSE);
    auto* pTexture = pDynTexArray->Update(pDevice, pContext);
    EXPECT_NE(pTexture, nullptr);
    EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    UpdateSlice(pContext, pTexture, 0);
    VerifySlices(pContext, pTexture, 0, 1);

    if (Usage == USAGE_SPARSE)
    {
        ASSERT_EQ(pDynTexArray->GetArraySize(), DynTexArrCI.NumSlicesInMemoryPage);

        // One requested slice still requires the same two-slice resident page,
        // so the resize completes without issuing an empty sparse bind.
        EXPECT_EQ(pDynTexArray->Resize(nullptr, pContext, 1), pTexture);
        EXPECT_FALSE(pDynTexArray->PendingUpdate());
        EXPECT_EQ(pDynTexArray->GetArraySize(), DynTexArrCI.NumSlicesInMemoryPage);
    }

    pDynTexArray->Resize(pDevice, pContext, 2);
    pTexture = pDynTexArray->Update(nullptr, nullptr);
    EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    UpdateSlice(pContext, pTexture, 1);
    VerifySlices(pContext, pTexture, 1, 1);
    const Uint64 MemoryUsageBeforeWorkerResize = pDynTexArray->GetMemoryUsage();
    pDynTexArray->Resize(pDevice, nullptr, 16);
    if (Usage == USAGE_SPARSE)
    {
        // A worker-thread resize grows the sparse memory pool, but leaves tile
        // bindings pending until a device context is provided.
        EXPECT_GT(pDynTexArray->GetMemoryUsage(), MemoryUsageBeforeWorkerResize);
        EXPECT_TRUE(pDynTexArray->PendingUpdate());
    }

    pTexture = pDynTexArray->Update(pDevice, pContext);
    EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    UpdateSlice(pContext, pTexture, 2);
    VerifySlices(pContext, pTexture, 2, 1);

    pDynTexArray->Resize(pDevice, pContext, 9);
    // Resize has already queued the sparse mapping wait, so a committed
    // resize no longer requires a context in Update().
    pTexture = pDynTexArray->Update(nullptr, nullptr);
    EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    UpdateSlice(pContext, pTexture, 3);
    UpdateSlice(pContext, pTexture, 4);
    UpdateSlice(pContext, pTexture, 5);

    VerifySlices(pContext, pTexture, 0, NumTestSlices);

    pDynTexArray->Resize(nullptr, nullptr, 0);
    pTexture = pDynTexArray->Update(nullptr, pContext);
    EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    if (Desc.Usage != USAGE_SPARSE)
        EXPECT_EQ(pTexture, nullptr);
}


TEST(DynamicTextureArray, ResizeDiscardContentDoesNotRequireCopy)
{
    auto* pEnv     = GPUTestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureArrayCreateInfo DynTexArrCI;

    auto& Desc{DynTexArrCI.Desc};
    Desc.Name      = "Dynamic texture array discard resize test";
    Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    Desc.BindFlags = BIND_SHADER_RESOURCE;
    Desc.Width     = 64;
    Desc.Height    = 64;
    Desc.MipLevels = 1;
    Desc.Usage     = USAGE_DEFAULT;
    Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
    Desc.ArraySize = 1;

    auto pDynTexArray = std::make_unique<DynamicTextureArray>(pDevice, DynTexArrCI);
    ASSERT_NE(pDynTexArray, nullptr);
    ASSERT_FALSE(pDynTexArray->PendingUpdate());
    ASSERT_NE(pDynTexArray->GetTexture(), nullptr);
    EXPECT_EQ(pDynTexArray->GetArraySize(), 1u);

    // Discarding content releases the stale source texture. The resize must not
    // try to copy from it even when a context is available.
    auto* pTexture = pDynTexArray->Resize(pDevice, pContext, 2, true);
    ASSERT_NE(pTexture, nullptr);
    EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    EXPECT_FALSE(pDynTexArray->PendingUpdate());
    EXPECT_EQ(pDynTexArray->GetArraySize(), 2u);
    EXPECT_EQ(pTexture->GetDesc().ArraySize, 2u);

    // No copy is required when content is discarded, so a default-texture resize
    // can be committed without a context.
    pTexture = pDynTexArray->Resize(pDevice, nullptr, 3, true);
    ASSERT_NE(pTexture, nullptr);
    EXPECT_EQ(pTexture, pDynTexArray->GetTexture());
    EXPECT_FALSE(pDynTexArray->PendingUpdate());
    EXPECT_EQ(pDynTexArray->GetArraySize(), 3u);
    EXPECT_EQ(pTexture->GetDesc().ArraySize, 3u);
}

TEST(DynamicTextureArray, SparseResizeSupportsNonPowerOfTwoPageSize)
{
    auto* const pEnv     = GPUTestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto* const pContext = pEnv->GetDeviceContext();

    const RenderDeviceInfo& DeviceInfo = pDevice->GetDeviceInfo();
    if (!DeviceInfo.Features.SparseResources)
        GTEST_SKIP() << "Sparse resources are not enabled on this device";

    const GraphicsAdapterInfo& AdapterInfo = pDevice->GetAdapterInfo();
    if ((AdapterInfo.SparseResources.CapFlags & SPARSE_RESOURCE_CAP_FLAG_TEXTURE_2D_ARRAY_MIP_TAIL) == 0)
        GTEST_SKIP() << "This device does not support sparse texture 2D arrays with mip tails";

    if (DeviceInfo.IsMetalDevice())
        GTEST_SKIP() << "This test is currently disabled on Metal";

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureArrayCreateInfo CI;
    CI.NumSlicesInMemoryPage = 3;
    CI.Desc.Name             = "Dynamic texture array non-power-of-two page test";
    CI.Desc.Type             = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags        = BIND_SHADER_RESOURCE;
    CI.Desc.Width            = 1024;
    CI.Desc.Height           = 1024;
    CI.Desc.MipLevels        = 11;
    CI.Desc.Usage            = USAGE_SPARSE;
    CI.Desc.Format           = TEX_FORMAT_RGBA8_UNORM_SRGB;
    CI.Desc.ArraySize        = 0;

    DynamicTextureArray TextureArray{pDevice, CI};

    constexpr Uint32 RequestedSizes[] = {1, 2, 3, 4};
    constexpr Uint32 ExpectedSizes[]  = {3, 3, 3, 6};
    static_assert(_countof(RequestedSizes) == _countof(ExpectedSizes));

    for (size_t i = 0; i < _countof(RequestedSizes); ++i)
    {
        TextureArray.Resize(pDevice, pContext, RequestedSizes[i]);
        EXPECT_FALSE(TextureArray.PendingUpdate());
        EXPECT_EQ(TextureArray.GetArraySize(), ExpectedSizes[i]);
    }
}


INSTANTIATE_TEST_SUITE_P(DynamicTextureArray,
                         DynamicTextureArrayResizeTest,
                         testing::Combine(
                             testing::Values<USAGE>(USAGE_DEFAULT, USAGE_SPARSE),
                             testing::Values<TEXTURE_FORMAT>(TEX_FORMAT_RGBA8_UNORM_SRGB, TEX_FORMAT_BC1_UNORM_SRGB)),
                         GetTestName); //

} // namespace
