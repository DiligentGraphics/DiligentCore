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

#include <algorithm>
#include <cmath>
#include <limits>
#include <atomic>

#include "GraphicsUtilities.h"
#include "DebugUtilities.hpp"
#include "GraphicsAccessories.hpp"
#include "ColorConversion.h"
#include "RefCntAutoPtr.hpp"

#define PI_F 3.1415926f

namespace Diligent
{

#if D3D11_SUPPORTED
int64_t        GetNativeTextureFormatD3D11(TEXTURE_FORMAT TexFormat);
TEXTURE_FORMAT GetTextureFormatFromNativeD3D11(int64_t NativeFormat);
#endif

#if D3D12_SUPPORTED
int64_t        GetNativeTextureFormatD3D12(TEXTURE_FORMAT TexFormat);
TEXTURE_FORMAT GetTextureFormatFromNativeD3D12(int64_t NativeFormat);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
int64_t        GetNativeTextureFormatGL(TEXTURE_FORMAT TexFormat);
TEXTURE_FORMAT GetTextureFormatFromNativeGL(int64_t NativeFormat);
#endif

#if VULKAN_SUPPORTED
int64_t        GetNativeTextureFormatVk(TEXTURE_FORMAT TexFormat);
TEXTURE_FORMAT GetTextureFormatFromNativeVk(int64_t NativeFormat);
#endif

#if METAL_SUPPORTED
int64_t        GetNativeTextureFormatMtl(TEXTURE_FORMAT TexFormat);
TEXTURE_FORMAT GetTextureFormatFromNativeMtl(int64_t NativeFormat);
#endif

#if WEBGPU_SUPPORTED
int64_t        GetNativeTextureFormatWebGPU(TEXTURE_FORMAT TexFormat);
TEXTURE_FORMAT GetTextureFormatFromNativeWebGPU(int64_t NativeFormat);
#endif

void CreateUniformBuffer(IRenderDevice*   pDevice,
                         Uint64           Size,
                         const Char*      Name,
                         IBuffer**        ppBuffer,
                         USAGE            Usage,
                         BIND_FLAGS       BindFlags,
                         CPU_ACCESS_FLAGS CPUAccessFlags,
                         void*            pInitialData)
{
    if (Usage == USAGE_DEFAULT || Usage == USAGE_IMMUTABLE)
        CPUAccessFlags = CPU_ACCESS_NONE;

    BufferDesc CBDesc;
    CBDesc.Name           = Name;
    CBDesc.Size           = Size;
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
void GenerateCheckerBoardPatternInternal(Uint32 Width, Uint32 Height, TEXTURE_FORMAT Fmt, Uint32 HorzCells, Uint32 VertCells, Uint8* pData, Uint64 StrideInBytes, TConverter Converter)
{
    const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Fmt);
    for (Uint32 y = 0; y < Height; ++y)
    {
        for (Uint32 x = 0; x < Width; ++x)
        {
            float horzWave   = std::sin((static_cast<float>(x) + 0.5f) / static_cast<float>(Width) * PI_F * static_cast<float>(HorzCells));
            float vertWave   = std::sin((static_cast<float>(y) + 0.5f) / static_cast<float>(Height) * PI_F * static_cast<float>(VertCells));
            float val        = horzWave * vertWave;
            val              = std::max(std::min(val * 20.f, +1.f), -1.f);
            val              = val * 0.5f + 1.f;
            val              = val * 0.5f + 0.25f;
            Uint8* pDstTexel = pData + x * size_t{FmtAttribs.NumComponents} * size_t{FmtAttribs.ComponentSize} + y * StrideInBytes;
            Converter(pDstTexel, Uint32{FmtAttribs.NumComponents}, val);
        }
    }
}

void GenerateCheckerBoardPattern(Uint32 Width, Uint32 Height, TEXTURE_FORMAT Fmt, Uint32 HorzCells, Uint32 VertCells, Uint8* pData, Uint64 StrideInBytes)
{
    const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Fmt);
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
                    Uint8 uVal = static_cast<Uint8>(FastLinearToGamma(fVal) * 255.f);
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
ChannelType SRGBAverage(ChannelType c0, ChannelType c1, ChannelType c2, ChannelType c3, Uint32 /*col*/, Uint32 /*row*/)
{
    static_assert(std::numeric_limits<ChannelType>::is_integer && !std::numeric_limits<ChannelType>::is_signed, "Unsigned integers are expected");

    static constexpr float MaxVal    = static_cast<float>(std::numeric_limits<ChannelType>::max());
    static constexpr float MaxValInv = 1.f / MaxVal;

    float fc0 = static_cast<float>(c0) * MaxValInv;
    float fc1 = static_cast<float>(c1) * MaxValInv;
    float fc2 = static_cast<float>(c2) * MaxValInv;
    float fc3 = static_cast<float>(c3) * MaxValInv;

    float fLinearAverage = (FastGammaToLinear(fc0) + FastGammaToLinear(fc1) + FastGammaToLinear(fc2) + FastGammaToLinear(fc3)) * 0.25f;
    float fSRGBAverage   = FastLinearToGamma(fLinearAverage) * MaxVal;

    // Clamping on both ends is essential because fast SRGB math is imprecise
    fSRGBAverage = std::max(fSRGBAverage, 0.f);
    fSRGBAverage = std::min(fSRGBAverage, MaxVal);

    return static_cast<ChannelType>(fSRGBAverage);
}

template <typename ChannelType>
ChannelType LinearAverage(ChannelType c0, ChannelType c1, ChannelType c2, ChannelType c3, Uint32 /*col*/, Uint32 /*row*/);

template <>
Uint8 LinearAverage<Uint8>(Uint8 c0, Uint8 c1, Uint8 c2, Uint8 c3, Uint32 /*col*/, Uint32 /*row*/)
{
    return static_cast<Uint8>((Uint32{c0} + Uint32{c1} + Uint32{c2} + Uint32{c3}) >> 2);
}

template <>
Uint16 LinearAverage<Uint16>(Uint16 c0, Uint16 c1, Uint16 c2, Uint16 c3, Uint32 /*col*/, Uint32 /*row*/)
{
    return static_cast<Uint16>((Uint32{c0} + Uint32{c1} + Uint32{c2} + Uint32{c3}) >> 2);
}

template <>
Uint32 LinearAverage<Uint32>(Uint32 c0, Uint32 c1, Uint32 c2, Uint32 c3, Uint32 /*col*/, Uint32 /*row*/)
{
    return (c0 + c1 + c2 + c3) >> 2;
}

template <>
Int8 LinearAverage<Int8>(Int8 c0, Int8 c1, Int8 c2, Int8 c3, Uint32 /*col*/, Uint32 /*row*/)
{
    return static_cast<Int8>((Int32{c0} + Int32{c1} + Int32{c2} + Int32{c3}) / 4);
}

template <>
Int16 LinearAverage<Int16>(Int16 c0, Int16 c1, Int16 c2, Int16 c3, Uint32 /*col*/, Uint32 /*row*/)
{
    return static_cast<Int16>((Int32{c0} + Int32{c1} + Int32{c2} + Int32{c3}) / 4);
}

template <>
Int32 LinearAverage<Int32>(Int32 c0, Int32 c1, Int32 c2, Int32 c3, Uint32 /*col*/, Uint32 /*row*/)
{
    return (c0 + c1 + c2 + c3) / 4;
}

template <>
float LinearAverage<float>(float c0, float c1, float c2, float c3, Uint32 /*col*/, Uint32 /*row*/)
{
    return (c0 + c1 + c2 + c3) * 0.25f;
}


template <typename ChannelType>
ChannelType MostFrequentSelector(ChannelType c0, ChannelType c1, ChannelType c2, ChannelType c3, Uint32 col, Uint32 row)
{
    //  c2      c3
    //   *      *
    //
    //   *      *
    //  c0      c1
    const auto _01 = c0 == c1;
    const auto _02 = c0 == c2;
    const auto _03 = c0 == c3;
    const auto _12 = c1 == c2;
    const auto _13 = c1 == c3;
    const auto _23 = c2 == c3;
    if (_01)
    {
        //      2     3
        //      *-----*
        //                Use row to pseudo-randomly make selection
        //      *-----*
        //      0     1
        return (!_23 || (row & 0x01) != 0) ? c0 : c2;
    }
    if (_02)
    {
        //      2     3
        //      *     *
        //      |     |   Use col to pseudo-randomly make selection
        //      *     *
        //      0     1
        return (!_13 || (col & 0x01) != 0) ? c0 : c1;
    }
    if (_03)
    {
        //      2     3
        //      *.   .*
        //        '.'
        //       .' '.
        //      *     *
        //      0     1
        return (!_12 || ((col + row) & 0x01) != 0) ? c0 : c1;
    }
    if (_12 || _13)
    {
        //      2     3         2     3
        //      *.    *         *     *
        //        '.                  |
        //          '.                |
        //      *     *         *     *
        //      0     1         0     1
        return c1;
    }
    if (_23)
    {
        //      2     3
        //      *-----*
        //
        //      *     *
        //      0     1
        return c2;
    }

    // Select pseudo-random element
    //      2     3
    //      *     *
    //
    //      *     *
    //      0     1
    switch ((col + row) % 4)
    {
        case 0: return c0;
        case 1: return c1;
        case 2: return c2;
        case 3: return c3;
        default:
            UNEXPECTED("Unexpected index");
            return c0;
    }
}

template <typename ChannelType,
          typename FilterType>
void FilterMipLevel(const ComputeMipLevelAttribs& Attribs,
                    Uint32                        NumChannels,
                    FilterType                    Filter)
{
    VERIFY_EXPR(Attribs.FineMipWidth > 0 && Attribs.FineMipHeight > 0);
    DEV_CHECK_ERR(Attribs.FineMipHeight == 1 || Attribs.FineMipStride >= Attribs.FineMipWidth * sizeof(ChannelType) * NumChannels, "Fine mip level stride is too small");

    const Uint32 CoarseMipWidth  = std::max(Attribs.FineMipWidth / Uint32{2}, Uint32{1});
    const Uint32 CoarseMipHeight = std::max(Attribs.FineMipHeight / Uint32{2}, Uint32{1});

    VERIFY(CoarseMipHeight == 1 || Attribs.CoarseMipStride >= CoarseMipWidth * sizeof(ChannelType) * NumChannels, "Coarse mip level stride is too small");

    for (Uint32 row = 0; row < CoarseMipHeight; ++row)
    {
        Uint32 src_row0 = row * 2;
        Uint32 src_row1 = std::min(row * 2 + 1, Attribs.FineMipHeight - 1);

        const ChannelType* pSrcRow0 = reinterpret_cast<const ChannelType*>(reinterpret_cast<const Uint8*>(Attribs.pFineMipData) + src_row0 * Attribs.FineMipStride);
        const ChannelType* pSrcRow1 = reinterpret_cast<const ChannelType*>(reinterpret_cast<const Uint8*>(Attribs.pFineMipData) + src_row1 * Attribs.FineMipStride);

        for (Uint32 col = 0; col < CoarseMipWidth; ++col)
        {
            Uint32 src_col0 = col * 2;
            Uint32 src_col1 = std::min(col * 2 + 1, Attribs.FineMipWidth - 1);

            for (Uint32 c = 0; c < NumChannels; ++c)
            {
                const ChannelType Chnl00 = pSrcRow0[src_col0 * NumChannels + c];
                const ChannelType Chnl10 = pSrcRow0[src_col1 * NumChannels + c];
                const ChannelType Chnl01 = pSrcRow1[src_col0 * NumChannels + c];
                const ChannelType Chnl11 = pSrcRow1[src_col1 * NumChannels + c];

                ChannelType& DstCol = reinterpret_cast<ChannelType*>(reinterpret_cast<Uint8*>(Attribs.pCoarseMipData) + row * Attribs.CoarseMipStride)[col * NumChannels + c];

                DstCol = Filter(Chnl00, Chnl10, Chnl01, Chnl11, col, row);
            }
        }
    }
}

void RemapAlpha(const ComputeMipLevelAttribs& Attribs,
                Uint32                        NumChannels,
                Uint32                        AlphaChannelInd)
{
    const Uint32 CoarseMipWidth  = std::max(Attribs.FineMipWidth / Uint32{2}, Uint32{1});
    const Uint32 CoarseMipHeight = std::max(Attribs.FineMipHeight / Uint32{2}, Uint32{1});
    for (Uint32 row = 0; row < CoarseMipHeight; ++row)
    {
        for (Uint32 col = 0; col < CoarseMipWidth; ++col)
        {
            Uint8& Alpha = (reinterpret_cast<Uint8*>(Attribs.pCoarseMipData) + row * Attribs.CoarseMipStride)[col * NumChannels + AlphaChannelInd];

            // Remap alpha channel using the following formula to improve mip maps:
            //
            //      A_new = max(A_old; 1/3 * A_old + 2/3 * CutoffThreshold)
            //
            // https://asawicki.info/articles/alpha_test.php5

            float AlphaNew = std::min((static_cast<float>(Alpha) + 2.f * (Attribs.AlphaCutoff * 255.f)) / 3.f, 255.f);

            Alpha = std::max(Alpha, static_cast<Uint8>(AlphaNew));
        }
    }
}

template <typename ChannelType>
void ComputeMipLevelInternal(const ComputeMipLevelAttribs& Attribs,
                             const TextureFormatAttribs&   FmtAttribs)
{
    MIP_FILTER_TYPE FilterType = Attribs.FilterType;
    if (FilterType == MIP_FILTER_TYPE_DEFAULT)
    {
        FilterType = FmtAttribs.ComponentType == COMPONENT_TYPE_UINT || FmtAttribs.ComponentType == COMPONENT_TYPE_SINT ?
            MIP_FILTER_TYPE_MOST_FREQUENT :
            MIP_FILTER_TYPE_BOX_AVERAGE;
    }

    FilterMipLevel<ChannelType>(Attribs, FmtAttribs.NumComponents,
                                FilterType == MIP_FILTER_TYPE_BOX_AVERAGE ?
                                    LinearAverage<ChannelType> :
                                    MostFrequentSelector<ChannelType>);
}

void ComputeMipLevel(const ComputeMipLevelAttribs& Attribs)
{
    DEV_CHECK_ERR(Attribs.Format != TEX_FORMAT_UNKNOWN, "Format must not be unknown");
    DEV_CHECK_ERR(Attribs.FineMipWidth != 0, "Fine mip width must not be zero");
    DEV_CHECK_ERR(Attribs.FineMipHeight != 0, "Fine mip height must not be zero");
    DEV_CHECK_ERR(Attribs.pFineMipData != nullptr, "Fine level data must not be null");
    DEV_CHECK_ERR(Attribs.pCoarseMipData != nullptr, "Coarse level data must not be null");

    const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Attribs.Format);

