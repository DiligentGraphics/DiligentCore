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
#include "ShaderResourceBindingGLImpl.h"
#include "PipelineStateGLImpl.h"
#include "ShaderGLImpl.h"
#include "FixedBlockMemoryAllocator.h"

namespace Diligent
{

ShaderResourceBindingGLImpl::ShaderResourceBindingGLImpl(IReferenceCounters* pRefCounters, PipelineStateGLImpl* pPSO) :
    TBase  (pRefCounters, pPSO)
{
    if (IsUsingSeparatePrograms())
    {
        SHADER_VARIABLE_TYPE VarTypes[] = {SHADER_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_TYPE_DYNAMIC};
#define INIT_SHADER(SN)\
        if(auto p##SN = ValidatedCast<ShaderGLImpl>( pPSO->Get##SN() ))     \
        {                                                                   \
            auto &GLProg = p##SN->GetGlProgram();                           \
            m_DynamicProgResources[SN##Ind].Clone(GLProg.GetAllResources(), VarTypes, _countof(VarTypes), *this); \
        }

        INIT_SHADER(VS)
        INIT_SHADER(PS)
        INIT_SHADER(GS)
        INIT_SHADER(HS)
        INIT_SHADER(DS)
        INIT_SHADER(CS)
#undef INIT_SHADER
    }
    else
    {
        // Clone all variable types
        SHADER_VARIABLE_TYPE VarTypes[] = {SHADER_VARIABLE_TYPE_STATIC, SHADER_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_TYPE_DYNAMIC};
        m_DynamicProgResources[0].Clone(pPSO->GetGLProgram().GetAllResources(), VarTypes, _countof(VarTypes), *this);
    }
}

ShaderResourceBindingGLImpl::~ShaderResourceBindingGLImpl()
{
}

IMPLEMENT_QUERY_INTERFACE(ShaderResourceBindingGLImpl, IID_ShaderResourceBindingGL, TBase)

bool ShaderResourceBindingGLImpl::IsUsingSeparatePrograms()const
{
    return GetPipelineState<PipelineStateGLImpl>()->GetGLProgram() == 0;
}

void ShaderResourceBindingGLImpl::BindResources(Uint32 ShaderFlags, IResourceMapping* pResMapping, Uint32 Flags)
{
    if (IsUsingSeparatePrograms())
    {
        if(ShaderFlags & SHADER_TYPE_VERTEX)
            m_DynamicProgResources[VSInd].BindResources(pResMapping, Flags);
        if(ShaderFlags & SHADER_TYPE_PIXEL)                        
            m_DynamicProgResources[PSInd].BindResources(pResMapping, Flags);
        if(ShaderFlags & SHADER_TYPE_GEOMETRY)                     
            m_DynamicProgResources[GSInd].BindResources(pResMapping, Flags);
        if(ShaderFlags & SHADER_TYPE_HULL)                         
            m_DynamicProgResources[HSInd].BindResources(pResMapping, Flags);
        if(ShaderFlags & SHADER_TYPE_DOMAIN)                       
            m_DynamicProgResources[DSInd].BindResources(pResMapping, Flags);
        if(ShaderFlags & SHADER_TYPE_COMPUTE)                      
            m_DynamicProgResources[CSInd].BindResources(pResMapping, Flags);
    }
    else
    {
        // Using non-separable program
        m_DynamicProgResources[0].BindResources(pResMapping, Flags);
    }
}

IShaderVariable* ShaderResourceBindingGLImpl::GetVariable(SHADER_TYPE ShaderType, const char* Name)
{
    auto ShaderInd = IsUsingSeparatePrograms() ? GetShaderTypeIndex(ShaderType) : 0;
    return m_DynamicProgResources[ShaderInd].GetShaderVariable(Name);
}

Uint32 ShaderResourceBindingGLImpl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    auto ShaderInd = IsUsingSeparatePrograms() ? GetShaderTypeIndex(ShaderType) : 0;
    return m_DynamicProgResources[ShaderInd].GetVariableCount();
}

IShaderVariable* ShaderResourceBindingGLImpl::GetVariable(SHADER_TYPE ShaderType, Uint32 Index)
{
    auto ShaderInd = IsUsingSeparatePrograms() ? GetShaderTypeIndex(ShaderType) : 0;
    return m_DynamicProgResources[ShaderInd].GetShaderVariable(Index);
}

static GLProgramResources NullProgramResources;
GLProgramResources& ShaderResourceBindingGLImpl::GetProgramResources(SHADER_TYPE ShaderType, PipelineStateGLImpl* pdbgPSO)
{
#ifdef _DEBUG
    if (pdbgPSO->IsIncompatibleWith(GetPipelineState()))
    {
        LOG_ERROR("Shader resource binding is incompatible with the currently bound pipeline state.");
    }
#endif
    auto ShaderInd = IsUsingSeparatePrograms() ? GetShaderTypeIndex(ShaderType) : 0;
    return m_DynamicProgResources[ShaderInd];
}

void ShaderResourceBindingGLImpl::InitializeStaticResources(const IPipelineState* pPipelineState)
{
    if (!IsUsingSeparatePrograms())
    {
        class ResourceMappingProxy final : public IResourceMapping
        {
        public:
            ResourceMappingProxy(const PipelineStateGLImpl& PSO) :
                m_PSO(PSO)
            {
            }
            virtual void QueryInterface (const INTERFACE_ID& IID, IObject** ppInterface)override final
            {
                UNEXPECTED("This method should never be called");
            }
            virtual CounterValueType AddRef()override final
            {
                UNEXPECTED("This method should never be called");
                return 0;
            }
            virtual CounterValueType Release()override final
            {
                UNEXPECTED("This method should never be called");
                return 0;
            }
            virtual IReferenceCounters* GetReferenceCounters()const override final
            {
                UNEXPECTED("This method should never be called");
                return nullptr;
            }
            virtual void AddResource (const Char* Name, IDeviceObject* pObject, bool bIsUnique)override final
            {
                UNEXPECTED("This method should never be called");
            }
            virtual void AddResourceArray (const Char* Name, Uint32 StartIndex, IDeviceObject* const* ppObjects, Uint32 NumElements, bool bIsUnique)override final
            {
                UNEXPECTED("This method should never be called");
            }
            virtual void RemoveResourceByName (const Char* Name, Uint32 ArrayIndex = 0)
            {
                UNEXPECTED("This method should never be called");
            }
            virtual void GetResource (const Char* Name, IDeviceObject** ppResource, Uint32 ArrayIndex = 0)
            {
                auto NumShaders = m_PSO.GetNumShaders();
                for (Uint32 s=0; s < NumShaders; ++s)
                {
                    auto* pShader = m_PSO.GetShader<ShaderGLImpl>(s);
                    auto* pVar = pShader->GetShaderVariable(Name, false);
                    if (pVar != nullptr)
                    {
                        auto* pStaticVarPlaceholder = ValidatedCast<ShaderGLImpl::StaticVarPlaceholder>(pVar);
                        *ppResource = pStaticVarPlaceholder->Get(ArrayIndex);
                        if (*ppResource != nullptr)
                            (*ppResource)->AddRef();
                    }
                }
            }
            virtual size_t GetSize()
            {
                UNEXPECTED("This method should never be called");
                return 0;
            }
        private:
            const PipelineStateGLImpl& m_PSO;
        };

        if (pPipelineState != nullptr)
        {
            const auto* PSOGL = ValidatedCast<const PipelineStateGLImpl>(pPipelineState);
            ResourceMappingProxy StaticResMapping(*PSOGL);
            m_DynamicProgResources[0].BindResources(&StaticResMapping, 0);
        }
    }
}

}
