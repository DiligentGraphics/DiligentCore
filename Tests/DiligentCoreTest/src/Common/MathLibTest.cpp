/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

#include <climits>
#include <sstream>

#include "BasicMath.hpp"
#include "AdvancedMath.hpp"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

// Constructors
TEST(Common_BasicMath, VectorConstructors)
{
    {
        float2 f2{1, 2};

        EXPECT_EQ(f2.x, 1);
        EXPECT_EQ(f2.y, 2);

        EXPECT_EQ(f2.x, f2[0]);
        EXPECT_EQ(f2.y, f2[1]);
    }

    {
        float3 f3{1, 2, 3};

        EXPECT_EQ(f3.x, 1);
        EXPECT_EQ(f3.y, 2);
        EXPECT_EQ(f3.z, 3);

        EXPECT_EQ(f3.x, f3[0]);
        EXPECT_EQ(f3.y, f3[1]);
        EXPECT_EQ(f3.z, f3[2]);
    }

    {
        float4 f4{1, 2, 3, 4};

        EXPECT_EQ(f4.x, 1);
        EXPECT_EQ(f4.y, 2);
        EXPECT_EQ(f4.z, 3);
        EXPECT_EQ(f4.w, 4);

        EXPECT_EQ(f4.x, f4[0]);
        EXPECT_EQ(f4.y, f4[1]);
        EXPECT_EQ(f4.z, f4[2]);
        EXPECT_EQ(f4.w, f4[3]);
    }
}

// a - b
TEST(Common_BasicMath, OpeartorMinus)
{
    {
        auto v = float2{5, 3} - float2{1, 2};
        EXPECT_EQ(v.x, 4);
        EXPECT_EQ(v.y, 1);
    }

    {
        auto v = float3{5, 3, 20} - float3{1, 2, 10};
        EXPECT_EQ(v.x, 4);
        EXPECT_EQ(v.y, 1);
        EXPECT_EQ(v.z, 10);
    }

    {
        auto v = float4{5, 3, 20, 200} - float4{1, 2, 10, 100};
        EXPECT_EQ(v.x, 4);
        EXPECT_EQ(v.y, 1);
        EXPECT_EQ(v.z, 10);
        EXPECT_EQ(v.w, 100);
    }
}

// a -= b
TEST(Common_BasicMath, OpeartorMinusEqual)
{
    {
        auto v = float2{5, 3};
        v -= float2{1, 2};
        EXPECT_EQ(v.x, 4);
        EXPECT_EQ(v.y, 1);
    }

    {
        auto v = float3{5, 3, 20};
        v -= float3{1, 2, 10};
        EXPECT_EQ(v.x, 4);
        EXPECT_EQ(v.y, 1);
        EXPECT_EQ(v.z, 10);
    }

    {
        auto v = float4{5, 3, 20, 200};
        v -= float4{1, 2, 10, 100};
        EXPECT_EQ(v.x, 4);
        EXPECT_EQ(v.y, 1);
        EXPECT_EQ(v.z, 10);
        EXPECT_EQ(v.w, 100);
    }
}

// -a
TEST(Common_BasicMath, UnaryMinus)
{
    {
        auto v = -float2{1, 2};
        EXPECT_EQ(v.x, -1);
        EXPECT_EQ(v.y, -2);
    }

    {
        auto v = -float3{1, 2, 3};
        EXPECT_EQ(v.x, -1);
        EXPECT_EQ(v.y, -2);
        EXPECT_EQ(v.z, -3);
    }

    {
        auto v = -float4{1, 2, 3, 4};
        EXPECT_EQ(v.x, -1);
        EXPECT_EQ(v.y, -2);
        EXPECT_EQ(v.z, -3);
        EXPECT_EQ(v.w, -4);
    }
}

// a + b
TEST(Common_BasicMath, OperatorPlus)
{
    // a + b
    {
        auto v = float2{5, 3} + float2{1, 2};
        EXPECT_EQ(v.x, 6);
        EXPECT_EQ(v.y, 5);
    }

    {
        auto v = float3{5, 3, 20} + float3{1, 2, 10};
        EXPECT_EQ(v.x, 6);
        EXPECT_EQ(v.y, 5);
        EXPECT_EQ(v.z, 30);
    }

    {
        auto v = float4{5, 3, 20, 200} + float4{1, 2, 10, 100};
        EXPECT_EQ(v.x, 6);
        EXPECT_EQ(v.y, 5);
        EXPECT_EQ(v.z, 30);
        EXPECT_EQ(v.w, 300);
    }
}

// a += b
TEST(Common_BasicMath, OpeartorPlusEqual)
{
    {
        auto v = float2{5, 3};
        v += float2{1, 2};
        EXPECT_EQ(v.x, 6);
        EXPECT_EQ(v.y, 5);
    }

    {
        auto v = float3{5, 3, 20};
        v += float3{1, 2, 10};
        EXPECT_EQ(v.x, 6);
        EXPECT_EQ(v.y, 5);
        EXPECT_EQ(v.z, 30);
    }

    {
        auto v = float4{5, 3, 20, 200};
        v += float4{1, 2, 10, 100};
        EXPECT_EQ(v.x, 6);
        EXPECT_EQ(v.y, 5);
        EXPECT_EQ(v.z, 30);
        EXPECT_EQ(v.w, 300);
    }
}

// a * b
TEST(Common_BasicMath, VectorVectorMultiply)
{
    {
        auto v = float2{5, 3} * float2{1, 2};
        EXPECT_EQ(v.x, 5);
        EXPECT_EQ(v.y, 6);
    }

    {
        auto v = float3{5, 3, 20} * float3{1, 2, 3};
        EXPECT_EQ(v.x, 5);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 60);
    }

    {
        auto v = float4{5, 3, 20, 200} * float4{1, 2, 3, 4};
        EXPECT_EQ(v.x, 5);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 60);
        EXPECT_EQ(v.w, 800);
    }
}

// a *= b
TEST(Common_BasicMath, VectorVectorMultiplyEqual)
{
    {
        auto v = float2{5, 3};
        v *= float2{1, 2};
        EXPECT_EQ(v.x, 5);
        EXPECT_EQ(v.y, 6);
    }

    {
        auto v = float3{5, 3, 20};
        v *= float3{1, 2, 3};
        EXPECT_EQ(v.x, 5);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 60);
    }

    {
        auto v = float4{5, 3, 20, 200};
        v *= float4{1, 2, 3, 4};
        EXPECT_EQ(v.x, 5);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 60);
        EXPECT_EQ(v.w, 800);
    }
}


// a * s
TEST(Common_BasicMath, VectorScalarMultiply)
{
    {
        auto v = float2{5, 3} * 2;
        EXPECT_EQ(v.x, 10);
        EXPECT_EQ(v.y, 6);
    }

    {
        auto v = float3{5, 3, 20} * 2;
        EXPECT_EQ(v.x, 10);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 40);
    }

    {
        auto v = float4{5, 3, 20, 200} * 2;
        EXPECT_EQ(v.x, 10);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 40);
        EXPECT_EQ(v.w, 400);
    }
}

// a *= s
TEST(Common_BasicMath, VectorScalarMultiplyEqual)
{
    {
        auto v = float2{5, 3};
        v *= 2;
        EXPECT_EQ(v.x, 10);
        EXPECT_EQ(v.y, 6);
    }

    {
        auto v = float3{5, 3, 20};
        v *= 2;
        EXPECT_EQ(v.x, 10);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 40);
    }

    {
        auto v = float4{5, 3, 20, 200};
        v *= 2;
        EXPECT_EQ(v.x, 10);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 40);
        EXPECT_EQ(v.w, 400);
    }
}

// s * a
TEST(Common_BasicMath, ScalarVectorMultiply)
{
    {
        auto v = 2.f * float2{5, 3};
        EXPECT_EQ(v.x, 10);
        EXPECT_EQ(v.y, 6);
    }

    {
        auto v = 2.f * float3{5, 3, 20};
        EXPECT_EQ(v.x, 10);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 40);
    }

    {
        auto v = 2.f * float4{5, 3, 20, 200};
        EXPECT_EQ(v.x, 10);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 40);
        EXPECT_EQ(v.w, 400);
    }
}

// v * m, m * v
TEST(Common_BasicMath, VectorMatrixMultiply)
{
    {
        float2 v{1, 2};

        // clang-format off
        float2x2 m
        {
            1, 2,
            3, 4
        };
        // clang-format on

        auto v1 = v * m;
        EXPECT_EQ(v1.x, 1 * 1 + 2 * 3);
        EXPECT_EQ(v1.y, 1 * 2 + 2 * 4);

        auto v2 = m * v;
        EXPECT_EQ(v2.x, 1 * 1 + 2 * 2);
        EXPECT_EQ(v2.y, 3 * 1 + 4 * 2);
    }

    {
        float3 v{1, 2, 3};

        // clang-format off
        float3x3 m
        {
            1, 2, 3,
            4, 5, 6,
            7, 8, 9
        };
        // clang-format on

        auto v1 = v * m;
        EXPECT_EQ(v1.x, 1 * 1 + 2 * 4 + 3 * 7);
        EXPECT_EQ(v1.y, 1 * 2 + 2 * 5 + 3 * 8);
        EXPECT_EQ(v1.z, 1 * 3 + 2 * 6 + 3 * 9);

        auto v2 = m * v;
        EXPECT_EQ(v2.x, 1 * 1 + 2 * 2 + 3 * 3);
        EXPECT_EQ(v2.y, 4 * 1 + 5 * 2 + 6 * 3);
        EXPECT_EQ(v2.z, 7 * 1 + 8 * 2 + 9 * 3);
    }

    {
        float4 v{1, 2, 3, 4};

        // clang-format off
        float4x4 m
        {
            1,   2,  3,  4,
            5,   6,  7,  8,
            9,  10, 11, 12,
            13, 14, 15, 16
        };
        // clang-format on

        auto v1 = v * m;
        EXPECT_EQ(v1.x, 1 * 1 + 2 * 5 + 3 * 9 + 4 * 13);
        EXPECT_EQ(v1.y, 1 * 2 + 2 * 6 + 3 * 10 + 4 * 14);
        EXPECT_EQ(v1.z, 1 * 3 + 2 * 7 + 3 * 11 + 4 * 15);
        EXPECT_EQ(v1.w, 1 * 4 + 2 * 8 + 3 * 12 + 4 * 16);

        auto v2 = m * v;
        EXPECT_EQ(v2.x, 1 * 1 + 2 * 2 + 3 * 3 + 4 * 4);
        EXPECT_EQ(v2.y, 5 * 1 + 6 * 2 + 7 * 3 + 8 * 4);
        EXPECT_EQ(v2.z, 9 * 1 + 10 * 2 + 11 * 3 + 12 * 4);
        EXPECT_EQ(v2.w, 13 * 1 + 14 * 2 + 15 * 3 + 16 * 4);
    }
}

// a / s
TEST(Common_BasicMath, VectorScalarDivision)
{
    {
        auto v = float2{10, 6} / 2;
        EXPECT_EQ(v.x, 5);
        EXPECT_EQ(v.y, 3);
    }

    {
        auto v = float3{10, 6, 40} / 2;
        EXPECT_EQ(v.x, 5);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 20);
    }

    {
        auto v = float4{10, 6, 40, 400} / 2;
        EXPECT_EQ(v.x, 5);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 20);
        EXPECT_EQ(v.w, 200);
    }
}

// a / b
TEST(Common_BasicMath, VectorVectorDivision)
{
    {
        auto v = float2{6, 4} / float2{1, 2};
        EXPECT_EQ(v.x, 6);
        EXPECT_EQ(v.y, 2);
    }

    {
        auto v = float3{6, 3, 20} / float3{3, 1, 5};
        EXPECT_EQ(v.x, 2);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 4);
    }

    {
        auto v = float4{6, 3, 20, 200} / float4{3, 1, 5, 40};
        EXPECT_EQ(v.x, 2);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 4);
        EXPECT_EQ(v.w, 5);
    }
}

// a /= b
TEST(Common_BasicMath, VectorVectorDivideEqual)
{
    {
        auto v = float2{6, 4};
        v /= float2{1, 2};
        EXPECT_EQ(v.x, 6);
        EXPECT_EQ(v.y, 2);
    }

    {
        auto v = float3{6, 3, 20};
        v /= float3{3, 1, 5};
        EXPECT_EQ(v.x, 2);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 4);
    }

    {
        auto v = float4{6, 3, 20, 200};
        v /= float4{3, 1, 5, 40};
        EXPECT_EQ(v.x, 2);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 4);
        EXPECT_EQ(v.w, 5);
    }
}


// a /= s
TEST(Common_BasicMath, VectorScalarDivideEqual)
{
    {
        auto v = float2{6, 4};
        v /= 2;
        EXPECT_EQ(v.x, 3);
        EXPECT_EQ(v.y, 2);
    }

    {
        auto v = float3{4, 6, 20};
        v /= 2;
        EXPECT_EQ(v.x, 2);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 10);
    }

    {
        auto v = float4{4, 6, 20, 200};
        v /= 2;
        EXPECT_EQ(v.x, 2);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 10);
        EXPECT_EQ(v.w, 100);
    }
}


// max
TEST(Common_BasicMath, StdMax)
{
    {
        auto v = std::max(float2{6, 4}, float2{1, 40});
        EXPECT_EQ(v.x, 6);
        EXPECT_EQ(v.y, 40);
    }

    {
        auto v = std::max(float3{4, 6, 20}, float3{40, 3, 23});
        EXPECT_EQ(v.x, 40);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 23);
    }

    {
        auto v = std::max(float4{4, 6, 20, 100}, float4{40, 3, 23, 50});
        EXPECT_EQ(v.x, 40);
        EXPECT_EQ(v.y, 6);
        EXPECT_EQ(v.z, 23);
        EXPECT_EQ(v.w, 100);
    }
}

// min
TEST(Common_BasicMath, StdMin)
{
    {
        auto v = std::min(float2{6, 4}, float2{1, 40});
        EXPECT_EQ(v.x, 1);
        EXPECT_EQ(v.y, 4);
    }

    {
        auto v = std::min(float3{4, 6, 20}, float3{40, 3, 23});
        EXPECT_EQ(v.x, 4);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 20);
    }

    {
        auto v = std::min(float4{4, 6, 20, 100}, float4{40, 3, 23, 50});
        EXPECT_EQ(v.x, 4);
        EXPECT_EQ(v.y, 3);
        EXPECT_EQ(v.z, 20);
        EXPECT_EQ(v.w, 50);
    }
}