    VERIFY_EXPR(Attribs.AlphaCutoff >= 0 && Attribs.AlphaCutoff <= 1);
    VERIFY(Attribs.AlphaCutoff == 0 || FmtAttribs.NumComponents == 4 && FmtAttribs.ComponentSize == 1,
           "Alpha remapping is only supported for 4-channel 8-bit textures");

    switch (FmtAttribs.ComponentType)
    {
        case COMPONENT_TYPE_UNORM_SRGB:
            VERIFY(FmtAttribs.ComponentSize == 1, "Only 8-bit sRGB formats are expected");
            FilterMipLevel<Uint8>(Attribs, FmtAttribs.NumComponents,
                                  Attribs.FilterType == MIP_FILTER_TYPE_MOST_FREQUENT ?
                                      MostFrequentSelector<Uint8> :
                                      SRGBAverage<Uint8>);
            if (Attribs.AlphaCutoff > 0)
            {
                RemapAlpha(Attribs, FmtAttribs.NumComponents, FmtAttribs.NumComponents - 1);
            }
            break;

        case COMPONENT_TYPE_UNORM:
        case COMPONENT_TYPE_UINT:
            switch (FmtAttribs.ComponentSize)
            {
                case 1:
                    ComputeMipLevelInternal<Uint8>(Attribs, FmtAttribs);
                    if (Attribs.AlphaCutoff > 0)
                    {
                        RemapAlpha(Attribs, FmtAttribs.NumComponents, FmtAttribs.NumComponents - 1);
                    }
                    break;

                case 2:
                    ComputeMipLevelInternal<Uint16>(Attribs, FmtAttribs);
                    break;

                case 4:
                    ComputeMipLevelInternal<Uint32>(Attribs, FmtAttribs);
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
                    ComputeMipLevelInternal<Int8>(Attribs, FmtAttribs);
                    break;

                case 2:
                    ComputeMipLevelInternal<Int16>(Attribs, FmtAttribs);
                    break;

                case 4:
                    ComputeMipLevelInternal<Int32>(Attribs, FmtAttribs);
                    break;

                default:
                    UNEXPECTED("Unexpected component size (", FmtAttribs.ComponentSize, ") for UINT/SINT texture format");
            }
            break;

        case COMPONENT_TYPE_FLOAT:
            VERIFY(FmtAttribs.ComponentSize == 4, "Only 32-bit float formats are currently supported");
            ComputeMipLevelInternal<Float32>(Attribs, FmtAttribs);
            break;

        default:
            UNEXPECTED("Unsupported component type");
    }
}

