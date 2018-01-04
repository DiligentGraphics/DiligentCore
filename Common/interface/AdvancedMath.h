/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "BasicMath.h"

// Structure describing a plane
struct Plane3D
{
    float3 Normal;
    float Distance; //Distance from the coordinate system origin to the plane along the normal direction
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
inline void ExtractViewFrustumPlanesFromMatrix(const float4x4 &Matrix, ViewFrustum &Frustum, bool bIsDirectX)
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
    if( bIsDirectX )
    {
        // 0 <= z <= w
        Frustum.NearPlane.Normal.x = Matrix._13;
        Frustum.NearPlane.Normal.y = Matrix._23;
        Frustum.NearPlane.Normal.z = Matrix._33;
        Frustum.NearPlane.Distance = Matrix._43;
    }
    else
    {
        // -w <= z <= w
	    Frustum.NearPlane.Normal.x = Matrix._14 + Matrix._13;
	    Frustum.NearPlane.Normal.y = Matrix._24 + Matrix._23;
	    Frustum.NearPlane.Normal.z = Matrix._34 + Matrix._33;
	    Frustum.NearPlane.Distance = Matrix._44 + Matrix._43;
    }

	// Far clipping plane 
	Frustum.FarPlane.Normal.x = Matrix._14 - Matrix._13;
	Frustum.FarPlane.Normal.y = Matrix._24 - Matrix._23;
	Frustum.FarPlane.Normal.z = Matrix._34 - Matrix._33;
	Frustum.FarPlane.Distance = Matrix._44 - Matrix._43;
}

inline void ExtractViewFrustumPlanesFromMatrix(const float4x4 &Matrix, ViewFrustumExt &FrustumExt, bool bIsDirectX)
{
    ExtractViewFrustumPlanesFromMatrix(Matrix, static_cast<ViewFrustum&>(FrustumExt), bIsDirectX);

    // Compute frustum corners
    float4x4 InvMatrix = inverseMatrix(Matrix);
    
    float nearClipZ = bIsDirectX ? 0.f : -1.f;
    static const float3 ProjSpaceCorners[] = 
    {
        float3(-1,-1, nearClipZ),
        float3( 1,-1, nearClipZ),
        float3(-1, 1, nearClipZ),
        float3( 1, 1, nearClipZ),

        float3(-1,-1, 1),
        float3( 1,-1, 1),
        float3(-1, 1, 1),
        float3( 1, 1, 1),
    };

	for(int i = 0; i < 8; ++i)
		FrustumExt.FrustumCorners[i] = ProjSpaceCorners[i] * InvMatrix;
}

