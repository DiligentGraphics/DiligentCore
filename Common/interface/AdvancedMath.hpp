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

#pragma once

#include "../../Platforms/interface/PlatformDefinitions.h"
#include "../../Primitives/interface/FlagEnum.h"

#include "BasicMath.hpp"

namespace Diligent
{

// Structure describing a plane
struct Plane3D
{
    float3 Normal;
    float  Distance = 0; //Distance from the coordinate system origin to the plane along the normal direction
};

struct ViewFrustum
{
    Plane3D LeftPlane, RightPlane, BottomPlane, TopPlane, NearPlane, FarPlane;
};

struct ViewFrustumExt : public ViewFrustum
{
    float3 FrustumCorners[8];
};

// For OpenGL, matrix is still considered row-major. The only difference is that
// near clip plane is at -1, not 0.
inline void ExtractViewFrustumPlanesFromMatrix(const float4x4& Matrix, ViewFrustum& Frustum, bool bIsOpenGL)
{
    // For more details, see Gribb G., Hartmann K., "Fast Extraction of Viewing Frustum Planes from the
    // World-View-Projection Matrix" (the paper is available at
    // http://gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf)

    // Left clipping plane
    Frustum.LeftPlane.Normal.x = Matrix._14 + Matrix._11;
    Frustum.LeftPlane.Normal.y = Matrix._24 + Matrix._21;
    Frustum.LeftPlane.Normal.z = Matrix._34 + Matrix._31;
    Frustum.LeftPlane.Distance = Matrix._44 + Matrix._41;

    // Right clipping plane
    Frustum.RightPlane.Normal.x = Matrix._14 - Matrix._11;
    Frustum.RightPlane.Normal.y = Matrix._24 - Matrix._21;
    Frustum.RightPlane.Normal.z = Matrix._34 - Matrix._31;
    Frustum.RightPlane.Distance = Matrix._44 - Matrix._41;

    // Top clipping plane
    Frustum.TopPlane.Normal.x = Matrix._14 - Matrix._12;
    Frustum.TopPlane.Normal.y = Matrix._24 - Matrix._22;
    Frustum.TopPlane.Normal.z = Matrix._34 - Matrix._32;
    Frustum.TopPlane.Distance = Matrix._44 - Matrix._42;

    // Bottom clipping plane
    Frustum.BottomPlane.Normal.x = Matrix._14 + Matrix._12;
    Frustum.BottomPlane.Normal.y = Matrix._24 + Matrix._22;
    Frustum.BottomPlane.Normal.z = Matrix._34 + Matrix._32;
    Frustum.BottomPlane.Distance = Matrix._44 + Matrix._42;

    // Near clipping plane
    if (bIsOpenGL)
    {
        // -w <= z <= w
        Frustum.NearPlane.Normal.x = Matrix._14 + Matrix._13;
        Frustum.NearPlane.Normal.y = Matrix._24 + Matrix._23;
        Frustum.NearPlane.Normal.z = Matrix._34 + Matrix._33;
        Frustum.NearPlane.Distance = Matrix._44 + Matrix._43;
    }
    else
    {
        // 0 <= z <= w
        Frustum.NearPlane.Normal.x = Matrix._13;
        Frustum.NearPlane.Normal.y = Matrix._23;
        Frustum.NearPlane.Normal.z = Matrix._33;
        Frustum.NearPlane.Distance = Matrix._43;
    }

    // Far clipping plane
    Frustum.FarPlane.Normal.x = Matrix._14 - Matrix._13;
    Frustum.FarPlane.Normal.y = Matrix._24 - Matrix._23;
    Frustum.FarPlane.Normal.z = Matrix._34 - Matrix._33;
    Frustum.FarPlane.Distance = Matrix._44 - Matrix._43;
}

inline void ExtractViewFrustumPlanesFromMatrix(const float4x4& Matrix, ViewFrustumExt& FrustumExt, bool bIsOpenGL)
{
    ExtractViewFrustumPlanesFromMatrix(Matrix, static_cast<ViewFrustum&>(FrustumExt), bIsOpenGL);

    // Compute frustum corners
    float4x4 InvMatrix = Matrix.Inverse();

    float nearClipZ = bIsOpenGL ? -1.f : 0.f;

    static const float3 ProjSpaceCorners[] =
        {
            // clang-format off
            float3(-1, -1, nearClipZ),
            float3( 1, -1, nearClipZ),
            float3(-1,  1, nearClipZ),
            float3( 1,  1, nearClipZ),

            float3(-1, -1, 1),
            float3( 1, -1, 1),
            float3(-1,  1, 1),
            float3( 1,  1, 1),
            // clang-format on
        };

    for (int i = 0; i < 8; ++i)
        FrustumExt.FrustumCorners[i] = ProjSpaceCorners[i] * InvMatrix;
}

struct BoundBox
{
    float3 Min;
    float3 Max;
};

enum class BoxVisibility
{
    //  Bounding box is guaranteed to be outside of the view frustum
    //                 .
    //             . ' |
    //         . '     |
    //       |         |
    //         .       |
    //       ___ ' .   |
    //      |   |    ' .
    //      |___|
    //
    Invisible,

