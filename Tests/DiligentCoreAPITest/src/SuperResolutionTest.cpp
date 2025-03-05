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

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static const SuperResolutionInfo* FindUpscalerByType(const SuperResolutionProperties& SRProps, SUPER_RESOLUTION_UPSCALER_TYPE Type)
{
    for (Uint8 i = 0; i < SRProps.NumUpscalers; ++i)
    {
        if (SRProps.Upscalers[i].Type == Type)
            return &SRProps.Upscalers[i];
    }
    return nullptr;
}

TEST(SuperResolutionTest, CheckProperties)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.SuperResolution)
    {
        GTEST_SKIP() << "Super resolution is not supported by this device";
    }

    const auto& SRProps = pDevice->GetAdapterInfo().SuperResolution;
    EXPECT_GT(SRProps.NumUpscalers, static_cast<Uint8>(0));

    for (Uint8 i = 0; i < SRProps.NumUpscalers; ++i)
    {
        EXPECT_NE(SRProps.Upscalers[i].Name[0], '\0') << "Upscaler " << i << " has empty name";
        EXPECT_NE(SRProps.Upscalers[i].VariantId, IID_Unknown) << "Upscaler " << i << " has unknown UID";
    }
}

TEST(SuperResolutionTest, QuerySourceSettings)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.SuperResolution)
    {
        GTEST_SKIP() << "Super resolution is not supported by this device";
    }

    const auto& SRProps = pDevice->GetAdapterInfo().SuperResolution;
    ASSERT_GT(SRProps.NumUpscalers, static_cast<Uint8>(0));

    for (Uint8 i = 0; i < SRProps.NumUpscalers; ++i)
    {
        SuperResolutionSourceSettingsAttribs Attribs;
        Attribs.VariantId        = SRProps.Upscalers[i].VariantId;
        Attribs.OutputWidth      = 1920;
        Attribs.OutputHeight     = 1080;
        Attribs.OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED;

        SuperResolutionSourceSettings Settings;
        pDevice->GetSuperResolutionSourceSettings(Attribs, Settings);

        EXPECT_GT(Settings.OptimalInputWidth, 0u) << "Upscaler " << SRProps.Upscalers[i].Name;
        EXPECT_GT(Settings.OptimalInputHeight, 0u) << "Upscaler " << SRProps.Upscalers[i].Name;
        EXPECT_LE(Settings.OptimalInputWidth, 1920u) << "Upscaler " << SRProps.Upscalers[i].Name;
        EXPECT_LE(Settings.OptimalInputHeight, 1080u) << "Upscaler " << SRProps.Upscalers[i].Name;
    }

    // Test all optimization types produce monotonically decreasing input resolution
    // (enum is ordered from MAX_QUALITY=0 to MAX_PERFORMANCE)
    {
        const auto& Upscaler = SRProps.Upscalers[0];

        Uint32 PrevWidth = 0;
        for (int opt = static_cast<int>(SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_QUALITY);
             opt <= static_cast<int>(SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_PERFORMANCE); ++opt)
        {
            SuperResolutionSourceSettingsAttribs Attribs;
            Attribs.VariantId        = Upscaler.VariantId;
            Attribs.OutputWidth      = 1920;
            Attribs.OutputHeight     = 1080;
            Attribs.OptimizationType = static_cast<SUPER_RESOLUTION_OPTIMIZATION_TYPE>(opt);

            SuperResolutionSourceSettings Settings;
            pDevice->GetSuperResolutionSourceSettings(Attribs, Settings);

            // First iteration: just record. Subsequent: input should decrease or stay same.
            if (PrevWidth > 0)
                EXPECT_LE(Settings.OptimalInputWidth, PrevWidth) << "OptimizationType " << opt;
            PrevWidth = Settings.OptimalInputWidth;
        }
    }
}

