/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include <algorithm>
#include <cmath>
#include <limits>

#include "GraphicsUtilities.h"
#include "DebugUtilities.hpp"
#include "GraphicsAccessories.hpp"
#include "ColorConversion.h"

#define PI_F 3.1415926f

namespace Diligent
{

void CreateUniformBuffer(IRenderDevice*   pDevice,
                         Uint32           Size,
                         const Char*      Name,
                         IBuffer**        ppBuffer,
                         USAGE            Usage,
                         BIND_FLAGS       BindFlags,
                         CPU_ACCESS_FLAGS CPUAccessFlags,
                         void*            pInitialData)
{
    BufferDesc CBDesc;
    CBDesc.Name           = Name;
    CBDesc.uiSizeInBytes  = Size;
    CBDesc.Usage          = Usage;
    CBDesc.BindFlags      = BindFlags;
    CBDesc.CPUAccessFlags = CPUAccessFlags;

    BufferData InitialData;
    if (pInitialData != nullptr)
    {
        InitialData.pData    = pInitialData;
        InitialData.DataSize = Size;
    }
    pDevice->CreateBuffer(CBDesc, pInitialData != nullptr ? &InitialData : nullptr, ppBuffer);
}

template <class TConverter>
void GenerateCheckerBoardPatternInternal(Uint32 Width, Uint32 Height, TEXTURE_FORMAT Fmt, Uint32 HorzCells, Uint32 VertCells, Uint8* pData, Uint32 StrideInBytes, TConverter Converter)
{
    const auto& FmtAttribs = GetTextureFormatAttribs(Fmt);
    for (Uint32 y = 0; y < Height; ++y)
    {
        for (Uint32 x = 0; x < Width; ++x)
        {
            float horzWave   = sin((static_cast<float>(x) + 0.5f) / static_cast<float>(Width) * PI_F * static_cast<float>(HorzCells));
            float vertWave   = sin((static_cast<float>(y) + 0.5f) / static_cast<float>(Height) * PI_F * static_cast<float>(VertCells));
            float val        = horzWave * vertWave;
            val              = std::max(std::min(val * 20.f, +1.f), -1.f);
            val              = val * 0.5f + 1.f;
            val              = val * 0.5f + 0.25f;
            Uint8* pDstTexel = pData + x * Uint32{FmtAttribs.NumComponents} * Uint32{FmtAttribs.ComponentSize} + y * StrideInBytes;
            Converter(pDstTexel, Uint32{FmtAttribs.NumComponents}, val);
        }
    }
}

void GenerateCheckerBoardPattern(Uint32 Width, Uint32 Height, TEXTURE_FORMAT Fmt, Uint32 HorzCells, Uint32 VertCells, Uint8* pData, Uint32 StrideInBytes)
{
    const auto& FmtAttribs = GetTextureFormatAttribs(Fmt);
    switch (FmtAttribs.ComponentType)
    {
        case COMPONENT_TYPE_UINT:
        case COMPONENT_TYPE_UNORM:
            GenerateCheckerBoardPatternInternal(
                Width, Height, Fmt, HorzCells, VertCells, pData, StrideInBytes,
                [](Uint8* pDstTexel, Uint32 NumComponents, float fVal) //
                {
                    Uint8 uVal = static_cast<Uint8>(fVal * 255.f);
                    for (Uint32 c = 0; c < NumComponents; ++c)
                        pDstTexel[c] = uVal;
                } //
            );
            break;

        case COMPONENT_TYPE_UNORM_SRGB:
            GenerateCheckerBoardPatternInternal(
                Width, Height, Fmt, HorzCells, VertCells, pData, StrideInBytes,
                [](Uint8* pDstTexel, Uint32 NumComponents, float fVal) //
                {
                    Uint8 uVal = static_cast<Uint8>(FastLinearToSRGB(fVal) * 255.f);
                    for (Uint32 c = 0; c < NumComponents; ++c)
                        pDstTexel[c] = uVal;
                } //
            );
            break;

        case COMPONENT_TYPE_FLOAT:
            GenerateCheckerBoardPatternInternal(
                Width, Height, Fmt, HorzCells, VertCells, pData, StrideInBytes,
                [](Uint8* pDstTexel, Uint32 NumComponents, float fVal) //
                {
                    for (Uint32 c = 0; c < NumComponents; ++c)
                        (reinterpret_cast<float*>(pDstTexel))[c] = fVal;
                } //
            );
            break;

        default:
            UNSUPPORTED("Unsupported component type");
            return;
    }
}



template <typename ChannelType>
ChannelType SRGBAverage(ChannelType c0, ChannelType c1, ChannelType c2, ChannelType c3)
{
    static_assert(std::numeric_limits<ChannelType>::is_integer && !std::numeric_limits<ChannelType>::is_signed, "Unsigned integers are expected");

    static constexpr float MaxVal    = static_cast<float>(std::numeric_limits<ChannelType>::max());
    static constexpr float MaxValInv = 1.f / MaxVal;

    float fc0 = static_cast<float>(c0) * MaxValInv;
    float fc1 = static_cast<float>(c1) * MaxValInv;
    float fc2 = static_cast<float>(c2) * MaxValInv;
    float fc3 = static_cast<float>(c3) * MaxValInv;

    float fLinearAverage = (FastSRGBToLinear(fc0) + FastSRGBToLinear(fc1) + FastSRGBToLinear(fc2) + FastSRGBToLinear(fc3)) * 0.25f;
    float fSRGBAverage   = FastLinearToSRGB(fLinearAverage) * MaxVal;

    // Clamping on both ends is essential because fast SRGB math is imprecise
    fSRGBAverage = std::max(fSRGBAverage, 0.f);
    fSRGBAverage = std::min(fSRGBAverage, MaxVal);

    return static_cast<ChannelType>(fSRGBAverage);
}

template <typename ChannelType>
ChannelType LinearAverage(ChannelType c0, ChannelType c1, ChannelType c2, ChannelType c3);

template <>
Uint8 LinearAverage<Uint8>(Uint8 c0, Uint8 c1, Uint8 c2, Uint8 c3)
{
    return static_cast<Uint8>((static_cast<Uint32>(c0) + static_cast<Uint32>(c1) + static_cast<Uint32>(c2) + static_cast<Uint32>(c3)) >> 2);
}

template <>
Uint16 LinearAverage<Uint16>(Uint16 c0, Uint16 c1, Uint16 c2, Uint16 c3)
{
    return static_cast<Uint16>((static_cast<Uint32>(c0) + static_cast<Uint32>(c1) + static_cast<Uint32>(c2) + static_cast<Uint32>(c3)) >> 2);
}

template <>
Uint32 LinearAverage<Uint32>(Uint32 c0, Uint32 c1, Uint32 c2, Uint32 c3)
{
    return (c0 + c1 + c2 + c3) >> 2;
}

template <>
Int8 LinearAverage<Int8>(Int8 c0, Int8 c1, Int8 c2, Int8 c3)
{
    return static_cast<Int8>((static_cast<Int32>(c0) + static_cast<Int32>(c1) + static_cast<Int32>(c2) + static_cast<Int32>(c3)) / 4);
}

template <>
Int16 LinearAverage<Int16>(Int16 c0, Int16 c1, Int16 c2, Int16 c3)
{
    return static_cast<Int16>((static_cast<Int32>(c0) + static_cast<Int32>(c1) + static_cast<Int32>(c2) + static_cast<Int32>(c3)) / 4);
}

template <>
Int32 LinearAverage<Int32>(Int32 c0, Int32 c1, Int32 c2, Int32 c3)
{
    return (c0 + c1 + c2 + c3) / 4;
}

template <>
float LinearAverage<float>(float c0, float c1, float c2, float c3)
{
    return (c0 + c1 + c2 + c3) * 0.25f;
}

struct ComputeCoarseMipHelper
{
    const Uint32 FineMipWidth;
    const Uint32 FineMipHeight;

