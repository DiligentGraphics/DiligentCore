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

#include "SuperResolutionBase.hpp"

namespace Diligent
{

namespace
{

float HaltonSequence(Uint32 Base, Uint32 Index)
{
    float  Result = 0.0f;
    float  Frac   = 1.0f / static_cast<float>(Base);
    Uint32 Idx    = Index + 1;
    while (Idx > 0)
    {
        Result += Frac * static_cast<float>(Idx % Base);
        Idx /= Base;
        Frac /= static_cast<float>(Base);
    }
    return Result;
}

} // namespace

void PopulateHaltonJitterPattern(std::vector<SuperResolutionBase::JitterOffset>& JitterPattern, Uint32 PatternSize)
{
    JitterPattern.resize(PatternSize);
    for (Uint32 Idx = 0; Idx < PatternSize; ++Idx)
    {
        JitterPattern[Idx].X = HaltonSequence(2, Idx) - 0.5f;
        JitterPattern[Idx].Y = HaltonSequence(3, Idx) - 0.5f;
    }
}

void ValidateSourceSettingsAttribs(const SuperResolutionSourceSettingsAttribs& Attribs)
{
#ifdef DILIGENT_DEVELOPMENT
    DEV_CHECK_ERR(Attribs.OutputWidth > 0 && Attribs.OutputHeight > 0, "Output resolution must be greater than zero");
    DEV_CHECK_ERR(Attribs.OptimizationType < SUPER_RESOLUTION_OPTIMIZATION_TYPE_COUNT, "Invalid optimization type");
#endif
}

void ValidateSuperResolutionDesc(const SuperResolutionDesc& Desc, const SuperResolutionInfo& Info) noexcept(false)
{
    VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.OutputWidth > 0 && Desc.OutputHeight > 0, "Output resolution must be greater than zero");
    VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.OutputFormat != TEX_FORMAT_UNKNOWN, "OutputFormat must not be TEX_FORMAT_UNKNOWN");
    VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.ColorFormat != TEX_FORMAT_UNKNOWN, "ColorFormat must not be TEX_FORMAT_UNKNOWN");
    VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.InputWidth > 0 && Desc.InputHeight > 0, "InputWidth and InputHeight must be greater than zero");
    VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.InputWidth <= Desc.OutputWidth && Desc.InputHeight <= Desc.OutputHeight,
                            "Input resolution must not exceed output resolution");

    if (Desc.Flags & SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING)
    {
        const bool SharpnessSupported = Info.Type == SUPER_RESOLUTION_TYPE_SPATIAL ?
            (Info.SpatialCapFlags & SUPER_RESOLUTION_SPATIAL_CAP_FLAG_SHARPNESS) != 0 :
            (Info.TemporalCapFlags & SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_SHARPNESS) != 0;

        VERIFY_SUPER_RESOLUTION(Desc.Name, SharpnessSupported,
                                "SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING is set, but the '", Info.Name,
                                "' variant does not support sharpness. Check the variant's capability flags.");
    }

    if (Desc.Flags & SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE)
    {
        VERIFY_SUPER_RESOLUTION(Desc.Name, Info.Type == SUPER_RESOLUTION_TYPE_TEMPORAL,
                                "SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE is only supported for temporal upscaling.");
    }

    if (Info.Type == SUPER_RESOLUTION_TYPE_TEMPORAL)
    {
        VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.DepthFormat != TEX_FORMAT_UNKNOWN, "DepthFormat must not be TEX_FORMAT_UNKNOWN for temporal upscaling");
        VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.MotionFormat != TEX_FORMAT_UNKNOWN, "MotionFormat must not be TEX_FORMAT_UNKNOWN for temporal upscaling");

        if (!(Desc.Flags & SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE))
        {
            VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.ExposureFormat != TEX_FORMAT_UNKNOWN,
                                    "ExposureFormat must not be TEX_FORMAT_UNKNOWN when SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE is not set for temporal upscaling");
        }
    }
    else
    {
        VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.DepthFormat == TEX_FORMAT_UNKNOWN,
                                "DepthFormat must be TEX_FORMAT_UNKNOWN for spatial upscaling");
        VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.MotionFormat == TEX_FORMAT_UNKNOWN,
                                "MotionFormat must be TEX_FORMAT_UNKNOWN for spatial upscaling");
        VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.ReactiveMaskFormat == TEX_FORMAT_UNKNOWN,
                                "ReactiveMaskFormat must be TEX_FORMAT_UNKNOWN for spatial upscaling");
        VERIFY_SUPER_RESOLUTION(Desc.Name, Desc.ExposureFormat == TEX_FORMAT_UNKNOWN,
                                "ExposureFormat must be TEX_FORMAT_UNKNOWN for spatial upscaling");
    }
}

