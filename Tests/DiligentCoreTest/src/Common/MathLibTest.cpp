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
}


TEST(Common_BasicMath, Hash)
{
    {
        EXPECT_NE(std::hash<float2>{}(float2{1, 2}), 0);
        EXPECT_NE(std::hash<float3>{}(float3{1, 2, 3}), 0);
        EXPECT_NE(std::hash<float4>{}(float4{1, 2, 3, 5}), 0);
        // clang-format off
        float4x4 m1
        {
            1,   2,  3,  4,
            5,   6,  7,  8,
            9,  10, 11, 12,
            13, 14, 15, 16
        };
        // clang-format on
        EXPECT_NE(std::hash<float4x4>{}(m1), 0);

        // clang-format off
        float3x3 m2
        {
            1,  2,  3,
            5,  6,  7,
            9, 10, 11
        };
        // clang-format on
        EXPECT_NE(std::hash<float3x3>{}(m2), 0);

        // clang-format off
        float2x2 m3
        {
            1, 2,
            5, 6
        };
        // clang-format on
        EXPECT_NE(std::hash<float2x2>{}(m3), 0);
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
    double data[] = {1, 2, 3, 4,
                     5, 6, 7, 8,
                     9, 10, 11, 12,
                     13, 14, 15, 16};
    EXPECT_EQ(float2::MakeVector(data), float2(1, 2));
    EXPECT_EQ(float3::MakeVector(data), float3(1, 2, 3));
    EXPECT_EQ(float4::MakeVector(data), float4(1, 2, 3, 4));
    EXPECT_EQ(Quaternion::MakeQuaternion(data), Quaternion(1, 2, 3, 4));
    EXPECT_EQ(float4x4::MakeMatrix(data), float4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
    EXPECT_EQ(float3x3::MakeMatrix(data), float3x3(1, 2, 3, 4, 5, 6, 7, 8, 9));
    EXPECT_EQ(float2x2::MakeMatrix(data), float2x2(1, 2, 3, 4));
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
    {
        EXPECT_EQ(float2(1, 2).Recast<int>(), Vector2<int>(1, 2));
        EXPECT_EQ(float3(1, 2, 3).Recast<int>(), Vector3<int>(1, 2, 3));
        EXPECT_EQ(float4(1, 2, 3, 4).Recast<int>(), Vector4<int>(1, 2, 3, 4));
    }
}

TEST(Common_BasicMath, StdFloorCeil)
{
    {
        EXPECT_EQ(std::floor(float2(0.1f, 1.2f)), float2(0, 1));
        EXPECT_EQ(std::floor(float3(0.1f, 1.2f, 2.3f)), float3(0, 1, 2));
        EXPECT_EQ(std::floor(float4(0.1f, 1.2f, 2.3f, 3.4f)), float4(0, 1, 2, 3));
        EXPECT_EQ(std::ceil(float2(0.1f, 1.2f)), float2(1, 2));
        EXPECT_EQ(std::ceil(float3(0.1f, 1.2f, 2.3f)), float3(1, 2, 3));
        EXPECT_EQ(std::ceil(float4(0.1f, 1.2f, 2.3f, 3.4f)), float4(1, 2, 3, 4));
    }
}

TEST(Common_AdvancedMath, Planes)
{
    Plane3D plane = {};
    EXPECT_NE(std::hash<Plane3D>{}(plane), 0);

    ViewFrustum frustum = {};
    EXPECT_NE(std::hash<ViewFrustum>{}(frustum), 0);

    ViewFrustumExt frustm_ext = {};
    EXPECT_NE(std::hash<ViewFrustumExt>{}(frustm_ext), 0);
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

} // namespace
