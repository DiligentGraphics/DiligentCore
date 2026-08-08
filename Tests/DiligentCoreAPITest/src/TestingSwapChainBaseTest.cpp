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

#include "TestingSwapChainBase.hpp"

#include "FileSystem.hpp"
#include "TempDirectory.hpp"
#include "TestingEnvironment.hpp"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(TestingSwapChainBaseTest, ExactComparisonAcceptsIdenticalImages)
{
    constexpr std::array<Uint8, 8> Pixels{
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
    };

    std::unordered_map<std::string, int> FailureCounters;
    CompareTestImages(Pixels.data(), 8, Pixels.data(), 8, 2, 1,
                      TEX_FORMAT_RGBA8_UNORM, FailureCounters);
    EXPECT_TRUE(FailureCounters.empty());
}

TEST(TestingSwapChainBaseTest, ToleratesConfiguredImageDifferences)
{
    constexpr std::array<Uint8, 8> Reference{
        10,
        20,
        30,
        255,
        40,
        50,
        60,
        255,
    };
    constexpr std::array<Uint8, 8> Actual{
        12, 20, 30, 255, // Within the per-channel threshold.
        50, 50, 60, 255, // One bad pixel allowed by the ratio.
    };

    TestImageComparisonAttribs ComparisonAttribs;
    ComparisonAttribs.MaxChannelError  = 2;
    ComparisonAttribs.MaxBadPixelRatio = 0.5f;

    std::unordered_map<std::string, int> FailureCounters;
    CompareTestImages(Reference.data(), 8, Actual.data(), 8, 2, 1,
                      TEX_FORMAT_RGBA8_UNORM, FailureCounters, ComparisonAttribs);
    EXPECT_TRUE(FailureCounters.empty());
}

#if !PLATFORM_WEB

TEST(TestingSwapChainBaseTest, LoadsPNGAsRGBA8)
{
    constexpr Uint32                                Width  = 2;
    constexpr Uint32                                Height = 2;
    constexpr std::array<Uint8, Width * Height * 4> SourcePixels{
        255,
        0,
        0,
        0,
        0,
        255,
        0,
        64,
        0,
        0,
        255,
        128,
        255,
        255,
        255,
        255,
    };

    TempDirectory     TempDir{"TestingSwapChainBaseTest"};
    const std::string ImageName = TempDir.Get() + "/Reference";
    const std::string ImagePath = ImageName + ".png";
    DumpTestImage(SourcePixels.data(), Width * 4, Width, Height,
                  TEX_FORMAT_RGBA8_UNORM, ImageName.c_str(), false);

    std::vector<Uint8> LoadedPixels;
    Uint32             LoadedWidth  = 0;
    Uint32             LoadedHeight = 0;
    ASSERT_TRUE(LoadTestImage(ImagePath.c_str(), LoadedPixels, LoadedWidth, LoadedHeight));
    EXPECT_EQ(LoadedWidth, Width);
    EXPECT_EQ(LoadedHeight, Height);
    EXPECT_EQ(LoadedPixels.size(), SourcePixels.size());
    EXPECT_TRUE(std::equal(LoadedPixels.begin(), LoadedPixels.end(), SourcePixels.begin()));
}

TEST(TestingSwapChainBaseTest, LoadsReferenceImageIntoSwapChain)
{
    GPUTestingEnvironment::ScopedReset AutoReset;

    GPUTestingEnvironment* const pEnvironment = GPUTestingEnvironment::GetInstance();
    IDeviceContext* const        pContext     = pEnvironment->GetDeviceContext();
    ISwapChain* const            pSwapChain   = pEnvironment->GetSwapChain();
    ASSERT_NE(pContext, nullptr);
    ASSERT_NE(pSwapChain, nullptr);

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    ASSERT_NE(pTestingSwapChain, nullptr);

    const SwapChainDesc& SwapChainDesc = pSwapChain->GetDesc();
    std::vector<Uint8>   ReferencePixels(static_cast<size_t>(SwapChainDesc.Width) * SwapChainDesc.Height * 4);
    for (size_t Pixel = 0; Pixel < ReferencePixels.size(); Pixel += 4)
    {
        ReferencePixels[Pixel + 0] = 255;
        ReferencePixels[Pixel + 1] = 0;
        ReferencePixels[Pixel + 2] = 0;
        ReferencePixels[Pixel + 3] = 255;
    }

    TempDirectory     TempDir{"TestingSwapChainBaseTest"};
    const std::string ImageName = TempDir.Get() + "/Reference";
    const std::string ImagePath = ImageName + ".png";
    DumpTestImage(ReferencePixels.data(), Uint64{SwapChainDesc.Width} * 4,
                  SwapChainDesc.Width, SwapChainDesc.Height,
                  TEX_FORMAT_RGBA8_UNORM, ImageName.c_str(), false);
    ASSERT_TRUE(pTestingSwapChain->LoadReferenceImage(ImagePath.c_str()));

    constexpr float ClearColor[] = {1, 0, 0, 1};
    ITextureView*   pRTV         = pSwapChain->GetCurrentBackBufferRTV();
    pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(pRTV, ClearColor,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pTestingSwapChain->CompareWithSnapshot(nullptr);
}

TEST(TestingSwapChainBaseTest, ReportsComparisonFailure)
{
    GPUTestingEnvironment::ScopedReset AutoReset;

    GPUTestingEnvironment* const pEnvironment = GPUTestingEnvironment::GetInstance();
    IDeviceContext* const        pContext     = pEnvironment->GetDeviceContext();
    ISwapChain* const            pSwapChain   = pEnvironment->GetSwapChain();
    ASSERT_NE(pContext, nullptr);
    ASSERT_NE(pSwapChain, nullptr);

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    ASSERT_NE(pTestingSwapChain, nullptr);

    const SwapChainDesc& SwapChainDesc = pSwapChain->GetDesc();
    std::vector<Uint8>   ReferencePixels(static_cast<size_t>(SwapChainDesc.Width) * SwapChainDesc.Height * 4, 0);
    for (size_t Pixel = 3; Pixel < ReferencePixels.size(); Pixel += 4)
        ReferencePixels[Pixel] = 255;

    TempDirectory     TempDir{"TestingSwapChainBaseTest"};
    const std::string ImageName = TempDir.Get() + "/Reference";
    const std::string ImagePath = ImageName + ".png";
    DumpTestImage(ReferencePixels.data(), Uint64{SwapChainDesc.Width} * 4,
                  SwapChainDesc.Width, SwapChainDesc.Height,
                  TEX_FORMAT_RGBA8_UNORM, ImageName.c_str(), false);
    ASSERT_TRUE(pTestingSwapChain->LoadReferenceImage(ImagePath.c_str()));

    constexpr float ClearColor[] = {1, 1, 1, 1};
    ITextureView*   pRTV         = pSwapChain->GetCurrentBackBufferRTV();
    pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    EXPECT_NONFATAL_FAILURE(
        pTestingSwapChain->CompareWithSnapshot(nullptr),
        "Image rendered by the test differs from the reference image");

    FileSystem::DeleteFile("TestingSwapChainBaseTest.ReportsComparisonFailure_FAIL_.png");
}

#endif

} // namespace
