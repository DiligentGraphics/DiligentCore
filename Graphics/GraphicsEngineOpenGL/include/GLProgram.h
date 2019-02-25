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
#include "GLProgramResources.h"

namespace Diligent
{
    class GLProgram : public GLObjectWrappers::GLProgramObj
    {
    public:
        GLProgram(bool CreateObject);
        GLProgram(GLProgram&& Program);

        GLProgram             (const GLProgram&)  = delete;
        GLProgram& operator = (const GLProgram&)  = delete;
        GLProgram& operator = (      GLProgram&&) = delete;

        void InitResources(RenderDeviceGLImpl*       pDeviceGLImpl, 
                           SHADER_VARIABLE_TYPE      DefaultVariableType, 
                           const ShaderVariableDesc* VariableDesc, 
                           Uint32                    NumVars, 
                           const StaticSamplerDesc*  StaticSamplers,
                           Uint32                    NumStaticSamplers,
                           IObject&                  Owner);

        void BindConstantResources(IResourceMapping* pResourceMapping, Uint32 Flags);

        const GLProgramResources& GetAllResources()     const{return m_AllResources;}
              GLProgramResources& GetConstantResources()     {return m_ConstantResources;}
        const GLProgramResources& GetConstantResources()const{return m_ConstantResources;}
        

#ifdef VERIFY_RESOURCE_BINDINGS
        template<typename TResArrayType>
        void dbgVerifyBindingCompletenessHelper(TResArrayType& ResArr, GLProgramResources* pDynamicResources);
        void dbgVerifyBindingCompleteness(GLProgramResources* pDynamicResources, class PipelineStateGLImpl* pPSO);
#endif

    private:
        GLProgramResources m_AllResources;
        GLProgramResources m_ConstantResources;
        // When adding new member DO NOT FORGET TO UPDATE GLProgram( GLProgram&& Program )!!!
    };
}
