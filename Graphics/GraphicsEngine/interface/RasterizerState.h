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

/// \file
/// Rasterizer state description

#include "BasicTypes.h"

namespace Diligent
{

/// Fill mode

/// [D3D11_FILL_MODE]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476131(v=vs.85).aspx
/// [D3D12_FILL_MODE]: https://msdn.microsoft.com/en-us/library/windows/desktop/dn770366(v=vs.85).aspx
/// This enumeration determines the fill mode to use when rendering triangles and mirrors the 
/// [D3D11_FILL_MODE][]/[D3D12_FILL_MODE][] enum. It is used by RasterizerStateDesc structure to define the fill mode.
enum FILL_MODE : Int32
{ 
    /// Undefined fill mode.
    FILL_MODE_UNDEFINED = 0,

    /// Rasterize triangles using wireframe fill. \n
    /// Direct3D counterpart: D3D11_FILL_WIREFRAME/D3D12_FILL_MODE_WIREFRAME. OpenGL counterpart: GL_LINE.
    FILL_MODE_WIREFRAME,    

    /// Rasterize triangles using solid fill. \n
    /// Direct3D counterpart: D3D11_FILL_SOLID/D3D12_FILL_MODE_SOLID. OpenGL counterpart: GL_FILL.
    FILL_MODE_SOLID,        

    /// Helper value that stores the total number of fill modes in the enumeration.
    FILL_MODE_NUM_MODES     
};

/// Cull mode

/// [D3D11_CULL_MODE]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476108(v=vs.85).aspx
/// [D3D12_CULL_MODE]: https://msdn.microsoft.com/en-us/library/windows/desktop/dn770354(v=vs.85).aspx
/// This enumeration defines which triangles are not drawn during the rasterization and mirrors
/// [D3D11_CULL_MODE][]/[D3D12_CULL_MODE][] enum. It is used by RasterizerStateDesc structure to define the polygon cull mode.
enum CULL_MODE : Int32
{
    /// Undefined cull mode.
    CULL_MODE_UNDEFINED = 0,

    /// Draw all triangles. \n
    /// Direct3D counterpart: D3D11_CULL_NONE/D3D12_CULL_MODE_NONE. OpenGL counterpart: glDisable( GL_CULL_FACE ).
    CULL_MODE_NONE,

    /// Do not draw triangles that are front-facing. Front- and back-facing triangles are determined
    /// by the RasterizerStateDesc::FrontCounterClockwise member. \n
    /// Direct3D counterpart: D3D11_CULL_FRONT/D3D12_CULL_MODE_FRONT. OpenGL counterpart: GL_FRONT.
    CULL_MODE_FRONT,

    /// Do not draw triangles that are back-facing. Front- and back-facing triangles are determined
    /// by the RasterizerStateDesc::FrontCounterClockwise member. \n
    /// Direct3D counterpart: D3D11_CULL_BACK/D3D12_CULL_MODE_BACK. OpenGL counterpart: GL_BACK.
    CULL_MODE_BACK,

    /// Helper value that stores the total number of cull modes in the enumeration.
    CULL_MODE_NUM_MODES
};


/// Rasterizer state description

/// This structure describes the rasterizer state and is part of the GraphicsPipelineDesc.
struct RasterizerStateDesc
{
    /// Determines traingle fill mode, see Diligent::FILL_MODE for details.
    FILL_MODE FillMode;

    /// Determines traingle cull mode, see Diligent::CULL_MODE for details.
    CULL_MODE CullMode;

    /// Determines if a triangle is front- or back-facing. If this parameter is True, 
    /// a triangle will be considered front-facing if its vertices are counter-clockwise 
    /// on the render target and considered back-facing if they are clockwise. 
    /// If this parameter is False, the opposite is true.
    Bool      FrontCounterClockwise;

    /// Constant value added to the depth of a given pixel.
    Int32     DepthBias;

    /// Maximum depth bias of a pixel.
    /// \warning Depth bias clamp is not available in OpenGL
    Float32   DepthBiasClamp;

    /// Scalar that scales the given pixel's slope before adding to the pixel's depth.
    Float32   SlopeScaledDepthBias;

    /// Enable clipping based on distance.
    /// \warning On DirectX this only disables clipping against far clipping plane,
    ///          while on OpenGL this disables clipping against both far and near clip planes.
    Bool      DepthClipEnable;

    /// Enable scissor-rectangle culling. All pixels outside an active scissor rectangle are culled.
    Bool      ScissorEnable;

    /// Specifies whether to enable line antialiasing.
    Bool      AntialiasedLineEnable;

    /// Initializes the structure members with default values

    /// Member                | Default value
    /// ----------------------|--------------
    /// FillMode              | FILL_MODE_SOLID
    /// CullMode              | CULL_MODE_BACK
    /// FrontCounterClockwise | False
    /// DepthBias             | 0
    /// DepthBiasClamp        | 0.f
    /// SlopeScaledDepthBias  | 0.f
    /// DepthClipEnable       | True
    /// ScissorEnable         | False
    /// AntialiasedLineEnable | False
    RasterizerStateDesc() : 
        FillMode             ( FILL_MODE_SOLID ),
        CullMode             ( CULL_MODE_BACK ),
        FrontCounterClockwise( False ),
        DepthBias            ( 0 ),
        DepthBiasClamp       ( 0.f ),
        SlopeScaledDepthBias ( 0.f ),
        DepthClipEnable      ( True ),
        ScissorEnable        ( False ),
        AntialiasedLineEnable( False )
    {
    }
    

    /// Tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return 
    /// - True if all members of the two structures are equal.
    /// - False otherwise
    bool operator == (const RasterizerStateDesc& RHS)const
    {
        return  FillMode              == RHS.FillMode              &&
                CullMode              == RHS.CullMode              &&
                FrontCounterClockwise == RHS.FrontCounterClockwise &&
                DepthBias             == RHS.DepthBias             &&
                DepthBiasClamp        == RHS.DepthBiasClamp        &&
                SlopeScaledDepthBias  == RHS.SlopeScaledDepthBias  &&
                DepthClipEnable       == RHS.DepthClipEnable       &&
                ScissorEnable         == RHS.ScissorEnable         &&
                AntialiasedLineEnable == RHS.AntialiasedLineEnable;
    }
};

}