/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "pch.h"
#include "GraphicsUtilities.h"
#include "DebugUtilities.h"
#include "GraphicsAccessories.h"
#include <algorithm>
#include <cmath>

#define PI_F 3.1415926f

namespace Diligent
{

void CreateUniformBuffer( IRenderDevice *pDevice, Uint32 Size, const Char *Name, IBuffer **ppBuffer, USAGE Usage, BIND_FLAGS BindFlags, CPU_ACCESS_FLAGS CPUAccessFlags)
{
    BufferDesc CBDesc;
    CBDesc.Name = Name;
    CBDesc.uiSizeInBytes = Size;
    CBDesc.Usage = Usage;
    CBDesc.BindFlags = BindFlags;
    CBDesc.CPUAccessFlags = CPUAccessFlags;
    pDevice->CreateBuffer( CBDesc, BufferData(), ppBuffer );
}

template<class TConverter>
void GenerateCheckerBoardPatternInternal(Uint32 Width, Uint32 Height, TEXTURE_FORMAT Fmt, Uint32 HorzCells, Uint32 VertCells, Uint8 *pData, Uint32 StrideInBytes, TConverter Converter)
{
    const auto& FmtAttribs = GetTextureFormatAttribs(Fmt);
    for (Uint32 y = 0; y < Height; ++y)
    {
        for (Uint32 x = 0; x < Width; ++x)
        {
            float horzWave = sin((static_cast<float>(x) + 0.5f) / static_cast<float>(Width)  * PI_F * static_cast<float>(HorzCells));
            float vertWave = sin((static_cast<float>(y) + 0.5f) / static_cast<float>(Height) * PI_F * static_cast<float>(VertCells));
            float val = horzWave * vertWave;
            val = std::max( std::min( val*20.f, +1.f), -1.f );
            val = val * 0.5f + 1.f;
            val = val * 0.5f + 0.25f;
            Uint8 *pDstTexel = pData + x * Uint32{FmtAttribs.NumComponents} * Uint32{FmtAttribs.ComponentSize} + y * StrideInBytes;
            Converter(pDstTexel, Uint32{FmtAttribs.NumComponents}, val);
        }
    }
}

static float LinearToSRGB(float x)
{
    // This is exactly the sRGB curve
    //return x < 0.0031308 ? 12.92 * x : 1.055 * pow(std::abs(x), 1.0 / 2.4) - 0.055;

    // This is cheaper but nearly equivalent
    return x < 0.0031308f ? 12.92f * x : 1.13005f * sqrtf(std::abs(x - 0.00228f)) - 0.13448f * x + 0.005719f;
}


void GenerateCheckerBoardPattern(Uint32 Width, Uint32 Height, TEXTURE_FORMAT Fmt, Uint32 HorzCells, Uint32 VertCells, Uint8 *pData, Uint32 StrideInBytes)
{
    const auto& FmtAttribs = GetTextureFormatAttribs(Fmt);
    switch (FmtAttribs.ComponentType)
    {
    case COMPONENT_TYPE_UINT:
    case COMPONENT_TYPE_UNORM:
        GenerateCheckerBoardPatternInternal(Width, Height, Fmt, HorzCells, VertCells, pData, StrideInBytes, 
            [](Uint8 *pDstTexel, Uint32 NumComponents, float fVal)
            {
                Uint8 uVal = static_cast<Uint8>(fVal * 255.f);
                for (Uint32 c = 0; c < NumComponents; ++c)
                    pDstTexel[c] = uVal;
            });
        break;

    case COMPONENT_TYPE_UNORM_SRGB:
        GenerateCheckerBoardPatternInternal(Width, Height, Fmt, HorzCells, VertCells, pData, StrideInBytes, 
            [](Uint8 *pDstTexel, Uint32 NumComponents, float fVal)
            {
                Uint8 uVal = static_cast<Uint8>(  LinearToSRGB(fVal) * 255.f);
                for (Uint32 c = 0; c < NumComponents; ++c)
                    pDstTexel[c] = uVal;
            });
        break;

    case COMPONENT_TYPE_FLOAT:
        GenerateCheckerBoardPatternInternal(Width, Height, Fmt, HorzCells, VertCells, pData, StrideInBytes, 
            [](Uint8 *pDstTexel, Uint32 NumComponents, float fVal)
            {
                for (Uint32 c = 0; c < NumComponents; ++c)
                    (reinterpret_cast<float*>(pDstTexel))[c] = fVal;
            });
        break;

    default:
        UNSUPPORTED("Unsupported component type");
        return;
    }
}
}
