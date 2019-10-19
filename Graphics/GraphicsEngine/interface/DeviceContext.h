/*     Copyright 2019 Diligent Graphics LLC
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
#include "../../../Primitives/interface/FlagEnum.h"
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
    /// No flags.
    DRAW_FLAG_NONE                            = 0x00,

    /// Verify the sate of index and vertex buffers (if any) used by the draw 
    /// command. State validation is only performed in debug and development builds 
    /// and the flag has no effect in release build.
    DRAW_FLAG_VERIFY_STATES                   = 0x01,

    /// Verify correctness of parameters passed to the draw command.
    DRAW_FLAG_VERIFY_DRAW_ATTRIBS             = 0x02,

    /// Verify that render targets bound to the context are consistent with the pipeline state.
    DRAW_FLAG_VERIFY_RENDER_TARGETS           = 0x04,

    /// Perform all state validation checks
    DRAW_FLAG_VERIFY_ALL                      = DRAW_FLAG_VERIFY_STATES | DRAW_FLAG_VERIFY_DRAW_ATTRIBS | DRAW_FLAG_VERIFY_RENDER_TARGETS
};
DEFINE_FLAG_ENUM_OPERATORS(DRAW_FLAGS)


/// Defines resource state transition mode performed by various commands.

/// Refer to http://diligentgraphics.com/2018/12/09/resource-state-management/ for detailed explanation
/// of resource state management in Diligent Engine.
enum RESOURCE_STATE_TRANSITION_MODE : Uint8
{
    /// Perform no state transitions and no state validation. 
    /// Resource states are not accessed (either read or written) by the command.
    RESOURCE_STATE_TRANSITION_MODE_NONE = 0,
    
    /// Transition resources to the states required by the specific command.
    /// Resources in unknown state are ignored.
    ///
    /// \note    Any method that uses this mode may alter the state of the resources it works with.
    ///          As automatic state management is not thread-safe, no other thread is allowed to read
    ///          or write the state of the resources being transitioned. 
    ///          If the application intends to use the same resources in other threads simultaneously, it needs to 
    ///          explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    ///          Refer to http://diligentgraphics.com/2018/12/09/resource-state-management/ for detailed explanation
    ///          of resource state management in Diligent Engine.
    RESOURCE_STATE_TRANSITION_MODE_TRANSITION,

    /// Do not transition, but verify that states are correct.
    /// No validation is performed if the state is unknown to the engine.
    /// This mode only has effect in debug and development builds. No validation 
    /// is performed in release build.
    ///
    /// \note    Any method that uses this mode will read the state of resources it works with.
    ///          As automatic state management is not thread-safe, no other thread is allowed to alter
    ///          the state of resources being used by the command. It is safe to read these states.
    RESOURCE_STATE_TRANSITION_MODE_VERIFY
};


/// Defines the draw command attributes.

/// This structure is used by IDeviceContext::Draw().
struct DrawAttribs
{
    /// The number of vertices to draw.
    Uint32     NumVertices           = 0;

    /// Additional flags, see Diligent::DRAW_FLAGS.
    DRAW_FLAGS Flags                 = DRAW_FLAG_NONE;

    /// The number of instances to draw. If more than one instance is specified,
    /// instanced draw call will be performed.
    Uint32     NumInstances          = 1;

    /// LOCATION (or INDEX, but NOT the byte offset) of the first vertex in the
    /// vertex buffer to start reading vertices from.
    Uint32     StartVertexLocation   = 0;

    /// LOCATION (or INDEX, but NOT the byte offset) in the vertex buffer to start
    /// reading instance data from.
    Uint32     FirstInstanceLocation = 0;


    /// Initializes the structure members with default values.

    /// Default values:
    ///
    /// Member                                   | Default value
    /// -----------------------------------------|--------------------------------------
    /// NumVertices                              | 0
    /// Flags                                    | DRAW_FLAG_NONE
    /// NumInstances                             | 1
    /// StartVertexLocation                      | 0
    /// FirstInstanceLocation                    | 0
    DrawAttribs()noexcept{}

    /// Initializes the structure with user-specified values.
    DrawAttribs(Uint32     _NumVertices,
                DRAW_FLAGS _Flags,
                Uint32     _NumInstances          = 1,
                Uint32     _StartVertexLocation   = 0,
                Uint32     _FirstInstanceLocation = 0)noexcept : 
        NumVertices          {_NumVertices          },
        Flags                {_Flags                },
        NumInstances         {_NumInstances         },
        StartVertexLocation  {_StartVertexLocation  },
        FirstInstanceLocation{_FirstInstanceLocation}
    {}
};


/// Defines the indexed draw command attributes.

/// This structure is used by IDeviceContext::DrawIndexed().
struct DrawIndexedAttribs
{
    /// The number of indices to draw.
    Uint32     NumIndices            = 0;

    /// The type of elements in the index buffer.
    /// Allowed values: VT_UINT16 and VT_UINT32.
    VALUE_TYPE IndexType             = VT_UNDEFINED;

    /// Additional flags, see Diligent::DRAW_FLAGS.
    DRAW_FLAGS Flags                 = DRAW_FLAG_NONE;

    /// Number of instances to draw. If more than one instance is specified,
    /// instanced draw call will be performed.
    Uint32     NumInstances          = 1;

    /// A constant which is added to each index before accessing the vertex buffer.
    Uint32     BaseVertex            = 0; 

    /// LOCATION (NOT the byte offset) of the first index in
    /// the index buffer to start reading indices from.
    Uint32     FirstIndexLocation    = 0; 

    /// LOCATION (or INDEX, but NOT the byte offset) in the vertex
    /// buffer to start reading instance data from.
    Uint32     FirstInstanceLocation = 0;


    /// Initializes the structure members with default values.

    /// Default values:
    /// Member                                   | Default value
    /// -----------------------------------------|--------------------------------------
    /// NumIndices                               | 0
    /// IndexType                                | VT_UNDEFINED
    /// Flags                                    | DRAW_FLAG_NONE
    /// NumInstances                             | 1
    /// BaseVertex                               | 0
    /// FirstIndexLocation                       | 0
    /// FirstInstanceLocation                    | 0
    DrawIndexedAttribs()noexcept{}

    /// Initializes the structure members with user-specified values.
    DrawIndexedAttribs(Uint32      _NumIndices,
                       VALUE_TYPE  _IndexType,
                       DRAW_FLAGS  _Flags,
                       Uint32      _NumInstances          = 1,
                       Uint32      _BaseVertex            = 0,
                       Uint32      _FirstIndexLocation    = 0,
                       Uint32      _FirstInstanceLocation = 0)noexcept : 
        NumIndices           {_NumIndices           },
        IndexType            {_IndexType            },
        Flags                {_Flags                },
        NumInstances         {_NumInstances         },
        BaseVertex           {_BaseVertex           },
        FirstIndexLocation   {_FirstIndexLocation   },
        FirstInstanceLocation{_FirstInstanceLocation}
    {}
};


/// Defines the indirect draw command attributes.

/// This structure is used by IDeviceContext::DrawIndirect().
struct DrawIndirectAttribs
{
    /// Additional flags, see Diligent::DRAW_FLAGS.
    DRAW_FLAGS Flags                = DRAW_FLAG_NONE;

    /// State transition mode for indirect draw arguments buffer.
    RESOURCE_STATE_TRANSITION_MODE IndirectAttribsBufferStateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;

    /// Offset from the beginning of the buffer to the location of draw command attributes.
    Uint32 IndirectDrawArgsOffset   = 0;
    
    /// Initializes the structure members with default values

    /// Default values:
    /// Member                                   | Default value
    /// -----------------------------------------|--------------------------------------
    /// Flags                                    | DRAW_FLAG_NONE
    /// IndirectAttribsBufferStateTransitionMode | RESOURCE_STATE_TRANSITION_MODE_NONE
    /// IndirectDrawArgsOffset                   | 0
    DrawIndirectAttribs()noexcept{}

    /// Initializes the structure members with user-specified values.
    DrawIndirectAttribs(DRAW_FLAGS                     _Flags,
                        RESOURCE_STATE_TRANSITION_MODE _IndirectAttribsBufferStateTransitionMode,
                        Uint32                         _IndirectDrawArgsOffset = 0)noexcept :
        Flags                                   {_Flags                                   },
        IndirectAttribsBufferStateTransitionMode{_IndirectAttribsBufferStateTransitionMode},
        IndirectDrawArgsOffset                  {_IndirectDrawArgsOffset                  }
    {}
};


/// Defines the indexed indirect draw command attributes.

/// This structure is used by IDeviceContext::DrawIndexedIndirect().
struct DrawIndexedIndirectAttribs
{
    /// The type of the elements in the index buffer.
    /// Allowed values: VT_UINT16 and VT_UINT32. Ignored if DrawAttribs::IsIndexed is False.
    VALUE_TYPE IndexType            = VT_UNDEFINED;

    /// Additional flags, see Diligent::DRAW_FLAGS.
    DRAW_FLAGS Flags                = DRAW_FLAG_NONE;

    /// State transition mode for indirect draw arguments buffer.
    RESOURCE_STATE_TRANSITION_MODE IndirectAttribsBufferStateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;

    /// Offset from the beginning of the buffer to the location of draw command attributes.
    Uint32 IndirectDrawArgsOffset   = 0;


    /// Initializes the structure members with default values

    /// Default values:
    /// Member                                   | Default value
    /// -----------------------------------------|--------------------------------------
    /// IndexType                                | VT_UNDEFINED
    /// Flags                                    | DRAW_FLAG_NONE
    /// IndirectAttribsBufferStateTransitionMode | RESOURCE_STATE_TRANSITION_MODE_NONE
    /// IndirectDrawArgsOffset                   | 0
    DrawIndexedIndirectAttribs()noexcept{}

    /// Initializes the structure members with user-specified values.
    DrawIndexedIndirectAttribs(VALUE_TYPE                     _IndexType,
                               DRAW_FLAGS                     _Flags,
                               RESOURCE_STATE_TRANSITION_MODE _IndirectAttribsBufferStateTransitionMode,
                               Uint32                         _IndirectDrawArgsOffset = 0)noexcept : 
        IndexType                               {_IndexType                               },
        Flags                                   {_Flags                                   },
        IndirectAttribsBufferStateTransitionMode{_IndirectAttribsBufferStateTransitionMode},
        IndirectDrawArgsOffset                  {_IndirectDrawArgsOffset                  }
    {}
};


/// Defines which parts of the depth-stencil buffer to clear.

/// These flags are used by IDeviceContext::ClearDepthStencil().
enum CLEAR_DEPTH_STENCIL_FLAGS : Uint32
{
    /// Perform no clear.
    CLEAR_DEPTH_FLAG_NONE = 0x00,  

    /// Clear depth part of the buffer.
    CLEAR_DEPTH_FLAG      = 0x01,  

    /// Clear stencil part of the buffer.
    CLEAR_STENCIL_FLAG    = 0x02   
};
DEFINE_FLAG_ENUM_OPERATORS(CLEAR_DEPTH_STENCIL_FLAGS)


/// Describes dispatch command arguments.

/// This structure is used by IDeviceContext::DispatchCompute().
struct DispatchComputeAttribs
{
    Uint32 ThreadGroupCountX = 1; ///< Number of groups dispatched in X direction.
    Uint32 ThreadGroupCountY = 1; ///< Number of groups dispatched in Y direction.
    Uint32 ThreadGroupCountZ = 1; ///< Number of groups dispatched in Z direction.

    DispatchComputeAttribs()noexcept{}

    /// Initializes the structure with user-specified values.
    DispatchComputeAttribs(Uint32 GroupsX, Uint32 GroupsY, Uint32 GroupsZ = 1)noexcept :
        ThreadGroupCountX {GroupsX},
        ThreadGroupCountY {GroupsY},
        ThreadGroupCountZ {GroupsZ}
    {}
};

/// Describes dispatch command arguments.

/// This structure is used by IDeviceContext::DispatchComputeIndirect().
struct DispatchComputeIndirectAttribs
{
    /// State transition mode for indirect dispatch attributes buffer.
    RESOURCE_STATE_TRANSITION_MODE IndirectAttribsBufferStateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;

    /// The offset from the beginning of the buffer to the dispatch command arguments.
    Uint32  DispatchArgsByteOffset    = 0;

    DispatchComputeIndirectAttribs()noexcept{}

    /// Initializes the structure with user-specified values.
    explicit
    DispatchComputeIndirectAttribs(RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                   Uint32                         Offset              = 0) :
        IndirectAttribsBufferStateTransitionMode{StateTransitionMode},
        DispatchArgsByteOffset                  {Offset             }
    {}
};

/// Defines allowed flags for IDeviceContext::SetVertexBuffers() function.
enum SET_VERTEX_BUFFERS_FLAGS : Uint8
{
    /// No extra operations.
    SET_VERTEX_BUFFERS_FLAG_NONE  = 0x00,

    /// Reset the vertex buffers to only the buffers specified in this
    /// call. All buffers previously bound to the pipeline will be unbound.
    SET_VERTEX_BUFFERS_FLAG_RESET = 0x01
};
DEFINE_FLAG_ENUM_OPERATORS(SET_VERTEX_BUFFERS_FLAGS)


/// Describes the viewport.

/// This structure is used by IDeviceContext::SetViewports().
struct Viewport
{
    /// X coordinate of the left boundary of the viewport.
    Float32 TopLeftX    = 0.f;

    /// Y coordinate of the top boundary of the viewport.
    /// When defining a viewport, DirectX convention is used:
    /// window coordinate systems originates in the LEFT TOP corner
    /// of the screen with Y axis pointing down.
    Float32 TopLeftY    = 0.f;

    /// Viewport width.
    Float32 Width       = 0.f;

    /// Viewport Height.
    Float32 Height      = 0.f;

    /// Minimum depth of the viewport. Ranges between 0 and 1.
    Float32 MinDepth    = 0.f;

    /// Maximum depth of the viewport. Ranges between 0 and 1.
    Float32 MaxDepth    = 1.f;

    /// Initializes the structure.
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
///          window coordinate systems originates in the LEFT TOP corner
///          of the screen with Y axis pointing down.
struct Rect
{
    Int32 left   = 0;  ///< X coordinate of the left boundary of the viewport.
    Int32 top    = 0;  ///< Y coordinate of the top boundary of the viewport.
    Int32 right  = 0;  ///< X coordinate of the right boundary of the viewport.
    Int32 bottom = 0;  ///< Y coordinate of the bottom boundary of the viewport.

    /// Initializes the structure
    Rect(Int32 _left, Int32 _top, Int32 _right, Int32 _bottom)noexcept : 
        left  ( _left   ),
        top   ( _top    ),
        right ( _right  ),
        bottom( _bottom )
    {}

    Rect()noexcept{}

    bool IsValid() const
    {
        return right > left && bottom > top;
    }
};


/// Defines copy texture command attributes.

/// This structure is used by IDeviceContext::CopyTexture().
struct CopyTextureAttribs
{
    /// Source texture to copy data from.
    ITexture*                      pSrcTexture              = nullptr;  

    /// Mip level of the source texture to copy data from.
    Uint32                         SrcMipLevel              = 0;

    /// Array slice of the source texture to copy data from. Must be 0 for non-array textures.
    Uint32                         SrcSlice                 = 0;
    
    /// Source region to copy. Use nullptr to copy the entire subresource.
    const Box*                     pSrcBox                  = nullptr;  
    
    /// Source texture state transition mode (see Diligent::RESOURCE_STATE_TRANSITION_MODE).
    RESOURCE_STATE_TRANSITION_MODE SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;

    /// Destination texture.
    ITexture*                      pDstTexture              = nullptr;

    /// Destination mip level.
    Uint32                         DstMipLevel              = 0;

    /// Destination array slice. Must be 0 for non-array textures.
    Uint32                         DstSlice                 = 0;

    /// X offset on the destination subresource.
    Uint32                         DstX                     = 0;

    /// Y offset on the destination subresource.
    Uint32                         DstY                     = 0;

    /// Z offset on the destination subresource
    Uint32                         DstZ                     = 0;

    /// Destination texture state transition mode (see Diligent::RESOURCE_STATE_TRANSITION_MODE).
    RESOURCE_STATE_TRANSITION_MODE DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_NONE;

    CopyTextureAttribs()noexcept{}

    CopyTextureAttribs(ITexture*                      _pSrcTexture,
                       RESOURCE_STATE_TRANSITION_MODE _SrcTextureTransitionMode,
                       ITexture*                      _pDstTexture,
                       RESOURCE_STATE_TRANSITION_MODE _DstTextureTransitionMode)noexcept :
        pSrcTexture             (_pSrcTexture),
        SrcTextureTransitionMode(_SrcTextureTransitionMode),
        pDstTexture             (_pDstTexture),
        DstTextureTransitionMode(_DstTextureTransitionMode)
    {}
};

/// Device context interface.

/// \remarks Device context keeps strong references to all objects currently bound to 
///          the pipeline: buffers, states, samplers, shaders, etc.
///          The context also keeps strong reference to the device and
///          the swap chain.
class IDeviceContext : public IObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details.
    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override = 0;

    /// Sets the pipeline state.

    /// \param [in] pPipelineState - Pointer to IPipelineState interface to bind to the context.
    virtual void SetPipelineState(IPipelineState* pPipelineState) = 0;


    /// Transitions shader resources to the states required by Draw or Dispatch command.
    ///
    /// \param [in] pPipelineState         - Pipeline state object that was used to create the shader resource binding.
    /// \param [in] pShaderResourceBinding - Shader resource binding whose resources will be transitioned.
    ///
    /// \remarks This method explicitly transitiones all resources except ones in unknown state to the states required 
    ///          by Draw or Dispatch command.
    ///          If this method was called, there is no need to use Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION
    ///          when calling IDeviceContext::CommitShaderResources()
    ///
    /// \remarks Resource state transitioning is not thread safe. As the method may alter the states 
    ///          of resources referenced by the shader resource binding, no other thread is allowed to read or 
    ///          write these states.
    ///
    ///          If the application intends to use the same resources in other threads simultaneously, it needs to 
    ///          explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    ///          Refer to http://diligentgraphics.com/2018/12/09/resource-state-management/ for detailed explanation
    ///          of resource state management in Diligent Engine.
    virtual void TransitionShaderResources(IPipelineState* pPipelineState, IShaderResourceBinding* pShaderResourceBinding) = 0;

    /// Commits shader resources to the device context.

    /// \param [in] pShaderResourceBinding - Shader resource binding whose resources will be committed.
    ///                                      If pipeline state contains no shader resources, this parameter
    ///                                      can be null.
    /// \param [in] StateTransitionMode    - State transition mode (see Diligent::RESOURCE_STATE_TRANSITION_MODE).
    ///
    /// \remarks Pipeline state object that was used to create the shader resource binding must be bound 
    ///          to the pipeline when CommitShaderResources() is called. If no pipeline state object is bound
    ///          or the pipeline state object does not match the shader resource binding, the method will fail.\n
    ///          If Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode is used,
    ///          the engine will also transition all shader resources to required states. If the flag
    ///          is not set, it is assumed that all resources are already in correct states.\n
    ///          Resources can be explicitly transitioned to required states by calling 
    ///          IDeviceContext::TransitionShaderResources() or IDeviceContext::TransitionResourceStates().\n
    ///
    /// \remarks Automatic resource state transitioning is not thread-safe.
    ///
    ///          - If Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode is used, the method may alter the states 
    ///            of resources referenced by the shader resource binding and no other thread is allowed to read or write these states.
    ///
    ///          - If Diligent::RESOURCE_STATE_TRANSITION_MODE_VERIFY mode is used, the method will read the states, so no other thread
    ///            should alter the states by calling any of the methods that use Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode.
    ///            It is safe for other threads to read the states.
    ///
    ///          - If Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE mode is used, the method does not access the states of resources.
    ///
    ///          If the application intends to use the same resources in other threads simultaneously, it should manage the states
    ///          manually by setting the state to Diligent::RESOURCE_STATE_UNKNOWN (which will disable automatic state 
    ///          management) using IBuffer::SetState() or ITexture::SetState() and explicitly transitioning the states with 
    ///          IDeviceContext::TransitionResourceStates().
    ///          Refer to http://diligentgraphics.com/2018/12/09/resource-state-management/ for detailed explanation
    ///          of resource state management in Diligent Engine.
    virtual void CommitShaderResources(IShaderResourceBinding* pShaderResourceBinding, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) = 0;

    /// Sets the stencil reference value.

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

    /// \param [in] StartSlot           - The first input slot for binding. The first vertex buffer is 
    ///                                   explicitly bound to the start slot; each additional vertex buffer 
    ///                                   in the array is implicitly bound to each subsequent input slot. 
    /// \param [in] NumBuffersSet       - The number of vertex buffers in the array.
    /// \param [in] ppBuffers           - A pointer to an array of vertex buffers. 
    ///                                   The buffers must have been created with the Diligent::BIND_VERTEX_BUFFER flag.
    /// \param [in] pOffsets            - Pointer to an array of offset values; one offset value for each buffer 
    ///                                   in the vertex-buffer array. Each offset is the number of bytes between 
    ///                                   the first element of a vertex buffer and the first element that will be 
    ///                                   used. If this parameter is nullptr, zero offsets for all buffers will be used.
    /// \param [in] StateTransitionMode - State transition mode for buffers being set (see Diligent::RESOURCE_STATE_TRANSITION_MODE).
    /// \param [in] Flags               - Additional flags. See Diligent::SET_VERTEX_BUFFERS_FLAGS for a list of allowed values.
    ///                                   
    /// \remarks The device context keeps strong references to all bound vertex buffers.
    ///          Thus a buffer cannot be released until it is unbound from the context.\n
    ///          It is suggested to specify Diligent::SET_VERTEX_BUFFERS_FLAG_RESET flag
    ///          whenever possible. This will assure that no buffers from previous draw calls
    ///          are bound to the pipeline.
    ///
    /// \remarks When StateTransitionMode is Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, the method will 
    ///          transition all buffers in known states to Diligent::RESOURCE_STATE_VERTEX_BUFFER. Resource state 
    ///          transitioning is not thread safe, so no other thread is allowed to read or write the states of 
    ///          these buffers.
    ///
    ///          If the application intends to use the same resources in other threads simultaneously, it needs to 
    ///          explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    ///          Refer to http://diligentgraphics.com/2018/12/09/resource-state-management/ for detailed explanation
    ///          of resource state management in Diligent Engine.
    virtual void SetVertexBuffers(Uint32                         StartSlot, 
                                  Uint32                         NumBuffersSet, 
                                  IBuffer**                      ppBuffers, 
                                  Uint32*                        pOffsets,
                                  RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                  SET_VERTEX_BUFFERS_FLAGS       Flags) = 0;


    /// Invalidates the cached context state.

    /// This method should be called by an application to invalidate 
    /// internal cached states.
    virtual void InvalidateState() = 0;


    /// Binds an index buffer to the pipeline.
    
    /// \param [in] pIndexBuffer        - Pointer to the index buffer. The buffer must have been created 
    ///                                   with the Diligent::BIND_INDEX_BUFFER flag.
    /// \param [in] ByteOffset          - Offset from the beginning of the buffer to 
    ///                                   the start of index data.
    /// \param [in] StateTransitionMode - State transiton mode for the index buffer to bind (see Diligent::RESOURCE_STATE_TRANSITION_MODE).
    ///
    /// \remarks The device context keeps strong reference to the index buffer.
    ///          Thus an index buffer object cannot be released until it is unbound 
    ///          from the context.
    ///
    /// \remarks When StateTransitionMode is Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, the method will 
    ///          transition the buffer to Diligent::RESOURCE_STATE_INDEX_BUFFER (if its state is not unknown). Resource 
    ///          state transitioning is not thread safe, so no other thread is allowed to read or write the state of 
    ///          the buffer.
    ///
    ///          If the application intends to use the same resource in other threads simultaneously, it needs to 
    ///          explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    ///          Refer to http://diligentgraphics.com/2018/12/09/resource-state-management/ for detailed explanation
    ///          of resource state management in Diligent Engine.
    virtual void SetIndexBuffer(IBuffer* pIndexBuffer, Uint32 ByteOffset, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) = 0;


    /// Sets an array of viewports.

    /// \param [in] NumViewports - Number of viewports to set.
    /// \param [in] pViewports   - An array of Viewport structures describing the viewports to bind.
    /// \param [in] RTWidth      - Render target width. If 0 is provided, width of the currently bound render target will be used.
    /// \param [in] RTHeight     - Render target height. If 0 is provided, height of the currently bound render target will be used.
    ///
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


    /// Sets active scissor rects.

    /// \param [in] NumRects - Number of scissor rectangles to set.
    /// \param [in] pRects   - An array of Rect structures describing the scissor rectangles to bind.
    /// \param [in] RTWidth  - Render target width. If 0 is provided, width of the currently bound render target will be used.
    /// \param [in] RTHeight - Render target height. If 0 is provided, height of the currently bound render target will be used.
    ///
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

    /// \param [in] NumRenderTargets    - Number of render targets to bind.
    /// \param [in] ppRenderTargets     - Array of pointers to ITextureView that represent the render 
    ///                                   targets to bind to the device. The type of each view in the 
    ///                                   array must be Diligent::TEXTURE_VIEW_RENDER_TARGET.
    /// \param [in] pDepthStencil       - Pointer to the ITextureView that represents the depth stencil to 
    ///                                   bind to the device. The view type must be
    ///                                   Diligent::TEXTURE_VIEW_DEPTH_STENCIL.
    /// \param [in] StateTransitionMode - State transition mode of the render targets and depth stencil buffer being set (see Diligent::RESOURCE_STATE_TRANSITION_MODE).
    /// 
    /// \remarks     The device context will keep strong references to all bound render target 
    ///              and depth-stencil views. Thus these views (and consequently referenced textures) 
    ///              cannot be released until they are unbound from the context.\n
    ///              Any render targets not defined by this call are set to nullptr.\n\n
    ///              You can set the default render target and depth stencil using the
    ///              following call:
    ///
    ///     pContext->SetRenderTargets(0, nullptr, nullptr);
    ///
    /// \remarks When StateTransitionMode is Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, the method will 
    ///          transition all render targets in known states to Diligent::RESOURCE_STATE_REDER_TARGET,
    ///          and the depth-stencil buffer to Diligent::RESOURCE_STATE_DEPTH_WRITE state.
    ///          Resource state transitioning is not thread safe, so no other thread is allowed to read or write 
    ///          the states of resources used by the command.
    ///
    ///          If the application intends to use the same resource in other threads simultaneously, it needs to 
    ///          explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    ///          Refer to http://diligentgraphics.com/2018/12/09/resource-state-management/ for detailed explanation
    ///          of resource state management in Diligent Engine.
    virtual void SetRenderTargets(Uint32                         NumRenderTargets,
                                  ITextureView*                  ppRenderTargets[],
                                  ITextureView*                  pDepthStencil,
                                  RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) = 0;


    /// Executes a draw command.

    /// \param [in] Attribs - Draw command attributes, see Diligent::DrawAttribs for details.
    ///
    /// \remarks  If Diligent::DRAW_FLAG_VERIFY_STATES flag is set, the method reads the state of vertex
    ///           buffers, so no other threads are allowed to alter the states of the same resources.
    ///           It is OK to read these states.
    ///          
    ///           If the application intends to use the same resources in other threads simultaneously, it needs to 
    ///           explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    virtual void Draw(const DrawAttribs& Attribs) = 0;


    /// Executes an indexed draw command.

    /// \param [in] Attribs - Draw command attributes, see Diligent::DrawIndexedAttribs for details.
    ///
    /// \remarks  If Diligent::DRAW_FLAG_VERIFY_STATES flag is set, the method reads the state of vertex/index
    ///           buffers, so no other threads are allowed to alter the states of the same resources.
    ///           It is OK to read these states.
    ///          
    ///           If the application intends to use the same resources in other threads simultaneously, it needs to 
    ///           explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    virtual void DrawIndexed(const DrawIndexedAttribs& Attribs) = 0;


    /// Executes an indirect draw command.

    /// \param [in] Attribs        - Structure describing the command attributes, see Diligent::DrawIndirectAttribs for details.
    /// \param [in] pAttribsBuffer - Pointer to the buffer, from which indirect draw attributes will be read.
    ///
    /// \remarks  If IndirectAttribsBufferStateTransitionMode member is Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
    ///           the method may transition the state of the indirect draw arguments buffer. This is not a thread safe operation, 
    ///           so no other thread is allowed to read or write the state of the buffer.
    ///
    ///           If Diligent::DRAW_FLAG_VERIFY_STATES flag is set, the method reads the state of vertex/index
    ///           buffers, so no other threads are allowed to alter the states of the same resources.
    ///           It is OK to read these states.
    ///          
    ///           If the application intends to use the same resources in other threads simultaneously, it needs to 
    ///           explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    virtual void DrawIndirect(const DrawIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) = 0;


    /// Executes an indexed indirect draw command.

    /// \param [in] Attribs        - Structure describing the command attributes, see Diligent::DrawIndexedIndirectAttribs for details.
    /// \param [in] pAttribsBuffer - Pointer to the buffer, from which indirect draw attributes will be read.
    ///
    /// \remarks  If IndirectAttribsBufferStateTransitionMode member is Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
    ///           the method may transition the state of the indirect draw arguments buffer. This is not a thread safe operation, 
    ///           so no other thread is allowed to read or write the state of the buffer.
    ///
    ///           If Diligent::DRAW_FLAG_VERIFY_STATES flag is set, the method reads the state of vertex/index
    ///           buffers, so no other threads are allowed to alter the states of the same resources.
    ///           It is OK to read these states.
    ///          
    ///           If the application intends to use the same resources in other threads simultaneously, it needs to 
    ///           explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    virtual void DrawIndexedIndirect(const DrawIndexedIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) = 0;


    /// Executes a dispatch compute command.
    
    /// \param [in] Attribs - Dispatch command attributes, see Diligent::DispatchComputeAttribs for details.
    virtual void DispatchCompute(const DispatchComputeAttribs& Attribs) = 0;


    /// Executes an indirect dispatch compute command.
    
    /// \param [in] Attribs        - The command attributes, see Diligent::DispatchComputeIndirectAttribs for details.
    /// \param [in] pAttribsBuffer - Pointer to the buffer containing indirect dispatch arguments.
    ///
    /// \remarks  If IndirectAttribsBufferStateTransitionMode member is Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
    ///           the method may transition the state of indirect dispatch arguments buffer. This is not a thread safe operation, 
    ///           so no other thread is allowed to read or write the state of the same resource.
    ///          
    ///           If the application intends to use the same resources in other threads simultaneously, it needs to 
    ///           explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    virtual void DispatchComputeIndirect(const DispatchComputeIndirectAttribs& Attribs, IBuffer* pAttribsBuffer) = 0;


    /// Clears a depth-stencil view.
    
    /// \param [in] pView               - Pointer to ITextureView interface to clear. The view type must be 
    ///                                   Diligent::TEXTURE_VIEW_DEPTH_STENCIL.
    /// \param [in] StateTransitionMode - state transition mode of the depth-stencil buffer to clear.
    /// \param [in] ClearFlags          - Idicates which parts of the buffer to clear, see Diligent::CLEAR_DEPTH_STENCIL_FLAGS.
    /// \param [in] fDepth              - Value to clear depth part of the view with.
    /// \param [in] Stencil             - Value to clear stencil part of the view with.
    ///
    /// \remarks The full extent of the view is always cleared. Viewport and scissor settings are not applied.
    /// \note The depth-stencil view must be bound to the pipeline for clear operation to be performed.
    ///
    /// \remarks When StateTransitionMode is Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, the method will 
    ///          transition the state of the texture to the state required by clear operation. 
    ///          In Direct3D12, this satate is always Diligent::RESOURCE_STATE_DEPTH_WRITE, however in Vulkan
    ///          the state depends on whether the depth buffer is bound to the pipeline.
    ///
    ///          Resource state transitioning is not thread safe, so no other thread is allowed to read or write 
    ///          the state of resources used by the command.
    ///          Refer to http://diligentgraphics.com/2018/12/09/resource-state-management/ for detailed explanation
    ///          of resource state management in Diligent Engine.
    virtual void ClearDepthStencil(ITextureView*                  pView,
                                   CLEAR_DEPTH_STENCIL_FLAGS      ClearFlags,
                                   float                          fDepth,
                                   Uint8                          Stencil,
                                   RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) = 0;


    /// Clears a render target view

    /// \param [in] pView               - Pointer to ITextureView interface to clear. The view type must be 
    ///                                   Diligent::TEXTURE_VIEW_RENDER_TARGET.
    /// \param [in] RGBA                - A 4-component array that represents the color to fill the render target with.
    ///                                   If nullptr is provided, the default array {0,0,0,0} will be used.
    /// \param [in] StateTransitionMode - Defines required state transitions (see Diligent::RESOURCE_STATE_TRANSITION_MODE)
    ///
    /// \remarks The full extent of the view is always cleared. Viewport and scissor settings are not applied.
    ///
    ///          The render target view must be bound to the pipeline for clear operation to be performed in OpenGL backend.
    ///
    /// \remarks When StateTransitionMode is Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, the method will 
    ///          transition the texture to the state required by the command. Resource state transitioning is not 
    ///          thread safe, so no other thread is allowed to read or write the states of the same textures.
    ///
    ///          If the application intends to use the same resource in other threads simultaneously, it needs to 
    ///          explicitly manage the states using IDeviceContext::TransitionResourceStates() method.
    ///
    /// \note    In D3D12 backend clearing render targets requires textures to always be transitioned to 
    ///          Diligent::RESOURCE_STATE_RENDER_TARGET state. In Vulkan backend however this depends on whether a 
    ///          render pass has been started. To clear render target outside of a render pass, the texture must be transitioned to
    ///          Diligent::RESOURCE_STATE_COPY_DEST state. Inside a render pass it must be in Diligent::RESOURCE_STATE_RENDER_TARGET
    ///          state. When using Diligent::RESOURCE_STATE_TRANSITION_TRANSITION mode, the engine takes care of proper
    ///          resource state transition, otherwise it is the responsibility of the application.
    virtual void ClearRenderTarget(ITextureView* pView, const float* RGBA, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) = 0;


    /// Finishes recording commands and generates a command list.
    
    /// \param [out] ppCommandList - Memory location where pointer to the recorded command list will be written.
    virtual void FinishCommandList(ICommandList **ppCommandList) = 0;


    /// Executes recorded commands in a command list.

    /// \param [in] pCommandList - Pointer to the command list to executre.
    /// \remarks After command list is executed, it is no longer valid and should be released.
    virtual void ExecuteCommandList(ICommandList* pCommandList) = 0;


    /// Tells the GPU to set a fence to a specified value after all previous work has completed.

    /// \note The method does not flush the context (an application can do this explcitly if needed)
    ///       and the fence will be signaled only when the command context is flushed next time.
    ///       If an application needs to wait for the fence in a loop, it must flush the context
    ///       after signalling the fence.
    ///
    /// \param [in] pFence - The fence to signal
    /// \param [in] Value  - The value to set the fence to. This value must be greater than the
    ///                      previously signaled value on the same fence.
    virtual void SignalFence(IFence* pFence, Uint64 Value) = 0;


    /// Waits until the specified fence reaches or exceeds the specified value, on the host.

    /// \note The method blocks the execution of the calling thread until the wait is complete.
    ///
    /// \param [in] pFence       - The fence to wait.
    /// \param [in] Value        - The value that the context is waiting for the fence to reach.
    /// \param [in] FlushContext - Whether to flush the commands in the context before initiating the wait.
    ///
    /// \remarks    Wait is only allowed for immediate contexts.\n
    ///             When FlushContext is true, the method flushes the context before initiating the wait 
    ///             (see IDeviceContext::Flush()), so an application must explicitly reset the PSO and 
    ///             bind all required shader resources after waiting for the fence.\n
    ///             If FlushContext is false, the commands preceding the fence (including signaling the fence itself)
    ///             may not have been submitted to the GPU and the method may never return.  If an application does 
    ///             not explicitly flush the context, it should typically set FlushContext to true.\n
    ///             If the value the context is waiting for has never been signaled, the method
    ///             may never return.\n
    ///             The fence can only be waited for from the same context it has
    ///             previously been signaled.
    virtual void WaitForFence(IFence* pFence, Uint64 Value, bool FlushContext) = 0;


    /// Submits all outstanding commands for execution to the GPU and waits until they are complete.

    /// \note The method blocks the execution of the calling thread until the wait is complete.
    ///
    /// \remarks    Only immediate contexts can be idled.\n
    ///             The methods implicitly flushes the context (see IDeviceContext::Flush()), so an 
    ///             application must explicitly reset the PSO and bind all required shader resources after 
    ///             idling the context.\n
    virtual void WaitForIdle() = 0;


    /// Submits all pending commands in the context for execution to the command queue.

    /// \remarks    Only immediate contexts can be flushed.\n
    ///             Internally the method resets the state of the current command list/buffer.
    ///             When the next draw command is issued, the engine will restore all states 
    ///             (rebind render targets and depth-stencil buffer as well as index and vertex buffers,
    ///             restore viewports and scissor rects, etc.) except for the pipeline state and shader resource
    ///             bindings. An application must explicitly reset the PSO and bind all required shader 
    ///             resources after flushing the context.
    virtual void Flush() = 0;


    /// Updates the data in the buffer.

    /// \param [in] pBuffer             - Pointer to the buffer to updates.
    /// \param [in] Offset              - Offset in bytes from the beginning of the buffer to the update region.
    /// \param [in] Size                - Size in bytes of the data region to update.
    /// \param [in] pData               - Pointer to the data to write to the buffer.
    /// \param [in] StateTransitionMode - Buffer state transition mode (see Diligent::RESOURCE_STATE_TRANSITION_MODE)
    virtual void UpdateBuffer(IBuffer*                       pBuffer,
                              Uint32                         Offset,
                              Uint32                         Size,
                              const PVoid                    pData,
                              RESOURCE_STATE_TRANSITION_MODE StateTransitionMode) = 0;


    /// Copies the data from one buffer to another.

    /// \param [in] pSrcBuffer              - Source buffer to copy data from.
    /// \param [in] SrcOffset               - Offset in bytes from the beginning of the source buffer to the beginning of data to copy.
    /// \param [in] SrcBufferTransitionMode - State transition mode of the source buffer (see Diligent::RESOURCE_STATE_TRANSITION_MODE).
    /// \param [in] pDstBuffer              - Destination buffer to copy data to.
    /// \param [in] DstOffset               - Offset in bytes from the beginning of the destination buffer to the beginning 
    ///                                       of the destination region.
    /// \param [in] Size                    - Size in bytes of data to copy.
    /// \param [in] DstBufferTransitionMode - State transition mode of the destination buffer (see Diligent::RESOURCE_STATE_TRANSITION_MODE).
    virtual void CopyBuffer(IBuffer*                       pSrcBuffer,
                            Uint32                         SrcOffset,
                            RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                            IBuffer*                       pDstBuffer,
                            Uint32                         DstOffset,
                            Uint32                         Size,
                            RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode) = 0;


    /// Maps the buffer.

    /// \param [in] pBuffer      - Pointer to the buffer to map.
    /// \param [in] MapType      - Type of the map operation. See Diligent::MAP_TYPE.
    /// \param [in] MapFlags     - Special map flags. See Diligent::MAP_FLAGS.
    /// \param [out] pMappedData - Reference to the void pointer to store the address of the mapped region.
    virtual void MapBuffer( IBuffer* pBuffer, MAP_TYPE MapType, MAP_FLAGS MapFlags, PVoid& pMappedData ) = 0;


    /// Unmaps the previously mapped buffer.

    /// \param [in] pBuffer - Pointer to the buffer to unmap.
    /// \param [in] MapType - Type of the map operation. This parameter must match the type that was 
    ///                       provided to the Map() method. 
    virtual void UnmapBuffer( IBuffer* pBuffer, MAP_TYPE MapType ) = 0;


    /// Updates the data in the texture.

    /// \param [in] pTexture    - Pointer to the device context interface to be used to perform the operation.
    /// \param [in] MipLevel    - Mip level of the texture subresource to update.
    /// \param [in] Slice       - Array slice. Should be 0 for non-array textures.
    /// \param [in] DstBox      - Destination region on the texture to update.
    /// \param [in] SubresData  - Source data to copy to the texture.
    /// \param [in] SrcBufferTransitionMode - If pSrcBuffer member of TextureSubResData structure is not null, this 
    ///                                       parameter defines state transition mode of the source buffer. 
    ///                                       If pSrcBuffer is null, this parameter is ignored.
    /// \param [in] TextureTransitionMode   - Texture state transition mode (see Diligent::RESOURCE_STATE_TRANSITION_MODE)
    virtual void UpdateTexture(ITexture*                      pTexture,
                               Uint32                         MipLevel,
                               Uint32                         Slice,
                               const Box&                     DstBox,
                               const TextureSubResData&       SubresData,
                               RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                               RESOURCE_STATE_TRANSITION_MODE TextureTransitionMode) = 0;


    /// Copies data from one texture to another.

    /// \param [in] CopyAttribs - Structure describing copy command attributes, see Diligent::CopyTextureAttribs for details.
    virtual void CopyTexture(const CopyTextureAttribs& CopyAttribs) = 0;


    /// Maps the texture subresource.

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
    ///          with MAP_FLAG_DISCARD has exactly the same behavior.
    virtual void MapTextureSubresource( ITexture*                 pTexture,
                                        Uint32                    MipLevel,
                                        Uint32                    ArraySlice,
                                        MAP_TYPE                  MapType,
                                        MAP_FLAGS                 MapFlags,
                                        const Box*                pMapRegion,
                                        MappedTextureSubresource& MappedData ) = 0;


    /// Unmaps the texture subresource.
    virtual void UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice) = 0;

    
    /// Generates a mipmap chain.

    /// \param [in] pTextureView - Texture view to generate mip maps for.
    /// \remarks This function can only be called for a shader resource view.
    ///          The texture must be created with MISC_TEXTURE_FLAG_GENERATE_MIPS flag.
    virtual void GenerateMips(ITextureView* pTextureView) = 0;


    /// Sets the swap chain in the device context.

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


    /// Finishes the current frame and releases dynamic resources allocated by the context.

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


    /// Transitions resource states.

    /// \param [in] BarrierCount      - Number of barriers in pResourceBarriers array
    /// \param [in] pResourceBarriers - Pointer to the array of resource barriers
    /// \remarks When both old and new states are RESOURCE_STATE_UNORDERED_ACCESS, the engine
    ///          executes UAV barrier on the resource. The barrier makes sure that all UAV accesses 
    ///          (reads or writes) are complete before any future UAV accesses (read or write) can begin.\n
    /// 
    ///          There are two main usage scenarios for this method:
    ///          1. An application knows specifics of resource state transitions not available to the engine.
    ///             For example, only single mip level needs to be transitioned.
    ///          2. An application manages resource states in multiple threads in parallel.
    ///         
    ///          The method always reads the states of all resources to transition. If the state of a resource is managed
    ///          by multiple threads in parallel, the resource must first be transitioned to unknown state
    ///          (Diligent::RESOURCE_STATE_UNKNOWN) to disable automatic state management in the engine.
    ///          
    ///          When StateTransitionDesc::UpdateResourceState is set to true, the method may update the state of the
    ///          corresponding resource which is not thread safe. No other threads should read or write the sate of that 
    ///          resource.

    /// \note    Any method that uses Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode may alter
    ///          the state of resources it works with. Diligent::RESOURCE_STATE_TRANSITION_MODE_VERIFY mode
    ///          makes the method read the states, but not write them. When Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE
    ///          is used, the method assumes the states are guaranteed to be correct and does not read or write them.
    ///          It is the responsibility of the application to make sure this is indeed true.
    ///
    ///          Refer to http://diligentgraphics.com/2018/12/09/resource-state-management/ for detailed explanation
    ///          of resource state management in Diligent Engine.
    virtual void TransitionResourceStates(Uint32 BarrierCount, StateTransitionDesc* pResourceBarriers) = 0;
};

}
