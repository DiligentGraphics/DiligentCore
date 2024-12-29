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

#include "../../Primitives/interface/DataBlob.h"
#include "../../Primitives/interface/FlagEnum.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

/// Geometry primitive vertex flags.
// clang-format off
DILIGENT_TYPED_ENUM(GEOMETRY_PRIMITIVE_VERTEX_FLAGS, Uint32)
{
    /// No flags.
    GEOMETRY_PRIMITIVE_VERTEX_FLAG_NONE = 0u,

    /// The geometry primitive vertex contains position.
    GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION = 1u << 0u,

    /// The geometry primitive vertex contains normal.
    GEOMETRY_PRIMITIVE_VERTEX_FLAG_NORMAL = 1u << 1u,

    /// The geometry primitive vertex contains texture coordinates.
    GEOMETRY_PRIMITIVE_VERTEX_FLAG_TEXCOORD = 1u << 2u,

    GEOMETRY_PRIMITIVE_VERTEX_FLAG_LAST = GEOMETRY_PRIMITIVE_VERTEX_FLAG_TEXCOORD,

    GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL = GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION |
                                         GEOMETRY_PRIMITIVE_VERTEX_FLAG_NORMAL |
                                          GEOMETRY_PRIMITIVE_VERTEX_FLAG_TEXCOORD,

    GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_NORM = GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION |
											  GEOMETRY_PRIMITIVE_VERTEX_FLAG_NORMAL,

    GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX = GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION |
                                             GEOMETRY_PRIMITIVE_VERTEX_FLAG_TEXCOORD,
};
// clang-format on

DEFINE_FLAG_ENUM_OPERATORS(GEOMETRY_PRIMITIVE_VERTEX_FLAGS)

/// Returns the size of the geometry primitive vertex in bytes.
Uint32 GetGeometryPrimitiveVertexSize(GEOMETRY_PRIMITIVE_VERTEX_FLAGS VertexFlags);

/// Creates a cube geometry.
///
/// \param [in]  Size               The size of the cube.
///                                 The cube is centered at (0, 0, 0) and has the size of Size x Size x Size.
///                                 If the cube size is 1, the coordinates of the cube vertices are in the range [-0.5, 0.5].
/// \param [in]  NumSubdivisions    The number of subdivisions.
///                                 The cube faces are subdivided into Subdivision x Subdivision quads.
/// \param [in]  VertexFlags        Flags that specify which vertex components to include in the output vertices.
/// \param [out] ppVertices         Address of the memory location where the pointer to the output vertex data blob will be stored.
///                                 The vertex components are stored as interleaved floating-point values.
///                                 For example, if VertexFlags = GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_NORM, the vertex data will
///                                 be stored as follows:
///                                     P0, N0, P1, N1, ..., Pn, Nn.
/// \param [out] ppIndices          Address of the memory location where the pointer to the output index data blob will be stored.
///                                 Index data is stored as 32-bit unsigned integers representing the triangle list.
/// \param [out] pNumVertices       Address of the memory location where the number of vertices will be stored.
///                                 This parameter can be null.
/// \param [out] pNumIndices        Address of the memory location where the number of indices will be stored.
///                                 This parameter can be null.
void DILIGENT_GLOBAL_FUNCTION(CreateCubeGeometry)(float                           Size,
                                                  Uint32                          NumSubdivisions,
                                                  GEOMETRY_PRIMITIVE_VERTEX_FLAGS VertexFlags,
                                                  IDataBlob**                     ppVertices,
                                                  IDataBlob**                     ppIndices,
                                                  Uint32* pNumVertices            DEFAULT_VALUE(nullptr),
                                                  Uint32* pNumIndices             DEFAULT_VALUE(nullptr));

DILIGENT_END_NAMESPACE // namespace Diligent
