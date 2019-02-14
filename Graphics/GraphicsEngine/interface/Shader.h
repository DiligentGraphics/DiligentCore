/*     Copyright 2015-2019 Egor Yusov
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
#include "DeviceObject.h"
#include "ResourceMapping.h"
#include "Sampler.h"

namespace Diligent
{

// {2989B45C-143D-4886-B89C-C3271C2DCC5D}
static constexpr INTERFACE_ID IID_Shader =
{ 0x2989b45c, 0x143d, 0x4886, { 0xb8, 0x9c, 0xc3, 0x27, 0x1c, 0x2d, 0xcc, 0x5d } };

// {0D57DF3F-977D-4C8F-B64C-6675814BC80C}
static constexpr INTERFACE_ID IID_ShaderVariable =
{ 0xd57df3f, 0x977d, 0x4c8f, { 0xb6, 0x4c, 0x66, 0x75, 0x81, 0x4b, 0xc8, 0xc } };

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

enum SHADER_PROFILE : Uint8
{
    SHADER_PROFILE_DEFAULT = 0,
    SHADER_PROFILE_DX_4_0,
    SHADER_PROFILE_DX_5_0,
    SHADER_PROFILE_DX_5_1,
    SHADER_PROFILE_GL_4_2
};

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

/// Describes shader variable type that is used by ShaderVariableDesc
enum SHADER_VARIABLE_TYPE : Uint8
{
    /// Shader variable is constant across all shader instances.
    /// It must be set *once* directly through IShader::BindResources() or through 
    /// the shader variable.
    SHADER_VARIABLE_TYPE_STATIC = 0, 

    /// Shader variable is constant across shader resource bindings instance (see IShaderResourceBinding).
    /// It must be set *once* through IShaderResourceBinding::BindResources() or through
    /// the shader variable. It cannot be set through IShader interface
    SHADER_VARIABLE_TYPE_MUTABLE,

    /// Shader variable is dynamic. It can be set multiple times for every instance of shader resource 
    /// bindings (see IShaderResourceBinding). It cannot be set through IShader interface
    SHADER_VARIABLE_TYPE_DYNAMIC,

    /// Total number of shader variable types
    SHADER_VARIABLE_TYPE_NUM_TYPES
};


static_assert(SHADER_VARIABLE_TYPE_STATIC == 0 && SHADER_VARIABLE_TYPE_MUTABLE == 1 && SHADER_VARIABLE_TYPE_DYNAMIC == 2 && SHADER_VARIABLE_TYPE_NUM_TYPES == 3, "BIND_SHADER_RESOURCES_UPDATE_* flags rely on shader variable SHADER_VARIABLE_TYPE_* values being 0,1,2");
/// Describes flags that can be given to IShader::BindResources(),
/// IPipelineState::BindShaderResources(), and IDeviceContext::BindShaderResources() methods.
enum BIND_SHADER_RESOURCES_FLAGS : Uint32
{
    /// Indicates that static variable bindings are to be updated.
    BIND_SHADER_RESOURCES_UPDATE_STATIC  = (0x01 << SHADER_VARIABLE_TYPE_STATIC),

    /// Indicates that mutable variable bindings are to be updated.
    BIND_SHADER_RESOURCES_UPDATE_MUTABLE = (0x01 << SHADER_VARIABLE_TYPE_MUTABLE),

    /// Indicates that dynamic variable bindings are to be updated.
    BIND_SHADER_RESOURCES_UPDATE_DYNAMIC = (0x01 << SHADER_VARIABLE_TYPE_DYNAMIC),

    /// Indicates that all variable types (static, mutable and dynamic) are to be updated.
    /// \note If none of BIND_SHADER_RESOURCES_UPDATE_STATIC, BIND_SHADER_RESOURCES_UPDATE_MUTABLE,
    ///       and BIND_SHADER_RESOURCES_UPDATE_DYNAMIC flags are set, all variable types are updated
    ///       as if BIND_SHADER_RESOURCES_UPDATE_ALL was specified.
    BIND_SHADER_RESOURCES_UPDATE_ALL = (BIND_SHADER_RESOURCES_UPDATE_STATIC | BIND_SHADER_RESOURCES_UPDATE_MUTABLE | BIND_SHADER_RESOURCES_UPDATE_DYNAMIC),

    /// If this flag is specified, all existing bindings will be preserved and 
    /// only unresolved ones will be updated.
    /// If this flag is not specified, every shader variable will be
    /// updated if the mapping contains corresponding resource.
    BIND_SHADER_RESOURCES_KEEP_EXISTING = 0x08,

    /// If this flag is specified, all shader bindings are expected
    /// to be resolved after the call. If this is not the case, debug message 
    /// will be displayed.
    /// \note Only these variables are verified that are being updated by setting
    ///       BIND_SHADER_RESOURCES_UPDATE_STATIC, BIND_SHADER_RESOURCES_UPDATE_MUTABLE, and
    ///       BIND_SHADER_RESOURCES_UPDATE_DYNAMIC flags.
    BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED = 0x10
};

/// Describes shader variable
struct ShaderVariableDesc
{
    /// Shader variable name
    const Char* Name            = nullptr;

    /// Shader variable type. See Diligent::SHADER_VARIABLE_TYPE for a list of allowed types
    SHADER_VARIABLE_TYPE Type   = SHADER_VARIABLE_TYPE_STATIC;

    ShaderVariableDesc()noexcept{}

    ShaderVariableDesc(const Char*          _Name,
                       SHADER_VARIABLE_TYPE _Type) : 
        Name(_Name),
        Type(_Type)
    {}
};


/// Static sampler description
struct StaticSamplerDesc
{
    /// The name of the sampler itself or the name of the texture variable that 
    /// this static sampler is assigned to if combined texture samplers are used.
    const Char* SamplerOrTextureName = nullptr;

    /// Sampler description
    SamplerDesc Desc;

    StaticSamplerDesc()noexcept{}
    StaticSamplerDesc(const Char*        _SamplerOrTextureName,    
                      const SamplerDesc& _Desc)noexcept : 
        SamplerOrTextureName(_SamplerOrTextureName),
        Desc                (_Desc)
    {}
};

/// Shader description
struct ShaderDesc : DeviceObjectAttribs
{
	/// Shader type. See Diligent::SHADER_TYPE
    SHADER_TYPE ShaderType                      = SHADER_TYPE_VERTEX;

    Bool bCacheCompiledShader                   = False;

    SHADER_PROFILE TargetProfile                = SHADER_PROFILE_DEFAULT;

    /// Default shader variable type. This type will be used if shader 
    /// variable description is not found in array VariableDesc points to
    /// or if VariableDesc == nullptr
    SHADER_VARIABLE_TYPE DefaultVariableType    = SHADER_VARIABLE_TYPE_STATIC;

    /// Array of shader variable descriptions
    const ShaderVariableDesc* VariableDesc      = nullptr;

    /// Number of elements in VariableDesc array
    Uint32 NumVariables                         = 0;

    /// Number of static samplers in StaticSamplers array
    Uint32 NumStaticSamplers                    = 0;
    
    /// Array of static sampler descriptions
    const StaticSamplerDesc* StaticSamplers     = nullptr;
};

/// Shader source stream factory interface
class IShaderSourceInputStreamFactory
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
struct ShaderCreationAttribs
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
    ///        Also, shaders need to be compiled against 4.0 profile or higher.
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

    /// Memory address where pointer to the compiler messages data blob will be written

    /// The buffer contains two null-terminated strings. The first one is the compiler
    /// output message. The second one is the full shader source code including definitions added
    /// by the engine. Data blob object must be released by the client.
    IDataBlob** ppCompilerOutput = nullptr;
};


/// Shader resource variable
class IShaderVariable : public IObject
{
public:
    /// Sets the variable to the given value

    /// \remark The method performs run-time correctness checks.
    ///         For instance, shader resource view cannot
    ///         be assigned to a constant buffer variable.
    virtual void Set(IDeviceObject* pObject) = 0;

    /// Sets the variable array

    /// \param [in] ppObjects - pointer to the array of objects
    /// \param [in] FirstElement - first array element to set
    /// \param [in] NumElements - number of objects in ppObjects array
    ///
    /// \remark The method performs run-time correctness checks.
    ///         For instance, shader resource view cannot
    ///         be assigned to a constant buffer variable.
    virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements) = 0;

    /// Returns shader variable type
    virtual SHADER_VARIABLE_TYPE GetType()const = 0;

    /// Returns array size. For non-array variables returns one.
    virtual Uint32 GetArraySize()const = 0;

    /// Returns the variable name
    virtual const Char* GetName()const = 0;

    /// Returns variable index that can be used to access the variable through
    /// shader or shader resource binding object
    virtual Uint32 GetIndex()const = 0;
};

/// Shader interface
class IShader : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) = 0;
    
    /// Returns the shader description
    virtual const ShaderDesc& GetDesc()const = 0;

    /// Binds shader resources.
    /// \param [in] pResourceMapping - Pointer to IResourceMapping interface to 
    ///                                look for resources.
    /// \param [in] Flags - Additional flags for the operation. See
    ///                     Diligent::BIND_SHADER_RESOURCES_FLAGS for details.
    /// \remark The shader will keep strong references to all resources bound to it.
    virtual void BindResources( IResourceMapping* pResourceMapping, Uint32 Flags ) = 0;

    /// Returns an interface to a shader variable. If the shader variable
    /// is not found, an interface to a dummy variable will be returned.
    
    /// \param [in] Name - Name of the variable.
    /// \remark The method does not increment the reference counter
    ///         of the returned interface.
    virtual IShaderVariable* GetShaderVariable(const Char* Name) = 0;

    /// Returns the number of shader variables.

    /// \remark Only static variables (that can be accessed directly through the shader) are counted.
    ///         Mutable and dynamic variables are accessed through Shader Resource Binding object.
    virtual Uint32 GetVariableCount() const = 0;

    /// Returns shader variable by its index.

    /// \param [in] Index - Shader variable index. The index must be between
    ///                     0 and the total number of variables returned by 
    ///                     GetVariableCount().
    /// \remark Only static shader variables can be accessed through this method.
    ///         Mutable and dynamic variables are accessed through Shader Resource 
    ///         Binding object
    virtual IShaderVariable* GetShaderVariable(Uint32 Index) = 0;
};

}