#if !METAL_SUPPORTED
void CreateSparseTextureMtl(IRenderDevice*     pDevice,
                            const TextureDesc& TexDesc,
                            IDeviceMemory*     pMemory,
                            ITexture**         ppTexture)
{
}
#endif

inline ITextureView* ExtractTextureView(ITexture* pTexture, TEXTURE_VIEW_TYPE ViewType)
{
    return pTexture != nullptr ? pTexture->GetDefaultView(ViewType) : nullptr;
}

inline IBufferView* ExtractBufferView(IBuffer* pBuffer, BUFFER_VIEW_TYPE ViewType)
{
    return pBuffer != nullptr ? pBuffer->GetDefaultView(ViewType) : nullptr;
}

ITextureView* GetDefaultSRV(ITexture* pTexture)
{
    return ExtractTextureView(pTexture, TEXTURE_VIEW_SHADER_RESOURCE);
}

ITextureView* GetDefaultRTV(ITexture* pTexture)
{
    return ExtractTextureView(pTexture, TEXTURE_VIEW_RENDER_TARGET);
}

ITextureView* GetDefaultDSV(ITexture* pTexture)
{
    return ExtractTextureView(pTexture, TEXTURE_VIEW_DEPTH_STENCIL);
}

ITextureView* GetDefaultUAV(ITexture* pTexture)
{
    return ExtractTextureView(pTexture, TEXTURE_VIEW_UNORDERED_ACCESS);
}

