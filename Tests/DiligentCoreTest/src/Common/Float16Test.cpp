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

#include "Float16.hpp"

#include "gtest/gtest.h"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <iostream>

using namespace Diligent;

namespace
{

float FloatFromBits(uint32_t bits)
{
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

uint32_t BitsFromFloat(float f)
{
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    return bits;
}

bool IsNegZero(float f)
{
    return f == 0.0f && ((BitsFromFloat(f) & 0x80000000u) != 0);
}

// Your implementation ensures:
// - HalfBitsToFloat produces a quiet NaN for half NaNs
// - FloatToHalfBits produces a quiet NaN for float NaNs and forces non-zero payload
//
// This canonicalization makes exhaustive round-trip tests stable.
uint16_t CanonicalizeHalfForRoundTrip(uint16_t h)
{
    const uint16_t sign = h & 0x8000u;
    const uint16_t exp  = (h >> 10) & 0x1Fu;
    uint16_t       mant = h & 0x03FFu;

    if (exp == 0x1Fu && mant != 0)
    {
        // Force quiet bit (mantissa bit 9 in half = 0x0200)
        mant |= 0x0200u;
        // Ensure payload remains non-zero (it already is since mant != 0)
        return static_cast<uint16_t>(sign | 0x7C00u | mant);
    }
    return h;
}

} // namespace

// -------------------- Basic bit-pattern checks --------------------

TEST(Common_Float16, DefaultIsPositiveZero)
{
    Float16 h;
    EXPECT_EQ(h.Raw(), 0x0000u);
    EXPECT_TRUE(h.IsZero());
    EXPECT_FALSE(h.Sign());

    float f = static_cast<float>(h);
    EXPECT_EQ(f, 0.0f);
    EXPECT_FALSE(IsNegZero(f));
}

TEST(Common_Float16, NegativeZeroRoundTrip)
{
    Float16 h{static_cast<uint16_t>(0x8000u)};
    EXPECT_TRUE(h.IsZero());
    EXPECT_TRUE(h.Sign());

    float f = static_cast<float>(h);
    EXPECT_EQ(f, 0.0f);
    EXPECT_TRUE(IsNegZero(f));

    Float16 back{f};
    EXPECT_EQ(back.Raw(), 0x8000u);
}

TEST(Common_Float16, OneAndMinusOne)
{
    Float16 hp{1.0f};
    Float16 hn{-1.0f};

    EXPECT_EQ(hp.Raw(), 0x3C00u); // +1
    EXPECT_EQ(hn.Raw(), 0xBC00u); // -1

    EXPECT_EQ(static_cast<float>(hp), 1.0f);
    EXPECT_EQ(static_cast<float>(hn), -1.0f);
}

TEST(Common_Float16, Infinity)
{
    Float16 pinf{std::numeric_limits<float>::infinity()};
    Float16 ninf{-std::numeric_limits<float>::infinity()};

    EXPECT_EQ(pinf.Raw(), 0x7C00u);
    EXPECT_EQ(ninf.Raw(), 0xFC00u);

    EXPECT_TRUE(std::isinf(static_cast<float>(pinf)));
    EXPECT_TRUE(std::isinf(static_cast<float>(ninf)));
    EXPECT_GT(static_cast<float>(pinf), 0.0f);
    EXPECT_LT(static_cast<float>(ninf), 0.0f);

    // Half bits to float bits check
    EXPECT_EQ(BitsFromFloat(static_cast<float>(pinf)), 0x7F800000u);
    EXPECT_EQ(BitsFromFloat(static_cast<float>(ninf)), 0xFF800000u);
}

TEST(Common_Float16, NaNFromFloat)
{
    // A signaling-ish NaN pattern in float; regardless, input is NaN.
    float fnan = FloatFromBits(0x7FA12345u);
    EXPECT_TRUE(std::isnan(fnan));

    Float16 h{fnan};

    // Must become half NaN:
    EXPECT_EQ(h.Raw() & 0x7C00u, 0x7C00u);
    EXPECT_NE(h.Raw() & 0x03FFu, 0u);

    // Must be quiet NaN in half (0x0200 set)
    EXPECT_NE(h.Raw() & 0x0200u, 0u);

    // Converting back yields float NaN
    float back = static_cast<float>(h);
    EXPECT_TRUE(std::isnan(back));
}

TEST(Common_Float16, NaNFromHalfBecomesQuietInFloat)
{
    // Create a half NaN with quiet bit not set (a "signaling" half NaN payload).
    // exp=all 1s, mant!=0, quiet bit (0x0200) = 0 here.
    Float16 h{static_cast<uint16_t>(0x7C01u)};
    float   f = static_cast<float>(h);
    EXPECT_TRUE(std::isnan(f));

    // Ensure float NaN is quiet (bit 22 of float mantissa set).
    // Note: This matches your `fbits |= 0x00400000u`.
    uint32_t fb = BitsFromFloat(f);
    EXPECT_EQ(fb & 0x7F800000u, 0x7F800000u);
    EXPECT_NE(fb & 0x007FFFFFu, 0u);
    EXPECT_NE(fb & 0x00400000u, 0u);
}

TEST(Common_Float16, MinSubnormalAndMinNormalAndMaxNormal)
{
    // Min positive subnormal: 0x0001 = 2^-24
    Float16 min_sub{static_cast<uint16_t>(0x0001u)};
    float   f_sub = static_cast<float>(min_sub);
    EXPECT_GT(f_sub, 0.0f);
    EXPECT_LT(f_sub, std::ldexp(1.0f, -13)); // definitely tiny
    EXPECT_FLOAT_EQ(f_sub, std::ldexp(1.0f, -24));

    // Min positive normal: 0x0400 = 2^-14
    Float16 min_norm{static_cast<uint16_t>(0x0400u)};
    EXPECT_FLOAT_EQ(static_cast<float>(min_norm), std::ldexp(1.0f, -14));

    // Max normal: 0x7BFF = 65504
    Float16 max_norm{static_cast<uint16_t>(0x7BFFu)};
    EXPECT_FLOAT_EQ(static_cast<float>(max_norm), 65504.0f);

    // Constructing from float should hit same bit patterns
    EXPECT_EQ(Float16{std::ldexp(1.0f, -24)}.Raw(), 0x0001u);
    EXPECT_EQ(Float16{std::ldexp(1.0f, -14)}.Raw(), 0x0400u);
    EXPECT_EQ(Float16{65504.0f}.Raw(), 0x7BFFu);
}

// -------------------- Rounding tests (RN-even) --------------------

TEST(Common_Float16, NormalTiesToEven)
{
    // Half spacing around 1.0 is 2^-10 = 0.0009765625
    // Half ULP at 1.0 is 2^-11 = 0.00048828125

    const float one      = 1.0f;
    const float half_ulp = std::ldexp(1.0f, -11);

    // Exactly halfway between 1.0 and next representable half => tie to even.
    // 1.0 has mantissa LSB=0 at that boundary, so should round to 1.0.
    Float16 h_tie{one + half_ulp};
    EXPECT_EQ(h_tie.Raw(), 0x3C00u);

    // Slightly above halfway should round up to 1.0009765625 (0x3C01)
    Float16 h_up{std::nextafter(one + half_ulp, std::numeric_limits<float>::infinity())};
    EXPECT_EQ(h_up.Raw(), 0x3C01u);

    // Slightly below halfway should round down
    Float16 h_dn{std::nextafter(one + half_ulp, -std::numeric_limits<float>::infinity())};
    EXPECT_EQ(h_dn.Raw(), 0x3C00u);
}

TEST(Common_Float16, MantissaCarryIntoExponent)
{
    // Construct a float just above max mantissa for exponent corresponding to 1.x range.
    // A clean case: nextafter of the largest value with mantissa all 1s before rounding.
    // Pick half just below 2.0: 0x3FFF = 1.9990234375
    // Halfway to 2.0 would round with carry on mantissa overflow.
    Float16     h_max_before_two{static_cast<uint16_t>(0x3FFFu)};
    const float v = static_cast<float>(h_max_before_two);

    // Add half of half-ULP at that exponent (still 2^-11 since exponent for 1.x)
    const float half_ulp = std::ldexp(1.0f, -11);
    Float16     h_tie{v + half_ulp};

    // Tie-to-even: 0x3FFF has mantissa LSB=1, so tie should round to even -> 2.0 (0x4000)
    EXPECT_EQ(h_tie.Raw(), 0x4000u);
    EXPECT_FLOAT_EQ(static_cast<float>(h_tie), 2.0f);
}

TEST(Common_Float16, UnderflowToZeroTieToEven)
{
    // Smallest subnormal is 2^-24.
    // Halfway between +0 and min subnormal is 2^-25.
    // Tie-to-even should go to 0 (even).
    const float halfway = std::ldexp(1.0f, -25);
    Float16     h{halfway};
    EXPECT_EQ(h.Raw(), 0x0000u);

    // Slightly above should become min subnormal
    Float16 hup{std::nextafter(halfway, std::numeric_limits<float>::infinity())};
    EXPECT_EQ(hup.Raw(), 0x0001u);
}

// -------------------- int32 conversion semantics --------------------

TEST(Common_Float16, Int32_NaNBecomesZero)
{
    Float16 h_nan{FloatFromBits(0x7FC00001u)};
    EXPECT_TRUE(std::isnan(static_cast<float>(h_nan)));
    EXPECT_EQ(static_cast<int32_t>(h_nan), 0);
}

TEST(Common_Float16, Int32_InfinitySaturates)
{
    Float16 h_pinf{std::numeric_limits<float>::infinity()};
    Float16 h_ninf{-std::numeric_limits<float>::infinity()};

    EXPECT_EQ(static_cast<int32_t>(h_pinf), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(static_cast<int32_t>(h_ninf), std::numeric_limits<int32_t>::min());
}

TEST(Common_Float16, Int32_TruncatesTowardZero)
{
    Float16 h1{3.9f};
    Float16 h2{-3.9f};
    EXPECT_EQ(static_cast<int32_t>(h1), 3);
    EXPECT_EQ(static_cast<int32_t>(h2), -3);
}

// Note: half finite range doesn't overflow int32, but we still test saturation triggers using inf above.
// Also test exact integer representability boundaries.
TEST(Common_Float16, Int32_ExactIntegers)
{
    Float16 h0{0.0f};
    Float16 h5{5.0f};
    Float16 hm5{-5.0f};

    EXPECT_EQ(static_cast<int32_t>(h0), 0);
    EXPECT_EQ(static_cast<int32_t>(h5), 5);
    EXPECT_EQ(static_cast<int32_t>(hm5), -5);
}

// -------------------- Exhaustive round-trip over all half bit patterns --------------------

TEST(Common_Float16, HalfToFloatToHalfRoundTrip_All65536)
{
    for (uint32_t u = 0; u <= 0xFFFFu; ++u)
    {
        const uint16_t h0 = static_cast<uint16_t>(u);

        Float16 a{h0}; // from raw bits
        float   f = static_cast<float>(a);

        Float16  b{f}; // float -> half
        uint16_t got = b.Raw();

        const uint16_t expect = CanonicalizeHalfForRoundTrip(h0);

        // After canonicalization, the round-trip should match.
        if (got != expect)
        {
            // Provide helpful diagnostics
            ADD_FAILURE() << std::hex
                          << "Mismatch at half bits 0x" << h0
                          << ": float bits 0x" << BitsFromFloat(f)
                          << ", got half 0x" << got
                          << ", expected 0x" << expect
                          << std::dec;
            return;
        }

        // Also sanity: IsZero/Sign should agree with bits
        EXPECT_EQ(a.IsZero(), (h0 & 0x7FFFu) == 0);
        EXPECT_EQ(a.Sign(), (h0 & 0x8000u) != 0);
    }
}

// -------------------- Extra: float->half->float should match "round then expand" exactly --------------------
// This isn't exhaustive over float32 (too big), but it hits many patterns including edge cases.

TEST(Common_Float16, ManyFloatBitPatterns)
{
    // Deterministic LCG to avoid <random> & to keep tests reproducible.
    uint32_t x = 0x12345678u;

    auto next_u32 = [&]() {
        x = x * 1664525u + 1013904223u;
        return x;
    };

    // Include some hand-picked edge cases first.
    const uint32_t cases[] = {
        0x00000000u, 0x80000000u, // +/-0
        0x3F800000u, 0xBF800000u, // +/-1
        0x7F800000u, 0xFF800000u, // +/-inf
        0x7FC00000u, 0xFFC00000u, // qNaN
        0x00800000u, 0x80800000u, // min normal (float)
        0x007FFFFFu, 0x807FFFFFu, // max subnormal (float)
        0x3F000000u, 0x3F7FFFFFu, // ~0.5 to ~1
    };

    for (uint32_t bits : cases)
    {
        float   f = FloatFromBits(bits);
        Float16 h{f};
        float   back = static_cast<float>(h);

        if (std::isnan(f))
        {
            EXPECT_TRUE(std::isnan(back));
        }
        else
        {
            // back must be exactly representable half expanded to float
            // so converting back to half should recover same bits (up to NaN canonicalization which we handled above).
            Float16 h2{back};
            EXPECT_EQ(h2.Raw(), h.Raw());
        }
    }

    // Now pseudo-random patterns
    for (int i = 0; i < 200000; ++i)
    {
        float   f = FloatFromBits(next_u32());
        Float16 h{f};
        float   back = static_cast<float>(h);

        if (std::isnan(f))
        {
            EXPECT_TRUE(std::isnan(back));
            continue;
        }

        // Idempotence: once rounded to half and expanded, repeating should not change bits.
        Float16 h2{back};
        EXPECT_EQ(h2.Raw(), h.Raw());
    }
}
