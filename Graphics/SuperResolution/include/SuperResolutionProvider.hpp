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

#include "SuperResolutionFactory.h"
#include "SuperResolution.h"
#include "SuperResolutionBase.hpp"

#include <algorithm>
#include <vector>

namespace Diligent
{

class SuperResolutionProvider
{
public:
    virtual ~SuperResolutionProvider()
    {}

    virtual void EnumerateVariants(std::vector<SuperResolutionInfo>& Variants) = 0;

    virtual void GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs,
                                   SuperResolutionSourceSettings&              Settings)
    {
        Settings = {};

        ValidateSourceSettingsAttribs(Attribs);

        float ScaleFactor = 1.0f;
        switch (Attribs.OptimizationType)
        {
                // clang-format off
            case SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_QUALITY:      ScaleFactor = 12.f / 16.f; break;
            case SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_QUALITY:     ScaleFactor = 11.f / 16.f; break;
            case SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED:         ScaleFactor = 9.f / 16.f; break;
            case SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_PERFORMANCE: ScaleFactor = 8.f / 16.f; break;
            case SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_PERFORMANCE:  ScaleFactor = 5.f / 16.f; break;
            default:                                                  ScaleFactor = 9.f / 16.f; break;
                // clang-format on
        }

        Settings.OptimalInputWidth  = std::max(1u, static_cast<Uint32>(Attribs.OutputWidth * ScaleFactor));
        Settings.OptimalInputHeight = std::max(1u, static_cast<Uint32>(Attribs.OutputHeight * ScaleFactor));
    }

    virtual void CreateSuperResolution(const SuperResolutionDesc& Desc,
                                       const SuperResolutionInfo& Info,
                                       ISuperResolution**         ppUpscaler) = 0;
};

} // namespace Diligent
