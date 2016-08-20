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
/// Definition of the Diligent::IDeviceContext interface and related data structures

#include "Object.h"
#include "DeviceCaps.h"
#include "Constants.h"
#include "Buffer.h"
#include "InputLayout.h"
#include "Shader.h"
#include "Texture.h"
#include "Sampler.h"
#include "ResourceMapping.h"
#include "TextureView.h"
#include "BufferView.h"
#include "DepthStencilState.h"
#include "BlendState.h"
#include "PipelineState.h"
#include "CommandList.h"

namespace Diligent
{

// {DC92711B-A1BE-4319-B2BD-C662D1CC19E4}
static const Diligent::INTERFACE_ID IID_DeviceContext =
{ 0xdc92711b, 0xa1be, 0x4319, { 0xb2, 0xbd, 0xc6, 0x62, 0xd1, 0xcc, 0x19, 0xe4 } };

/// Input primitive topology.

/// This enumeration is used by DrawAttribs structure to define input primitive topology.
enum PRIMITIVE_TOPOLOGY : Int32
{
    /// Undefined topology
    PRIMITIVE_TOPOLOGY_UNDEFINED = 0,
    
    /// Interpret the vertex data as a list of triangles.\n
    /// D3D counterpart: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST. OpenGL counterpart: GL_TRIANGLES.
    PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,

    /// Interpret the vertex data as a triangle strip.\n
    /// D3D counterpart: D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP. OpenGL counterpart: GL_TRIANGLE_STRIP.
    PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,

    /// Interpret the vertex data as a list of points.\n
    /// D3D counterpart: D3D_PRIMITIVE_TOPOLOGY_POINTLIST. OpenGL counterpart: GL_POINTS.
    PRIMITIVE_TOPOLOGY_POINT_LIST,

    /// Interpret the vertex data as a list of lines.\n
    /// D3D counterpart: D3D_PRIMITIVE_TOPOLOGY_LINELIST. OpenGL counterpart: GL_LINES.
    PRIMITIVE_TOPOLOGY_LINE_LIST,

    /// Helper value that stores the total number of topologies in the enumeration
    PRIMITIVE_TOPOLOGY_NUM_TOPOLOGIES
};

/// Defines the draw command attributes

/// This structure is used by IRenderDevice::Draw()
struct DrawAttribs
{
    /// Input primitive topology, see Diligent::PRIMITIVE_TOPOLOGY.
    PRIMITIVE_TOPOLOGY Topology;
    union
    {
        /// For a non-indexed draw call, number of vertices to draw
        Uint32 NumVertices;
        
        /// For an indexed draw call, number of indices to draw
        Uint32 NumIndices;
    };
    /// For an indexed draw call, type of elements in the index buffer.
    /// Allowed values: VT_UINT16 and VT_UINT32. Ignored if DrawAttribs::IsIndexed is False.
    VALUE_TYPE IndexType;

    /// Indicates if index buffer will be used to index input vertices
    Bool IsIndexed;

    /// Number of instances to draw. If more than one instance is specified,
    /// instanced draw call will be performed.
    Uint32 NumInstances;

    /// Indicates if indirect draw call will be performed. If set to True, 
    /// pIndirectDrawAttribs must contain valid pointer to the buffer, from which
    /// draw attributes will be read.
    Bool IsIndirect;

    /// For indexed rendering, a constant which is added to each index before 
    /// accessing the vertex buffer.
    Uint32 BaseVertex; 

    /// For indirect rendering, offset from the beginning of the buffer to the
    /// location of draw command attributes. Ignored if DrawAttribs::IsIndirect is False.
    Uint32 IndirectDrawArgsOffset;

    union
    {
        /// For non-indexed rendering, LOCATION (or INDEX, but NOT the byte offset) of the 
        /// first vertex in the vertex buffer to start reading vertices from
        Uint32 StartVertexLocation;  

        /// For indexed rendering, LOCATION (NOT the byte offset) of the first index in 
        /// the index buffer to start reading indices from
        Uint32 FirstIndexLocation; 
    };
    /// For instanced rendering, LOCATION (or INDEX, but NOT the byte offset) in the vertex 
    /// buffer to start reading instance data from
    Uint32 FirstInstanceLocation;  

    /// For indirect rendering, pointer to the buffer, from which
    /// draw attributes will be read. Ignored if DrawAttribs::IsIndirect is False.
    IBuffer *pIndirectDrawAttribs;


    /// Initializes the structure members with default values

