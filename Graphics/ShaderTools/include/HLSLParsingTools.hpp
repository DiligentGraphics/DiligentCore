/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include <unordered_map>

#include "ParsingTools.hpp"
#include "GraphicsTypes.h"
#include "HashUtils.hpp"
#include "SPIRVUtils.hpp"

namespace Diligent
{

namespace Parsing
{

/// Parses HLSL source code and extracts image formats and access modes
/// from RWTexture comments. Annotations are expected inside the RWTexture
/// template argument list, for example:
///
/// HLSL:
///     RWTexture2D<unorm float4 /*format=rgba8*/>                    g_Tex2D;
///     RWTexture3D</*format=rg16f*/ float2 /*access=write*/>         g_Tex3D;
///     RWTexture2D<unorm float4 /*format=rgba8*/ /*access=read*/>    g_Tex2D_Read;
///
/// Output:
///     {
///         { "g_Tex2D",      { TEX_FORMAT_RGBA8_UNORM, IMAGE_ACCESS_MODE_READ_WRITE } },
///         { "g_Tex3D",      { TEX_FORMAT_RG16_FLOAT,  IMAGE_ACCESS_MODE_WRITE      } },
///         { "g_Tex2D_Read", { TEX_FORMAT_RGBA8_UNORM, IMAGE_ACCESS_MODE_READ       } }
///     }
///
/// The following comment patterns are recognized:
///     /*format=<glsl_image_format>*/
///     /*access=read*/
///     /*access=write*/
///     /*access=read_write*/
///
/// If no access comment is present, the access mode defaults to IMAGE_ACCESS_MODE_UNKNOWN.
///
/// \param[in] HLSLSource - HLSL source code.
/// \return A map that associates RWTexture variable names with the image format
///         and access mode extracted from their comments.
///
/// \remarks Only RWTexture declarations in global scope are processed.
std::unordered_map<HashMapStringKey, ImageFormatAndAccess> ExtractGLSLImageFormatsAndAccessModeFromHLSL(const std::string& HLSLSource);

} // namespace Parsing

} // namespace Diligent
