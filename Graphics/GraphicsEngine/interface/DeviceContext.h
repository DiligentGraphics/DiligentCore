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

/// \file
/// Definition of the Diligent::IDeviceContext interface and related data structures

#include "../../../Primitives/interface/Object.h"
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
#include "Fence.h"
#include "CommandList.h"
#include "SwapChain.h"

namespace Diligent
{

// {DC92711B-A1BE-4319-B2BD-C662D1CC19E4}
static constexpr INTERFACE_ID IID_DeviceContext =
{ 0xdc92711b, 0xa1be, 0x4319, { 0xb2, 0xbd, 0xc6, 0x62, 0xd1, 0xcc, 0x19, 0xe4 } };

/// Draw command flags
enum DRAW_FLAGS : Uint8
{
    /// Perform no state transitions
    DRAW_FLAG_NONE                            = 0x00,

    /// Transition vertex buffers to RESOURCE_STATE_VERTEX_BUFFER state (see Diligent::RESOURCE_STATE).
    /// Vertex buffers in unknown state will not be transitioned.
    DRAW_FLAG_TRANSITION_VERTEX_BUFFERS       = 0x01,

    /// Transition index buffer to RESOURCE_STATE_INDEX_BUFFER state (see Diligent::RESOURCE_STATE).
    /// If the index buffer is in unknown state, this flag has no effect.
    DRAW_FLAG_TRANSITION_INDEX_BUFFER         = 0x02,

    /// Transition indirect draw arguments buffer to RESOURCE_STATE_INDIRECT_ARGUMENT state (see Diligent::RESOURCE_STATE).
    /// If the buffer is in unknown state, this flag has no effect.
    DRAW_FLAG_TRANSITION_INDIRECT_ARGS_BUFFER = 0x04
};

/// Defines the draw command attributes

/// This structure is used by IRenderDevice::Draw()
struct DrawAttribs
{
    union
    {
        /// For a non-indexed draw call, number of vertices to draw
        Uint32 NumVertices = 0;
        
        /// For an indexed draw call, number of indices to draw
        Uint32 NumIndices;
    };

    /// Indicates if index buffer will be used to index input vertices
    Bool IsIndexed = False;

    /// For an indexed draw call, type of elements in the index buffer.
    /// Allowed values: VT_UINT16 and VT_UINT32. Ignored if DrawAttribs::IsIndexed is False.
    VALUE_TYPE IndexType = VT_UNDEFINED;

    /// Additional flags controlling the draw command behavior, see Diligent::DRAW_FLAGS.
    Uint8 Flags = DRAW_FLAG_NONE;

    /// Number of instances to draw. If more than one instance is specified,
    /// instanced draw call will be performed.
    Uint32 NumInstances = 1;

    /// For indexed rendering, a constant which is added to each index before 
    /// accessing the vertex buffer.
    Uint32 BaseVertex = 0; 

    /// For indirect rendering, offset from the beginning of the buffer to the location
    /// of draw command attributes. Ignored if DrawAttribs::pIndirectDrawAttribs is null.
    Uint32 IndirectDrawArgsOffset = 0;

    union
    {
        /// For non-indexed rendering, LOCATION (or INDEX, but NOT the byte offset) of the 
        /// first vertex in the vertex buffer to start reading vertices from
        Uint32 StartVertexLocation = 0;  

        /// For indexed rendering, LOCATION (NOT the byte offset) of the first index in 
        /// the index buffer to start reading indices from
        Uint32 FirstIndexLocation; 
    };
    /// For instanced rendering, LOCATION (or INDEX, but NOT the byte offset) in the vertex 
    /// buffer to start reading instance data from
    Uint32 FirstInstanceLocation = 0;

    /// For indirect rendering, pointer to the buffer, from which
    /// draw attributes will be read.
    IBuffer* pIndirectDrawAttribs = nullptr;


    /// Initializes the structure members with default values

    /// Default values:
    /// Member                  | Default value
    /// ------------------------|--------------
    /// NumVertices             | 0
    /// IsIndexed               | False
    /// IndexType               | VT_UNDEFINED
    /// Flags                   | DRAW_FLAG_NONE
    /// NumInstances            | 1
    /// BaseVertex              | 0
    /// IndirectDrawArgsOffset  | 0
    /// StartVertexLocation     | 0
    /// FirstInstanceLocation   | 0
    /// pIndirectDrawAttribs    | nullptr
    DrawAttribs()noexcept{}
};

/// Defines which parts of the depth-stencil buffer to clear.

/// These flags are used by IDeviceContext::ClearDepthStencil().
enum CLEAR_DEPTH_STENCIL_FLAGS : Int32
{
    CLEAR_DEPTH_FLAG   = 0x01,  ///< Clear depth part of the buffer
    CLEAR_STENCIL_FLAG = 0x02   ///< Clear stencil part of the buffer
};


/// Dispatch compute command flags
enum DISPATCH_FLAGS : Uint8
{
    /// Perform no state transitions
    DISPATCH_FLAG_FLAG_NONE                       = 0x00,
        