    //  Bounding box intersects the frustum
    //                 .
    //             . ' |
    //         . '     |
    //       |         |
    //        _.__     |
    //       |   '|.   |
    //       |____|  ' .
    //
    Intersecting,

    //  Bounding box is fully inside the view frustum
    //                 .
    //             . ' |
    //         . '___  |
    //       |   |   | |
    //         . |___| |
    //           ' .   |
    //               ' .
    //
    FullyVisible
};

/// Returns the nearest bounding box corner along the given direction
inline float3 GetBoxNearestCorner(const float3& Direction, const BoundBox& Box)
{
    return float3 //
        {
            (Direction.x > 0) ? Box.Min.x : Box.Max.x,
            (Direction.y > 0) ? Box.Min.y : Box.Max.y,
            (Direction.z > 0) ? Box.Min.z : Box.Max.z //
        };
}

/// Returns the farthest bounding box corner along the given direction
inline float3 GetBoxFarthestCorner(const float3& Direction, const BoundBox& Box)
{
    return float3 //
        {
            (Direction.x > 0) ? Box.Max.x : Box.Min.x,
            (Direction.y > 0) ? Box.Max.y : Box.Min.y,
            (Direction.z > 0) ? Box.Max.z : Box.Min.z //
        };
}

inline BoxVisibility GetBoxVisibilityAgainstPlane(const Plane3D& Plane, const BoundBox& Box)
{
    const float3& Normal = Plane.Normal;

    float3 MaxPoint //
        {
            (Normal.x > 0) ? Box.Max.x : Box.Min.x,
            (Normal.y > 0) ? Box.Max.y : Box.Min.y,
            (Normal.z > 0) ? Box.Max.z : Box.Min.z //
        };
    float DMax = dot(MaxPoint, Normal) + Plane.Distance;
    if (DMax < 0)
        return BoxVisibility::Invisible;

    float3 MinPoint //
        {
            (Normal.x > 0) ? Box.Min.x : Box.Max.x,
            (Normal.y > 0) ? Box.Min.y : Box.Max.y,
            (Normal.z > 0) ? Box.Min.z : Box.Max.z //
        };
    float DMin = dot(MinPoint, Normal) + Plane.Distance;
    if (DMin > 0)
        return BoxVisibility::FullyVisible;

    return BoxVisibility::Intersecting;
}

// Flags must be listed in the same order as planes in the ViewFrustum struct:
// LeftPlane, RightPlane, BottomPlane, TopPlane, NearPlane, FarPlane
enum FRUSTUM_PLANE_FLAGS : Uint32
{
    FRUSTUM_PLANE_FLAG_NONE         = 0x00,
    FRUSTUM_PLANE_FLAG_LEFT_PLANE   = 0x01,
    FRUSTUM_PLANE_FLAG_RIGHT_PLANE  = 0x02,
    FRUSTUM_PLANE_FLAG_BOTTOM_PLANE = 0x04,
    FRUSTUM_PLANE_FLAG_TOP_PLANE    = 0x08,
    FRUSTUM_PLANE_FLAG_NEAR_PLANE   = 0x10,
    FRUSTUM_PLANE_FLAG_FAR_PLANE    = 0x20,

    FRUSTUM_PLANE_FLAG_FULL_FRUSTUM = FRUSTUM_PLANE_FLAG_LEFT_PLANE |
        FRUSTUM_PLANE_FLAG_RIGHT_PLANE |
        FRUSTUM_PLANE_FLAG_BOTTOM_PLANE |
        FRUSTUM_PLANE_FLAG_TOP_PLANE |
        FRUSTUM_PLANE_FLAG_NEAR_PLANE |
        FRUSTUM_PLANE_FLAG_FAR_PLANE,