IBufferView* GetDefaultSRV(IBuffer* pBuffer)
{
    return ExtractBufferView(pBuffer, BUFFER_VIEW_SHADER_RESOURCE);
}

IBufferView* GetDefaultUAV(IBuffer* pBuffer)
{
    return ExtractBufferView(pBuffer, BUFFER_VIEW_UNORDERED_ACCESS);
}

ITextureView* GetTextureDefaultSRV(IObject* pTexture)
{
    DEV_CHECK_ERR(pTexture == nullptr || RefCntAutoPtr<ITexture>(pTexture, IID_Texture), "Resource is not a texture");
    return GetDefaultSRV(static_cast<ITexture*>(pTexture));
}

ITextureView* GetTextureDefaultRTV(IObject* pTexture)
{
    DEV_CHECK_ERR(pTexture == nullptr || RefCntAutoPtr<ITexture>(pTexture, IID_Texture), "Resource is not a texture");
    return GetDefaultRTV(static_cast<ITexture*>(pTexture));
}

ITextureView* GetTextureDefaultDSV(IObject* pTexture)
{
    DEV_CHECK_ERR(pTexture == nullptr || RefCntAutoPtr<ITexture>(pTexture, IID_Texture), "Resource is not a texture");
    return GetDefaultDSV(static_cast<ITexture*>(pTexture));
}

