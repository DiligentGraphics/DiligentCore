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

#pragma once
#include "GLObjectWrapper.h"

#ifdef _DEBUG
#   define VERIFY_RESOURCE_BINDINGS
#endif

namespace Diligent
{

    class GLProgram : public GLObjectWrappers::GLProgramObj
    {
    public:
        GLProgram( bool CreateObject );
        GLProgram( GLProgram&& Program );

        void LoadUniforms();

        struct GLProgramVariableBase
        {
            GLProgramVariableBase( const Char* _Name ) :
                Name( _Name )
            {}

            String Name;
            RefCntAutoPtr<IDeviceObject> pResource;
        };

        struct UniformBufferInfo : GLProgramVariableBase
        {
            UniformBufferInfo(const Char* _Name, GLint _Index) :
                GLProgramVariableBase(_Name),
                Index(_Index)
            {}

            GLuint Index;
        };
        std::vector<UniformBufferInfo>& GetUniformBlocks(){ return m_UniformBlocks; }

        struct SamplerInfo : GLProgramVariableBase
        {
            SamplerInfo(const Char* _Name, GLint _Location, GLenum _Type) :
                GLProgramVariableBase(_Name),
                Location(_Location),
                Type(_Type)
            {}
            GLint Location;
            GLenum Type;
        };
        std::vector<SamplerInfo>& GetSamplers(){ return m_Samplers; }
        
        struct ImageInfo : GLProgramVariableBase
        {
            ImageInfo(const Char* _Name, GLint _BindingPoint, GLenum _Type) :
                GLProgramVariableBase(_Name),
                BindingPoint(_BindingPoint),
                Type(_Type)
            {}

            GLint BindingPoint;
            GLenum Type;
        };
        std::vector<ImageInfo>& GetImages(){ return m_Images; }

        struct StorageBlockInfo : GLProgramVariableBase
        {
            StorageBlockInfo(const Char* _Name, GLint _Binding) :
                GLProgramVariableBase(_Name),
                Binding(_Binding)
            {}

            GLint Binding;
        };
        std::vector<StorageBlockInfo>& GetStorageBlocks(){ return m_StorageBlocks; }

        void BindResources(IResourceMapping *pResourceMapping, Uint32 Flags);

#ifdef VERIFY_RESOURCE_BINDINGS
        void dbgVerifyResourceBindings();
#endif

    private:
        GLProgram( const GLProgram& Program );
        const GLProgram& operator = (const GLProgram& Program);
        std::vector<UniformBufferInfo> m_UniformBlocks;
        std::vector< SamplerInfo > m_Samplers;
        std::vector< ImageInfo > m_Images;
        std::vector< StorageBlockInfo > m_StorageBlocks;
        // When adding new member DO NOT FORGET TO UPDATE GLProgram( GLProgram&& Program )!!!
    };
}
