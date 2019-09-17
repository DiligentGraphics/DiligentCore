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
/// Declaration of Diligent::PipelineStateD3D11Impl class

#include "PipelineStateD3D11.h"
#include "RenderDeviceD3D11.h"
#include "PipelineStateBase.h"
#include "ShaderD3D11Impl.h"
#include "SRBMemoryAllocator.h"
#include "RenderDeviceD3D11Impl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::IPipelineStateD3D11 interface
class PipelineStateD3D11Impl final : public PipelineStateBase<IPipelineStateD3D11, RenderDeviceD3D11Impl>
{
public:
    using TPipelineStateBase = PipelineStateBase<IPipelineStateD3D11, RenderDeviceD3D11Impl>;

    PipelineStateD3D11Impl(IReferenceCounters*          pRefCounters,
                           class RenderDeviceD3D11Impl* pDeviceD3D11,
                           const PipelineStateDesc&     PipelineDesc);
    ~PipelineStateD3D11Impl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override final;
    

    virtual void BindStaticResources(Uint32 ShaderFlags, IResourceMapping* pResourceMapping, Uint32 Flags)override final;

    virtual Uint32 GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    virtual IShaderResourceVariable* GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    virtual IShaderResourceVariable* GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    virtual void CreateShaderResourceBinding( IShaderResourceBinding **ppShaderResourceBinding, bool InitStaticResources )override final;

    virtual bool IsCompatibleWith(const IPipelineState *pPSO)const override final;


    /// Implementation of the IPipelineStateD3D11::GetD3D11BlendState() method.
    virtual ID3D11BlendState* GetD3D11BlendState()override final;

    /// Implementation of the IPipelineStateD3D11::GetD3D11RasterizerState() method.
    virtual ID3D11RasterizerState* GetD3D11RasterizerState()override final;

    /// Implementation of the IPipelineStateD3D11::GetD3D11DepthStencilState() method.
    virtual ID3D11DepthStencilState* GetD3D11DepthStencilState()override final;

    virtual ID3D11InputLayout* GetD3D11InputLayout()override final;

    virtual ID3D11VertexShader*   GetD3D11VertexShader()override final;
    virtual ID3D11PixelShader*    GetD3D11PixelShader()override final;
    virtual ID3D11GeometryShader* GetD3D11GeometryShader()override final;
    virtual ID3D11DomainShader*   GetD3D11DomainShader()override final;
    virtual ID3D11HullShader*     GetD3D11HullShader()override final;
    virtual ID3D11ComputeShader*  GetD3D11ComputeShader()override final;


    SRBMemoryAllocator& GetSRBMemoryAllocator()
    {
        return m_SRBMemAllocator;
    }

    const ShaderResourceLayoutD3D11& GetStaticResourceLayout(Uint32 s)const
    {
        VERIFY_EXPR(s < m_NumShaders);
        return m_pStaticResourceLayouts[s];
    }

    ShaderResourceCacheD3D11& GetStaticResourceCache(Uint32 s)
    {
        VERIFY_EXPR(s < m_NumShaders);
        return m_pStaticResourceCaches[s];
    }

    void SetStaticSamplers(ShaderResourceCacheD3D11& ResourceCache, Uint32 ShaderInd)const;

private:
    
    CComPtr<ID3D11BlendState>        m_pd3d11BlendState;
    CComPtr<ID3D11RasterizerState>   m_pd3d11RasterizerState;
    CComPtr<ID3D11DepthStencilState> m_pd3d11DepthStencilState;
    CComPtr<ID3D11InputLayout>       m_pd3d11InputLayout;

    // The caches are indexed by the shader order in the PSO, not shader index
    ShaderResourceCacheD3D11*  m_pStaticResourceCaches = nullptr;
    ShaderResourceLayoutD3D11* m_pStaticResourceLayouts= nullptr;

    // SRB memory allocator must be defined before the default shader res binding
    SRBMemoryAllocator m_SRBMemAllocator;

    Int8  m_ResourceLayoutIndex[6] = {-1, -1, -1, -1, -1, -1};

    Uint16 m_StaticSamplerOffsets[MaxShadersInPipeline+1] = {};
    struct StaticSamplerInfo
    {
        const D3DShaderResourceAttribs& Attribs;
        RefCntAutoPtr<ISampler>         pSampler;
        StaticSamplerInfo(const D3DShaderResourceAttribs& _Attribs,
                          RefCntAutoPtr<ISampler>         _pSampler) : 
            Attribs  (_Attribs),
            pSampler (std::move(_pSampler))
        {}
    };
    std::vector< StaticSamplerInfo, STDAllocatorRawMem<StaticSamplerInfo> > m_StaticSamplers;
};

}
