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
/// Contains basic graphics engine type defintions

#include "../../../Primitives/interface/BasicTypes.h"
#include "../../../Primitives/interface/DebugOutput.h"

/// Graphics engine namespace
namespace Diligent
{
    /// Value type

    /// This enumeration describes value type. It is used by
    /// - BufferDesc structure to describe value type of a formatted buffer
    /// - DrawAttribs structure to describe index type for an indexed draw call
    enum VALUE_TYPE : Int32
    {
        VT_UNDEFINED = 0, ///< Undefined type
        VT_INT8,          ///< Signed 8-bit integer
        VT_INT16,         ///< Signed 16-bit integer
        VT_INT32,         ///< Signed 32-bit integer
        VT_UINT8,         ///< Unsigned 8-bit integer
        VT_UINT16,        ///< Unsigned 16-bit integer
        VT_UINT32,        ///< Unsigned 32-bit integer
        VT_FLOAT16,       ///< Half-precision 16-bit floating point
        VT_FLOAT32,       ///< Full-precision 32-bit floating point
        VT_NUM_TYPES      ///< Helper value storing total number of types in the enumeration
    };

    /// Resource binding flags

    /// [D3D11_BIND_FLAG]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476085(v=vs.85).aspx
    ///
    /// This enumeration describes which parts of the pipeline a resource can be bound to.
    /// It generally mirrors [D3D11_BIND_FLAG][] enumeration. It is used by
    /// - BufferDesc to describe bind flags for a buffer
    /// - TextureDesc to describe bind flags for a texture
    enum BIND_FLAGS : Int32
    {
        BIND_NONE               = 0x0L, ///< Undefined binding
        BIND_VERTEX_BUFFER	    = 0x1L, ///< A buffer can be bound as a vertex buffer
	    BIND_INDEX_BUFFER	    = 0x2L, ///< A buffer can be bound as an index buffer
	    BIND_UNIFORM_BUFFER	    = 0x4L, ///< A buffer can be bound as a uniform buffer
                                        ///  \warning This flag may not be combined with any other bind flag
	    BIND_SHADER_RESOURCE	= 0x8L, ///< A buffer or a texture can be bound as a shader resource
                                        ///  \warning This flag cannot be used with MAP_WRITE_NO_OVERWRITE flag 
	    BIND_STREAM_OUTPUT	    = 0x10L,///< A buffer can be bound as a target for stream output stage
	    BIND_RENDER_TARGET	    = 0x20L,///< A texture can be bound as a render target
	    BIND_DEPTH_STENCIL	    = 0x40L,///< A texture can be bound as a depth-stencil target
	    BIND_UNORDERED_ACCESS	= 0x80L,///< A buffer or a texture can be bound as an unordered access view
        BIND_INDIRECT_DRAW_ARGS	= 0x100L///< A buffer can be bound as the source buffer for indirect draw commands
    };

    /// Resource usage

    /// [D3D11_USAGE]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476259(v=vs.85).aspx
    /// This enumeration describes expected resource usage. It generally mirrors [D3D11_USAGE] enumeration.
    /// The enumeration is used by
    /// - BufferDesc to describe usage for a buffer
    /// - TextureDesc to describe usage for a texture
    enum USAGE : Int32
    {
        /// A resource that can only be read by the GPU. It cannot be written by the GPU, 
        /// and cannot be accessed at all by the CPU. This type of resource must be initialized 
        /// when it is created, since it cannot be changed after creation. \n
        /// D3D11 Counterpart: D3D11_USAGE_IMMUTABLE. OpenGL counterpart: GL_STATIC_DRAW
        USAGE_STATIC = 0, 

        /// A resource that requires read and write access by the GPU and can also be occasionally
        /// written by the CPU.  \n
        /// D3D11 Counterpart: D3D11_USAGE_DEFAULT. OpenGL counterpart: GL_DYNAMIC_DRAW
        USAGE_DEFAULT,

        /// A resource that can be read by the GPU and written at least once per frame by the CPU.  \n
        /// D3D11 Counterpart: D3D11_USAGE_DYNAMIC. OpenGL counterpart: GL_STREAM_DRAW
        USAGE_DYNAMIC,

        /// A resource that facilitates transferring data from GPU to CPU. \n
        /// D3D11 Counterpart: D3D11_USAGE_STAGING. OpenGL counterpart: GL_DYNAMIC_READ
        USAGE_CPU_ACCESSIBLE
    };

    /// Allowed CPU access mode flags when mapping a resource
    
    /// The enumeration is used by
    /// - BufferDesc to describe CPU access mode for a buffer
    /// - TextureDesc to describe CPU access mode for a texture
    /// \note Only USAGE_DYNAMIC resources can be mapped
    enum CPU_ACCESS_FLAG : Int32
    {
        CPU_ACCESS_READ = 0x01, ///< A resource can be mapped for reading
        CPU_ACCESS_WRITE = 0x02 ///< A resource can be mapped for writing
    };

    /// Resource mapping type

    /// [D3D11_MAP]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476181(v=vs.85).aspx
    /// Describes how a mapped resource will be accessed. This enumeration generally
    /// mirrors [D3D11_MAP][] enumeration. It is used by
    /// - IBuffer::Map to describe buffer mapping type
    /// - ITexture::Map to describe texture mapping type
    enum MAP_TYPE : Int32
    {
        /// Resource is mapped for reading. \n
        /// D3D11 counterpart: D3D11_MAP_READ. OpenGL counterpart: GL_MAP_READ_BIT
        MAP_READ = 0x01,       

        /// Resource is mapped for writing. \n
        /// D3D11 counterpart: D3D11_MAP_WRITE. OpenGL counterpart: GL_MAP_WRITE_BIT
	    MAP_WRITE = 0x02,          

        /// Resource is mapped for reading and writing. \n
        /// D3D11 counterpart: D3D11_MAP_READ_WRITE. OpenGL counterpart: GL_MAP_WRITE_BIT | GL_MAP_READ_BIT
	    MAP_READ_WRITE = 0x03
    };

    /// Special map flags

    /// Describes special arguments for a map operation.
    /// This enumeration is used by
    /// - IBuffer::Map to describe buffer mapping flags
    /// - ITexture::Map to describe texture mapping flags
    enum MAP_FLAGS : Int32
    {
        /// Specifies that map operation should not wait until previous command that
        /// using the same resource completes. Map returns null pointer if the resource
        /// is still in use.\n
        /// D3D11 counterpart:  D3D11_MAP_FLAG_DO_NOT_WAIT
        /// \note: OpenGL does not have corresponding flag, so a buffer will always be mapped
        MAP_FLAG_DO_NOT_WAIT = 0x001,

        /// Previous contents of the resource will be undefined. This flag is only compatible with MAP_WRITE\n
        /// D3D11 counterpart: D3D11_MAP_WRITE_DISCARD. OpenGL counterpart: GL_MAP_INVALIDATE_BUFFER_BIT
        /// \note OpenGL implementation may orphan a buffer instead 
	    MAP_FLAG_DISCARD = 0x002,

        /// The system will not synchronize pending operations before mapping the buffer. It is responsibility
        /// of the application to make sure that the buffer contents is not overwritten while it is in use by 
        /// the GPU.\n
        /// D3D11 counterpart:  D3D11_MAP_WRITE_NO_OVERWRITE. OpenGL counterpart: GL_MAP_UNSYNCHRONIZED_BIT
	    MAP_FLAG_DO_NOT_SYNCHRONIZE = 0x004
    };

    /// Describes resource dimension

    /// This enumeration is used by
    /// - TextureDesc to describe texture type
    /// - TextureViewDesc to describe texture view type
    enum RESOURCE_DIMENSION : Int32
    {
        RESOURCE_DIM_UNDEFINED = 0, ///< Texture type undefined
        RESOURCE_DIM_BUFFER,            ///< Buffer
        RESOURCE_DIM_TEX_1D,            ///< One-dimensional texture
        RESOURCE_DIM_TEX_1D_ARRAY,      ///< One-dimensional texture array
        RESOURCE_DIM_TEX_2D,            ///< Two-dimensional texture
        RESOURCE_DIM_TEX_2D_ARRAY,      ///< Two-dimensional texture array
        RESOURCE_DIM_TEX_3D,            ///< Three-dimensional texture
        RESOURCE_DIM_TEX_CUBE,          ///< Cube-map texture
        RESOURCE_DIM_TEX_CUBE_ARRAY,    ///< Cube-map array texture
        RESOURCE_DIM_NUM_DIMENSIONS     ///< Helper value that stores the total number of texture types in the enumeration
    };