// max(x, y, z, ...)
TEST(Common_BasicMath, Max_N)
{
    EXPECT_EQ(max(10, 2, 3), 10);
    EXPECT_EQ(max(1, 20, 3), 20);
    EXPECT_EQ(max(1, 2, 30), 30);

    EXPECT_EQ(max(10, 2, 3, 4), 10);
    EXPECT_EQ(max(1, 20, 3, 4), 20);
    EXPECT_EQ(max(1, 2, 30, 4), 30);
    EXPECT_EQ(max(1, 2, 3, 40), 40);
}

// min(x, y, z, ...)
TEST(Common_BasicMath, Min_N)
{
    EXPECT_EQ(min(1, 20, 30), 1);
    EXPECT_EQ(min(10, 2, 30), 2);
    EXPECT_EQ(min(10, 20, 3), 3);

    EXPECT_EQ(min(1, 20, 30, 40), 1);
    EXPECT_EQ(min(10, 2, 30, 40), 2);
    EXPECT_EQ(min(10, 20, 3, 40), 3);
    EXPECT_EQ(min(10, 20, 30, 4), 4);
}

// a == b
TEST(Common_BasicMath, ComparisonOperators)
{
    {
        EXPECT_TRUE(float2(1, 2) == float2(1, 2));
        EXPECT_TRUE(float3(1, 2, 3) == float3(1, 2, 3));
        EXPECT_TRUE(float4(1, 2, 3, 4) == float4(1, 2, 3, 4));
    }

    {
        float4 vec4(1, 2, 3, 4);
        float3 vec3 = vec4;
        EXPECT_TRUE(vec3 == float3(1, 2, 3));
    }

    // a != b
    {
        EXPECT_TRUE(float2(1, 2) != float2(1, 9));
        EXPECT_TRUE(float2(9, 2) != float2(1, 2));
        EXPECT_TRUE(float3(1, 2, 3) != float3(9, 2, 3));
        EXPECT_TRUE(float3(1, 2, 3) != float3(1, 9, 3));
        EXPECT_TRUE(float3(1, 2, 3) != float3(1, 2, 9));
        EXPECT_TRUE(float4(1, 2, 3, 4) != float4(9, 2, 3, 4));
        EXPECT_TRUE(float4(1, 2, 3, 4) != float4(1, 9, 3, 4));
        EXPECT_TRUE(float4(1, 2, 3, 4) != float4(1, 2, 9, 4));
        EXPECT_TRUE(float4(1, 2, 3, 4) != float4(1, 2, 3, 9));
    }

    // a < b
    {
        // clang-format off
        EXPECT_TRUE((float2(1, 5) < float2(3, 5)) == float2(1, 0));
        EXPECT_TRUE((float2(3, 1) < float2(3, 4)) == float2(0, 1));
        EXPECT_TRUE((float3(1, 5, 10) < float3(3, 5, 20)) == float3(1, 0, 1));
        EXPECT_TRUE((float3(3, 1,  2) < float3(3, 4,  2)) == float3(0, 1, 0));
        EXPECT_TRUE((float4(1, 4, 10, 50) < float4(3, 4, 20, 50)) == float4(1, 0, 1, 0));
        EXPECT_TRUE((float4(3, 1,  2, 30) < float4(3, 4,  2, 70)) == float4(0, 1, 0, 1));
        // clang-format on
    }

    // a <= b
    {
        // clang-format off
        EXPECT_TRUE((float2(1, 5) <= float2(1, 4)) == float2(1, 0));
        EXPECT_TRUE((float2(5, 2) <= float2(3, 2)) == float2(0, 1));
        EXPECT_TRUE((float3(3, 5, 10) <= float3(3, 4, 10)) == float3(1, 0, 1));
        EXPECT_TRUE((float3(5, 4,  2) <= float3(3, 4,  0)) == float3(0, 1, 0));
        EXPECT_TRUE((float4(3, 5, 20, 100) <= float4(3, 4, 20, 50)) == float4(1, 0, 1, 0));
        EXPECT_TRUE((float4(5, 4,  2, 70)  <= float4(3, 4,  0, 70)) == float4(0, 1, 0, 1));
        // clang-format on
    }

    // a >= b
    {
        // clang-format off
        EXPECT_TRUE((float2(1, 5) >= float2(3, 5)) == float2(0, 1));
        EXPECT_TRUE((float2(3, 1) >= float2(3, 4)) == float2(1, 0));
        EXPECT_TRUE((float3(1, 5, 10) >= float3(3, 5, 20)) == float3(0, 1, 0));
        EXPECT_TRUE((float3(3, 1,  2) >= float3(3, 4,  2)) == float3(1, 0, 1));
        EXPECT_TRUE((float4(1, 4, 10, 50) >= float4(3, 4, 20, 50)) == float4(0, 1, 0, 1));
        EXPECT_TRUE((float4(3, 1, 2,  30) >= float4(3, 4,  2, 70)) == float4(1, 0, 1, 0));
        // clang-format on
    }

    // a > b
    {
        // clang-format off
        EXPECT_TRUE((float2(1, 5) > float2(1, 4)) == float2(0, 1));
        EXPECT_TRUE((float2(5, 2) > float2(3, 2)) == float2(1, 0));
        EXPECT_TRUE((float3(3, 5, 10) > float3(3, 4, 10)) == float3(0, 1, 0));
        EXPECT_TRUE((float3(5, 4,  2) > float3(3, 4,  0)) == float3(1, 0, 1));
        EXPECT_TRUE((float4(3, 5, 20, 100) > float4(3, 4, 20, 50)) == float4(0, 1, 0, 1));
        EXPECT_TRUE((float4(5, 4,  2,  70) > float4(3, 4,  0, 70)) == float4(1, 0, 1, 0));
        // clang-format on
    }
}

// Functions
TEST(Common_BasicMath, Abs)
{
    {
        // clang-format off
        EXPECT_EQ(abs(float2(-1, -5)), float2(1, 5));
        EXPECT_EQ(abs(float2( 1,  5)), float2(1, 5));

        EXPECT_EQ(abs(float3(-1, -5, -10)), float3(1, 5, 10));
        EXPECT_EQ(abs(float3( 1,  5,  10)), float3(1, 5, 10));

        EXPECT_EQ(abs(float4(-1, -5, -10, -100)), float4(1, 5, 10, 100));
        EXPECT_EQ(abs(float4( 1,  5,  10,  100)), float4(1, 5, 10, 100));
        // clang-format on
    }

    // clamp
    {
        // clang-format off
        EXPECT_EQ(clamp(-1, 1, 10),  1);
        EXPECT_EQ(clamp(11, 1, 10), 10);
        EXPECT_EQ(clamp( 9, 1, 10),  9);

        EXPECT_EQ(clamp(float2(-10, -11), float2(1, 2), float2(10, 11)), float2(1,   2));
        EXPECT_EQ(clamp(float2( 11,  12), float2(1, 2), float2(10, 11)), float2(10, 11));
        EXPECT_EQ(clamp(float2(  9,   8), float2(1, 2), float2(10, 11)), float2(9,   8));

        EXPECT_EQ(clamp(float3(-10, -11, -12), float3(1, 2, 3), float3(10, 11, 12)), float3( 1, 2,   3));
        EXPECT_EQ(clamp(float3( 11,  12,  13), float3(1, 2, 3), float3(10, 11, 12)), float3(10, 11, 12));
        EXPECT_EQ(clamp(float3( 9,    8,   7), float3(1, 2, 3), float3(10, 11, 12)), float3( 9, 8,   7));

        EXPECT_EQ(clamp(float4(-10, -11, -12, -13), float4(1, 2, 3, 4), float4(10, 11, 12, 13)), float4( 1,  2,  3,  4));
        EXPECT_EQ(clamp(float4( 11,  12,  13,  14), float4(1, 2, 3, 4), float4(10, 11, 12, 13)), float4(10, 11, 12, 13));
        EXPECT_EQ(clamp(float4(  9,   8,   7,   6), float4(1, 2, 3, 4), float4(10, 11, 12, 13)), float4( 9,  8,  7,  6));
        // clang-format on
    }

    // dot
    {
        EXPECT_EQ(dot(float2(1, 2), float2(1, 2)), 5);
        EXPECT_EQ(dot(float3(1, 2, 3), float3(1, 2, 3)), 14);
        EXPECT_EQ(dot(float4(1, 2, 3, 4), float4(1, 2, 3, 4)), 30);
    }

    // length
    {
        auto l = length(float2(3, 4));
        EXPECT_NEAR(l, 5.f, 1e-6);
    }
}


TEST(Common_BasicMath, MatrixConstructors)
{
    // Matrix 2x2
    {
        // clang-format off
        float2x2 m1
        {
            1, 2,
            5, 6
        };
        float2x2 m2
        {
            1, 2,
            5, 6
        };
        // clang-format on
        EXPECT_TRUE(m1._11 == 1 && m1._12 == 2 &&
                    m1._21 == 5 && m1._22 == 6);
        EXPECT_TRUE(m1[0][0] == 1 && m1[0][1] == 2 &&
                    m1[1][0] == 5 && m1[1][1] == 6);

        EXPECT_TRUE(m1 == m2);
        auto t = m1.Transpose().Transpose();
        EXPECT_TRUE(t == m1);
    }

    // Matrix 3x3
    {
        // clang-format off
        float3x3 m1
        {
            1,  2, 3,
            5,  6, 7,
            9, 10, 11
        };
        float3x3 m2
        {
            1,  2,  3,
            5,  6,  7,
            9, 10, 11
        };

        EXPECT_TRUE(m1._11 == 1 && m1._12 ==  2 && m1._13 ==  3 &&
                    m1._21 == 5 && m1._22 ==  6 && m1._23 ==  7 &&
                    m1._31 == 9 && m1._32 == 10 && m1._33 == 11);
        EXPECT_TRUE(m1[0][0] == 1 && m1[0][1] ==  2 && m1[0][2] ==  3 &&
                    m1[1][0] == 5 && m1[1][1] ==  6 && m1[1][2] ==  7 &&
                    m1[2][0] == 9 && m1[2][1] == 10 && m1[2][2] == 11);

        // clang-format on

        EXPECT_TRUE(m1 == m2);
        auto t = m1.Transpose().Transpose();
        EXPECT_TRUE(t == m1);
    }

    // Matrix 4x4
    {
        // clang-format off
        float4x4 m1
        {
            1,   2,  3,  4,
            5,   6,  7,  8,
            9,  10, 11, 12,
            13, 14, 15, 16
        };

        float4x4 m2
        {
             1,  2,  3,  4,
             5,  6,  7,  8,
             9, 10, 11, 12,
            13, 14, 15, 16
        };

        EXPECT_TRUE(m1._11 ==  1 && m1._12 ==  2 && m1._13 ==  3 && m1._14 ==  4 &&
                    m1._21 ==  5 && m1._22 ==  6 && m1._23 ==  7 && m1._24 ==  8 &&
                    m1._31 ==  9 && m1._32 == 10 && m1._33 == 11 && m1._34 == 12 &&
                    m1._41 == 13 && m1._42 == 14 && m1._43 == 15 && m1._44 == 16);
        EXPECT_TRUE(m1[0][0] ==  1 && m1[0][1] ==  2 && m1[0][2] ==  3 && m1[0][3] ==  4 &&
                    m1[1][0] ==  5 && m1[1][1] ==  6 && m1[1][2] ==  7 && m1[1][3] ==  8 &&
                    m1[2][0] ==  9 && m1[2][1] == 10 && m1[2][2] == 11 && m1[2][3] == 12 &&
                    m1[3][0] == 13 && m1[3][1] == 14 && m1[3][2] == 15 && m1[3][3] == 16);
        // clang-format on

        EXPECT_TRUE(m1 == m2);
        auto t = m1.Transpose().Transpose();
        EXPECT_TRUE(t == m1);

        // clang-format off
        float4x4 m3
        {
            float4{ 1,  2,  3,  4},
            float4{ 5,  6,  7,  8},
            float4{ 9, 10, 11, 12},
            float4{13, 14, 15, 16}
        };
        EXPECT_TRUE(m1 == m3);
        // clang-format on
    }
}

TEST(Common_BasicMath, MatrixEquality)
{
    // Matrix 2x2
    {
        // clang-format off
        const float2x2 ref
        {
            1, 2,
            5, 6
        };
        // clang-format on

        {
            auto m1 = ref;
            auto m2 = ref;
            EXPECT_TRUE(m1 == m2);
            EXPECT_FALSE(m1 != m2);
        }

        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                auto m1  = ref;
                auto m2  = ref;
                m1[i][j] = 0;
                EXPECT_FALSE(m1 == m2);
                EXPECT_TRUE(m1 != m2);
            }
        }
    }

    // Matrix 3x3
    {
        // clang-format off
        const float3x3 ref
        {
             1,  2,  3,
             5,  6,  7,
             9, 10, 11
        };
        // clang-format on

        {
            auto m1 = ref;
            auto m2 = ref;
            EXPECT_TRUE(m1 == m2);
            EXPECT_FALSE(m1 != m2);
        }

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                auto m1  = ref;
                auto m2  = ref;
                m1[i][j] = 0;
                EXPECT_FALSE(m1 == m2);
                EXPECT_TRUE(m1 != m2);
            }
        }
    }

    // Matrix 4x4
    {
        // clang-format off
        const float4x4 ref
        {
             1,  2,  3,  4,
             5,  6,  7,  8,
             9, 10, 11, 12,
            13, 14, 15, 16
        };
        // clang-format on

        {
            auto m1 = ref;
            auto m2 = ref;
            EXPECT_TRUE(m1 == m2);
            EXPECT_FALSE(m1 != m2);
        }

        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                auto m1  = ref;
                auto m2  = ref;
                m1[i][j] = 0;
                EXPECT_FALSE(m1 == m2);
                EXPECT_TRUE(m1 != m2);
            }
        }
    }
}

