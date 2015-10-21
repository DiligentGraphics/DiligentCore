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
#include "GLProgram.h"

namespace Diligent
{
    GLProgram::GLProgram( bool CreateObject ) :
        GLObjectWrappers::GLProgramObj( CreateObject )
    {}
    
    GLProgram::GLProgram( GLProgram&& Program ):
        GLObjectWrappers::GLProgramObj( std::move( Program ) ),
        m_UniformBlocks( std::move( Program.m_UniformBlocks ) ),
        m_Samplers( std::move( Program.m_Samplers ) ),
        m_Images( std::move( Program.m_Images ) ),
        m_StorageBlocks( std::move( Program.m_StorageBlocks ) )
    {}

    void GLProgram::LoadUniforms()
    {
        GLuint GLProgram = static_cast<GLuint>(*this);

        GLint numActiveUniforms = 0;
        glGetProgramiv( GLProgram, GL_ACTIVE_UNIFORMS, &numActiveUniforms );
        CHECK_GL_ERROR_AND_THROW( "Unable to get number of active uniforms\n" );

        // Query the maximum name length of the active uniform (including null terminator)
        GLint activeUniformMaxLength = 0;
        glGetProgramiv( GLProgram, GL_ACTIVE_UNIFORM_MAX_LENGTH, &activeUniformMaxLength );
        CHECK_GL_ERROR_AND_THROW( "Unable to get maximum uniform name length\n" );

        GLint numActiveUniformBlocks = 0;
        glGetProgramiv( GLProgram, GL_ACTIVE_UNIFORM_BLOCKS, &numActiveUniformBlocks );
        CHECK_GL_ERROR_AND_THROW( "Unable to get the number of active uniform blocks\n" );

        //
        // #### This parameter is currently unsupported by Intel OGL drivers.
        //
        // Query the maximum name length of the active uniform block (including null terminator)
        GLint activeUniformBlockMaxLength = 0;
        // On Intel driver, this call might fail:
        glGetProgramiv( GLProgram, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &activeUniformBlockMaxLength );
        //CHECK_GL_ERROR_AND_THROW("Unable to get the maximum uniform block name length\n");
        if( GL_NO_ERROR != glGetError() )
        {
            LOG_WARNING_MESSAGE( "Unable to get the maximum uniform block name length. Using 1024 as a workaround\n" );
            activeUniformBlockMaxLength = 1024;
        }
        

        GLint numActiveShaderStorageBlocks = 0;
        glGetProgramInterfaceiv( GLProgram, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &numActiveShaderStorageBlocks );
        CHECK_GL_ERROR_AND_THROW( "Unable to get the number of shader storage blocks blocks\n" );

        // Query the maximum name length of the active shader storage block (including null terminator)
        GLint MaxShaderStorageBlockNameLen = 0;
        glGetProgramInterfaceiv( GLProgram, GL_SHADER_STORAGE_BLOCK, GL_MAX_NAME_LENGTH, &MaxShaderStorageBlockNameLen );
        CHECK_GL_ERROR_AND_THROW( "Unable to get the maximum shader storage block name length\n" );

        auto MaxNameLength = std::max( activeUniformMaxLength, activeUniformBlockMaxLength );
        MaxNameLength = std::max( MaxNameLength, MaxShaderStorageBlockNameLen );

        MaxNameLength = std::max( MaxNameLength, 512 );
        std::vector<GLchar> Name( MaxNameLength + 1 );
        for( int i = 0; i < numActiveUniforms; i++ ) 
        {
            GLenum  dataType = 0;
            GLint   size = 0;
            GLint NameLen = 0;
            glGetActiveUniform( GLProgram, i, MaxNameLength, &NameLen, &size, &dataType, Name.data() );
            CHECK_GL_ERROR_AND_THROW( "Unable to get active uniform\n" );
            VERIFY( NameLen < MaxNameLength && NameLen == strlen( Name.data() ), "Incorrect uniform name" );
            // Note that 
            // glGetActiveUniform( program, index, bufSize, length, size, type, name );
            //
            // is equivalent to
            //
            // const enum props[] = { ARRAY_SIZE, TYPE };
            // glGetProgramResourceName( program, UNIFORM, index, bufSize, length, name );
            // glGetProgramResourceiv( program, GL_UNIFORM, index, 1, &props[0], 1, NULL, size );
            // glGetProgramResourceiv( program, GL_UNIFORM, index, 1, &props[1], 1, NULL, (int *)type );
            //
            // The latter is only available in GL 4.4 and GLES 3.1 

            switch( dataType )
            {
                case GL_SAMPLER_1D:
                case GL_SAMPLER_2D:
                case GL_SAMPLER_3D:
                case GL_SAMPLER_CUBE:
                case GL_SAMPLER_1D_SHADOW:
                case GL_SAMPLER_2D_SHADOW:

                case GL_SAMPLER_1D_ARRAY:
                case GL_SAMPLER_2D_ARRAY:
                case GL_SAMPLER_1D_ARRAY_SHADOW:
                case GL_SAMPLER_2D_ARRAY_SHADOW:
                case GL_SAMPLER_CUBE_SHADOW:

                case GL_INT_SAMPLER_1D:
                case GL_INT_SAMPLER_2D:
                case GL_INT_SAMPLER_3D:
                case GL_INT_SAMPLER_CUBE:
                case GL_INT_SAMPLER_1D_ARRAY:
                case GL_INT_SAMPLER_2D_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_1D:
                case GL_UNSIGNED_INT_SAMPLER_2D:
                case GL_UNSIGNED_INT_SAMPLER_3D:
                case GL_UNSIGNED_INT_SAMPLER_CUBE:
                case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:

                case GL_SAMPLER_CUBE_MAP_ARRAY:
                case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
                case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:

                case GL_SAMPLER_2D_MULTISAMPLE:
                case GL_INT_SAMPLER_2D_MULTISAMPLE:
                case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
                case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
                case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:

                case GL_SAMPLER_BUFFER:
                case GL_INT_SAMPLER_BUFFER:
                case GL_UNSIGNED_INT_SAMPLER_BUFFER:
                {
                    auto UniformLocation = glGetUniformLocation( GLProgram, Name.data() );
                    // Note that glGetUniformLocation(program, name) is equivalent to 
                    // glGetProgramResourceLocation( program, GL_UNIFORM, name );
                    // The latter is only available in GL 4.4 and GLES 3.1
                    m_Samplers.emplace_back( SamplerInfo( Name.data(), UniformLocation, dataType) );
                    break;
                }

                case GL_IMAGE_1D:
                case GL_IMAGE_2D:
                case GL_IMAGE_3D:
                case GL_IMAGE_2D_RECT:
                case GL_IMAGE_CUBE:
                case GL_IMAGE_BUFFER:
                case GL_IMAGE_1D_ARRAY:
                case GL_IMAGE_2D_ARRAY:
                case GL_IMAGE_CUBE_MAP_ARRAY:
                case GL_IMAGE_2D_MULTISAMPLE:
                case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
                case GL_INT_IMAGE_1D:
                case GL_INT_IMAGE_2D:
                case GL_INT_IMAGE_3D:
                case GL_INT_IMAGE_2D_RECT:
                case GL_INT_IMAGE_CUBE:
                case GL_INT_IMAGE_BUFFER:
                case GL_INT_IMAGE_1D_ARRAY:
                case GL_INT_IMAGE_2D_ARRAY:
                case GL_INT_IMAGE_CUBE_MAP_ARRAY:
                case GL_INT_IMAGE_2D_MULTISAMPLE:
                case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_1D:
                case GL_UNSIGNED_INT_IMAGE_2D:
                case GL_UNSIGNED_INT_IMAGE_3D:
                case GL_UNSIGNED_INT_IMAGE_2D_RECT:
                case GL_UNSIGNED_INT_IMAGE_CUBE:
                case GL_UNSIGNED_INT_IMAGE_BUFFER:
                case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
                case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
                {
                    auto UniformLocation = glGetUniformLocation( GLProgram, Name.data() );
                    
                    GLint BindingPoint = -1;
                    // The value of an image uniform is an integer specifying the image unit accessed
                    glGetUniformiv( GLProgram, UniformLocation, &BindingPoint );
                    CHECK_GL_ERROR_AND_THROW("Failed to get image binding point");
                    VERIFY( BindingPoint >= 0, "Incorrect binding point" );

                    m_Images.emplace_back( ImageInfo(Name.data(), BindingPoint, dataType) );
                    break;
                }

                default:
                    // Some other uniform type like scalar, matrix etc.
                    break;
            }
        }

        for( int i = 0; i < numActiveUniformBlocks; i++ )
        {
            GLsizei NameLen = 0;
            glGetActiveUniformBlockName( GLProgram, i, MaxNameLength, &NameLen, Name.data() );
            CHECK_GL_ERROR_AND_THROW( "Unable to get active uniform block name\n" );
            VERIFY( NameLen < MaxNameLength && NameLen == strlen( Name.data() ), "Incorrect uniform block name" );

            // glGetActiveUniformBlockName( program, uniformBlockIndex, bufSize, length, uniformBlockName );
            // is equivalent to
            // glGetProgramResourceName(program, GL_UNIFORM_BLOCK, uniformBlockIndex, bufSize, length, uniformBlockName);

            auto UniformBlockIndex = glGetUniformBlockIndex( GLProgram, Name.data() );
            CHECK_GL_ERROR_AND_THROW( "Unable to get active uniform block index\n" );
            // glGetUniformBlockIndex( program, uniformBlockName );
            // is equivalent to
            // glGetProgramResourceIndex( program, GL_UNIFORM_BLOCK, uniformBlockName );
            m_UniformBlocks.emplace_back(UniformBufferInfo(Name.data(), UniformBlockIndex));
        }

        for( int i = 0; i < numActiveShaderStorageBlocks; ++i )
        {
            GLsizei Length = 0;
            glGetProgramResourceName( GLProgram, GL_SHADER_STORAGE_BLOCK, i, MaxNameLength, &Length, Name.data() );
            CHECK_GL_ERROR_AND_THROW( "Unable to get shader storage block name\n" );
            VERIFY( Length < MaxNameLength && Length == strlen( Name.data() ), "Incorrect shader storage block name" );

            GLenum Prop = GL_BUFFER_BINDING;
            GLint Binding = -1;
            GLint ValuesWritten = 0;
            glGetProgramResourceiv( GLProgram, GL_SHADER_STORAGE_BLOCK, i, 1, &Prop, 1, &ValuesWritten, &Binding );
            CHECK_GL_ERROR_AND_THROW( "Unable to get shader storage block binding\n" );
            VERIFY( ValuesWritten == 1 && Binding >= 0, "Incorrect shader storage block binding" );

            m_StorageBlocks.emplace_back( StorageBlockInfo(Name.data(), Binding) );
        }
    }

