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

#include "GeometryPrimitives.h"

#include <array>

#include "DebugUtilities.hpp"
#include "BasicMath.hpp"
#include "DataBlobImpl.hpp"

namespace Diligent
{

Uint32 GetGeometryPrimitiveVertexSize(GEOMETRY_PRIMITIVE_VERTEX_FLAGS VertexFlags)
{
    return (((VertexFlags & GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION) ? sizeof(float3) : 0) +
            ((VertexFlags & GEOMETRY_PRIMITIVE_VERTEX_FLAG_NORMAL) ? sizeof(float3) : 0) +
            ((VertexFlags & GEOMETRY_PRIMITIVE_VERTEX_FLAG_TEXCOORD) ? sizeof(float2) : 0));
}

template <typename VertexHandlerType>
void CreateCubeGeometryInternal(Uint32                          NumSubdivisions,
                                GEOMETRY_PRIMITIVE_VERTEX_FLAGS VertexFlags,
                                IDataBlob**                     ppVertices,
                                IDataBlob**                     ppIndices,
                                GeometryPrimitiveInfo*          pInfo,
                                VertexHandlerType&&             HandleVertex)
{
    if (NumSubdivisions == 0)
    {
        UNEXPECTED("NumSubdivisions must be positive");
        return;
    }
    if (NumSubdivisions > 2048)
    {
        UNEXPECTED("NumSubdivisions is too large");
        return;
    }

    //   ______ ______
    //  |    .'|    .'|
    //  |  .'  |  .'  |
    //  |.'____|.'____|  NumSubdivisions = 2
    //  |    .'|    .'|
    //  |  .'  |  .'  |
    //  |.'____|.'____|
    //
    const Uint32 NumFaceVertices  = (NumSubdivisions + 1) * (NumSubdivisions + 1);
    const Uint32 NumFaceTriangles = NumSubdivisions * NumSubdivisions * 2;
    const Uint32 NumFaceIndices   = NumFaceTriangles * 3;
    const Uint32 VertexSize       = GetGeometryPrimitiveVertexSize(VertexFlags);
    const Uint32 NumFaces         = 6;
    const Uint32 VertexDataSize   = NumFaceVertices * NumFaces * VertexSize;
    const Uint32 IndexDataSize    = NumFaceIndices * NumFaces * sizeof(Uint32);

    if (pInfo != nullptr)
    {
        pInfo->NumVertices = NumFaceVertices * NumFaces;
        pInfo->NumIndices  = NumFaceIndices * NumFaces;
        pInfo->VertexSize  = VertexSize;
    }

    RefCntAutoPtr<DataBlobImpl> pVertexData;
    Uint8*                      pVert = nullptr;
    if (ppVertices != nullptr && VertexFlags != GEOMETRY_PRIMITIVE_VERTEX_FLAG_NONE)
    {
        pVertexData = DataBlobImpl::Create(VertexDataSize);
        DEV_CHECK_ERR(*ppVertices == nullptr, "*ppVertices is not null, which may cause memory leak");
        pVertexData->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ppVertices));
        pVert = pVertexData->GetDataPtr<Uint8>();
    }

    RefCntAutoPtr<DataBlobImpl> pIndexData;
    Uint32*                     pIdx = nullptr;
    if (ppIndices != nullptr)
    {
        pIndexData = DataBlobImpl::Create(IndexDataSize);
        DEV_CHECK_ERR(*ppIndices == nullptr, "*ppIndices is not null, which may cause memory leak");
        pIndexData->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ppIndices));
        pIdx = pIndexData->GetDataPtr<Uint32>();
    }

    static constexpr std::array<float3, NumFaces> FaceNormals{
        float3{+1, 0, 0},
        float3{-1, 0, 0},
        float3{0, +1, 0},
        float3{0, -1, 0},
        float3{0, 0, +1},
        float3{0, 0, -1},
    };

    for (Uint32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
    {
        if (pVert != nullptr)
        {
            // 6 ______7______ 8
            //  |    .'|    .'|
            //  |  .'  |  .'  |
            //  |.'____|.'____|
            // 3|    .'|4   .'|5
            //  |  .'  |  .'  |
            //  |.'____|.'____|
            // 0       1      2

            for (Uint32 y = 0; y <= NumSubdivisions; ++y)
            {
                for (Uint32 x = 0; x <= NumSubdivisions; ++x)
                {
                    float2 UV{
                        static_cast<float>(x) / NumSubdivisions,
                        static_cast<float>(y) / NumSubdivisions,
                    };

                    float2 XY{
                        UV.x - 0.5f,
                        0.5f - UV.y,
                    };

                    float3 Pos;
                    switch (FaceIndex)
                    {
                        case 0: Pos = float3{+0.5f, XY.y, +XY.x}; break;
                        case 1: Pos = float3{-0.5f, XY.y, -XY.x}; break;
                        case 2: Pos = float3{XY.x, +0.5f, +XY.y}; break;
                        case 3: Pos = float3{XY.x, -0.5f, -XY.y}; break;
                        case 4: Pos = float3{-XY.x, XY.y, +0.5f}; break;
                        case 5: Pos = float3{+XY.x, XY.y, -0.5f}; break;
                    }

                    float3 Normal = FaceNormals[FaceIndex];
                    HandleVertex(Pos, Normal, UV);

                    if (VertexFlags & GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION)
                    {
                        memcpy(pVert, &Pos, sizeof(Pos));
                        pVert += sizeof(Pos);
                    }

                    if (VertexFlags & GEOMETRY_PRIMITIVE_VERTEX_FLAG_NORMAL)
                    {
                        memcpy(pVert, &Normal, sizeof(Normal));
                        pVert += sizeof(Normal);
                    }

                    if (VertexFlags & GEOMETRY_PRIMITIVE_VERTEX_FLAG_TEXCOORD)
                    {
                        memcpy(pVert, &UV, sizeof(UV));
                        pVert += sizeof(UV);
                    }
                }
            }
        }

        if (pIdx != nullptr)
        {
            Uint32 FaceBaseVertex = FaceIndex * NumFaceVertices;
            for (Uint32 y = 0; y < NumSubdivisions; ++y)
            {
                for (Uint32 x = 0; x < NumSubdivisions; ++x)
                {
                    //  01     11
                    //   *-----*
                    //   |   .'|
                    //   | .'  |
                    //   *'----*
                    //  00     10
                    Uint32 v00 = FaceBaseVertex + y * (NumSubdivisions + 1) + x;
                    Uint32 v10 = v00 + 1;
                    Uint32 v01 = v00 + NumSubdivisions + 1;
                    Uint32 v11 = v01 + 1;

                    *pIdx++ = v00;
                    *pIdx++ = v10;
                    *pIdx++ = v11;

                    *pIdx++ = v00;
                    *pIdx++ = v11;
                    *pIdx++ = v01;
                }
            }
        }
    }

    VERIFY_EXPR(pVert == nullptr || pVert == pVertexData->GetConstDataPtr<Uint8>() + VertexDataSize);
    VERIFY_EXPR(pIdx == nullptr || pIdx == pIndexData->GetConstDataPtr<Uint32>() + IndexDataSize / sizeof(Uint32));
}