TEST(Common_BasicMath, MatrixInverse)
{
    {
        // clang-format off
        float4x4 m
        {
            7,   8,  3, 6,
            5,   1,  4, 9,
            5,  11,  7, 2,
            13,  4, 19, 8
        };
        // clang-format on

        auto inv      = m.Inverse();
        auto identity = m * inv;

        for (int j = 0; j < 4; ++j)
        {
            for (int i = 0; i < 4; ++i)
            {
                float ref = i == j ? 1.f : 0.f;
                auto  val = identity[i][j];
                EXPECT_NEAR(val, ref, 1e-6f);
            }
        }
    }

    // Determinant
    {
        // clang-format off
        float4x4 m1
        {
            1,  2,   3,  4,
            5,  6,   7,  8,
            9, 10,  11, 12,
            13, 14, 15, 16
        };
        // clang-format on
        auto det = m1.Determinant();
        EXPECT_EQ(det, 0);
    }

    {
        // clang-format off
        float2x2 m
        {
            7,   13,
            3,   1
        };
        // clang-format on

        auto inv      = m.Inverse();
        auto identity = m * inv;

        for (int j = 0; j < 2; ++j)
        {
            for (int i = 0; i < 2; ++i)
            {
                float ref = i == j ? 1.f : 0.f;
                auto  val = identity[i][j];
                EXPECT_NEAR(val, ref, 1e-6f);
            }
        }
    }

    // Determinant
    {
        // clang-format off
        float3x3 m1
        {
            1,  2,   3,
            5,  6,   7,
            9, 10,  11
        };
        // clang-format on
        auto det = m1.Determinant();
        EXPECT_EQ(det, 0);
    }

    {
        // clang-format off
        const float3x3 m
        {
            7,   8,  3,
            5,   1,  4,
            5,  11,  7
        };
        // clang-format on

        auto inv      = m.Inverse();
        auto identity = m * inv;

        for (int j = 0; j < 3; ++j)
        {
            for (int i = 0; i < 3; ++i)
            {
                float ref = i == j ? 1.f : 0.f;
                auto  val = identity[i][j];
                EXPECT_NEAR(val, ref, 1e-6f);
            }
        }
    }
}


TEST(Common_BasicMath, Hash)
{
    {
        EXPECT_NE(std::hash<float2>{}(float2{1, 2}), size_t{0});
        EXPECT_NE(std::hash<float3>{}(float3{1, 2, 3}), size_t{0});
        EXPECT_NE(std::hash<float4>{}(float4{1, 2, 3, 5}), size_t{0});
        // clang-format off
        float4x4 m1
        {
            1,   2,  3,  4,
            5,   6,  7,  8,
            9,  10, 11, 12,
            13, 14, 15, 16
        };
        // clang-format on
        EXPECT_NE(std::hash<float4x4>{}(m1), size_t{0});

        // clang-format off
        float3x3 m2
        {
            1,  2,  3,
            5,  6,  7,
            9, 10, 11
        };
        // clang-format on
        EXPECT_NE(std::hash<float3x3>{}(m2), size_t{0});

        // clang-format off
        float2x2 m3
        {
            1, 2,
            5, 6
        };
        // clang-format on
        EXPECT_NE(std::hash<float2x2>{}(m3), size_t{0});
    }
}

TEST(Common_BasicMath, OrthoProjection)
{
    {
        float4x4 OrthoProj = float4x4::Ortho(2.f, 4.f, -4.f, 12.f, false);

        auto c0 = float3{-1.f, -2.f, -4.f} * OrthoProj;
        auto c1 = float3{+1.f, +2.f, +12.f} * OrthoProj;
        EXPECT_EQ(c0, float3(-1, -1, 0));
        EXPECT_EQ(c1, float3(+1, +1, +1));
    }

    {
        float4x4 OrthoProj = float4x4::Ortho(2.f, 4.f, -4.f, 12.f, true);

        auto c0 = float3(-1.f, -2.f, -4.f) * OrthoProj;
        auto c1 = float3(+1.f, +2.f, +12.f) * OrthoProj;
        EXPECT_EQ(c0, float3(-1, -1, -1));
        EXPECT_EQ(c1, float3(+1, +1, +1));
    }

    {
        float4x4 OrthoProj = float4x4::OrthoOffCenter(-2.f, 6.f, -4.f, +12.f, -6.f, 10.f, false);

        auto c0 = float3{-2.f, -4.f, -6.f} * OrthoProj;
        auto c1 = float3{+6.f, +12.f, +10.f} * OrthoProj;
        EXPECT_EQ(c0, float3(-1, -1, 0));
        EXPECT_EQ(c1, float3(+1, +1, +1));
    }

    {
        float4x4 OrthoProj = float4x4::OrthoOffCenter(-2.f, 6.f, -4.f, +12.f, -6.f, 10.f, true);

        auto c0 = float3{-2.f, -4.f, -6.f} * OrthoProj;
        auto c1 = float3{+6.f, +12.f, +10.f} * OrthoProj;
        EXPECT_EQ(c0, float3(-1, -1, -1));
        EXPECT_EQ(c1, float3(+1, +1, +1));
    }
}