ITextureView* GetTextureDefaultUAV(IObject* pTexture)
{
    DEV_CHECK_ERR(pTexture == nullptr || RefCntAutoPtr<ITexture>(pTexture, IID_Texture), "Resource is not a texture");
    return GetDefaultUAV(static_cast<ITexture*>(pTexture));
}

IBufferView* GetBufferDefaultSRV(IObject* pBuffer)
{
    DEV_CHECK_ERR(pBuffer == nullptr || RefCntAutoPtr<IBuffer>(pBuffer, IID_Buffer), "Resource is not a buffer");
    return GetDefaultSRV(static_cast<IBuffer*>(pBuffer));
}

IBufferView* GetBufferDefaultUAV(IObject* pBuffer)
{
    DEV_CHECK_ERR(pBuffer == nullptr || RefCntAutoPtr<IBuffer>(pBuffer, IID_Buffer), "Resource is not a buffer");
    return GetDefaultUAV(static_cast<IBuffer*>(pBuffer));
}

#if !WEBGPU_SUPPORTED
const char* GetWebGPUEmulatedArrayIndexSuffix(IShader* pShader)
{
    return nullptr;
}
#endif

int64_t GetNativeTextureFormat(TEXTURE_FORMAT TexFormat, RENDER_DEVICE_TYPE DeviceType)
{
    switch (DeviceType)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
            return GetNativeTextureFormatD3D11(TexFormat);
#endif

#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
            return GetNativeTextureFormatD3D12(TexFormat);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            return GetNativeTextureFormatGL(TexFormat);
#endif

#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
            return GetNativeTextureFormatVk(TexFormat);
#endif

#if METAL_SUPPORTED
        case RENDER_DEVICE_TYPE_METAL:
            return GetNativeTextureFormatMtl(TexFormat);
#endif

#if WEBGPU_SUPPORTED
        case RENDER_DEVICE_TYPE_WEBGPU:
            return GetNativeTextureFormatWebGPU(TexFormat);
#endif

        default:
            UNSUPPORTED("Unsupported device type");
            return 0;
    }
}

