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

#include "pch.h"
#include <unordered_set>
#include "GLProgramResources.h"
#include "RenderDeviceGLImpl.h"
#include "ShaderResourceBindingBase.h"
#include "Align.h"

namespace Diligent
{
    GLProgramResources::GLProgramResources(GLProgramResources&& Program)noexcept :
        m_ShaderStages   (Program.m_ShaderStages),
        m_UniformBuffers (Program.m_UniformBuffers),
        m_Samplers       (Program.m_Samplers),
        m_Images         (Program.m_Images),
        m_StorageBlocks  (Program.m_StorageBlocks),
        m_ResourceCache  (Program.m_ResourceCache),
        m_StringPool     (std::move(Program.m_StringPool)),
        m_NumUniformBuffers(Program.m_NumUniformBuffers),
        m_NumSamplers      (Program.m_NumSamplers),
        m_NumImages        (Program.m_NumImages),        
        m_NumStorageBlocks (Program.m_NumStorageBlocks)
    {
        Program.m_UniformBuffers = nullptr;
        Program.m_Samplers       = nullptr;
        Program.m_Images         = nullptr;
        Program.m_StorageBlocks  = nullptr;
        Program.m_ResourceCache  = nullptr;

        Program.m_NumUniformBuffers = 0;
        Program.m_NumSamplers       = 0;
        Program.m_NumImages         = 0;
        Program.m_NumStorageBlocks  = 0;
    }

    inline void RemoveArrayBrackets(char *Str)
    {
        auto* OpenBacketPtr = strchr(Str, '[');
        if ( OpenBacketPtr != nullptr )
            *OpenBacketPtr = 0;
    }

