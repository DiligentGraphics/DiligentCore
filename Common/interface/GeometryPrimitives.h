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

#include "../../Primitives/interface/DefineRefMacro.h"

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

/// Geometry primitive types.
// clang-format off
DILIGENT_TYPED_ENUM(GEOMETRY_PRIMITIVE_TYPE, Uint32)
{
    /// Geometry primitive type is undefined.
    GEOMETRY_PRIMITIVE_TYPE_UNDEFINED = 0u,

    /// Cube geometry primitive type.
    GEOMETRY_PRIMITIVE_TYPE_CUBE,

    /// Sphere geometry primitive type.
    GEOMETRY_PRIMITIVE_TYPE_SPHERE,

    GEOMETRY_PRIMITIVE_TYPE_COUNT
};
// clang-format on

/// Geometry primitive attributes.
struct GeometryPrimitiveAttributes
{
    /// The geometry primitive type, see Diligent::GEOMETRY_PRIMITIVE_TYPE.
    GEOMETRY_PRIMITIVE_TYPE Type DEFAULT_INITIALIZER(GEOMETRY_PRIMITIVE_TYPE_UNDEFINED);

    /// Vertex flags that specify which vertex components to include in the output vertices,
    /// see Diligent::GEOMETRY_PRIMITIVE_VERTEX_FLAGS.
    GEOMETRY_PRIMITIVE_VERTEX_FLAGS VertexFlags DEFAULT_INITIALIZER(GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL);

    /// The number of subdivisions.
    ///
    /// \remarks    This parameter defines the fidelity of the geometry primitive.
    ///             For example, for a cube geometry primitive, the cube faces are subdivided
    ///             into Subdivision x Subdivision quads, producing (Subdivision + 1)^2 vertices
    ///             per face.
    Uint32 NumSubdivisions DEFAULT_INITIALIZER(0);

#if DILIGENT_CPP_INTERFACE
    GeometryPrimitiveAttributes() noexcept = default;

    explicit GeometryPrimitiveAttributes(GEOMETRY_PRIMITIVE_TYPE         _Type,
                                         GEOMETRY_PRIMITIVE_VERTEX_FLAGS _VertexFlags    = GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL,
                                         Uint32                          _NumSubdivision = 1) noexcept :
        Type{_Type},
        VertexFlags{_VertexFlags},
        NumSubdivisions{_NumSubdivision}
    {}
#endif
};
typedef struct GeometryPrimitiveAttributes GeometryPrimitiveAttributes;

/// Cube geometry primitive attributes.
// clang-format off
struct CubeGeometryPrimitiveAttributes DILIGENT_DERIVE(GeometryPrimitiveAttributes)

    /// The size of the cube.
    /// The cube is centered at (0, 0, 0) and has the size of Size x Size x Size.
    /// If the cube size is 1, the coordinates of the cube vertices are in the range [-0.5, 0.5].
    float Size DEFAULT_INITIALIZER(1.f);

#if DILIGENT_CPP_INTERFACE
	explicit CubeGeometryPrimitiveAttributes(float                           _Size           = 1,
                                             GEOMETRY_PRIMITIVE_VERTEX_FLAGS _VertexFlags    = GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL,
                                             Uint32                          _NumSubdivision = 1) noexcept :
        GeometryPrimitiveAttributes{GEOMETRY_PRIMITIVE_TYPE_CUBE, _VertexFlags, _NumSubdivision},
        Size{_Size}
    {}
#endif
};
// clang-format on


/// Sphere geometry primitive attributes.
// clang-format off
struct SphereGeometryPrimitiveAttributes DILIGENT_DERIVE(GeometryPrimitiveAttributes)

    /// Sphere radius.
    float Radius DEFAULT_INITIALIZER(1.f);

#if DILIGENT_CPP_INTERFACE
	explicit SphereGeometryPrimitiveAttributes(float                           _Radius         = 1,
                                               GEOMETRY_PRIMITIVE_VERTEX_FLAGS _VertexFlags    = GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL,
                                               Uint32                          _NumSubdivision = 1) noexcept :
        GeometryPrimitiveAttributes{GEOMETRY_PRIMITIVE_TYPE_SPHERE, _VertexFlags, _NumSubdivision},
        Radius{_Radius}
    {}
#endif
};
// clang-format on

/// Geometry primitive info.
struct GeometryPrimitiveInfo
{
    /// The number of vertices.
    Uint32 NumVertices DEFAULT_INITIALIZER(0);

    /// The number of indices.
    Uint32 NumIndices DEFAULT_INITIALIZER(0);

    /// The size of the vertex in bytes.
    Uint32 VertexSize DEFAULT_INITIALIZER(0);
};
typedef struct GeometryPrimitiveInfo GeometryPrimitiveInfo;

/// Returns the size of the geometry primitive vertex in bytes.
Uint32 GetGeometryPrimitiveVertexSize(GEOMETRY_PRIMITIVE_VERTEX_FLAGS VertexFlags);

/// Creates a geometry primitive
///
/// \param [in]  Attribs    - Geometry primitive attributes, see Diligent::GeometryPrimitiveAttributes.
/// \param [out] ppVertices - Address of the memory location where the pointer to the output vertex data blob will be stored.
///                           The vertex components are stored as interleaved floating-point values.
///                           For example, if VertexFlags = GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_NORM, the vertex data will
///                           be stored as follows:
///                             P0, N0, P1, N1, ..., Pn, Nn.
/// \param [out] ppIndices  - Address of the memory location where the pointer to the output index data blob will be stored.
///                           Index data is stored as 32-bit unsigned integers representing the triangle list.
/// \param [out] pInfo      - A pointer to the structure that will receive information about the created geometry primitive.
///                           See Diligent::GeometryPrimitiveInfo.
void DILIGENT_GLOBAL_FUNCTION(CreateGeometryPrimitive)(const GeometryPrimitiveAttributes REF Attribs,
                                                       IDataBlob**                           ppVertices,
                                                       IDataBlob**                           ppIndices,
                                                       GeometryPrimitiveInfo* pInfo          DEFAULT_VALUE(nullptr));
#include "../../Primitives/interface/UndefRefMacro.h"

DILIGENT_END_NAMESPACE // namespace Diligent