    /// Transition indirect dispatch arguments buffer to RESOURCE_STATE_INDIRECT_ARGUMENT state (see Diligent::RESOURCE_STATE).
    /// If the buffer is in unknown state, this flag has no effect.
    DISPATCH_FLAG_TRANSITION_INDIRECT_ARGS_BUFFER = 0x01
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
    IBuffer* pIndirectDispatchAttribs;
    
    /// If pIndirectDispatchAttribs is not nullptr, indicates offset from the beginning
    /// of the buffer to the dispatch command arguments. Ignored otherwise
    Uint32  DispatchArgsByteOffset;

    /// Flags controlling the dispatch command behavior, see Diligent::DISPATCH_FLAGS.
    Uint8 Flags = DISPATCH_FLAG_FLAG_NONE;

    /// Initializes the structure to perform non-indirect dispatch command
    
    /// \param [in] GroupsX - Number of groups dispatched in X direction. Default value is 1.
    /// \param [in] GroupsY - Number of groups dispatched in Y direction. Default value is 1.
    /// \param [in] GroupsZ - Number of groups dispatched in Z direction. Default value is 1.
    explicit
    DispatchComputeAttribs(Uint32 GroupsX = 1, Uint32 GroupsY = 1, Uint32 GroupsZ = 1) :
        ThreadGroupCountX       (GroupsX),
        ThreadGroupCountY       (GroupsY),
        ThreadGroupCountZ       (GroupsZ),
        pIndirectDispatchAttribs(nullptr),
        DispatchArgsByteOffset  (0)
    {}

    /// Initializes the structure to perform indirect dispatch command

    /// \param [in] pDispatchAttribs - Pointer to the buffer containing dispatch arguments.
    /// \param [in] Offset - Offset from the beginning of the buffer to the dispatch command 
    ///                 arguments. Default value is 0.
    explicit
    DispatchComputeAttribs(IBuffer* pDispatchAttribs, Uint32 Offset = 0) :
        ThreadGroupCountX        (0),
        ThreadGroupCountY        (0),
        ThreadGroupCountZ        (0),
        pIndirectDispatchAttribs (pDispatchAttribs),
        DispatchArgsByteOffset   (Offset)
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
    COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES = 0x01,


    /// Verify resource states

    /// The flag is used in debug and development builds to verify 
    /// that all resources are transitioned to correct states when
    /// COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag is not set.
    /// No resource state validation is performed in release build.
    COMMIT_SHADER_RESOURCES_FLAG_VERIFY_STATES = 0x02
};


/// Describes the viewport.

/// This structure is used by IDeviceContext::SetViewports().
struct Viewport
{
    /// X coordinate of the left boundary of the viewport.
    Float32 TopLeftX = 0.f;

    /// Y coordinate of the top boundary of the viewport.
    /// When defining a viewport, DirectX convention is used:
    /// window coordinate systems originates in the LEFT TOP corner
    /// of the screen with Y axis pointing down.
    Float32 TopLeftY = 0.f;

    /// Viewport width
    Float32 Width  = 0.f;

    /// Viewport Height
    Float32 Height = 0.f;

    /// Minimum depth of the viewport. Ranges between 0 and 1.
    Float32 MinDepth = 0.f;

    /// Maximum depth of the viewport. Ranges between 0 and 1.
    Float32 MaxDepth = 1.f;

    /// Initializes the structure
    Viewport(Float32 _TopLeftX,     Float32 _TopLeftY,
             Float32 _Width,        Float32 _Height,
             Float32 _MinDepth = 0, Float32 _MaxDepth = 1)noexcept :
        TopLeftX (_TopLeftX),
        TopLeftY (_TopLeftY),
        Width    (_Width   ),
        Height   (_Height  ),
        MinDepth (_MinDepth),
        MaxDepth (_MaxDepth)
    {}