TEST(SuperResolutionTest, CreateSpatialUpscaler)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.SuperResolution)
    {
        GTEST_SKIP() << "Super resolution is not supported by this device";
    }

    const auto& SRProps      = pDevice->GetAdapterInfo().SuperResolution;
    const auto* pSpatialInfo = FindUpscalerByType(SRProps, SUPER_RESOLUTION_UPSCALER_TYPE_SPATIAL);
    if (pSpatialInfo == nullptr)
    {
        GTEST_SKIP() << "Spatial super resolution is not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    // Query optimal input resolution
    SuperResolutionSourceSettingsAttribs QueryAttribs;
    QueryAttribs.VariantId        = pSpatialInfo->VariantId;
    QueryAttribs.OutputWidth      = 1920;
    QueryAttribs.OutputHeight     = 1080;
    QueryAttribs.OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED;

    SuperResolutionSourceSettings SourceSettings;
    pDevice->GetSuperResolutionSourceSettings(QueryAttribs, SourceSettings);
    ASSERT_GT(SourceSettings.OptimalInputWidth, 0u);
    ASSERT_GT(SourceSettings.OptimalInputHeight, 0u);

    SuperResolutionDesc Desc;
    Desc.Name         = "Test Spatial Upscaler";
    Desc.VariantId    = pSpatialInfo->VariantId;
    Desc.OutputWidth  = 1920;
    Desc.OutputHeight = 1080;
    Desc.OutputFormat = TEX_FORMAT_RGBA16_FLOAT;
    Desc.ColorFormat  = TEX_FORMAT_RGBA16_FLOAT;
    Desc.InputWidth   = SourceSettings.OptimalInputWidth;
    Desc.InputHeight  = SourceSettings.OptimalInputHeight;

    RefCntAutoPtr<ISuperResolution> pUpscaler;
    pDevice->CreateSuperResolution(Desc, &pUpscaler);
    ASSERT_NE(pUpscaler, nullptr) << "Failed to create spatial super resolution upscaler";

    const auto& RetDesc = pUpscaler->GetDesc();
    EXPECT_EQ(RetDesc.VariantId, pSpatialInfo->VariantId);
    EXPECT_EQ(RetDesc.OutputWidth, 1920u);
    EXPECT_EQ(RetDesc.OutputHeight, 1080u);
    EXPECT_EQ(RetDesc.InputWidth, SourceSettings.OptimalInputWidth);
    EXPECT_EQ(RetDesc.InputHeight, SourceSettings.OptimalInputHeight);

    // Spatial upscaler should return zero jitter
    float JitterX = 1.0f, JitterY = 1.0f;
    pUpscaler->GetOptimalJitterPattern(0, &JitterX, &JitterY);
    EXPECT_EQ(JitterX, 0.0f);
    EXPECT_EQ(JitterY, 0.0f);

    pUpscaler->GetOptimalJitterPattern(1, &JitterX, &JitterY);
    EXPECT_EQ(JitterX, 0.0f);
    EXPECT_EQ(JitterY, 0.0f);
}

