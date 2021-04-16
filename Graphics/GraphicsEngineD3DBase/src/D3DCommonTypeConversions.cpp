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

#include "ShaderResources.hpp"
#include "D3DCommonTypeConversions.hpp"
#include <d3dcommon.h>

namespace Diligent
{

RESOURCE_DIMENSION D3DSrvDimensionToResourceDimension(D3D_SRV_DIMENSION SrvDim)
{
    switch (SrvDim)
    {
        // clang-format off
        case D3D_SRV_DIMENSION_BUFFER:           return RESOURCE_DIM_BUFFER;
        case D3D_SRV_DIMENSION_TEXTURE1D:        return RESOURCE_DIM_TEX_1D;
        case D3D_SRV_DIMENSION_TEXTURE1DARRAY:   return RESOURCE_DIM_TEX_1D_ARRAY;
        case D3D_SRV_DIMENSION_TEXTURE2D:        return RESOURCE_DIM_TEX_2D;
        case D3D_SRV_DIMENSION_TEXTURE2DARRAY:   return RESOURCE_DIM_TEX_2D_ARRAY;
        case D3D_SRV_DIMENSION_TEXTURE2DMS:      return RESOURCE_DIM_TEX_2D;
        case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY: return RESOURCE_DIM_TEX_2D_ARRAY;
        case D3D_SRV_DIMENSION_TEXTURE3D:        return RESOURCE_DIM_TEX_3D;
        case D3D_SRV_DIMENSION_TEXTURECUBE:      return RESOURCE_DIM_TEX_CUBE;
        case D3D_SRV_DIMENSION_TEXTURECUBEARRAY: return RESOURCE_DIM_TEX_CUBE_ARRAY;
        // clang-format on
        default:
            return RESOURCE_DIM_BUFFER;
    }
}

void VerifyD3DResourceMerge(const PipelineStateDesc&        PSODesc,
                            const D3DShaderResourceAttribs& ExistingRes,
                            const D3DShaderResourceAttribs& NewResAttribs) noexcept(false)
{
#define LOG_RESOURCE_MERGE_ERROR_AND_THROW(PropertyName)                                                          \
    LOG_ERROR_AND_THROW("Shader variable '", NewResAttribs.Name,                                                  \
                        "' is shared between multiple shaders in pipeline '", (PSODesc.Name ? PSODesc.Name : ""), \
                        "', but its " PropertyName " varies. A variable shared between multiple shaders "         \
                        "must be defined identically in all shaders. Either use separate variables for "          \
                        "different shader stages, change resource name or make sure that " PropertyName " is consistent.");

    if (ExistingRes.GetInputType() != NewResAttribs.GetInputType())
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("input type");

    if (ExistingRes.GetSRVDimension() != NewResAttribs.GetSRVDimension())
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("resource dimension");

    if (ExistingRes.BindCount != NewResAttribs.BindCount)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("array size");

    if (ExistingRes.IsMultisample() != NewResAttribs.IsMultisample())
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("mutlisample state");
#undef LOG_RESOURCE_MERGE_ERROR_AND_THROW
}

} // namespace Diligent
