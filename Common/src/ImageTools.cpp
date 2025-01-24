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

void ComputeImageDifference(const ComputeImageDifferenceAttribs& Attribs,
                            ImageDiffInfo&                       Diff)
{
    Diff = {};

    if (Attribs.pImage1 == nullptr || Attribs.pImage2 == nullptr)
    {
        UNEXPECTED("Image pointers cannot be null");
        return;
    }

    if (Attribs.NumChannels1 == 0)
    {
        UNEXPECTED("NumChannels1 cannot be zero");
        return;
    }

    if (Attribs.Stride1 < Attribs.Width * Attribs.NumChannels1)
    {
        UNEXPECTED("Stride1 is too small. It must be at least ", Attribs.Width * Attribs.NumChannels1, " bytes long.");
        return;
    }

    if (Attribs.NumChannels2 == 0)
    {
        UNEXPECTED("NumChannels2 cannot be zero");
        return;
    }
    if (Attribs.Stride2 < Attribs.Width * Attribs.NumChannels2)
    {
        UNEXPECTED("Stride2 is too small. It must be at least ", Attribs.Width * Attribs.NumChannels2, " bytes long.");
        return;
    }

    const Uint32 NumSrcChannels  = std::min(Attribs.NumChannels1, Attribs.NumChannels2);
    const Uint32 NumDiffChannels = Attribs.NumDiffChannels != 0 ? Attribs.NumDiffChannels : NumSrcChannels;
    if (Attribs.pDiffImage != nullptr)
    {
        if (Attribs.DiffStride < Attribs.Width * NumDiffChannels)
        {
            UNEXPECTED("DiffStride is too small. It must be at least ", Attribs.Width * NumDiffChannels, " bytes long.");
            return;
        }
    }

    for (Uint32 row = 0; row < Attribs.Height; ++row)
    {
        const Uint8* pRow1    = reinterpret_cast<const Uint8*>(Attribs.pImage1) + row * Attribs.Stride1;
        const Uint8* pRow2    = reinterpret_cast<const Uint8*>(Attribs.pImage2) + row * Attribs.Stride2;
        Uint8*       pDiffRow = Attribs.pDiffImage != nullptr ? reinterpret_cast<Uint8*>(Attribs.pDiffImage) + row * Attribs.DiffStride : nullptr;

        for (Uint32 col = 0; col < Attribs.Width; ++col)
        {
            Uint32 PixelDiff = 0;
            for (Uint32 ch = 0; ch < NumSrcChannels; ++ch)
            {
                const Uint32 ChannelDiff = static_cast<Uint32>(
                    std::abs(static_cast<int>(pRow1[col * Attribs.NumChannels1 + ch]) -
                             static_cast<int>(pRow2[col * Attribs.NumChannels2 + ch])));
                PixelDiff = std::max(PixelDiff, ChannelDiff);

                if (pDiffRow != nullptr && ch < NumDiffChannels)
                {
                    pDiffRow[col * NumDiffChannels + ch] = static_cast<Uint8>(std::min(ChannelDiff * Attribs.Scale, 255.f));
                }
            }

            if (pDiffRow != nullptr)
            {
                for (Uint32 ch = NumSrcChannels; ch < NumDiffChannels; ++ch)
                {
                    pDiffRow[col * NumDiffChannels + ch] = ch == 3 ? 255 : 0;
                }
            }

            if (PixelDiff != 0)
            {
                ++Diff.NumDiffPixels;
                Diff.AvgDiff += static_cast<float>(PixelDiff);
                Diff.RmsDiff += static_cast<float>(PixelDiff * PixelDiff);
                Diff.MaxDiff = std::max(Diff.MaxDiff, PixelDiff);

                if (PixelDiff > Attribs.Threshold)
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

} // namespace Diligent

extern "C"
{
    void Diligent_ComputeImageDifference(const Diligent::ComputeImageDifferenceAttribs& Attribs,
                                         Diligent::ImageDiffInfo&                       ImageDiff)
    {
        Diligent::ComputeImageDifference(Attribs, ImageDiff);
    }
}