void CreateCubeGeometry(const CubeGeometryPrimitiveAttributes& Attribs,
                        IDataBlob**                            ppVertices,
                        IDataBlob**                            ppIndices,
                        GeometryPrimitiveInfo*                 pInfo)
{
    const float Size = Attribs.Size;
    if (Size <= 0)
    {
        UNEXPECTED("Size must be positive");
        return;
    }

    CreateCubeGeometryInternal(Attribs.NumSubdivisions,
                               Attribs.VertexFlags,
                               ppVertices,
                               ppIndices,
                               pInfo,
                               [&](float3& Pos, float3& Normal, float2& UV) {
                                   Pos *= Size;
                               });
}

void CreateSphereGeometry(const SphereGeometryPrimitiveAttributes& Attribs,
                          IDataBlob**                              ppVertices,
                          IDataBlob**                              ppIndices,
                          GeometryPrimitiveInfo*                   pInfo)
{
    const float Radius = Attribs.Radius;
    if (Radius <= 0)
    {
        UNEXPECTED("Radius must be positive");
        return;
    }

    CreateCubeGeometryInternal(Attribs.NumSubdivisions,
                               Attribs.VertexFlags,
                               ppVertices,
                               ppIndices,
                               pInfo,
                               [&](float3& Pos, float3& Normal, float2& UV) {
                                   Normal = normalize(Pos);
                                   Pos    = Normal * Radius;

                                   UV.x = 0.5f + atan2(Normal.z, Normal.x) / (2 * PI_F);
                                   UV.y = 0.5f - asin(Normal.y) / PI_F;
                               });
}

void CreateGeometryPrimitive(const GeometryPrimitiveAttributes& Attribs,
                             IDataBlob**                        ppVertices,
                             IDataBlob**                        ppIndices,
                             GeometryPrimitiveInfo*             pInfo)
{
    DEV_CHECK_ERR(ppVertices == nullptr || *ppVertices == nullptr, "*ppVertices is not null which may cause memory leaks");
    DEV_CHECK_ERR(ppIndices == nullptr || *ppIndices == nullptr, "*ppIndices is not null which may cause memory leaks");

    static_assert(GEOMETRY_PRIMITIVE_TYPE_COUNT == 3, "Please update the switch below to handle the new geometry primitive type");
    switch (Attribs.Type)
    {
        case GEOMETRY_PRIMITIVE_TYPE_UNDEFINED:
            UNEXPECTED("Undefined geometry primitive type");
            break;

        case GEOMETRY_PRIMITIVE_TYPE_CUBE:
            CreateCubeGeometry(static_cast<const CubeGeometryPrimitiveAttributes&>(Attribs), ppVertices, ppIndices, pInfo);
            break;

        case GEOMETRY_PRIMITIVE_TYPE_SPHERE:
            CreateSphereGeometry(static_cast<const SphereGeometryPrimitiveAttributes&>(Attribs), ppVertices, ppIndices, pInfo);
            break;

        default:
            UNEXPECTED("Unknown geometry primitive type");
    }
}

} // namespace Diligent

extern "C"
{
    Diligent::Uint32 Diligent_GetGeometryPrimitiveVertexSize(Diligent::GEOMETRY_PRIMITIVE_VERTEX_FLAGS VertexFlags)
    {
        return Diligent::GetGeometryPrimitiveVertexSize(VertexFlags);
    }

    void Diligent_CreateGeometryPrimitive(const Diligent::GeometryPrimitiveAttributes& Attribs,
                                          Diligent::IDataBlob**                        ppVertices,
                                          Diligent::IDataBlob**                        ppIndices,
                                          Diligent::GeometryPrimitiveInfo*             pInfo)
    {
        Diligent::CreateGeometryPrimitive(Attribs, ppVertices, ppIndices, pInfo);
    }
}
