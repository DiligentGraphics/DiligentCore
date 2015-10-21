/*     Copyright 2015 Egor Yusov
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
#include "VertexDescD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"
#include "ShaderD3D11Impl.h"
#include "D3D11TypeConversions.h"

namespace Diligent
{

VertexDescD3D11Impl::VertexDescD3D11Impl(RenderDeviceD3D11Impl *pRenderDeviceD3D11, const LayoutDesc &LayoutDesc, IShader *pVertexShader) : 
    TVertexDescriptionBase( pRenderDeviceD3D11, LayoutDesc )
{
    // Use m_LayoutDesc as original LayoutDescription might have been corrected
    auto *pLayoutElements = m_LayoutElements.data();
    auto NumElements = m_LayoutElements.size();

    if( pVertexShader->GetDesc().ShaderType != SHADER_TYPE_VERTEX )
        LOG_ERROR_AND_THROW( "Invalid shader type provided for the input layout creation" );
    std::vector<D3D11_INPUT_ELEMENT_DESC> InputElements;
    ID3DBlob *pVSByteCode = static_cast<ShaderD3D11Impl*>(pVertexShader)->m_pShaderByteCode;
    if( !pVSByteCode )
        LOG_ERROR_AND_THROW( "Vertex Shader byte code does not exist" );
    
    InputElements.resize(NumElements);
    for(Uint32 iElem=0; iElem < NumElements; ++iElem)
    {
        const auto &CurrElem = pLayoutElements[iElem];
        auto &D3D11Elem = InputElements[iElem];
        D3D11Elem.SemanticName = "ATTRIB";
        D3D11Elem.SemanticIndex = CurrElem.InputIndex;
        D3D11Elem.AlignedByteOffset = CurrElem.RelativeOffset;
        D3D11Elem.InputSlot = CurrElem.BufferSlot;
        D3D11Elem.Format = TypeToDXGI_Format(CurrElem.ValueType, CurrElem.NumComponents, CurrElem.IsNormalized);
        D3D11Elem.InputSlotClass = (CurrElem.Frequency == LayoutElement::FREQUENCY_PER_VERTEX) ? D3D11_INPUT_PER_VERTEX_DATA : D3D11_INPUT_PER_INSTANCE_DATA;
        D3D11Elem.InstanceDataStepRate = (CurrElem.Frequency == LayoutElement::FREQUENCY_PER_VERTEX) ? 0 : CurrElem.InstanceDataStepRate;
    }

    auto *pDeviceD3D11 = pRenderDeviceD3D11->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateInputLayout(InputElements.data(), static_cast<UINT>(InputElements.size()), pVSByteCode->GetBufferPointer(), pVSByteCode->GetBufferSize(), &m_pd3d11InputLayout),
                            "Failed to create the Direct3D11 input layout");
}

VertexDescD3D11Impl::~VertexDescD3D11Impl()
{
}

IMPLEMENT_QUERY_INTERFACE( VertexDescD3D11Impl, IID_VertexDescriptionD3D11, TVertexDescriptionBase )

}
