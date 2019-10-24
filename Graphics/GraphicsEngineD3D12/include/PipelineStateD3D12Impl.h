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

/// \file
/// Declaration of Diligent::PipelineStateD3D12Impl class

#include "RenderDeviceD3D12.h"
#include "PipelineStateD3D12.h"
#include "PipelineStateBase.h"
#include "RootSignature.h"
#include "ShaderResourceLayoutD3D12.h"
#include "SRBMemoryAllocator.h"
#include "RenderDeviceD3D12Impl.h"
#include "ShaderVariableD3D12.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::IRenderDeviceD3D12 interface
class PipelineStateD3D12Impl final : public PipelineStateBase<IPipelineStateD3D12, RenderDeviceD3D12Impl>
{
public:
    using TPipelineStateBase = PipelineStateBase<IPipelineStateD3D12, RenderDeviceD3D12Impl>;

    PipelineStateD3D12Impl( IReferenceCounters *pRefCounters, RenderDeviceD3D12Impl *pDeviceD3D12, const PipelineStateDesc &PipelineDesc );
    ~PipelineStateD3D12Impl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override final;
   
    virtual void BindStaticResources(Uint32 ShaderFlags, IResourceMapping* pResourceMapping, Uint32 Flags)override final;

    virtual Uint32 GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    virtual IShaderResourceVariable* GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    virtual IShaderResourceVariable* GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    virtual void CreateShaderResourceBinding( IShaderResourceBinding **ppShaderResourceBinding, bool InitStaticResources )override final;

    virtual bool IsCompatibleWith(const IPipelineState *pPSO)const override final;

    virtual ID3D12PipelineState *GetD3D12PipelineState()const override final{return m_pd3d12PSO;}

    virtual ID3D12RootSignature *GetD3D12RootSignature()const override final{return m_RootSig.GetD3D12RootSignature(); }

    struct CommitAndTransitionResourcesAttribs
    {
        Uint32                        CtxId                  = 0;
        IShaderResourceBinding*       pShaderResourceBinding = nullptr;
        bool                          CommitResources        = false;
        bool                          TransitionResources    = false;
        bool                          ValidateStates         = false;
    };
    ShaderResourceCacheD3D12* CommitAndTransitionShaderResources(class DeviceContextD3D12Impl*        pDeviceCtx,
                                                                 class CommandContext&                CmdCtx,
                                                                 CommitAndTransitionResourcesAttribs& Attrib)const;
    
    const RootSignature& GetRootSignature()const{return m_RootSig;}
    
    const ShaderResourceLayoutD3D12& GetShaderResLayout(Uint32 ShaderInd)const
    {
        VERIFY_EXPR(ShaderInd < m_NumShaders);
        return m_pShaderResourceLayouts[ShaderInd];
    }

    const ShaderResourceLayoutD3D12& GetStaticShaderResLayout(Uint32 ShaderInd)const
    {
        VERIFY_EXPR(ShaderInd < m_NumShaders);
        return m_pShaderResourceLayouts[m_NumShaders + ShaderInd];
    }
    
    ShaderResourceCacheD3D12& GetStaticShaderResCache(Uint32 ShaderInd)const
    {
        VERIFY_EXPR(ShaderInd < m_NumShaders);
        return m_pStaticResourceCaches[ShaderInd];
    }

    bool ContainsShaderResources()const;

    SRBMemoryAllocator& GetSRBMemoryAllocator()
    {
        return m_SRBMemAllocator;
    }

private:

    /// D3D12 device
    CComPtr<ID3D12PipelineState> m_pd3d12PSO;
    RootSignature m_RootSig;
    
    // Must be defined before default SRB
    SRBMemoryAllocator m_SRBMemAllocator;

    ShaderResourceLayoutD3D12*   m_pShaderResourceLayouts = nullptr;
    ShaderResourceCacheD3D12*    m_pStaticResourceCaches  = nullptr;
    ShaderVariableManagerD3D12*  m_pStaticVarManagers     = nullptr;
    // Resource layout index in m_ResourceLayouts[] array for every shader stage
    Int8 m_ResourceLayoutIndex[6] = {-1, -1, -1, -1, -1, -1};
};

}