TEST(SuperResolutionTest, CreateTemporalUpscaler)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.SuperResolution)
    {
        GTEST_SKIP() << "Super resolution is not supported by this device";
    }

    const auto& SRProps       = pDevice->GetAdapterInfo().SuperResolution;
    const auto* pTemporalInfo = FindUpscalerByType(SRProps, SUPER_RESOLUTION_UPSCALER_TYPE_TEMPORAL);
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

    SuperResolutionSourceSettings SourceSettings;
    pDevice->GetSuperResolutionSourceSettings(QueryAttribs, SourceSettings);
    ASSERT_GT(SourceSettings.OptimalInputWidth, 0u);
    ASSERT_GT(SourceSettings.OptimalInputHeight, 0u);

    SuperResolutionDesc Desc;
    Desc.Name         = "Test Temporal Upscaler";
    Desc.VariantId    = pTemporalInfo->VariantId;
    Desc.OutputWidth  = 1920;
    Desc.OutputHeight = 1080;
    Desc.OutputFormat = TEX_FORMAT_RGBA16_FLOAT;
    Desc.ColorFormat  = TEX_FORMAT_RGBA16_FLOAT;
    Desc.DepthFormat  = TEX_FORMAT_D32_FLOAT;
    Desc.MotionFormat = TEX_FORMAT_RG16_FLOAT;
    Desc.InputWidth   = SourceSettings.OptimalInputWidth;
    Desc.InputHeight  = SourceSettings.OptimalInputHeight;

    RefCntAutoPtr<ISuperResolution> pUpscaler;
    pDevice->CreateSuperResolution(Desc, &pUpscaler);
    ASSERT_NE(pUpscaler, nullptr) << "Failed to create temporal super resolution upscaler";

    const auto& RetDesc = pUpscaler->GetDesc();
    EXPECT_EQ(RetDesc.VariantId, pTemporalInfo->VariantId);
    EXPECT_EQ(RetDesc.OutputWidth, 1920u);
    EXPECT_EQ(RetDesc.OutputHeight, 1080u);
    EXPECT_EQ(RetDesc.InputWidth, SourceSettings.OptimalInputWidth);
    EXPECT_EQ(RetDesc.InputHeight, SourceSettings.OptimalInputHeight);

    // Temporal upscaler should return non-trivial jitter pattern (Halton sequence)
    float JitterX = 0.0f, JitterY = 0.0f;
    pUpscaler->GetOptimalJitterPattern(0, &JitterX, &JitterY);
    EXPECT_TRUE(JitterX != 0.0f || JitterY != 0.0f);

    // Verify a few frames produce different jitter values
    float PrevJitterX = JitterX, PrevJitterY = JitterY;
    pUpscaler->GetOptimalJitterPattern(1, &JitterX, &JitterY);
    EXPECT_TRUE(JitterX != PrevJitterX || JitterY != PrevJitterY);
}

