/*
 *  Copyright 2025 Diligent Graphics LLC
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

#include "ImageTools.h"

#include <cmath>

#include "gtest/gtest.h"
#include <array>

using namespace Diligent;

namespace
{

TEST(Common_ImageTools, ComputeImageDifference)
{
    constexpr Uint32 Width   = 3;
    constexpr Uint32 Height  = 2;
    constexpr Uint32 Stride1 = 11;
    constexpr Uint32 Stride2 = 12;
    constexpr Uint32 Stride3 = 9;

    // clang-format off
	constexpr char Image1[Stride1 * Height] = {
		1, 2, 3,   4, 5, 6,  7, 8, 9,  10, 20,
		9, 8, 7,   5, 6, 4,  3, 2, 1,  30, 40,
	};
	constexpr char Image2[Stride2 * Height] = {
		1, 2, 3,   5, 8, 8,  7, 8, 9,  10, 20, 30,
//                 ^  ^  ^
//                -1 -3 -2
		6, 4, 2,   5, 6, 4,  7, 6, 1,  40, 50, 60,
//      ^  ^  ^              ^  ^
//      3  4  5              4  4
	};
	constexpr char Image3[Stride3 * Height] = {
		1, 2,      5, 8,     7, 8,     10, 20, 30,
//                 ^  ^ 
//                -1 -3
		6, 4,      5, 6,     8, 6,     40, 50, 60,
//      ^  ^                 ^  ^
//      3  4                 5  4
	};
    // clang-format on

    {
        ComputeImageDifferenceAttribs Attribs;
        Attribs.Width        = Width;
        Attribs.Height       = Height;
        Attribs.pImage1      = Image1;
        Attribs.NumChannels1 = 3;
        Attribs.Stride1      = Stride1;
        Attribs.pImage2      = Image1;
        Attribs.NumChannels2 = 3;
        Attribs.Stride2      = Stride1;

        ImageDiffInfo Diff;
        ComputeImageDifference(Attribs, Diff);
        EXPECT_EQ(Diff.NumDiffPixels, 0u);
        EXPECT_EQ(Diff.NumDiffPixelsAboveThreshold, 0u);
        EXPECT_EQ(Diff.MaxDiff, 0u);
        EXPECT_EQ(Diff.AvgDiff, 0.f);
        EXPECT_EQ(Diff.RmsDiff, 0.f);

        constexpr std::array<Uint8, Width * Height * 3> RefDiffImage{};

        std::array<Uint8, Width * Height * 3> DiffImage{};

        Attribs.pDiffImage = DiffImage.data();
        Attribs.DiffStride = Width * 3;
        ComputeImageDifference(Attribs, Diff);
        EXPECT_EQ(DiffImage, RefDiffImage);
    }

    {
        ComputeImageDifferenceAttribs Attribs;
        Attribs.Width        = Width;
        Attribs.Height       = Height;
        Attribs.pImage1      = Image1;
        Attribs.NumChannels1 = 3;
        Attribs.Stride1      = Stride1;
        Attribs.pImage2      = Image2;
        Attribs.NumChannels2 = 3;
        Attribs.Stride2      = Stride2;
        Attribs.Threshold    = 3;

        ImageDiffInfo Diff;
        ComputeImageDifference(Attribs, Diff);
        EXPECT_EQ(Diff.NumDiffPixels, 3u);
        EXPECT_EQ(Diff.NumDiffPixelsAboveThreshold, 2u);
        EXPECT_EQ(Diff.MaxDiff, 5u);
        EXPECT_FLOAT_EQ(Diff.AvgDiff, 4.f);
        EXPECT_FLOAT_EQ(Diff.RmsDiff, std::sqrt((9.f + 16.f + 25.f) / 3.f));


        // clang-format off
        constexpr std::array<Uint8, Width * Height * 3> RefDiffImage = {
            0, 0, 0,   1, 3, 2,   0, 0, 0,
            3, 4, 5,   0, 0, 0,   4, 4, 0,
        };
        // clang-format on

        std::array<Uint8, Width * Height * 3> DiffImage{};

        Attribs.pDiffImage = DiffImage.data();
        Attribs.DiffStride = Width * 3;
        ComputeImageDifference(Attribs, Diff);
        EXPECT_EQ(DiffImage, RefDiffImage);
    }

    {
        ComputeImageDifferenceAttribs Attribs;
        Attribs.Width        = Width;
        Attribs.Height       = Height;
        Attribs.pImage1      = Image1;
        Attribs.NumChannels1 = 3;
        Attribs.Stride1      = Stride1;
        Attribs.pImage2      = Image3;
        Attribs.NumChannels2 = 2;
        Attribs.Stride2      = Stride3;
        Attribs.Threshold    = 3;

        ImageDiffInfo Diff;
        ComputeImageDifference(Attribs, Diff);
        EXPECT_EQ(Diff.NumDiffPixels, 3u);
        EXPECT_EQ(Diff.NumDiffPixelsAboveThreshold, 2u);
        EXPECT_EQ(Diff.MaxDiff, 5u);
        EXPECT_FLOAT_EQ(Diff.AvgDiff, 4.f);
        EXPECT_FLOAT_EQ(Diff.RmsDiff, std::sqrt((9.f + 16.f + 25.f) / 3.f));
    }

    // 3 channels -> 4 channels
    {
        // clang-format off
        constexpr std::array<Uint8, Width * Height * 4> RefDiffImage = {
            0, 0, 0, 255,  1, 3, 2, 255,   0, 0, 0,  255,
            3, 4, 5, 255,  0, 0, 0, 255,   4, 4, 0,  255,
        };
        // clang-format on

        std::array<Uint8, Width * Height * 4> DiffImage{};

        ComputeImageDifferenceAttribs Attribs;
        Attribs.Width           = Width;
        Attribs.Height          = Height;
        Attribs.pImage1         = Image1;
        Attribs.NumChannels1    = 3;
        Attribs.Stride1         = Stride1;
        Attribs.pImage2         = Image2;
        Attribs.NumChannels2    = 3;
        Attribs.Stride2         = Stride2;
        Attribs.pDiffImage      = DiffImage.data();
        Attribs.NumDiffChannels = 4;
        Attribs.DiffStride      = Width * 4;

        ImageDiffInfo Diff;
        ComputeImageDifference(Attribs, Diff);
        EXPECT_EQ(DiffImage, RefDiffImage);
    }

    // 3 channels -> 4 channels + scale
    {
        // clang-format off
        constexpr std::array<Uint8, Width * Height * 4> RefDiffImage = {
            0, 0,  0, 255,  2, 6, 4, 255,   0, 0, 0, 255,
            6, 8, 10, 255,  0, 0, 0, 255,   8, 8, 0, 255,
        };
        // clang-format on

        std::array<Uint8, Width * Height * 4> DiffImage{};

        ComputeImageDifferenceAttribs Attribs;
        Attribs.Width           = Width;
        Attribs.Height          = Height;
        Attribs.pImage1         = Image1;
        Attribs.NumChannels1    = 3;
        Attribs.Stride1         = Stride1;
        Attribs.pImage2         = Image2;
        Attribs.NumChannels2    = 3;
        Attribs.Stride2         = Stride2;
        Attribs.pDiffImage      = DiffImage.data();
        Attribs.NumDiffChannels = 4;
        Attribs.DiffStride      = Width * 4;
        Attribs.Scale           = 2.f;

        ImageDiffInfo Diff;
        ComputeImageDifference(Attribs, Diff);
        EXPECT_EQ(DiffImage, RefDiffImage);
    }

    // 3 vs 2 channels -> 4 channels + scale
    {
        // clang-format off
        constexpr std::array<Uint8, Width * Height * 4> RefDiffImage = {
            0, 0,  0, 255,  2, 6, 0, 255,    0, 0, 0, 255,
            6, 8,  0, 255,  0, 0, 0, 255,   10, 8, 0, 255,
        };
        // clang-format on

        std::array<Uint8, Width * Height * 4> DiffImage{};

        ComputeImageDifferenceAttribs Attribs;
        Attribs.Width           = Width;
        Attribs.Height          = Height;
        Attribs.pImage1         = Image1;
        Attribs.NumChannels1    = 3;
        Attribs.Stride1         = Stride1;
        Attribs.pImage2         = Image3;
        Attribs.NumChannels2    = 2;
        Attribs.Stride2         = Stride3;
        Attribs.pDiffImage      = DiffImage.data();
        Attribs.NumDiffChannels = 4;
        Attribs.DiffStride      = Width * 4;
        Attribs.Scale           = 2.f;

        ImageDiffInfo Diff;
        ComputeImageDifference(Attribs, Diff);
        EXPECT_EQ(DiffImage, RefDiffImage);
    }
}

} // namespace
