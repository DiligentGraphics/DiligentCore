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

#include "pch.h"
#include "GLProgramResources.h"
#include "RenderDeviceGLImpl.h"

namespace Diligent
{
    GLProgramResources::GLProgramResources()
    {
    }

    GLProgramResources::GLProgramResources( GLProgramResources&& Program ):
        m_UniformBlocks( std::move( Program.m_UniformBlocks ) ),
        m_Samplers( std::move( Program.m_Samplers ) ),
        m_Images( std::move( Program.m_Images ) ),
        m_StorageBlocks( std::move( Program.m_StorageBlocks ) ),
        m_VariableHash(std::move( Program.m_VariableHash))
    {
    }

    inline void RemoveArrayBrackets(char *Str)
    {
        auto* OpenBacketPtr = strchr(Str, '[');
        if ( OpenBacketPtr != nullptr )
            *OpenBacketPtr = 0;
    }

    void GLProgramResources::LoadUniforms(RenderDeviceGLImpl *pDeviceGLImpl,
                                          GLuint GLProgram, 
                                          const SHADER_VARIABLE_TYPE DefaultVariableType, 
                                          const ShaderVariableDesc *VariableDesc, 
                                          Uint32 NumVars,
                                          const StaticSamplerDesc *StaticSamplers,
                                          Uint32 NumStaticSamplers)
    {
        VERIFY(GLProgram != 0, "Null GL program");

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

        auto MaxNameLength = std::max( activeUniformMaxLength, activeUniformBlockMaxLength );

#if GL_ARB_program_interface_query
        GLint numActiveShaderStorageBlocks = 0;
        if(glGetProgramInterfaceiv)
        {
            glGetProgramInterfaceiv( GLProgram, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &numActiveShaderStorageBlocks );
            CHECK_GL_ERROR_AND_THROW( "Unable to get the number of shader storage blocks blocks\n" );

            // Query the maximum name length of the active shader storage block (including null terminator)
            GLint MaxShaderStorageBlockNameLen = 0;
            glGetProgramInterfaceiv( GLProgram, GL_SHADER_STORAGE_BLOCK, GL_MAX_NAME_LENGTH, &MaxShaderStorageBlockNameLen );
            CHECK_GL_ERROR_AND_THROW( "Unable to get the maximum shader storage block name length\n" );
            MaxNameLength = std::max( MaxNameLength, MaxShaderStorageBlockNameLen );
        }
#endif

        MaxNameLength = std::max( MaxNameLength, 512 );
        std::vector<GLchar> Name( MaxNameLength + 1 );
        for( int i = 0; i < numActiveUniforms; i++ ) 
        {
            GLenum  dataType = 0;
            GLint   size = 0;
            GLint NameLen = 0;
            // If one or more elements of an array are active, the name of the array is returned in name?, 
            // the type is returned in type?, and the size? parameter returns the highest array element index used, 
            // plus one, as determined by the compiler and/or linker. 
            // Only one active uniform variable will be reported for a uniform array.
            // Uniform variables other than arrays will have a size of 1
            glGetActiveUniform( GLProgram, i, MaxNameLength, &NameLen, &size, &dataType, Name.data() );
            CHECK_GL_ERROR_AND_THROW( "Unable to get active uniform\n" );
            VERIFY( NameLen < MaxNameLength && static_cast<size_t>(NameLen) == strlen( Name.data() ), "Incorrect uniform name" );
            VERIFY( size >= 1, "Size is expected to be at least 1" );
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

                    RemoveArrayBrackets(Name.data());
                    auto VarType = GetShaderVariableType(Name.data(), DefaultVariableType, VariableDesc, NumVars);

                    RefCntAutoPtr<SamplerGLImpl> pStaticSampler;
                    for (Uint32 s = 0; s < NumStaticSamplers; ++s)
                    {
                        if (strcmp(Name.data(), StaticSamplers[s].TextureName) == 0)
                        {
                            pDeviceGLImpl->CreateSampler(StaticSamplers[s].Desc, reinterpret_cast<ISampler**>(static_cast<SamplerGLImpl**>(&pStaticSampler)) );
                            break;
                        }
                    }
                    m_Samplers.emplace_back( Name.data(), size, VarType, UniformLocation, dataType, pStaticSampler );
                    break;
                }

#if GL_ARB_shader_image_load_store
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

                    RemoveArrayBrackets(Name.data());
                    auto VarType = GetShaderVariableType(Name.data(), DefaultVariableType, VariableDesc, NumVars);
                    m_Images.emplace_back( Name.data(), size, VarType, BindingPoint, dataType );
                    break;
                }
#endif
                default:
                    // Some other uniform type like scalar, matrix etc.
                    break;
            }
        }

        for( int i = 0; i < numActiveUniformBlocks; i++ )
        {
            // In contrast to shader uniforms, every element in uniform block array is enumerated individually
            GLsizei NameLen = 0;
            glGetActiveUniformBlockName( GLProgram, i, MaxNameLength, &NameLen, Name.data() );
            CHECK_GL_ERROR_AND_THROW( "Unable to get active uniform block name\n" );
            VERIFY( NameLen < MaxNameLength && static_cast<size_t>(NameLen) == strlen( Name.data() ), "Incorrect uniform block name" );

            // glGetActiveUniformBlockName( program, uniformBlockIndex, bufSize, length, uniformBlockName );
            // is equivalent to
            // glGetProgramResourceName(program, GL_UNIFORM_BLOCK, uniformBlockIndex, bufSize, length, uniformBlockName);

            auto UniformBlockIndex = glGetUniformBlockIndex( GLProgram, Name.data() );
            CHECK_GL_ERROR_AND_THROW( "Unable to get active uniform block index\n" );
            // glGetUniformBlockIndex( program, uniformBlockName );
            // is equivalent to
            // glGetProgramResourceIndex( program, GL_UNIFORM_BLOCK, uniformBlockName );
            
            GLint ArraySize = 1;
            auto* OpenBacketPtr = strchr(Name.data(), '[');
            if(OpenBacketPtr != nullptr)
            {
                auto Ind = atoi(OpenBacketPtr+1);
                ArraySize = std::max(ArraySize, Ind+1);
                *OpenBacketPtr = 0;
                if (m_UniformBlocks.size() > 0)
                {
                    // Look at previous uniform block to check if it is the same array
                    auto &LastBlock = m_UniformBlocks.back();
                    if (LastBlock.Name.compare(Name.data()) == 0)
                    {
                        ArraySize = std::max(ArraySize, static_cast<GLint>(LastBlock.pResources.size()));
                        VERIFY(UniformBlockIndex == LastBlock.Index + Ind, "Uniform block indices are expected to be continuous");
                        LastBlock.pResources.resize(ArraySize);
                        continue;
                    }
                    else
                    {
#ifdef _DEBUG
                        for(const auto &ub : m_UniformBlocks)
                            VERIFY(ub.Name.compare(Name.data()) != 0, "Uniform block with the name \"", ub.Name, "\" has already been enumerated");
#endif
                    }
                }
            }

            
            auto VarType = GetShaderVariableType(Name.data(), DefaultVariableType, VariableDesc, NumVars);
            m_UniformBlocks.emplace_back( Name.data(), ArraySize, VarType, UniformBlockIndex );
        }

