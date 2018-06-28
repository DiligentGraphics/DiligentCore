/*     Copyright 2015-2018 Egor Yusov
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

namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::IPipelineStateD3D11 interface
class PipelineStateD3D11Impl : public PipelineStateBase<IPipelineStateD3D11, IRenderDeviceD3D11>
{
public:
    typedef PipelineStateBase<IPipelineStateD3D11, IRenderDeviceD3D11> TPipelineStateBase;

    PipelineStateD3D11Impl(IReferenceCounters*          pRefCounters,
                           class RenderDeviceD3D11Impl* pDeviceD3D11,
                           const PipelineStateDesc&     PipelineDesc);
    ~PipelineStateD3D11Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;
    
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

    virtual void CreateShaderResourceBinding( IShaderResourceBinding **ppShaderResourceBinding )override final;

    virtual bool IsCompatibleWith(const IPipelineState *pPSO)const override final;

    class ShaderResourceBindingD3D11Impl* GetDefaultResourceBinding(){return m_pDefaultShaderResBinding.get();}
    
    SRBMemoryAllocator& GetSRBMemoryAllocator()
    {
        return m_SRBMemAllocator;
    }

    IShaderVariable* GetDummyShaderVariable(){return &m_DummyShaderVar;}

private:
    CComPtr<ID3D11BlendState>        m_pd3d11BlendState;
    CComPtr<ID3D11RasterizerState>   m_pd3d11RasterizerState;
    CComPtr<ID3D11DepthStencilState> m_pd3d11DepthStencilState;
    CComPtr<ID3D11InputLayout>       m_pd3d11InputLayout;

    // SRB memory allocator must be defined before the default shader res binding
    SRBMemoryAllocator m_SRBMemAllocator;

    // Do not use strong reference to avoid cyclic references
    // Must be declared after the data allocators
    std::unique_ptr<class ShaderResourceBindingD3D11Impl, STDDeleter<ShaderResourceBindingD3D11Impl, FixedBlockMemoryAllocator> > m_pDefaultShaderResBinding;

    DummyShaderVariable m_DummyShaderVar; ///< Dummy shader variable
};

}