    const void* const pFineMip;
    const Uint32      FineMipStride;

    void* const  pCoarseMip;
    const Uint32 CoarseMipStride;

    const Uint32 NumChannels;

    template <typename ChannelType,
              typename AverageFuncType>
    void Run(AverageFuncType ComputeAverage) const
    {
        VERIFY_EXPR(FineMipWidth > 0 && FineMipHeight > 0);
        VERIFY(FineMipHeight == 1 || FineMipStride >= FineMipWidth * sizeof(ChannelType) * NumChannels, "Fine mip level stride is too small");

        const auto CoarseMipWidth  = std::max(FineMipWidth / Uint32{2}, Uint32{1});
        const auto CoarseMipHeight = std::max(FineMipHeight / Uint32{2}, Uint32{1});

        VERIFY(CoarseMipHeight == 1 || CoarseMipStride >= CoarseMipWidth * sizeof(ChannelType) * NumChannels, "Coarse mip level stride is too small");

        for (Uint32 row = 0; row < CoarseMipHeight; ++row)
        {
            auto src_row0 = row * 2;
            auto src_row1 = std::min(row * 2 + 1, FineMipHeight - 1);

            auto pSrcRow0 = reinterpret_cast<const ChannelType*>(reinterpret_cast<const Uint8*>(pFineMip) + src_row0 * FineMipStride);
            auto pSrcRow1 = reinterpret_cast<const ChannelType*>(reinterpret_cast<const Uint8*>(pFineMip) + src_row1 * FineMipStride);

            for (Uint32 col = 0; col < CoarseMipWidth; ++col)
            {
                auto src_col0 = col * 2;
                auto src_col1 = std::min(col * 2 + 1, FineMipWidth - 1);

                for (Uint32 c = 0; c < NumChannels; ++c)
                {
                    const auto Chnl00 = pSrcRow0[src_col0 * NumChannels + c];
                    const auto Chnl01 = pSrcRow0[src_col1 * NumChannels + c];
                    const auto Chnl10 = pSrcRow1[src_col0 * NumChannels + c];
                    const auto Chnl11 = pSrcRow1[src_col1 * NumChannels + c];

                    auto& DstCol = reinterpret_cast<ChannelType*>(reinterpret_cast<Uint8*>(pCoarseMip) + row * CoarseMipStride)[col * NumChannels + c];

                    DstCol = ComputeAverage(Chnl00, Chnl01, Chnl10, Chnl11);
                }
            }
        }
    }
};

void ComputeMipLevel(Uint32         FineLevelWidth,
                     Uint32         FineLevelHeight,
                     TEXTURE_FORMAT Fmt,
                     const void*    pFineLevelData,
                     Uint32         FineDataStrideInBytes,
                     void*          pCoarseLevelData,
                     Uint32         CoarseDataStrideInBytes)
{
    const auto& FmtAttribs = GetTextureFormatAttribs(Fmt);

    ComputeCoarseMipHelper ComputeMipHelper //
        {
            FineLevelWidth,
            FineLevelHeight,
            pFineLevelData,
            FineDataStrideInBytes,
            pCoarseLevelData,
            CoarseDataStrideInBytes,
            FmtAttribs.NumComponents //
        };

    switch (FmtAttribs.ComponentType)
    {
        case COMPONENT_TYPE_UNORM_SRGB:
            VERIFY(FmtAttribs.ComponentSize == 1, "Only 8-bit sRGB formats are expected");
            ComputeMipHelper.Run<Uint8>(SRGBAverage<Uint8>);
            break;

        case COMPONENT_TYPE_UNORM:
        case COMPONENT_TYPE_UINT:
            switch (FmtAttribs.ComponentSize)
            {
                case 1:
                    ComputeMipHelper.Run<Uint8>(LinearAverage<Uint8>);
                    break;

                case 2:
                    ComputeMipHelper.Run<Uint16>(LinearAverage<Uint16>);
                    break;

                case 4:
                    ComputeMipHelper.Run<Uint32>(LinearAverage<Uint32>);
                    break;

                default:
                    UNEXPECTED("Unexpected component size (", FmtAttribs.ComponentSize, ") for UNORM/UINT texture format");
            }
            break;

        case COMPONENT_TYPE_SNORM:
        case COMPONENT_TYPE_SINT:
            switch (FmtAttribs.ComponentSize)
            {
                case 1:
                    ComputeMipHelper.Run<Int8>(LinearAverage<Int8>);
                    break;

                case 2:
                    ComputeMipHelper.Run<Int16>(LinearAverage<Int16>);
                    break;

                case 4:
                    ComputeMipHelper.Run<Int32>(LinearAverage<Int32>);
                    break;

                default:
                    UNEXPECTED("Unexpected component size (", FmtAttribs.ComponentSize, ") for UINT/SINT texture format");
            }
            break;

        case COMPONENT_TYPE_FLOAT:
            VERIFY(FmtAttribs.ComponentSize == 4, "Only 32-bit float formats are currently supported");
            ComputeMipHelper.Run<Float32>(LinearAverage<Float32>);
            break;

        default:
            UNEXPECTED("Unsupported component type");
    }
}

} // namespace Diligent


