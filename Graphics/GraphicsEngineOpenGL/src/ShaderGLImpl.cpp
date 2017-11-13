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

#include "pch.h"

#include "ShaderGLImpl.h"
#include "RenderDeviceGLImpl.h"
#include "DataBlobImpl.h"
#include "HLSL2GLSLConverterImpl.h"

using namespace Diligent;

namespace Diligent
{

ShaderGLImpl::ShaderGLImpl(IReferenceCounters *pRefCounters, RenderDeviceGLImpl *pDeviceGL, const ShaderCreationAttribs &ShaderCreationAttribs, bool bIsDeviceInternal) : 
    TShaderBase( pRefCounters, pDeviceGL, ShaderCreationAttribs.Desc, bIsDeviceInternal ),
    m_GlProgObj(false),
    m_GLShaderObj( false, GLObjectWrappers::GLShaderObjCreateReleaseHelper( GetGLShaderType( m_Desc.ShaderType ) ) )
{
    std::vector<const char *> ShaderStrings;

    // Each element in the length array may contain the length of the corresponding string 
    // (the null character is not counted as part of the string length).
    // Not specifying lengths causes shader compilation errors on Android
    std::vector<GLint> Lenghts;

    String Settings;
    
#if defined(PLATFORM_WIN32)
    Settings.append(
        "#version 430 core\n"
        "#define DESKTOP_GL 1\n"
    );
#elif defined(ANDROID)
    Settings.append(
        "#version 310 es\n"
    );

    if(m_Desc.ShaderType == SHADER_TYPE_GEOMETRY)
        Settings.append("#extension GL_EXT_geometry_shader : enable\n");

    if(m_Desc.ShaderType == SHADER_TYPE_HULL || m_Desc.ShaderType == SHADER_TYPE_DOMAIN)
        Settings.append("#extension GL_EXT_tessellation_shader : enable\n");

    Settings.append(
        "#ifndef GL_ES\n"
        "#  define GL_ES 1\n"
        "#endif\n"

        "precision highp float;\n"
        "precision highp int;\n"
        //"precision highp uint;\n"  // This line causes shader compilation error on NVidia!

        "precision highp sampler2D;\n"
        "precision highp sampler3D;\n"
        "precision highp samplerCube;\n"
        "precision highp samplerCubeArray;\n"
        "precision highp samplerCubeShadow;\n"
        "precision highp samplerCubeArrayShadow;\n"
        "precision highp sampler2DShadow;\n"
        "precision highp sampler2DArray;\n"
        "precision highp sampler2DArrayShadow;\n"
        "precision highp sampler2DMS;\n"       // ES3.1

        "precision highp isampler2D;\n"
        "precision highp isampler3D;\n"
        "precision highp isamplerCube;\n"
        "precision highp isamplerCubeArray;\n"
        "precision highp isampler2DArray;\n"
        "precision highp isampler2DMS;\n"      // ES3.1

        "precision highp usampler2D;\n"
        "precision highp usampler3D;\n"
        "precision highp usamplerCube;\n"
        "precision highp usamplerCubeArray;\n"
        "precision highp usampler2DArray;\n"
        "precision highp usampler2DMS;\n"      // ES3.1

        "precision highp image2D;\n"
        "precision highp image3D;\n"
        "precision highp imageCube;\n"
        "precision highp image2DArray;\n"

        "precision highp iimage2D;\n"
        "precision highp iimage3D;\n"
        "precision highp iimageCube;\n"
        "precision highp iimage2DArray;\n"

        "precision highp uimage2D;\n"
        "precision highp uimage3D;\n"
        "precision highp uimageCube;\n"
        "precision highp uimage2DArray;\n"
    );
#endif
        // It would be much more convenient to use row_major matrices.
        // But unfortunatelly on NVIDIA, the following directive 
        // layout(std140, row_major) uniform;
        // does not have any effect on matrices that are part of structures
        // So we have to use column-major matrices which are default in both
        // DX and GLSL.
    Settings.append(
        "layout(std140) uniform;\n"
    );

    ShaderStrings.push_back(Settings.c_str());
    Lenghts.push_back( static_cast<GLint>( Settings.length() ) );

    const Char* ShaderTypeDefine = nullptr;
    switch( m_Desc.ShaderType )
    {
        case SHADER_TYPE_VERTEX:    ShaderTypeDefine = "#define VERTEX_SHADER\n";         break;
        case SHADER_TYPE_PIXEL:     ShaderTypeDefine = "#define FRAGMENT_SHADER\n";       break;
        case SHADER_TYPE_GEOMETRY:  ShaderTypeDefine = "#define GEOMETRY_SHADER\n";       break;
        case SHADER_TYPE_HULL:      ShaderTypeDefine = "#define TESS_CONTROL_SHADER\n";   break;
        case SHADER_TYPE_DOMAIN:    ShaderTypeDefine = "#define TESS_EVALUATION_SHADER\n";break;
        case SHADER_TYPE_COMPUTE:   ShaderTypeDefine = "#define COMPUTE_SHADER\n";        break;
        default: UNEXPECTED("Shader type is not specified");
    }
    ShaderStrings.push_back( ShaderTypeDefine );
    Lenghts.push_back( static_cast<GLint>( strlen(ShaderTypeDefine) ) );

    String UserDefines;
    if( ShaderCreationAttribs.Macros != nullptr)
    {
        auto *pMacro = ShaderCreationAttribs.Macros;
        while( pMacro->Name != nullptr && pMacro->Definition != nullptr )
        {
            UserDefines += "#define ";
            UserDefines += pMacro->Name;
            UserDefines += ' ';
            UserDefines += pMacro->Definition;
            UserDefines += "\n";
            ++pMacro;
        }
        ShaderStrings.push_back( UserDefines.c_str() );
        Lenghts.push_back( static_cast<GLint>( UserDefines.length() ) );
    }

    RefCntAutoPtr<IDataBlob> pFileData(MakeNewRCObj<DataBlobImpl>()(0));
    auto ShaderSource = ShaderCreationAttribs.Source;
    GLuint SourceLen = 0;
    if( ShaderSource )
    {
        SourceLen = (GLint)strlen(ShaderSource);
    }
    else
    {
        VERIFY(ShaderCreationAttribs.pShaderSourceStreamFactory, "Input stream factory is null");
        RefCntAutoPtr<IFileStream> pSourceStream;
        ShaderCreationAttribs.pShaderSourceStreamFactory->CreateInputStream( ShaderCreationAttribs.FilePath, &pSourceStream );
        if (pSourceStream == nullptr)
            LOG_ERROR_AND_THROW("Failed to open shader source file")

        pSourceStream->Read( pFileData );
        ShaderSource = reinterpret_cast<char*>(pFileData->GetDataPtr());
        SourceLen = static_cast<GLint>( pFileData->GetSize() );
    }

    String ConvertedSource;
    if( ShaderCreationAttribs.SourceLanguage == SHADER_SOURCE_LANGUAGE_HLSL )
    {
        // Convert HLSL to GLSL
        const auto &Converter = HLSL2GLSLConverterImpl::GetInstance();
        HLSL2GLSLConverterImpl::ConversionAttribs Attribs;
        Attribs.pSourceStreamFactory = ShaderCreationAttribs.pShaderSourceStreamFactory;
        Attribs.ppConversionStream = ShaderCreationAttribs.ppConversionStream;
        Attribs.HLSLSource = ShaderSource;
        Attribs.NumSymbols = SourceLen;
        Attribs.EntryPoint = ShaderCreationAttribs.EntryPoint;
        Attribs.ShaderType = ShaderCreationAttribs.Desc.ShaderType;
        Attribs.IncludeDefinitions = true;
        Attribs.InputFileName = ShaderCreationAttribs.FilePath;
        ConvertedSource = Converter.Convert(Attribs);

        ShaderSource = ConvertedSource.c_str();
        SourceLen = static_cast<GLint>( ConvertedSource.length() );
    }

    ShaderStrings.push_back( ShaderSource );
    Lenghts.push_back( SourceLen );

    // Note: there is a simpler way to create the program:
    //m_uiShaderSeparateProg = glCreateShaderProgramv(GL_VERTEX_SHADER, _countof(ShaderStrings), ShaderStrings);
    // NOTE: glCreateShaderProgramv() is considered equivalent to both a shader compilation and a program linking 
    // operation. Since it performs both at the same time, compiler or linker errors can be encountered. However, 
    // since this function only returns a program object, compiler-type errors will be reported as linker errors 
    // through the following API:
    // GLint isLinked = 0;
    // glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    // The log can then be queried in the same way

    // Create empty shader object
    auto GLShaderType = GetGLShaderType(m_Desc.ShaderType);
    GLObjectWrappers::GLShaderObj ShaderObj(true, GLObjectWrappers::GLShaderObjCreateReleaseHelper(GLShaderType));
    
    VERIFY( ShaderStrings.size() == Lenghts.size(), "Incosistent array size" );

    // Provide source strings (the strings will be saved in internal OpenGL memory)
    glShaderSource(ShaderObj, static_cast<GLuint>( ShaderStrings.size() ), ShaderStrings.data(), Lenghts.data() );
    // When the shader is compiled, it will be compiled as if all of the given strings were concatenated end-to-end.
    glCompileShader(ShaderObj);
    GLint compiled = GL_FALSE;
    // Get compilation status
    glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &compiled);
    if(!compiled) 
    {
        std::stringstream ErrorMsgSS;
		ErrorMsgSS << "Failed to compile shader file \""<< (ShaderCreationAttribs.FilePath != nullptr ? ShaderCreationAttribs.FilePath : "") << '\"' << std::endl;
        int infoLogLen = 0;
        // The function glGetShaderiv() tells how many bytes to allocate; the length includes the NULL terminator. 
        glGetShaderiv(ShaderObj, GL_INFO_LOG_LENGTH, &infoLogLen);

        if (infoLogLen > 0)
        {
            std::vector<GLchar> infoLog(infoLogLen);
            int charsWritten = 0;
            // Get the log. infoLogLen is the size of infoLog. This tells OpenGL how many bytes at maximum it will write
            // charsWritten is a return value, specifying how many bytes it actually wrote. One may pass NULL if he 
            // doesn't care
            glGetShaderInfoLog(ShaderObj, infoLogLen, &charsWritten, infoLog.data());
            VERIFY(charsWritten == infoLogLen-1, "Unexpected info log length");
            ErrorMsgSS << "InfoLog:" << std::endl << infoLog.data() << std::endl;
        }
        LOG_ERROR_AND_THROW(ErrorMsgSS.str().c_str());
    }

