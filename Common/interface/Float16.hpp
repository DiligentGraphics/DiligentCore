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

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <type_traits>

namespace Diligent
{

class Float16
{
public:
    constexpr Float16() noexcept = default;
    constexpr explicit Float16(uint16_t Bits) noexcept :
        m_Bits{Bits}
    {}

    explicit Float16(float f) noexcept :
        m_Bits(FloatToHalfBits(f))
    {}

    explicit Float16(double d) noexcept :
        m_Bits(DoubleToHalfBits(d))
    {}

    explicit Float16(int32_t i) noexcept :
        m_Bits(FloatToHalfBits(static_cast<float>(i)))
    {}

    explicit operator float() const noexcept
    {
        return HalfBitsToFloat(m_Bits);
    }

    explicit operator double() const noexcept
    {
        return static_cast<double>(HalfBitsToFloat(m_Bits));
    }

    // Int32 conversion: trunc toward 0, saturate on overflow, NaN->0
    explicit operator int32_t() const noexcept
    {
        const float f = HalfBitsToFloat(m_Bits);

        if (std::isnan(f)) return 0;
        if (f >= static_cast<float>(std::numeric_limits<int32_t>::max()))
            return std::numeric_limits<int32_t>::max();
        if (f <= static_cast<float>(std::numeric_limits<int32_t>::min()))
            return std::numeric_limits<int32_t>::min();

        return static_cast<int32_t>(f); // C++ truncates toward 0
    }

    bool     IsZero() const { return (m_Bits & 0x7FFFu) == 0; }
    bool     Sign() const { return (m_Bits >> 15) != 0; }
    uint16_t Raw() const { return m_Bits; }


    static float HalfBitsToFloat(uint16_t h)
    {
        const uint32_t sign = (uint32_t(h) & 0x8000u) << 16;
        const uint32_t exp  = (h >> 10) & 0x1Fu;
        const uint32_t mant = h & 0x03FFu;

        uint32_t fbits = 0;

        if (exp == 0)
        {
            if (mant == 0)
            {
                // +/-0
                fbits = sign;
            }
            else
            {
                // Subnormal: normalize mantissa
                // value = mant * 2^-24
                // Convert to float bits by shifting into float mantissa with adjusted exponent.
                uint32_t m = mant;
                int      e = -14;
                while ((m & 0x0400u) == 0)
                {
                    m <<= 1;
                    --e;
                }
                m &= 0x03FFu;
                const uint32_t exp_f = uint32_t(e + 127);
                fbits                = sign | (exp_f << 23) | (m << 13);
            }
        }
        else if (exp == 0x1F)
        {
            // Inf/NaN
            fbits = sign | 0x7F800000u | (mant << 13);
            if (mant != 0) fbits |= 0x00400000u; // Make sure it's a quiet NaN in float
        }
        else
        {
            // Normal
            const uint32_t exp_f = exp + (127 - 15);
            fbits                = sign | (exp_f << 23) | (mant << 13);
        }

        float out;
        std::memcpy(&out, &fbits, sizeof(out));
        return out;
    }

    static uint16_t DoubleToHalfBits(double d)
    {
        // Convert via float to keep code smaller; every half is exactly representable as float.
        return FloatToHalfBits(static_cast<float>(d));
    }

    // float -> half (binary16), round-to-nearest-even
    static uint16_t FloatToHalfBits(float f)
    {
        uint32_t x;
        std::memcpy(&x, &f, sizeof(x));

        const uint32_t sign = (x >> 16) & 0x8000u;
        uint32_t       exp  = (x >> 23) & 0xFFu;
        uint32_t       mant = x & 0x007FFFFFu;

        // NaN/Inf
        if (exp == 0xFFu)
        {
            if (mant == 0) return static_cast<uint16_t>(sign | 0x7C00u); // Inf
            // Preserve some payload; ensure qNaN
            uint16_t payload = static_cast<uint16_t>(mant >> 13);
            if (payload == 0) payload = 1;
            return static_cast<uint16_t>(sign | 0x7C00u | payload | 0x0200u);
        }

        // Unbias exponent from float, then bias to half
        int32_t e = static_cast<int32_t>(exp) - 127 + 15;

        // Handle subnormals/underflow
        if (e <= 0)
        {
            if (e < -10)
            {
                // Too small -> signed zero
                return static_cast<uint16_t>(sign);
            }

            // Make implicit leading 1 explicit
            mant |= 0x00800000u;

            // Shift to subnormal half mantissa position
            const int shift        = 1 - e; // 1..10
            uint32_t  mant_shifted = mant >> (shift + 13);

            // Round-to-nearest-even using the bits we threw away
            const uint32_t round_mask = (1u << (shift + 13)) - 1u;
            const uint32_t round_bits = mant & round_mask;
            const uint32_t halfway    = 1u << (shift + 12);

            if (round_bits > halfway || (round_bits == halfway && (mant_shifted & 1u)))
                mant_shifted++;

            return static_cast<uint16_t>(sign | static_cast<uint16_t>(mant_shifted));
        }

        // Overflow -> Inf
        if (e >= 31)
        {
            return static_cast<uint16_t>(sign | 0x7C00u);
        }

        // Normal case: round mantissa from 23 to 10 bits
        uint32_t       mant_half  = mant >> 13;
        const uint32_t round_bits = mant & 0x1FFFu; // lower 13 bits

        // Round-to-nearest-even
        if (round_bits > 0x1000u || (round_bits == 0x1000u && (mant_half & 1u)))
        {
            mant_half++;
            if (mant_half == 0x0400u) // mantissa overflow
            {
                mant_half = 0;
                e++;
                if (e >= 31) return static_cast<uint16_t>(sign | 0x7C00u);
            }
        }

        return static_cast<uint16_t>(sign | (static_cast<uint16_t>(e) << 10) | static_cast<uint16_t>(mant_half));
    }

private:
    uint16_t m_Bits{0};
};

} // namespace Diligent