TEST(Common_BasicMath, MakeObject)
{
    {
        double data[] = {1, 2, 3, 4,
                         5, 6, 7, 8,
                         9, 10, 11, 12,
                         13, 14, 15, 16};
        EXPECT_EQ(float2::MakeVector(data), float2(1, 2));
        EXPECT_EQ(float3::MakeVector(data), float3(1, 2, 3));
        EXPECT_EQ(float4::MakeVector(data), float4(1, 2, 3, 4));
        EXPECT_EQ(QuaternionF::MakeQuaternion(data), QuaternionF(1, 2, 3, 4));
        EXPECT_EQ(float4x4::MakeMatrix(data), float4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
        EXPECT_EQ(float3x3::MakeMatrix(data), float3x3(1, 2, 3, 4, 5, 6, 7, 8, 9));
        EXPECT_EQ(float2x2::MakeMatrix(data), float2x2(1, 2, 3, 4));
    }

    {
        std::vector<int> data = {10, 20, 30, 40,
                                 50, 60, 70, 80,
                                 90, 100, 110, 120,
                                 130, 140, 150, 160};
        EXPECT_EQ(float2::MakeVector(data), float2(10, 20));
        EXPECT_EQ(float3::MakeVector(data.begin()), float3(10, 20, 30));
        EXPECT_EQ(float4::MakeVector(data.cbegin()), float4(10, 20, 30, 40));
        EXPECT_EQ(QuaternionF::MakeQuaternion(data.rbegin()), QuaternionF(160, 150, 140, 130));
        EXPECT_EQ(float4x4::MakeMatrix(data.begin()), float4x4(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160));
        EXPECT_EQ(float3x3::MakeMatrix(data.cbegin()), float3x3(10, 20, 30, 40, 50, 60, 70, 80, 90));
        EXPECT_EQ(float2x2::MakeMatrix(data.crbegin()), float2x2(160, 150, 140, 130));
    }
}

TEST(Common_BasicMath, MatrixMultiply)
{
    {
        float2x2 m1(1, 2, 3, 4);
        float2x2 m2(5, 6, 7, 8);

        auto m = m1;
        m *= m2;
        EXPECT_EQ(m, m1 * m2);
    }
    {
        float3x3 m1(1, 2, 3,
                    4, 5, 6,
                    7, 8, 9);
        float3x3 m2(10, 11, 12,
                    13, 14, 15,
                    16, 17, 18);

        auto m = m1;
        m *= m2;
        EXPECT_EQ(m, m1 * m2);
    }
    {
        float4x4 m1(1, 2, 3, 4,
                    5, 6, 7, 8,
                    9, 10, 11, 12,
                    13, 14, 15, 16);
        float4x4 m2(17, 18, 19, 20,
                    21, 22, 23, 24,
                    25, 26, 27, 28,
                    29, 30, 31, 32);

        auto m = m1;
        m *= m2;
        EXPECT_EQ(m, m1 * m2);
    }
}

TEST(Common_BasicMath, VectorRecast)
{
    EXPECT_EQ(float2(1, 2).Recast<int>(), Vector2<int>(1, 2));
    EXPECT_EQ(float3(1, 2, 3).Recast<int>(), Vector3<int>(1, 2, 3));
    EXPECT_EQ(float4(1, 2, 3, 4).Recast<int>(), Vector4<int>(1, 2, 3, 4));
}

TEST(Common_BasicMath, StdFloorCeilVector)
{
    EXPECT_EQ(std::floor(float2(0.1f, 1.2f)), float2(0, 1));
    EXPECT_EQ(std::floor(float3(0.1f, 1.2f, 2.3f)), float3(0, 1, 2));
    EXPECT_EQ(std::floor(float4(0.1f, 1.2f, 2.3f, 3.4f)), float4(0, 1, 2, 3));
    EXPECT_EQ(std::ceil(float2(0.1f, 1.2f)), float2(1, 2));
    EXPECT_EQ(std::ceil(float3(0.1f, 1.2f, 2.3f)), float3(1, 2, 3));
    EXPECT_EQ(std::ceil(float4(0.1f, 1.2f, 2.3f, 3.4f)), float4(1, 2, 3, 4));
}

TEST(Common_BasicMath, FastFloorCeil)
{
    for (float x = 1.f; x < FLT_MAX / 2.f; x *= 2.f)
    {
        EXPECT_EQ(FastFloor(x), x);
        EXPECT_EQ(FastFloor(x + 0.5f), std::floor(x + 0.5f));
        EXPECT_EQ(FastFloor(x - 0.5f), std::floor(x - 0.5f));
        EXPECT_EQ(FastFloor(-x), -x);
        EXPECT_EQ(FastFloor(-x + 0.5f), std::floor(-x + 0.5f));
        EXPECT_EQ(FastFloor(-x - 0.5f), std::floor(-x - 0.5f));

        EXPECT_EQ(FastCeil(x), x);
        EXPECT_EQ(FastCeil(x + 0.5f), std::ceil(x + 0.5f));
        EXPECT_EQ(FastCeil(x - 0.5f), std::ceil(x - 0.5f));
        EXPECT_EQ(FastCeil(-x), -x);
        EXPECT_EQ(FastCeil(-x + 0.5f), std::ceil(-x + 0.5f));
        EXPECT_EQ(FastCeil(-x - 0.5f), std::ceil(-x - 0.5f));
    }

    for (double x = 1.0; x < DBL_MAX / 2.0; x *= 2.0)
    {
        EXPECT_EQ(FastFloor(x), x);
        EXPECT_EQ(FastFloor(x + 0.5), std::floor(x + 0.5));
        EXPECT_EQ(FastFloor(x - 0.5), std::floor(x - 0.5));
        EXPECT_EQ(FastFloor(-x), -x);
        EXPECT_EQ(FastFloor(-x + 0.5), std::floor(-x + 0.5));
        EXPECT_EQ(FastFloor(-x - 0.5), std::floor(-x - 0.5));

        EXPECT_EQ(FastCeil(x), x);
        EXPECT_EQ(FastCeil(x + 0.5), std::ceil(x + 0.5));
        EXPECT_EQ(FastCeil(x - 0.5), std::ceil(x - 0.5));
        EXPECT_EQ(FastCeil(-x), -x);
        EXPECT_EQ(FastCeil(-x + 0.5), std::ceil(-x + 0.5));
        EXPECT_EQ(FastCeil(-x - 0.5), std::ceil(-x - 0.5));
    }
}

TEST(Common_BasicMath, FastFloorCeilVector)
{
    EXPECT_EQ(FastFloor(float2(-0.1f, 1.2f)), float2(-1, 1));
    EXPECT_EQ(FastFloor(float3(-0.1f, 1.f, 2.3f)), float3(-1, 1, 2));
    EXPECT_EQ(FastFloor(float4(-2.f, -0.875f, 0.f, 0.125f)), float4(-2, -1, 0, 0));

    EXPECT_EQ(FastCeil(float2(-0.1f, 1.2f)), float2(0, 2));
    EXPECT_EQ(FastCeil(float3(-0.875f, 1.f, 2.3f)), float3(0, 1, 3));
    EXPECT_EQ(FastCeil(float4(-2.f, -0.875f, 0.f, 0.125f)), float4(-2, 0, 0, 1));
}


TEST(Common_BasicMath, FracVector)
{
    EXPECT_EQ(Frac(float2(0.125f, 1.25f)), float2(0.125f, 0.25f));
    EXPECT_EQ(Frac(float3(0.125f, 1.25f, -2.25f)), float3(0.125f, 0.25f, 0.75f));
    EXPECT_EQ(Frac(float4(0.125f, 1.25f, -2.25f, -3.0f)), float4(0.125f, 0.25f, 0.75f, 0.f));

    EXPECT_EQ(FastFrac(float2(0.125f, 1.25f)), float2(0.125f, 0.25f));
    EXPECT_EQ(FastFrac(float3(0.125f, 1.25f, -2.25f)), float3(0.125f, 0.25f, 0.75f));
    EXPECT_EQ(FastFrac(float4(0.125f, 1.25f, -2.25f, -3.0f)), float4(0.125f, 0.25f, 0.75f, 0.f));
}


TEST(Common_BasicMath, MinMaxVector)
{
    EXPECT_EQ(max(float3(1, 2, 30), float3(4, 50, 6), float3(70, 8, 9)), float3(70, 50, 30));
    EXPECT_EQ(max(float4(1, 2, 3, 40), float4(5, 6, 70, 8), float4(9, 100, 11, 12), float4(130, 14, 15, 16)), float4(130, 100, 70, 40));

    EXPECT_EQ(min(float3(1, 20, 30), float3(40, 5, 60), float3(70, 80, 9)), float3(1, 5, 9));
    EXPECT_EQ(min(float4(1, 20, 30, 40), float4(50, 6, 70, 80), float4(90, 100, 11, 120), float4(130, 140, 150, 16)), float4(1, 6, 11, 16));
}


TEST(Common_BasicMath, HighPrecisionCross)
{
    {
        int3 v1{INT_MAX, INT_MAX - 1, 0};
        int3 v2{INT_MAX - 1, INT_MAX - 2, 0};
        auto v1xv2 = high_precision_cross(v1, v2);
        EXPECT_EQ(v1xv2, int3(0, 0, -1));
    }

    {
        int3 v1{INT_MAX, 0, INT_MAX - 1};
        int3 v2{INT_MAX - 1, 0, INT_MAX - 2};
        auto v1xv2 = high_precision_cross(v1, v2);
        EXPECT_EQ(v1xv2, int3(0, 1, 0));
    }

    {
        int3 v1{0, INT_MAX, INT_MAX - 1};
        int3 v2{0, INT_MAX - 1, INT_MAX - 2};
        auto v1xv2 = high_precision_cross(v1, v2);
        EXPECT_EQ(v1xv2, int3(-1, 0, 0));
    }

    constexpr float epsilon = 1.f / 32768.f;

    {
        float3 v1{1.f + epsilon, 1.f, 0.f};
        float3 v2{1.f, 1.f - epsilon, 0.f};
        auto   v1xv2    = cross(v1, v2);
        auto   v1xv2_hp = high_precision_cross(v1, v2);
        EXPECT_EQ(v1xv2, float3{});
        EXPECT_EQ(v1xv2_hp, float3(0, 0, -epsilon * epsilon));
    }

    {
        float3 v1{1.f + epsilon, 0.f, 1.f};
        float3 v2{1.f, 0.f, 1.f - epsilon};
        auto   v1xv2    = cross(v1, v2);
        auto   v1xv2_hp = high_precision_cross(v1, v2);
        EXPECT_EQ(v1xv2, float3{});
        EXPECT_EQ(v1xv2_hp, float3(0, epsilon * epsilon, 0));
    }

    {
        float3 v1{0.f, 1.f + epsilon, 1.f};
        float3 v2{0.f, 1.f, 1.f - epsilon};
        auto   v1xv2    = cross(v1, v2);
        auto   v1xv2_hp = high_precision_cross(v1, v2);
        EXPECT_EQ(v1xv2, float3{});
        EXPECT_EQ(v1xv2_hp, float3(-epsilon * epsilon, 0, 0));
    }
}

TEST(Common_AdvancedMath, Planes)
{
    Plane3D plane = {};
    EXPECT_NE(std::hash<Plane3D>{}(plane), size_t{0});

    ViewFrustum frustum = {};
    EXPECT_NE(std::hash<ViewFrustum>{}(frustum), size_t{0});

    ViewFrustumExt frustum_ext = {};
    EXPECT_NE(std::hash<ViewFrustumExt>{}(frustum_ext), size_t{0});
}

TEST(Common_AdvancedMath, HermiteSpline)
{
    EXPECT_NE(HermiteSpline(float3(1, 2, 3), float3(4, 5, 6), float3(7, 8, 9), float3(10, 11, 12), 0.1f), float3(0, 0, 0));
    EXPECT_NE(HermiteSpline(double3(1, 2, 3), double3(4, 5, 6), double3(7, 8, 9), double3(10, 11, 12), 0.1), double3(0, 0, 0));
}

TEST(Common_AdvancedMath, IntersectRayAABB)
{
    BoundBox AABB{float3{2, 4, 6}, float3{4, 8, 12}};
    float3   Center     = (AABB.Min + AABB.Max) * 0.5f;
    float3   HalfExtent = (AABB.Max - AABB.Min) * 0.5f;

    float Enter = 0, Exit = 0;

    // Intersections along axes

    // +X
    EXPECT_TRUE(IntersectRayAABB(Center + HalfExtent * float3{-2.f, 0.25f, 0.125f}, float3{+1, 0, 0}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.x);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.x * 3);
    EXPECT_FALSE(IntersectRayAABB(Center + HalfExtent * float3{-2.f, 0.25f, 0.125f}, float3{-1, 0, 0}, AABB, Enter, Exit));

    // -X
    EXPECT_TRUE(IntersectRayAABB(Center + HalfExtent * float3{+2.f, 0.25f, 0.125f}, float3{-1, 0, 0}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.x);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.x * 3);
    EXPECT_FALSE(IntersectRayAABB(Center + HalfExtent * float3{+2.f, 0.25f, 0.125f}, float3{+1, 0, 0}, AABB, Enter, Exit));

    // +Y
    EXPECT_TRUE(IntersectRayAABB(Center + HalfExtent * float3{0.75f, -2.f, 0.125f}, float3{0, +1, 0}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.y);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.y * 3);
    EXPECT_FALSE(IntersectRayAABB(Center + HalfExtent * float3{0.75f, -2.f, 0.125f}, float3{0, -1, 0}, AABB, Enter, Exit));

    // -Y
    EXPECT_TRUE(IntersectRayAABB(Center + HalfExtent * float3{0.75f, 2.f, 0.125f}, float3{0, -1, 0}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.y);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.y * 3);
    EXPECT_FALSE(IntersectRayAABB(Center + HalfExtent * float3{0.75f, 2.f, 0.125f}, float3{0, +1, 0}, AABB, Enter, Exit));

    // +Z
    EXPECT_TRUE(IntersectRayAABB(Center + HalfExtent * float3{0.75f, 0.5f, -2.f}, float3{0, 0, +1}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.z);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.z * 3);
    EXPECT_FALSE(IntersectRayAABB(Center + HalfExtent * float3{0.75f, 0.5f, -2.f}, float3{0, 0, -1}, AABB, Enter, Exit));

    // -Z
    EXPECT_TRUE(IntersectRayAABB(Center + HalfExtent * float3{0.75f, 0.5f, 2.f}, float3{0, 0, -1}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.z);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.z * 3);
    EXPECT_FALSE(IntersectRayAABB(Center + HalfExtent * float3{0.75f, 0.5f, 2.f}, float3{0, 0, +1}, AABB, Enter, Exit));


    // Origin in the box

    // +X
    EXPECT_TRUE(IntersectRayAABB(Center, float3{1, 0, 0}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.x);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.x);

    // -X
    EXPECT_TRUE(IntersectRayAABB(Center, float3{-1, 0, 0}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.x);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.x);

    // +Y
    EXPECT_TRUE(IntersectRayAABB(Center, float3{0, 1, 0}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.y);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.y);

    // -Y
    EXPECT_TRUE(IntersectRayAABB(Center, float3{0, -1, 0}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.y);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.y);

    // +Z
    EXPECT_TRUE(IntersectRayAABB(Center, float3{0, 0, 1}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.z);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.z);

    // -Z
    EXPECT_TRUE(IntersectRayAABB(Center, float3{0, 0, -1}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.z);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.z);

    const float rsqrt2 = 1.f / std::sqrt(2.f);

    EXPECT_TRUE(IntersectRayAABB(Center + HalfExtent * float3{-1.f, -0.5f, -0.125f} + float3{-rsqrt2, 0, 0}, float3{rsqrt2, rsqrt2, 0}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, 1.f);

    EXPECT_TRUE(IntersectRayAABB(Center + HalfExtent * float3{-0.5f, -1.f, -0.125f} + float3{0, -rsqrt2, 0}, float3{0, rsqrt2, rsqrt2}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, 1.f);

    EXPECT_TRUE(IntersectRayAABB(Center + HalfExtent * float3{-0.125f, -0.5f, -1.f} + float3{0, 0, -rsqrt2}, float3{rsqrt2, 0, rsqrt2}, AABB, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, 1.f);
}

TEST(Common_AdvancedMath, IntersectRayBox2D)
{
    float2 BoxMin{2, 4};
    float2 BoxMax{4, 8};
    float2 Center     = (BoxMin + BoxMax) * 0.5f;
    float2 HalfExtent = (BoxMax - BoxMin) * 0.5f;

    float Enter = 0, Exit = 0;

    // Intersections along axes

    // +X
    EXPECT_TRUE(IntersectRayBox2D(Center + HalfExtent * float2{-2.f, 0.25f}, float2{+1, 0}, BoxMin, BoxMax, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.x);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.x * 3);
    EXPECT_FALSE(IntersectRayBox2D(Center + HalfExtent * float2{-2.f, 0.25f}, float2{-1, 0}, BoxMin, BoxMax, Enter, Exit));

    // -X
    EXPECT_TRUE(IntersectRayBox2D(Center + HalfExtent * float2{+2.f, 0.25f}, float2{-1, 0}, BoxMin, BoxMax, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.x);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.x * 3);
    EXPECT_FALSE(IntersectRayBox2D(Center + HalfExtent * float2{+2.f, 0.25f}, float2{+1, 0}, BoxMin, BoxMax, Enter, Exit));

    // +Y
    EXPECT_TRUE(IntersectRayBox2D(Center + HalfExtent * float2{0.75f, -2.f}, float2{0, +1}, BoxMin, BoxMax, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.y);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.y * 3);
    EXPECT_FALSE(IntersectRayBox2D(Center + HalfExtent * float2{0.75f, -2.f}, float2{0, -1}, BoxMin, BoxMax, Enter, Exit));

    // -Y
    EXPECT_TRUE(IntersectRayBox2D(Center + HalfExtent * float2{0.75f, 2.f}, float2{0, -1}, BoxMin, BoxMax, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, HalfExtent.y);
    EXPECT_FLOAT_EQ(Exit, HalfExtent.y * 3);
    EXPECT_FALSE(IntersectRayBox2D(Center + HalfExtent * float2{0.75f, 2.f}, float2{0, +1}, BoxMin, BoxMax, Enter, Exit));


    // Origin in the box

    // +X
    EXPECT_TRUE(IntersectRayBox2D(Center, float2{1, 0}, BoxMin, BoxMax, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.x);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.x);

    // -X
    EXPECT_TRUE(IntersectRayBox2D(Center, float2{-1, 0}, BoxMin, BoxMax, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.x);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.x);

    // +Y
    EXPECT_TRUE(IntersectRayBox2D(Center, float2{0, 1}, BoxMin, BoxMax, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.y);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.y);

    // -Y
    EXPECT_TRUE(IntersectRayBox2D(Center, float2{0, -1}, BoxMin, BoxMax, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, -HalfExtent.y);
    EXPECT_FLOAT_EQ(Exit, +HalfExtent.y);

    const float rsqrt2 = 1.f / std::sqrt(2.f);
    EXPECT_TRUE(IntersectRayBox2D(Center + HalfExtent * float2{-1.f, -0.5f} + float2{-rsqrt2, 0}, float2{rsqrt2, rsqrt2}, BoxMin, BoxMax, Enter, Exit));
    EXPECT_FLOAT_EQ(Enter, 1.f);
}

TEST(Common_AdvancedMath, IntersectRayTriangle)
{
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{0, -1, -1}, float3{0, +1, -1}, float3{0, 0, +1}, float3{-1, 0, 0}, float3{+1, 0, 0}), 1);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{0, -1, -1}, float3{0, +1, -1}, float3{0, 0, +1}, float3{-1, 0, 0}, float3{+1, 0, 0}, true), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{0, -1, -1}, float3{0, +1, -1}, float3{0, 0, +1}, float3{-1, 0, 0}, float3{-1, 0, 0}), -1);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{0, -1, -1}, float3{0, +1, -1}, float3{0, 0, +1}, float3{-1, 0, 0}, float3{0, +1, 0}), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{0, -1, -1}, float3{0, +1, -1}, float3{0, 0, +1}, float3{-1, 0, 0}, float3{0, -1, 0}), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{0, -1, -1}, float3{0, +1, -1}, float3{0, 0, +1}, float3{-1, 0, 0}, float3{0, 0, +1}), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{0, -1, -1}, float3{0, +1, -1}, float3{0, 0, +1}, float3{-1, 0, 0}, float3{0, 0, -1}), FLT_MAX);

    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{0, -1, -1}, float3{0, +1, -1}, float3{0, 0, +1}, float3{+1, 0, 0}, float3{-1, 0, 0}), 1);

    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, 0, -1}, float3{+1, 0, -1}, float3{0, 0, +1}, float3{0, -1, 0}, float3{0, +1, 0}), 1);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, 0, -1}, float3{+1, 0, -1}, float3{0, 0, +1}, float3{0, -1, 0}, float3{0, -1, 0}), -1);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, 0, -1}, float3{+1, 0, -1}, float3{0, 0, +1}, float3{0, -1, 0}, float3{0, -1, 0}, true), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, 0, -1}, float3{+1, 0, -1}, float3{0, 0, +1}, float3{0, -1, 0}, float3{+1, 0, 0}), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, 0, -1}, float3{+1, 0, -1}, float3{0, 0, +1}, float3{0, -1, 0}, float3{-1, 0, 0}), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, 0, -1}, float3{+1, 0, -1}, float3{0, 0, +1}, float3{0, -1, 0}, float3{0, 0, +1}), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, 0, -1}, float3{+1, 0, -1}, float3{0, 0, +1}, float3{0, -1, 0}, float3{0, 0, -1}), FLT_MAX);

    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, -1, 0}, float3{+1, -1, 0}, float3{0, +1, 0}, float3{0, 0, -1}, float3{0, 0, +1}), 1);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, -1, 0}, float3{+1, -1, 0}, float3{0, +1, 0}, float3{0, 0, -1}, float3{0, 0, +1}, true), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, -1, 0}, float3{+1, -1, 0}, float3{0, +1, 0}, float3{0, 0, -1}, float3{0, 0, -1}), -1);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, -1, 0}, float3{+1, -1, 0}, float3{0, +1, 0}, float3{0, 0, -1}, float3{+1, 0, 0}), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, -1, 0}, float3{+1, -1, 0}, float3{0, +1, 0}, float3{0, 0, -1}, float3{-1, 0, 0}), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, -1, 0}, float3{+1, -1, 0}, float3{0, +1, 0}, float3{0, 0, -1}, float3{0, +1, 0}), FLT_MAX);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-1, -1, 0}, float3{+1, -1, 0}, float3{0, +1, 0}, float3{0, 0, -1}, float3{0, -1, 0}), FLT_MAX);


    const float rsqrt2 = 1.f / std::sqrt(2.f);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{0, -2, -2}, float3{0, +2, -2}, float3{0, 0, +2}, float3{-rsqrt2, 0, 0}, float3{+rsqrt2, +rsqrt2, 0}), 1);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-2, 0, -2}, float3{+2, 0, -2}, float3{0, 0, +2}, float3{0, -rsqrt2, 0}, float3{0, +rsqrt2, +rsqrt2}), 1);
    EXPECT_FLOAT_EQ(IntersectRayTriangle(float3{-2, -2, 0}, float3{+2, -2, 0}, float3{0, +2, 0}, float3{0, 0, -rsqrt2}, float3{+rsqrt2, 0, +rsqrt2}), 1);
}



static void TestLineTrace(float2 Start, float2 End, const std::initializer_list<int2> Reference, int2 GridSize = {10, 10})
{
    auto ref     = Reference.begin();
    bool TraceOK = true;

    std::vector<int2> trace;
    TraceLineThroughGrid(Start, End, GridSize,
                         [&](int2 pos) //
                         {
                             if (ref != Reference.end())
                             {
                                 if (pos != *ref)
                                 {
                                     TraceOK = false;
                                 }
                                 ++ref;
                             }
                             else
                             {
                                 TraceOK = false;
                             }

                             trace.emplace_back(pos);
                             return true;
                         } //
    );

    if (ref != Reference.end())
        TraceOK = false;

    if (!TraceOK)
    {
        std::stringstream ss;
        ss << "Expected: ";
        for (ref = Reference.begin(); ref != Reference.end(); ++ref)
            ss << "(" << ref->x << ", " << ref->y << ") ";
        ss << "\n";

        ss << "Actual:   ";
        for (auto it = trace.begin(); it != trace.end(); ++it)
            ss << "(" << it->x << ", " << it->y << ") ";

        ADD_FAILURE() << "Failed to trace line (" << std::setprecision(3) << Start.x << ", " << Start.y << ") - (" << End.x << ", " << End.y << ") "
                      << "through " << GridSize.x << "x" << GridSize.y << " grid:\n"
                      << ss.str();
    }
}

