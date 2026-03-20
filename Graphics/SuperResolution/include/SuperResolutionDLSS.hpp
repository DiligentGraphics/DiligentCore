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

/// \file
/// Shared DLSS utilities used by per-API DLSS backend implementations.

#include <vector>

#include <nvsdk_ngx_defs.h>

#include "SuperResolutionFactory.h"
#include "SuperResolution.h"

struct NVSDK_NGX_Parameter;

namespace Diligent
{

extern const char*    DLSSProjectId;
extern const wchar_t* DLSSAppDataPath;

/// Maps Diligent optimization type to NGX performance/quality preset.
NVSDK_NGX_PerfQuality_Value OptimizationTypeToNGXPerfQuality(SUPER_RESOLUTION_OPTIMIZATION_TYPE Type);

/// Computes the full set of DLSS feature flags from the description and execution attributes.
Int32 ComputeDLSSFeatureFlags(SUPER_RESOLUTION_FLAGS Flags, const ExecuteSuperResolutionAttribs& Attribs);

/// Populates DLSS variant info using NGX capability parameters.
void EnumerateDLSSVariants(NVSDK_NGX_Parameter* pNGXParams, std::vector<SuperResolutionInfo>& Variants);

/// Queries DLSS optimal source settings using NGX capability parameters.
void GetDLSSSourceSettings(NVSDK_NGX_Parameter*                        pNGXParams,
                           const SuperResolutionSourceSettingsAttribs& Attribs,
                           SuperResolutionSourceSettings&              Settings);

} // namespace Diligent
