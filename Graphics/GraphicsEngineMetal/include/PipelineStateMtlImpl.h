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

/// \file
/// Declaration of Diligent::PipelineStateMtlImpl class

#include "PipelineStateMtl.h"
#include "RenderDeviceMtl.h"
#include "PipelineStateBase.hpp"
#include "ShaderMtlImpl.h"
#include "SRBMemoryAllocator.hpp"
#include "RenderDeviceMtlImpl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::IPipelineStateMtl interface
class PipelineStateMtlImpl final : public PipelineStateBase<IPipelineStateMtl, RenderDeviceMtlImpl>
{
public:
    using TPipelineStateBase = PipelineStateBase<IPipelineStateMtl, RenderDeviceMtlImpl>;

    PipelineStateMtlImpl(IReferenceCounters*        pRefCounters,
                         class RenderDeviceMtlImpl* pDeviceMtl,
                         const PipelineStateDesc&   PipelineDesc);
    ~PipelineStateMtlImpl();

    virtual void QueryInterface(const Diligent::INTERFACE_ID& IID, IObject** ppInterface) override final;

    virtual void CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources) override final;

    virtual bool IsCompatibleWith(const IPipelineState* pPSO) const override final;

    virtual void BindStaticResources(Uint32 ShaderFlags, IResourceMapping* pResourceMapping, Uint32 Flags) override final
    {
        LOG_ERROR_MESSAGE("PipelineStateMtlImpl::BindStaticResources() is not implemented");
    }

    virtual Uint32 GetStaticVariableCount(SHADER_TYPE ShaderType) const override final
    {
        LOG_ERROR_MESSAGE("PipelineStateMtlImpl::GetStaticVariableCount() is not implemented");
        return 0;
    }

    virtual IShaderResourceVariable* GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final
    {
        LOG_ERROR_MESSAGE("PipelineStateMtlImpl::GetStaticShaderVariable() is not implemented");
        return nullptr;
    }

    virtual IShaderResourceVariable* GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final
    {
        LOG_ERROR_MESSAGE("PipelineStateMtlImpl::GetStaticShaderVariable() is not implemented");
        return nullptr;
    }


private:
};

} // namespace Diligent