TEST(SuperResolutionTest, ExecuteSpatialUpscaler)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.SuperResolution)
    {
        GTEST_SKIP() << "Super resolution is not supported by this device";
    }

    const auto& SRProps      = pDevice->GetAdapterInfo().SuperResolution;
    const auto* pSpatialInfo = FindUpscalerByType(SRProps, SUPER_RESOLUTION_UPSCALER_TYPE_SPATIAL);
    if (pSpatialInfo == nullptr)
    {
        GTEST_SKIP() << "Spatial super resolution is not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    constexpr Uint32 OutputWidth  = 256;
    constexpr Uint32 OutputHeight = 256;

    // Query optimal input resolution
    SuperResolutionSourceSettingsAttribs QueryAttribs;
    QueryAttribs.VariantId        = pSpatialInfo->VariantId;
    QueryAttribs.OutputWidth      = OutputWidth;
    QueryAttribs.OutputHeight     = OutputHeight;
    QueryAttribs.OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED;

    SuperResolutionSourceSettings SourceSettings;
    pDevice->GetSuperResolutionSourceSettings(QueryAttribs, SourceSettings);
    ASSERT_GT(SourceSettings.OptimalInputWidth, 0u);
    ASSERT_GT(SourceSettings.OptimalInputHeight, 0u);

    // Create upscaler
    SuperResolutionDesc UpscalerDesc;
    UpscalerDesc.Name         = "Test Spatial Execute Upscaler";
    UpscalerDesc.VariantId    = pSpatialInfo->VariantId;
    UpscalerDesc.OutputWidth  = OutputWidth;
    UpscalerDesc.OutputHeight = OutputHeight;
    UpscalerDesc.OutputFormat = TEX_FORMAT_RGBA16_FLOAT;
    UpscalerDesc.ColorFormat  = TEX_FORMAT_RGBA16_FLOAT;
    UpscalerDesc.InputWidth   = SourceSettings.OptimalInputWidth;
    UpscalerDesc.InputHeight  = SourceSettings.OptimalInputHeight;

    RefCntAutoPtr<ISuperResolution> pUpscaler;
    pDevice->CreateSuperResolution(UpscalerDesc, &pUpscaler);
    ASSERT_NE(pUpscaler, nullptr);

    const Uint32 RenderWidth  = SourceSettings.OptimalInputWidth;
    const Uint32 RenderHeight = SourceSettings.OptimalInputHeight;

    // Create input color texture
    TextureDesc ColorTexDesc;
    ColorTexDesc.Name      = "SR Color Input";
    ColorTexDesc.Type      = RESOURCE_DIM_TEX_2D;
    ColorTexDesc.Width     = RenderWidth;
    ColorTexDesc.Height    = RenderHeight;
    ColorTexDesc.Format    = TEX_FORMAT_RGBA16_FLOAT;
    ColorTexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    ColorTexDesc.Usage     = USAGE_DEFAULT;

    RefCntAutoPtr<ITexture> pColorTex;
    pDevice->CreateTexture(ColorTexDesc, nullptr, &pColorTex);
    ASSERT_NE(pColorTex, nullptr);

    // Create output texture
    TextureDesc OutputTexDesc;
    OutputTexDesc.Name      = "SR Output";
    OutputTexDesc.Type      = RESOURCE_DIM_TEX_2D;
    OutputTexDesc.Width     = OutputWidth;
    OutputTexDesc.Height    = OutputHeight;
    OutputTexDesc.Format    = TEX_FORMAT_RGBA16_FLOAT;
    OutputTexDesc.BindFlags = BIND_RENDER_TARGET | BIND_UNORDERED_ACCESS;
    OutputTexDesc.Usage     = USAGE_DEFAULT;

    RefCntAutoPtr<ITexture> pOutputTex;
    pDevice->CreateTexture(OutputTexDesc, nullptr, &pOutputTex);
    ASSERT_NE(pOutputTex, nullptr);

    // Execute spatial upscaling
    auto* pContext = pEnv->GetDeviceContext();

    ExecuteSuperResolutionAttribs Attribs;
    Attribs.pColorTextureSRV  = pColorTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    Attribs.pOutputTextureView = pOutputTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

    pContext->ExecuteSuperResolution(Attribs, pUpscaler);
    pContext->Flush();
    pContext->WaitForIdle();
}

