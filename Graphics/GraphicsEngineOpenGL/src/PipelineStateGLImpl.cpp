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
#include "PipelineStateGLImpl.h"
#include "RenderDeviceGLImpl.h"
#include "ShaderGLImpl.h"
#include "ShaderResourceBindingGLImpl.h"
#include "EngineMemory.h"

namespace Diligent
{

PipelineStateGLImpl::PipelineStateGLImpl(IReferenceCounters*      pRefCounters,
                                         RenderDeviceGLImpl*      pDeviceGL,
                                         const PipelineStateDesc& PipelineDesc,
                                         bool                     bIsDeviceInternal) : 
    TPipelineStateBase(pRefCounters, pDeviceGL, PipelineDesc, bIsDeviceInternal),
    m_GLProgram(false)
{

    auto &DeviceCaps = pDeviceGL->GetDeviceCaps();
    VERIFY( DeviceCaps.DevType != DeviceType::Undefined, "Device caps are not initialized" );
    bool bIsProgramPipelineSupported = DeviceCaps.bSeparableProgramSupported;

    LinkGLProgram(bIsProgramPipelineSupported);
}

void PipelineStateGLImpl::LinkGLProgram(bool bIsProgramPipelineSupported)
{
    if (bIsProgramPipelineSupported)
    {
        // Program pipelines are not shared between GL contexts, so we cannot create
        // it now
        m_ShaderResourceLayoutHash = 0;
        for (Uint32 Shader = 0; Shader < m_NumShaders; ++Shader)
        {
            auto* pShaderGL = GetShader<ShaderGLImpl>(Shader);
            HashCombine(m_ShaderResourceLayoutHash, pShaderGL->m_GlProgObj.GetAllResources().GetHash());
        }
    }
    else
    {
        // Create new progam
        m_GLProgram.Create();
        for (Uint32 Shader = 0; Shader < m_NumShaders; ++Shader)
        {
            auto* pCurrShader = GetShader<ShaderGLImpl>(Shader);
            glAttachShader(m_GLProgram, pCurrShader->m_GLShaderObj);
            CHECK_GL_ERROR("glAttachShader() failed");
        }
        glLinkProgram(m_GLProgram);
        CHECK_GL_ERROR("glLinkProgram() failed");
        int IsLinked = GL_FALSE;
        glGetProgramiv(m_GLProgram, GL_LINK_STATUS, &IsLinked);
        CHECK_GL_ERROR("glGetProgramiv() failed");
        if (!IsLinked)
        {
            int LengthWithNull = 0, Length = 0;
            // Notice that glGetProgramiv is used to get the length for a shader program, not glGetShaderiv.
            // The length of the info log includes a null terminator.
            glGetProgramiv(m_GLProgram, GL_INFO_LOG_LENGTH, &LengthWithNull);

            // The maxLength includes the NULL character
            std::vector<char> shaderProgramInfoLog(LengthWithNull);

            // Notice that glGetProgramInfoLog  is used, not glGetShaderInfoLog.
            glGetProgramInfoLog(m_GLProgram, LengthWithNull, &Length, shaderProgramInfoLog.data());
            VERIFY(Length == LengthWithNull-1, "Incorrect program info log len");
            LOG_ERROR_MESSAGE("Failed to link shader program:\n", shaderProgramInfoLog.data(), '\n');
            UNEXPECTED("glLinkProgram failed");
        }
            
        // Detach shaders from the program object
        for (Uint32 Shader = 0; Shader < m_NumShaders; ++Shader)
        {
            auto* pCurrShader = GetShader<ShaderGLImpl>(Shader);
            glDetachShader(m_GLProgram, pCurrShader->m_GLShaderObj);
            CHECK_GL_ERROR("glDetachShader() failed");
        }

        std::vector<ShaderVariableDesc> MergedVarTypesArray;
        std::vector<StaticSamplerDesc> MergedStSamArray;
        SHADER_VARIABLE_TYPE DefaultVarType = SHADER_VARIABLE_TYPE_STATIC;
        for (Uint32 Shader = 0; Shader < m_NumShaders; ++Shader)
        {
            auto* pCurrShader = GetShader<ShaderGLImpl>(Shader);
            const auto& Desc = pCurrShader->GetDesc();
            if (Shader == 0)
            {
                DefaultVarType = Desc.DefaultVariableType;
            }
            else
            {
                if (DefaultVarType != Desc.DefaultVariableType)
                {
                    LOG_ERROR_MESSAGE("When separate shader programs are not available, all shaders linked into a program "
                                      "must use the same default variable type.");
                }
            }

            for (Uint32 v = 0; v < Desc.NumVariables; ++v)
            {
                const auto& NewVar = Desc.VariableDesc[v];
                bool VarExists = false;
                for (const auto& Var : MergedVarTypesArray)
                {
                    if (strcmp(Var.Name, NewVar.Name) == 0)
                    {
                        VarExists = true;
                        if (Var.Type != NewVar.Type)
                        {
                            LOG_ERROR_MESSAGE("The type of variable '", Var.Name, "' is not consistent between shader stages. "
                                              "When separate shader programs are not available, all shaders must use the same "
                                              "type for identically named shader variables.");
                        }
                        break;
                    }
                }

                if (!VarExists)
                {
                    MergedVarTypesArray.push_back(NewVar);
                }
            }

            for (Uint32 s = 0; s < Desc.NumStaticSamplers; ++s)
            {
                const auto& NewSampler = Desc.StaticSamplers[s];
                bool SamplerExists = false;
                for (const auto& Sampler : MergedStSamArray)
                {
                    if (strcmp(Sampler.SamplerOrTextureName, NewSampler.SamplerOrTextureName) == 0)
                    {
                        SamplerExists = true;
                        if ( !(Sampler.Desc == NewSampler.Desc) )
                        {
                            LOG_ERROR_MESSAGE("Static sampler defined for texture '", NewSampler.SamplerOrTextureName, "' is not consistent between shader stages. "
                                              "When separate shader programs are not available, all shaders must use the same "
                                              "static samplers for identically named shader variables.");
                        }
                        break;
                    }
                }

                if (!SamplerExists)
                {
                    MergedStSamArray.push_back(NewSampler);
                }
            }
        }

        auto pDeviceGL = GetDevice();
        m_GLProgram.InitResources(pDeviceGL, DefaultVarType, MergedVarTypesArray.data(), static_cast<Uint32>(MergedVarTypesArray.size()), MergedStSamArray.data(), static_cast<Uint32>(MergedStSamArray.size()), *this);

        m_ShaderResourceLayoutHash = m_GLProgram.GetAllResources().GetHash();
    }   
}


PipelineStateGLImpl::~PipelineStateGLImpl()
{
    GetDevice()->OnDestroyPSO(this);
}

IMPLEMENT_QUERY_INTERFACE( PipelineStateGLImpl, IID_PipelineStateGL, TPipelineStateBase )

void PipelineStateGLImpl::BindShaderResources(IResourceMapping* pResourceMapping, Uint32 Flags)
{
    if (GetDevice()->GetDeviceCaps().bSeparableProgramSupported)
    {
        TPipelineStateBase::BindShaderResources(pResourceMapping, Flags);
    }
    else
    {
        if (m_GLProgram)
            m_GLProgram.BindConstantResources(pResourceMapping, Flags);
    }
}

void PipelineStateGLImpl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources)
{
    auto* pRenderDeviceGL = GetDevice();
    auto& SRBAllocator = pRenderDeviceGL->GetSRBAllocator();
    auto pResBinding = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingGLImpl instance", ShaderResourceBindingGLImpl)(this);
    if (InitStaticResources)
        pResBinding->InitializeStaticResources(this);
    pResBinding->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
}