    /// Default values:
    /// Member                  | Default value
    /// ------------------------|--------------
    /// Topology                | PRIMITIVE_TOPOLOGY_UNDEFINED
    /// NumVertices             | 0
    /// IndexType               | VT_UNDEFINED
    /// IsIndexed               | False
    /// NumInstances            | 1
    /// IsIndirect              | False
    /// BaseVertex              | 0
    /// IndirectDrawArgsOffset  | 0
    /// StartVertexLocation     | 0
    /// FirstInstanceLocation   | 0
    /// pIndirectDrawAttribs    | nullptr
    DrawAttribs() : 
        Topology(PRIMITIVE_TOPOLOGY_UNDEFINED),
        NumVertices(0),
        IndexType(VT_UNDEFINED),
        IsIndexed(False),
        NumInstances(1),
        IsIndirect(False),
        BaseVertex(0),
        IndirectDrawArgsOffset(0),
        StartVertexLocation(0),
        FirstInstanceLocation(0),
        pIndirectDrawAttribs(nullptr)
    {}
};

/// Defines which parts of the depth-stencil buffer to clear.

/// These flags are used by IDeviceContext::ClearDepthStencil().
enum CLEAR_DEPTH_STENCIL_FLAGS : Int32
{
    CLEAR_DEPTH_FLAG = 0x01,    ///< Clear depth part of the buffer
    CLEAR_STENCIL_FLAG = 0x02   ///< Clear stencil part of the buffer
};

/// Describes dispatch command arguments.

/// [Dispatch]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476405(v=vs.85).aspx
/// This structure is used by IDeviceContext::DispatchCompute().
/// See [ID3D11DeviceContext::Dispatch on MSDN][Dispatch] for details.
struct DispatchComputeAttribs
{
    Uint32 ThreadGroupCountX; ///< Number of groups dispatched in X direction.
    Uint32 ThreadGroupCountY; ///< Number of groups dispatched in Y direction.
    Uint32 ThreadGroupCountZ; ///< Number of groups dispatched in Z direction.

    /// Pointer to the buffer containing dispatch arguments.
    /// If not nullptr, then indirect dispatch command is executed, and
    /// ThreadGroupCountX, ThreadGroupCountY, and ThreadGroupCountZ are ignored
    IBuffer *pIndirectDispatchAttribs;
    
    /// If pIndirectDispatchAttribs is not nullptr, indicates offset from the beginning
    /// of the buffer to the dispatch command arguments. Ignored otherwise
    Uint32  DispatchArgsByteOffset;

    /// Initializes the structure to perform non-indirect dispatch command
    
    /// \param [in] GroupsX - Number of groups dispatched in X direction. Default value is 1.
    /// \param [in] GroupsY - Number of groups dispatched in Y direction. Default value is 1.
    /// \param [in] GroupsZ - Number of groups dispatched in Z direction. Default value is 1.
    DispatchComputeAttribs( Uint32 GroupsX = 1, Uint32 GroupsY = 1, Uint32 GroupsZ = 1 ) : 
        ThreadGroupCountX( GroupsX ),
        ThreadGroupCountY( GroupsY ),
        ThreadGroupCountZ( GroupsZ ),
        pIndirectDispatchAttribs(nullptr),
        DispatchArgsByteOffset(0)
    {}

    /// Initializes the structure to perform indirect dispatch command

    /// \param [in] pDispatchAttribs - Pointer to the buffer containing dispatch arguments.
    /// \param [in] Offset - Offset from the beginning of the buffer to the dispatch command 
    ///                 arguments. Default value is 0.
    DispatchComputeAttribs( IBuffer *pDispatchAttribs, Uint32 Offset = 0 ) :
        ThreadGroupCountX( 0 ),
        ThreadGroupCountY( 0 ),
        ThreadGroupCountZ( 0 ),
        pIndirectDispatchAttribs( pDispatchAttribs ),
        DispatchArgsByteOffset( Offset )
    {}
};

/// Defines allowed flags for IDeviceContext::SetVertexBuffers() function.
enum SET_VERTEX_BUFFERS_FLAGS : Int32
{
    /// Reset the vertex buffers to only the buffers specified in this
    /// call. All buffers previously bound to the pipeline will be unbound.
    SET_VERTEX_BUFFERS_FLAG_RESET = 0x01
};

/// Defines allowed flags for IDeviceContext::CommitShaderResources() function.
enum COMMIT_SHADER_RESOURCES_FLAG
{
    /// Transition resources being committed

