/*     Copyright 2015-2016 Egor Yusov
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
#include "Texture1D_D3D11.h"
#include "RenderDeviceD3D11Impl.h"
#include "D3D11TypeConversions.h"

namespace Diligent
{

Texture1D_D3D11 :: Texture1D_D3D11(FixedBlockMemoryAllocator &TexObjAllocator, 
                                   FixedBlockMemoryAllocator &TexViewObjAllocator, 
                                   RenderDeviceD3D11Impl *pRenderDeviceD3D11, 
                                   const TextureDesc& TexDesc, 
                                   const TextureData &InitData /*= TextureData()*/) : 
    TextureBaseD3D11(TexObjAllocator, TexViewObjAllocator, pRenderDeviceD3D11, TexDesc, InitData)
{
    auto D3D11TexFormat = TexFormatToDXGI_Format(m_Desc.Format, m_Desc.BindFlags);
    auto D3D11BindFlags = BindFlagsToD3D11BindFlags(m_Desc.BindFlags);
    auto D3D11CPUAccessFlags = CPUAccessFlagsToD3D11CPUAccessFlags(m_Desc.CPUAccessFlags);
    auto D3D11Usage = UsageToD3D11Usage(m_Desc.Usage);
    UINT MiscFlags = MiscTextureFlagsToD3D11Flags(m_Desc.MiscFlags);
    auto *pDeviceD3D11 = pRenderDeviceD3D11->GetD3D11Device();

    D3D11_TEXTURE1D_DESC Tex1DDesc = 
    {
        m_Desc.Width,
        m_Desc.MipLevels,
        m_Desc.ArraySize,
        D3D11TexFormat,
        D3D11Usage,
        D3D11BindFlags,
        D3D11CPUAccessFlags,
        MiscFlags
    };

    std::vector<D3D11_SUBRESOURCE_DATA, STDAllocatorRawMem<D3D11_SUBRESOURCE_DATA>> D3D11InitData( STD_ALLOCATOR_RAW_MEM(D3D11_SUBRESOURCE_DATA, GetRawAllocator(), "Allocator for vector<D3D11_SUBRESOURCE_DATA>") );
    PrepareD3D11InitData(InitData, Tex1DDesc.ArraySize * Tex1DDesc.MipLevels, D3D11InitData);

    ID3D11Texture1D *ptex1D = nullptr;
    HRESULT hr = pDeviceD3D11->CreateTexture1D(&Tex1DDesc, D3D11InitData.size() ? D3D11InitData.data() : nullptr, &ptex1D);
    m_pd3d11Texture.Attach(ptex1D);
    CHECK_D3D_RESULT_THROW( hr, "Failed to create the Direct3D11 Texture1D" );
}

Texture1D_D3D11 :: ~Texture1D_D3D11()
{
}

void Texture1D_D3D11::CreateSRV( TextureViewDesc &SRVDesc, ID3D11ShaderResourceView **ppD3D11SRV )
{
    VERIFY( ppD3D11SRV && *ppD3D11SRV == nullptr, "SRV pointer address is null or contains non-null pointer to an existing object" );
    
    VERIFY( SRVDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE, "Incorrect view type: shader resource is expected" );
    if( !(SRVDesc.TextureDim == RESOURCE_DIM_TEX_1D || SRVDesc.TextureDim == RESOURCE_DIM_TEX_1D_ARRAY) )
        LOG_ERROR_AND_THROW( "Unsupported texture type. Only RESOURCE_DIM_TEX_1D or RESOURCE_DIM_TEX_1D_ARRAY is allowed" );
    
    if( SRVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        SRVDesc.Format = m_Desc.Format;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC D3D11_SRVDesc;
    TextureViewDesc_to_D3D11_SRV_DESC(SRVDesc, D3D11_SRVDesc, m_Desc.SampleCount);

    auto *pDeviceD3D11 = static_cast<RenderDeviceD3D11Impl*>(GetDevice())->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateShaderResourceView( m_pd3d11Texture, &D3D11_SRVDesc, ppD3D11SRV ),
                            "Failed to create D3D11 shader resource view");
}

