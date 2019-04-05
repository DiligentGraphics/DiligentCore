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
    TBase  (pRefCounters, pPSO),
    m_Resources(pPSO->GetGLProgram() == 0 ? pPSO->GetNumShaders() : 1)
{
    const SHADER_RESOURCE_VARIABLE_TYPE SRBVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};
    if (IsUsingSeparatePrograms())
    {
        for (Uint32 s = 0; s < pPSO->GetNumShaders(); ++s)
        {
            auto* pShaderGL = pPSO->GetShader<ShaderGLImpl>(s);
            m_Resources[s].Clone(pPSO->GetDevice(), *this, pShaderGL->GetGlProgram().GetResources(), pPSO->GetDesc().ResourceLayout, SRBVarTypes, _countof(SRBVarTypes));
            const auto ShaderType = pShaderGL->GetDesc().ShaderType;
            const auto ShaderTypeInd = GetShaderTypeIndex(ShaderType);
            m_ResourceIndex[ShaderTypeInd] = static_cast<Int8>(s);
        }
    }
    else
    {
        m_Resources[0].Clone(pPSO->GetDevice(), *this, pPSO->GetGLProgram().GetResources(), pPSO->GetDesc().ResourceLayout, SRBVarTypes, _countof(SRBVarTypes));
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
    for(auto& Resource : m_Resources)
    {
        if ((Resource.GetShaderStages() & ShaderFlags)!=0)
            Resource.BindResources(pResMapping, Flags);
    }
}

IShaderResourceVariable* ShaderResourceBindingGLImpl::GetVariableByName(SHADER_TYPE ShaderType, const char* Name)
{
    if (IsUsingSeparatePrograms())
    {
        auto ShaderInd =  m_ResourceIndex[GetShaderTypeIndex(ShaderType)];
        return ShaderInd >= 0 ? m_Resources[ShaderInd].GetVariable(Name) : nullptr;
    }
    else
    {
        return (m_Resources[0].GetShaderStages() & ShaderType) != 0 ? m_Resources[0].GetVariable(Name) : nullptr;
    }
}

Uint32 ShaderResourceBindingGLImpl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    if (IsUsingSeparatePrograms())
    {
        auto ShaderInd =  m_ResourceIndex[GetShaderTypeIndex(ShaderType)];
        return ShaderInd >= 0 ? m_Resources[ShaderInd].GetVariableCount() : 0;
    }
    else
    {
        return (m_Resources[0].GetShaderStages() & ShaderType) != 0 ? m_Resources[0].GetVariableCount() : 0;
    }
}

IShaderResourceVariable* ShaderResourceBindingGLImpl::GetVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    if (IsUsingSeparatePrograms())
    {
        auto ShaderInd =  m_ResourceIndex[GetShaderTypeIndex(ShaderType)];
        return ShaderInd >= 0 ? m_Resources[ShaderInd].GetVariable(Index) : 0;
    }
    else
    {
        return (m_Resources[0].GetShaderStages() & ShaderType) != 0 ? m_Resources[0].GetVariable(Index) : nullptr;
    }
}

static GLProgramResources NullProgramResources;
GLProgramResources& ShaderResourceBindingGLImpl::GetResources(Uint32 Ind, PipelineStateGLImpl* pdbgPSO)
{
#ifdef _DEBUG
    if (pdbgPSO->IsIncompatibleWith(GetPipelineState()))
    {
        LOG_ERROR("Shader resource binding is incompatible with the currently bound pipeline state.");
    }
#endif
    return m_Resources[Ind];
}

void ShaderResourceBindingGLImpl::InitializeStaticResources(const IPipelineState* pPipelineState)
{
    // Do nothing
}

}
