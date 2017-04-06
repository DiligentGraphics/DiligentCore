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
#include "GLObjectWrapper.h"
#include "HashUtils.h"
#include "ShaderBase.h"
#include "SamplerGLImpl.h"

#ifdef _DEBUG
#   define VERIFY_RESOURCE_BINDINGS
#endif

namespace Diligent
{
    class GLProgramResources
    {
    public:
        GLProgramResources();
        GLProgramResources( GLProgramResources&& Program );

        void LoadUniforms(class RenderDeviceGLImpl *pDeviceGLImpl,
                          GLuint GLProgram, 
                          const SHADER_VARIABLE_TYPE DefaultVariableType, 
                          const ShaderVariableDesc *VariableDesc, 
                          Uint32 NumVars,
                          const StaticSamplerDesc *StaticSamplers,
                          Uint32 NumStaticSamplers);

        void Clone(const GLProgramResources& SrcLayout, 
                   SHADER_VARIABLE_TYPE *VarTypes, 
                   Uint32 NumVarTypes,
                   IObject &Owner);

        struct GLProgramVariableBase
        {
            GLProgramVariableBase( const Char* _Name, size_t _ArraySize, SHADER_VARIABLE_TYPE _VarType) :
                Name( _Name ),
                VarType(_VarType),
                pResources(_ArraySize)
            {
                VERIFY_EXPR(_ArraySize >= 1);
            }

            String Name;
            std::vector< RefCntAutoPtr<IDeviceObject> > pResources;
            SHADER_VARIABLE_TYPE VarType;
        };

        struct UniformBufferInfo : GLProgramVariableBase
        {
            UniformBufferInfo(const Char* _Name, size_t _ArraySize, SHADER_VARIABLE_TYPE _VarType, GLint _Index) :
                GLProgramVariableBase(_Name, _ArraySize, _VarType),
                Index(_Index)
            {}

            GLuint Index;
        };
        std::vector<UniformBufferInfo>& GetUniformBlocks(){ return m_UniformBlocks; }

        struct SamplerInfo : GLProgramVariableBase
        {
            SamplerInfo(const Char* _Name, size_t _ArraySize, SHADER_VARIABLE_TYPE _VarType, GLint _Location, GLenum _Type, class SamplerGLImpl *_pStaticSampler) :
                GLProgramVariableBase(_Name, _ArraySize, _VarType),
                Location(_Location),
                Type(_Type),
                pStaticSampler(_pStaticSampler)
            {}
            GLint Location;
            GLenum Type;
            RefCntAutoPtr<class SamplerGLImpl> pStaticSampler;
        };
        std::vector<SamplerInfo>& GetSamplers(){ return m_Samplers; }
        
        struct ImageInfo : GLProgramVariableBase
        {
            ImageInfo(const Char* _Name, size_t _ArraySize, SHADER_VARIABLE_TYPE _VarType, GLint _BindingPoint, GLenum _Type) :
                GLProgramVariableBase(_Name, _ArraySize, _VarType),
                BindingPoint(_BindingPoint),
                Type(_Type)
            {}

            GLint BindingPoint;
            GLenum Type;
        };
        std::vector<ImageInfo>& GetImages(){ return m_Images; }

        struct StorageBlockInfo : GLProgramVariableBase
        {
            StorageBlockInfo(const Char* _Name, size_t _ArraySize, SHADER_VARIABLE_TYPE _VarType, GLint _Binding) :
                GLProgramVariableBase(_Name, _ArraySize, _VarType),
                Binding(_Binding)
            {}

            GLint Binding;
        };
        std::vector<StorageBlockInfo>& GetStorageBlocks(){ return m_StorageBlocks; }


        struct CGLShaderVariable : ShaderVariableBase
        {
            CGLShaderVariable( IObject &Owner, GLProgramResources::GLProgramVariableBase &ProgVar ) :
                ShaderVariableBase( Owner ),
                ProgramVar(ProgVar)
            {}

            virtual void Set( IDeviceObject *pObject )override final
            {
                ProgramVar.pResources[0] = pObject;
            }

            virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
            {
                for(Uint32 i=0; i < NumElements; ++i)
                    ProgramVar.pResources[FirstElement + i] = ppObjects[i];
            }

        private:
            GLProgramVariableBase &ProgramVar;
        };

        void BindResources(IResourceMapping *pResourceMapping, Uint32 Flags);

#ifdef VERIFY_RESOURCE_BINDINGS
        void dbgVerifyResourceBindings();
#endif

        IShaderVariable* GetShaderVariable( const Char* Name );

        const std::unordered_map<HashMapStringKey, CGLShaderVariable>& GetVariables(){return m_VariableHash;}

    private:
        const GLProgramResources& operator = (const GLProgramResources& Program);

        void InitVariables(IObject &Owner);

        std::vector<UniformBufferInfo> m_UniformBlocks;
        std::vector< SamplerInfo > m_Samplers;
        std::vector< ImageInfo > m_Images;
        std::vector< StorageBlockInfo > m_StorageBlocks;
        
        /// Hash map to look up shader variables by name.
        std::unordered_map<HashMapStringKey, CGLShaderVariable> m_VariableHash;
        // When adding new member DO NOT FORGET TO UPDATE GLProgramResources( GLProgramResources&& ProgramResources )!!!
    };
}