TEST(Common_AdvancedMath, TraceLineThroughGrid)
{
    // Horizontal direction
    TestLineTrace(float2{0.f, 0.5f}, float2{2.f, 0.5f}, {int2{0, 0}, int2{1, 0}, int2{2, 0}});
    TestLineTrace(float2{-10.f, 0.5f}, float2{2.f, 0.5f}, {int2{0, 0}, int2{1, 0}, int2{2, 0}});
    TestLineTrace(float2{2.f, 0.5f}, float2{-10.f, 0.5f}, {int2{2, 0}, int2{1, 0}, int2{0, 0}});
    TestLineTrace(float2{8.f, 0.5f}, float2{10.f, 0.5f}, {int2{8, 0}, int2{9, 0}});
    TestLineTrace(float2{8.f, 0.5f}, float2{20.f, 0.5f}, {int2{8, 0}, int2{9, 0}});
    TestLineTrace(float2{20.f, 0.5f}, float2{8.f, 0.5f}, {int2{9, 0}, int2{8, 0}});

    // Vertical direction
    TestLineTrace(float2{0.5f, 0.f}, float2{0.5f, 2.f}, {int2{0, 0}, int2{0, 1}, int2{0, 2}});
    TestLineTrace(float2{0.5f, -10.f}, float2{0.5f, 2.f}, {int2{0, 0}, int2{0, 1}, int2{0, 2}});
    TestLineTrace(float2{0.5f, 2.f}, float2{0.5f, -10.f}, {int2{0, 2}, int2{0, 1}, int2{0, 0}});
    TestLineTrace(float2{0.5f, 8.f}, float2{0.5f, 10.f}, {int2{0, 8}, int2{0, 9}});
    TestLineTrace(float2{0.5f, 8.f}, float2{0.5f, 20.f}, {int2{0, 8}, int2{0, 9}});
    TestLineTrace(float2{0.5f, 20.f}, float2{0.5f, 8.f}, {int2{0, 9}, int2{0, 8}});

    // Sub-cell horizontal
    TestLineTrace(float2{5.85f, 5.5f}, float2{5.9f, 5.5f}, {int2{5, 5}});
    TestLineTrace(float2{5.9f, 5.5f}, float2{5.85f, 5.5f}, {int2{5, 5}});
    TestLineTrace(float2{5.05f, 5.5f}, float2{5.1f, 5.5f}, {int2{5, 5}});
    TestLineTrace(float2{5.1f, 5.5f}, float2{5.05f, 5.5f}, {int2{5, 5}});

    // Sub-cell vertical
    TestLineTrace(float2{5.5f, 5.85f}, float2{5.5f, 5.9f}, {int2{5, 5}});
    TestLineTrace(float2{5.5f, 5.9f}, float2{5.5f, 5.85f}, {int2{5, 5}});
    TestLineTrace(float2{5.5f, 5.05f}, float2{5.5f, 5.1f}, {int2{5, 5}});
    TestLineTrace(float2{5.5f, 5.1f}, float2{5.5f, 5.05f}, {int2{5, 5}});

    // Sub-cell diagonal
    TestLineTrace(float2{5.85f, 5.85f}, float2{5.9f, 5.9f}, {int2{5, 5}});
    TestLineTrace(float2{5.9f, 5.9f}, float2{5.85f, 5.85f}, {int2{5, 5}});
    TestLineTrace(float2{5.05f, 5.05f}, float2{5.1f, 5.1f}, {int2{5, 5}});
    TestLineTrace(float2{5.1f, 5.1f}, float2{5.05f, 5.05f}, {int2{5, 5}});
    TestLineTrace(float2{5.85f, 5.05f}, float2{5.9f, 5.1f}, {int2{5, 5}});
    TestLineTrace(float2{5.9f, 5.1f}, float2{5.85f, 5.05f}, {int2{5, 5}});
    TestLineTrace(float2{5.05f, 5.85f}, float2{5.1f, 5.9f}, {int2{5, 5}});
    TestLineTrace(float2{5.1f, 5.9f}, float2{5.05f, 5.85f}, {int2{5, 5}});


    TestLineTrace(float2{0.5f, 0.9f}, float2{1.5f, 1.2f}, {int2{0, 0}, int2{0, 1}, int2{1, 1}});
    TestLineTrace(float2{1.5f, 1.2f}, float2{0.5f, 0.9f}, {int2{1, 1}, int2{0, 1}, int2{0, 0}});

    TestLineTrace(float2{1.5f, 0.9f}, float2{0.5f, 1.2f}, {int2{1, 0}, int2{1, 1}, int2{0, 1}});
    TestLineTrace(float2{0.5f, 1.2f}, float2{1.5f, 0.9f}, {int2{0, 1}, int2{1, 1}, int2{1, 0}});

    TestLineTrace(float2{0.95f, 0.5f}, float2{1.5f, 1.5f}, {int2{0, 0}, int2{1, 0}, int2{1, 1}});
    TestLineTrace(float2{1.5f, 1.5f}, float2{0.95f, 0.5f}, {int2{1, 1}, int2{1, 0}, int2{0, 0}});

    TestLineTrace(float2{0.95f, 1.5f}, float2{1.5f, 0.5f}, {int2{0, 1}, int2{1, 1}, int2{1, 0}});
    TestLineTrace(float2{1.5f, 0.5f}, float2{0.95f, 1.5f}, {int2{1, 0}, int2{1, 1}, int2{0, 1}});

    // Test intersections
    TestLineTrace(float2{-0.1f, 0.85f}, float2{0.35f, -2.f}, {int2{0, 0}});
    TestLineTrace(float2{10.1f, 0.85f}, float2{9.15f, -3.f}, {int2{9, 0}});

    TestLineTrace(float2{0.25f - 5.f, 9.75f - 6.f}, float2{0.25f + 5.f, 9.75f + 6.f}, {int2{0, 9}});
    TestLineTrace(float2{9.75f + 5.f, 9.85f - 6.f}, float2{9.75f - 5.f, 9.85f + 6.f}, {int2{9, 9}});

    // Degenerate line
    TestLineTrace(float2{0.5f, 0.5f}, float2{0.5f, 0.5f}, {int2{0, 0}});
    TestLineTrace(float2{-0.5f, 0.5f}, float2{-0.5f, 0.5f}, {});
    TestLineTrace(float2{10.5f, 0.5f}, float2{10.5f, 0.5f}, {});
    TestLineTrace(float2{0.5f, -0.5f}, float2{0.5f, -0.5f}, {});
    TestLineTrace(float2{0.5f, 10.5f}, float2{0.5f, 10.5f}, {});

    // Some random lines
    TestLineTrace(float2{-2.9f, 0.9f}, float2{2.9f, 1.9f}, {int2{0, 1}, int2{1, 1}, int2{2, 1}});
    TestLineTrace(float2{-2.9f, 0.9f}, float2{3.0f, 1.9f}, {int2{0, 1}, int2{1, 1}, int2{2, 1}, int2{3, 1}});
    TestLineTrace(float2{-2.9f, 0.9f}, float2{3.1f, 1.9f}, {int2{0, 1}, int2{1, 1}, int2{2, 1}, int2{3, 1}});

    TestLineTrace(float2{8.1f, 0.1f}, float2{12.9f, 1.1f}, {int2{8, 0}, int2{9, 0}});

    TestLineTrace(float2{5.1f, -3.1f}, float2{6.1f, 3.1f}, {int2{5, 0}, int2{5, 1}, int2{5, 2}, int2{6, 2}, int2{6, 3}});

    TestLineTrace(float2{5.1f, 8.1f}, float2{7.9f, 12.1f}, {int2{5, 8}, int2{5, 9}, int2{6, 9}});


    // This line makes the algorithm miss the end point. The reason is that at the last step,
    //      abs(t + tx) == abs(t + ty)
    // and choice of horizontal or vertical step is ambiguous. The algorithm chooses vertical step which makes
    // it miss the end point.
    TestLineTrace(float2{1, 3}, float2{3, 1}, {int2{1, 3}, int2{1, 2}, int2{1, 1}, int2{2, 1}});

    // This line is symmetric to previous one but it does not miss the end point because in the case when
    //      abs(t + tx) == abs(t + ty)
    // vertical step turns out to be the right choice.
    // It is either this line or the previous one that will make the algorithm miss the end point depending on
    // whether 'abs(t + tx) < abs(t + ty)' or 'abs(t + tx) <= abs(t + ty)' condition is used.
    TestLineTrace(float2{3, 1}, float2{1, 3}, {int2{3, 1}, int2{2, 1}, int2{2, 2}, int2{1, 2}, int2{1, 3}});
}

TEST(Common_BasicMath, FastFloor)
{
    // float
    EXPECT_EQ(FastFloor(0.f), 0.f);

    EXPECT_EQ(FastFloor(-0.0625f), -1.f);
    EXPECT_EQ(FastFloor(-0.975f), -1.f);
    EXPECT_EQ(FastFloor(-1.f), -1.f);
    EXPECT_EQ(FastFloor(-1.125f), -2.f);

    EXPECT_EQ(FastFloor(0.0625f), 0.f);
    EXPECT_EQ(FastFloor(0.975f), 0.f);
    EXPECT_EQ(FastFloor(1.f), 1.f);
    EXPECT_EQ(FastFloor(1.125f), 1.f);

    // double
    EXPECT_EQ(FastFloor(0.0), 0.0);

    EXPECT_EQ(FastFloor(-0.03125), -1.0);
    EXPECT_EQ(FastFloor(-0.96875), -1.0);
    EXPECT_EQ(FastFloor(-1.0), -1.0);
    EXPECT_EQ(FastFloor(-1.03125), -2.0);

    EXPECT_EQ(FastFloor(0.03125), 0.0);
    EXPECT_EQ(FastFloor(0.96875), 0.0);
    EXPECT_EQ(FastFloor(1.0), 1.0);
    EXPECT_EQ(FastFloor(1.03125), 1.0);
}

TEST(Common_BasicMath, FastCeil)
{
    // float
    EXPECT_EQ(FastCeil(0.f), 0.f);

    EXPECT_EQ(FastCeil(-0.0625f), 0.f);
    EXPECT_EQ(FastCeil(-0.975f), 0.f);
    EXPECT_EQ(FastCeil(-1.f), -1.f);
    EXPECT_EQ(FastCeil(-1.125f), -1.f);

    EXPECT_EQ(FastCeil(0.0625f), 1.f);
    EXPECT_EQ(FastCeil(0.975f), 1.f);
    EXPECT_EQ(FastCeil(1.f), 1.f);
    EXPECT_EQ(FastCeil(1.125f), 2.f);

    // double
    EXPECT_EQ(FastCeil(0.0), 0.0);

    EXPECT_EQ(FastCeil(-0.03125), 0.0);
    EXPECT_EQ(FastCeil(-0.96875), 0.0);
    EXPECT_EQ(FastCeil(-1.0), -1.0);
    EXPECT_EQ(FastCeil(-1.03125), -1.0);

    EXPECT_EQ(FastCeil(0.03125), 1.0);
    EXPECT_EQ(FastCeil(0.96875), 1.0);
    EXPECT_EQ(FastCeil(1.0), 1.0);
    EXPECT_EQ(FastCeil(1.03125), 2.0);
}

TEST(Common_BasicMath, Frac)
{
    // float
    EXPECT_EQ(Frac(0.f), 0.f);

    EXPECT_EQ(Frac(-0.0625f), 1.f - 0.0625f);
    EXPECT_EQ(Frac(-0.975f), 1.f - 0.975f);
    EXPECT_EQ(Frac(-1.f), 0.f);
    EXPECT_EQ(Frac(-1.125f), 1.f - 0.125f);

    EXPECT_EQ(Frac(0.0625f), 0.0625f);
    EXPECT_EQ(Frac(0.975f), 0.975f);
    EXPECT_EQ(Frac(1.f), 0.f);
    EXPECT_EQ(Frac(1.125f), 0.125f);

    // double
    EXPECT_EQ(Frac(0.0), 0.0);

    EXPECT_EQ(Frac(-0.0625), 1 - 0.0625);
    EXPECT_EQ(Frac(-0.975), 1 - 0.975);
    EXPECT_EQ(Frac(-1.0), 0.0);
    EXPECT_EQ(Frac(-1.125), 1.0 - 0.125);

    EXPECT_EQ(Frac(0.0625), 0.0625);
    EXPECT_EQ(Frac(0.975), 0.975);
    EXPECT_EQ(Frac(1.0), 0.0);
    EXPECT_EQ(Frac(1.125), 0.125);
}

TEST(Common_BasicMath, FastFrac)
{
    // float
    EXPECT_EQ(FastFrac(0.f), 0.f);

    EXPECT_EQ(FastFrac(-0.0625f), 1.f - 0.0625f);
    EXPECT_EQ(FastFrac(-0.975f), 1.f - 0.975f);
    EXPECT_EQ(FastFrac(-1.f), 0.f);
    EXPECT_EQ(FastFrac(-1.125f), 1.f - 0.125f);

    EXPECT_EQ(FastFrac(0.0625f), 0.0625f);
    EXPECT_EQ(FastFrac(0.975f), 0.975f);
    EXPECT_EQ(FastFrac(1.f), 0.f);
    EXPECT_EQ(FastFrac(1.125f), 0.125f);

    // double
    EXPECT_EQ(FastFrac(0.0), 0.0);

    EXPECT_EQ(FastFrac(-0.0625), 1 - 0.0625);
    EXPECT_EQ(FastFrac(-0.975), 1 - 0.975);
    EXPECT_EQ(FastFrac(-1.0), 0.0);
    EXPECT_EQ(FastFrac(-1.125), 1.0 - 0.125);

    EXPECT_EQ(FastFrac(0.0625), 0.0625);
    EXPECT_EQ(FastFrac(0.975), 0.975);
    EXPECT_EQ(FastFrac(1.0), 0.0);
    EXPECT_EQ(FastFrac(1.125), 0.125);
}


TEST(Common_BasicMath, BitInterleave16)
{
    for (Uint32 i = 0; i <= 16; ++i)
    {
        for (Uint32 j = 0; j <= 16; ++j)
        {
            Uint16 x = i < 16 ? (1u << i) : 0u;
            Uint16 y = j < 16 ? (1u << j) : 0u;

            Uint32 res = 0;
            res |= x != 0 ? (Uint32{1} << (i * 2u)) : 0;
            res |= y != 0 ? (Uint32{2} << (j * 2u)) : 0;
            EXPECT_EQ(BitInterleave16(x, y), res);
        }
    }
}

