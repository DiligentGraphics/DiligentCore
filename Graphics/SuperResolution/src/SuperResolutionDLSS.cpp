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

#include "SuperResolutionDLSS.hpp"
#include "SuperResolutionBase.hpp"
#include "SuperResolutionVariants.hpp"
#include "DebugUtilities.hpp"

#include <nvsdk_ngx.h>
#include <nvsdk_ngx_params.h>
#include <nvsdk_ngx_helpers.h>

namespace Diligent
{

const char*    DLSSProjectId   = "750fed3a-efba-42ba-801b-22d4cbad9148";
const wchar_t* DLSSAppDataPath = L".";

static const Char* GetOptimizationTypeName(SUPER_RESOLUTION_OPTIMIZATION_TYPE Type)
{
    static_assert(SUPER_RESOLUTION_OPTIMIZATION_TYPE_COUNT == 5, "Please update the switch below to handle the new optimization type");
    switch (Type)
    {
        // clang-format off
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_QUALITY:      return "Max Quality";
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_QUALITY:     return "High Quality";
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED:         return "Balanced";
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_PERFORMANCE: return "High Performance";
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_PERFORMANCE:  return "Max Performance";
            // clang-format on
        default:
            UNEXPECTED("Unexpected optimization type");
            return "Unknown";
    }
}

NVSDK_NGX_PerfQuality_Value OptimizationTypeToNGXPerfQuality(SUPER_RESOLUTION_OPTIMIZATION_TYPE Type)
{
    switch (Type)
    {
        // clang-format off
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_QUALITY:      return NVSDK_NGX_PerfQuality_Value_UltraQuality;
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_QUALITY:     return NVSDK_NGX_PerfQuality_Value_MaxQuality;
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED:         return NVSDK_NGX_PerfQuality_Value_Balanced;
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_PERFORMANCE: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
        case SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_PERFORMANCE:  return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
        default:                                                  return NVSDK_NGX_PerfQuality_Value_Balanced;
            // clang-format on
    }
}

Int32 ComputeDLSSFeatureFlags(SUPER_RESOLUTION_FLAGS Flags, const ExecuteSuperResolutionAttribs& Attribs)
{
    Int32 DLSSFlags = NVSDK_NGX_DLSS_Feature_Flags_None;

    if (Flags & SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE)
        DLSSFlags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    if (Flags & SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING)
        DLSSFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;
    if (Attribs.CameraNear > Attribs.CameraFar)
        DLSSFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;

    DLSSFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    DLSSFlags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;

    return DLSSFlags;
}

void DLSSProviderBase::EnumerateVariants(std::vector<SuperResolutionInfo>& Variants)
{
    if (m_pNGXParams == nullptr)
        return;

    Int32 NeedsUpdatedDriver = 0;
    NVSDK_NGX_Parameter_GetI(m_pNGXParams, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &NeedsUpdatedDriver);
    if (NeedsUpdatedDriver)
        LOG_WARNING_MESSAGE("NVIDIA DLSS requires an updated driver.");

    Int32            DLSSAvailable = 0;
    NVSDK_NGX_Result Result        = NVSDK_NGX_Parameter_GetI(m_pNGXParams, NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSAvailable);
    if (NVSDK_NGX_FAILED(Result))
    {
        LOG_WARNING_MESSAGE("Failed to query DLSS availability. Result: ", static_cast<Uint32>(Result));
        return;
    }

    if (DLSSAvailable)
    {
        SuperResolutionInfo Info{};
        Info.Type             = SUPER_RESOLUTION_TYPE_TEMPORAL;
        Info.TemporalCapFlags = SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_NATIVE |
            SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_EXPOSURE_SCALE_TEXTURE |
            SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_IGNORE_HISTORY_MASK |
            SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_REACTIVE_MASK |
            SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_SHARPNESS;

        snprintf(Info.Name, sizeof(Info.Name), "NGX: DLSS");
        Info.VariantId = VariantId_DLSS;

        Variants.push_back(Info);
        LOG_INFO_MESSAGE("NVIDIA DLSS is available: ", Info.Name);
    }
    else
    {
        LOG_INFO_MESSAGE("NVIDIA DLSS is not available on this hardware.");
    }
}

void DLSSProviderBase::GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs,
                                         SuperResolutionSourceSettings&              Settings)
{
    Settings = {};

    ValidateSourceSettingsAttribs(Attribs);

    for (SUPER_RESOLUTION_OPTIMIZATION_TYPE OptType = Attribs.OptimizationType; OptType < SUPER_RESOLUTION_OPTIMIZATION_TYPE_COUNT; OptType = static_cast<SUPER_RESOLUTION_OPTIMIZATION_TYPE>(OptType + 1))
    {
        NVSDK_NGX_PerfQuality_Value PerfQuality = OptimizationTypeToNGXPerfQuality(OptType);

        Uint32 OptimalWidth  = 0;
        Uint32 OptimalHeight = 0;
        Uint32 MaxWidth      = 0;
        Uint32 MaxHeight     = 0;
        Uint32 MinWidth      = 0;
        Uint32 MinHeight     = 0;
        float  Sharpness     = 0.0f;

        NVSDK_NGX_Result Result = NGX_DLSS_GET_OPTIMAL_SETTINGS(
            m_pNGXParams,
            Attribs.OutputWidth, Attribs.OutputHeight,
            PerfQuality,
            &OptimalWidth, &OptimalHeight,
            &MaxWidth, &MaxHeight,
            &MinWidth, &MinHeight,
            &Sharpness);

        if (NVSDK_NGX_SUCCEED(Result) && OptimalWidth > 0 && OptimalHeight > 0)
        {
            if (OptType != Attribs.OptimizationType)
            {
                LOG_WARNING_MESSAGE("DLSS quality mode '", GetOptimizationTypeName(Attribs.OptimizationType),
                                    "' is not available. Falling back to '", GetOptimizationTypeName(OptType), "'.");
            }
            Settings.OptimalInputWidth  = OptimalWidth;
            Settings.OptimalInputHeight = OptimalHeight;
            return;
        }
    }

    LOG_WARNING_MESSAGE("Failed to get DLSS optimal settings: no quality mode is available for ",
                        Attribs.OutputWidth, "x", Attribs.OutputHeight, " output resolution.");
}

} // namespace Diligent