    /// Texture view type

    /// This enumeration describes allowed view types for a texture view. It is used by TextureViewDesc
    /// structure.
    enum TEXTURE_VIEW_TYPE : Int32
    {
        /// Undefined view type
        TEXTURE_VIEW_UNDEFINED = 0,     

        /// A texture view will define a shader resource view that will be used 
        /// as the source for the shader read operations
        TEXTURE_VIEW_SHADER_RESOURCE,

        /// A texture view will define a render target view that will be used
        /// as the target for rendering operations
        TEXTURE_VIEW_RENDER_TARGET,     

        /// A texture view will define a depth stencil view that will be used
        /// as the target for rendering operations
        TEXTURE_VIEW_DEPTH_STENCIL,     

        /// A texture view will define an unordered access view that will be used
        /// for unordered read/write operations from the shaders
        TEXTURE_VIEW_UNORDERED_ACCESS,  

        /// Helper value that stores that total number of texture views
        TEXTURE_VIEW_NUM_VIEWS          
    };

    /// Buffer view type

    /// This enumeration describes allowed view types for a buffer view. It is used by BufferViewDesc
    /// structure.
    enum BUFFER_VIEW_TYPE : Int32
    {
        /// Undefined view type
        BUFFER_VIEW_UNDEFINED = 0,

        /// A buffer view will define a shader resource view that will be used 
        /// as the source for the shader read operations
        BUFFER_VIEW_SHADER_RESOURCE,

        /// A buffer view will define an unordered access view that will be used
        /// for unordered read/write operations from the shaders
        BUFFER_VIEW_UNORDERED_ACCESS,

        /// Helper value that stores that total number of buffer views
        BUFFER_VIEW_NUM_VIEWS
    };

    /// Texture formats

    /// This enumeration describes available texture formats and generally mirrors DXGI_FORMAT enumeration.
    /// The table below provides detailed information on each format. Most of the formats are widely supported 
    /// by all modern APIs (DX10+, OpenGL3.3+ and OpenGLES3.0+). Specific requirements are additionally indicated.
    /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb173059(v=vs.85).aspx">DXGI_FORMAT enumeration on MSDN</a>, 
    ///     <a href = "https://www.opengl.org/wiki/Image_Format">OpenGL Texture Formats</a>
    ///
    enum TEXTURE_FORMAT : Int32
    {
        /// Unknown format
        TEX_FORMAT_UNKNOWN = 0,  
        
        /// Four-component 128-bit typeless format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32B32A32_TYPELESS. OpenGL does not have direct counterpart, GL_RGBA32F is used.
        TEX_FORMAT_RGBA32_TYPELESS,

        /// Four-component 128-bit floating-point format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32B32A32_FLOAT. OpenGL counterpart: GL_RGBA32F.
        TEX_FORMAT_RGBA32_FLOAT,  

        /// Four-component 128-bit unsigned-integer format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32B32A32_UINT. OpenGL counterpart: GL_RGBA32UI.
        TEX_FORMAT_RGBA32_UINT,

        /// Four-component 128-bit signed-integer format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32B32A32_SINT. OpenGL counterpart: GL_RGBA32I.
        TEX_FORMAT_RGBA32_SINT,  

        /// Three-component 96-bit typeless format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32B32_TYPELESS. OpenGL does not have direct counterpart, GL_RGB32F is used.
        /// \warning This format has weak hardware support and is not recommended 
        TEX_FORMAT_RGB32_TYPELESS,  
                                                  
        /// Three-component 96-bit floating-point format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32B32_FLOAT. OpenGL counterpart: GL_RGB32F.
        /// \warning This format has weak hardware support and is not recommended 
        TEX_FORMAT_RGB32_FLOAT,  

        /// Three-component 96-bit unsigned-integer format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32B32_UINT. OpenGL counterpart: GL_RGB32UI.
        /// \warning This format has weak hardware support and is not recommended 
        TEX_FORMAT_RGB32_UINT,  

        /// Three-component 96-bit signed-integer format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32B32_SINT. OpenGL counterpart: GL_RGB32I.
        /// \warning This format has weak hardware support and is not recommended 
        TEX_FORMAT_RGB32_SINT,  

        /// Four-component 64-bit typeless format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16B16A16_TYPELESS. OpenGL does not have direct counterpart, GL_RGBA16F is used.
        TEX_FORMAT_RGBA16_TYPELESS,  

        /// Four-component 64-bit half-precision floating-point format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16B16A16_FLOAT. OpenGL counterpart: GL_RGBA16F.
        TEX_FORMAT_RGBA16_FLOAT, 

        /// Four-component 64-bit unsigned-normalized-integer format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16B16A16_UNORM. OpenGL counterpart: GL_RGBA16. \n
        /// [GL_EXT_texture_norm16]: https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_norm16.txt
        /// OpenGLES: [GL_EXT_texture_norm16][] extension is required
        TEX_FORMAT_RGBA16_UNORM, 

        /// Four-component 64-bit unsigned-integer format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16B16A16_UINT. OpenGL counterpart: GL_RGBA16UI.
        TEX_FORMAT_RGBA16_UINT, 

        /// [GL_EXT_texture_norm16]: https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_norm16.txt
        /// Four-component 64-bit signed-normalized-integer format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16B16A16_SNORM. OpenGL counterpart: GL_RGBA16_SNORM. \n
        /// [GL_EXT_texture_norm16]: https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_norm16.txt
        /// OpenGLES: [GL_EXT_texture_norm16][] extension is required
        TEX_FORMAT_RGBA16_SNORM, 

        /// Four-component 64-bit signed-integer format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16B16A16_SINT. OpenGL counterpart: GL_RGBA16I.
        TEX_FORMAT_RGBA16_SINT, 

        /// Two-component 64-bit typeless format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32_TYPELESS. OpenGL does not have direct counterpart, GL_RG32F is used.
        TEX_FORMAT_RG32_TYPELESS, 

        /// Two-component 64-bit floating-point format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32_FLOAT. OpenGL counterpart: GL_RG32F.
        TEX_FORMAT_RG32_FLOAT, 

        /// Two-component 64-bit unsigned-integer format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32_UINT. OpenGL counterpart: GL_RG32UI.
        TEX_FORMAT_RG32_UINT, 

        /// Two-component 64-bit signed-integer format with 32-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R32G32_SINT. OpenGL counterpart: GL_RG32I.
        TEX_FORMAT_RG32_SINT, 

        /// Two-component 64-bit typeless format with 32-bits for R channel and 8 bits for G channel. \n
        /// D3D counterpart: DXGI_FORMAT_R32G8X24_TYPELESS. OpenGL does not have direct counterpart, GL_DEPTH32F_STENCIL8 is used.
        TEX_FORMAT_R32G8X24_TYPELESS, 

        /// Two-component 64-bit format with 32-bit floating-point depth channel and 8-bit stencil channel. \n
        /// D3D counterpart: DXGI_FORMAT_D32_FLOAT_S8X24_UINT. OpenGL counterpart: GL_DEPTH32F_STENCIL8.
        TEX_FORMAT_D32_FLOAT_S8X24_UINT, 

        /// Two-component 64-bit format with 32-bit floating-point R channel and 8+24-bits of typeless data. \n
        /// D3D counterpart: DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS. OpenGL does not have direct counterpart, GL_DEPTH32F_STENCIL8 is used.
        TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS, 

        /// Two-component 64-bit format with 32-bit typeless data and 8-bit G channel. \n
        /// D3D counterpart: DXGI_FORMAT_X32_TYPELESS_G8X24_UINT
        /// \warning This format is currently not implemented in OpenGL version
        TEX_FORMAT_X32_TYPELESS_G8X24_UINT, 

        /// Four-component 32-bit typeless format with 10 bits for RGB and 2 bits for alpha channel. \n
        /// D3D counterpart: DXGI_FORMAT_R10G10B10A2_TYPELESS. OpenGL does not have direct counterpart, GL_RGB10_A2 is used.
        TEX_FORMAT_RGB10A2_TYPELESS, 

        /// Four-component 32-bit unsigned-normalized-integer format with 10 bits for each color and 2 bits for alpha channel. \n
        /// D3D counterpart: DXGI_FORMAT_R10G10B10A2_UNORM. OpenGL counterpart: GL_RGB10_A2.
        TEX_FORMAT_RGB10A2_UNORM, 