    void GLProgramResources::AllocateResources(IObject&                        Owner,
                                               std::vector<UniformBufferInfo>& UniformBlocks,
                                               std::vector<SamplerInfo>&       Samplers,
                                               std::vector<ImageInfo>&         Images,
                                               std::vector<StorageBlockInfo>&  StorageBlocks,
                                               bool                            InitializeResourceCache)
    {
        VERIFY(m_UniformBuffers == nullptr, "Resources have already been allocated!");

        m_NumUniformBuffers = static_cast<Uint32>(UniformBlocks.size());
        m_NumSamplers       = static_cast<Uint32>(Samplers.size());
        m_NumImages         = static_cast<Uint32>(Images.size());
        m_NumStorageBlocks  = static_cast<Uint32>(StorageBlocks.size());

        size_t StringPoolDataSize = 0;
        size_t ResourceCacheSize = 0;
        for (const auto& ub : UniformBlocks)
        {
            StringPoolDataSize += strlen(ub.Name) + 1;
            ResourceCacheSize  += ub.ArraySize;
        }

        for (const auto& sam : Samplers)
        {
            StringPoolDataSize += strlen(sam.Name) + 1;
            ResourceCacheSize  += sam.ArraySize;
        }

        for (const auto& img : Images)
        {
            StringPoolDataSize += strlen(img.Name) + 1;
            ResourceCacheSize  += img.ArraySize;
        }

        for (const auto& sb : StorageBlocks)
        {
            StringPoolDataSize += strlen(sb.Name) + 1;
            ResourceCacheSize  += sb.ArraySize;
        }

        auto AlignedStringPoolDataSize = Align(StringPoolDataSize, sizeof(void*));

        size_t TotalMemorySize = 
            m_NumUniformBuffers * sizeof(UniformBufferInfo) + 
            m_NumSamplers       * sizeof(SamplerInfo) +
            m_NumImages         * sizeof(ImageInfo) +
            m_NumStorageBlocks  * sizeof(StorageBlockInfo);
        
        if (TotalMemorySize == 0)
        {
            m_UniformBuffers = nullptr;
            m_Samplers       = nullptr;
            m_Images         = nullptr;
            m_StorageBlocks  = nullptr;
            m_ResourceCache  = nullptr;

            m_NumUniformBuffers = 0;
            m_NumSamplers       = 0;
            m_NumImages         = 0;
            m_NumStorageBlocks  = 0;

            return;
        }

        if (InitializeResourceCache)
            TotalMemorySize += ResourceCacheSize * sizeof(RefCntAutoPtr<IDeviceObject>);
        
        TotalMemorySize += AlignedStringPoolDataSize * sizeof(Char);

        auto& MemAllocator = GetRawAllocator();
        void* RawMemory = ALLOCATE(MemAllocator, "Memory buffer for GLProgramResources", TotalMemorySize);

        m_UniformBuffers = reinterpret_cast<UniformBufferInfo*>(RawMemory);
        m_Samplers       = reinterpret_cast<SamplerInfo*>     (m_UniformBuffers + m_NumUniformBuffers);
        m_Images         = reinterpret_cast<ImageInfo*>       (m_Samplers       + m_NumSamplers);
        m_StorageBlocks  = reinterpret_cast<StorageBlockInfo*>(m_Images         + m_NumImages);
        void* EndOfResourceData =                              m_StorageBlocks + m_NumStorageBlocks;
        Char* StringPoolData = nullptr;
        if (InitializeResourceCache)
        {
            m_ResourceCache = reinterpret_cast<RefCntAutoPtr<IDeviceObject>*>(EndOfResourceData);
            StringPoolData = reinterpret_cast<Char*>(m_ResourceCache + ResourceCacheSize);
            for (Uint32 res=0; res < ResourceCacheSize; ++res)
                new (m_ResourceCache+res) RefCntAutoPtr<IDeviceObject>{};
        }
        else
        {
            m_ResourceCache = nullptr;
            StringPoolData = reinterpret_cast<Char*>(EndOfResourceData);
        }

        m_StringPool.AssignMemory(StringPoolData, StringPoolDataSize);

        Uint16 VariableIndex = 0;
        auto* pCurrResource = m_ResourceCache;
        for (Uint32 ub=0; ub < m_NumUniformBuffers; ++ub)
        {
            auto& SrcUB = UniformBlocks[ub];
            new (m_UniformBuffers + ub) UniformBufferInfo
            {
                Owner,
                m_StringPool.CopyString(SrcUB.Name),
                SrcUB.VariableType,
                SrcUB.ResourceType,
                VariableIndex++,
                SrcUB.ArraySize,
                pCurrResource,
                SrcUB.UBIndex
            };
            if (pCurrResource != nullptr)
                pCurrResource += SrcUB.ArraySize;
        }

        for (Uint32 s=0; s < m_NumSamplers; ++s)
        {
            auto& SrcSam = Samplers[s];
            new (m_Samplers + s) SamplerInfo
            {
                Owner,
                m_StringPool.CopyString(SrcSam.Name),
                SrcSam.VariableType,
                SrcSam.ResourceType,
                VariableIndex++,
                SrcSam.ArraySize,
                pCurrResource,
                SrcSam.Location,
                SrcSam.SamplerType,
                SrcSam.pStaticSampler
            };
            if (pCurrResource != nullptr)
                pCurrResource += SrcSam.ArraySize;
        }

        for (Uint32 img=0; img < m_NumImages; ++img)
        {
            auto& SrcImg = Images[img];
            new (m_Images + img) ImageInfo
            {
                Owner,
                m_StringPool.CopyString(SrcImg.Name),
                SrcImg.VariableType,
                SrcImg.ResourceType,
                VariableIndex++,
                SrcImg.ArraySize,
                pCurrResource,
                SrcImg.BindingPoint,
                SrcImg.ImageType
            };
            if (pCurrResource != nullptr)
                pCurrResource += SrcImg.ArraySize;
        }

        for (Uint32 sb=0; sb < m_NumStorageBlocks; ++sb)
        {
            auto& SrcSB = StorageBlocks[sb];
            new (m_StorageBlocks + sb) StorageBlockInfo
            {
                Owner,
                m_StringPool.CopyString(SrcSB.Name),
                SrcSB.VariableType,
                SrcSB.ResourceType,
                VariableIndex++,
                SrcSB.ArraySize,
                pCurrResource,
                SrcSB.Binding
            };

            if (pCurrResource != nullptr)
                pCurrResource += SrcSB.ArraySize;
        }

        VERIFY_EXPR(VariableIndex == GetVariableCount());
        VERIFY_EXPR(m_StringPool.GetRemainingSize() == 0);
        VERIFY_EXPR(pCurrResource == nullptr || static_cast<size_t>(pCurrResource - m_ResourceCache) == ResourceCacheSize);
    }