TEST(Common_BasicMath, ExtractLSB)
{
    {
        Uint32 Bits = 0;
        EXPECT_EQ(ExtractLSB(Bits), 0U);
        EXPECT_EQ(Bits, 0U);
    }

    for (Uint8 i = 0; i < 8; ++i)
    {
        const Uint8 Bit = 1 << i;

        Uint8 Bits = Bit;
        EXPECT_EQ(ExtractLSB(Bits), Bit);
        EXPECT_EQ(Bits, 0U);
    }

    for (Uint32 i = 0; i < 32; ++i)
    {
        const Uint32 Bit = 1 << i;

        Uint32 Bits = Bit;
        EXPECT_EQ(ExtractLSB(Bits), Bit);
        EXPECT_EQ(Bits, 0U);
    }

    for (Uint64 i = 0; i < 64; ++i)
    {
        const Uint64 Bit = Uint64{1} << i;

        Uint64 Bits = Bit;
        EXPECT_EQ(ExtractLSB(Bits), Bit);
        EXPECT_EQ(Bits, 0U);
    }

    for (Uint32 i = 0; i < 32; ++i)
    {
        for (Uint32 j = i + 1; j < 32; ++j)
        {
            const Uint32 LSB = 1 << i;
            const Uint32 MSB = 1 << j;

            Uint32 Bits = LSB | MSB;
            EXPECT_EQ(ExtractLSB(Bits), LSB);
            EXPECT_EQ(ExtractLSB(Bits), MSB);
            EXPECT_EQ(Bits, 0U);
        }
    }

    {
        enum FLAG_ENUM : Uint32
        {
            FLAG_ENUM_00 = 0x00,
            FLAG_ENUM_01 = 0x01,
            FLAG_ENUM_02 = 0x02,
            FLAG_ENUM_10 = 0x10
        };
        FLAG_ENUM Bits = static_cast<FLAG_ENUM>(FLAG_ENUM_01 | FLAG_ENUM_02 | FLAG_ENUM_10);
        EXPECT_EQ(ExtractLSB(Bits), FLAG_ENUM_01);
        EXPECT_EQ(Bits, static_cast<FLAG_ENUM>(FLAG_ENUM_02 | FLAG_ENUM_10));
        EXPECT_EQ(ExtractLSB(Bits), FLAG_ENUM_02);
        EXPECT_EQ(Bits, FLAG_ENUM_10);
        EXPECT_EQ(ExtractLSB(Bits), FLAG_ENUM_10);
        EXPECT_EQ(Bits, FLAG_ENUM_00);
    }
}

TEST(Common_BasicMath, VectorInserters)
{
    {
        std::stringstream ss;
        ss << float4{1, 20, 300, 4000};
        EXPECT_STREQ(ss.str().c_str(), "float4(1, 20, 300, 4000)");
    }
    {
        std::stringstream ss;
        ss << float3{1, 201, 302};
        EXPECT_STREQ(ss.str().c_str(), "float3(1, 201, 302)");
    }
    {
        std::stringstream ss;
        ss << float2{1, 23};
        EXPECT_STREQ(ss.str().c_str(), "float2(1, 23)");
    }

    {
        std::stringstream ss;
        ss << int4{1, 20, 300, 4000};
        EXPECT_STREQ(ss.str().c_str(), "int4(1, 20, 300, 4000)");
    }
    {
        std::stringstream ss;
        ss << int3{1, 201, 302};
        EXPECT_STREQ(ss.str().c_str(), "int3(1, 201, 302)");
    }
    {
        std::stringstream ss;
        ss << int2{1, 23};
        EXPECT_STREQ(ss.str().c_str(), "int2(1, 23)");
    }

    {
        std::stringstream ss;
        ss << uint4{1, 20, 300, 4000};
        EXPECT_STREQ(ss.str().c_str(), "uint4(1, 20, 300, 4000)");
    }
    {
        std::stringstream ss;
        ss << uint3{1, 201, 302};
        EXPECT_STREQ(ss.str().c_str(), "uint3(1, 201, 302)");
    }
    {
        std::stringstream ss;
        ss << uint2{1, 23};
        EXPECT_STREQ(ss.str().c_str(), "uint2(1, 23)");
    }
}

TEST(Common_BasicMath, VectorAny)
{
    EXPECT_FALSE(any(bool2{false, false}));
    EXPECT_TRUE(any(bool2{true, false}));
    EXPECT_TRUE(any(bool2{false, true}));

    EXPECT_FALSE(any(bool3{false, false, false}));
    EXPECT_TRUE(any(bool3{true, false, false}));
    EXPECT_TRUE(any(bool3{false, true, false}));
    EXPECT_TRUE(any(bool3{false, false, true}));

    EXPECT_FALSE(any(bool4{false, false, false, false}));
    EXPECT_TRUE(any(bool4{true, false, false, false}));
    EXPECT_TRUE(any(bool4{false, true, false, false}));
    EXPECT_TRUE(any(bool4{false, false, true, false}));
    EXPECT_TRUE(any(bool4{false, false, false, true}));

    EXPECT_FALSE(any(int4{0, 0, 0, 0}));
    EXPECT_TRUE(any(int4{1, 0, 0, 0}));
    EXPECT_TRUE(any(uint4{0, 1, 0, 0}));
    EXPECT_TRUE(any(float4{0, 0, 1, 0}));
    EXPECT_TRUE(any(double4{0, 0, 0, 1}));
}

TEST(Common_BasicMath, VectorAll)
{
    EXPECT_TRUE(all(bool2{true, true}));
    EXPECT_FALSE(all(bool2{false, true}));
    EXPECT_FALSE(all(bool2{true, false}));


    EXPECT_TRUE(all(bool3{true, true, true}));
    EXPECT_FALSE(all(bool3{false, true, true}));
    EXPECT_FALSE(all(bool3{true, false, true}));
    EXPECT_FALSE(all(bool3{true, true, false}));

    EXPECT_TRUE(all(bool4{true, true, true, true}));
    EXPECT_FALSE(all(bool4{false, true, true, true}));
    EXPECT_FALSE(all(bool4{true, false, true, true}));
    EXPECT_FALSE(all(bool4{true, true, false, true}));
    EXPECT_FALSE(all(bool4{true, true, true, false}));

    EXPECT_TRUE(all(int4{1, 1, 1, 1}));
    EXPECT_FALSE(all(int4{0, 1, 1, 1}));
    EXPECT_FALSE(all(uint4{1, 0, 1, 1}));
    EXPECT_FALSE(all(float4{1, 1, 0, 1}));
    EXPECT_FALSE(all(double4{1, 1, 1, 0}));
}

TEST(Common_AdvancedMath, IsPointInsideTriangleF)
{
    {
        //              (20,20)
        //               .'|
        //             .'  |
        //    (10,10).'    |
        //            '.   |
        //              '. |
        //                '|
        //               (20,0)

        float2 V0{10, 10};
        float2 V1{20, 20};
        float2 V2{20, 0};
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V0, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V1, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V2, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, (V0 + V1) * 0.5f, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, (V1 + V2) * 0.5f, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, (V2 + V0) * 0.5f, true));

        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V0, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V1, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V2, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, (V0 + V1) * 0.5f, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, (V1 + V2) * 0.5f, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, (V2 + V0) * 0.5f, false));


        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V0 + float2(+0.125, 0), true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V1 + float2(-0.125, -0.25), true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V2 + float2(-0.125, +0.25), true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V0 + float2(+0.125, 0), false));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V1 + float2(-0.125, -0.25), false));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V2 + float2(-0.125, +0.25), false));

        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V0 + float2(-0.125, 0), false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V1 + float2(+0.125, +0.25), false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V2 + float2(+0.125, -0.25), false));

        for (int x = -10; x <= +30; ++x)
        {
            for (int y = -10; y <= +30; ++y)
            {
                bool IsInside = (x + y >= 20) && (y <= x) && (x <= 20);
                bool IsOnEdge = (x + y == 20) || (y == x) || (x == 20);
                EXPECT_EQ(IsPointInsideTriangle(V0, V1, V2, float2(static_cast<float>(x), static_cast<float>(y)), true), IsInside) << "(" << x << ", " << y << ")";
                EXPECT_EQ(IsPointInsideTriangle(V0, V1, V2, float2(static_cast<float>(x), static_cast<float>(y)), false), IsInside && !IsOnEdge) << "(" << x << ", " << y << ")";
            }
        }
    }
}

TEST(Common_AdvancedMath, IsPointInsideTriangleI)
{
    {
        //              (20,20)
        //               .'|
        //             .'  |
        //    (10,10).'    |
        //            '.   |
        //              '. |
        //                '|
        //               (20,0)

        int2 V0{10, 10};
        int2 V1{20, 20};
        int2 V2{20, 0};
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V0, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V1, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V2, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, (V0 + V1) / 2, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, (V1 + V2) / 2, true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, (V2 + V0) / 2, true));

        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V0, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V1, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V2, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, (V0 + V1) / 2, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, (V1 + V2) / 2, false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, (V2 + V0) / 2, false));


        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V0 + int2(1, 0), true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V1 + int2(-1, -2), true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V2 + int2(-1, +2), true));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V0 + int2(+1, 0), false));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V1 + int2(-1, -2), false));
        EXPECT_TRUE(IsPointInsideTriangle(V0, V1, V2, V2 + int2(-1, +2), false));

        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V0 + int2(-1, 0), false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V1 + int2(+1, +2), false));
        EXPECT_FALSE(IsPointInsideTriangle(V0, V1, V2, V2 + int2(+1, -2), false));

        for (int x = -10; x <= +30; ++x)
        {
            for (int y = -10; y <= +30; ++y)
            {
                bool IsInside = (x + y >= 20) && (y <= x) && (x <= 20);
                bool IsOnEdge = (x + y == 20) || (y == x) || (x == 20);
                EXPECT_EQ(IsPointInsideTriangle(V0, V1, V2, int2(x, y), true), IsInside) << "(" << x << ", " << y << ")";
                EXPECT_EQ(IsPointInsideTriangle(V0, V1, V2, int2(x, y), false), IsInside && !IsOnEdge) << "(" << x << ", " << y << ")";
            }
        }
    }
}


static void TestRasterizeTriangle(const float2&                     V0,
                                  const float2&                     V1,
                                  const float2&                     V2,
                                  const std::initializer_list<int2> Reference)
{
    const float2 Verts[] = {V0, V1, V2};

    int ids[] = {0, 1, 2};
    do
    {
        auto ref     = Reference.begin();
        bool TraceOK = true;

        std::vector<int2> trace;
        RasterizeTriangle(Verts[ids[0]], Verts[ids[1]], Verts[ids[2]],
                          [&](int2 pos) //
                          {
                              if (ref != Reference.end())
                              {
                                  if (pos != *ref)
                                  {
                                      TraceOK = false;
                                  }
                                  ++ref;
                              }
                              else
                              {
                                  TraceOK = false;
                              }

                              trace.emplace_back(pos);
                              return true;
                          } //
        );

        if (ref != Reference.end())
            TraceOK = false;

        if (!TraceOK)
        {
            std::stringstream ss;
            ss << "Expected: ";
            for (ref = Reference.begin(); ref != Reference.end(); ++ref)
                ss << "(" << ref->x << ", " << ref->y << ") ";
            ss << "\n";

            ss << "Actual:   ";
            for (auto it = trace.begin(); it != trace.end(); ++it)
                ss << "(" << it->x << ", " << it->y << ") ";

            ADD_FAILURE() << "Failed to rasterize triangle (" << std::setprecision(3) << Verts[ids[0]].x << ", " << Verts[ids[0]].y << ") "
                          << " - (" << Verts[ids[1]].x << ", " << Verts[ids[1]].y << ") "
                          << " - (" << Verts[ids[2]].x << ", " << Verts[ids[2]].y << ")\n"
                          << ss.str();
        }
    } while (std::next_permutation(ids, ids + 3));
}