        /// Four-component 32-bit unsigned-integer format with 10 bits for each color and 2 bits for alpha channel. \n
        /// D3D counterpart: DXGI_FORMAT_R10G10B10A2_UINT. OpenGL counterpart: GL_RGB10_A2UI.
        TEX_FORMAT_RGB10A2_UINT, 

        /// Three-component 32-bit format encoding three partial precision channels using 11 bits for red and green and 10 bits for blue channel. \n
        /// D3D counterpart: DXGI_FORMAT_R11G11B10_FLOAT. OpenGL counterpart: GL_R11F_G11F_B10F.
        TEX_FORMAT_R11G11B10_FLOAT, 

        /// Four-component 32-bit typeless format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8B8A8_TYPELESS. OpenGL does not have direct counterpart, GL_RGBA8 is used.
        TEX_FORMAT_RGBA8_TYPELESS, 

        /// Four-component 32-bit unsigned-normalized-integer format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8B8A8_UNORM. OpenGL counterpart: GL_RGBA8.
        TEX_FORMAT_RGBA8_UNORM,

        /// Four-component 32-bit unsigned-normalized-integer sRGB format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8B8A8_UNORM_SRGB. OpenGL counterpart: GL_SRGB8_ALPHA8.
        TEX_FORMAT_RGBA8_UNORM_SRGB, 

        /// Four-component 32-bit unsigned-integer format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8B8A8_UINT. OpenGL counterpart: GL_RGBA8UI.
        TEX_FORMAT_RGBA8_UINT, 

        /// Four-component 32-bit signed-normalized-integer format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8B8A8_SNORM. OpenGL counterpart: GL_RGBA8_SNORM.
        TEX_FORMAT_RGBA8_SNORM, 

        /// Four-component 32-bit signed-integer format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8B8A8_SINT. OpenGL counterpart: GL_RGBA8I.
        TEX_FORMAT_RGBA8_SINT, 
        
        /// Two-component 32-bit typeless format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16_TYPELESS. OpenGL does not have direct counterpart, GL_RG16F is used.
        TEX_FORMAT_RG16_TYPELESS, 
        
        /// Two-component 32-bit half-precision floating-point format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16_FLOAT. OpenGL counterpart: GL_RG16F.
        TEX_FORMAT_RG16_FLOAT, 
        
        /// Two-component 32-bit unsigned-normalized-integer format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16_UNORM. OpenGL counterpart: GL_RG16. \n
        /// [GL_EXT_texture_norm16]: https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_norm16.txt
        /// OpenGLES: [GL_EXT_texture_norm16][] extension is required
        TEX_FORMAT_RG16_UNORM, 
        
        /// Two-component 32-bit unsigned-integer format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16_UINT. OpenGL counterpart: GL_RG16UI.
        TEX_FORMAT_RG16_UINT, 
        
        /// Two-component 32-bit signed-normalized-integer format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16_SNORM. OpenGL counterpart: GL_RG16_SNORM. \n
        /// [GL_EXT_texture_norm16]: https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_norm16.txt
        /// OpenGLES: [GL_EXT_texture_norm16][] extension is required
        TEX_FORMAT_RG16_SNORM, 
        
        /// Two-component 32-bit signed-integer format with 16-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R16G16_SINT. OpenGL counterpart: GL_RG16I.
        TEX_FORMAT_RG16_SINT, 
        
        /// Single-component 32-bit typeless format. \n
        /// D3D counterpart: DXGI_FORMAT_R32_TYPELESS. OpenGL does not have direct counterpart, GL_R32F is used.
        TEX_FORMAT_R32_TYPELESS, 
        
        /// Single-component 32-bit floating-point depth format. \n
        /// D3D counterpart: DXGI_FORMAT_D32_FLOAT. OpenGL counterpart: GL_DEPTH_COMPONENT32F.
        TEX_FORMAT_D32_FLOAT, 
        
        /// Single-component 32-bit floating-point format. \n
        /// D3D counterpart: DXGI_FORMAT_R32_FLOAT. OpenGL counterpart: GL_R32F.
        TEX_FORMAT_R32_FLOAT, 
        
        /// Single-component 32-bit unsigned-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R32_UINT. OpenGL counterpart: GL_R32UI.
        TEX_FORMAT_R32_UINT, 
        
        /// Single-component 32-bit signed-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R32_SINT. OpenGL counterpart: GL_R32I.
        TEX_FORMAT_R32_SINT, 
        
        /// Two-component 32-bit typeless format with 24 bits for R and 8 bits for G channel. \n
        /// D3D counterpart: DXGI_FORMAT_R24G8_TYPELESS. OpenGL does not have direct counterpart, GL_DEPTH24_STENCIL8 is used.
        TEX_FORMAT_R24G8_TYPELESS, 
        
        /// Two-component 32-bit format with 24 bits for unsigned-normalized-integer depth and 8 bits for stencil. \n
        /// D3D counterpart: DXGI_FORMAT_D24_UNORM_S8_UINT. OpenGL counterpart: GL_DEPTH24_STENCIL8.
        TEX_FORMAT_D24_UNORM_S8_UINT, 
        
        /// Two-component 32-bit format with 24 bits for unsigned-normalized-integer data and 8 bits of unreferenced data. \n
        /// D3D counterpart: DXGI_FORMAT_R24_UNORM_X8_TYPELESS. OpenGL does not have direct counterpart, GL_DEPTH24_STENCIL8 is used.
        TEX_FORMAT_R24_UNORM_X8_TYPELESS, 
        
        /// Two-component 32-bit format with 24 bits of unreferenced data and 8 bits of unsigned-integer data. \n
        /// D3D counterpart: DXGI_FORMAT_X24_TYPELESS_G8_UINT
        /// \warning This format is currently not implemented in OpenGL version
        TEX_FORMAT_X24_TYPELESS_G8_UINT, 
        
        /// Two-component 16-bit typeless format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8_TYPELESS. OpenGL does not have direct counterpart, GL_RG8 is used.
        TEX_FORMAT_RG8_TYPELESS, 
        
        /// Two-component 16-bit unsigned-normalized-integer format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8_UNORM. OpenGL counterpart: GL_RG8.
        TEX_FORMAT_RG8_UNORM, 
        
        /// Two-component 16-bit unsigned-integer format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8_UINT. OpenGL counterpart: GL_RG8UI.
        TEX_FORMAT_RG8_UINT, 
        
        /// Two-component 16-bit signed-normalized-integer format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8_SNORM. OpenGL counterpart: GL_RG8_SNORM.
        TEX_FORMAT_RG8_SNORM, 
        
        /// Two-component 16-bit signed-integer format with 8-bit channels. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8_SINT. OpenGL counterpart: GL_RG8I.
        TEX_FORMAT_RG8_SINT, 
        
        /// Single-component 16-bit typeless format. \n
        /// D3D counterpart: DXGI_FORMAT_R16_TYPELESS. OpenGL does not have direct counterpart, GL_R16F is used.
        TEX_FORMAT_R16_TYPELESS, 
        
        /// Single-component 16-bit half-precisoin floating-point format. \n
        /// D3D counterpart: DXGI_FORMAT_R16_FLOAT. OpenGL counterpart: GL_R16F.
        TEX_FORMAT_R16_FLOAT, 
        
        /// Single-component 16-bit unsigned-normalized-integer depth format. \n
        /// D3D counterpart: DXGI_FORMAT_D16_UNORM. OpenGL counterpart: GL_DEPTH_COMPONENT16.
        TEX_FORMAT_D16_UNORM, 
        
        /// Single-component 16-bit unsigned-normalized-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R16_UNORM. OpenGL counterpart: GL_R16.
        /// [GL_EXT_texture_norm16]: https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_norm16.txt
        /// OpenGLES: [GL_EXT_texture_norm16][] extension is required
        TEX_FORMAT_R16_UNORM, 
        
        /// Single-component 16-bit unsigned-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R16_UINT. OpenGL counterpart: GL_R16UI.
        TEX_FORMAT_R16_UINT, 
        
        /// Single-component 16-bit signed-normalized-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R16_SNORM. OpenGL counterpart: GL_R16_SNORM. \n
        /// [GL_EXT_texture_norm16]: https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_norm16.txt
        /// OpenGLES: [GL_EXT_texture_norm16][] extension is required
        TEX_FORMAT_R16_SNORM, 
        
        /// Single-component 16-bit signed-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R16_SINT. OpenGL counterpart: GL_R16I.
        TEX_FORMAT_R16_SINT, 
        
