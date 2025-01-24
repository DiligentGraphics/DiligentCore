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

using namespace Diligent;

namespace
{

TEST(Common_ImageTools, GetImageDifference)
{
    constexpr Uint32 Width   = 3;
    constexpr Uint32 Height  = 2;
    constexpr Uint32 Stride1 = 11;
    constexpr Uint32 Stride2 = 12;
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
    // clang-format on

    {
        ImageDiffInfo Diff;
        GetImageDifference(Width, Height, 3, Image1, Stride1, Image1, Stride1, 3, Diff);
        EXPECT_EQ(Diff.NumDiffPixels, 0);
        EXPECT_EQ(Diff.NumDiffPixelsAboveThreshold, 0);
        EXPECT_EQ(Diff.MaxDiff, 0);
        EXPECT_EQ(Diff.AvgDiff, 0.f);
        EXPECT_EQ(Diff.RmsDiff, 0.f);
    }

    {
        ImageDiffInfo Diff;
        GetImageDifference(Width, Height, 3, Image1, Stride1, Image2, Stride2, 3, Diff);
        EXPECT_EQ(Diff.NumDiffPixels, 3);
        EXPECT_EQ(Diff.NumDiffPixelsAboveThreshold, 2);
        EXPECT_EQ(Diff.MaxDiff, 5);
        EXPECT_FLOAT_EQ(Diff.AvgDiff, 4.f);
        EXPECT_FLOAT_EQ(Diff.RmsDiff, std::sqrt((9.f + 16.f + 25.f) / 3.f));
    }
}

} // namespace
