/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#include "RayTracingBase.hpp"

namespace Diligent
{

void ValidateBottomLevelASDesc(const BottomLevelASDesc& Desc)
{
#define LOG_BLAS_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of Bottom-level AS '", (Desc.Name ? Desc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

    if (!((Desc.pBoxes != nullptr) ^ (Desc.pTriangles != nullptr)))
    {
        LOG_BLAS_ERROR_AND_THROW("Only one of pTriangles and pBoxes must be defined");
    }

    if (Desc.pBoxes == nullptr && Desc.BoxCount > 0)
    {
        LOG_BLAS_ERROR_AND_THROW("pBoxes is null but BoxCount is not 0");
    }

    if (Desc.pTriangles == nullptr && Desc.TriangleCount > 0)
    {
        LOG_BLAS_ERROR_AND_THROW("pTriangles is null but TriangleCount is not 0");
    }

#undef LOG_BLAS_ERROR_AND_THROW
}


void ValidateTopLevelASDesc(const TopLevelASDesc& Desc)
{
#define LOG_TLAS_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of Top-level AS '", (Desc.Name ? Desc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

    if (Desc.MaxInstanceCount == 0)
    {
        LOG_TLAS_ERROR_AND_THROW("MaxInstanceCount must not be zero");
    }

    if (!!(Desc.Flags & RAYTRACING_BUILD_AS_PREFER_FAST_TRACE) + !!(Desc.Flags & RAYTRACING_BUILD_AS_PREFER_FAST_BUILD))
    {
        LOG_TLAS_ERROR_AND_THROW("Used incompatible flags: RAYTRACING_BUILD_AS_PREFER_FAST_TRACE and RAYTRACING_BUILD_AS_PREFER_FAST_BUILD");
    }

#undef LOG_TLAS_ERROR_AND_THROW
}


void ValidateShaderBindingTableDesc(const ShaderBindingTableDesc& Desc)
{
#define LOG_SBT_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of Top-level AS '", (Desc.Name ? Desc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

    if (Desc.pPSO == nullptr)
    {
        LOG_SBT_ERROR_AND_THROW("PipelineState must be defined");
    }

#undef LOG_SBT_ERROR_AND_THROW
}

} // namespace Diligent