    FRUSTUM_PLANE_FLAG_OPEN_NEAR = FRUSTUM_PLANE_FLAG_LEFT_PLANE |
        FRUSTUM_PLANE_FLAG_RIGHT_PLANE |
        FRUSTUM_PLANE_FLAG_BOTTOM_PLANE |
        FRUSTUM_PLANE_FLAG_TOP_PLANE |
        FRUSTUM_PLANE_FLAG_FAR_PLANE
};
DEFINE_FLAG_ENUM_OPERATORS(FRUSTUM_PLANE_FLAGS);

// Tests if bounding box is visible by the camera
inline BoxVisibility GetBoxVisibility(const ViewFrustum&  ViewFrustum,
                                      const BoundBox&     Box,
                                      FRUSTUM_PLANE_FLAGS PlaneFlags = FRUSTUM_PLANE_FLAG_FULL_FRUSTUM)
{
    const Plane3D* pPlanes = reinterpret_cast<const Plane3D*>(&ViewFrustum);

    int NumPlanesInside = 0;
    int TotalPlanes     = 0;
    for (int iViewFrustumPlane = 0; iViewFrustumPlane < 6; iViewFrustumPlane++)
    {
        if ((PlaneFlags & (1 << iViewFrustumPlane)) == 0)
            continue;

        const Plane3D& CurrPlane = pPlanes[iViewFrustumPlane];

        auto VisibilityAgainstPlane = GetBoxVisibilityAgainstPlane(CurrPlane, Box);

        // If bounding box is "behind" one of the planes, it is definitely invisible
        if (VisibilityAgainstPlane == BoxVisibility::Invisible)
            return BoxVisibility::Invisible;

        // Count total number of planes the bound box is inside
        if (VisibilityAgainstPlane == BoxVisibility::FullyVisible)
            ++NumPlanesInside;

        ++TotalPlanes;
    }

    return (NumPlanesInside == TotalPlanes) ? BoxVisibility::FullyVisible : BoxVisibility::Intersecting;
}

inline BoxVisibility GetBoxVisibility(const ViewFrustumExt& ViewFrustumExt,
                                      const BoundBox&       Box,
                                      FRUSTUM_PLANE_FLAGS   PlaneFlags = FRUSTUM_PLANE_FLAG_FULL_FRUSTUM)
{
    auto Visibility = GetBoxVisibility(static_cast<const ViewFrustum&>(ViewFrustumExt), Box, PlaneFlags);
    if (Visibility == BoxVisibility::FullyVisible || Visibility == BoxVisibility::Invisible)
        return Visibility;

    if ((PlaneFlags & FRUSTUM_PLANE_FLAG_FULL_FRUSTUM) == FRUSTUM_PLANE_FLAG_FULL_FRUSTUM)
    {
        // Additionally test if the whole frustum is outside one of
        // the the bounding box planes. This helps in the following situation:
        //
        //
        //       .
        //      /   '  .       .
        //     / AABB  /   . ' |
        //    /       /. '     |
        //       ' . / |       |
        //       * .   |       |
        //           ' .       |
        //               ' .   |
        //                   ' .

        // Test all frustum corners against every bound box plane
        for (int iBoundBoxPlane = 0; iBoundBoxPlane < 6; ++iBoundBoxPlane)
        {
            // struct BoundBox
            // {
            //     float3 Min;
            //     float3 Max;
            // };
            float CurrPlaneCoord = reinterpret_cast<const float*>(&Box)[iBoundBoxPlane];
            // Bound box normal is one of the axis, so we just need to pick the right coordinate
            int iCoordOrder = iBoundBoxPlane % 3; // 0, 1, 2, 0, 1, 2
            // Since plane normal is directed along one of the axis, we only need to select
            // if it is pointing in the positive (max planes) or negative (min planes) direction
            float fSign              = (iBoundBoxPlane >= 3) ? +1.f : -1.f;
            bool  bAllCornersOutside = true;
            for (int iCorner = 0; iCorner < 8; iCorner++)
            {
                // Pick the frustum corner coordinate
                float CurrCornerCoord = ViewFrustumExt.FrustumCorners[iCorner][iCoordOrder];
                // Dot product is simply the coordinate difference multiplied by the sign
                if (fSign * (CurrPlaneCoord - CurrCornerCoord) > 0)
                {
                    bAllCornersOutside = false;
                    break;
                }
            }
            if (bAllCornersOutside)
                return BoxVisibility::Invisible;
        }
    }

    return BoxVisibility::Intersecting;
}

inline float GetPointToBoxDistance(const BoundBox& BndBox, const float3& Pos)
{
    VERIFY_EXPR(BndBox.Max.x >= BndBox.Min.x &&
                BndBox.Max.y >= BndBox.Min.y &&
                BndBox.Max.z >= BndBox.Min.z);
    float fdX = (Pos.x > BndBox.Max.x) ? (Pos.x - BndBox.Max.x) : ((Pos.x < BndBox.Min.x) ? (BndBox.Min.x - Pos.x) : 0.f);
    float fdY = (Pos.y > BndBox.Max.y) ? (Pos.y - BndBox.Max.y) : ((Pos.y < BndBox.Min.y) ? (BndBox.Min.y - Pos.y) : 0.f);
    float fdZ = (Pos.z > BndBox.Max.z) ? (Pos.z - BndBox.Max.z) : ((Pos.z < BndBox.Min.z) ? (BndBox.Min.z - Pos.z) : 0.f);
    VERIFY_EXPR(fdX >= 0 && fdY >= 0 && fdZ >= 0);

    float3 RangeVec(fdX, fdY, fdZ);
    return length(RangeVec);
}

inline bool operator==(const Plane3D& p1, const Plane3D& p2)
{
    return p1.Normal == p2.Normal &&
        p1.Distance == p2.Distance;
}

inline bool operator==(const ViewFrustum& f1, const ViewFrustum& f2)
{
    // clang-format off
    return f1.LeftPlane   == f2.LeftPlane   &&
           f1.RightPlane  == f2.RightPlane  &&
           f1.BottomPlane == f2.BottomPlane &&
           f1.TopPlane    == f2.TopPlane    &&
           f1.NearPlane   == f2.NearPlane   &&
           f1.FarPlane    == f2.FarPlane;
    // clang-format on
}

inline bool operator==(const ViewFrustumExt& f1, const ViewFrustumExt& f2)
{
    if (!(static_cast<const ViewFrustum&>(f1) == static_cast<const ViewFrustum&>(f2)))
        return false;

    for (int c = 0; c < _countof(f1.FrustumCorners); ++c)
        if (f1.FrustumCorners[c] != f2.FrustumCorners[c])
            return false;

    return true;
}

template <typename T, typename Y>
T HermiteSpline(T f0, // F(0)
                T f1, // F(1)
                T t0, // F'(0)
                T t1, // F'(1)
                Y x)
{
    // https://en.wikipedia.org/wiki/Cubic_Hermite_spline
    auto x2 = x * x;
    auto x3 = x2 * x;
    return (2 * x3 - 3 * x2 + 1) * f0 + (x3 - 2 * x2 + x) * t0 + (-2 * x3 + 3 * x2) * f1 + (x3 - x2) * t1;
}

// Retuns the minimum bounding sphere of a view frustum
inline void GetFrustumMinimumBoundingSphere(float   Proj_00,   // cot(HorzFOV / 2)
                                            float   Proj_11,   // cot(VertFOV / 2) == proj_00 / AspectRatio
                                            float   NearPlane, // Near clip plane
                                            float   FarPlane,  // Far clip plane
                                            float3& Center,    // Sphere center == (0, 0, c)
                                            float&  Radius     // Sphere radius
)
{
    // https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
    VERIFY_EXPR(FarPlane >= NearPlane);
    auto k2 = 1.f / (Proj_00 * Proj_00) + 1.f / (Proj_11 * Proj_11);
    if (k2 > (FarPlane - NearPlane) / (FarPlane + NearPlane))
    {
        Center = float3(0, 0, FarPlane);
        Radius = FarPlane * std::sqrt(k2);
    }
    else
    {
        Center = float3(0, 0, 0.5f * (FarPlane + NearPlane) * (1 + k2));
        Radius = 0.5f * std::sqrt((FarPlane - NearPlane) * (FarPlane - NearPlane) + 2 * (FarPlane * FarPlane + NearPlane * NearPlane) * k2 + (FarPlane + NearPlane) * (FarPlane + NearPlane) * k2 * k2);
    }
}

} // namespace Diligent

namespace std
{

template <>
struct hash<Diligent::Plane3D>
{
    size_t operator()(const Diligent::Plane3D& Plane) const
    {
        return Diligent::ComputeHash(Plane.Normal, Plane.Distance);
    }
};

template <>
struct hash<Diligent::ViewFrustum>
{
    size_t operator()(const Diligent::ViewFrustum& Frustum) const
    {
        return Diligent::ComputeHash(Frustum.LeftPlane, Frustum.RightPlane, Frustum.BottomPlane, Frustum.TopPlane, Frustum.NearPlane, Frustum.FarPlane);
    }
};

template <>
struct hash<Diligent::ViewFrustumExt>
{
    size_t operator()(const Diligent::ViewFrustumExt& Frustum) const
    {
        auto Seed = Diligent::ComputeHash(static_cast<const Diligent::ViewFrustum&>(Frustum));
        for (int Corner = 0; Corner < _countof(Frustum.FrustumCorners); ++Corner)
            Diligent::HashCombine(Seed, Frustum.FrustumCorners[Corner]);
        return Seed;
    }
};

} // namespace std
