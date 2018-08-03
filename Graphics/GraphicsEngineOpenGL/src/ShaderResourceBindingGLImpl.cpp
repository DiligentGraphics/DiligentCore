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
#include "ShaderResourceBindingGLImpl.h"
#include "PipelineStateGLImpl.h"
#include "ShaderGLImpl.h"
#include "FixedBlockMemoryAllocator.h"

namespace Diligent
{

ShaderResourceBindingGLImpl::ShaderResourceBindingGLImpl( IReferenceCounters *pRefCounters, PipelineStateGLImpl *pPSO) :
    TBase( pRefCounters, pPSO ),
    m_DummyShaderVar(*this),
    m_wpPSO(pPSO)
{
    SHADER_VARIABLE_TYPE VarTypes[] = {SHADER_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_TYPE_DYNAMIC};
    if ( static_cast<GLuint>( pPSO->GetGLProgram() ) )
    {
        m_DynamicProgResources[0].Clone(pPSO->GetGLProgram().GetAllResources(), VarTypes, _countof(VarTypes), *this);
    }
    else
    {
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
}

ShaderResourceBindingGLImpl::~ShaderResourceBindingGLImpl()
{

}

IMPLEMENT_QUERY_INTERFACE( ShaderResourceBindingGLImpl, IID_ShaderResourceBindingGL, TBase )

void ShaderResourceBindingGLImpl::BindResources(Uint32 ShaderFlags, IResourceMapping *pResMapping, Uint32 Flags)
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

IShaderVariable *ShaderResourceBindingGLImpl::GetVariable(SHADER_TYPE ShaderType, const char *Name)
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    IShaderVariable *pVar = m_DynamicProgResources[ShaderInd].GetShaderVariable(Name);
    if( !pVar )
    {
        LOG_ERROR_MESSAGE( "Shader variable \"", Name, "\" is not found in the shader resource mapping. Attempts to set the variable will be silently ignored." );
        pVar = &m_DummyShaderVar;
    }
    return pVar;
}

Uint32 ShaderResourceBindingGLImpl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    UNSUPPORTED("Not yet implemented");
    return 0;
}

IShaderVariable* ShaderResourceBindingGLImpl::GetVariable(SHADER_TYPE ShaderType, Uint32 Index)
{
    UNSUPPORTED("Not yet implemented");
    return 0;
}

static GLProgramResources NullProgramResources;
GLProgramResources &ShaderResourceBindingGLImpl::GetProgramResources(SHADER_TYPE ShaderType, PipelineStateGLImpl *pdbgPSO)
{
#ifdef _DEBUG
    auto pPSO = m_wpPSO.Lock();
    if (pdbgPSO->IsIncompatibleWith(pPSO))
    {
        LOG_ERROR("Shader resource binding is incompatible with the currently bound pipeline state.");
    }
#endif
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    return m_DynamicProgResources[ShaderInd];
}

}
