/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "GPUTestingEnvironment.hpp"
#include "GraphicsAccessories.hpp"
#include "SuperResolutionFactory.h"
#include "SuperResolutionFactoryLoader.h"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

extern "C"
{
    int TestSuperResolutionCInterface(void* pUpscaler);
    int TestSuperResolutionFactoryCInterface(void* pFactory);
}

namespace
{

static ISuperResolutionFactory* GetFactory()
{
    auto*                                         pDevice = GPUTestingEnvironment::GetInstance()->GetDevice();
    static RefCntAutoPtr<ISuperResolutionFactory> pFactory;
    if (!pFactory)
        LoadAndCreateSuperResolutionFactory(pDevice, &pFactory);
    return pFactory;
}

static const SuperResolutionInfo* FindVariantByType(const SuperResolutionInfo* pVariants, Uint32 NumVariants, SUPER_RESOLUTION_TYPE Type)
{
    for (Uint32 VariantIdx = 0; VariantIdx < NumVariants; ++VariantIdx)
    {
        if (pVariants[VariantIdx].Type == Type)
            return &pVariants[VariantIdx];
    }
    return nullptr;
}

TEST(SuperResolutionTest, EnumerateVariants)
{
    auto* pFactory = GetFactory();
    ASSERT_NE(pFactory, nullptr);

    Uint32 NumVariants = 0;
    pFactory->EnumerateVariants(NumVariants, nullptr);
    if (NumVariants == 0)
    {
        GTEST_SKIP() << "No super resolution variants available on this device";
    }

    std::vector<SuperResolutionInfo> Variants(NumVariants);
    pFactory->EnumerateVariants(NumVariants, Variants.data());

    for (Uint32 VariantIdx = 0; VariantIdx < NumVariants; ++VariantIdx)
    {
        EXPECT_NE(Variants[VariantIdx].Name[0], '\0') << "Variant " << VariantIdx << " has empty name";
        EXPECT_NE(Variants[VariantIdx].VariantId, IID_Unknown) << "Variant " << VariantIdx << " has unknown UID";
    }
}

TEST(SuperResolutionTest, QuerySourceSettings)
{
    auto* pFactory = GetFactory();
    ASSERT_NE(pFactory, nullptr);

    Uint32 NumVariants = 0;
    pFactory->EnumerateVariants(NumVariants, nullptr);
    if (NumVariants == 0)
    {
        GTEST_SKIP() << "No super resolution variants available on this device";
    }

    std::vector<SuperResolutionInfo> Variants(NumVariants);
    pFactory->EnumerateVariants(NumVariants, Variants.data());

    for (Uint32 VariantIdx = 0; VariantIdx < NumVariants; ++VariantIdx)
    {
        SuperResolutionSourceSettingsAttribs Attribs;
        Attribs.VariantId        = Variants[VariantIdx].VariantId;
        Attribs.OutputWidth      = 1920;
        Attribs.OutputHeight     = 1080;
        Attribs.OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED;
        Attribs.OutputFormat     = TEX_FORMAT_RGBA16_FLOAT;
        Attribs.Flags            = SUPER_RESOLUTION_FLAG_NONE;

        SuperResolutionSourceSettings Settings;
        pFactory->GetSourceSettings(Attribs, Settings);

        EXPECT_GT(Settings.OptimalInputWidth, 0u) << "Variant " << Variants[VariantIdx].Name;
        EXPECT_GT(Settings.OptimalInputHeight, 0u) << "Variant " << Variants[VariantIdx].Name;
        EXPECT_LE(Settings.OptimalInputWidth, 1920u) << "Variant " << Variants[VariantIdx].Name;
        EXPECT_LE(Settings.OptimalInputHeight, 1080u) << "Variant " << Variants[VariantIdx].Name;
    }

    // Test all optimization types produce monotonically decreasing input resolution
    // (enum is ordered from MAX_QUALITY=0 to MAX_PERFORMANCE)
    {
        const auto& Variant = Variants[0];

        Uint32 PrevWidth = 0;
        for (Uint8 OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_QUALITY; OptimizationType < SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_PERFORMANCE; ++OptimizationType)
        {
            SuperResolutionSourceSettingsAttribs Attribs;
            Attribs.VariantId        = Variant.VariantId;
            Attribs.OutputWidth      = 1920;
            Attribs.OutputHeight     = 1080;
            Attribs.OptimizationType = static_cast<SUPER_RESOLUTION_OPTIMIZATION_TYPE>(OptimizationType);
            Attribs.OutputFormat     = TEX_FORMAT_RGBA16_FLOAT;
            Attribs.Flags            = SUPER_RESOLUTION_FLAG_NONE;

            SuperResolutionSourceSettings Settings;
            pFactory->GetSourceSettings(Attribs, Settings);

            // First iteration: just record. Subsequent: input should decrease or stay same.
            if (PrevWidth > 0)
                EXPECT_LE(Settings.OptimalInputWidth, PrevWidth) << "OptimizationType " << OptimizationType;
            PrevWidth = Settings.OptimalInputWidth;
        }
    }
}

TEST(SuperResolutionTest, CreateTemporalUpscaler)
{
    auto* pFactory = GetFactory();
    ASSERT_NE(pFactory, nullptr);

    Uint32 NumVariants = 0;
    pFactory->EnumerateVariants(NumVariants, nullptr);
    if (NumVariants == 0)
    {
        GTEST_SKIP() << "No super resolution variants available on this device";
    }

    std::vector<SuperResolutionInfo> Variants(NumVariants);
    pFactory->EnumerateVariants(NumVariants, Variants.data());

    const auto* pTemporalInfo = FindVariantByType(Variants.data(), NumVariants, SUPER_RESOLUTION_TYPE_TEMPORAL);
    if (pTemporalInfo == nullptr)
    {
        GTEST_SKIP() << "Temporal super resolution is not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    // Query optimal input resolution
    SuperResolutionSourceSettingsAttribs QueryAttribs;
    QueryAttribs.VariantId        = pTemporalInfo->VariantId;
    QueryAttribs.OutputWidth      = 1920;
    QueryAttribs.OutputHeight     = 1080;
    QueryAttribs.OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED;
    QueryAttribs.OutputFormat     = TEX_FORMAT_RGBA16_FLOAT;
    QueryAttribs.Flags            = SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE;

    SuperResolutionSourceSettings SourceSettings;
    pFactory->GetSourceSettings(QueryAttribs, SourceSettings);
    ASSERT_GT(SourceSettings.OptimalInputWidth, 0u);
    ASSERT_GT(SourceSettings.OptimalInputHeight, 0u);

    SuperResolutionDesc Desc;
    Desc.Name         = "Test Temporal Upscaler";
    Desc.VariantId    = pTemporalInfo->VariantId;
    Desc.OutputWidth  = QueryAttribs.OutputWidth;
    Desc.OutputHeight = QueryAttribs.OutputHeight;
    Desc.OutputFormat = QueryAttribs.OutputFormat;
    Desc.InputWidth   = SourceSettings.OptimalInputWidth;
    Desc.InputHeight  = SourceSettings.OptimalInputHeight;
    Desc.ColorFormat  = TEX_FORMAT_RGBA16_FLOAT;
    Desc.DepthFormat  = TEX_FORMAT_R32_FLOAT;
    Desc.MotionFormat = TEX_FORMAT_RG16_FLOAT;
    Desc.Flags        = QueryAttribs.Flags;

    RefCntAutoPtr<ISuperResolution> pUpscaler;
    pFactory->CreateSuperResolution(Desc, &pUpscaler);
    ASSERT_NE(pUpscaler, nullptr) << "Failed to create temporal super resolution upscaler";

    const auto& RetDesc = pUpscaler->GetDesc();
    EXPECT_EQ(RetDesc.VariantId, pTemporalInfo->VariantId);
    EXPECT_EQ(RetDesc.OutputWidth, 1920u);
    EXPECT_EQ(RetDesc.OutputHeight, 1080u);
    EXPECT_EQ(RetDesc.InputWidth, SourceSettings.OptimalInputWidth);
    EXPECT_EQ(RetDesc.InputHeight, SourceSettings.OptimalInputHeight);

    // Temporal upscaler should return non-trivial jitter pattern (Halton sequence)
    float JitterX = 0.0f, JitterY = 0.0f;
    pUpscaler->GetJitterOffset(0, JitterX, JitterY);
    EXPECT_TRUE(JitterX != 0.0f || JitterY != 0.0f);

    // Verify a few frames produce different jitter values
    float PrevJitterX = JitterX, PrevJitterY = JitterY;
    pUpscaler->GetJitterOffset(1, JitterX, JitterY);
    EXPECT_TRUE(JitterX != PrevJitterX || JitterY != PrevJitterY);
}

TEST(SuperResolutionTest, ExecuteTemporalUpscaler)
{
    auto* pEnv     = GPUTestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pFactory = GetFactory();
    ASSERT_NE(pFactory, nullptr);

    Uint32 NumVariants = 0;
    pFactory->EnumerateVariants(NumVariants, nullptr);
    if (NumVariants == 0)
    {
        GTEST_SKIP() << "No super resolution variants available on this device";
    }

    std::vector<SuperResolutionInfo> Variants(NumVariants);
    pFactory->EnumerateVariants(NumVariants, Variants.data());

    const auto* pTemporalInfo = FindVariantByType(Variants.data(), NumVariants, SUPER_RESOLUTION_TYPE_TEMPORAL);
    if (pTemporalInfo == nullptr)
    {
        GTEST_SKIP() << "Temporal super resolution is not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;


    // Query optimal input resolution
    SuperResolutionSourceSettingsAttribs QueryAttribs{};
    QueryAttribs.VariantId        = pTemporalInfo->VariantId;
    QueryAttribs.OutputWidth      = 1920;
    QueryAttribs.OutputHeight     = 1080;
    QueryAttribs.OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED;
    QueryAttribs.OutputFormat     = TEX_FORMAT_RGBA16_FLOAT;
    QueryAttribs.Flags            = SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE;

    SuperResolutionSourceSettings SourceSettings{};
    pFactory->GetSourceSettings(QueryAttribs, SourceSettings);
    ASSERT_GT(SourceSettings.OptimalInputWidth, 0u);
    ASSERT_GT(SourceSettings.OptimalInputHeight, 0u);

    // Create upscaler
    SuperResolutionDesc UpscalerDesc{};
    UpscalerDesc.Name         = "Test Temporal Execute Upscaler";
    UpscalerDesc.VariantId    = pTemporalInfo->VariantId;
    UpscalerDesc.OutputWidth  = QueryAttribs.OutputWidth;
    UpscalerDesc.OutputHeight = QueryAttribs.OutputHeight;
    UpscalerDesc.OutputFormat = QueryAttribs.OutputFormat;
    UpscalerDesc.InputWidth   = SourceSettings.OptimalInputWidth;
    UpscalerDesc.InputHeight  = SourceSettings.OptimalInputHeight;
    UpscalerDesc.ColorFormat  = TEX_FORMAT_RGBA16_FLOAT;
    UpscalerDesc.DepthFormat  = TEX_FORMAT_R32_FLOAT;
    UpscalerDesc.MotionFormat = TEX_FORMAT_RG16_FLOAT;
    UpscalerDesc.Flags        = QueryAttribs.Flags;

    RefCntAutoPtr<ISuperResolution> pUpscaler;
    pFactory->CreateSuperResolution(UpscalerDesc, &pUpscaler);
    ASSERT_NE(pUpscaler, nullptr);


    // Create input color texture
    TextureDesc ColorTexDesc;
    ColorTexDesc.Name      = "SR Color Input";
    ColorTexDesc.Type      = RESOURCE_DIM_TEX_2D;
    ColorTexDesc.Width     = SourceSettings.OptimalInputWidth;
    ColorTexDesc.Height    = SourceSettings.OptimalInputHeight;
    ColorTexDesc.Format    = TEX_FORMAT_RGBA16_FLOAT;
    ColorTexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    ColorTexDesc.Usage     = USAGE_DEFAULT;

    RefCntAutoPtr<ITexture> pColorTex;
    pDevice->CreateTexture(ColorTexDesc, nullptr, &pColorTex);
    ASSERT_NE(pColorTex, nullptr);

    // Create depth texture
    TextureDesc DepthTexDesc;
    DepthTexDesc.Name      = "SR Depth Input";
    DepthTexDesc.Type      = RESOURCE_DIM_TEX_2D;
    DepthTexDesc.Width     = SourceSettings.OptimalInputWidth;
    DepthTexDesc.Height    = SourceSettings.OptimalInputHeight;
    DepthTexDesc.Format    = TEX_FORMAT_D32_FLOAT;
    DepthTexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
    DepthTexDesc.Usage     = USAGE_DEFAULT;

    RefCntAutoPtr<ITexture> pDepthTex;
    pDevice->CreateTexture(DepthTexDesc, nullptr, &pDepthTex);
    ASSERT_NE(pDepthTex, nullptr);

    // Create motion vectors texture
    TextureDesc MotionTexDesc;
    MotionTexDesc.Name      = "SR Motion Vectors";
    MotionTexDesc.Type      = RESOURCE_DIM_TEX_2D;
    MotionTexDesc.Width     = SourceSettings.OptimalInputWidth;
    MotionTexDesc.Height    = SourceSettings.OptimalInputHeight;
    MotionTexDesc.Format    = TEX_FORMAT_RG16_FLOAT;
    MotionTexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    MotionTexDesc.Usage     = USAGE_DEFAULT;

    RefCntAutoPtr<ITexture> pMotionTex;
    pDevice->CreateTexture(MotionTexDesc, nullptr, &pMotionTex);
    ASSERT_NE(pMotionTex, nullptr);

    // Create output texture
    TextureDesc OutputTexDesc{};
    OutputTexDesc.Name      = "SR Output";
    OutputTexDesc.Type      = RESOURCE_DIM_TEX_2D;
    OutputTexDesc.Width     = QueryAttribs.OutputWidth;
    OutputTexDesc.Height    = QueryAttribs.OutputHeight;
    OutputTexDesc.Format    = TEX_FORMAT_RGBA16_FLOAT;
    OutputTexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    OutputTexDesc.Usage     = USAGE_DEFAULT;

    RefCntAutoPtr<ITexture> pOutputTex;
    pDevice->CreateTexture(OutputTexDesc, nullptr, &pOutputTex);
    ASSERT_NE(pOutputTex, nullptr);

    // Execute temporal upscaling with reset
    auto* pContext = pEnv->GetDeviceContext();

    ExecuteSuperResolutionAttribs Attribs;
    Attribs.pContext           = pContext;
    Attribs.pColorTextureSRV   = pColorTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    Attribs.pDepthTextureSRV   = pDepthTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    Attribs.pMotionVectorsSRV  = pMotionTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    Attribs.pOutputTextureView = pOutputTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    Attribs.JitterX            = 0.0f;
    Attribs.JitterY            = 0.0f;
    Attribs.MotionVectorScaleX = 1.0f;
    Attribs.MotionVectorScaleY = 1.0f;
    Attribs.ExposureScale      = 1.0f;
    Attribs.Sharpness          = 0.5f;
    Attribs.CameraNear         = 0.1f;
    Attribs.CameraFar          = 1000.0f;
    Attribs.CameraFovAngleVert = 1.0472f; // ~60 degrees

    Attribs.TimeDeltaInSeconds = 0.016f;
    Attribs.ResetHistory       = true;
    pUpscaler->Execute(Attribs);

    // Execute a second frame without reset
    Attribs.JitterX      = -0.25f;
    Attribs.JitterY      = 0.25f;
    Attribs.ResetHistory = False;
    pUpscaler->Execute(Attribs);

    pContext->Flush();
    pContext->WaitForIdle();
}

TEST(SuperResolution_CInterface, Factory)
{
    auto* pFactory = GetFactory();
    ASSERT_NE(pFactory, nullptr);
    EXPECT_EQ(TestSuperResolutionFactoryCInterface(pFactory), 0);
}

TEST(SuperResolution_CInterface, SuperResolution)
{
    auto* pFactory = GetFactory();
    ASSERT_NE(pFactory, nullptr);

    Uint32 NumVariants = 0;
    pFactory->EnumerateVariants(NumVariants, nullptr);
    if (NumVariants == 0)
    {
        GTEST_SKIP() << "No super resolution variants available on this device";
    }

    std::vector<SuperResolutionInfo> Variants(NumVariants);
    pFactory->EnumerateVariants(NumVariants, Variants.data());

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    SuperResolutionSourceSettingsAttribs QueryAttribs;
    QueryAttribs.VariantId        = Variants[0].VariantId;
    QueryAttribs.OutputWidth      = 1920;
    QueryAttribs.OutputHeight     = 1080;
    QueryAttribs.OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED;
    QueryAttribs.OutputFormat     = TEX_FORMAT_RGBA16_FLOAT;
    if (Variants[0].Type == SUPER_RESOLUTION_TYPE_TEMPORAL)
        QueryAttribs.Flags = SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE;

    SuperResolutionSourceSettings SourceSettings;
    pFactory->GetSourceSettings(QueryAttribs, SourceSettings);

    SuperResolutionDesc Desc;
    Desc.Name         = "C Interface Test Upscaler";
    Desc.VariantId    = Variants[0].VariantId;
    Desc.OutputWidth  = QueryAttribs.OutputWidth;
    Desc.OutputHeight = QueryAttribs.OutputHeight;
    Desc.OutputFormat = QueryAttribs.OutputFormat;
    Desc.InputWidth   = SourceSettings.OptimalInputWidth;
    Desc.InputHeight  = SourceSettings.OptimalInputHeight;
    Desc.ColorFormat  = TEX_FORMAT_RGBA16_FLOAT;
    if (Variants[0].Type == SUPER_RESOLUTION_TYPE_TEMPORAL)
    {
        Desc.DepthFormat  = TEX_FORMAT_R32_FLOAT;
        Desc.MotionFormat = TEX_FORMAT_RG16_FLOAT;
        Desc.Flags        = SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE;
    }

    RefCntAutoPtr<ISuperResolution> pUpscaler;
    pFactory->CreateSuperResolution(Desc, &pUpscaler);
    ASSERT_NE(pUpscaler, nullptr);

    EXPECT_EQ(TestSuperResolutionCInterface(pUpscaler), 0);
}

} // namespace
