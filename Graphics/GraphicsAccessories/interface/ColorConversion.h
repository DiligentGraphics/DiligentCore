/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#pragma once

/// \file
/// Color conversion functions

#include <cmath>
#include "../../../Primitives/interface/BasicTypes.h"
#include "../../../Common/interface/BasicMath.hpp"

DILIGENT_BEGIN_NAMESPACE(Diligent)

/// Converts linear color to gamma color space

/// \param x - Linear color value in the range [0, 1]
/// \return Gamma color value in the range [0, 1]
///
/// See https://en.wikipedia.org/wiki/SRGB
inline float LinearToGamma(float x)
{
    return x <= 0.0031308 ? x * 12.92f : 1.055f * std::pow(x, 1.f / 2.4f) - 0.055f;
}


/// Converts gamma color to linear color space

/// \param x - Gamma color value in the range [0, 1]
/// \return Linear color value in the range [0, 1]
inline float GammaToLinear(float x)
{
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
}


/// Converts linear color to gamma color space

/// \param x - Linear color value in the range [0, 255]
/// \return Gamma color value in the range [0, 255]
float LinearToGamma(Uint8 x);


/// Converts gamma color to linear color space

/// \param x - Gamma color value in the range [0, 255]
/// \return Linear color value in the range [0, 255]
float GammaToLinear(Uint8 x);


/// Converts linear color to gamma color space using fast approximation

/// \param x - Linear color value in the range [0, 1]
/// \return Gamma color value in the range [0, 1]
inline float FastLinearToGamma(float x)
{
    return x < 0.0031308f ? 12.92f * x : 1.13005f * sqrtf(std::abs(x - 0.00228f)) - 0.13448f * x + 0.005719f;
}


/// Converts gamma color to linear color space using fast approximation

/// \param x - Gamma color value in the range [0, 1]
/// \return Linear color value in the range [0, 1]
inline float FastGammaToLinear(float x)
{
    // http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    return x * (x * (x * 0.305306011f + 0.682171111f) + 0.012522878f);
}


/// Converts RGB color from linear to gamma color space

/// \param RGB - Linear color value in the range [0, 1]
/// \return Gamma color value in the range [0, 1]
inline float3 LinearToSRGB(const float3& RGB)
{
    return float3{LinearToGamma(RGB.r), LinearToGamma(RGB.g), LinearToGamma(RGB.b)};
}


/// Converts RGBA color from linear to gamma color space

/// \param RGBA - Linear color value in the range [0, 1]
/// \return Gamma color value in the range [0, 1]
///
/// \note Alpha channel is not converted
inline float4 LinearToSRGBA(const float4& RGBA)
{
    return float4{LinearToGamma(RGBA.r), LinearToGamma(RGBA.g), LinearToGamma(RGBA.b), RGBA.a};
}


/// Converts RGB color from gamma to linear color space using fast approximation

/// \param RGB - Gamma color value in the range [0, 1]
/// \return Linear color value in the range [0, 1]
inline float3 FastLinearToSRGB(const float3& RGB)
{
    return float3{FastLinearToGamma(RGB.r), FastLinearToGamma(RGB.g), FastLinearToGamma(RGB.b)};
}


/// Converts RGBA color from gamma to linear color space using fast approximation

/// \param RGBA - Gamma color value in the range [0, 1]
/// \return Linear color value in the range [0, 1]
///
/// \note Alpha channel is not converted
inline float4 FastLinearToSRGBA(const float4& RGBA)
{
    return float4{FastLinearToGamma(RGBA.r), FastLinearToGamma(RGBA.g), FastLinearToGamma(RGBA.b), RGBA.a};
}


/// Converts RGB color from gamma to linear color space

/// \param RGB - Gamma color value in the range [0, 1]
/// \return Linear color value in the range [0, 1]
inline float3 SRGBToLinear(const float3& SRGB)
{
    return float3{GammaToLinear(SRGB.r), GammaToLinear(SRGB.g), GammaToLinear(SRGB.b)};
}


/// Converts RGBA color from gamma to linear color space

/// \param RGBA - Gamma color value in the range [0, 1]
/// \return Linear color value in the range [0, 1]
///
/// \note Alpha channel is not converted
inline float4 SRGBAToLinear(const float4& SRGBA)
{
    return float4{GammaToLinear(SRGBA.r), GammaToLinear(SRGBA.g), GammaToLinear(SRGBA.b), SRGBA.a};
}


/// Converts RGB color from gamma to linear color space using fast approximation

/// \param RGB - Gamma color value in the range [0, 1]
/// \return Linear color value in the range [0, 1]
inline float3 FastSRGBToLinear(const float3& SRGB)
{
    return float3{FastGammaToLinear(SRGB.r), FastGammaToLinear(SRGB.g), FastGammaToLinear(SRGB.b)};
}


/// Converts RGBA color from gamma to linear color space using fast approximation

/// \param RGBA - Gamma color value in the range [0, 1]
/// \return Linear color value in the range [0, 1]
inline float4 FastSRGBAToLinear(const float4& SRGBA)
{
    return float4{FastGammaToLinear(SRGBA.r), FastGammaToLinear(SRGBA.g), FastGammaToLinear(SRGBA.b), SRGBA.a};
}

DILIGENT_END_NAMESPACE // namespace Diligent
