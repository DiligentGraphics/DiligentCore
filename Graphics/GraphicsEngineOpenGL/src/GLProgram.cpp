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
#include "GLProgram.h"
#include "ShaderBase.h"
#include "ShaderResourceBindingGLImpl.h"

namespace Diligent
{
    GLProgram::GLProgram( bool CreateObject ) :
        GLObjectWrappers::GLProgramObj( CreateObject )
    {}
    
    GLProgram::GLProgram( GLProgram&& Program ):
        GLObjectWrappers::GLProgramObj(std::move(Program )                   ),
        m_AllResources                (std::move(Program.m_AllResources)     ),
        m_ConstantResources           (std::move(Program.m_ConstantResources))
    {}

    void GLProgram::InitResources(RenderDeviceGLImpl* pDeviceGLImpl, 
                                  const SHADER_RESOURCE_VARIABLE_TYPE DefaultVariableType, 
                                  const ShaderResourceVariableDesc *VariableDesc, 
                                  Uint32 NumVars, 
                                  const StaticSamplerDesc *StaticSamplers,
                                  Uint32 NumStaticSamplers,
                                  IObject &Owner)
    {
        GLuint GLProgram = static_cast<GLuint>(*this);
        m_AllResources.LoadUniforms(pDeviceGLImpl, GLProgram, DefaultVariableType, VariableDesc, NumVars, StaticSamplers, NumStaticSamplers);

        SHADER_RESOURCE_VARIABLE_TYPE VarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_STATIC};
        m_ConstantResources.Clone(m_AllResources, VarTypes, _countof(VarTypes), Owner);
    }

    void GLProgram::BindConstantResources( IResourceMapping *pResourceMapping, Uint32 Flags )
    {
        if( !pResourceMapping )
            return;

        m_ConstantResources.BindResources(pResourceMapping, Flags);
    }


#ifdef VERIFY_RESOURCE_BINDINGS
    template<typename TResArrayType>
    void GLProgram::dbgVerifyBindingCompletenessHelper(TResArrayType &ResArr, GLProgramResources *pDynamicResources)
    {
        const auto &ConstVariables = m_ConstantResources.GetVariables();
        for( auto res = ResArr.begin(); res != ResArr.end(); ++res )
        {
            auto ConstRes = ConstVariables.find(HashMapStringKey(res->Name.c_str()));
            if (ConstRes == ConstVariables.end())
            {
                bool bVarFound = false;
                if( pDynamicResources)
                {
                    const auto &DynamicVariables = pDynamicResources->GetVariables();
                    auto DynRes = DynamicVariables.find(HashMapStringKey(res->Name.c_str()));
                    bVarFound = (DynRes != DynamicVariables.end());
                }

                if(!bVarFound)
                {
                    LOG_ERROR_MESSAGE( "Incomplete binding: non-static shader variable \"", res->Name, "\" not found" );
                }
            }
        }
    }

    void GLProgram::dbgVerifyBindingCompleteness(GLProgramResources *pDynamicResources, PipelineStateGLImpl *pPSO)
    {
        dbgVerifyBindingCompletenessHelper(m_AllResources.GetUniformBlocks(), pDynamicResources);
        dbgVerifyBindingCompletenessHelper(m_AllResources.GetSamplers(),      pDynamicResources);
        dbgVerifyBindingCompletenessHelper(m_AllResources.GetImages(),        pDynamicResources);
        dbgVerifyBindingCompletenessHelper(m_AllResources.GetStorageBlocks(), pDynamicResources);
    }
#endif
}