    GLProgramResources::~GLProgramResources()
    {
        Uint32 ResourceCacheSize = 0;
        ProcessResources(
            [&](UniformBufferInfo& UB)
            {
                ResourceCacheSize += UB.ArraySize;
                UB.~UniformBufferInfo();
            },
            [&](SamplerInfo& Sam)
            {
                ResourceCacheSize += Sam.ArraySize;
                Sam.~SamplerInfo();
            },
            [&](ImageInfo& Img)
            {
                ResourceCacheSize += Img.ArraySize;
                Img.~ImageInfo();
            },
            [&](StorageBlockInfo& SB)
            {
                ResourceCacheSize += SB.ArraySize;
                SB.~StorageBlockInfo();
            }
        );

        if (m_ResourceCache != nullptr)
        {
            for (Uint32 res=0; res < ResourceCacheSize; ++res)
                m_ResourceCache[res].~RefCntAutoPtr();
        }

        void* RawMemory = m_UniformBuffers;
        if (RawMemory != nullptr)
        {
            auto& MemAllocator = GetRawAllocator();
            MemAllocator.Free(RawMemory);
        }
    }


    void GLProgramResources::LoadUniforms(IObject&             Owner,
                                          RenderDeviceGLImpl*  pDeviceGLImpl,
                                          SHADER_TYPE          ShaderStages,
                                          GLuint               GLProgram)
    {
        std::vector<UniformBufferInfo> UniformBlocks;
        std::vector<SamplerInfo>       Samplers;
        std::vector<ImageInfo>         Images;
        std::vector<StorageBlockInfo>  StorageBlocks;
        std::unordered_set<String>     NamesPool;

        VERIFY(GLProgram != 0, "Null GL program");

        m_ShaderStages = ShaderStages;

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
                    
                    Samplers.emplace_back(
                        Owner,
                        NamesPool.emplace(Name.data()).first->c_str(),
                        SHADER_RESOURCE_VARIABLE_TYPE_STATIC, 
                        SHADER_RESOURCE_TYPE_TEXTURE_SRV,
                        Uint16{0xFFFF}, // Variable index is assigned by AllocateResources
                        static_cast<Uint32>(size),
                        nullptr,        // pResources
                        UniformLocation, 
                        dataType,
                        nullptr
                    );
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

                    Images.emplace_back(
                        Owner,
                        NamesPool.emplace(Name.data()).first->c_str(),
                        SHADER_RESOURCE_VARIABLE_TYPE_STATIC, 
                        SHADER_RESOURCE_TYPE_TEXTURE_UAV,
                        Uint16{0xFFFF}, // Variable index is assigned by AllocateResources
                        static_cast<Uint32>(size),
                        nullptr,        // pResources
                        BindingPoint,
                        dataType );
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
            if (OpenBacketPtr != nullptr)
            {
                auto Ind = atoi(OpenBacketPtr+1);
                ArraySize = std::max(ArraySize, Ind+1);
                *OpenBacketPtr = 0;
                if (UniformBlocks.size() > 0)
                {
                    // Look at previous uniform block to check if it is the same array
                    auto& LastBlock = UniformBlocks.back();
                    if ( strcmp(LastBlock.Name, Name.data()) == 0)
                    {
                        ArraySize = std::max(ArraySize, static_cast<GLint>(LastBlock.ArraySize));
                        VERIFY(UniformBlockIndex == LastBlock.UBIndex + Ind, "Uniform block indices are expected to be continuous");
                        LastBlock.ArraySize = ArraySize;
                        continue;
                    }
                    else
                    {
#ifdef _DEBUG
                        for(const auto& ub : UniformBlocks)
                            VERIFY( strcmp(ub.Name, Name.data()) != 0, "Uniform block with the name \"", ub.Name, "\" has already been enumerated");
#endif
                    }
                }
            }

            UniformBlocks.emplace_back(
                Owner,
                NamesPool.emplace(Name.data()).first->c_str(),
                SHADER_RESOURCE_VARIABLE_TYPE_STATIC, 
                SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
                Uint16{0xFFFF}, // Variable index is assigned by AllocateResources
                static_cast<Uint32>(ArraySize),
                nullptr,        // pResources
                UniformBlockIndex
            );
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
                if (StorageBlocks.size() > 0)
                {
                    // Look at previous storage block to check if it is the same array
                    auto& LastBlock = StorageBlocks.back();
                    if ( strcmp(LastBlock.Name, Name.data()) == 0)
                    {
                        ArraySize = std::max(ArraySize, static_cast<GLint>(LastBlock.ArraySize));
                        VERIFY(Binding == LastBlock.Binding + Ind, "Storage block bindings are expected to be continuous");
                        LastBlock.ArraySize = ArraySize;
                        continue;
                    }
                    else
                    {
#ifdef _DEBUG
                        for(const auto& sb : StorageBlocks)
                            VERIFY( strcmp(sb.Name, Name.data()) != 0, "Storage block with the name \"", sb.Name, "\" has already been enumerated");
#endif
                    }
                }
            }