    template<typename TResArrayType>
    void BindResourcesHelper(TResArrayType &ResArr, IResourceMapping *pResourceMapping, Uint32 Flags)
    {
        for( auto res = ResArr.begin(); res != ResArr.end(); ++res )
        {
            auto &Name = res->Name;
            if( Flags & BIND_SHADER_RESOURCES_RESET_BINDINGS )
                res->pResource.Release();

            if( (Flags & BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED) && res->pResource )
                continue; // Skip already resolved resources

            RefCntAutoPtr<IDeviceObject> pNewRes;
            pResourceMapping->GetResource( Name.c_str(), static_cast<IDeviceObject**>(&pNewRes) );

            if( !pNewRes )
            {
                if( Flags & BIND_SHADER_RESOURCES_ALL_RESOLVED )
                    LOG_ERROR_MESSAGE( "Resource \"", Name, "\" is not found in the resource mapping" );
                continue;
            }
            res->pResource = pNewRes;
        }
    }

    void GLProgram::BindResources( IResourceMapping *pResourceMapping, Uint32 Flags )
    {
        if( !pResourceMapping )
            return;

        BindResourcesHelper( m_UniformBlocks, pResourceMapping, Flags );
        BindResourcesHelper( m_Samplers,      pResourceMapping, Flags );
        BindResourcesHelper( m_Images,        pResourceMapping, Flags );
        BindResourcesHelper( m_StorageBlocks, pResourceMapping, Flags );
    }

#ifdef VERIFY_RESOURCE_BINDINGS
    template<typename TResArrayType>
    void dbgVerifyResourceBindingsHelper(TResArrayType &ResArr, const Char *VarType)
    {
        for( auto res = ResArr.begin(); res != ResArr.end(); ++res )
        {
            auto &Name = res->Name;
            if( !res->pResource )
                LOG_ERROR_MESSAGE( "No resource is bound to ", VarType, " variable \"", res->Name, "\"" );
        }
    }

    void GLProgram::dbgVerifyResourceBindings()
    {
        dbgVerifyResourceBindingsHelper( m_UniformBlocks, "uniform block" );
        dbgVerifyResourceBindingsHelper( m_Samplers,      "sampler" );
        dbgVerifyResourceBindingsHelper( m_Images,        "image" );
        dbgVerifyResourceBindingsHelper( m_StorageBlocks, "shader storage block" );
    }
#endif

}
