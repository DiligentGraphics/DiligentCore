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

#include "ShaderResourceBindingMtlImpl.h"
#include "PipelineStateMtlImpl.h"
#include "DeviceContextMtlImpl.h"
#include "RenderDeviceMtlImpl.h"

namespace Diligent
{


ShaderResourceBindingMtlImpl::ShaderResourceBindingMtlImpl( IReferenceCounters*   pRefCounters,
                                                            PipelineStateMtlImpl* pPSO,
                                                            bool                  IsInternal) :
    TBase( pRefCounters, pPSO, IsInternal )
{
    LOG_ERROR_AND_THROW("Shader resource binding is not implemented in Metal backend");
}

ShaderResourceBindingMtlImpl::~ShaderResourceBindingMtlImpl()
{

}

IMPLEMENT_QUERY_INTERFACE( ShaderResourceBindingMtlImpl, IID_ShaderResourceBindingMtl, TBase )

void ShaderResourceBindingMtlImpl::BindResources(Uint32 ShaderFlags, IResourceMapping *pResMapping, Uint32 Flags)
{
    LOG_ERROR_MESSAGE("ShaderResourceBindingMtlImpl::BindResources() is not implemented");
}

void ShaderResourceBindingMtlImpl::InitializeStaticResources(const IPipelineState* pPipelineState)
{
    LOG_ERROR_MESSAGE("ShaderResourceBindingMtlImpl::InitializeStaticResources() is not implemented");
}

IShaderResourceVariable* ShaderResourceBindingMtlImpl::GetVariableByName(SHADER_TYPE ShaderType, const char* Name)
{
    LOG_ERROR_MESSAGE("ShaderResourceBindingMtlImpl::GetVariable() is not implemented");
    return nullptr;
}

Uint32 ShaderResourceBindingMtlImpl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    LOG_ERROR_MESSAGE("ShaderResourceBindingMtlImpl::GetVariableCount() is not implemented");
    return 0;
}

IShaderResourceVariable* ShaderResourceBindingMtlImpl::GetVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    LOG_ERROR_MESSAGE("ShaderResourceBindingMtlImpl::GetVariable() is not implemented");
    return nullptr;
}

}