    Viewport()noexcept{}
};

/// Describes the rectangle.

/// This structure is used by IDeviceContext::SetScissorRects().
///
/// \remarks When defining a viewport, Windows convention is used:
///         window coordinate systems originates in the LEFT TOP corner
///         of the screen with Y axis pointing down.
struct Rect
{
    Int32 left   = 0;  ///< X coordinate of the left boundary of the viewport.
    Int32 top    = 0;  ///< Y coordinate of the top boundary of the viewport.
    Int32 right  = 0;  ///< X coordinate of the right boundary of the viewport.
    Int32 bottom = 0;  ///< Y coordinate of the bottom boundary of the viewport.

    /// Initializes the structure
    Rect(Int32 _left, Int32 _top, Int32 _right, Int32 _bottom) : 
        left  ( _left   ),
        top   ( _top    ),
        right ( _right  ),
        bottom( _bottom )
    {}

    Rect(){}
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
    virtual void QueryInterface(const Diligent::INTERFACE_ID &IID, IObject** ppInterface) = 0;

    /// Sets the pipeline state

    /// \param [in] pPipelineState - Pointer to IPipelineState interface to bind to the context.
    virtual void SetPipelineState(IPipelineState* pPipelineState) = 0;


    /// Transitions shader resources to the require states.
    /// \param [in] pPipelineState - Pipeline state object that was used to create the shader resource binding.
    /// \param [in] pShaderResourceBinding - Shader resource binding whose resources will be transitioned.
    /// \remarks This method explicitly transitiones all resources to the correct states.
    ///          If this method was called, there is no need to specify Diligent::COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES
    ///          when calling IDeviceContext::CommitShaderResources()
    virtual void TransitionShaderResources(IPipelineState* pPipelineState, IShaderResourceBinding* pShaderResourceBinding) = 0;

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
    virtual void CommitShaderResources(IShaderResourceBinding* pShaderResourceBinding, Uint32 Flags) = 0;

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
    virtual void SetVertexBuffers(Uint32    StartSlot, 
                                  Uint32    NumBuffersSet, 
                                  IBuffer** ppBuffers, 
                                  Uint32*   pOffsets,
                                  Uint32    Flags) = 0;

    /// Invalidates the cached context state.

    /// This method should be called by say Unity plugin before (or after)
    /// issuing draw commands to invalidate cached states
    virtual void InvalidateState() = 0;


    /// Binds an index buffer to the pipeline
    
    /// \param [in] pIndexBuffer - Pointer to the index buffer. The buffer must have been created 
    ///                            with the Diligent::BIND_INDEX_BUFFER flag.
    /// \param [in] ByteOffset - Offset from the beginning of the buffer to 
    ///                          the start of index data.
    /// \remarks The device context keeps strong reference to the index buffer.
    ///          Thus an index buffer object cannot be released until it is unbound 
    ///          from the context.
    virtual void SetIndexBuffer(IBuffer* pIndexBuffer, Uint32 ByteOffset) = 0;


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
    virtual void SetViewports(Uint32 NumViewports, const Viewport* pViewports, Uint32 RTWidth, Uint32 RTHeight) = 0;

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
    virtual void SetScissorRects(Uint32 NumRects, const Rect* pRects, Uint32 RTWidth, Uint32 RTHeight) = 0;

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
    virtual void SetRenderTargets(Uint32 NumRenderTargets, ITextureView* ppRenderTargets[], ITextureView* pDepthStencil) = 0;

    /// Executes a draw command

    /// \param [in] DrawAttribs - Structure describing draw command attributes, see DrawAttribs for details.
    virtual void Draw(DrawAttribs &DrawAttribs) = 0;
    
    /// Executes a dispatch compute command
    
    /// \param [in] DispatchAttrs - Structure describing dispatch command attributes, 
    ///                             see DispatchComputeAttribs for details.
    virtual void DispatchCompute(const DispatchComputeAttribs &DispatchAttrs) = 0;

    /// Clears a depth-stencil view
    
    /// \param [in] pView - Pointer to ITextureView interface to clear. The view type must be 
    ///                     Diligent::TEXTURE_VIEW_DEPTH_STENCIL.
    /// \param [in] ClearFlags - Idicates which parts of the buffer to clear, see Diligent::CLEAR_DEPTH_STENCIL_FLAGS.
    /// \param [in] fDepth - Value to clear depth part of the view with.
    /// \param [in] Stencil - Value to clear stencil part of the view with.
    /// \remarks The full extent of the view is always cleared. Viewport and scissor settings are not applied.
    /// \note The depth-stencil view must be bound to the pipeline for clear operation to be performed.
    virtual void ClearDepthStencil(ITextureView* pView, Uint32 ClearFlags = CLEAR_DEPTH_FLAG, float fDepth = 1.f, Uint8 Stencil = 0) = 0;

