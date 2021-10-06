/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "SamplerBase.hpp"

namespace Diligent
{

#define LOG_SAMPLER_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of sampler '", Desc.Name, "' is invalid: ", ##__VA_ARGS__)
#define VERIFY_SAMPLER(Expr, ...)                     \
    do                                                \
    {                                                 \
        if (!(Expr))                                  \
        {                                             \
            LOG_SAMPLER_ERROR_AND_THROW(__VA_ARGS__); \
        }                                             \
    } while (false)

void ValidateSamplerDesc(const SamplerDesc& Desc, const IRenderDevice* pDevice)
{
    if (Desc.Flags & (SAMPLER_FLAG_SUBSAMPLED | SAMPLER_FLAG_SUBSAMPLED_COARSE_RECONSTRUCTION))
    {
        VERIFY_SAMPLER(pDevice->GetAdapterInfo().ShadingRate.CapFlags & SHADING_RATE_CAP_FLAG_SUBSAMPLED_RENDER_TARGET,
                       "Subsampled sampler requires SHADING_RATE_CAP_FLAG_SUBSAMPLED_RENDER_TARGET capability");
    }
}

} // namespace Diligent
