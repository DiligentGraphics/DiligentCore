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

#include "PipelineStateMtlImpl.h"
#include "RenderDeviceMtlImpl.h"
#include "ShaderResourceBindingMtlImpl.h"
#include "EngineMemory.h"

namespace Diligent
{

PipelineStateMtlImpl::PipelineStateMtlImpl(IReferenceCounters*      pRefCounters,
                                           RenderDeviceMtlImpl*     pRenderDeviceMtl,
                                           const PipelineStateDesc& PipelineDesc) : 
    TPipelineStateBase(pRefCounters, pRenderDeviceMtl, PipelineDesc)
{
    LOG_ERROR_AND_THROW("Pipeline states are not implemented in Metal backend");
    if (PipelineDesc.IsComputePipeline)
    {

    }
    else
    {

    }
}


PipelineStateMtlImpl::~PipelineStateMtlImpl()
{
}

IMPLEMENT_QUERY_INTERFACE( PipelineStateMtlImpl, IID_PipelineStateMtl, TPipelineStateBase )


void PipelineStateMtlImpl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources)
{
    auto* pRenderDeviceMtl = ValidatedCast<RenderDeviceMtlImpl>( GetDevice() );

    (void)pRenderDeviceMtl;
    LOG_ERROR_MESSAGE("PipelineStateMtlImpl::CreateShaderResourceBinding() is not implemented");
}

bool PipelineStateMtlImpl::IsCompatibleWith(const IPipelineState* pPSO)const
{
    VERIFY_EXPR(pPSO != nullptr);

    if (pPSO == this)
        return true;
    
    const PipelineStateMtlImpl* pPSOMtl = ValidatedCast<const PipelineStateMtlImpl>(pPSO);

    (void)pPSOMtl;
    LOG_ERROR_MESSAGE("PipelineStateMtlImpl::IsCompatibleWith() is not implemented");
    
    return false;
}

}