        /// Single-component 8-bit typeless format. \n
        /// D3D counterpart: DXGI_FORMAT_R8_TYPELESS. OpenGL does not have direct counterpart, GL_R8 is used.
        TEX_FORMAT_R8_TYPELESS, 
        
        /// Single-component 8-bit unsigned-normalized-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R8_UNORM. OpenGL counterpart: GL_R8.
        TEX_FORMAT_R8_UNORM, 
        
        /// Single-component 8-bit unsigned-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R8_UINT. OpenGL counterpart: GL_R8UI.
        TEX_FORMAT_R8_UINT, 
        
        /// Single-component 8-bit signed-normalized-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R8_SNORM. OpenGL counterpart: GL_R8_SNORM.
        TEX_FORMAT_R8_SNORM, 
        
        /// Single-component 8-bit signed-integer format. \n
        /// D3D counterpart: DXGI_FORMAT_R8_SINT. OpenGL counterpart: GL_R8I.
        TEX_FORMAT_R8_SINT, 
        
        /// Single-component 8-bit unsigned-normalized-integer format for alpha only. \n
        /// D3D counterpart: DXGI_FORMAT_A8_UNORM
        /// \warning This format is not availanle in OpenGL
        TEX_FORMAT_A8_UNORM, 
        
        /// Single-component 1-bit format. \n
        /// D3D counterpart: DXGI_FORMAT_R1_UNORM
        /// \warning This format is not availanle in OpenGL
        TEX_FORMAT_R1_UNORM, 
        
        /// Three partial-precision floating pointer numbers sharing single exponent encoded into a 32-bit value. \n
        /// D3D counterpart: DXGI_FORMAT_R9G9B9E5_SHAREDEXP. OpenGL counterpart: GL_RGB9_E5.
        TEX_FORMAT_RGB9E5_SHAREDEXP, 
        
        /// Four-component unsigned-normalized integer format analogous to UYVY encoding. \n
        /// D3D counterpart: DXGI_FORMAT_R8G8_B8G8_UNORM
        /// \warning This format is not availanle in OpenGL
        TEX_FORMAT_RG8_B8G8_UNORM, 
        
        /// Four-component unsigned-normalized integer format analogous to YUY2 encoding. \n
        /// D3D counterpart: DXGI_FORMAT_G8R8_G8B8_UNORM
        /// \warning This format is not availanle in OpenGL
        TEX_FORMAT_G8R8_G8B8_UNORM, 
        
        /// Four-component typeless block-compression format with 1:8 compression ratio.\n
        /// D3D counterpart: DXGI_FORMAT_BC1_TYPELESS. OpenGL does not have direct counterpart, GL_COMPRESSED_RGB_S3TC_DXT1_EXT is used. \n
        /// [GL_EXT_texture_compression_s3tc]: https://www.khronos.org/registry/gles/extensions/EXT/texture_compression_s3tc.txt
        /// OpenGL & OpenGLES: [GL_EXT_texture_compression_s3tc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC1">BC1 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/S3_Texture_Compression#DXT1_Format">DXT1 on OpenGL.org </a>
        TEX_FORMAT_BC1_TYPELESS, 
        
        /// Four-component unsigned-normalized-integer block-compression format with 5 bits for R, 6 bits for G, 5 bits for B, and 0 or 1 bit for A channel. 
        /// The pixel data is encoded using 8 bytes per 4x4 block (4 bits per pixel) providing 1:8 compression ratio against RGBA8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC1_UNORM. OpenGL counterpart: GL_COMPRESSED_RGB_S3TC_DXT1_EXT.\n
        /// [GL_EXT_texture_compression_s3tc]: https://www.khronos.org/registry/gles/extensions/EXT/texture_compression_s3tc.txt
        /// OpenGL & OpenGLES: [GL_EXT_texture_compression_s3tc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC1">BC1 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/S3_Texture_Compression#DXT1_Format">DXT1 on OpenGL.org </a>
        TEX_FORMAT_BC1_UNORM,
        
        /// Four-component unsigned-normalized-integer block-compression sRGB format with 5 bits for R, 6 bits for G, 5 bits for B, and 0 or 1 bit for A channel. \n
        /// The pixel data is encoded using 8 bytes per 4x4 block (4 bits per pixel) providing 1:8 compression ratio against RGBA8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC1_UNORM_SRGB. OpenGL counterpart: GL_COMPRESSED_SRGB_S3TC_DXT1_EXT.\n
        /// [GL_EXT_texture_compression_s3tc]: https://www.khronos.org/registry/gles/extensions/EXT/texture_compression_s3tc.txt
        /// OpenGL & OpenGLES: [GL_EXT_texture_compression_s3tc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC1">BC1 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/S3_Texture_Compression#DXT1_Format">DXT1 on OpenGL.org </a>
        TEX_FORMAT_BC1_UNORM_SRGB,
        
        /// Four component typeless block-compression format with 1:4 compression ratio.\n
        /// D3D counterpart: DXGI_FORMAT_BC2_TYPELESS. OpenGL does not have direct counterpart, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT is used. \n 
        /// [GL_EXT_texture_compression_s3tc]: https://www.khronos.org/registry/gles/extensions/EXT/texture_compression_s3tc.txt
        /// OpenGL & OpenGLES: [GL_EXT_texture_compression_s3tc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC2">BC2 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/S3_Texture_Compression#DXT3_Format">DXT3 on OpenGL.org </a>
        TEX_FORMAT_BC2_TYPELESS, 
        
        /// Four-component unsigned-normalized-integer block-compression format with 5 bits for R, 6 bits for G, 5 bits for B, and 4 bits for low-coherent separate A channel.
        /// The pixel data is encoded using 16 bytes per 4x4 block (8 bits per pixel) providing 1:4 compression ratio against RGBA8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC2_UNORM. OpenGL counterpart: GL_COMPRESSED_RGBA_S3TC_DXT3_EXT. \n
        /// [GL_EXT_texture_compression_s3tc]: https://www.khronos.org/registry/gles/extensions/EXT/texture_compression_s3tc.txt
        /// OpenGL & OpenGLES: [GL_EXT_texture_compression_s3tc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC2">BC2 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/S3_Texture_Compression#DXT3_Format">DXT3 on OpenGL.org </a>
        TEX_FORMAT_BC2_UNORM,
        
        /// Four-component signed-normalized-integer block-compression sRGB format with 5 bits for R, 6 bits for G, 5 bits for B, and 4 bits for low-coherent separate A channel.
        /// The pixel data is encoded using 16 bytes per 4x4 block (8 bits per pixel) providing 1:4 compression ratio against RGBA8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC2_UNORM_SRGB. OpenGL counterpart: GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT. \n
        /// [GL_EXT_texture_compression_s3tc]: https://www.khronos.org/registry/gles/extensions/EXT/texture_compression_s3tc.txt
        /// OpenGL & OpenGLES: [GL_EXT_texture_compression_s3tc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC2">BC2 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/S3_Texture_Compression#DXT3_Format">DXT3 on OpenGL.org </a>
        TEX_FORMAT_BC2_UNORM_SRGB,
        
        /// Four-component typeless block-compression format with 1:4 compression ratio.\n
        /// D3D counterpart: DXGI_FORMAT_BC3_TYPELESS. OpenGL does not have direct counterpart, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT is used. \n
        /// [GL_EXT_texture_compression_s3tc]: https://www.khronos.org/registry/gles/extensions/EXT/texture_compression_s3tc.txt
        /// OpenGL & OpenGLES: [GL_EXT_texture_compression_s3tc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC3">BC3 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/S3_Texture_Compression#DXT5_Format">DXT5 on OpenGL.org </a>
        TEX_FORMAT_BC3_TYPELESS, 
        
        /// Four-component unsigned-normalized-integer block-compression format with 5 bits for R, 6 bits for G, 5 bits for B, and 8 bits for highly-coherent A channel.
        /// The pixel data is encoded using 16 bytes per 4x4 block (8 bits per pixel) providing 1:4 compression ratio against RGBA8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC3_UNORM. OpenGL counterpart: GL_COMPRESSED_RGBA_S3TC_DXT5_EXT. \n
        /// [GL_EXT_texture_compression_s3tc]: https://www.khronos.org/registry/gles/extensions/EXT/texture_compression_s3tc.txt
        /// OpenGL & OpenGLES: [GL_EXT_texture_compression_s3tc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC3">BC3 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/S3_Texture_Compression#DXT5_Format">DXT5 on OpenGL.org </a>
        TEX_FORMAT_BC3_UNORM,
        
