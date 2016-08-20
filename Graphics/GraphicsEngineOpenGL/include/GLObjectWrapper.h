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

#include "UniqueIdentifier.h"

namespace GLObjectWrappers
{

template<class CreateReleaseHelperType>
class GLObjWrapper
{
public:
    GLObjWrapper(bool CreateObject, CreateReleaseHelperType CreateReleaseHelper = CreateReleaseHelperType()) : 
        m_uiHandle(0),
        m_CreateReleaseHelper(CreateReleaseHelper)
    {
        if( CreateObject )
        {
            Create();
            if(!m_uiHandle)
            {
                std::string ErrorStr("Failed to create ");
                ErrorStr += m_CreateReleaseHelper.Name;
                ErrorStr += " object";
                LOG_ERROR_AND_THROW(ErrorStr.c_str());
            }
        }
    }

    ~GLObjWrapper()
    {
        Release();
    }
    
    GLObjWrapper(GLObjWrapper&& Wrapper) : 
        m_uiHandle(Wrapper.m_uiHandle),
        m_CreateReleaseHelper( std::move( Wrapper.m_CreateReleaseHelper ) ),
        m_UniqueId( std::move(Wrapper.m_UniqueId) )
    {
        Wrapper.m_uiHandle = 0;
    }

    GLObjWrapper& operator = (GLObjWrapper&& Wrapper)
    {
        Release();
        m_uiHandle = Wrapper.m_uiHandle;
        Wrapper.m_uiHandle = 0;
        m_CreateReleaseHelper = std::move( Wrapper.m_CreateReleaseHelper );
        m_UniqueId = std::move(Wrapper.m_UniqueId);
        return *this;
    }

    void Create()
    {
        VERIFY(m_uiHandle == 0, "GL object is already initialized");
        Release();
        m_CreateReleaseHelper.Create(m_uiHandle);
        VERIFY(m_uiHandle, "Failed to initialize GL object" );
    }

    void Release()
    {
        if( m_uiHandle )
        {
            m_CreateReleaseHelper.Release(m_uiHandle);
            m_uiHandle = 0;
        }
    }

    Diligent::UniqueIdentifier GetUniqueID()const
    {
        // This unique ID is used to unambiguously identify the object for
        // tracking purposes.
        // Niether GL handle nor pointer could be safely used for this purpose
        // as both GL reuses released handles and the OS reuses released pointers
        return m_UniqueId.GetID();
    }

    operator GLuint()const{return m_uiHandle;}

private:
    GLObjWrapper(const GLObjWrapper&);
    GLObjWrapper& operator = (const GLObjWrapper&);

    GLuint m_uiHandle;
    CreateReleaseHelperType m_CreateReleaseHelper;

    // Have separate counter for every type of wrappers
    Diligent::UniqueIdHelper<CreateReleaseHelperType> m_UniqueId;
};

class GLBufferObjCreateReleaseHelper
{
public:
    static void Create(GLuint &BuffObj){ glGenBuffers   (1, &BuffObj); }
    static void Release(GLuint BuffObj){ glDeleteBuffers(1, &BuffObj); }
    static const char *Name;
};
typedef GLObjWrapper<GLBufferObjCreateReleaseHelper> GLBufferObj;


class GLProgramObjCreateReleaseHelper
{
public:
    static void Create(GLuint &ProgObj){ ProgObj = glCreateProgram(); }
    static void Release(GLuint ProgObj){ glDeleteProgram(ProgObj);    }
    static const char *Name;
};
typedef GLObjWrapper<GLProgramObjCreateReleaseHelper> GLProgramObj;


class GLShaderObjCreateReleaseHelper
{
public:
    GLShaderObjCreateReleaseHelper(GLenum ShaderType) : m_ShaderType(ShaderType){}
    void Create(GLuint &ShaderObj){ ShaderObj = glCreateShader(m_ShaderType); }
    void Release(GLuint ShaderObj){ glDeleteShader(ShaderObj);    }
    static const char *Name;
private:
    GLenum m_ShaderType;
};
typedef GLObjWrapper<GLShaderObjCreateReleaseHelper> GLShaderObj;


class GLPipelineObjCreateReleaseHelper
{
public:
    void Create(GLuint &Pipeline) { glGenProgramPipelines(1, &Pipeline); }
    void Release(GLuint Pipeline) { glDeleteProgramPipelines(1, &Pipeline); }
    static const char *Name;
};
typedef GLObjWrapper<GLPipelineObjCreateReleaseHelper> GLPipelineObj;


class GLVAOCreateReleaseHelper
{
public:
    void Create(GLuint &VAO) { glGenVertexArrays(1, &VAO); }
    void Release(GLuint VAO) { glDeleteVertexArrays(1, &VAO); }
    static const char *Name;
};
typedef GLObjWrapper<GLVAOCreateReleaseHelper> GLVertexArrayObj;


class GLTextureCreateReleaseHelper
{
public:
    void Create(GLuint &Tex) { glGenTextures(1, &Tex); }
    void Release(GLuint Tex) { glDeleteTextures(1, &Tex); }
    static const char *Name;
};
typedef GLObjWrapper<GLTextureCreateReleaseHelper> GLTextureObj;

class GLSamplerCreateReleaseHelper
{
public:
    void Create(GLuint &Sampler) { glGenSamplers(1, &Sampler); }
    void Release(GLuint Sampler) { glDeleteSamplers(1, &Sampler); }
    static const char *Name;
};
typedef GLObjWrapper<GLSamplerCreateReleaseHelper> GLSamplerObj;


class GLFBOCreateReleaseHelper
{
public:
    void Create(GLuint &FBO) { glGenFramebuffers(1, &FBO); }
    void Release(GLuint FBO) { glDeleteFramebuffers(1, &FBO); }
    static const char *Name;
};
typedef GLObjWrapper<GLFBOCreateReleaseHelper> GLFrameBufferObj;

}