TEST(Common_AdvancedMath, RasterizeTriangle)
{
    {
        // Empty triangle
        TestRasterizeTriangle(float2(0.5f, 0.5f), float2(0.5f, 0.5f), float2(0.5f, 0.5f),
                              {});
        TestRasterizeTriangle(float2(0.25f, 0.25f), float2(0.785f, 0.5f), float2(0.125f, 0.875f),
                              {});
        TestRasterizeTriangle(float2(0.25f, 0.25f), float2(0.785f, 2.5f), float2(0.125f, 10.875f),
                              {});
        TestRasterizeTriangle(float2(0.25f, 0.5f), float2(5.125f, 5.5f), float2(1.875f, 2.125f),
                              {});
        TestRasterizeTriangle(float2(0.5f, 0.5f), float2(2.5f, 0.5f), float2(5.5f, 0.5f),
                              {});
        TestRasterizeTriangle(float2(0.5f, 0.5f), float2(0.5f, 2.5f), float2(0.5f, 5.5f),
                              {});

        TestRasterizeTriangle(float2(0.25f, 0.0f), float2(0.5f, 1.f), float2(0.75f, 0.f),
                              {});
    }

    {
        // Horizontal degenerate
        TestRasterizeTriangle(float2(1.f, 1.f), float2(3.f, 1.f), float2(5.f, 1.f),
                              {int2{1, 1}, int2{2, 1}, int2{3, 1}, int2{4, 1}, int2{5, 1}});
        TestRasterizeTriangle(float2(0.75f, 1.f), float2(3.125f, 1.f), float2(5.875f, 1.f),
                              {int2{1, 1}, int2{2, 1}, int2{3, 1}, int2{4, 1}, int2{5, 1}});
    }

    {
        // Vertical degenerate
        TestRasterizeTriangle(float2(1.f, 1.f), float2(1.f, 3.f), float2(1.f, 5.f),
                              {int2{1, 1}, int2{1, 2}, int2{1, 3}, int2{1, 4}, int2{1, 5}});
        TestRasterizeTriangle(float2(1.f, 0.125f), float2(1.f, 2.875f), float2(1.f, 5.25f),
                              {int2{1, 1}, int2{1, 2}, int2{1, 3}, int2{1, 4}, int2{1, 5}});
    }

    {
        //  .
        //  |'.
        //  |__'.
        //
        TestRasterizeTriangle(float2(1, 1), float2(1, 3), float2(3, 1),
                              {int2{1, 1}, int2{2, 1}, int2{3, 1}, int2{1, 2}, int2{2, 2}, int2{1, 3}});
        TestRasterizeTriangle(float2(0.75f, 0.875f), float2(1, 3.125f), float2(3.375f, 0.75f),
                              {int2{1, 1}, int2{2, 1}, int2{3, 1}, int2{1, 2}, int2{2, 2}, int2{1, 3}});
    }

    {
        //       .
        //     .'|
        //   .'__|
        //
        TestRasterizeTriangle(float2(1, 1), float2(3, 1), float2(3, 3),
                              {int2{1, 1}, int2{2, 1}, int2{3, 1}, int2{2, 2}, int2{3, 2}, int2{3, 3}});
        TestRasterizeTriangle(float2(0.75f, 0.875f), float2(3.5f, 1), float2(3, 3.25),
                              {int2{1, 1}, int2{2, 1}, int2{3, 1}, int2{2, 2}, int2{3, 2}, int2{3, 3}});
    }

    {
        //    ____
        //   |  .'
        //   |.'
        //
        TestRasterizeTriangle(float2(1, 1), float2(1, 3), float2(3, 3),
                              {int2{1, 1}, int2{1, 2}, int2{2, 2}, int2{1, 3}, int2{2, 3}, int2{3, 3}});
        TestRasterizeTriangle(float2(0.75f, 0.5f), float2(1, 3.75f), float2(3.75f, 3.125f),
                              {int2{1, 1}, int2{1, 2}, int2{2, 2}, int2{1, 3}, int2{2, 3}, int2{3, 3}});
    }

    {
        //   ____
        //   '.  |
        //     '.|
        //
        TestRasterizeTriangle(float2(3, 1), float2(1, 3), float2(3, 3),
                              {int2{3, 1}, int2{2, 2}, int2{3, 2}, int2{1, 3}, int2{2, 3}, int2{3, 3}});
        TestRasterizeTriangle(float2(3.125f, 0.125f), float2(0.5f, 3.125f), float2(3.5f, 3.875f),
                              {int2{3, 1}, int2{2, 2}, int2{3, 2}, int2{1, 3}, int2{2, 3}, int2{3, 3}});
    }

    {
        //                .V2
        //            . '/
        //      V1. '   /
        //        \    /
        //         \  /
        //          \/
        //          V0
        TestRasterizeTriangle(float2(-5.f, -10.f), float2(-6.f, -8.f), float2(-2.f, -4.f),
                              {int2{-5, -10},
                               int2{-5, -9},
                               int2{-6, -8}, int2{-5, -8}, int2{-4, -8},
                               int2{-5, -7}, int2{-4, -7},
                               int2{-4, -6}, int2{-3, -6},
                               int2{-3, -5},
                               int2{-2, -4}});
    }

    {
        //   V2.
        //      \' .
        //       \   ' . V1
        //        \    /
        //         \  /
        //          \/
        //          V0
        TestRasterizeTriangle(float2(5.f, 10.f), float2(6.f, 8.f), float2(2.f, 4.f),
                              {int2{2, 4}, int2{3, 5},
                               int2{3, 6},
                               int2{4, 6},
                               int2{4, 7}, int2{5, 7},
                               int2{4, 8}, int2{5, 8}, int2{6, 8},
                               int2{5, 9},
                               int2{5, 10}});
    }

    {
        //     V1 ______ V2
        //        \    /
        //         \  /
        //          \/
        //          V0
        TestRasterizeTriangle(float2(-5.f, -10.f), float2(-7.f, -8.f), float2(-3.f, -8.f),
                              {int2{-5, -10},
                               int2{-6, -9}, int2{-5, -9}, int2{-4, -9},
                               int2{-7, -8}, int2{-6, -8}, int2{-5, -8}, int2{-4, -8}, int2{-3, -8}});

        TestRasterizeTriangle(float2(-5.f, -10.25f), float2(-7.875f, -7.25f), float2(-2.5f, -7.875f),
                              {int2{-5, -10},
                               int2{-6, -9}, int2{-5, -9}, int2{-4, -9},
                               int2{-7, -8}, int2{-6, -8}, int2{-5, -8}, int2{-4, -8}, int2{-3, -8}});
    }

    {
        //         V2
        //         /\
        //        /  \
        //       /____\
        //     V0      V1
        TestRasterizeTriangle(float2(-5.f, -10.f), float2(-3.f, -8.f), float2(-1.f, -10.f),
                              {int2{-5, -10}, int2{-4, -10}, int2{-3, -10}, int2{-2, -10}, int2{-1, -10},
                               int2{-4, -9}, int2{-3, -9}, int2{-2, -9},
                               int2{-3, -8}});

        TestRasterizeTriangle(float2(-5.875f, -10.5f), float2(-2.875f, -7.125f), float2(-0.25f, -10.75f),
                              {int2{-5, -10}, int2{-4, -10}, int2{-3, -10}, int2{-2, -10}, int2{-1, -10},
                               int2{-4, -9}, int2{-3, -9}, int2{-2, -9},
                               int2{-3, -8}});
    }
}

TEST(Common_AdvancedMath, TransformBoundBox)
{
    BoundBox BB;
    BB.Min = float3{1, 2, 3};
    BB.Max = float3{2, 4, 6};

    {
        float3 Offset{2.5f, 3.125f, -7.25f};
        auto   TransformedBB = BB.Transform(float4x4::Translation(Offset));
        EXPECT_EQ(TransformedBB.Min, BB.Min + Offset);
        EXPECT_EQ(TransformedBB.Max, BB.Max + Offset);
    }

    {
        float3 Scale{0.5f, 2.125f, 1.25f};
        auto   TransformedBB = BB.Transform(float4x4::Scale(Scale));
        EXPECT_EQ(TransformedBB.Min, BB.Min * Scale);
        EXPECT_EQ(TransformedBB.Max, BB.Max * Scale);
    }

    {
        // clang-format off
        float4x4 Rotation
        {
            0,  1,  0,  0,
            0,  0,  1,  0,
           -1,  0,  0,  0,
            0,  0,  0,  1
        };
        // clang-format on
        auto TransformedBB = BB.Transform(Rotation);
        EXPECT_EQ(TransformedBB.Min, float3(-6, 1, 2));
        EXPECT_EQ(TransformedBB.Max, float3(-3, 2, 4));
    }
}

TEST(Common_AdvancedMath, CheckBox2DBox2DOverlap)
{
    // clang-format off
    // Self-intersection
    EXPECT_TRUE(CheckBox2DBox2DOverlap<true> (uint2(0, 0), uint2(1, 1), uint2(0, 0), uint2(1, 1)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<false>(uint2(0, 0), uint2(1, 1), uint2(0, 0), uint2(1, 1)));

    // One box fully inside another
    EXPECT_TRUE(CheckBox2DBox2DOverlap<true> (uint2(0, 0), uint2(10, 10), uint2(1, 1), uint2( 2,  2)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<true> (uint2(1, 1), uint2( 2,  2), uint2(0, 0), uint2(10, 10)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<false>(uint2(0, 0), uint2(10, 10), uint2(1, 1), uint2( 2,  2)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<false>(uint2(1, 1), uint2( 2, 2 ), uint2(0, 0), uint2(10, 10)));

    // Touching corners
    EXPECT_TRUE (CheckBox2DBox2DOverlap<true> (int2(0, 0), int2(10, 10), int2(-1, -1), int2( 0,  0)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(int2(0, 0), int2(10, 10), int2(-1, -1), int2( 0,  0)));
    EXPECT_TRUE (CheckBox2DBox2DOverlap<true> (int2(0, 0), int2(10, 10), int2(-1, 10), int2( 0, 11)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(int2(0, 0), int2(10, 10), int2(-1, 10), int2( 0, 11)));
    EXPECT_TRUE (CheckBox2DBox2DOverlap<true> (int2(0, 0), int2(10, 10), int2(10, 10), int2(11, 11)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(int2(0, 0), int2(10, 10), int2(10, 10), int2(11, 11)));
    EXPECT_TRUE (CheckBox2DBox2DOverlap<true> (int2(0, 0), int2(10, 10), int2(10, -1), int2(11,  0)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(int2(0, 0), int2(10, 10), int2(10, -1), int2(11,  0)));

    // Intersections
    EXPECT_TRUE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2(-1, -1), float2( 1,  1)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2(-1, -1), float2( 1,  1)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2(-1,  9), float2( 1, 11)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2(-1,  9), float2( 1, 11)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2( 9,  9), float2(11, 11)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2( 9,  9), float2(11, 11)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2( 9, -1), float2(11,  1)));
    EXPECT_TRUE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2( 9, -1), float2(11,  1)));

    // No intersections
    EXPECT_FALSE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2(-2, -2), float2(-1, -1)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2(-2, -2), float2(-1, -1)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2(-2,  5), float2(-1,  6)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2(-2,  5), float2(-1,  6)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2(-2, 11), float2(-1, 12)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2(-2, 11), float2(-1, 12)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2( 5, 11), float2( 6, 12)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2( 5, 11), float2( 6, 12)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2(11, 11), float2(12, 12)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2(11, 11), float2(12, 12)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2(11,  5), float2(12,  6)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2(11,  5), float2(12,  6)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2(11, -2), float2(12, -1)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2(11, -2), float2(12, -1)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<true> (float2(0, 0), float2(10, 10), float2( 5, -2), float2( 6, -1)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(float2(0, 0), float2(10, 10), float2( 5, -2), float2( 6, -1)));

    // Touching boundaries
    EXPECT_TRUE (CheckBox2DBox2DOverlap<true> (double2(0, 0), double2(10, 10), double2(-2,  5), double2( 0,  6)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(double2(0, 0), double2(10, 10), double2(-2,  5), double2( 0,  6)));
    EXPECT_TRUE (CheckBox2DBox2DOverlap<true> (double2(0, 0), double2(10, 10), double2( 5, 10), double2( 6, 12)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(double2(0, 0), double2(10, 10), double2( 5, 10), double2( 6, 12)));
    EXPECT_TRUE (CheckBox2DBox2DOverlap<true> (double2(0, 0), double2(10, 10), double2(10,  5), double2(12,  6)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(double2(0, 0), double2(10, 10), double2(10,  5), double2(12,  6)));
    EXPECT_TRUE (CheckBox2DBox2DOverlap<true> (double2(0, 0), double2(10, 10), double2( 5, -2), double2( 6,  0)));
    EXPECT_FALSE(CheckBox2DBox2DOverlap<false>(double2(0, 0), double2(10, 10), double2( 5, -2), double2( 6,  0)));

    // clang-format on
}

TEST(Common_AdvancedMath, CheckLineSectionOverlap)
{
    EXPECT_FALSE(CheckLineSectionOverlap<true>(0, 10, 12, 22));
    EXPECT_FALSE(CheckLineSectionOverlap<false>(0, 10, 12, 22));
    EXPECT_FALSE(CheckLineSectionOverlap<true>(0, 10, -10, -2));
    EXPECT_FALSE(CheckLineSectionOverlap<false>(0, 10, -10, -2));

    EXPECT_TRUE(CheckLineSectionOverlap<true>(0, 10, 1, 2));
    EXPECT_TRUE(CheckLineSectionOverlap<true>(1, 2, 0, 10));
    EXPECT_TRUE(CheckLineSectionOverlap<true>(0, 10, 8, 10));
    EXPECT_TRUE(CheckLineSectionOverlap<true>(8, 10, 0, 10));
    EXPECT_TRUE(CheckLineSectionOverlap<true>(0, 10, 0, 2));
    EXPECT_TRUE(CheckLineSectionOverlap<true>(0, 2, 0, 10));
    EXPECT_TRUE(CheckLineSectionOverlap<true>(0, 10, 5, 15));
    EXPECT_TRUE(CheckLineSectionOverlap<true>(5, 15, 0, 10));

    EXPECT_TRUE(CheckLineSectionOverlap<false>(0, 10, 1, 2));
    EXPECT_TRUE(CheckLineSectionOverlap<false>(1, 2, 0, 10));
    EXPECT_TRUE(CheckLineSectionOverlap<false>(0, 10, 8, 10));
    EXPECT_TRUE(CheckLineSectionOverlap<false>(8, 10, 0, 10));
    EXPECT_TRUE(CheckLineSectionOverlap<false>(0, 10, 0, 2));
    EXPECT_TRUE(CheckLineSectionOverlap<false>(0, 2, 0, 10));
    EXPECT_TRUE(CheckLineSectionOverlap<false>(0, 10, 5, 15));
    EXPECT_TRUE(CheckLineSectionOverlap<false>(5, 15, 0, 10));

    EXPECT_TRUE(CheckLineSectionOverlap<true>(0, 10, 10, 20));
    EXPECT_TRUE(CheckLineSectionOverlap<true>(10, 20, 0, 10));
    EXPECT_FALSE(CheckLineSectionOverlap<false>(0, 10, 10, 20));
    EXPECT_FALSE(CheckLineSectionOverlap<false>(10, 20, 0, 10));
}


TEST(Common_AdvancedMath, GetBoxVisibilityAgainstPlane)
{
    BoundBox Box{
        float3{1, 2, 4},
        float3{3, 5, 7},
    };
    for (float s = 0.25f; s <= 4.f; s *= 2.f)
    {
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{1 * s, 0, 0}, -0.999f * s}, Box), BoxVisibility::FullyVisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{1 * s, 0, 0}, -1.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{1 * s, 0, 0}, -2.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{1 * s, 0, 0}, -3.001f * s}, Box), BoxVisibility::Invisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{-1 * s, 0, 0}, 0.999f * s}, Box), BoxVisibility::Invisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{-1 * s, 0, 0}, 1.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{-1 * s, 0, 0}, 2.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{-1 * s, 0, 0}, 3.001f * s}, Box), BoxVisibility::FullyVisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 1 * s, 0}, -1.999f * s}, Box), BoxVisibility::FullyVisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 1 * s, 0}, -2.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 1 * s, 0}, -4.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 1 * s, 0}, -5.001f * s}, Box), BoxVisibility::Invisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, -1 * s, 0}, 1.999f * s}, Box), BoxVisibility::Invisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, -1 * s, 0}, 2.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, -1 * s, 0}, 4.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, -1 * s, 0}, 5.001f * s}, Box), BoxVisibility::FullyVisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, 1 * s}, -3.999f * s}, Box), BoxVisibility::FullyVisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, 1 * s}, -4.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, 1 * s}, -6.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, 1 * s}, -7.001f * s}, Box), BoxVisibility::Invisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, -1 * s}, 3.999f * s}, Box), BoxVisibility::Invisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, -1 * s}, 4.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, -1 * s}, 6.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, -1 * s}, 7.001f * s}, Box), BoxVisibility::FullyVisible);
    }
}

TEST(Common_AdvancedMath, GetOrientedBoxVisibilityAgainstPlane)
{
    OrientedBoundingBox Box{
        float3{2, 4, 6},
        {
            float3{1, 0, 0},
            float3{0, 1, 0},
            float3{0, 0, 1},
        },
        {1, 2, 3},
    };
    for (float s = 0.25f; s <= 4.f; s *= 2.f)
    {
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{1 * s, 0, 0}, -0.999f * s}, Box), BoxVisibility::FullyVisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{1 * s, 0, 0}, -1.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{1 * s, 0, 0}, -2.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{1 * s, 0, 0}, -3.001f * s}, Box), BoxVisibility::Invisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{-1 * s, 0, 0}, 0.999f * s}, Box), BoxVisibility::Invisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{-1 * s, 0, 0}, 1.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{-1 * s, 0, 0}, 2.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{-1 * s, 0, 0}, 3.001f * s}, Box), BoxVisibility::FullyVisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 1 * s, 0}, -1.999f * s}, Box), BoxVisibility::FullyVisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 1 * s, 0}, -2.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 1 * s, 0}, -5.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 1 * s, 0}, -6.001f * s}, Box), BoxVisibility::Invisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, -1 * s, 0}, 1.999f * s}, Box), BoxVisibility::Invisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, -1 * s, 0}, 2.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, -1 * s, 0}, 5.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, -1 * s, 0}, 6.001f * s}, Box), BoxVisibility::FullyVisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, 1 * s}, -2.999f * s}, Box), BoxVisibility::FullyVisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, 1 * s}, -3.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, 1 * s}, -8.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, 1 * s}, -9.001f * s}, Box), BoxVisibility::Invisible);

        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, -1 * s}, 2.999f * s}, Box), BoxVisibility::Invisible);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, -1 * s}, 3.001f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, -1 * s}, 8.999f * s}, Box), BoxVisibility::Intersecting);
        EXPECT_EQ(GetBoxVisibilityAgainstPlane(Plane3D{float3{0, 0, -1 * s}, 9.001f * s}, Box), BoxVisibility::FullyVisible);
    }
}

