/*     Copyright 2019 Diligent Graphics LLC
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

#include <vector>
#include "PipelineStateGL.h"
#include "PipelineStateBase.h"
#include "RenderDevice.h"
#include "GLObjectWrapper.h"
#include "GLContext.h"
#include "RenderDeviceGLImpl.h"
#include "GLProgramResources.h"
#include "GLPipelineResourceLayout.h"
#include "GLProgramResourceCache.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Pipeline state object implementation in OpenGL backend.
class PipelineStateGLImpl final : public PipelineStateBase<IPipelineStateGL, RenderDeviceGLImpl>
{
public:
    using TPipelineStateBase = PipelineStateBase<IPipelineStateGL, RenderDeviceGLImpl>;

    PipelineStateGLImpl(IReferenceCounters*      pRefCounters,
                        RenderDeviceGLImpl*      pDeviceGL,
                        const PipelineStateDesc& PipelineDesc,
                        bool                     IsDeviceInternal = false);
    ~PipelineStateGLImpl();
    
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override;

    /// Implementation of IPipelineState::BindStaticResources() in OpenGL backend.
    virtual void BindStaticResources(Uint32 ShaderFlags, IResourceMapping* pResourceMapping, Uint32 Flags)override final;
    
    /// Implementation of IPipelineState::GetStaticVariableCount() in OpenGL backend.
    virtual Uint32 GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    /// Implementation of IPipelineState::GetStaticVariableByName() in OpenGL backend.
    virtual IShaderResourceVariable* GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    /// Implementation of IPipelineState::GetStaticVariableByIndex() in OpenGL backend.
    virtual IShaderResourceVariable* GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    /// Implementation of IPipelineState::CreateShaderResourceBinding() in OpenGL backend.
    virtual void CreateShaderResourceBinding( IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources )override final;

    /// Implementation of IPipelineState::IsCompatibleWith() in OpenGL backend.
    virtual bool IsCompatibleWith(const IPipelineState* pPSO)const override final;

    void CommitProgram(GLContextState& State);
    
    void InitializeSRBResourceCache(GLProgramResourceCache& ResourceCache)const;

    const GLPipelineResourceLayout& GetResourceLayout()const {return m_ResourceLayout;}
    const GLPipelineResourceLayout& GetStaticResourceLayout()const {return m_StaticResourceLayout;}
    const GLProgramResourceCache&   GetStaticResourceCache()const {return m_StaticResourceCache;}

private:
    GLObjectWrappers::GLPipelineObj& GetGLProgramPipeline(GLContext::NativeGLContextType Context);
    void InitStaticSamplersInResourceCache(const GLPipelineResourceLayout& ResourceLayout, GLProgramResourceCache& Cache)const;

    // Linked GL programs for every shader stage. Every pipeline needs to have its own programs
    // because resource bindings assigned by GLProgramResources::LoadUniforms depend on other
    // shader stages.
    std::vector<GLObjectWrappers::GLProgramObj> m_GLPrograms;

    ThreadingTools::LockFlag m_ProgPipelineLockFlag;
    std::vector< std::pair<GLContext::NativeGLContextType, GLObjectWrappers::GLPipelineObj > > m_GLProgPipelines;

    // Resource layout that keeps variables of all types, but does not reference a
    // resource cache.
    // This layout is used by SRB objects to initialize only mutable and dynamic variables and by
    // DeviceContextGLImpl::BindProgramResources to verify resource bindings.
    GLPipelineResourceLayout m_ResourceLayout;

    // Resource layout that only keeps static variables
    GLPipelineResourceLayout m_StaticResourceLayout;
    // Resource cache for static resource variables only
    GLProgramResourceCache   m_StaticResourceCache;

    // Program resources for all shader stages in the pipeline
    std::vector<GLProgramResources> m_ProgramResources;

    Uint32  m_TotalUniformBufferBindings = 0;   
    Uint32  m_TotalSamplerBindings       = 0;
    Uint32  m_TotalImageBindings         = 0;
    Uint32  m_TotalStorageBufferBindings = 0;

    std::vector<RefCntAutoPtr<ISampler>> m_StaticSamplers;
};

}