    /// If this flag is specified when IDeviceContext::CommitShaderResources() is called,
    /// the engine will transition all shader resources to the correct state.
    COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES = 0x01
};


/// Describes the viewport.

/// This structure is used by IDeviceContext::SetViewports().
struct Viewport
{
    /// X coordinate of the left boundary of the viewport.
    Float32 TopLeftX;

    /// Y coordinate of the top boundary of the viewport.
    /// When defining a viewport, DirectX convention is used:
    /// window coordinate systems originates in the LEFT TOP corner
    /// of the screen with Y axis pointing down.
    Float32 TopLeftY;

    /// Viewport width
    Float32 Width;

    /// Viewport Height
    Float32 Height;

    /// Minimum depth of the viewport. Ranges between 0 and 1.
    Float32 MinDepth;

    /// Maximum depth of the viewport. Ranges between 0 and 1.
    Float32 MaxDepth;

    /// Initializes the structure
    Viewport(Float32 _TopLeftX = 0, Float32 _TopLeftY = 0,
              Float32 _Width = 0, Float32 _Height = 0,
              Float32 _MinDepth = 0, Float32 _MaxDepth = 1 )
              :
        TopLeftX( _TopLeftX ),
        TopLeftY( _TopLeftY ),
        Width   ( _Width    ),
        Height  ( _Height   ),
        MinDepth( _MinDepth ),
        MaxDepth( _MaxDepth )
    {}
};

/// Describes the rectangle.

/// This structure is used by IDeviceContext::SetScissorRects().
///
/// \remarks When defining a viewport, Windows convention is used:
///         window coordinate systems originates in the LEFT TOP corner
///         of the screen with Y axis pointing down.
struct Rect
{
    Int32 left;  ///< X coordinate of the left boundary of the viewport.
    Int32 top;   ///< Y coordinate of the top boundary of the viewport.
    Int32 right; ///< X coordinate of the right boundary of the viewport.
    Int32 bottom;///< Y coordinate of the bottom boundary of the viewport.

