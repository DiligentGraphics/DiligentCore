/*     Copyright 2015 Egor Yusov
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
/// Definition of the Diligent::IRasterizerState interface and related data structures

#include "DeviceObject.h"

namespace Diligent
{

/// Fill mode

/// [D3D11_FILL_MODE]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476131(v=vs.85).aspx
/// This enumeration determines the fill mode to use when rendering triangles and mirrows the 
/// [D3D11_FILL_MODE][] enum. It is used by RasterizerStateDesc structure to define the fill mode.
enum FILL_MODE : Int32
{ 
    /// Undefined fill mode.
    FILL_MODE_UNDEFINED = 0,

    /// Rasterize triangles using wireframe fill. \n
    /// D3D11 counterpart: D3D11_FILL_WIREFRAME. OpenGL counterpart: GL_LINE.
    FILL_MODE_WIREFRAME,    

    /// Rasterize triangles using solid fill. \n
    /// D3D11 counterpart: D3D11_FILL_SOLID. OpenGL counterpart: GL_FILL.
    FILL_MODE_SOLID,        

    /// Helper value that stores the total number of fill modes in the enumeration.
    FILL_MODE_NUM_MODES     
};

/// Cull mode

/// [D3D11_CULL_MODE]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476108(v=vs.85).aspx
/// This enumeration defines which triangles are not drawn during the rasterization and mirrows
/// [D3D11_CULL_MODE][] enum. It is used by RasterizerStateDesc structure to define the polygin cull mode.
enum CULL_MODE : Int32
{
    /// Undefined cull mode.
    CULL_MODE_UNDEFINED = 0,

    /// Draw all triangles. \n
    /// D3D11 counterpart: D3D11_CULL_NONE. OpenGL counterpart: glDisable( GL_CULL_FACE ).
    CULL_MODE_NONE,

    /// Do not draw triangles that are front-facing. Front- and back-facing triangles are determined
    /// by the RasterizerStateDesc::FrontCounterClockwise member. \n
    /// D3D11 counterpart: D3D11_CULL_FRONT. OpenGL counterpart: GL_FRONT.
    CULL_MODE_FRONT,

    /// Do not draw triangles that are back-facing. Front- and back-facing triangles are determined
    /// by the RasterizerStateDesc::FrontCounterClockwise member. \n
    /// D3D11 counterpart: D3D11_CULL_BACK. OpenGL counterpart: GL_BACK.
    CULL_MODE_BACK,

    /// Helper value that stores the total number of cull modes in the enumeration.
    CULL_MODE_NUM_MODES
};

// {530A181E-2777-4DAA-B837-ED7D3C28418E}
static const Diligent::INTERFACE_ID IID_RasterizerState =
{ 0x530a181e, 0x2777, 0x4daa, { 0xb8, 0x37, 0xed, 0x7d, 0x3c, 0x28, 0x41, 0x8e } };

/// Rasterizer state description

/// This structure describes the rasterizer state which is used in a call to 
/// IRenderDevice::CreateRasterizerState() to create a rasterizer state object
struct RasterizerStateDesc : DeviceObjectAttribs
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
    /// The operator ignores DeviceObjectAttribs::Name field as it does not affect 
    /// the rasterizer state.
    bool operator == (const RasterizerStateDesc& RHS)const
    {
                // Name is primarily used for debug purposes and does not affect the state.
                // It is ignored in comparison operation.
        return  // strcmp(Name, RHS.Name) == 0                       &&
                FillMode              == RHS.FillMode              &&
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

/// Rasterizer state interface

/// The interface holds the rasterizer state object that can be bound to the pipeline by a call
/// to IDeviceContext::SetRasterizerState(). To create a rasterizer state, call 
/// IRenderDevice::CreateRasterizerState().
class IRasterizerState : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) = 0;

    /// Returns the rasterizer state description used to create the object
    virtual const RasterizerStateDesc& GetDesc()const = 0;
};

}