bool PipelineStateGLImpl::IsCompatibleWith(const IPipelineState* pPSO)const
{
    VERIFY_EXPR(pPSO != nullptr);

    if (pPSO == this)
        return true;

    const PipelineStateGLImpl* pPSOGL = ValidatedCast<const PipelineStateGLImpl>(pPSO);
    if (m_ShaderResourceLayoutHash != pPSOGL->m_ShaderResourceLayoutHash)
        return false;

    return m_GLProgram.GetAllResources().IsCompatibleWith(pPSOGL->m_GLProgram.GetAllResources());
}

GLObjectWrappers::GLPipelineObj& PipelineStateGLImpl::GetGLProgramPipeline(GLContext::NativeGLContextType Context)
{
    ThreadingTools::LockHelper Lock(m_ProgPipelineLockFlag);
    auto it = m_GLProgPipelines.find(Context);
    if (it != m_GLProgPipelines.end())
        return it->second;
    else
    {
        // Create new progam pipeline
        it = m_GLProgPipelines.emplace(Context, true).first;
        GLuint Pipeline = it->second;
        for (Uint32 Shader = 0; Shader < m_NumShaders; ++Shader)
        {
            auto* pCurrShader = GetShader<ShaderGLImpl>(Shader);
            auto GLShaderBit = ShaderTypeToGLShaderBit(pCurrShader->GetDesc().ShaderType);
            // If the program has an active code for each stage mentioned in set flags,
            // then that code will be used by the pipeline. If program is 0, then the given
            // stages are cleared from the pipeline.
            glUseProgramStages(Pipeline, GLShaderBit, pCurrShader->m_GlProgObj);
            CHECK_GL_ERROR("glUseProgramStages() failed");
        }
        return it->second;
    }
}

}