    /// Initializes the structure
    Rect( Int32 _left = 0, Int32 _top = 0, Int32 _right = 0, Int32 _bottom = 0 ) : 
        left  ( _left   ),
        top   ( _top    ),
        right ( _right  ),
        bottom( _bottom )
    {}
};


/// Device context interface

/// \remarks Device context keeps strong references to all objects currently bound to 
///          the pipeline: buffers, states, samplers, shaders, etc.
///          The context also keeps strong reference to the device and
///          the swap chain.
class IDeviceContext : public IObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) = 0;

    /// Sets the pipeline state

    /// \param [in] pPipelineState - Pointer to IPipelineState interface to bind to the context.
    virtual void SetPipelineState(IPipelineState *pPipelineState) = 0;


    /// Transitions shader resources to the require states.
    /// \param [in] pPipelineState - Pipeline state object that was used to create the shader resource binding.
    /// \param [in] pShaderResourceBinding - Shader resource binding whose resources will be transitioned.
    /// \remarks This method explicitly transitiones all resources to the correct states.
    ///          If this method was called, there is no need to specify Diligent::COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES
    ///          when calling IDeviceContext::CommitShaderResources()
    virtual void TransitionShaderResources(IPipelineState *pPipelineState, IShaderResourceBinding *pShaderResourceBinding) = 0;

    /// Commits shader resources to the device context

    /// \param [in] pShaderResourceBinding - Shader resource binding whose resources will be committed.
    ///                                      If pipeline state contains no shader resources, this parameter
    ///                                      can be null.
    /// \param [in] Flags - Additional flags for the operation. See Diligent::COMMIT_SHADER_RESOURCES_FLAG
    ///                     for a list of allowed values.
    ///
    /// \remarks Pipeline state object that was used to create the shader resource binding must be bound 
    ///          to the pipeline when CommitShaderResources() is called. If no pipeline state object is bound
    ///          or the pipeline state object does not match shader resource binding, the method will fail.\n
    ///          If Diligent::COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag is specified,
    ///          the engine will also transition all shader resources to the correct state. If the flag
    ///          is not specified, it is assumed that all resources are already in correct states.\n
    ///          Resources can be explicitly transitioned to the required states by calling 
    ///          IDeviceContext::TransitionShaderResources()
    virtual void CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags) = 0;

    /// Sets the stencil reference value

    /// \param [in] StencilRef - Stencil reference value.
    virtual void SetStencilRef(Uint32 StencilRef) = 0;

    
    /// \param [in] pBlendFactors - Array of four blend factors, one for each RGBA component. 
    ///                             Theses factors are used if the blend state uses one of the 
    ///                             Diligent::BLEND_FACTOR_BLEND_FACTOR or 
    ///                             Diligent::BLEND_FACTOR_INV_BLEND_FACTOR 
    ///                             blend factors. If nullptr is provided,
    ///                             default blend factors array {1,1,1,1} will be used.
    virtual void SetBlendFactors(const float* pBlendFactors = nullptr) = 0;


    /// Binds vertex buffers to the pipeline.

    /// \param [in] StartSlot - The first input slot for binding. The first vertex buffer is 
    ///                         explicitly bound to the start slot; each additional vertex buffer 
    ///                         in the array is implicitly bound to each subsequent input slot. 
    /// \param [in] NumBuffersSet - The number of vertex buffers in the array.
    /// \param [in] ppBuffers - A pointer to an array of vertex buffers. 
    //                          The vertex buffers must have been created with the Diligent::BIND_VERTEX_BUFFER flag.
    /// \param [in] pStrides  - Pointer to an array of stride values; one stride value for each buffer 
    ///                         in the vertex-buffer array. Each stride is the size (in bytes) of the 
    ///                         elements that are to be used from that vertex buffer.
    ///                         If this parameter is nullptr, tight strides from the input layout
    ///                         will be used for each buffer. See IPipelineState::GetTightStrides().
    /// \param [in] pOffsets  - Pointer to an array of offset values; one offset value for each buffer 
    ///                         in the vertex-buffer array. Each offset is the number of bytes between 
    ///                         the first element of a vertex buffer and the first element that will be 
    ///                         used. If this parameter is nullptr, zero offsets for all buffers will be used.
    /// \param [in] Flags     - Additional flags for the operation. See Diligent::SET_VERTEX_BUFFERS_FLAGS
    ///                         for a list of allowed values.      
    /// \remarks The device context keeps strong references to all bound vertex buffers.
    ///          Thus a buffer cannot be released until it is unbound from the context.\n
    ///          It is suggested to specify Diligent::SET_VERTEX_BUFFERS_FLAG_RESET flag
    ///          whenever possible. This will assure that no buffers from previous draw calls are
    ///          are bound to the pipeline.
    virtual void SetVertexBuffers(Uint32 StartSlot, 
                                  Uint32 NumBuffersSet, 
                                  IBuffer **ppBuffers, 
                                  Uint32 *pStrides, 
                                  Uint32 *pOffsets,
                                  Uint32 Flags) = 0;

    /// Clears the context state.
    virtual void ClearState() = 0;


    /// Binds an index buffer to the pipeline
    
    /// \param [in] pIndexBuffer - Pointer to the index buffer. The buffer must have been created 
    ///                            with the Diligent::BIND_INDEX_BUFFER flag.
    /// \param [in] ByteOffset - Offset from the beginning of the buffer to 
    ///                          the start of index data.
    /// \remarks The device context keeps strong reference to the index buffer.
    ///          Thus an index buffer object cannot be released until it is unbound 
    ///          from the context.
    virtual void SetIndexBuffer(IBuffer *pIndexBuffer, Uint32 ByteOffset) = 0;


    /// Sets an array of viewports

    /// \param [in] NumViewports - Number of viewports to set.
    /// \param [in] pViewports - An array of Viewport structures describing the viewports to bind.
    /// \param [in] RTWidth - Render target width. If 0 is provided, width of the currently bound render target will be used.
    /// \param [in] RTHeight- Render target height. If 0 is provided, height of the currently bound render target will be used.
    /// \remarks
    /// DirectX and OpenGL use different window coordinate systems. In DirectX, the coordinate system origin
    /// is in the left top corner of the screen with Y axis pointing down. In OpenGL, the origin
    /// is in the left bottom corener of the screen with Y axis pointing up. Render target size is 
    /// required to convert viewport from DirectX to OpenGL coordinate system if OpenGL device is used.\n\n
    /// All viewports must be set atomically as one operation. Any viewports not 
    /// defined by the call are disabled.\n\n
    /// You can set the viewport size to match the currently bound render target using the
    /// following call:
    ///
    ///     pContext->SetViewports(1, nullptr, 0, 0);
    virtual void SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight ) = 0;

    /// Sets active scissor rects

    /// \param [in] NumRects - Number of scissor rectangles to set.
    /// \param [in] pRects - An array of Rect structures describing the scissor rectangles to bind.
    /// \param [in] RTWidth - Render target width. If 0 is provided, width of the currently bound render target will be used.
    /// \param [in] RTHeight - Render target height. If 0 is provided, height of the currently bound render target will be used.
    /// \remarks
    /// DirectX and OpenGL use different window coordinate systems. In DirectX, the coordinate system origin
    /// is in the left top corner of the screen with Y axis pointing down. In OpenGL, the origin
    /// is in the left bottom corener of the screen with Y axis pointing up. Render target size is 
    /// required to convert viewport from DirectX to OpenGL coordinate system if OpenGL device is used.\n\n
    /// All scissor rects must be set atomically as one operation. Any rects not 
    /// defined by the call are disabled.
    virtual void SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight ) = 0;

    /// Binds one or more render targets and the depth-stencil buffer to the pipeline. It also
    /// sets the viewport to match the first non-null render target or depth-stencil buffer.

    /// \param [in] NumRenderTargets - Number of render targets to bind.
    /// \param [in] ppRenderTargets - Array of pointers to ITextureView that represent the render 
    ///                               targets to bind to the device. The type of each view in the 
    ///                               array must be Diligent::TEXTURE_VIEW_RENDER_TARGET.
    /// \param [in] pDepthStencil - Pointer to the ITextureView that represents the depth stencil to 
    ///                             bind to the device. The view type must be
    ///                             Diligent::TEXTURE_VIEW_DEPTH_STENCIL.
    /// \remarks
    /// The device context will keep strong references to all bound render target 
    /// and depth-stencil views. Thus these views (and consequently referenced textures) 
    /// cannot be released until they are unbound from the context.\n
    /// Any render targets not defined by this call are set to nullptr.\n\n
    /// You can set the default render target and depth stencil using the
    /// following call:
    ///
    ///     pContext->SetRenderTargets(0, nullptr, nullptr);
    virtual void SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil ) = 0;

    /// Executes a draw command

    /// \param [in] DrawAttribs - Structure describing draw command attributes, see DrawAttribs for details.
    virtual void Draw(DrawAttribs &DrawAttribs) = 0;
    
    /// Executes a dispatch compute command
    
    /// \param [in] DispatchAttrs - Structure describing dispatch command attributes, 
    ///                             see DispatchComputeAttribs for details.
    virtual void DispatchCompute( const DispatchComputeAttribs &DispatchAttrs ) = 0;

    /// Clears a depth-stencil view
    
    /// \param [in] pView - Pointer to ITextureView interface to clear. The view type must be 
    ///                     Diligent::TEXTURE_VIEW_DEPTH_STENCIL.
    /// \param [in] ClearFlags - Idicates which parts of the buffer to clear, see Diligent::CLEAR_DEPTH_STENCIL_FLAGS.
    /// \param [in] fDepth - Value to clear depth part of the view with.
    /// \param [in] Stencil - Value to clear stencil part of the view with.
    /// \remarks The full extent of the view is always cleared. Viewport and scissor settings are not applied.
    /// \note The depth-stencil view must be bound to the pipeline for clear operation to be performed.
    virtual void ClearDepthStencil( ITextureView *pView, Uint32 ClearFlags = CLEAR_DEPTH_FLAG, float fDepth = 1.f, Uint8 Stencil = 0 ) = 0;

    /// Clears a render target view

    /// \param [in] pView - Pointer to ITextureView interface to clear. The view type must be 
    ///                     Diligent::TEXTURE_VIEW_RENDER_TARGET.
    /// \param [in] RGBA - A 4-component array that represents the color to fill the render target with.
    ///                    If nullptr is provided, the default array {0,0,0,0} will be used.
    /// \remarks The full extent of the view is always cleared. Viewport and scissor settings are not applied.
    /// \note The render target view must be bound to the pipeline for clear operation to be performed.
    virtual void ClearRenderTarget( ITextureView *pView, const float *RGBA = nullptr ) = 0;

    /// Finishes recording commands and generates a command list
    
    /// \param [out] ppCommandList - Memory location where pointer to the recorded command list will be written.
    virtual void FinishCommandList(class ICommandList **ppCommandList) = 0;

    /// Executes recorded commands in a command list

    /// \param [in] pCommandList - Pointer to the command list to executre.
    /// \remarks After command list is executed, it is no longer valid and should be released.
    virtual void ExecuteCommandList(class ICommandList *pCommandList) = 0;

    /// Flushes the command buffer
    virtual void Flush() = 0;
};

}
