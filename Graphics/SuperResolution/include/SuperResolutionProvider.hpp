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

        static constexpr float ScaleFactors[] = {
            24.f / 32.f, // MAX_QUALITY      (75%)
            22.f / 32.f, // HIGH_QUALITY     (69%)
            18.f / 32.f, // BALANCED         (56%)
            16.f / 32.f, // HIGH_PERFORMANCE (50%)
            11.f / 32.f, // MAX_PERFORMANCE  (34%)
        };

        static_assert(_countof(ScaleFactors) == SUPER_RESOLUTION_OPTIMIZATION_TYPE_COUNT,
                      "Scale factor table must match SUPER_RESOLUTION_OPTIMIZATION_TYPE_COUNT");

        const float ScaleFactor = Attribs.OptimizationType < SUPER_RESOLUTION_OPTIMIZATION_TYPE_COUNT ? ScaleFactors[Attribs.OptimizationType] : ScaleFactors[SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED];

        Settings.OptimalInputWidth  = std::max(1u, static_cast<Uint32>(Attribs.OutputWidth * ScaleFactor));
        Settings.OptimalInputHeight = std::max(1u, static_cast<Uint32>(Attribs.OutputHeight * ScaleFactor));
    }

    virtual void CreateSuperResolution(const SuperResolutionDesc& Desc,
                                       const SuperResolutionInfo& Info,
                                       ISuperResolution**         ppUpscaler) = 0;
};

} // namespace Diligent