TEST(SuperResolutionTest, ExecuteTemporalUpscaler)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.SuperResolution)
    {
        GTEST_SKIP() << "Super resolution is not supported by this device";
    }

    const auto& SRProps       = pDevice->GetAdapterInfo().SuperResolution;
    const auto* pTemporalInfo = FindUpscalerByType(SRProps, SUPER_RESOLUTION_UPSCALER_TYPE_TEMPORAL);
    if (pTemporalInfo == nullptr)
    {
        GTEST_SKIP() << "Temporal super resolution is not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    constexpr Uint32 OutputWidth  = 256;
    constexpr Uint32 OutputHeight = 256;

    // Query optimal input resolution
    SuperResolutionSourceSettingsAttribs QueryAttribs;
    QueryAttribs.VariantId        = pTemporalInfo->VariantId;
    QueryAttribs.OutputWidth      = OutputWidth;
    QueryAttribs.OutputHeight     = OutputHeight;
    QueryAttribs.OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED;

    SuperResolutionSourceSettings SourceSettings;
    pDevice->GetSuperResolutionSourceSettings(QueryAttribs, SourceSettings);
    ASSERT_GT(SourceSettings.OptimalInputWidth, 0u);
    ASSERT_GT(SourceSettings.OptimalInputHeight, 0u);

    // Create upscaler
    SuperResolutionDesc UpscalerDesc;
    UpscalerDesc.Name         = "Test Temporal Execute Upscaler";
    UpscalerDesc.VariantId    = pTemporalInfo->VariantId;
    UpscalerDesc.OutputWidth  = OutputWidth;
    UpscalerDesc.OutputHeight = OutputHeight;
    UpscalerDesc.OutputFormat = TEX_FORMAT_RGBA16_FLOAT;
    UpscalerDesc.ColorFormat  = TEX_FORMAT_RGBA16_FLOAT;
    UpscalerDesc.DepthFormat  = TEX_FORMAT_D32_FLOAT;
    UpscalerDesc.MotionFormat = TEX_FORMAT_RG16_FLOAT;
    UpscalerDesc.InputWidth   = SourceSettings.OptimalInputWidth;
    UpscalerDesc.InputHeight  = SourceSettings.OptimalInputHeight;

    RefCntAutoPtr<ISuperResolution> pUpscaler;
    pDevice->CreateSuperResolution(UpscalerDesc, &pUpscaler);
    ASSERT_NE(pUpscaler, nullptr);

    const Uint32 RenderWidth  = SourceSettings.OptimalInputWidth;
    const Uint32 RenderHeight = SourceSettings.OptimalInputHeight;

    // Create input color texture
    TextureDesc ColorTexDesc;
    ColorTexDesc.Name      = "SR Color Input";
    ColorTexDesc.Type      = RESOURCE_DIM_TEX_2D;
    ColorTexDesc.Width     = RenderWidth;
    ColorTexDesc.Height    = RenderHeight;
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
    DepthTexDesc.Width     = RenderWidth;
    DepthTexDesc.Height    = RenderHeight;
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
    MotionTexDesc.Width     = RenderWidth;
    MotionTexDesc.Height    = RenderHeight;
    MotionTexDesc.Format    = TEX_FORMAT_RG16_FLOAT;
    MotionTexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    MotionTexDesc.Usage     = USAGE_DEFAULT;

    RefCntAutoPtr<ITexture> pMotionTex;
    pDevice->CreateTexture(MotionTexDesc, nullptr, &pMotionTex);
    ASSERT_NE(pMotionTex, nullptr);

    // Create output texture
    TextureDesc OutputTexDesc;
    OutputTexDesc.Name      = "SR Output";
    OutputTexDesc.Type      = RESOURCE_DIM_TEX_2D;
    OutputTexDesc.Width     = OutputWidth;
    OutputTexDesc.Height    = OutputHeight;
    OutputTexDesc.Format    = TEX_FORMAT_RGBA16_FLOAT;
    OutputTexDesc.BindFlags = BIND_RENDER_TARGET | BIND_UNORDERED_ACCESS;
    OutputTexDesc.Usage     = USAGE_DEFAULT;

    RefCntAutoPtr<ITexture> pOutputTex;
    pDevice->CreateTexture(OutputTexDesc, nullptr, &pOutputTex);
    ASSERT_NE(pOutputTex, nullptr);

    // Execute temporal upscaling with reset
    auto* pContext = pEnv->GetDeviceContext();

    ExecuteSuperResolutionAttribs Attribs;
    Attribs.pColorTextureSRV   = pColorTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    Attribs.pDepthTextureSRV   = pDepthTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    Attribs.pMotionVectorsSRV  = pMotionTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    Attribs.pOutputTextureView  = pOutputTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    Attribs.JitterX            = 0.5f;
    Attribs.JitterY            = -0.5f;
    Attribs.MotionVectorScaleX = 1.0f;
    Attribs.MotionVectorScaleY = 1.0f;
    Attribs.ExposureScale      = 1.0f;
    Attribs.Sharpness          = 0.5f;
    Attribs.CameraNear         = 0.1f;
    Attribs.CameraFar          = 1000.0f;
    Attribs.CameraFovAngleVert = 1.0472f; // ~60 degrees

    Attribs.TimeDeltaInSeconds = 0.016f;
    Attribs.ResetHistory       = True;
    pContext->ExecuteSuperResolution(Attribs, pUpscaler);

    // Execute a second frame without reset
    Attribs.JitterX      = -0.25f;
    Attribs.JitterY      = 0.25f;
    Attribs.ResetHistory = False;
    pContext->ExecuteSuperResolution(Attribs, pUpscaler);

    pContext->Flush();
    pContext->WaitForIdle();
}

} // namespace
