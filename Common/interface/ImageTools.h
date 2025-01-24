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

#pragma once

/// \file
/// Image processing tools

#include "../../Primitives/interface/BasicTypes.h"


DILIGENT_BEGIN_NAMESPACE(Diligent)

#include "../../Primitives/interface/DefineRefMacro.h"

/// Image difference information
struct ImageDiffInfo
{
    /// The number of pixels that differ
    Uint32 NumDiffPixels DEFAULT_INITIALIZER(0);

    /// The number of pixels that differ above the threshold
    Uint32 NumDiffPixelsAboveThreshold DEFAULT_INITIALIZER(0);

    /// The maximum difference between any two pixels
    Uint32 MaxDiff DEFAULT_INITIALIZER(0);

    /// The average difference between all pixels, not counting pixels that are equal
    float AvgDiff DEFAULT_INITIALIZER(0);

    /// The root mean square difference between all pixels, not counting pixels that are equal
    float RmsDiff DEFAULT_INITIALIZER(0);
};
typedef struct ImageDiffInfo ImageDiffInfo;

/// Compute the difference between two images
///
/// \param [in]  Width        Image width
/// \param [in]  Height       Image height
/// \param [in]  NumChannels  Number of channels in the image
/// \param [in]  pImage1      Pointer to the first image data
/// \param [in]  Stride1      Row stride of the first image data, in bytes
/// \param [in]  pImage2      Pointer to the second image data
/// \param [in]  Stride2      Row stride of the second image data, in bytes
/// \param [in]  Threshold    Difference threshold
/// \return     The image difference information, see Diligent::ImageDiffInfo.
///
/// \remarks    The difference between two pixels is calculated as the maximum of the
///             absolute differences of all channels. The average difference is the
///             average of all differences, not counting pixels that are equal.
///             The root mean square difference is calculated as the square root of
///             the average of the squares of all differences, not counting pixels that
///             are equal.
void DILIGENT_GLOBAL_FUNCTION(GetImageDifference)(
    Uint32            Width,
    Uint32            Height,
    Uint32            NumChannels,
    const void*       pImage1,
    Uint32            Stride1,
    const void*       pImage2,
    Uint32            Stride2,
    Uint32            Threshold,
    ImageDiffInfo REF ImageDiff);

#include "../../Primitives/interface/UndefRefMacro.h"

DILIGENT_END_NAMESPACE // namespace Diligent