        /// Four-component unsigned-normalized-integer block-compression sRGB format with 5 bits for R, 6 bits for G, 5 bits for B, and 8 bits for highly-coherent A channel.
        /// The pixel data is encoded using 16 bytes per 4x4 block (8 bits per pixel) providing 1:4 compression ratio against RGBA8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC3_UNORM_SRGB. OpenGL counterpart: GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT. \n
        /// [GL_EXT_texture_compression_s3tc]: https://www.khronos.org/registry/gles/extensions/EXT/texture_compression_s3tc.txt
        /// OpenGL & OpenGLES: [GL_EXT_texture_compression_s3tc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC3">BC3 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/S3_Texture_Compression#DXT5_Format">DXT5 on OpenGL.org </a>
        TEX_FORMAT_BC3_UNORM_SRGB,
        
        /// One-component typeless block-compression format with 1:2 compression ratio. \n
        /// D3D counterpart: DXGI_FORMAT_BC4_TYPELESS. OpenGL does not have direct counterpart, GL_COMPRESSED_RED_RGTC1 is used. \n
        /// [GL_ARB_texture_compression_rgtc]: https://www.opengl.org/registry/specs/ARB/texture_compression_rgtc.txt
        /// OpenGL & OpenGLES: [GL_ARB_texture_compression_rgtc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC4">BC4 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/Image_Format#Compressed_formats">Compressed formats on OpenGL.org </a>
        TEX_FORMAT_BC4_TYPELESS, 
        
        /// One-component unsigned-normalized-integer block-compression format with 8 bits for R channel.
        /// The pixel data is encoded using 8 bytes per 4x4 block (4 bits per pixel) providing 1:2 compression ratio against R8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC4_UNORM. OpenGL counterpart: GL_COMPRESSED_RED_RGTC1. \n
        /// [GL_ARB_texture_compression_rgtc]: https://www.opengl.org/registry/specs/ARB/texture_compression_rgtc.txt
        /// OpenGL & OpenGLES: [GL_ARB_texture_compression_rgtc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC4">BC4 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/Image_Format#Compressed_formats">Compressed formats on OpenGL.org </a>
        TEX_FORMAT_BC4_UNORM,
        
        /// One-component signed-normalized-integer block-compression format with 8 bits for R channel.
        /// The pixel data is encoded using 8 bytes per 4x4 block (4 bits per pixel) providing 1:2 compression ratio against R8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC4_SNORM. OpenGL counterpart: GL_COMPRESSED_SIGNED_RED_RGTC1. \n
        /// [GL_ARB_texture_compression_rgtc]: https://www.opengl.org/registry/specs/ARB/texture_compression_rgtc.txt
        /// OpenGL & OpenGLES: [GL_ARB_texture_compression_rgtc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC4">BC4 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/Image_Format#Compressed_formats">Compressed formats on OpenGL.org </a>
        TEX_FORMAT_BC4_SNORM,
        
        /// Two-component typeless block-compression format with 1:2 compression ratio. \n
        /// D3D counterpart: DXGI_FORMAT_BC5_TYPELESS. OpenGL does not have direct counterpart, GL_COMPRESSED_RG_RGTC2 is used. \n
        /// [GL_ARB_texture_compression_rgtc]: https://www.opengl.org/registry/specs/ARB/texture_compression_rgtc.txt
        /// OpenGL & OpenGLES: [GL_ARB_texture_compression_rgtc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC5">BC5 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/Image_Format#Compressed_formats">Compressed formats on OpenGL.org </a>
        TEX_FORMAT_BC5_TYPELESS,
        
        /// Two-component unsigned-normalized-integer block-compression format with 8 bits for R and 8 bits for G channel.
        /// The pixel data is encoded using 16 bytes per 4x4 block (8 bits per pixel) providing 1:2 compression ratio against RG8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC5_UNORM. OpenGL counterpart: GL_COMPRESSED_RG_RGTC2. \n
        /// [GL_ARB_texture_compression_rgtc]: https://www.opengl.org/registry/specs/ARB/texture_compression_rgtc.txt
        /// OpenGL & OpenGLES: [GL_ARB_texture_compression_rgtc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC5">BC5 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/Image_Format#Compressed_formats">Compressed formats on OpenGL.org </a>
        TEX_FORMAT_BC5_UNORM,
        
        /// Two-component signed-normalized-integer block-compression format with 8 bits for R and 8 bits for G channel.
        /// The pixel data is encoded using 16 bytes per 4x4 block (8 bits per pixel) providing 1:2 compression ratio against RG8 format. \n
        /// D3D counterpart: DXGI_FORMAT_BC5_SNORM. OpenGL counterpart: GL_COMPRESSED_SIGNED_RG_RGTC2. \n
        /// [GL_ARB_texture_compression_rgtc]: https://www.opengl.org/registry/specs/ARB/texture_compression_rgtc.txt
        /// OpenGL & OpenGLES: [GL_ARB_texture_compression_rgtc][] extension is required
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb694531(v=vs.85).aspx#BC5">BC5 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/Image_Format#Compressed_formats">Compressed formats on OpenGL.org </a>
        TEX_FORMAT_BC5_SNORM,
        
        /// Three-component 16-bit unsigned-normalized-integer format with 5 bits for blue, 6 bits for green, and 5 bits for red channel. \n
        /// D3D counterpart: DXGI_FORMAT_B5G6R5_UNORM
        /// \warning This format is not available until D3D11.1 and Windows 8. It is also not available in OpenGL
        TEX_FORMAT_B5G6R5_UNORM,
        
        /// Four-component 16-bit unsigned-normalized-integer format with 5 bits for each color channel and 1-bit alpha. \n
        /// D3D counterpart: DXGI_FORMAT_B5G5R5A1_UNORM
        /// \warning This format is not available until D3D11.1 and Windows 8. It is also not available in OpenGL
        TEX_FORMAT_B5G5R5A1_UNORM, 
        
        /// Four-component 32-bit unsigned-normalized-integer format with 8 bits for each channel. \n
        /// D3D counterpart: DXGI_FORMAT_B8G8R8A8_UNORM.
        /// \warning This format is not available in OpenGL
        TEX_FORMAT_BGRA8_UNORM,
        
        /// Four-component 32-bit unsigned-normalized-integer format with 8 bits for each color channel and 8 bits unused. \n
        /// D3D counterpart: DXGI_FORMAT_B8G8R8X8_UNORM.
        /// \warning This format is not available in OpenGL
        TEX_FORMAT_BGRX8_UNORM,
        
        /// Four-component 32-bit 2.8-biased fixed-point format with 10 bits for each color channel and 2-bit alpha. \n
        /// D3D counterpart: DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM.
        /// \warning This format is not available in OpenGL
        TEX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
        
        /// Four-component 32-bit typeless format with 8 bits for each channel. \n
        /// D3D counterpart: DXGI_FORMAT_B8G8R8A8_TYPELESS.
        /// \warning This format is not available in OpenGL
        TEX_FORMAT_BGRA8_TYPELESS,
        
        /// Four-component 32-bit unsigned-normalized sRGB format with 8 bits for each channel. \n
        /// D3D counterpart: DXGI_FORMAT_B8G8R8A8_UNORM_SRGB.
        /// \warning This format is not available in OpenGL.
        TEX_FORMAT_BGRA8_UNORM_SRGB,
        
        /// Four-component 32-bit typeless format that with 8 bits for each color channel, and 8 bits are unused. \n
        /// D3D counterpart: DXGI_FORMAT_B8G8R8X8_TYPELESS.
        /// \warning This format is not available in OpenGL.
        TEX_FORMAT_BGRX8_TYPELESS,
        
        /// Four-component 32-bit unsigned-normalized sRGB format with 8 bits for each color channel, and 8 bits are unused. \n
        /// D3D counterpart: DXGI_FORMAT_B8G8R8X8_UNORM_SRGB.
        /// \warning This format is not available in OpenGL.
        TEX_FORMAT_BGRX8_UNORM_SRGB,
        