    /// Clears a render target view

    /// \param [in] pView - Pointer to ITextureView interface to clear. The view type must be 
    ///                     Diligent::TEXTURE_VIEW_RENDER_TARGET.
    /// \param [in] RGBA - A 4-component array that represents the color to fill the render target with.
    ///                    If nullptr is provided, the default array {0,0,0,0} will be used.
    /// \remarks The full extent of the view is always cleared. Viewport and scissor settings are not applied.
    /// \note The render target view must be bound to the pipeline for clear operation to be performed.
    virtual void ClearRenderTarget(ITextureView* pView, const float* RGBA = nullptr) = 0;

    /// Finishes recording commands and generates a command list
    
    /// \param [out] ppCommandList - Memory location where pointer to the recorded command list will be written.
    virtual void FinishCommandList(ICommandList **ppCommandList) = 0;

    /// Executes recorded commands in a command list

    /// \param [in] pCommandList - Pointer to the command list to executre.
    /// \remarks After command list is executed, it is no longer valid and should be released.
    virtual void ExecuteCommandList(ICommandList* pCommandList) = 0;

    /// Tells the GPU to set a fence to a specified value after all previous work has completed

    /// \note The method does not flush the context (an application can do this explcitly if needed)
    ///       and the fence will be signalled only when the command context is flushed next time.
    ///       If an application needs to wait for the fence in a loop, it must flush the context
    ///       after signalling the fence.
    /// \param [in] pFence - The fence to signal
    /// \param [in] Value  - The value to set the fence to. This value must be greater than the
    ///                      previously signalled value on the same fence.
    virtual void SignalFence(IFence* pFence, Uint64 Value) = 0;

    /// Flushes the command buffer
    virtual void Flush() = 0;


    /// Updates the data in the buffer

    /// \param [in] pBuffer - Pointer to the buffer to updates.
    /// \param [in] Offset  - Offset in bytes from the beginning of the buffer to the update region.
    /// \param [in] Size    - Size in bytes of the data region to update.
    /// \param [in] pData   - Pointer to the data to write to the buffer.
    virtual void UpdateBuffer(IBuffer* pBuffer, Uint32 Offset, Uint32 Size, const PVoid pData) = 0;

    /// Copies the data from one buffer to another

    /// \param [in] pSrcBuffer - Source buffer to copy data from.
    /// \param [in] SrcOffset  - Offset in bytes from the beginning of the source buffer to the beginning of data to copy.
    /// \param [in] pDstBuffer - Destination buffer to copy data to.
    /// \param [in] DstOffset  - Offset in bytes from the beginning of the destination buffer to the beginning 
    ///                          of the destination region.
    /// \param [in] Size       - Size in bytes of data to copy.
    virtual void CopyBuffer(IBuffer* pSrcBuffer, Uint32 SrcOffset, IBuffer* pDstBuffer, Uint32 DstOffset, Uint32 Size) = 0;


    /// Updates the data in the texture

    /// \param [in] pTexture   - Pointer to the device context interface to be used to perform the operation.
    /// \param [in] MipLevel   - Mip level of the texture subresource to update.
    /// \param [in] Slice      - Array slice. Should be 0 for non-array textures.
    /// \param [in] DstBox     - Destination region on the texture to update.
    /// \param [in] SubresData - Source data to copy to the texture.
    virtual void UpdateTexture(ITexture* pTexture, Uint32 MipLevel, Uint32 Slice, const Box& DstBox, const TextureSubResData& SubresData) = 0;

    /// Copies data from one texture to another

    /// \param [in] pSrcTexture - Source texture to copy data from
    /// \param [in] SrcMipLevel - Mip level of the source texture to copy data from.
    /// \param [in] SrcSlice    - Array slice of the source texture to copy data from. 
    ///                           Should be 0 for non-array textures.
    /// \param [in] pSrcBox     - Source region to copy.
    ///                           Use nullptr to copy the entire subresource.
    /// \param [in] pDstTexture - Destination texture to copy data to
    /// \param [in] DstMipLevel - Mip level to copy data to.
    /// \param [in] DstSlice    - Array slice to copy data to. 
    ///                           Must be 0 for non-array textures.
    /// \param [in] DstX        - X offset on the destination subresource
    /// \param [in] DstY        - Y offset on the destination subresource
    /// \param [in] DstZ        - Z offset on the destination subresource
    virtual void CopyTexture(ITexture* pSrcTexture, 
                             Uint32 SrcMipLevel,
                             Uint32 SrcSlice,
                             const Box *pSrcBox,
                             ITexture* pDstTexture, 
                             Uint32 DstMipLevel,
                             Uint32 DstSlice,
                             Uint32 DstX,
                             Uint32 DstY,
                             Uint32 DstZ) = 0;