#if GL_ARB_shader_storage_buffer_object
        for( int i = 0; i < numActiveShaderStorageBlocks; ++i )
        {
            GLsizei Length = 0;
            glGetProgramResourceName( GLProgram, GL_SHADER_STORAGE_BLOCK, i, MaxNameLength, &Length, Name.data() );
            CHECK_GL_ERROR_AND_THROW( "Unable to get shader storage block name\n" );
            VERIFY( Length < MaxNameLength && static_cast<size_t>(Length) == strlen( Name.data() ), "Incorrect shader storage block name" );

            GLenum Props[] = {GL_BUFFER_BINDING};
            GLint Binding = -1;
            GLint ValuesWritten = 0;
            glGetProgramResourceiv( GLProgram, GL_SHADER_STORAGE_BLOCK, i, _countof(Props), Props, 1, &ValuesWritten, &Binding );
            CHECK_GL_ERROR_AND_THROW( "Unable to get shader storage block binding & array size\n" );
            VERIFY( ValuesWritten == _countof(Props), "Unexpected number of values written" );
            VERIFY( Binding >= 0, "Incorrect shader storage block binding" );

            Int32 ArraySize = 1;
            auto* OpenBacketPtr = strchr(Name.data(), '[');
            if(OpenBacketPtr != nullptr)
            {
                auto Ind = atoi(OpenBacketPtr+1);
                ArraySize = std::max(ArraySize, Ind+1);
                *OpenBacketPtr = 0;
                if (m_StorageBlocks.size() > 0)
                {
                    // Look at previous storage block to check if it is the same array
                    auto &LastBlock = m_StorageBlocks.back();
                    if (LastBlock.Name.compare(Name.data()) == 0)
                    {
                        ArraySize = std::max(ArraySize, static_cast<GLint>(LastBlock.pResources.size()));
                        VERIFY(Binding == LastBlock.Binding + Ind, "Storage block bindings are expected to be continuous");
                        LastBlock.pResources.resize(ArraySize);
                        continue;
                    }
                    else
                    {
#ifdef _DEBUG
                        for(const auto &sb : m_StorageBlocks)
                            VERIFY(sb.Name.compare(Name.data()) != 0, "Storage block with the name \"", sb.Name, "\" has already been enumerated");
#endif
                    }
                }
            }

            auto VarType = GetShaderVariableType(Name.data(), DefaultVariableType, VariableDesc, NumVars);
            m_StorageBlocks.emplace_back( Name.data(), ArraySize, VarType, Binding );
        }