TEXTURE_FORMAT GetTextureFormatFromNative(int64_t NativeFormat, RENDER_DEVICE_TYPE DeviceType)
{
    switch (DeviceType)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
            return GetTextureFormatFromNativeD3D11(NativeFormat);
#endif

#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
            return GetTextureFormatFromNativeD3D12(NativeFormat);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            return GetTextureFormatFromNativeGL(NativeFormat);
#endif

#if METAL_SUPPORTED
        case RENDER_DEVICE_TYPE_METAL:
            return GetTextureFormatFromNativeMtl(NativeFormat);
#endif

#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
            return GetTextureFormatFromNativeVk(NativeFormat);
#endif

#if WEBGPU_SUPPORTED
        case RENDER_DEVICE_TYPE_WEBGPU:
            return GetTextureFormatFromNativeWebGPU(NativeFormat);
#endif

        default:
            UNSUPPORTED("Unsupported device type");
            return TEX_FORMAT_UNKNOWN;
    }
}

void CreateGeometryPrimitiveBuffers(IRenderDevice*                            pDevice,
                                    const GeometryPrimitiveAttributes&        Attribs,
                                    const GeometryPrimitiveBuffersCreateInfo* pBufferCI,
                                    IBuffer**                                 ppVertices,
                                    IBuffer**                                 ppIndices,
                                    GeometryPrimitiveInfo*                    pInfo)
{
    static_assert(GEOMETRY_PRIMITIVE_TYPE_COUNT == 3, "Please handle the new primitive type");

    RefCntAutoPtr<IDataBlob> pVertexData;
    RefCntAutoPtr<IDataBlob> pIndexData;
    CreateGeometryPrimitive(Attribs,
                            ppVertices != nullptr ? pVertexData.RawDblPtr() : nullptr,
                            ppIndices != nullptr ? pIndexData.RawDblPtr() : nullptr,
                            pInfo);

    static constexpr GeometryPrimitiveBuffersCreateInfo DefaultCI{};
    if (pBufferCI == nullptr)
        pBufferCI = &DefaultCI;

    const char* PrimTypeStr = "";
    switch (Attribs.Type)
    {
        case GEOMETRY_PRIMITIVE_TYPE_CUBE: PrimTypeStr = "Cube"; break;
        case GEOMETRY_PRIMITIVE_TYPE_SPHERE: PrimTypeStr = "Sphere"; break;
        default: UNEXPECTED("Unexpected primitive type");
    }

    static std::atomic<int> PrimCounter{0};
    const int               PrimId = PrimCounter.fetch_add(1);
    if (pVertexData)
    {
        const std::string Name = std::string{"Geometry primitive "} + std::to_string(PrimId) + " (" + PrimTypeStr + ")";

        BufferDesc VBDesc;
        VBDesc.Name           = Name.c_str();
        VBDesc.Size           = pVertexData->GetSize();
        VBDesc.BindFlags      = pBufferCI->VertexBufferBindFlags;
        VBDesc.Usage          = pBufferCI->VertexBufferUsage;
        VBDesc.CPUAccessFlags = pBufferCI->VertexBufferCPUAccessFlags;
        VBDesc.Mode           = pBufferCI->VertexBufferMode;
        if (VBDesc.Mode != BUFFER_MODE_UNDEFINED)
        {
            VBDesc.ElementByteStride = GetGeometryPrimitiveVertexSize(Attribs.VertexFlags);
        }

        BufferData VBData{pVertexData->GetDataPtr(), pVertexData->GetSize()};
        pDevice->CreateBuffer(VBDesc, &VBData, ppVertices);
    }

    if (pIndexData)
    {
        const std::string Name = std::string{"Geometry primitive "} + std::to_string(PrimId) + " (" + PrimTypeStr + ")";

        BufferDesc IBDesc;
        IBDesc.Name           = Name.c_str();
        IBDesc.Size           = pIndexData->GetSize();
        IBDesc.BindFlags      = pBufferCI->IndexBufferBindFlags;
        IBDesc.Usage          = pBufferCI->IndexBufferUsage;
        IBDesc.CPUAccessFlags = pBufferCI->IndexBufferCPUAccessFlags;
        IBDesc.Mode           = pBufferCI->IndexBufferMode;
        if (IBDesc.Mode != BUFFER_MODE_UNDEFINED)
        {
            IBDesc.ElementByteStride = sizeof(Uint32);
        }

        BufferData IBData{pIndexData->GetDataPtr(), pIndexData->GetSize()};
        pDevice->CreateBuffer(IBDesc, &IBData, ppIndices);
    }
}

} // namespace Diligent


