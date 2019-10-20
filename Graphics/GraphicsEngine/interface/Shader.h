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
/// Definition of the Diligent::IShader interface and related data structures

#include "../../../Primitives/interface/FileStream.h"
#include "../../../Primitives/interface/FlagEnum.h"
#include "DeviceObject.h"

namespace Diligent
{

// {2989B45C-143D-4886-B89C-C3271C2DCC5D}
static constexpr INTERFACE_ID IID_Shader =
{ 0x2989b45c, 0x143d, 0x4886, { 0xb8, 0x9c, 0xc3, 0x27, 0x1c, 0x2d, 0xcc, 0x5d } };

/// Describes the shader type
enum SHADER_TYPE : Uint32
{
    SHADER_TYPE_UNKNOWN     = 0x000, ///< Unknown shader type
    SHADER_TYPE_VERTEX      = 0x001, ///< Vertex shader
    SHADER_TYPE_PIXEL       = 0x002, ///< Pixel (fragment) shader
    SHADER_TYPE_GEOMETRY    = 0x004, ///< Geometry shader
    SHADER_TYPE_HULL        = 0x008, ///< Hull (tessellation control) shader
    SHADER_TYPE_DOMAIN      = 0x010, ///< Domain (tessellation evaluation) shader
    SHADER_TYPE_COMPUTE     = 0x020  ///< Compute shader
};
DEFINE_FLAG_ENUM_OPERATORS(SHADER_TYPE);

/// Describes shader source code language
enum SHADER_SOURCE_LANGUAGE : Uint32
{
    /// Default language (GLSL for OpenGL/OpenGLES devices, HLSL for Direct3D11/Direct3D12 devices)
    SHADER_SOURCE_LANGUAGE_DEFAULT = 0,

    /// The source language is HLSL
    SHADER_SOURCE_LANGUAGE_HLSL,

    /// The source language is GLSL
    SHADER_SOURCE_LANGUAGE_GLSL
};

/// Shader description
struct ShaderDesc : DeviceObjectAttribs
{
	/// Shader type. See Diligent::SHADER_TYPE.
    SHADER_TYPE    ShaderType    = SHADER_TYPE_VERTEX;
};


// {3EA98781-082F-4413-8C30-B9BA6D82DBB7}
static constexpr INTERFACE_ID IID_IShaderSourceInputStreamFactory =
{ 0x3ea98781, 0x82f, 0x4413, { 0x8c, 0x30, 0xb9, 0xba, 0x6d, 0x82, 0xdb, 0xb7 } };

/// Shader source stream factory interface
class IShaderSourceInputStreamFactory : public IObject
{
public:
    virtual void CreateInputStream(const Diligent::Char *Name, IFileStream **ppStream) = 0;
};

struct ShaderMacro
{
    const Char* Name        = nullptr;
    const Char* Definition  = nullptr;
    
    ShaderMacro()noexcept{}
    ShaderMacro(const Char* _Name,
                const Char* _Def)noexcept :
        Name      ( _Name ),
        Definition( _Def )
    {}
};

/// Shader creation attributes
struct ShaderCreateInfo
{
	/// Source file path

    /// If source file path is provided, Source and ByteCode members must be null
    const Char* FilePath = nullptr;

	/// Pointer to the shader source input stream factory.

    /// The factory is used to load the shader source file if FilePath is not null.
	/// It is also used to create additional input streams for shader include files
    IShaderSourceInputStreamFactory* pShaderSourceStreamFactory = nullptr;

    /// HLSL->GLSL conversion stream
    
    /// If HLSL->GLSL converter is used to convert HLSL shader source to
    /// GLSL, this member can provide pointer to the conversion stream. It is useful
    /// when the same file is used to create a number of different shaders. If
    /// ppConversionStream is null, the converter will parse the same file
    /// every time new shader is converted. If ppConversionStream is not null,
    /// the converter will write pointer to the conversion stream to *ppConversionStream
    /// the first time and will use it in all subsequent times. 
    /// For all subsequent conversions, FilePath member must be the same, or 
    /// new stream will be crated and warning message will be displayed.
    class IHLSL2GLSLConversionStream** ppConversionStream = nullptr;

	/// Shader source

    /// If shader source is provided, FilePath and ByteCode members must be null
    const Char* Source = nullptr;

    /// Compiled shader bytecode. 
    
    /// If shader byte code is provided, FilePath and Source members must be null
    /// \note. This option is supported for D3D11, D3D12 and Vulkan backends. 
    ///        For D3D11 and D3D12 backends, HLSL bytecode should be provided. Vulkan
    ///        backend expects SPIRV bytecode.
    ///        The bytecode must contain reflection information. If shaders were compiled 
    ///        using fxc, make sure that /Qstrip_reflect option is *not* specified.
    ///        HLSL shaders need to be compiled against 4.0 profile or higher.
    const void* ByteCode = nullptr;

