/*     Copyright 2015-2016 Egor Yusov
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

// For OpenGL, matrix is still considered row-major. The only difference is that
// near clip plane is at -1, not 0.
inline void ExtractViewFrustumPlanesFromMatrix(const float4x4 &Matrix, ViewFrustum &ViewFrustum, bool bIsDirectX)
{
    // For more details, see Gribb G., Hartmann K., "Fast Extraction of Viewing Frustum Planes from the 
    // World-View-Projection Matrix" (the paper is available at 
    // http://gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf)

	// Left clipping plane 
    ViewFrustum.LeftPlane.Normal.x = Matrix._14 + Matrix._11; 
	ViewFrustum.LeftPlane.Normal.y = Matrix._24 + Matrix._21; 
	ViewFrustum.LeftPlane.Normal.z = Matrix._34 + Matrix._31; 
	ViewFrustum.LeftPlane.Distance = Matrix._44 + Matrix._41;

	// Right clipping plane 
	ViewFrustum.RightPlane.Normal.x = Matrix._14 - Matrix._11; 
	ViewFrustum.RightPlane.Normal.y = Matrix._24 - Matrix._21; 
	ViewFrustum.RightPlane.Normal.z = Matrix._34 - Matrix._31; 
	ViewFrustum.RightPlane.Distance = Matrix._44 - Matrix._41;

	// Top clipping plane 
	ViewFrustum.TopPlane.Normal.x = Matrix._14 - Matrix._12; 
	ViewFrustum.TopPlane.Normal.y = Matrix._24 - Matrix._22; 
	ViewFrustum.TopPlane.Normal.z = Matrix._34 - Matrix._32; 
	ViewFrustum.TopPlane.Distance = Matrix._44 - Matrix._42;

	// Bottom clipping plane 
	ViewFrustum.BottomPlane.Normal.x = Matrix._14 + Matrix._12; 
	ViewFrustum.BottomPlane.Normal.y = Matrix._24 + Matrix._22; 
	ViewFrustum.BottomPlane.Normal.z = Matrix._34 + Matrix._32; 
	ViewFrustum.BottomPlane.Distance = Matrix._44 + Matrix._42;

    // Near clipping plane 
    if( bIsDirectX )
    {
        // 0 <= z <= w
        ViewFrustum.NearPlane.Normal.x = Matrix._13;
        ViewFrustum.NearPlane.Normal.y = Matrix._23;
        ViewFrustum.NearPlane.Normal.z = Matrix._33;
        ViewFrustum.NearPlane.Distance = Matrix._43;
    }
    else
    {
        // -w <= z <= w
	    ViewFrustum.NearPlane.Normal.x = Matrix._14 + Matrix._13; 
	    ViewFrustum.NearPlane.Normal.y = Matrix._24 + Matrix._23; 
	    ViewFrustum.NearPlane.Normal.z = Matrix._34 + Matrix._33; 
	    ViewFrustum.NearPlane.Distance = Matrix._44 + Matrix._43; 
    }

	// Far clipping plane 
	ViewFrustum.FarPlane.Normal.x = Matrix._14 - Matrix._13; 
	ViewFrustum.FarPlane.Normal.y = Matrix._24 - Matrix._23; 
	ViewFrustum.FarPlane.Normal.z = Matrix._34 - Matrix._33; 
	ViewFrustum.FarPlane.Distance = Matrix._44 - Matrix._43; 
}



struct BoundBox
{
    float fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ;
};

// Tests if bounding box is visible by the camera
inline bool IBoxVisible(const ViewFrustum &ViewFrustum, const BoundBox &Box)
{
    Plane3D *pPlanes = (Plane3D *)&ViewFrustum;
    // If bounding box is "behind" some plane, then it is invisible
    // Otherwise it is treated as visible
    for(int iViewFrustumPlane = 0; iViewFrustumPlane < 6; iViewFrustumPlane++)
    {
        Plane3D *pCurrPlane = pPlanes + iViewFrustumPlane;
        float3 *pCurrNormal = &pCurrPlane->Normal;
        float3 MaxPoint;
        
        MaxPoint.x = (pCurrNormal->x > 0) ? Box.fMaxX : Box.fMinX;
        MaxPoint.y = (pCurrNormal->y > 0) ? Box.fMaxY : Box.fMinY;
        MaxPoint.z = (pCurrNormal->z > 0) ? Box.fMaxZ : Box.fMinZ;
        
        float DMax = dot( MaxPoint, *pCurrNormal ) + pCurrPlane->Distance;

        if( DMax < 0 )
            return false;
    }

    return true;
}
