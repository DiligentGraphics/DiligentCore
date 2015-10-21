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

#include "pch.h"

#include "ProgramPipelineCache.h"
#include "ShaderGLImpl.h"
#include "RenderDeviceGLImpl.h"

namespace Diligent
{

ProgramPipelineCache::ProgramPipelineCache( RenderDeviceGLImpl *pRenderDeviceOpenGL )
{
    auto &DeviceCaps = pRenderDeviceOpenGL->GetDeviceCaps();
    VERIFY( DeviceCaps.DevType != DeviceType::Undefined, "Device caps are not initialized" );
    m_bIsProgramPipelineSupported = DeviceCaps.bSeparableProgramSupported;

    m_Cache.max_load_factor(0.5f);
    m_ShaderToKey.max_load_factor(0.5f);
}

ProgramPipelineCache::~ProgramPipelineCache()
{
    VERIFY( m_Cache.empty(), "Program pipeline cache is not empty" );
    VERIFY( m_ShaderToKey.empty(), "Not all shaders that use the program pipleine are released" );
}

ProgramPipelineCache::CacheElementType &ProgramPipelineCache::GetProgramPipeline( RefCntAutoPtr<IShader> *ppShaders, Uint32 NumShadersToSet )
{
    ThreadingTools::LockHelper LockHelper(m_CacheLockFlag);

    PipelineCacheKey Key = {0};
    // Create a key for the look-up
    for(Uint32 Shader = 0; Shader < NumShadersToSet; ++Shader)
    {
        auto *pCurrShader = static_cast<IShader*>(ppShaders[Shader].RawPtr());
        switch(pCurrShader->GetDesc().ShaderType)
        {
            case SHADER_TYPE_VERTEX:  Key.pVS = pCurrShader; break;
            case SHADER_TYPE_PIXEL:   Key.pPS = pCurrShader; break;
            case SHADER_TYPE_GEOMETRY:Key.pGS = pCurrShader; break;
            case SHADER_TYPE_HULL:    Key.pHS = pCurrShader; break;
            case SHADER_TYPE_DOMAIN:  Key.pDS = pCurrShader; break;
            case SHADER_TYPE_COMPUTE: Key.pCS = pCurrShader; break;
            default: UNEXPECTED( "Unknown shader type" );
        }
    }

    // Try to find the Pipeline in the map
    auto It = m_Cache.find(Key);
    if( It != m_Cache.end() )
    {
        return It->second;
    }
    else
    {
        CacheElementType NewPipelineOrProg;

        if( m_bIsProgramPipelineSupported )
        {
            // Create new progam pipeline
            NewPipelineOrProg.Pipeline.Create();
            for( Uint32 Shader = 0; Shader < NumShadersToSet; ++Shader )
            {
                auto *pCurrShader = static_cast<ShaderGLImpl*>(ppShaders[Shader].RawPtr());
                auto GLShaderBit = ShaderTypeToGLShaderBit( pCurrShader->GetDesc().ShaderType );
                // If the program has an active code for each stage mentioned in set flags,
                // then that code will be used by the pipeline. If program is 0, then the given
                // stages are cleared from the pipeline.
                glUseProgramStages( NewPipelineOrProg.Pipeline, GLShaderBit, pCurrShader->m_GlProgObj );
                CHECK_GL_ERROR( "glUseProgramStages() failed" );
            }
        }
        else
        {
            // Create new progam
            NewPipelineOrProg.Program.Create();
            GLuint GLProgram = NewPipelineOrProg.Program;
            for( Uint32 Shader = 0; Shader < NumShadersToSet; ++Shader )
            {
                auto *pCurrShader = static_cast<ShaderGLImpl*>(ppShaders[Shader].RawPtr());
                glAttachShader( GLProgram, pCurrShader->m_GLShaderObj );
                CHECK_GL_ERROR( "glAttachShader() failed" );
            }
            glLinkProgram( GLProgram );
            CHECK_GL_ERROR( "glLinkProgram() failed" );
            int IsLinked = GL_FALSE;
            glGetProgramiv( GLProgram, GL_LINK_STATUS, (int *)&IsLinked );
            CHECK_GL_ERROR( "glGetProgramiv() failed" );
            if( !IsLinked )
            {
                int LengthWithNull = 0, Length = 0;
                // Notice that glGetProgramiv is used to get the length for a shader program, not glGetShaderiv.
                // The length of the info log includes a null terminator.
                glGetProgramiv( GLProgram, GL_INFO_LOG_LENGTH, &LengthWithNull );

                // The maxLength includes the NULL character
                std::vector<char> shaderProgramInfoLog( LengthWithNull );

                // Notice that glGetProgramInfoLog  is used, not glGetShaderInfoLog.
                glGetProgramInfoLog( GLProgram, LengthWithNull, &Length, &shaderProgramInfoLog[0] );
                VERIFY( Length == LengthWithNull-1, "Incorrect program info log len" );
                LOG_ERROR_MESSAGE( "Failed to link shader program:\n", &shaderProgramInfoLog[0], '\n');
                UNEXPECTED( "glLinkProgram failed" );
            }
            
            // Detach shaders from the program object
            for( Uint32 Shader = 0; Shader < NumShadersToSet; ++Shader )
            {
                auto *pCurrShader = static_cast<ShaderGLImpl*>(ppShaders[Shader].RawPtr());
                glDetachShader( GLProgram, pCurrShader->m_GLShaderObj );
                CHECK_GL_ERROR( "glDetachShader() failed" );
            }

            NewPipelineOrProg.Program.LoadUniforms();
        }
        
        auto NewElems = m_Cache.emplace( make_pair( Key, std::move( NewPipelineOrProg ) ) );
        // New element must be actually inserted
        VERIFY( NewElems.second, "Element was not inserted into the hash" );
        if( Key.pVS )m_ShaderToKey.insert( make_pair(Key.pVS, Key) );
        if( Key.pGS )m_ShaderToKey.insert( make_pair(Key.pGS, Key) );
        if( Key.pPS )m_ShaderToKey.insert( make_pair(Key.pPS, Key) );
        if( Key.pDS )m_ShaderToKey.insert( make_pair(Key.pDS, Key) );
        if( Key.pHS )m_ShaderToKey.insert( make_pair(Key.pHS, Key) );
        if( Key.pCS )m_ShaderToKey.insert( make_pair(Key.pCS, Key) );

        return NewElems.first->second;
    }
}

void ProgramPipelineCache::OnDestroyShader(IShader *pShader)
{
    ThreadingTools::LockHelper LockHelper(m_CacheLockFlag);
    auto EqualRange = m_ShaderToKey.equal_range(pShader);
    for(auto It = EqualRange.first; It != EqualRange.second; ++It)
    {
        m_Cache.erase(It->second);
    }
    m_ShaderToKey.erase(EqualRange.first, EqualRange.second);
}

}