extern "C"
{
    void Diligent_CreateUniformBuffer(Diligent::IRenderDevice*   pDevice,
                                      Diligent::Uint64           Size,
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
                                              Diligent::Uint64         StrideInBytes)
    {
        Diligent::GenerateCheckerBoardPattern(Width, Height, Fmt, HorzCells, VertCells, pData, StrideInBytes);
    }

    void Diligent_ComputeMipLevel(const Diligent::ComputeMipLevelAttribs& Attribs)
    {
        Diligent::ComputeMipLevel(Attribs);
    }

    void Diligent_CreateSparseTextureMtl(Diligent::IRenderDevice*     pDevice,
                                         const Diligent::TextureDesc& TexDesc,
                                         Diligent::IDeviceMemory*     pMemory,
                                         Diligent::ITexture**         ppTexture)
    {
        Diligent::CreateSparseTextureMtl(pDevice, TexDesc, pMemory, ppTexture);
    }


    Diligent::ITextureView* Diligent_GetTextureDefaultSRV(Diligent::IObject* pTexture)
    {
        return Diligent::GetTextureDefaultSRV(pTexture);
    }

    Diligent::ITextureView* Diligent_GetTextureDefaultRTV(Diligent::IObject* pTexture)
    {
        return Diligent::GetTextureDefaultRTV(pTexture);
    }

    Diligent::ITextureView* Diligent_GetTextureDefaultDSV(Diligent::IObject* pTexture)
    {
        return Diligent::GetTextureDefaultDSV(pTexture);
    }

    Diligent::ITextureView* Diligent_GetTextureDefaultUAV(Diligent::IObject* pTexture)
    {
        return Diligent::GetTextureDefaultUAV(pTexture);
    }

    Diligent::IBufferView* Diligent_GetBufferDefaultSRV(Diligent::IObject* pBuffer)
    {
        return Diligent::GetBufferDefaultSRV(pBuffer);
    }

    Diligent::IBufferView* Diligent_GetBufferDefaultRTV(Diligent::IObject* pBuffer)
    {
        return Diligent::GetBufferDefaultUAV(pBuffer);
    }

    const char* Diligent_GetWebGPUEmulatedArrayIndexSuffix(Diligent::IShader* pShader)
    {
        return Diligent::GetWebGPUEmulatedArrayIndexSuffix(pShader);
    }

    int64_t Diligent_GetNativeTextureFormat(Diligent::TEXTURE_FORMAT TexFormat, Diligent::RENDER_DEVICE_TYPE DeviceType)
    {
        return Diligent::GetNativeTextureFormat(TexFormat, DeviceType);
    }

    Diligent::TEXTURE_FORMAT Diligent_GetTextureFormatFromNative(int64_t NativeFormat, Diligent::RENDER_DEVICE_TYPE DeviceType)
    {
        return Diligent::GetTextureFormatFromNative(NativeFormat, DeviceType);
    }

    void Diligent_CreateGeometryPrimitiveBuffers(Diligent::IRenderDevice*                            pDevice,
                                                 const Diligent::GeometryPrimitiveAttributes&        Attribs,
                                                 const Diligent::GeometryPrimitiveBuffersCreateInfo* pBufferCI,
                                                 Diligent::IBuffer**                                 ppVertices,
                                                 Diligent::IBuffer**                                 ppIndices,
                                                 Diligent::GeometryPrimitiveInfo*                    pInfo)
    {
        Diligent::CreateGeometryPrimitiveBuffers(pDevice, Attribs, pBufferCI, ppVertices, ppIndices, pInfo);
    }
}