    auto DeviceCaps = pDeviceGL->GetDeviceCaps();
    if( DeviceCaps.bSeparableProgramSupported )
    {
        m_GlProgObj.Create();

        // GL_PROGRAM_SEPARABLE parameter must be set before linking!
        glProgramParameteri( m_GlProgObj, GL_PROGRAM_SEPARABLE, GL_TRUE );
        glAttachShader( m_GlProgObj, ShaderObj );
        //With separable program objects, interfaces between shader stages may
        //involve the outputs from one program object and the inputs from a
        //second program object. For such interfaces, it is not possible to
        //detect mismatches at link time, because the programs are linked
        //separately. When each such program is linked, all inputs or outputs
        //interfacing with another program stage are treated as active. The
        //linker will generate an executable that assumes the presence of a
        //compatible program on the other side of the interface. If a mismatch
        //between programs occurs, no GL error will be generated, but some or all
        //of the inputs on the interface will be undefined.
        glLinkProgram( m_GlProgObj );
        CHECK_GL_ERROR( "glLinkProgram() failed" );
        int IsLinked = GL_FALSE;
        glGetProgramiv( m_GlProgObj, GL_LINK_STATUS, (int *)&IsLinked );
        CHECK_GL_ERROR( "glGetProgramiv() failed" );
        if( !IsLinked )
        {
            int LengthWithNull = 0, Length = 0;
            // Notice that glGetProgramiv is used to get the length for a shader program, not glGetShaderiv.
            // The length of the info log includes a null terminator.
            glGetProgramiv( m_GlProgObj, GL_INFO_LOG_LENGTH, &LengthWithNull );

            // The maxLength includes the NULL character
            std::vector<char> shaderProgramInfoLog( LengthWithNull );

            // Notice that glGetProgramInfoLog is used, not glGetShaderInfoLog.
            glGetProgramInfoLog( m_GlProgObj, LengthWithNull, &Length, &shaderProgramInfoLog[0] );
            VERIFY( Length == LengthWithNull-1, "Incorrect program info log len" );
            LOG_ERROR_AND_THROW( "Failed to link shader program:\n", &shaderProgramInfoLog[0], '\n');
        }

        glDetachShader( m_GlProgObj, ShaderObj );
        
        // glDeleteShader() deletes the shader immediately if it is not attached to any program 
        // object. Otherwise, the shader is flagged for deletion and will be deleted when it is 
        // no longer attached to any program object. If an object is flagged for deletion, its 
        // boolean status bit DELETE_STATUS is set to true
        ShaderObj.Release();

        m_GlProgObj.InitResources(pDeviceGL, m_Desc.DefaultVariableType, m_Desc.VariableDesc, m_Desc.NumVariables, m_Desc.StaticSamplers, m_Desc.NumStaticSamplers, *this);
    }
    else
    {
        m_GLShaderObj = std::move( ShaderObj );
    }
}

