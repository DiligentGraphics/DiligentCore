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
#include "D3D11DebugUtilities.h"
#include "TextureViewD3D11.h"
#include "BufferViewD3D11.h"
#include "BufferD3D11.h"
#include "SamplerD3D11.h"
#include "GraphicsUtilities.h"

namespace Diligent
{

#ifdef VERIFY_RESOURCE_ARRAYS

template<typename TResourceArr, typename TD3D11ResourceArr, typename TTestProc>
void dbgVerifyResourceArraysInternal( TResourceArr &Resources, TD3D11ResourceArr& d3d11Resources, TTestProc TestProc )
{
    VERIFY( Resources.size() == d3d11Resources.size(), "Inconsistent resource array sizes (", Resources.size(), ",", d3d11Resources.size(), ")" );
    auto NumSlots = std::min(Resources.size(), d3d11Resources.size());
    for( Uint32 Slot = 0; Slot < NumSlots; ++Slot )
    {
        auto &Res = Resources.at(Slot);
        const auto &d3d11Res = d3d11Resources.at( Slot );
        TestProc( Slot, Res, d3d11Res );
    }
}

static const Char* GetD3D11ResName( const ID3D11ShaderResourceView* ){ return "SRV"; }
static const Char* GetD3D11ResName( const ID3D11UnorderedAccessView* ){ return "UAV"; }
static const Char* GetD3D11ResName( const ID3D11Buffer* ){ return "constant buffer"; }
static const Char* GetD3D11ResName( const ID3D11SamplerState* ){ return "sampler"; }

template<typename TBoundResourceType, typename TD3D11ResourceType>
class dbgVerifyResourceViewArrays
{
public:
    dbgVerifyResourceViewArrays( TEXTURE_VIEW_TYPE TVT, BUFFER_VIEW_TYPE BVT, IShader *pShader ) :
        TexViewType( TVT ),
        BuffViewType( BVT ),
        ShaderTypeName(GetShaderTypeLiteralName(pShader->GetDesc().ShaderType)),
        ShaderName(pShader->GetDesc().Name)
    {}