            StorageBlocks.emplace_back(
                Owner,
                NamesPool.emplace(Name.data()).first->c_str(),
                SHADER_RESOURCE_VARIABLE_TYPE_STATIC, 
                SHADER_RESOURCE_TYPE_BUFFER_UAV,
                Uint16{0xFFFF}, // Variable index is assigned by AllocateResources
                static_cast<Uint32>(ArraySize), 
                nullptr,        // pResources
                Binding
            );
        }
#endif
        AllocateResources(Owner, UniformBlocks, Samplers, Images, StorageBlocks, false);
    }


    void GLProgramResources::Clone(RenderDeviceGLImpl*                   pDeviceGLImpl, 
                                   IObject&                              Owner,
                                   const GLProgramResources&             SrcResources, 
                                   const PipelineResourceLayoutDesc&     ResourceLayout,
                                   const SHADER_RESOURCE_VARIABLE_TYPE*  AllowedVarTypes, 
                                   Uint32                                NumAllowedTypes)
    {
        std::vector<UniformBufferInfo> UniformBlocks;
        std::vector<SamplerInfo>       Samplers;
        std::vector<ImageInfo>         Images;
        std::vector<StorageBlockInfo>  StorageBlocks;

        m_ShaderStages = SrcResources.m_ShaderStages;
        const Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

        for (Uint32 ub=0; ub < SrcResources.GetNumUniformBuffers(); ++ub)
        {
            const auto& SrcUB = SrcResources.GetUniformBuffer(ub);
            auto VarType = GetShaderVariableType(m_ShaderStages, SrcUB.Name, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
            {
                UniformBlocks.emplace_back(
                    Owner,
                    SrcUB.Name,
                    VarType,
                    SrcUB.ResourceType,
                    Uint16{0xFFFF}, // Variable index is assigned by AllocateResources
                    SrcUB.ArraySize,
                    nullptr,        // pResources
                    SrcUB.UBIndex
                );
            }
        }

        for (Uint32 sam = 0; sam < SrcResources.GetNumSamplers(); ++sam)
        {
            const auto& SrcSam = SrcResources.GetSampler(sam);
            auto VarType = GetShaderVariableType(m_ShaderStages, SrcSam.Name, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
            {
                RefCntAutoPtr<ISampler> pStaticSampler;
                for (Uint32 s = 0; s < ResourceLayout.NumStaticSamplers; ++s)
                {
                    const auto& StSam = ResourceLayout.StaticSamplers[s];
                    if (strcmp(SrcSam.Name, StSam.SamplerOrTextureName) == 0)
                    {
                        pDeviceGLImpl->CreateSampler(StSam.Desc, &pStaticSampler);
                        break;
                    }
                }
                Samplers.emplace_back(
                    Owner,
                    SrcSam.Name,
                    VarType,
                    SrcSam.ResourceType,
                    Uint16{0xFFFF}, // Variable index is assigned by AllocateResources
                    SrcSam.ArraySize,
                    nullptr,        // pResources
                    SrcSam.Location,
                    SrcSam.SamplerType,
                    pStaticSampler.RawPtr<SamplerGLImpl>()
                );
            }
        }

        for (Uint32 img  = 0; img < SrcResources.GetNumImages(); ++img)
        {
            const auto& SrcImg = SrcResources.GetImage(img);
            auto VarType = GetShaderVariableType(m_ShaderStages, SrcImg.Name, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
            {
                Images.emplace_back(
                    Owner,
                    SrcImg.Name,
                    VarType,
                    SrcImg.ResourceType,
                    Uint16{0xFFFF}, // Variable index is assigned by AllocateResources
                    SrcImg.ArraySize,
                    nullptr,        // pResources
                    SrcImg.BindingPoint,
                    SrcImg.ImageType
                );
            }
        }

        for (Uint32 sb = 0; sb < SrcResources.GetNumStorageBlocks(); ++sb)
        {
            const auto& SrcSB = SrcResources.GetStorageBlock(sb);
            auto VarType = GetShaderVariableType(m_ShaderStages, SrcSB.Name, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
            {
                StorageBlocks.emplace_back(
                    Owner,
                    SrcSB.Name,
                    VarType,
                    SrcSB.ResourceType,
                    Uint16{0xFFFF}, // Variable index is assigned by AllocateResources
                    SrcSB.ArraySize,
                    nullptr,        // pResources
                    SrcSB.Binding
                );
            }
        }

        AllocateResources(Owner, UniformBlocks, Samplers, Images, StorageBlocks, true);
    }



    GLProgramResources::GLProgramVariableBase* GLProgramResources::GetVariable(const Char* Name)
    {
        // Name will be implicitly converted to HashMapStringKey without making a copy
        for (Uint32 ub=0; ub < m_NumUniformBuffers; ++ub)
        {
            auto& UB = GetUniformBuffer(ub);
            if (strcmp(UB.Name, Name) == 0)
                return &UB;
        }

        for (Uint32 s=0; s < m_NumSamplers; ++s)
        {
            auto& Sam = GetSampler(s);
            if (strcmp(Sam.Name, Name) == 0)
                return &Sam;
        }

        for (Uint32 img=0; img < m_NumImages; ++img)
        {
            auto& Img = GetImage(img);
            if (strcmp(Img.Name, Name) == 0)
                return &Img;
        }

        for (Uint32 sb=0; sb < m_NumStorageBlocks; ++sb)
        {
            auto& SB = GetStorageBlock(sb);
            if (strcmp(SB.Name, Name) == 0)
                return &SB;
        }

        return nullptr;
    }

    const GLProgramResources::GLProgramVariableBase* GLProgramResources::GetVariable(Uint32 Index)const
    {
        if (Index < GetNumUniformBuffers())
            return &GetUniformBuffer(Index);
        else
            Index -= GetNumUniformBuffers();

        if (Index < GetNumSamplers())
            return &GetSampler(Index);
        else
            Index -= GetNumSamplers();

        if (Index < GetNumImages())
            return &GetImage(Index);
        else
            Index -= GetNumImages();

        if (Index < GetNumStorageBlocks())
            return &GetStorageBlock(Index);
        else
            Index -= GetNumStorageBlocks();

        return nullptr;
    }


    static void BindResourcesHelper(GLProgramResources::GLProgramVariableBase& res, IResourceMapping* pResourceMapping, Uint32 Flags)
    {
        if ( (Flags & (1 << res.VariableType)) == 0 )
            return;

        auto& Name = res.Name;
        for(Uint32 ArrInd = 0; ArrInd < res.ArraySize; ++ArrInd)
        {
            auto& CurrResource = res.pResources[ArrInd];

            if( (Flags & BIND_SHADER_RESOURCES_KEEP_EXISTING) != 0 && CurrResource )
                continue; // Skip already resolved resources

            RefCntAutoPtr<IDeviceObject> pNewRes;
            pResourceMapping->GetResource( Name, static_cast<IDeviceObject**>(&pNewRes), ArrInd );

            if (pNewRes != nullptr)
            {
                if(res.VariableType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC && CurrResource !=  nullptr && CurrResource != pNewRes )
                    LOG_ERROR_MESSAGE( "Updating binding for static variable \"", Name, "\" is invalid and may result in an undefined behavior" );
                CurrResource = pNewRes;
            }
            else
            {
                if ( CurrResource == nullptr && (Flags & BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED) )
                    LOG_ERROR_MESSAGE("Resource \"", Name, "\" is not found in the resource mapping");
            }
        }
    }


    void GLProgramResources::BindResources(IResourceMapping* pResourceMapping, Uint32 Flags )
    {
        if( !pResourceMapping )
            return;

        if ( (Flags & BIND_SHADER_RESOURCES_UPDATE_ALL) == 0 )
            Flags |= BIND_SHADER_RESOURCES_UPDATE_ALL;

        ProcessResources(
            [&](UniformBufferInfo& UB)
            {
                BindResourcesHelper(UB, pResourceMapping, Flags);
            },
            [&](SamplerInfo& Sam)
            {
                BindResourcesHelper(Sam, pResourceMapping, Flags);
            },
            [&](ImageInfo& Img)
            {
                BindResourcesHelper(Img, pResourceMapping, Flags);
            },
            [&](StorageBlockInfo& SB)
            {
                BindResourcesHelper(SB, pResourceMapping, Flags);
            }
        );
    }


    bool GLProgramResources::IsCompatibleWith(const GLProgramResources& Res)const
    {
        if (GetNumUniformBuffers() != Res.GetNumUniformBuffers() ||
            GetNumSamplers()       != Res.GetNumSamplers()       ||
            GetNumImages()         != Res.GetNumImages()         ||
            GetNumStorageBlocks()  != Res.GetNumStorageBlocks())
            return false;

        for (Uint32 ub = 0; ub < GetNumUniformBuffers(); ++ub)
        {
            const auto& UB0 = GetUniformBuffer(ub);
            const auto& UB1 = Res.GetUniformBuffer(ub);
            if(!UB0.IsCompatibleWith(UB1))
                return false;
        }

        for (Uint32 sam = 0; sam < GetNumSamplers(); ++sam)
        {
            const auto& Sam0 = GetSampler(sam);
            const auto& Sam1 = Res.GetSampler(sam);
            if (!Sam0.IsCompatibleWith(Sam1))
                return false;
        }

        for (Uint32 img = 0; img < GetNumImages(); ++img)
        {
            const auto& Img0 = GetImage(img);
            const auto& Img1 = Res.GetImage(img);
            if (!Img0.IsCompatibleWith(Img1))
                return false;
        }

        for (Uint32 sb = 0; sb < GetNumStorageBlocks(); ++sb)
        {
            const auto& SB0 = GetStorageBlock(sb);
            const auto& SB1 = Res.GetStorageBlock(sb);
            if (!SB0.IsCompatibleWith(SB1))
                return false;
        }

        return true;
    }


    size_t GLProgramResources::GetHash()const
    {
        size_t hash = ComputeHash(GetNumUniformBuffers(), GetNumSamplers(), GetNumImages(), GetNumStorageBlocks());

        ProcessConstResources(
            [&](const UniformBufferInfo& UB)
            {
                HashCombine(hash, UB.GetHash());
            },
            [&](const SamplerInfo& Sam)
            {
                HashCombine(hash, Sam.GetHash());
            },
            [&](const ImageInfo& Img)
            {
                HashCombine(hash, Img.GetHash());
            },
            [&](const StorageBlockInfo& SB)
            {
                HashCombine(hash, SB.GetHash());
            }
        );

        return hash;
    }

#ifdef VERIFY_RESOURCE_BINDINGS
    static void dbgVerifyResourceBindingsHelper(const GLProgramResources::GLProgramVariableBase& res, const Char* VarTypeName)
    {
        for(Uint32 ArrInd = 0; ArrInd < res.ArraySize; ++ArrInd)
        {
            if( !res.pResources[ArrInd] )
            {
                if( res.ArraySize > 1)
                    LOG_ERROR_MESSAGE( "No resource is bound to ", VarTypeName, " variable \"", res.Name, "[", ArrInd, "]\"" );
                else
                    LOG_ERROR_MESSAGE( "No resource is bound to ", VarTypeName, " variable \"", res.Name, "\"" );
            }
        }
    }

    void GLProgramResources::dbgVerifyResourceBindings()const
    {
        ProcessConstResources(
            [&](const UniformBufferInfo& UB)
            {
                dbgVerifyResourceBindingsHelper(UB, "uniform block");
            },
            [&](const SamplerInfo& Sam)
            {
                dbgVerifyResourceBindingsHelper(Sam, "sampler");
            },
            [&](const ImageInfo& Img)
            {
                dbgVerifyResourceBindingsHelper(Img, "image");
            },
            [&](const StorageBlockInfo& SB)
            {
                dbgVerifyResourceBindingsHelper(SB, "shader storage block");
            }
        );
    }
#endif

}