extern "C"
{
    void Diligent_CreateUniformBuffer(Diligent::IRenderDevice*   pDevice,
                                      Diligent::Uint32           Size,
                                      const Diligent::Char*      Name,
                                      Diligent::IBuffer**        ppBuffer,
                                      Diligent::USAGE            Usage,
                                      Diligent::BIND_FLAGS       BindFlags,
                                      Diligent::CPU_ACCESS_FLAGS CPUAccessFlags,
                                      void*                      pInitialData)
    {
        Diligent::CreateUniformBuffer(pDevice, Size, Name, ppBuffer, Usage, BindFlags, CPUAccessFlags, pInitialData);
    }

    void Diligent_GenerateCheckerBoardPattern(Diligent::Uint32         Width,
                                              Diligent::Uint32         Height,
                                              Diligent::TEXTURE_FORMAT Fmt,
                                              Diligent::Uint32         HorzCells,
                                              Diligent::Uint32         VertCells,
                                              Diligent::Uint8*         pData,
                                              Diligent::Uint32         StrideInBytes)
    {
        Diligent::GenerateCheckerBoardPattern(Width, Height, Fmt, HorzCells, VertCells, pData, StrideInBytes);
    }

    void Diligent_ComputeMipLevel(Diligent::Uint32         FineLevelWidth,
                                  Diligent::Uint32         FineLevelHeight,
                                  Diligent::TEXTURE_FORMAT Fmt,
                                  const void*              pFineLevelData,
                                  Diligent::Uint32         FineDataStrideInBytes,
                                  void*                    pCoarseLevelData,
                                  Diligent::Uint32         CoarseDataStrideInBytes)
    {
        ComputeMipLevel(FineLevelWidth, FineLevelHeight, Fmt, pFineLevelData,
                        FineDataStrideInBytes, pCoarseLevelData, CoarseDataStrideInBytes);
    }
}