struct BoundBox
{
    // Order must not be changed!
    float fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ;
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

template<bool TestFullVisibility>
inline BoxVisibility GetBoxVisibilityAgainstPlane(const Plane3D& Plane, const BoundBox &Box)
{
    const float3& Normal = Plane.Normal;
        
    float3 MaxPoint(
        (Normal.x > 0) ? Box.fMaxX : Box.fMinX,
        (Normal.y > 0) ? Box.fMaxY : Box.fMinY,
        (Normal.z > 0) ? Box.fMaxZ : Box.fMinZ
    );
        
    float DMax = dot( MaxPoint, Normal ) + Plane.Distance;

    if( DMax < 0 )
        return BoxVisibility::Invisible;

    if (TestFullVisibility)
    {
        float3 MinPoint(
            (Normal.x > 0) ? Box.fMinX : Box.fMaxX,
            (Normal.y > 0) ? Box.fMinY : Box.fMaxY,
            (Normal.z > 0) ? Box.fMinZ : Box.fMaxZ
        );

        float DMin = dot(MinPoint, Normal) + Plane.Distance;
        
        if (DMin > 0)
            return BoxVisibility::FullyVisible;
    }

    return BoxVisibility::Intersecting;
}

// Tests if bounding box is visible by the camera
template<bool TestFullVisibility>
inline BoxVisibility GetBoxVisibility(const ViewFrustum &ViewFrustum, const BoundBox &Box)
{
    const Plane3D *pPlanes = reinterpret_cast<const Plane3D*>(&ViewFrustum);
    
    int NumPlanesInside = 0;
    for(int iViewFrustumPlane = 0; iViewFrustumPlane < 6; iViewFrustumPlane++)
    {
        const Plane3D &CurrPlane = pPlanes[iViewFrustumPlane];
        auto VisibilityAgainstPlane = GetBoxVisibilityAgainstPlane<TestFullVisibility>(CurrPlane, Box);

        // If bounding box is "behind" one of the planes, it is definitely invisible
        if (VisibilityAgainstPlane == BoxVisibility::Invisible)
            return BoxVisibility::Invisible;

        // Count total number of planes the bound box is inside
        if (VisibilityAgainstPlane == BoxVisibility::FullyVisible)
            ++NumPlanesInside;
    }

    return (TestFullVisibility && NumPlanesInside == 6) ? BoxVisibility::FullyVisible : BoxVisibility::Intersecting;
}

template<bool TestFullVisibility>
inline BoxVisibility GetBoxVisibility(const ViewFrustumExt &ViewFrustumExt, const BoundBox &Box)
{
    auto Visibility = GetBoxVisibility<TestFullVisibility>(static_cast<const ViewFrustum&>(ViewFrustumExt), Box);
    if (Visibility == BoxVisibility::FullyVisible || Visibility == BoxVisibility::Invisible)
        return Visibility;

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

    for(int iBoundBoxPlane = 0; iBoundBoxPlane < 6; ++iBoundBoxPlane)
    {
        // struct BoundBox
        // {
        //     float fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ;
        // };
        float CurrPlaneCoord = reinterpret_cast<const float*>(&Box)[iBoundBoxPlane];
        int iCoordOrder = iBoundBoxPlane / 2; // 0, 0, 1, 1, 2, 2
        float fSign = (iBoundBoxPlane & 0x01) ? +1.f : -1.f;
        bool bAllCornersOutside = true;
        for(int iCorner=0; iCorner < 8; iCorner++)
        {
            float CurrCornerCoord = ViewFrustumExt.FrustumCorners[iCorner][iCoordOrder];
            if( fSign * (CurrPlaneCoord - CurrCornerCoord) > 0)
            {                    
                bAllCornersOutside = false;
                break;
            }
        }
        if( bAllCornersOutside )
            return BoxVisibility::Invisible;
    }

    return BoxVisibility::Intersecting;
}

inline float GetPointToBoxDistance(const BoundBox &BndBox, const float3 &Pos)
{
    VERIFY_EXPR(BndBox.fMaxX >= BndBox.fMinX && 
                BndBox.fMaxY >= BndBox.fMinY && 
                BndBox.fMaxZ >= BndBox.fMinZ);
    float fdX = (Pos.x > BndBox.fMaxX) ? (Pos.x - BndBox.fMaxX) : ( (Pos.x < BndBox.fMinX) ? (BndBox.fMinX - Pos.x) : 0.f );
    float fdY = (Pos.y > BndBox.fMaxY) ? (Pos.y - BndBox.fMaxY) : ( (Pos.y < BndBox.fMinY) ? (BndBox.fMinY - Pos.y) : 0.f );
    float fdZ = (Pos.z > BndBox.fMaxZ) ? (Pos.z - BndBox.fMaxZ) : ( (Pos.z < BndBox.fMinZ) ? (BndBox.fMinZ - Pos.z) : 0.f );
    VERIFY_EXPR(fdX >= 0 && fdY >= 0 && fdZ >= 0);

    float3 RangeVec(fdX, fdY, fdZ);
    return length( RangeVec );
}

inline bool operator == (const Plane3D &p1, const Plane3D &p2)
{
    return p1.Normal   == p2.Normal && 
           p1.Distance == p2.Distance;
}

inline bool operator == (const ViewFrustum &f1, const ViewFrustum &f2)
{
    return f1.LeftPlane   == f2.LeftPlane   && 
           f1.RightPlane  == f2.RightPlane  && 
           f1.BottomPlane == f2.BottomPlane &&
           f1.TopPlane    == f2.TopPlane    &&
           f1.NearPlane   == f2.NearPlane   &&
           f1.FarPlane    == f2.FarPlane;
}

inline bool operator == (const ViewFrustumExt &f1, const ViewFrustumExt &f2)
{
    if (! (static_cast<const ViewFrustum &>(f1) == static_cast<const ViewFrustum &>(f2)) )
        return false;

    for (int c = 0; c < _countof(f1.FrustumCorners); ++c)
        if (f1.FrustumCorners[c] != f2.FrustumCorners[c])
            return false;

    return true;
}

namespace std
{
    template<>
    struct hash<Plane3D>
    {
        size_t operator()( const Plane3D &Plane ) const
        {
            return Diligent::ComputeHash(Plane.Normal, Plane.Distance);
        }
    };

    template<>
    struct hash<ViewFrustum>
    {
        size_t operator()( const ViewFrustum &Frustum ) const
        {
            return Diligent::ComputeHash(Frustum.LeftPlane, Frustum.RightPlane, Frustum.BottomPlane, Frustum.TopPlane, Frustum.NearPlane, Frustum.FarPlane);
        }
    };

    template<>
    struct hash<ViewFrustumExt>
    {
        size_t operator()( const ViewFrustumExt &Frustum ) const
        {
            auto Seed = Diligent::ComputeHash(static_cast<const ViewFrustum&>(Frustum));
            for (int Corner = 0; Corner < _countof(Frustum.FrustumCorners); ++Corner)
                Diligent::HashCombine(Seed, Frustum.FrustumCorners[Corner]);
            return Seed;
        }
    };
}