        /// Three-component typeless block-compression format. \n
        /// D3D counterpart: DXGI_FORMAT_BC6H_TYPELESS. OpenGL does not have direct counterpart, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT is used. \n
        /// [GL_ARB_texture_compression_bptc]: https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/specs/ARB/texture_compression_bptc.txt
        /// OpenGL: [GL_ARB_texture_compression_bptc][] extension is required. Not supported in at least OpenGLES3.1
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/hh308952(v=vs.85).aspx">BC6H on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/BPTC_Texture_Compression">BPTC Texture Compression on OpenGL.org </a>
        TEX_FORMAT_BC6H_TYPELESS,
        
        /// Three-component unsigned half-precision floating-point format with 16 bits for each channel. \n
        /// D3D counterpart: DXGI_FORMAT_BC6H_UF16. OpenGL counterpart: GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT. \n
        /// [GL_ARB_texture_compression_bptc]: https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/specs/ARB/texture_compression_bptc.txt
        /// OpenGL: [GL_ARB_texture_compression_bptc][] extension is required. Not supported in at least OpenGLES3.1
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/hh308952(v=vs.85).aspx">BC6H on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/BPTC_Texture_Compression">BPTC Texture Compression on OpenGL.org </a>
        TEX_FORMAT_BC6H_UF16,
        
        /// Three-channel signed half-precision floating-point format with 16 bits per each channel. \n
        /// D3D counterpart: DXGI_FORMAT_BC6H_SF16. OpenGL counterpart: GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT. \n
        /// [GL_ARB_texture_compression_bptc]: https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/specs/ARB/texture_compression_bptc.txt
        /// OpenGL: [GL_ARB_texture_compression_bptc][] extension is required. Not supported in at least OpenGLES3.1
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/hh308952(v=vs.85).aspx">BC6H on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/BPTC_Texture_Compression">BPTC Texture Compression on OpenGL.org </a>
        TEX_FORMAT_BC6H_SF16,
        
        /// Three-component typeless block-compression format. \n
        /// D3D counterpart: DXGI_FORMAT_BC7_TYPELESS. OpenGL does not have direct counterpart, GL_COMPRESSED_RGBA_BPTC_UNORM is used. \n
        /// [GL_ARB_texture_compression_bptc]: https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/specs/ARB/texture_compression_bptc.txt
        /// OpenGL: [GL_ARB_texture_compression_bptc][] extension is required. Not supported in at least OpenGLES3.1
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/hh308953(v=vs.85).aspx">BC7 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/BPTC_Texture_Compression">BPTC Texture Compression on OpenGL.org </a>
        TEX_FORMAT_BC7_TYPELESS,
        
        /// Three-component block-compression unsigned-normalized-integer format with 4 to 7 bits per color channel and 0 to 8 bits of alpha. \n
        /// D3D counterpart: DXGI_FORMAT_BC7_UNORM. OpenGL counterpart: GL_COMPRESSED_RGBA_BPTC_UNORM. \n
        /// [GL_ARB_texture_compression_bptc]: https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/specs/ARB/texture_compression_bptc.txt
        /// OpenGL: [GL_ARB_texture_compression_bptc][] extension is required. Not supported in at least OpenGLES3.1
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/hh308953(v=vs.85).aspx">BC7 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/BPTC_Texture_Compression">BPTC Texture Compression on OpenGL.org </a>
        TEX_FORMAT_BC7_UNORM,
        
        /// Three-component block-compression unsigned-normalized-integer sRGB format with 4 to 7 bits per color channel and 0 to 8 bits of alpha. \n
        /// D3D counterpart: DXGI_FORMAT_BC7_UNORM_SRGB. OpenGL counterpart: GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM. \n
        /// [GL_ARB_texture_compression_bptc]: https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/specs/ARB/texture_compression_bptc.txt
        /// OpenGL: [GL_ARB_texture_compression_bptc][] extension is required. Not supported in at least OpenGLES3.1
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/hh308953(v=vs.85).aspx">BC7 on MSDN </a>, 
        ///     <a href = "https://www.opengl.org/wiki/BPTC_Texture_Compression">BPTC Texture Compression on OpenGL.org </a>
        TEX_FORMAT_BC7_UNORM_SRGB,
        
        /// Helper member containing the total number of texture formats in the enumeration
        TEX_FORMAT_NUM_FORMATS
    };

    /// Filter type

    /// This enumeration defines filter type. It is used by SamplerDesc structure to define min, mag and mip filters.
    /// \note On D3D11, comparison filters only work with textures that have the following formats: 
    /// R32_FLOAT_X8X24_TYPELESS, R32_FLOAT, R24_UNORM_X8_TYPELESS, R16_UNORM.
    enum FILTER_TYPE : Int32
    {
        FILTER_TYPE_UNKNOWN  = 0,           ///< Unknown filter type
        FILTER_TYPE_POINT,                  ///< Point filtering
	    FILTER_TYPE_LINEAR,                 ///< Linear filtering
	    FILTER_TYPE_ANISOTROPIC,            ///< Anisotropic filtering
        FILTER_TYPE_COMPARISON_POINT,       ///< Comparison-point filtering
        FILTER_TYPE_COMPARISON_LINEAR,      ///< Comparison-linear filtering
        FILTER_TYPE_COMPARISON_ANISOTROPIC, ///< Comparison-anisotropic filtering
        FILTER_TYPE_MINIMUM_POINT,          ///< Minimum-point filtering (DX12 only)
        FILTER_TYPE_MINIMUM_LINEAR,         ///< Minimum-linear filtering (DX12 only)
        FILTER_TYPE_MINIMUM_ANISOTROPIC,    ///< Minimum-anisotropic filtering (DX12 only)
        FILTER_TYPE_MAXIMUM_POINT,          ///< Maximum-point filtering (DX12 only)
        FILTER_TYPE_MAXIMUM_LINEAR,         ///< Maximum-linear filtering (DX12 only)
        FILTER_TYPE_MAXIMUM_ANISOTROPIC,    ///< Maximum-anisotropic filtering (DX12 only)
        FILTER_TYPE_NUM_FILTERS             ///< Helper value that stores the total number of filter types in the enumeration
    };

    /// Texture address mode

    /// [D3D11_TEXTURE_ADDRESS_MODE]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476256(v=vs.85).aspx
    /// [D3D12_TEXTURE_ADDRESS_MODE]: https://msdn.microsoft.com/en-us/library/windows/desktop/dn770441(v=vs.85).aspx
    /// Defines a technique for resolving texture coordinates that are outside of 
    /// the boundaries of a texture. The enumeration generally mirrors [D3D11_TEXTURE_ADDRESS_MODE][]/[D3D12_TEXTURE_ADDRESS_MODE][] enumeration. 
    /// It is used by SamplerDesc structure to define the address mode for U,V and W texture coordinates.
    enum TEXTURE_ADDRESS_MODE : Int32
    {
        /// Unknown mode
        TEXTURE_ADDRESS_UNKNOWN = 0,

        /// Tile the texture at every integer junction. \n
        /// Direct3D Counterpart: D3D11_TEXTURE_ADDRESS_WRAP/D3D12_TEXTURE_ADDRESS_MODE_WRAP. OpenGL counterpart: GL_REPEAT
        TEXTURE_ADDRESS_WRAP	= 1,

        /// Flip the texture at every integer junction. \n
        /// Direct3D Counterpart: D3D11_TEXTURE_ADDRESS_MIRROR/D3D12_TEXTURE_ADDRESS_MODE_MIRROR. OpenGL counterpart: GL_MIRRORED_REPEAT
	    TEXTURE_ADDRESS_MIRROR	= 2,

        /// Texture coordinates outside the range [0.0, 1.0] are set to the 
        /// texture color at 0.0 or 1.0, respectively. \n
        /// Direct3D Counterpart: D3D11_TEXTURE_ADDRESS_CLAMP/D3D12_TEXTURE_ADDRESS_MODE_CLAMP. OpenGL counterpart: GL_CLAMP_TO_EDGE
	    TEXTURE_ADDRESS_CLAMP	= 3,

        /// Texture coordinates outside the range [0.0, 1.0] are set to the border color specified
        /// specified in SamplerDesc structure. \n
        /// Direct3D Counterpart: D3D11_TEXTURE_ADDRESS_BORDER/D3D12_TEXTURE_ADDRESS_MODE_BORDER. OpenGL counterpart: GL_CLAMP_TO_BORDER
	    TEXTURE_ADDRESS_BORDER	= 4,

