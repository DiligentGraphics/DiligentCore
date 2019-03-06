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
#include "GLObjectWrapper.h"
#include "HashUtils.h"
#include "ShaderBase.h"
#include "SamplerGLImpl.h"
#include "HashUtils.h"
#include "ShaderResourceVariableBase.h"

#ifdef _DEBUG
#   define VERIFY_RESOURCE_BINDINGS
#endif

namespace Diligent
{
    class GLProgramResources
    {
    public:
        GLProgramResources(){}
        GLProgramResources             (GLProgramResources&& Program)noexcept;

        GLProgramResources             (const GLProgramResources&)  = delete;
        GLProgramResources& operator = (const GLProgramResources&)  = delete;
        GLProgramResources& operator = (      GLProgramResources&&) = delete;

        void LoadUniforms(class RenderDeviceGLImpl*             pDeviceGLImpl,
                          SHADER_TYPE                           ShaderStages,
                          GLuint                                GLProgram, 
                          const PipelineResourceLayoutDesc*     pResourceLayout,
                          const SHADER_RESOURCE_VARIABLE_TYPE*  AllowedVarTypes, 
                          Uint32                                NumAllowedTypes);


        void Clone(class RenderDeviceGLImpl*             pDeviceGLImpl, 
                   IObject&                              Owner,
                   const GLProgramResources&             SrcResources, 
                   const PipelineResourceLayoutDesc&     ResourceLayout,
                   const SHADER_RESOURCE_VARIABLE_TYPE*  AllowedVarTypes, 
                   Uint32                                NumAllowedTypes);

        struct GLProgramVariableBase
        {
            GLProgramVariableBase(String                        _Name, 
                                  size_t                        _ArraySize,
                                  SHADER_RESOURCE_VARIABLE_TYPE _VarType,
                                  SHADER_RESOURCE_TYPE          _ResourceType) :
                Name        ( std::move(_Name) ),
                pResources  (_ArraySize),
                VarType     (_VarType),
                ResourceType(_ResourceType)
            {
                VERIFY_EXPR(_ArraySize >= 1);
            }

            bool IsCompatibleWith(const GLProgramVariableBase &Var)const
            {
                return VarType           == Var.VarType && 
                       pResources.size() == Var.pResources.size();
            }

            size_t GetHash()const
            {
                return ComputeHash(static_cast<Int32>(VarType), pResources.size());
            }

            ShaderResourceDesc GetResourceDesc()const
            {
                ShaderResourceDesc ResourceDesc;
                ResourceDesc.Name      = Name.c_str();
                ResourceDesc.ArraySize = static_cast<Uint32>(pResources.size());
                ResourceDesc.Type      = ResourceType;
                return ResourceDesc;
            }

            String                                      Name;
            std::vector< RefCntAutoPtr<IDeviceObject> > pResources;
            const SHADER_RESOURCE_VARIABLE_TYPE         VarType;
            const SHADER_RESOURCE_TYPE                  ResourceType;
        };

        struct UniformBufferInfo : GLProgramVariableBase
        {
            UniformBufferInfo(String                        _Name,
                              size_t                        _ArraySize,
                              SHADER_RESOURCE_VARIABLE_TYPE _VarType,
                              SHADER_RESOURCE_TYPE          _ResourceType,
                              GLint                         _Index) :
                GLProgramVariableBase(std::move(_Name), _ArraySize, _VarType, _ResourceType),
                Index(_Index)
            {}

            bool IsCompatibleWith(const UniformBufferInfo& UBI)const
            {
                return Index == UBI.Index &&
                       GLProgramVariableBase::IsCompatibleWith(UBI);
            }

            size_t GetHash()const
            {
                return ComputeHash(Index, GLProgramVariableBase::GetHash());
            }

            const GLuint Index;
        };
        std::vector<UniformBufferInfo>& GetUniformBlocks(){ return m_UniformBlocks; }

        struct SamplerInfo : GLProgramVariableBase
        {
            SamplerInfo(String                        _Name,
                        size_t                        _ArraySize,
                        SHADER_RESOURCE_VARIABLE_TYPE _VarType,
                        SHADER_RESOURCE_TYPE          _ResourceType,
                        GLint                         _Location,
                        GLenum                        _Type,
                        class SamplerGLImpl*          _pStaticSampler) :
                GLProgramVariableBase(std::move(_Name), _ArraySize, _VarType, _ResourceType),
                Location      (_Location),
                Type          (_Type),
                pStaticSampler(_pStaticSampler)
            {}

            bool IsCompatibleWith(const SamplerInfo& SI)const
            {
                return Location == SI.Location &&
                       Type     == SI.Type &&
                       GLProgramVariableBase::IsCompatibleWith(SI);
            }

            size_t GetHash()const
            {
                return ComputeHash(Location, Type, GLProgramVariableBase::GetHash());
            }

            const GLint                        Location;
            const GLenum                       Type;
            RefCntAutoPtr<class SamplerGLImpl> pStaticSampler;
        };
        std::vector<SamplerInfo>& GetSamplers(){ return m_Samplers; }
        