void ValidateExecuteSuperResolutionAttribs(const SuperResolutionDesc&           Desc,
                                           const SuperResolutionInfo&           Info,
                                           const ExecuteSuperResolutionAttribs& Attribs)
{
#ifdef DILIGENT_DEVELOPMENT
    DEV_CHECK_SUPER_RESOLUTION(Desc.Name, Attribs.pContext != nullptr, "Device context must not be null");
    DEV_CHECK_SUPER_RESOLUTION(Desc.Name, Attribs.pColorTextureSRV != nullptr, "Color texture SRV must not be null");
    DEV_CHECK_SUPER_RESOLUTION(Desc.Name, Attribs.pOutputTextureView != nullptr, "Output texture view must not be null");

    // Validate color texture
    if (Attribs.pColorTextureSRV != nullptr)
    {
        const TextureDesc&     TexDesc  = Attribs.pColorTextureSRV->GetTexture()->GetDesc();
        const TextureViewDesc& ViewDesc = Attribs.pColorTextureSRV->GetDesc();
        DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE,
                                   "Color texture view '", TexDesc.Name, "' must be TEXTURE_VIEW_SHADER_RESOURCE");
        DEV_CHECK_SUPER_RESOLUTION(Desc.Name, TexDesc.Width >= Desc.InputWidth && TexDesc.Height >= Desc.InputHeight,
                                   "Color texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                                   ") must be at least the upscaler input resolution (", Desc.InputWidth, "x", Desc.InputHeight, ")");
        DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.Format == Desc.ColorFormat,
                                   "Color texture view '", TexDesc.Name, "' format (", GetTextureFormatAttribs(ViewDesc.Format).Name,
                                   ") does not match the expected ColorFormat (", GetTextureFormatAttribs(Desc.ColorFormat).Name, ")");
    }

    // Validate output texture
    if (Attribs.pOutputTextureView != nullptr)
    {
        const TextureDesc&     TexDesc  = Attribs.pOutputTextureView->GetTexture()->GetDesc();
        const TextureViewDesc& ViewDesc = Attribs.pOutputTextureView->GetDesc();
        DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET || ViewDesc.ViewType == TEXTURE_VIEW_UNORDERED_ACCESS,
                                   "Output texture view '", TexDesc.Name, "' must be TEXTURE_VIEW_RENDER_TARGET or TEXTURE_VIEW_UNORDERED_ACCESS");
        DEV_CHECK_SUPER_RESOLUTION(Desc.Name, TexDesc.Width == Desc.OutputWidth && TexDesc.Height == Desc.OutputHeight,
                                   "Output texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                                   ") must match the upscaler output resolution (", Desc.OutputWidth, "x", Desc.OutputHeight, ")");
        DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.Format == Desc.OutputFormat,
                                   "Output texture view '", TexDesc.Name, "' format (", GetTextureFormatAttribs(ViewDesc.Format).Name,
                                   ") does not match the expected OutputFormat (", GetTextureFormatAttribs(Desc.OutputFormat).Name, ")");
    }

    if (Info.Type == SUPER_RESOLUTION_TYPE_TEMPORAL)
    {
        DEV_CHECK_SUPER_RESOLUTION(Desc.Name, Attribs.pDepthTextureSRV != nullptr, "Depth texture SRV must not be null for temporal upscaling");
        DEV_CHECK_SUPER_RESOLUTION(Desc.Name, Attribs.pMotionVectorsSRV != nullptr, "Motion vectors SRV must not be null for temporal upscaling");
        DEV_CHECK_SUPER_RESOLUTION(Desc.Name, (Desc.Flags & SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE) != 0 || Attribs.pExposureTextureSRV != nullptr,
                                   "Exposure texture SRV must not be null when SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE is not set");

        // Validate output texture view type (DirectSR requires UAV)
        if (Attribs.pOutputTextureView != nullptr)
        {
            const TextureDesc&     TexDesc  = Attribs.pOutputTextureView->GetTexture()->GetDesc();
            const TextureViewDesc& ViewDesc = Attribs.pOutputTextureView->GetDesc();
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.ViewType == TEXTURE_VIEW_UNORDERED_ACCESS,
                                       "Output texture view '", TexDesc.Name, "' must be TEXTURE_VIEW_UNORDERED_ACCESS");
        }

        // Validate depth texture
        if (Attribs.pDepthTextureSRV != nullptr)
        {
            const TextureDesc&     TexDesc  = Attribs.pDepthTextureSRV->GetTexture()->GetDesc();
            const TextureViewDesc& ViewDesc = Attribs.pDepthTextureSRV->GetDesc();
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE,
                                       "Depth texture view '", TexDesc.Name, "' must be TEXTURE_VIEW_SHADER_RESOURCE");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, TexDesc.Width >= Desc.InputWidth && TexDesc.Height >= Desc.InputHeight,
                                       "Depth texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                                       ") must be at least the upscaler input resolution (", Desc.InputWidth, "x", Desc.InputHeight, ")");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.Format == Desc.DepthFormat,
                                       "Depth texture view '", TexDesc.Name, "' format (", GetTextureFormatAttribs(ViewDesc.Format).Name,
                                       ") does not match the expected DepthFormat (", GetTextureFormatAttribs(Desc.DepthFormat).Name, ")");
        }

        // Validate motion vectors texture
        if (Attribs.pMotionVectorsSRV != nullptr)
        {
            const TextureDesc&     TexDesc  = Attribs.pMotionVectorsSRV->GetTexture()->GetDesc();
            const TextureViewDesc& ViewDesc = Attribs.pMotionVectorsSRV->GetDesc();
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE,
                                       "Motion vectors view '", TexDesc.Name, "' must be TEXTURE_VIEW_SHADER_RESOURCE");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, TexDesc.Width >= Desc.InputWidth && TexDesc.Height >= Desc.InputHeight,
                                       "Motion vectors texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                                       ") must be at least the upscaler input resolution (", Desc.InputWidth, "x", Desc.InputHeight, ")");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.Format == Desc.MotionFormat,
                                       "Motion vectors view '", TexDesc.Name, "' format (", GetTextureFormatAttribs(ViewDesc.Format).Name,
                                       ") does not match the expected MotionFormat (", GetTextureFormatAttribs(Desc.MotionFormat).Name, ")");
        }

        // Validate exposure texture
        if (Attribs.pExposureTextureSRV != nullptr)
        {
            const TextureDesc&     TexDesc  = Attribs.pExposureTextureSRV->GetTexture()->GetDesc();
            const TextureViewDesc& ViewDesc = Attribs.pExposureTextureSRV->GetDesc();
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE,
                                       "Exposure texture view '", TexDesc.Name, "' must be TEXTURE_VIEW_SHADER_RESOURCE");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, TexDesc.Width == 1 && TexDesc.Height == 1,
                                       "Exposure texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                                       ") must be 1x1");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.Format == Desc.ExposureFormat,
                                       "Exposure texture view '", TexDesc.Name, "' format (", GetTextureFormatAttribs(ViewDesc.Format).Name,
                                       ") does not match the expected ExposureFormat (", GetTextureFormatAttribs(Desc.ExposureFormat).Name, ")");
        }

        // Validate reactive mask texture
        if (Attribs.pReactiveMaskTextureSRV != nullptr)
        {
            const TextureDesc&     TexDesc  = Attribs.pReactiveMaskTextureSRV->GetTexture()->GetDesc();
            const TextureViewDesc& ViewDesc = Attribs.pReactiveMaskTextureSRV->GetDesc();
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE,
                                       "Reactive mask view '", TexDesc.Name, "' must be TEXTURE_VIEW_SHADER_RESOURCE");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, Desc.ReactiveMaskFormat != TEX_FORMAT_UNKNOWN,
                                       "Reactive mask texture '", TexDesc.Name, "' provided but ReactiveMaskFormat was not set in SuperResolutionDesc");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, TexDesc.Width >= Desc.InputWidth && TexDesc.Height >= Desc.InputHeight,
                                       "Reactive mask texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                                       ") must be at least the upscaler input resolution (", Desc.InputWidth, "x", Desc.InputHeight, ")");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.Format == Desc.ReactiveMaskFormat,
                                       "Reactive mask view '", TexDesc.Name, "' format (", GetTextureFormatAttribs(ViewDesc.Format).Name,
                                       ") does not match the expected ReactiveMaskFormat (", GetTextureFormatAttribs(Desc.ReactiveMaskFormat).Name, ")");
        }

        // Validate ignore history mask texture
        if (Attribs.pIgnoreHistoryMaskTextureSRV != nullptr)
        {
            const TextureDesc&     TexDesc  = Attribs.pIgnoreHistoryMaskTextureSRV->GetTexture()->GetDesc();
            const TextureViewDesc& ViewDesc = Attribs.pIgnoreHistoryMaskTextureSRV->GetDesc();
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE,
                                       "Ignore history mask view '", TexDesc.Name, "' must be TEXTURE_VIEW_SHADER_RESOURCE");
            DEV_CHECK_SUPER_RESOLUTION(Desc.Name, TexDesc.Width >= Desc.InputWidth && TexDesc.Height >= Desc.InputHeight,
                                       "Ignore history mask texture '", TexDesc.Name, "' dimensions (", TexDesc.Width, "x", TexDesc.Height,
                                       ") must be at least the upscaler input resolution (", Desc.InputWidth, "x", Desc.InputHeight, ")");
        }
    }
#endif
}

} // namespace Diligent