        /// Similar to TEXTURE_ADDRESS_MIRROR and TEXTURE_ADDRESS_CLAMP. Takes the absolute 
        /// value of the texture coordinate (thus, mirroring around 0), and then clamps to 
        /// the maximum value. \n
        /// Direct3D Counterpart: D3D11_TEXTURE_ADDRESS_MIRROR_ONCE/D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE. OpenGL counterpart: GL_MIRROR_CLAMP_TO_EDGE
        /// \note GL_MIRROR_CLAMP_TO_EDGE is only available in OpenGL4.4+, and is not available until at least OpenGLES3.1
	    TEXTURE_ADDRESS_MIRROR_ONCE	= 5,

        /// Helper value that stores the total number of texture address modes in the enumeration
        TEXTURE_ADDRESS_NUM_MODES
    };

    /// Comparison function

    /// [D3D11_COMPARISON_FUNC]: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476101(v=vs.85).aspx
    /// [D3D12_COMPARISON_FUNC]: https://msdn.microsoft.com/en-us/library/windows/desktop/dn770349(v=vs.85).aspx
    /// This enumeartion defines a comparison function. It generally mirrors [D3D11_COMPARISON_FUNC]/[D3D12_COMPARISON_FUNC] enum and is used by
    /// - SamplerDesc to define a comparison function if one of the comparison mode filters is used
    /// - StencilOpDesc to define a stencil function
    /// - DepthStencilStateDesc to define a depth function
    enum COMPARISON_FUNCTION : Int32
    {
        /// Unknown comparison function
        COMPARISON_FUNC_UNKNOWN = 0,

        /// Comparison never passes. \n
        /// Direct3D counterpart: D3D11_COMPARISON_NEVER/D3D12_COMPARISON_FUNC_NEVER. OpenGL counterpart: GL_NEVER.
        COMPARISON_FUNC_NEVER,

        /// Comparison passes if the source data is less than the destination data.\n
        /// Direct3D counterpart: D3D11_COMPARISON_LESS/D3D12_COMPARISON_FUNC_LESS. OpenGL counterpart: GL_LESS.
	    COMPARISON_FUNC_LESS,

        /// Comparison passes if the source data is equal to the destination data.\n
        /// Direct3D counterpart: D3D11_COMPARISON_EQUAL/D3D12_COMPARISON_FUNC_EQUAL. OpenGL counterpart: GL_EQUAL.
	    COMPARISON_FUNC_EQUAL,

        /// Comparison passes if the source data is less than or equal to the destination data.\n
        /// Direct3D counterpart: D3D11_COMPARISON_LESS_EQUAL/D3D12_COMPARISON_FUNC_LESS_EQUAL. OpenGL counterpart: GL_LEQUAL.
	    COMPARISON_FUNC_LESS_EQUAL,

        /// Comparison passes if the source data is greater than the destination data.\n
        /// Direct3D counterpart: 3D11_COMPARISON_GREATER/D3D12_COMPARISON_FUNC_GREATER. OpenGL counterpart: GL_GREATER.
	    COMPARISON_FUNC_GREATER,

        /// Comparison passes if the source data is not equal to the destination data.\n
        /// Direct3D counterpart: D3D11_COMPARISON_NOT_EQUAL/D3D12_COMPARISON_FUNC_NOT_EQUAL. OpenGL counterpart: GL_NOTEQUAL.
	    COMPARISON_FUNC_NOT_EQUAL,
        
        /// Comparison passes if the source data is greater than or equal to the destination data.\n
        /// Direct3D counterpart: D3D11_COMPARISON_GREATER_EQUAL/D3D12_COMPARISON_FUNC_GREATER_EQUAL. OpenGL counterpart: GL_GEQUAL.
	    COMPARISON_FUNC_GREATER_EQUAL,
	    
        /// Comparison always passes. \n
        /// Direct3D counterpart: D3D11_COMPARISON_ALWAYS/D3D12_COMPARISON_FUNC_ALWAYS. OpenGL counterpart: GL_ALWAYS.
        COMPARISON_FUNC_ALWAYS,

        /// Helper value that stores the total number of comparison functions in the enumeration
        COMPARISON_FUNC_NUM_FUNCTIONS
    };

    /// Miscellaneous texture flags

    /// The enumeration is used by TextureDesc to describe misc texture flags
    enum MISC_TEXTURE_FLAG
    {
        /// Allow automatic mipmap generation with ITextureView::GenerateMips()

        /// \note A texture must be created with BIND_RENDER_TARGET bind flag
        MISC_TEXTURE_FLAG_GENERATE_MIPS = 0x01
    };

    /// Describes common device object attributes
    struct DeviceObjectAttribs
    {
        /// Object name
        const Char* Name;

        /// Constructor intializes the structure members with default values
        DeviceObjectAttribs() : 
            Name(nullptr)
        {}
    };

    /// Hardware adapter attributes
    struct HardwareAdapterAttribs
    {
        /// A string that contains the adapter description
        char Description[128];

        /// Dedicated video memory, in bytes
        size_t DedicatedVideoMemory;

        /// Dedicated system memory, in bytes
        size_t DedicatedSystemMemory;

        /// Dedicated shared memory, in bytes
        size_t SharedSystemMemory;

        /// The PCI ID of the hardware vendor
        Uint32 VendorId;

        /// The PCI ID of the hardware device
        Uint32 DeviceId;

        /// Number of outputs this device has
        Uint32 NumOutputs;
    };


    /// Display mode attributes
    struct DisplayModeAttribs
    {
        /// Flags indicating how an image is stretched to fit a given monitor's resolution.
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb173066(v=vs.85).aspx">DXGI_MODE_SCALING enumeration on MSDN</a>, 
        enum SCALING
        {
            /// Unspecified scaling.
            /// D3D Counterpart: DXGI_MODE_SCALING_UNSPECIFIED.
            SCALING_UNSPECIFIED = 0,

            /// Specifies no scaling. The image is centered on the display. 
            /// This flag is typically used for a fixed-dot-pitch display (such as an LED display).
            /// D3D Counterpart: DXGI_MODE_SCALING_CENTERED.
            SCALING_CENTERED = 1,

            /// Specifies stretched scaling.
            /// D3D Counterpart: DXGI_MODE_SCALING_STRETCHED.
            SCALING_STRETCHED = 2
        };

        /// Flags indicating the method the raster uses to create an image on a surface.
        /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/bb173067">DXGI_MODE_SCANLINE_ORDER enumeration on MSDN</a>, 
        enum SCANLINE_ORDER
        {
            /// Scanline order is unspecified
            /// D3D Counterpart: DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED.
            SCANLINE_ORDER_UNSPECIFIED = 0,

            /// The image is created from the first scanline to the last without skipping any
            /// D3D Counterpart: DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE.
            SCANLINE_ORDER_PROGRESSIVE = 1,

            /// The image is created beginning with the upper field
            /// D3D Counterpart: DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST.
            SCANLINE_ORDER_UPPER_FIELD_FIRST = 2,

            /// The image is created beginning with the lower field
            /// D3D Counterpart: DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST.
            SCANLINE_ORDER_LOWER_FIELD_FIRST = 3
        };
        
        /// Display resolution width
        Uint32 Width = 0;

        /// Display resolution height
        Uint32 Height = 0;

        /// Display format
        TEXTURE_FORMAT Format = TEX_FORMAT_UNKNOWN;

        /// Refresh rate numerator
        Uint32 RefreshRateNumerator = 0;

        /// Refresh rate denominator
        Uint32 RefreshRateDenominator = 0;

        /// The scanline drawing mode. 
        SCALING Scaling = SCALING_UNSPECIFIED;

        /// The scaling mode. 
        SCANLINE_ORDER ScanlineOrder = SCANLINE_ORDER_UNSPECIFIED;
    };

    /// Swap chain description
    struct SwapChainDesc
    {
        /// The swap chain width. Default value is 0
        Uint32 Width;

        /// The swap chain height. Default value is 0
        Uint32 Height;
        
        /// Back buffer format. Default value is TEX_FORMAT_RGBA8_UNORM_SRGB
        TEXTURE_FORMAT ColorBufferFormat;
        
        /// Depth buffer format. Default value is TEX_FORMAT_D32_FLOAT
        TEXTURE_FORMAT DepthBufferFormat;

        /// Sample count. Default value is 1
        Uint32 SamplesCount;

        /// Number of buffers int the swap chain
        Uint32 BufferCount;

        /// Default depth value, which is used as optimized depth clear value in D3D12
        Float32 DefaultDepthValue;

        /// Default stencil value, which is used as optimized stencil clear value in D3D12
        Uint8 DefaultStencilValue;

