/*     Copyright 2015-2017 Egor Yusov
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

#include "DeviceObject.h"
#include "ResourceMapping.h"
#include "FileStream.h"
#include "Sampler.h"

namespace Diligent
{

// {2989B45C-143D-4886-B89C-C3271C2DCC5D}
static const Diligent::INTERFACE_ID IID_Shader =
{ 0x2989b45c, 0x143d, 0x4886, { 0xb8, 0x9c, 0xc3, 0x27, 0x1c, 0x2d, 0xcc, 0x5d } };

// {0D57DF3F-977D-4C8F-B64C-6675814BC80C}
static const Diligent::INTERFACE_ID IID_ShaderVariable =
{ 0xd57df3f, 0x977d, 0x4c8f, { 0xb6, 0x4c, 0x66, 0x75, 0x81, 0x4b, 0xc8, 0xc } };

/// Describes the shader type
enum SHADER_TYPE : Int32
{
    SHADER_TYPE_UNKNOWN     = 0x000, ///< Unknown shader type
    SHADER_TYPE_VERTEX      = 0x001, ///< Vertex shader
    SHADER_TYPE_PIXEL       = 0x002, ///< Pixel (fragment) shader
    SHADER_TYPE_GEOMETRY    = 0x004, ///< Geometry shader
    SHADER_TYPE_HULL        = 0x008, ///< Hull (tessellation control) shader
    SHADER_TYPE_DOMAIN      = 0x010, ///< Domain (tessellation evaluation) shader
    SHADER_TYPE_COMPUTE     = 0x020  ///< Compute shader
};

enum SHADER_PROFILE : Int32
{
    SHADER_PROFILE_DEFAULT = 0,
    SHADER_PROFILE_DX_4_0,
    SHADER_PROFILE_DX_5_0,
    SHADER_PROFILE_DX_5_1,
    SHADER_PROFILE_GL_4_2
};

/// Describes shader source code language
enum SHADER_SOURCE_LANGUAGE : Int32
{
    /// Default language (GLSL for OpenGL/OpenGLES devices, HLSL for Direct3D11/Direct3D12 devices)
    SHADER_SOURCE_LANGUAGE_DEFAULT = 0,

    /// The source language is HLSL
    SHADER_SOURCE_LANGUAGE_HLSL,

    /// The source language is GLSL
    SHADER_SOURCE_LANGUAGE_GLSL
};

/// Describes flags that can be supplied to IShader::BindResources()
/// and IDeviceContext::BindShaderResources().
enum BIND_SHADER_RESOURCES_FLAGS : Int32
{
    /// Reset all bindings. If this flag is specified, all existing bindings will be
    /// broken. By default all existing bindings are preserved.
    BIND_SHADER_RESOURCES_RESET_BINDINGS = 0x01,

    /// If this flag is specified, only unresolved bindings will be updated.
    /// All resolved bindings will keep their original values.
    /// If this flag is not specified, every shader variable will be
    /// updated if the mapping contains corresponding resource.
    BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED = 0x02,

    /// If this flag is specified, all shader bindings are expected
    /// to be resolved after the call. If this is not the case, debug error 
    /// will be displayed.
    BIND_SHADER_RESOURCES_ALL_RESOLVED = 0x04
};

/// Describes shader variable type that is used by ShaderVariableDesc
enum SHADER_VARIABLE_TYPE : Int32
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

/// Describes shader variable
struct ShaderVariableDesc
{
    /// Shader variable name
    const Char *Name;

    /// Shader variable type. See Diligent::SHADER_VARIABLE_TYPE for a list of allowed types
    SHADER_VARIABLE_TYPE Type;
    ShaderVariableDesc(const Char *_Name = nullptr, SHADER_VARIABLE_TYPE _Type = SHADER_VARIABLE_TYPE_STATIC) : 
        Name(_Name),
        Type(_Type)
    {}
};


/// Static sampler description
struct StaticSamplerDesc
{
    /// Name of the texture variable that static sampler will be assigned to
    const Char* TextureName = nullptr;

    /// Sampler description
    SamplerDesc Desc;

    StaticSamplerDesc(){};
    StaticSamplerDesc(const Char* _TexName, const SamplerDesc &_Desc) : 
        TextureName(_TexName),
        Desc(_Desc)
    {}
};

/// Shader description
struct ShaderDesc : DeviceObjectAttribs
{
	/// Shader type. See Diligent::SHADER_TYPE
    SHADER_TYPE ShaderType;

    Bool bCacheCompiledShader;
    SHADER_PROFILE TargetProfile;

    /// Default shader variable type. This type will be used if shader 
    /// variable description is not found in array VariableDesc points to
    /// or if VariableDesc == nullptr
    SHADER_VARIABLE_TYPE DefaultVariableType;

    /// Array of shader variable descriptions
    const ShaderVariableDesc *VariableDesc;

    /// Number of elements in VariableDesc array
    Uint32 NumVariables;

    /// Number of static samplers in StaticSamplers array
    Uint32 NumStaticSamplers;
    
    /// Array of static sampler descriptions
    const StaticSamplerDesc *StaticSamplers;

    ShaderDesc() : 
        ShaderType(SHADER_TYPE_VERTEX),
        bCacheCompiledShader(False),
        TargetProfile(SHADER_PROFILE_DEFAULT),
        DefaultVariableType(SHADER_VARIABLE_TYPE_STATIC),
        VariableDesc(nullptr),
        NumVariables(0),
        NumStaticSamplers(0),
        StaticSamplers(nullptr)
    {}
};

/// Shader source stream factory interface
class IShaderSourceInputStreamFactory
{
public:
    virtual void CreateInputStream(const Diligent::Char *Name, IFileStream **ppStream) = 0;
};

struct ShaderMacro
{
    const Char* Name;
    const Char* Definition;
    ShaderMacro(const Char* _Name, const Char* _Def) : Name( _Name ), Definition( _Def ) {}
};

/// Shader creation attributes
struct ShaderCreationAttribs
{
	/// Source file path
    const Char* FilePath;

	/// Pointer to the shader source input stream factory.
	/// The factory is used to create additional input streams for
	/// shader include files
    IShaderSourceInputStreamFactory *pShaderSourceStreamFactory;

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
    class IHLSL2GLSLConversionStream **ppConversionStream;

	/// Shader source
    const Char* Source;

	/// Shader entry point
    const Char* EntryPoint;

	/// Shader macros
    const ShaderMacro *Macros;

	/// Shader description. See Diligent::ShaderDesc.
    ShaderDesc Desc;

	/// Shader source language. See Diligent::SHADER_SOURCE_LANGUAGE.
    SHADER_SOURCE_LANGUAGE SourceLanguage;

    ShaderCreationAttribs() :
        FilePath( nullptr ),
        Source( nullptr ),
        pShaderSourceStreamFactory( nullptr ),
        ppConversionStream( nullptr ),
        EntryPoint("main"),
        Macros(nullptr),
        SourceLanguage(SHADER_SOURCE_LANGUAGE_DEFAULT)
    {}
};


/// Shader resource variable
class IShaderVariable : public IObject
{
public:
    /// Sets the variable to the given value

    /// \remark The method performs run-time correctness checks.
    ///         For instance, shader resource view cannot
    ///         be assigned to a constant buffer variable.
    virtual void Set(IDeviceObject *pObject) = 0;

    /// Sets the variable array

    /// \param [in] ppObjects - pointer to the array of objects
    /// \param [in] FirstElement - first array element to set
    /// \param [in] NumElements - number of objects in ppObjects array
    ///
    /// \remark The method performs run-time correctness checks.
    ///         For instance, shader resource view cannot
    ///         be assigned to a constant buffer variable.
    virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements) = 0;
};

/// Shader interface
class IShader : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) = 0;
    
    /// Returns the shader description
    virtual const ShaderDesc &GetDesc()const = 0;

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
};

}
