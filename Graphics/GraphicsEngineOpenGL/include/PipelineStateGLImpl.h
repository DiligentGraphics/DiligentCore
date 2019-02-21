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

#include "PipelineStateGL.h"
#include "PipelineStateBase.h"
#include "RenderDevice.h"
#include "GLProgram.h"
#include "GLObjectWrapper.h"
#include "GLContext.h"
#include "RenderDeviceGLImpl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Implementation of the Diligent::IPipelineStateGL interface
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
    virtual void QueryInterface( const INTERFACE_ID& IID, IObject** ppInterface )override;

    virtual void BindShaderResources( IResourceMapping* pResourceMapping, Uint32 Flags )override final;
    
    virtual void CreateShaderResourceBinding( IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources )override final;

    virtual bool IsCompatibleWith(const IPipelineState* pPSO)const override final;

    GLProgram& GetGLProgram(){return m_GLProgram;}
    GLObjectWrappers::GLPipelineObj& GetGLProgramPipeline(GLContext::NativeGLContextType Context);

private:
    void LinkGLProgram(bool bIsProgramPipelineSupported);

    GLProgram m_GLProgram;
    ThreadingTools::LockFlag m_ProgPipelineLockFlag;
    std::unordered_map<GLContext::NativeGLContextType, GLObjectWrappers::GLPipelineObj> m_GLProgPipelines;
};

}