    void operator()( int Slot, const TBoundResourceType& BoundRes, const TD3D11ResourceType* d3d11View )
    {
        TD3D11ResourceType *pD3D11ResTypeMarker = nullptr;
#define VERFIY_RESOURCE_BINDING(Exp, Intro, Res, ...) \
        VERIFY(Exp, Intro, " \"", Res->GetDesc().Name, "\" bound as ", GetD3D11ResName(pD3D11ResTypeMarker)," to shader \"", ShaderName, "\" (", ShaderTypeName, ") at slot ", Slot, ' ', __VA_ARGS__)

        if( !d3d11View )
        {
            VERFIY_RESOURCE_BINDING( !BoundRes.pResource, "Unexpected non-null resource", BoundRes.pResource )
            VERFIY_RESOURCE_BINDING( !BoundRes.pView, "Unexpected non-null resource view", BoundRes.pView )
        }

        if( BoundRes.pView )
        {
            auto ncView = const_cast<IDeviceObject*>(BoundRes.pView.RawPtr());
            RefCntAutoPtr<ITextureViewD3D11> pTexView;
            ncView->QueryInterface( IID_TextureViewD3D11, reinterpret_cast<IObject**>(static_cast<ITextureViewD3D11**>(&pTexView)) );
            RefCntAutoPtr<IBufferViewD3D11> pBuffView;
            ncView->QueryInterface( IID_BufferViewD3D11, reinterpret_cast<IObject**>(static_cast<IBufferViewD3D11**>(&pBuffView)) );
            VERFIY_RESOURCE_BINDING( pTexView || pBuffView, "Resource", ncView, "is expected to be a texture view or a buffer view" );
            if( pTexView )
            {
                auto ViewType = pTexView->GetDesc().ViewType;
                VERFIY_RESOURCE_BINDING( ViewType == TexViewType, "Texture view", ncView, "has incorrect type: ", GetTexViewTypeLiteralName( TexViewType ), " is expected, while ", GetTexViewTypeLiteralName( ViewType ), " provided" );
                auto *d3d11RefView = pTexView->GetD3D11View();
                VERFIY_RESOURCE_BINDING( d3d11RefView == d3d11View && BoundRes.pd3d11View.RawPtr() == d3d11RefView, "Texture view", ncView, "does not match D3D11 resoruce" );
                auto *pTex = pTexView->GetTexture();
                VERFIY_RESOURCE_BINDING( pTex == BoundRes.pResource, "Texture view", ncView, "is not the view of resource \"", BoundRes.pResource->GetDesc().Name, "\"" );
            }

            if( pBuffView )
            {
                auto ViewType = pBuffView->GetDesc().ViewType;
                VERFIY_RESOURCE_BINDING( ViewType == BuffViewType, "Buffer view", ncView, "has incorrect type: ", GetBufferViewTypeLiteralName( BuffViewType ), " is expected, while ", GetBufferViewTypeLiteralName( ViewType ), " provided" );
                auto *d3d11RefView = pBuffView->GetD3D11View();
                VERFIY_RESOURCE_BINDING( d3d11RefView == d3d11View && BoundRes.pd3d11View.RawPtr() == d3d11RefView, "Buffer view", ncView, "does not match D3D11 resource" );
                auto *pBuf = pBuffView->GetBuffer();
                VERFIY_RESOURCE_BINDING( pBuf == BoundRes.pResource, "Buffer view", ncView, "is not the view of resource \"", BoundRes.pResource->GetDesc().Name, "\"" );
            }
        }
        else
        {
            VERIFY( !BoundRes.pResource, "Unexpected non-null resource bound to shader \"", ShaderName, "\" (", ShaderTypeName, ") at slot ", Slot);
            VERIFY( !BoundRes.pd3d11View, "Unexpected non-null D3D11 resource view bound to shader \"", ShaderName, "\" (", ShaderTypeName, ") at slot ", Slot);
        }
    }
private:
    TEXTURE_VIEW_TYPE TexViewType;
    BUFFER_VIEW_TYPE BuffViewType;
    const Char* ShaderTypeName;
    const char* ShaderName;
};

void dbgVerifyResourceArrays( const std::vector< ShaderD3D11Impl::BoundSRV >&SRVs, const std::vector<ID3D11ShaderResourceView*> d3d11SRVs, IShader *pShader )
{
    dbgVerifyResourceArraysInternal(SRVs, d3d11SRVs, dbgVerifyResourceViewArrays<ShaderD3D11Impl::BoundSRV, ID3D11ShaderResourceView>(TEXTURE_VIEW_SHADER_RESOURCE, BUFFER_VIEW_SHADER_RESOURCE, pShader));
}

void dbgVerifyResourceArrays( const std::vector< ShaderD3D11Impl::BoundUAV >&UAVs, const std::vector<ID3D11UnorderedAccessView*> d3d11UAVs, IShader *pShader )
{
    dbgVerifyResourceArraysInternal(UAVs, d3d11UAVs, dbgVerifyResourceViewArrays<ShaderD3D11Impl::BoundUAV, ID3D11UnorderedAccessView>(TEXTURE_VIEW_UNORDERED_ACCESS, BUFFER_VIEW_UNORDERED_ACCESS, pShader));
}

void dbgVerifyResourceArrays( const std::vector< ShaderD3D11Impl::BoundCB >&CBs, const std::vector<ID3D11Buffer*> d3d11CBs, IShader *pShader )
{
    const auto* ShaderTypeName = GetShaderTypeLiteralName( pShader->GetDesc().ShaderType );
    const auto &ShaderName = pShader->GetDesc().Name;
    ID3D11Buffer *pD3D11ResTypeMarker = nullptr;

    dbgVerifyResourceArraysInternal( CBs, d3d11CBs, 
        [&]( int Slot, const ShaderD3D11Impl::BoundCB& CB, const ID3D11Buffer* d3d11CB )
        {
            if( !d3d11CB )
            {
                VERFIY_RESOURCE_BINDING( !CB.pBuff, "Unexpected non-null resource", CB.pBuff )
            }
        
            if( CB.pBuff )
            {
                auto ncCB = const_cast<IDeviceObject*>(CB.pBuff.RawPtr());
                RefCntAutoPtr<IBufferD3D11> pBuff;
                ncCB->QueryInterface( IID_BufferD3D11, reinterpret_cast<IObject**>(static_cast<IBufferD3D11**>(&pBuff)) );
                VERFIY_RESOURCE_BINDING( pBuff, "Resource", ncCB, "is expected to be a buffer" );
                if( pBuff )
                {
                    auto *d3d11RefBuff = pBuff->GetD3D11Buffer();
                    VERFIY_RESOURCE_BINDING( d3d11RefBuff == d3d11CB && d3d11RefBuff == CB.pd3d11Buff, "Constant buffer", ncCB, "does not match D3D11 buffer" );
                }
            }
            else
            {
                 VERIFY( !d3d11CB, "Unexpected non-null D3D11 buffer bound to shader \"", ShaderName, "\" (", ShaderTypeName, ") at slot ", Slot);
            }
        }
    );
}

void dbgVerifyResourceArrays( const std::vector< ShaderD3D11Impl::BoundSampler >&Samplers, const std::vector<ID3D11SamplerState*> d3d11Samplers, IShader *pShader )
{
    const auto* ShaderTypeName = GetShaderTypeLiteralName( pShader->GetDesc().ShaderType );
    const auto &ShaderName = pShader->GetDesc().Name;
    ID3D11SamplerState *pD3D11ResTypeMarker = nullptr;

    dbgVerifyResourceArraysInternal( Samplers, d3d11Samplers, 
        [&]( int Slot, const ShaderD3D11Impl::BoundSampler& Sampler, const ID3D11SamplerState* d3d11Sampler )
        {
            if( !d3d11Sampler )
            {
                VERFIY_RESOURCE_BINDING( !Sampler.pSampler, "Unexpected non-null resource", Sampler.pSampler )
            }
            if( Sampler.pSampler )
            {
                auto ncSampler = const_cast<IDeviceObject*>(Sampler.pSampler.RawPtr());
                RefCntAutoPtr<ISamplerD3D11> pSamplerD3D11;
                ncSampler->QueryInterface( IID_SamplerD3D11, reinterpret_cast<IObject**>(static_cast<ISamplerD3D11**>(&pSamplerD3D11)) );
                VERFIY_RESOURCE_BINDING( pSamplerD3D11, "Resource", ncSampler, "is expected to be a sampler" );
                if( pSamplerD3D11 )
                {
                    auto *d3d11RefSampler = pSamplerD3D11->GetD3D11SamplerState();
                    VERFIY_RESOURCE_BINDING( d3d11RefSampler == d3d11Sampler && d3d11RefSampler == Sampler.pd3d11Sampler, "Sampler", ncSampler, "does not match D3D11 sampler" );
                }
            }
            else
            {
                VERIFY( !d3d11Sampler, "Unexpected non-null D3D11 sampler bound to shader \"", ShaderName, "\" (", ShaderTypeName, ") at slot ", Slot );
            }
        }
    );
}

#endif

}