void Texture1D_D3D11::CreateRTV( TextureViewDesc &RTVDesc, ID3D11RenderTargetView **ppD3D11RTV )
{
    VERIFY( ppD3D11RTV && *ppD3D11RTV == nullptr, "RTV pointer address is null or contains non-null pointer to an existing object" );
    
    VERIFY( RTVDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET, "Incorrect view type: render target is expected" );
    if( !(RTVDesc.TextureDim == RESOURCE_DIM_TEX_1D || RTVDesc.TextureDim == RESOURCE_DIM_TEX_1D_ARRAY ) )
        LOG_ERROR_AND_THROW( "Unsupported texture type. Only RESOURCE_DIM_TEX_1D or RESOURCE_DIM_TEX_1D_ARRAY is allowed" );

    if( RTVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        RTVDesc.Format = m_Desc.Format;
    }

    D3D11_RENDER_TARGET_VIEW_DESC D3D11_RTVDesc;
    TextureViewDesc_to_D3D11_RTV_DESC(RTVDesc, D3D11_RTVDesc, m_Desc.SampleCount);

    auto *pDeviceD3D11 = static_cast<RenderDeviceD3D11Impl*>(GetDevice())->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateRenderTargetView( m_pd3d11Texture, &D3D11_RTVDesc, ppD3D11RTV ),
                            "Failed to create D3D11 render target view");
}

void Texture1D_D3D11::CreateDSV( TextureViewDesc &DSVDesc, ID3D11DepthStencilView  **ppD3D11DSV )
{
    VERIFY( ppD3D11DSV && *ppD3D11DSV == nullptr, "DSV pointer address is null or contains non-null pointer to an existing object"  );

    VERIFY( DSVDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Incorrect view type: depth stencil is expected" );
    if( !(DSVDesc.TextureDim == RESOURCE_DIM_TEX_1D || DSVDesc.TextureDim == RESOURCE_DIM_TEX_1D_ARRAY) )
        LOG_ERROR_AND_THROW("Unsupported texture type. Only RESOURCE_DIM_TEX_1D or RESOURCE_DIM_TEX_1D_ARRAY is allowed");

    if( DSVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        DSVDesc.Format = m_Desc.Format;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC D3D11_DSVDesc;
    TextureViewDesc_to_D3D11_DSV_DESC(DSVDesc, D3D11_DSVDesc, m_Desc.SampleCount);

    auto *pDeviceD3D11 = static_cast<RenderDeviceD3D11Impl*>(GetDevice())->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateDepthStencilView( m_pd3d11Texture, &D3D11_DSVDesc, ppD3D11DSV ),
                            "Failed to create D3D11 depth stencil view");
}

void Texture1D_D3D11::CreateUAV( TextureViewDesc &UAVDesc, ID3D11UnorderedAccessView **ppD3D11UAV )
{
    VERIFY( ppD3D11UAV && *ppD3D11UAV == nullptr, "UAV pointer address is null or contains non-null pointer to an existing object"  );

    VERIFY( UAVDesc.ViewType == TEXTURE_VIEW_UNORDERED_ACCESS, "Incorrect view type: unordered access is expected" );
    if( !(UAVDesc.TextureDim == RESOURCE_DIM_TEX_1D || UAVDesc.TextureDim == RESOURCE_DIM_TEX_1D_ARRAY) )
        LOG_ERROR_AND_THROW("Unsupported texture type. Only RESOURCE_DIM_TEX_1D or RESOURCE_DIM_TEX_1D_ARRAY is allowed");
    
    if( UAVDesc.Format == TEX_FORMAT_UNKNOWN )
    {
        UAVDesc.Format = m_Desc.Format;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC D3D11_UAVDesc;
    TextureViewDesc_to_D3D11_UAV_DESC(UAVDesc, D3D11_UAVDesc);

    auto *pDeviceD3D11 = static_cast<RenderDeviceD3D11Impl*>(GetDevice())->GetD3D11Device();
    CHECK_D3D_RESULT_THROW( pDeviceD3D11->CreateUnorderedAccessView( m_pd3d11Texture, &D3D11_UAVDesc, ppD3D11UAV ),
                            "Failed to create D3D11 unordered access view");
}

}