TEST(Common_AdvancedMath, GetPointToBoxDistance)
{
    BoundBox Box{float3{1, 2, 3}, float3{4, 5, 6}};

    EXPECT_EQ(GetPointToBoxDistance(Box, float3{0, 3, 4}), 1.f);
    EXPECT_EQ(GetPointToBoxDistance(Box, float3{6, 3, 4}), 2.f);
    EXPECT_EQ(GetPointToBoxDistance(Box, float3{3, 0, 4}), 2.f);
    EXPECT_EQ(GetPointToBoxDistance(Box, float3{3, 8, 4}), 3.f);
    EXPECT_EQ(GetPointToBoxDistance(Box, float3{3, 4, 1}), 2.f);
    EXPECT_EQ(GetPointToBoxDistance(Box, float3{3, 4, 9}), 3.f);
    EXPECT_EQ(GetPointToBoxDistanceSqr(Box, float3{0, 0, 0}), 14.f);
    EXPECT_EQ(GetPointToBoxDistanceSqr(Box, float3{5, 6, 7}), 3.f);

    for (float x = -10.f; x <= +10.f; x += 0.5f)
    {
        for (float y = -10.f; y <= +10.f; y += 0.5f)
        {
            for (float z = -10.f; z <= +10.f; z += 0.5f)
            {
                float dx = x < Box.Min.x ?
                    Box.Min.x - x :
                    (x > Box.Max.x ? x - Box.Max.x : 0);
                float dy = y < Box.Min.y ?
                    Box.Min.y - y :
                    (y > Box.Max.y ? y - Box.Max.y : 0);
                float dz = z < Box.Min.z ?
                    Box.Min.z - z :
                    (z > Box.Max.z ? z - Box.Max.z : 0);
                float DistSqr = dx * dx + dy * dy + dz * dz;
                EXPECT_EQ(GetPointToBoxDistanceSqr(Box, float3{x, y, z}), DistSqr);
            }
        }
    }
}

TEST(Common_AdvancedMath, GetPointToOrientedBoxDistance)
{
    OrientedBoundingBox OBB{
        float3{1, -1.5, 3.5},
        {
            float3{1, 0, 0},
            float3{0, -1, 0},
            float3{0, 0, 1},
        },
        {2, 0.5, 1.5},
    };

    BoundBox AABB{float3{-1, -2, 2}, float3{3, -1, 5}};
    for (float x = -10.f; x <= +10.f; x += 0.5f)
    {
        for (float y = -10.f; y <= +10.f; y += 0.5f)
        {
            for (float z = -10.f; z <= +10.f; z += 0.5f)
            {
                EXPECT_EQ(GetPointToBoxDistanceSqr(OBB, float3{x, y, z}), GetPointToBoxDistanceSqr(AABB, float3{x, y, z}));
            }
        }
    }
}


TEST(Common_AdvancedMath, TriangulatePolygon2D)
{
    {
        const std::vector<int2> Verts = {
            {0, 0},
            {1, 0},
            {0, 1}};
        const std::vector<Uint32> RefTris = {0, 1, 2};

        const auto Tris = TriangulatePolygon<Uint32>(Verts);
        EXPECT_EQ(Tris, RefTris);
    }

    for (size_t start_vert = 0; start_vert < 4; ++start_vert)
    {
        std::vector<int2> Verts;
        {
            //   3 ______ 2
            //    |'.    |
            //    |  '.  |
            //    |____'.|
            //   0        1
            const std::vector<int2> BaseVerts = {
                {0, 0},
                {1, 0},
                {1, 1},
                {0, 1}};
            for (size_t i = 0; i < BaseVerts.size(); ++i)
                Verts.push_back(BaseVerts[(start_vert + i) % BaseVerts.size()]);
        }

        const std::vector<Uint32> RefTris = {3, 0, 1, 1, 2, 3};

        const auto Tris = TriangulatePolygon<Uint32>(Verts);
        EXPECT_EQ(Tris, RefTris);
    }

    for (size_t start_vert = 0; start_vert < 4; ++start_vert)
    {
        std::vector<float2> Verts;
        {
            //   1 ______ 2
            //    |'.    |
            //    |  '.  |
            //    |____'.|
            //   0        3
            const std::vector<float2> BaseVerts = {
                {0, 0},
                {0, 1},
                {1, 1},
                {1, 0}};
            for (size_t i = 0; i < BaseVerts.size(); ++i)
                Verts.push_back(BaseVerts[(start_vert + i) % BaseVerts.size()]);
        }
        const std::vector<Uint32> RefTris = {3, 0, 1, 1, 2, 3};

        const auto Tris = TriangulatePolygon<Uint32>(Verts);
        EXPECT_EQ(Tris, RefTris);
    }

    for (size_t start_vert = 0; start_vert < 4; ++start_vert)
    {
        std::vector<int2> Verts;
        {
            //
            //  3.       .1
            //   \'. 2 .'/
            //    \ '.' /
            //     \   /
            //      \ /
            //       0
            //
            const std::vector<int2> BaseVerts = {
                {0, 0},
                {2, 3},
                {0, 2},
                {-1, 4}};

            for (size_t i = 0; i < BaseVerts.size(); ++i)
                Verts.push_back(BaseVerts[(start_vert + i) % BaseVerts.size()]);
        }

        std::vector<Uint32> RefTris;
        switch (start_vert)
        {
            //
            //  3.       .1
            //   \'. 2 .'/
            //    \ '.' /
            //     \   /
            //      \ /
            //       0
            //
            case 0: RefTris = {0, 1, 2, 0, 2, 3}; break;

            //
            //  2.       .0
            //   \'. 1 .'/
            //    \ '.' /
            //     \   /
            //      \ /
            //       3
            //
            case 1: RefTris = {3, 0, 1, 1, 2, 3}; break;

            //
            //  1.       .3
            //   \'. 0 .'/
            //    \ '.' /
            //     \   /
            //      \ /
            //       2
            //
            case 2: RefTris = {0, 1, 2, 0, 2, 3}; break;

            //
            //  0.       .2
            //   \'. 3 .'/
            //    \ '.' /
            //     \   /
            //      \ /
            //       1
            //
            case 3: RefTris = {3, 0, 1, 1, 2, 3}; break;
        }

        const auto Tris = TriangulatePolygon<Uint32>(Verts);
        EXPECT_EQ(Tris, RefTris);
    }

    for (size_t start_vert = 0; start_vert < 4; ++start_vert)
    {
        std::vector<double2> Verts;
        {
            //
            //  1.       .3
            //   \'. 2 .'/
            //    \ '.' /
            //     \   /
            //      \ /
            //       0
            //
            const std::vector<double2> BaseVerts = {
                {0, 0},
                {-1, 4},
                {0, 2},
                {2, 3}};

            for (size_t i = 0; i < BaseVerts.size(); ++i)
                Verts.push_back(BaseVerts[(start_vert + i) % BaseVerts.size()]);
        }

        std::vector<Uint32> RefTris;
        switch (start_vert)
        {
            // Same as above
            case 0: RefTris = {0, 1, 2, 0, 2, 3}; break;
            case 1: RefTris = {3, 0, 1, 1, 2, 3}; break;
            case 2: RefTris = {0, 1, 2, 0, 2, 3}; break;
            case 3: RefTris = {3, 0, 1, 1, 2, 3}; break;
        }

        const auto Tris = TriangulatePolygon<Uint32>(Verts);
        EXPECT_EQ(Tris, RefTris);
    }

    for (size_t start_vert = 0; start_vert < 4; ++start_vert)
    {
        std::vector<int2> Verts;
        {
            //   0
            //    |'.
            //    |  '.
            //    |    '.
            //   1|    .'3
            //    |  .'
            //    |.'
            //   2
            const std::vector<int2> BaseVerts = {
                {0, 1},
                {0, 0},
                {0, -1},
                {1, 0}};
            for (size_t i = 0; i < BaseVerts.size(); ++i)
                Verts.push_back(BaseVerts[(start_vert + i) % BaseVerts.size()]);
        }

        const std::vector<Uint32> RefTris = {3, 0, 1, 1, 2, 3};

        const auto Tris = TriangulatePolygon<Uint32>(Verts);
        EXPECT_EQ(Tris, RefTris);
    }

    for (size_t start_vert = 0; start_vert < 4; ++start_vert)
    {
        std::vector<float2> Verts;
        {
            //   0
            //    |'.
            //    |  '.
            //    |    '.
            //   3|    .'1
            //    |  .'
            //    |.'
            //   2
            const std::vector<float2> BaseVerts = {
                {0, 1},
                {1, 0},
                {0, -1},
                {0, 0}};
            for (size_t i = 0; i < BaseVerts.size(); ++i)
                Verts.push_back(BaseVerts[(start_vert + i) % BaseVerts.size()]);
        }

        const std::vector<Uint32> RefTris = {3, 0, 1, 1, 2, 3};

        const auto Tris = TriangulatePolygon<Uint32>(Verts);
        EXPECT_EQ(Tris, RefTris);
    }

    for (size_t start_vert = 0; start_vert < 6; ++start_vert)
    {
        std::vector<float2> Verts;
        {
            //
            //  4.       .2
            //   |'. 3 .'|
            //   |  '.'  |
            //   |   .   |
            //   |  /0\  |
            //   | /   \ |
            //   |/     \|
            //  5         1
            const std::vector<float2> BaseVerts = {
                {0, -0.125},
                {1, -2},
                {1, +1},
                {0, +0.125},
                {-1, +1},
                {-1, -2}};
            for (size_t i = 0; i < BaseVerts.size(); ++i)
                Verts.push_back(BaseVerts[(start_vert + i) % BaseVerts.size()]);
        }
        std::vector<Uint32> RefTris;
        switch (start_vert)
        {
            case 0: RefTris = {0, 1, 2, 0, 2, 3, 5, 0, 3, 3, 4, 5}; break;

            //  3.       .1
            //   |'. 2 .'|
            //   |  '.'  |
            //   |   .   |
            //   |  /5\  |
            //   | /   \ |
            //   |/     \|
            //  4         0
            case 1: RefTris = {5, 0, 1, 5, 1, 2, 5, 2, 3, 3, 4, 5}; break;

            //  2.       .0
            //   |'. 1 .'|
            //   |  '.'  |
            //   |   .   |
            //   |  /4\  |
            //   | /   \ |
            //   |/     \|
            //  3         5
            case 2: RefTris = {5, 0, 1, 1, 2, 3, 1, 3, 4, 1, 4, 5}; break;

            //  1.       .5
            //   |'. 0 .'|
            //   |  '.'  |
            //   |   .   |
            //   |  /3\  |
            //   | /   \ |
            //   |/     \|
            //  2         4
            case 3: RefTris = {0, 1, 2, 0, 2, 3, 5, 0, 3, 3, 4, 5}; break;

            //  0.       .4
            //   |'. 5 .'|
            //   |  '.'  |
            //   |   .   |
            //   |  /2\  |
            //   | /   \ |
            //   |/     \|
            //  1         3
            case 4: RefTris = {5, 0, 1, 5, 1, 2, 5, 2, 3, 3, 4, 5}; break;

            //  5.       .3
            //   |'. 4 .'|
            //   |  '.'  |
            //   |   .   |
            //   |  /1\  |
            //   | /   \ |
            //   |/     \|
            //  0         2
            case 5: RefTris = {5, 0, 1, 1, 2, 3, 1, 3, 4, 1, 4, 5}; break;
        }

        const auto Tris = TriangulatePolygon<Uint32>(Verts);
        EXPECT_EQ(Tris, RefTris);
    }

    {
        const std::vector<double2> Verts = {
            {251.55066534585424, -239.37523310019236},
            {405.01594942379404, -231.22402215509803},
            {424.89128667213618, -233.76279213022161},
            {354.97745192934343, -200.59163763765051},
            {336.35298518777898, -212.50452779126249},
            {207.88902013106224, -220.28173532753399},
            {165.87167349297070, -158.97831739129651},
            {165.87232914097831, -199.49128333157239},
            {167.04717227593247, -201.15364958147507},
            {170.33697864625216, -214.39292757663975},
            {165.87260207492534, -216.35619205753781},
        };
        const auto Tris = TriangulatePolygon<Uint32>(Verts, false);

        const std::vector<Uint32> RefTris = {1, 2, 3, 1, 3, 4, 0, 1, 4, 0, 4, 5, 10, 0, 5, 5, 6, 7, 5, 7, 8, 5, 8, 9, 5, 9, 10};
        EXPECT_EQ(Tris, RefTris);
    }
}

TEST(Common_AdvancedMath, TriangulatePolygon3D)
{
    for (size_t proj = 0; proj < 3; ++proj)
    {
        std::vector<float3> Verts;
        {
            //
            //  4.       .2
            //   |'. 3 .'|
            //   |  '.'  |
            //   |   .   |
            //   |  /0\  |
            //   | /   \ |
            //   |/     \|
            //  5         1
            const std::vector<float2> BaseVerts = {
                {0, -0.125},
                {1, -2},
                {1, +1},
                {0, +0.125},
                {-1, +1},
                {-1, -2}};
            for (size_t i = 0; i < BaseVerts.size(); ++i)
            {
                const auto v2 = BaseVerts[i];
                switch (proj)
                {
                    case 0: Verts.push_back(float3{v2.x, v2.y, 0.75}); break;
                    case 1: Verts.push_back(float3{v2.x, -2.5, v2.y}); break;
                    case 2: Verts.push_back(float3{10.5, v2.x, v2.y}); break;
                }
            }
        }

        const std::vector<Uint32> RefTris = {0, 1, 2, 0, 2, 3, 5, 0, 3, 3, 4, 5};

        const auto Tris = TriangulatePolygon3D<Uint32>(Verts);
        EXPECT_EQ(Tris, RefTris);
    }
}

} // namespace