ShaderGLImpl::~ShaderGLImpl()
{
}

IMPLEMENT_QUERY_INTERFACE( ShaderGLImpl, IID_ShaderGL, TShaderBase )

void ShaderGLImpl::BindResources( IResourceMapping* pResourceMapping, Uint32 Flags )
{
    if( static_cast<GLuint>(m_GlProgObj) )
    {
        m_GlProgObj.BindConstantResources( pResourceMapping, Flags );
    }
    else
    {
        static bool FirstTime = true;
        if( FirstTime )
        {
            LOG_WARNING_MESSAGE( "IShader::BindResources() effectively does nothing when separable programs are not supported by the device. Use IDeviceContext::BindShaderResources() instead." );
            FirstTime = false;
        }
    }
}

IShaderVariable* ShaderGLImpl::GetShaderVariable( const Char* Name )
{
    if( !m_GlProgObj )
    {
        UNSUPPORTED( "Shader variable queries are currently supported for separable programs only" );
    }

    auto *pShaderVar = m_GlProgObj.GetConstantResources().GetShaderVariable(Name);
    if(!pShaderVar)
    {
        LOG_ERROR_MESSAGE( "Static shader variable \"", Name, "\" is not found in shader \"", m_Desc.Name ? m_Desc.Name : "", "\". Attempts to set the variable will be silently ignored." );
        pShaderVar = &m_DummyShaderVar;
    }
    return pShaderVar;
}

}
