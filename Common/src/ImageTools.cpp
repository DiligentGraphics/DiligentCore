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

#include <algorithm>
#include <cmath>

#include "DebugUtilities.hpp"

namespace Diligent
{

void GetImageDifference(Uint32         Width,
                        Uint32         Height,
                        Uint32         NumChannels,
                        const void*    pImage1,
                        Uint32         Stride1,
                        const void*    pImage2,
                        Uint32         Stride2,
                        Uint32         Threshold,
                        ImageDiffInfo& Diff)
{
    Diff = {};

    if (pImage1 == nullptr || pImage2 == nullptr)
    {
        UNEXPECTED("Image pointers cannot be null");
        return;
    }

    if (Stride1 < Width * NumChannels)
    {
        UNEXPECTED("Stride1 is too small. It must be at least ", Width * NumChannels, " bytes long.");
        return;
    }

    if (Stride2 < Width * NumChannels)
    {
        UNEXPECTED("Stride2 is too small. It must be at least ", Width * NumChannels, " bytes long.");
        return;
    }

    for (Uint32 row = 0; row < Height; ++row)
    {
        const Uint8* pRow1 = reinterpret_cast<const Uint8*>(pImage1) + row * Stride1;
        const Uint8* pRow2 = reinterpret_cast<const Uint8*>(pImage2) + row * Stride2;

        for (Uint32 col = 0; col < Width; ++col)
        {
            Uint32 PixelDiff = 0;
            for (Uint32 ch = 0; ch < NumChannels; ++ch)
            {
                const Uint32 ChannelDiff = static_cast<Uint32>(
                    std::abs(static_cast<int>(pRow1[col * NumChannels + ch]) -
                             static_cast<int>(pRow2[col * NumChannels + ch])));
                PixelDiff = std::max(PixelDiff, ChannelDiff);
            }

            if (PixelDiff != 0)
            {
                ++Diff.NumDiffPixels;
                Diff.AvgDiff += static_cast<float>(PixelDiff);
                Diff.RmsDiff += static_cast<float>(PixelDiff * PixelDiff);
                Diff.MaxDiff = std::max(Diff.MaxDiff, PixelDiff);

                if (PixelDiff > Threshold)
                {
                    ++Diff.NumDiffPixelsAboveThreshold;
                }
            }
        }
    }

    if (Diff.NumDiffPixels > 0)
    {
        Diff.AvgDiff /= static_cast<float>(Diff.NumDiffPixels);
        Diff.RmsDiff = std::sqrt(Diff.RmsDiff / static_cast<float>(Diff.NumDiffPixels));
    }
}

void ComputeDifferenceImage(
    Uint32      Width,
    Uint32      Height,
    Uint32      NumChannels,
    const void* pImage1,
    Uint32      Stride1,
    const void* pImage2,
    Uint32      Stride2,
    void*       pDiffImage,
    Uint32      DiffStride,
    Uint32      NumDiffChannels,
    float       Scale)
{
    if (pImage1 == nullptr || pImage2 == nullptr || pDiffImage == nullptr)
    {
        UNEXPECTED("Image pointers cannot be null");
        return;
    }

    if (Stride1 < Width * NumChannels)
    {
        UNEXPECTED("Stride1 is too small. It must be at least ", Width * NumChannels, " bytes long.");
        return;
    }

    if (Stride2 < Width * NumChannels)
    {
        UNEXPECTED("Stride2 is too small. It must be at least ", Width * NumChannels, " bytes long.");
        return;
    }

    if (DiffStride < Width * NumDiffChannels)
    {
        UNEXPECTED("DiffStride is too small. It must be at least ", Width * NumDiffChannels, " bytes long.");
        return;
    }

    if (NumDiffChannels == 0)
    {
        NumDiffChannels = NumChannels;
    }

    for (Uint32 row = 0; row < Height; ++row)
    {
        const Uint8* pRow1    = reinterpret_cast<const Uint8*>(pImage1) + row * Stride1;
        const Uint8* pRow2    = reinterpret_cast<const Uint8*>(pImage2) + row * Stride2;
        Uint8*       pDiffRow = reinterpret_cast<Uint8*>(pDiffImage) + row * DiffStride;

        for (Uint32 col = 0; col < Width; ++col)
        {
            for (Uint32 ch = 0; ch < NumDiffChannels; ++ch)
            {
                int ChannelDiff = ch == 3 ? 255 : 0;
                if (ch < NumChannels)
                {
                    ChannelDiff = std::abs(static_cast<int>(pRow1[col * NumChannels + ch]) - static_cast<int>(pRow2[col * NumChannels + ch]));
                    ChannelDiff = std::min(255, static_cast<int>(ChannelDiff * Scale));
                }
                pDiffRow[col * NumDiffChannels + ch] = static_cast<Uint8>(ChannelDiff);
            }
        }
    }
}

} // namespace Diligent

extern "C"
{
    void Diligent_GetImageDifference(Diligent::Uint32         Width,
                                     Diligent::Uint32         Height,
                                     Diligent::Uint32         NumChannels,
                                     const void*              pImage1,
                                     Diligent::Uint32         Stride1,
                                     const void*              pImage2,
                                     Diligent::Uint32         Stride2,
                                     Diligent::Uint32         Threshold,
                                     Diligent::ImageDiffInfo& ImageDiff)
    {
        Diligent::GetImageDifference(Width, Height, NumChannels, pImage1, Stride1, pImage2, Stride2, Threshold, ImageDiff);
    }

    void Diligent_ComputeDifferenceImage(Diligent::Uint32 Width,
                                         Diligent::Uint32 Height,
                                         Diligent::Uint32 NumChannels,
                                         const void*      pImage1,
                                         Diligent::Uint32 Stride1,
                                         const void*      pImage2,
                                         Diligent::Uint32 Stride2,
                                         void*            pDiffImage,
                                         Diligent::Uint32 DiffStride,
                                         Diligent::Uint32 NumDiffChannels,
                                         float            Scale)
    {
        Diligent::ComputeDifferenceImage(Width, Height, NumChannels, pImage1, Stride1, pImage2, Stride2, pDiffImage, DiffStride, NumDiffChannels, Scale);
    }
}