#endif

    }

    static bool CheckType(SHADER_VARIABLE_TYPE Type, SHADER_VARIABLE_TYPE* AllowedTypes, Uint32 NumAllowedTypes)
    {
        for(Uint32 i=0; i < NumAllowedTypes; ++i)
            if(Type == AllowedTypes[i])
                return true;
    
        return false;
    }

    void GLProgramResources::Clone(const GLProgramResources& SrcLayout, 
                                   SHADER_VARIABLE_TYPE *VarTypes, 
                                   Uint32 NumVarTypes,
                                   IObject &Owner)
    {
        for (auto ub = SrcLayout.m_UniformBlocks.begin(); ub != SrcLayout.m_UniformBlocks.end(); ++ub)
        {
            if(CheckType(ub->VarType, VarTypes, NumVarTypes))
                m_UniformBlocks.emplace_back( ub->Name.c_str(), ub->pResources.size(), ub->VarType, ub->Index );
        }

        for (auto sam = SrcLayout.m_Samplers.begin(); sam != SrcLayout.m_Samplers.end(); ++sam)
        {
            if(CheckType(sam->VarType, VarTypes, NumVarTypes))
                m_Samplers.emplace_back( sam->Name.c_str(), sam->pResources.size(), sam->VarType, sam->Location, sam->Type, const_cast<SamplerGLImpl*>(sam->pStaticSampler.RawPtr()) );
        }

        for (auto img = SrcLayout.m_Images.begin(); img != SrcLayout.m_Images.end(); ++img)
        {
            if(CheckType(img->VarType, VarTypes, NumVarTypes))
                m_Images.emplace_back( img->Name.c_str(), img->pResources.size(), img->VarType, img->BindingPoint, img->Type );
        }

        for (auto sb = SrcLayout.m_StorageBlocks.begin(); sb != SrcLayout.m_StorageBlocks.end(); ++sb)
        {
            if(CheckType(sb->VarType, VarTypes, NumVarTypes))
                m_StorageBlocks.emplace_back( sb->Name.c_str(), sb->pResources.size(), sb->VarType, sb->Binding );
        }

        InitVariables(Owner);
    }

    void GLProgramResources::InitVariables(IObject &Owner)
    {
        // After all program resources are loaded, we can populate shader variable hash map.
        // The map contains raw pointers, but none of the arrays will ever change.
#define STORE_SHADER_VARIABLES(ResArr)\
        {                                                               \
            auto& Arr = ResArr;                                         \
            for( auto it = Arr.begin(); it != Arr.end(); ++it )         \
                /* HashMapStringKey will make a copy of the string*/    \
                m_VariableHash.insert( std::make_pair( Diligent::HashMapStringKey(it->Name), CGLShaderVariable(Owner, *it) ) ); \
        }

        STORE_SHADER_VARIABLES(m_UniformBlocks)
        STORE_SHADER_VARIABLES(m_Samplers)
        STORE_SHADER_VARIABLES(m_Images)
        STORE_SHADER_VARIABLES(m_StorageBlocks)
#undef STORE_SHADER_VARIABLES
    }

    IShaderVariable* GLProgramResources::GetShaderVariable( const Char* Name )
    {
        // Name will be implicitly converted to HashMapStringKey without making a copy
        auto it = m_VariableHash.find( Name );
        if( it == m_VariableHash.end() )
        {
            return nullptr;
        }
        return &it->second;
    }

    template<typename TResArrayType>
    void BindResourcesHelper(TResArrayType &ResArr, IResourceMapping *pResourceMapping, Uint32 Flags)
    {
        for( auto res = ResArr.begin(); res != ResArr.end(); ++res )
        {
            auto &Name = res->Name;
            for(Uint32 ArrInd = 0; ArrInd < res->pResources.size(); ++ArrInd)
            {
                auto &CurrResource = res->pResources[ArrInd];
                if( Flags & BIND_SHADER_RESOURCES_RESET_BINDINGS )
                    CurrResource.Release();

                if( (Flags & BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED) && CurrResource )
                    continue; // Skip already resolved resources

                RefCntAutoPtr<IDeviceObject> pNewRes;
                pResourceMapping->GetResource( Name.c_str(), static_cast<IDeviceObject**>(&pNewRes), ArrInd );

                if (pNewRes != nullptr)
                {
                    if(res->VarType == SHADER_VARIABLE_TYPE_STATIC && CurrResource !=  nullptr && CurrResource != pNewRes )
                        LOG_ERROR_MESSAGE( "Updating binding for static variable \"", Name, "\" is invalid and may result in an undefined behavior" );
                    CurrResource = pNewRes;
                }
                else
                {
                    if ( CurrResource == nullptr && (Flags & BIND_SHADER_RESOURCES_ALL_RESOLVED) )
                        LOG_ERROR_MESSAGE("Resource \"", Name, "\" is not found in the resource mapping");
                }
            }
        }
    }

    void GLProgramResources::BindResources( IResourceMapping *pResourceMapping, Uint32 Flags )
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
            for(Uint32 ArrInd = 0; ArrInd < res->pResources.size(); ++ArrInd)
            {
                if( !res->pResources[ArrInd] )
                {
                    if( res->pResources.size() > 1)
                        LOG_ERROR_MESSAGE( "No resource is bound to ", VarType, " variable \"", res->Name, "[", ArrInd, "]\"" );
                    else
                        LOG_ERROR_MESSAGE( "No resource is bound to ", VarType, " variable \"", res->Name, "\"" );
                }
            }
        }
    }

    void GLProgramResources::dbgVerifyResourceBindings()
    {
        dbgVerifyResourceBindingsHelper( m_UniformBlocks, "uniform block" );
        dbgVerifyResourceBindingsHelper( m_Samplers,      "sampler" );
        dbgVerifyResourceBindingsHelper( m_Images,        "image" );
        dbgVerifyResourceBindingsHelper( m_StorageBlocks, "shader storage block" );
    }
#endif

}