    /// Size of the compiled shader bytecode

    /// Byte code size (in bytes) must be provided if ByteCode is not null
    size_t ByteCodeSize = 0;

	/// Shader entry point

    /// This member is ignored if ByteCode is not null
    const Char* EntryPoint = "main";

	/// Shader macros

    /// This member is ignored if ByteCode is not null
    const ShaderMacro* Macros = nullptr;

    /// If set to true, textures will be combined with texture samplers.
    /// The CombinedSamplerSuffix member defines the suffix added to the texture variable
    /// name to get corresponding sampler name. When using combined samplers,
    /// the sampler assigned to the shader resource view is automatically set when
    /// the view is bound. Otherwise samplers need to be explicitly set similar to other 
    /// shader variables.
    bool UseCombinedTextureSamplers = false;

    /// If UseCombinedTextureSamplers is true, defines the suffix added to the
    /// texture variable name to get corresponding sampler name.  For example,
    /// for default value "_sampler", a texture named "tex" will be combined 
    /// with sampler named "tex_sampler". 
    /// If UseCombinedTextureSamplers is false, this member is ignored.
    const Char* CombinedSamplerSuffix = "_sampler";

	/// Shader description. See Diligent::ShaderDesc.
    ShaderDesc Desc;

	/// Shader source language. See Diligent::SHADER_SOURCE_LANGUAGE.
    SHADER_SOURCE_LANGUAGE SourceLanguage = SHADER_SOURCE_LANGUAGE_DEFAULT;


    /// Shader version
    struct ShaderVersion
    {
        /// Major revision
        Uint8 Major = 0;

        /// Minor revision
        Uint8 Minor = 0;

        ShaderVersion()noexcept{}
        ShaderVersion(Uint8 _Major, Uint8 _Minor)noexcept :
            Major {_Major},
            Minor {_Minor}
        {}
    };

    /// HLSL shader model to use when compiling the shader. When default value 
    /// is given (0, 0), the engine will attempt to use the highest HLSL shader model
    /// supported by the device. If the shader is created from the byte code, this value
    /// has no effect.
    ///
    /// \note When HLSL source is converted to GLSL, corresponding GLSL/GLESSL version will be used.
    ShaderVersion HLSLVersion = ShaderVersion{};

    /// GLSL version to use when creating the shader. When default value 
    /// is given (0, 0), the engine will attempt to use the highest GLSL version
    /// supported by the device.
    ShaderVersion GLSLVersion = ShaderVersion{};

    /// GLES shading language version to use when creating the shader. When default value 
    /// is given (0, 0), the engine will attempt to use the highest GLESSL version
    /// supported by the device.
    ShaderVersion GLESSLVersion = ShaderVersion{};


    /// Memory address where pointer to the compiler messages data blob will be written

    /// The buffer contains two null-terminated strings. The first one is the compiler
    /// output message. The second one is the full shader source code including definitions added
    /// by the engine. Data blob object must be released by the client.
    IDataBlob** ppCompilerOutput = nullptr;
};

/// Describes shader resource type
enum SHADER_RESOURCE_TYPE : Uint8
{
    /// Shader resource type is unknown
    SHADER_RESOURCE_TYPE_UNKNOWN = 0,

    /// Constant (uniform) buffer
    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,

    /// Shader resource view of a texture (sampled image)
    SHADER_RESOURCE_TYPE_TEXTURE_SRV,

    /// Shader resource view of a buffer (read-only storage image)
    SHADER_RESOURCE_TYPE_BUFFER_SRV,

    /// Unordered access view of a texture (sotrage image)
    SHADER_RESOURCE_TYPE_TEXTURE_UAV,

    /// Unordered access view of a buffer (storage buffer)
    SHADER_RESOURCE_TYPE_BUFFER_UAV,

    /// Sampler (separate sampler)
    SHADER_RESOURCE_TYPE_SAMPLER
};

/// Shader resource description
struct ShaderResourceDesc
{
    /// Shader resource name
    const char*          Name      = nullptr;

    /// Shader resource type, see Diligent::SHADER_RESOURCE_TYPE.
    SHADER_RESOURCE_TYPE Type      = SHADER_RESOURCE_TYPE_UNKNOWN;

    /// Array size. For non-array resource this value is 1.
    Uint32               ArraySize = 0;
};

/// Shader interface
class IShader : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override = 0;
    
    /// Returns the shader description
    virtual const ShaderDesc& GetDesc()const override = 0;

    /// Returns the total number of shader resources
    virtual Uint32 GetResourceCount()const = 0;

    /// Returns the pointer to the array of shader resources
    virtual ShaderResourceDesc GetResource(Uint32 Index)const = 0;
};

}