        /// Constructor intializes the structure members with default values
        SwapChainDesc() :
            Width(0),
            Height(0),
            ColorBufferFormat( TEX_FORMAT_RGBA8_UNORM_SRGB ),
            DepthBufferFormat( TEX_FORMAT_D32_FLOAT ),
            SamplesCount( 1 ),
            BufferCount( 2 ),
            DefaultDepthValue(1.f),
            DefaultStencilValue(0)
        {}
    };

    /// Full screen mode description
    /// \sa <a href = "https://msdn.microsoft.com/en-us/library/windows/desktop/hh404531(v=vs.85).aspx">DXGI_SWAP_CHAIN_FULLSCREEN_DESC structure on MSDN</a>, 
    struct FullScreenModeDesc
    {
        /// A Boolean value that specifies whether the swap chain is in fullscreen mode.
        Bool Fullscreen = False;

        /// Refresh rate numerator
        Uint32 RefreshRateNumerator = 0;

        /// Refresh rate denominator
        Uint32 RefreshRateDenominator = 0;

        /// The scanline drawing mode. 
        DisplayModeAttribs::SCALING Scaling = DisplayModeAttribs::SCALING_UNSPECIFIED;

        /// The scaling mode. 
        DisplayModeAttribs::SCANLINE_ORDER ScanlineOrder = DisplayModeAttribs::SCANLINE_ORDER_UNSPECIFIED;
    };

    /// Engine creation attibutes
    struct EngineCreationAttribs
    {
        /// Pointer to the raw memory allocator that will be used for all memory allocation/deallocation
        /// operations in the engine
        class IMemoryAllocator *pRawMemAllocator = nullptr;

        /// Pointer to the user-specified debug message callback function
        DebugMessageCallbackType DebugMessageCallback = nullptr;
    };

    /// Attributes specific to D3D12 engine
    struct EngineD3D12Attribs : public EngineCreationAttribs
    {
        static constexpr Uint32 DefaultAdapterId = 0xFFFFFFFF;

        /// Id of the hardware adapter the engine should be initialized on
        Uint32 AdapterId = DefaultAdapterId;

        /// Size of the CPU descriptor heap allocations for different heap types.
        Uint32 CPUDescriptorHeapAllocationSize[4] = 
        {
            1024,  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            256,   // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
            128,   // D3D12_DESCRIPTOR_HEAP_TYPE_RTV
            64     // D3D12_DESCRIPTOR_HEAP_TYPE_DSV
        };

        /// Size of the GPU descriptor heap allocations for different heap types.
        Uint32 GPUDescriptorHeapSize[2] = 
        {
            16384,  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            1024    // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
        };

        /// Size of the dynamic GPU descriptor heap region for different heap types.
        Uint32 GPUDescriptorHeapDynamicSize[2] = 
        {
            4096,  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            1024   // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
        };

		/// This is the size of the chunk that dynamic descriptor allocations manager
		/// request from the main GPU descriptor heap.
		Uint32 DynamicDescriptorAllocationChunkSize[2] = 
		{
			128,  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			16    // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
		};

        /// Number of commands to flush the command list. Only draw/dispatch commands count
        /// towards the limit. Command lists are only flushed when pipeline state is changed
        /// or when backbuffer is presented.
        Uint32 NumCommandsToFlushCmdList = 256;
    };

    /// Box
    struct Box
    {
        Uint32 MinX; ///< Minimal X coordinate. Default value is 0
        Uint32 MaxX; ///< Maximal X coordinate. Default value is 0
        Uint32 MinY; ///< Minimal Y coordinate. Default value is 0
        Uint32 MaxY; ///< Maximal Y coordinate. Default value is 0
        Uint32 MinZ; ///< Minimal Z coordinate. Default value is 0
        Uint32 MaxZ; ///< Maximal Z coordinate. Default value is 1

        /// Constructor intializes the structure
        Box(Uint32 _MinX = 0, Uint32 _MaxX = 0,
            Uint32 _MinY = 0, Uint32 _MaxY = 0,
            Uint32 _MinZ = 0, Uint32 _MaxZ = 1) : 
            MinX(_MinX), MaxX(_MaxX),
            MinY(_MinY), MaxY(_MaxY),
            MinZ(_MinZ), MaxZ(_MaxZ)
        {}
    };


    /// Describes texture format component type
    enum COMPONENT_TYPE : Int32
    {
        /// Undefined component type
        COMPONENT_TYPE_UNDEFINED,

        /// Floating point component type
        COMPONENT_TYPE_FLOAT,

        /// Signed-normalized-integer component type
        COMPONENT_TYPE_SNORM,

        /// Unsigned-normalized-integer component type
        COMPONENT_TYPE_UNORM,

        /// Unsigned-normalized-integer sRGB component type
        COMPONENT_TYPE_UNORM_SRGB,

        /// Signed-integer component type
        COMPONENT_TYPE_SINT,

        /// Unsigned-integer component type
        COMPONENT_TYPE_UINT,

        /// Depth component type
        COMPONENT_TYPE_DEPTH,

        /// Depth-stencil component type
        COMPONENT_TYPE_DEPTH_STENCIL,

        /// Compound component type (example texture formats: TEX_FORMAT_R11G11B10_FLOAT or TEX_FORMAT_RGB9E5_SHAREDEXP)
        COMPONENT_TYPE_COMPOUND,

        /// Compressed component type
        COMPONENT_TYPE_COMPRESSED,
    };

    /// Describes invariant texture format attributes. These attributes are 
    /// intrinsic to the texture format itself and do not depend on the 
    /// format support.
    struct TextureFormatAttribs
    {
        /// Texture format, see Diligent::TEXTURE_FORMAT for a list of supported texture formats
        TEXTURE_FORMAT Format;

        /// Literal texture format name (for instance, for TEX_FORMAT_RGBA8_UNORM format, this
        /// will be "TEX_FORMAT_RGBA8_UNORM")
        const Char *Name;

        /// Size of one component in bytes (for instance, for TEX_FORMAT_RGBA8_UNORM format, this will be 1)
        /// For compressed formats, this is the block size in bytes (for TEX_FORMAT_BC1_UNORM format, this will be 8)
        Uint32 ComponentSize;

        /// Number of components
        Uint32 NumComponents;

        /// Component type, see Diligent::COMPONENT_TYPE for details.
        COMPONENT_TYPE ComponentType;    

        /// Initializes the structure
        explicit TextureFormatAttribs( TEXTURE_FORMAT _Format = TEX_FORMAT_UNKNOWN, 
                                        const Char *_Name = "TEX_FORMAT_UNKNOWN",
                                        Uint32 _ComponentSize = 0,
                                        Uint32 _NumComponents = 0,
                                        COMPONENT_TYPE _ComponentType = COMPONENT_TYPE_UNDEFINED ) : 
            Format(_Format),
            Name(_Name),
            ComponentSize(_ComponentSize),
            NumComponents(_NumComponents),
            ComponentType(_ComponentType)
        {}
    };

    /// Basic texture format description

    /// This structure is returned by IRenderDevice::GetTextureFormatInfo()
    struct TextureFormatInfo : public TextureFormatAttribs
    {
        /// Indicates if the format is supported by the device
        bool Supported;

        /// Initializes the structure with default values
        explicit TextureFormatInfo() :
            Supported(false)
        {}
    };

    /// Extended texture format description

    /// This structure is returned by IRenderDevice::GetTextureFormatInfoExt()
    struct TextureFormatInfoExt : TextureFormatInfo
    {
        /// Indicates if the format can be filtered
        bool Filterable;

        /// Indicates if the format can be used as a render target format
        bool ColorRenderable;

        /// Indicates if the format can be used as a depth format
        bool DepthRenderable;

        /// Indicates if the format can be used to create a 1D texture 
        bool Tex1DFmt;

        /// Indicates if the format can be used to create a 2D texture 
        bool Tex2DFmt;

        /// Indicates if the format can be used to create a 3D texture 
        bool Tex3DFmt;

        /// Indicates if the format can be used to create a cube texture 
        bool TexCubeFmt;

        /// Indicates if the format can be used to create a multisampled 2D texture 
        bool SupportsMS;

        /// Initializes the structure with default values
        explicit TextureFormatInfoExt() : 
            Filterable(false),
            ColorRenderable(false),
            DepthRenderable(false),
            Tex1DFmt(false),
            Tex2DFmt(false),
            Tex3DFmt(false),
            TexCubeFmt(false),
            SupportsMS(false)
        {}
    };
}