    /// Maps the texture subresource

    /// \param [in] pTexture    - Pointer to the texture to map.
    /// \param [in] MipLevel    - Mip level to map.
    /// \param [in] ArraySlice  - Array slice to map. This parameter must be 0 for non-array textures.
    /// \param [in] MapType     - Type of the map operation. See Diligent::MAP_TYPE.
    /// \param [in] MapFlags    - Special map flags. See Diligent::MAP_FLAGS.
    /// \param [in] pMapRegion  - Texture region to map. If this parameter is null, the entire subresource is mapped.
    /// \param [out] MappedData - Mapped texture region data
    ///
    /// \remarks This method is supported in D3D11, D3D12 and Vulkan backends. In D3D11 backend, only the entire 
    ///          subresource can be mapped, so pMapRegion must either be null, or cover the entire subresource.
    ///          In D3D11 and Vulkan backends, dynamic textures are no different from non-dynamic textures, and mapping 
    ///          with MAP_FLAG_DISCARD has exactly the same behavior
    virtual void MapTextureSubresource( ITexture*                 pTexture,
                                        Uint32                    MipLevel,
                                        Uint32                    ArraySlice,
                                        MAP_TYPE                  MapType,
                                        Uint32                    MapFlags,
                                        const Box*                pMapRegion,
                                        MappedTextureSubresource& MappedData ) = 0;

    /// Unmaps the texture subresource
    virtual void UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice) = 0;

    
    /// Generates a mipmap chain.

    /// \param [in] pTextureView - Texture view to generate mip maps for.
    /// \remarks This function can only be called for a shader resource view.
    ///          The texture must be created with MISC_TEXTURE_FLAG_GENERATE_MIPS flag.
    virtual void GenerateMips(ITextureView* pTextureView) = 0;


    /// Sets the swap chain in the device context

    /// The swap chain is used by the device context to work with the
    /// default framebuffer. Specifically, if the swap chain is set in the context,
    /// the following commands can be used:
    /// * SetRenderTargets(0, nullptr, nullptr) - to bind the default back buffer & depth buffer
    /// * SetViewports(1, nullptr, 0, 0) - to set the viewport to match the size of the back buffer
    /// * ClearRenderTarget(nullptr, color) - to clear the default back buffer
    /// * ClearDepthStencil(nullptr, ...) - to clear the default depth buffer
    /// The swap chain is automatically initialized for immediate and all deferred contexts
    /// by factory functions EngineFactoryD3D11Impl::CreateSwapChainD3D11(),
    /// EngineFactoryD3D12Impl::CreateSwapChainD3D12(), and EngineFactoryOpenGLImpl::CreateDeviceAndSwapChainGL().
    /// However, when the engine is initialized by attaching to existing d3d11/d3d12 device or OpenGL/GLES context, the
    /// swap chain needs to be set manually if the device context will be using any of the commands above.\n
    /// Device context keeps strong reference to the swap chain.
    virtual void SetSwapChain(ISwapChain* pSwapChain) = 0;


    /// Finishes the current frame and releases dynamic resources allocated by the context

    /// For immediate context, this method is called automatically by Present(), but can
    /// also be called explicitly. For deferred context, the method must be called by the application to
    /// release dynamic resources. The method has some overhead, so it is better to call it once
    /// per frame, though it can be called with different frequency. Note that unless the GPU is idled,
    /// the resources may actually be released several frames after the one they were used in last time.
    /// \note After the call all dynamic resources become invalid and must be written again before the next use. 
    ///       Also, all committed resources become invalid.\n
    ///       For deferred contexts, this method must be called after all command lists referencing dynamic resources
    ///       have been executed through immediate context.\n
    ///       The method does not Flush() the context.
    virtual void FinishFrame() = 0;


    /// Transitions resource states

    /// \param [in] BarrierCount      - Number of barriers in pResourceBarriers array
    /// \param [in] pResourceBarriers - Pointer to the array of resource barriers
    /// \remarks When both old and new states are RESOURCE_STATE_UNORDERED_ACCESS, the engine
    ///          executes UAV barrier on the resource. The barrier makes sure that all UAV accesses 
    ///          (reads or writes) are complete before any future UAV accesses (read or write) can begin.
    virtual void TransitionResourceStates(Uint32 BarrierCount, StateTransitionDesc* pResourceBarriers) = 0;
};

}