        struct ImageInfo : GLProgramVariableBase
        {
            ImageInfo(String                        _Name,
                      size_t                        _ArraySize,
                      SHADER_RESOURCE_VARIABLE_TYPE _VarType,
                      SHADER_RESOURCE_TYPE          _ResourceType,
                      GLint                         _BindingPoint,
                      GLenum                        _Type) :
                GLProgramVariableBase(std::move(_Name), _ArraySize, _VarType, _ResourceType),
                BindingPoint(_BindingPoint),
                Type        (_Type)
            {}

            bool IsCompatibleWith(const ImageInfo& II)const
            {
                return BindingPoint == II.BindingPoint &&
                       Type         == II.Type &&
                       GLProgramVariableBase::IsCompatibleWith(II);
            }

            size_t GetHash()const
            {
                return ComputeHash(BindingPoint, Type, GLProgramVariableBase::GetHash());
            }

            const GLint  BindingPoint;
            const GLenum Type;
        };
        std::vector<ImageInfo>& GetImages(){ return m_Images; }

        struct StorageBlockInfo : GLProgramVariableBase
        {
            StorageBlockInfo(String                        _Name,
                             size_t                        _ArraySize,
                             SHADER_RESOURCE_VARIABLE_TYPE _VarType,
                            SHADER_RESOURCE_TYPE           _ResourceType,
                             GLint                         _Binding) :
                GLProgramVariableBase(std::move(_Name), _ArraySize, _VarType, _ResourceType),
                Binding(_Binding)
            {}

            bool IsCompatibleWith(const StorageBlockInfo& SBI)const
            {
                return Binding == SBI.Binding &&
                       GLProgramVariableBase::IsCompatibleWith(SBI);
            }

            size_t GetHash()const
            {
                return ComputeHash(Binding, GLProgramVariableBase::GetHash());
            }

            const GLint Binding;
        };
        std::vector<StorageBlockInfo>& GetStorageBlocks(){ return m_StorageBlocks; }


        struct CGLShaderVariable : ShaderVariableBase
        {
            CGLShaderVariable(IObject& Owner, GLProgramResources::GLProgramVariableBase& ProgVar, Uint32 _Index) :
                ShaderVariableBase(Owner),
                ProgramVar        (ProgVar),
                VariableIndex     (_Index)
            {}

            virtual void Set(IDeviceObject *pObject)override final
            {
                ProgramVar.pResources[0] = pObject;
            }

            virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
            {
                for(Uint32 i=0; i < NumElements; ++i)
                    ProgramVar.pResources[FirstElement + i] = ppObjects[i];
            }

            virtual SHADER_RESOURCE_VARIABLE_TYPE GetType()const override final
            {
                return ProgramVar.VarType;
            }

            virtual Uint32 GetArraySize()const override final
            {
                return static_cast<Uint32>(ProgramVar.pResources.size());
            }

            virtual const Char* GetName()const override final
            {
                return ProgramVar.Name.c_str();
            }

            virtual Uint32 GetIndex()const override final
            {
                return VariableIndex;
            }

            ShaderResourceDesc GetResourceDesc() const 
            {
                return ProgramVar.GetResourceDesc();
            }
        private:
            GLProgramVariableBase& ProgramVar;
            const Uint32 VariableIndex;
        };

        void BindResources(IResourceMapping *pResourceMapping, Uint32 Flags);

#ifdef VERIFY_RESOURCE_BINDINGS
        void dbgVerifyResourceBindings();
#endif

        CGLShaderVariable* GetShaderVariable(const Char* Name);
        CGLShaderVariable* GetShaderVariable(Uint32 Index)
        {
            return Index < m_VariablesByIndex.size() ? m_VariablesByIndex[Index] : nullptr;
        }
        const CGLShaderVariable* GetShaderVariable(Uint32 Index)const
        {
            return Index < m_VariablesByIndex.size() ? m_VariablesByIndex[Index] : nullptr;
        }

        const std::unordered_map<HashMapStringKey, CGLShaderVariable, HashMapStringKey::Hasher>& GetVariables(){return m_VariableHash;}
        
        Uint32 GetVariableCount()const
        {
            return static_cast<Uint32>(m_VariableHash.size());
        }

        bool IsCompatibleWith(const GLProgramResources& Res)const;
        size_t GetHash()const;

        void InitVariables(IObject &Owner);

        SHADER_TYPE GetShaderStages() const {return m_ShaderStages;}

    private:
        // There could be more than one stage is using non-separable programs
        SHADER_TYPE                    m_ShaderStages = SHADER_TYPE_UNKNOWN; 

        std::vector<UniformBufferInfo> m_UniformBlocks;
        std::vector<SamplerInfo>       m_Samplers;
        std::vector<ImageInfo>         m_Images;
        std::vector<StorageBlockInfo>  m_StorageBlocks;
        
        /// Hash map to look up shader variables by name.
        std::unordered_map<HashMapStringKey, CGLShaderVariable, HashMapStringKey::Hasher> m_VariableHash;
        std::vector<CGLShaderVariable*>                                                   m_VariablesByIndex;
        // When adding new member DO NOT FORGET TO UPDATE GLProgramResources( GLProgramResources&& ProgramResources )!!!
    };
}
