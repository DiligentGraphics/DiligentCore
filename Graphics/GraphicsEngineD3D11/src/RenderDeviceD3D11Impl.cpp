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
#include "RenderDeviceD3D11Impl.h"
#include "DeviceContextD3D11Impl.h"
#include "BufferD3D11Impl.h"
#include "VertexDescD3D11Impl.h"
#include "ShaderD3D11Impl.h"
#include "Texture1D_D3D11.h"
#include "Texture2D_D3D11.h"
#include "Texture3D_D3D11.h"
#include "SamplerD3D11Impl.h"
#include "D3D11TypeConversions.h"
#include "TextureViewD3D11Impl.h"
#include "DSStateD3D11Impl.h"
#include "RasterizerStateD3D11Impl.h"
#include "BlendStateD3D11Impl.h"

namespace Diligent
{

RenderDeviceD3D11Impl :: RenderDeviceD3D11Impl(ID3D11Device *pd3d11Device) : 
    m_pd3d11Device(pd3d11Device)
{
    m_DeviceCaps.DevType = DeviceType::DirectX;
    m_DeviceCaps.MajorVersion = 11;
    m_DeviceCaps.MinorVersion = 0;
    m_DeviceCaps.bSeparableProgramSupported = true;

    FlagSupportedTexFormats();
}

void RenderDeviceD3D11Impl::FlagSupportedTexFormats()
{
#define FLAG_FORMAT(Fmt, IsSupported)\
    m_TextureFormatsInfo[Fmt].Supported  = IsSupported;

    FLAG_FORMAT(TEX_FORMAT_RGBA32_TYPELESS,            true);
    FLAG_FORMAT(TEX_FORMAT_RGBA32_FLOAT,               true);
    FLAG_FORMAT(TEX_FORMAT_RGBA32_UINT,                true);
    FLAG_FORMAT(TEX_FORMAT_RGBA32_SINT,                true);
    FLAG_FORMAT(TEX_FORMAT_RGB32_TYPELESS,             true);
    FLAG_FORMAT(TEX_FORMAT_RGB32_FLOAT,                true);
    FLAG_FORMAT(TEX_FORMAT_RGB32_UINT,                 true);
    FLAG_FORMAT(TEX_FORMAT_RGB32_SINT,                 true);
    FLAG_FORMAT(TEX_FORMAT_RGBA16_TYPELESS,            true);
    FLAG_FORMAT(TEX_FORMAT_RGBA16_FLOAT,               true);
    FLAG_FORMAT(TEX_FORMAT_RGBA16_UNORM,               true);
    FLAG_FORMAT(TEX_FORMAT_RGBA16_UINT,                true);
    FLAG_FORMAT(TEX_FORMAT_RGBA16_SNORM,               true);
    FLAG_FORMAT(TEX_FORMAT_RGBA16_SINT,                true);
    FLAG_FORMAT(TEX_FORMAT_RG32_TYPELESS,              true);
    FLAG_FORMAT(TEX_FORMAT_RG32_FLOAT,                 true);
    FLAG_FORMAT(TEX_FORMAT_RG32_UINT,                  true);
    FLAG_FORMAT(TEX_FORMAT_RG32_SINT,                  true);
    FLAG_FORMAT(TEX_FORMAT_R32G8X24_TYPELESS,          true);
    FLAG_FORMAT(TEX_FORMAT_D32_FLOAT_S8X24_UINT,       true);
    FLAG_FORMAT(TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS,   true);
    FLAG_FORMAT(TEX_FORMAT_X32_TYPELESS_G8X24_UINT,    true);
    FLAG_FORMAT(TEX_FORMAT_RGB10A2_TYPELESS,           true);
    FLAG_FORMAT(TEX_FORMAT_RGB10A2_UNORM,              true);
    FLAG_FORMAT(TEX_FORMAT_RGB10A2_UINT,               true);
    FLAG_FORMAT(TEX_FORMAT_R11G11B10_FLOAT,            true);
    FLAG_FORMAT(TEX_FORMAT_RGBA8_TYPELESS,             true);
    FLAG_FORMAT(TEX_FORMAT_RGBA8_UNORM,                true);
    FLAG_FORMAT(TEX_FORMAT_RGBA8_UNORM_SRGB,           true);
    FLAG_FORMAT(TEX_FORMAT_RGBA8_UINT,                 true);
    FLAG_FORMAT(TEX_FORMAT_RGBA8_SNORM,                true);
    FLAG_FORMAT(TEX_FORMAT_RGBA8_SINT,                 true);
    FLAG_FORMAT(TEX_FORMAT_RG16_TYPELESS,              true);
    FLAG_FORMAT(TEX_FORMAT_RG16_FLOAT,                 true);
    FLAG_FORMAT(TEX_FORMAT_RG16_UNORM,                 true);
    FLAG_FORMAT(TEX_FORMAT_RG16_UINT,                  true);
    FLAG_FORMAT(TEX_FORMAT_RG16_SNORM,                 true);
    FLAG_FORMAT(TEX_FORMAT_RG16_SINT,                  true);
    FLAG_FORMAT(TEX_FORMAT_R32_TYPELESS,               true);
    FLAG_FORMAT(TEX_FORMAT_D32_FLOAT,                  true);
    FLAG_FORMAT(TEX_FORMAT_R32_FLOAT,                  true);
    FLAG_FORMAT(TEX_FORMAT_R32_UINT,                   true);
    FLAG_FORMAT(TEX_FORMAT_R32_SINT,                   true);
    FLAG_FORMAT(TEX_FORMAT_R24G8_TYPELESS,             true);
    FLAG_FORMAT(TEX_FORMAT_D24_UNORM_S8_UINT,          true);
    FLAG_FORMAT(TEX_FORMAT_R24_UNORM_X8_TYPELESS,      true);
    FLAG_FORMAT(TEX_FORMAT_X24_TYPELESS_G8_UINT,       true);
    FLAG_FORMAT(TEX_FORMAT_RG8_TYPELESS,               true);
    FLAG_FORMAT(TEX_FORMAT_RG8_UNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_RG8_UINT,                   true);
    FLAG_FORMAT(TEX_FORMAT_RG8_SNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_RG8_SINT,                   true);
    FLAG_FORMAT(TEX_FORMAT_R16_TYPELESS,               true);
    FLAG_FORMAT(TEX_FORMAT_R16_FLOAT,                  true);
    FLAG_FORMAT(TEX_FORMAT_D16_UNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_R16_UNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_R16_UINT,                   true);
    FLAG_FORMAT(TEX_FORMAT_R16_SNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_R16_SINT,                   true);
    FLAG_FORMAT(TEX_FORMAT_R8_TYPELESS,                true);
    FLAG_FORMAT(TEX_FORMAT_R8_UNORM,                   true);
    FLAG_FORMAT(TEX_FORMAT_R8_UINT,                    true);
    FLAG_FORMAT(TEX_FORMAT_R8_SNORM,                   true);
    FLAG_FORMAT(TEX_FORMAT_R8_SINT,                    true);
    FLAG_FORMAT(TEX_FORMAT_A8_UNORM,                   true);
    FLAG_FORMAT(TEX_FORMAT_R1_UNORM,                   true);
    FLAG_FORMAT(TEX_FORMAT_RGB9E5_SHAREDEXP,           true);
    FLAG_FORMAT(TEX_FORMAT_RG8_B8G8_UNORM,             true);
    FLAG_FORMAT(TEX_FORMAT_G8R8_G8B8_UNORM,            true);
    FLAG_FORMAT(TEX_FORMAT_BC1_TYPELESS,               true);
    FLAG_FORMAT(TEX_FORMAT_BC1_UNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_BC1_UNORM_SRGB,             true);
    FLAG_FORMAT(TEX_FORMAT_BC2_TYPELESS,               true);
    FLAG_FORMAT(TEX_FORMAT_BC2_UNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_BC2_UNORM_SRGB,             true);
    FLAG_FORMAT(TEX_FORMAT_BC3_TYPELESS,               true);
    FLAG_FORMAT(TEX_FORMAT_BC3_UNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_BC3_UNORM_SRGB,             true);
    FLAG_FORMAT(TEX_FORMAT_BC4_TYPELESS,               true);
    FLAG_FORMAT(TEX_FORMAT_BC4_UNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_BC4_SNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_BC5_TYPELESS,               true);
    FLAG_FORMAT(TEX_FORMAT_BC5_UNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_BC5_SNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_B5G6R5_UNORM,               true);
    FLAG_FORMAT(TEX_FORMAT_B5G5R5A1_UNORM,             true);
    FLAG_FORMAT(TEX_FORMAT_BGRA8_UNORM,                true);
    FLAG_FORMAT(TEX_FORMAT_BGRX8_UNORM,                true);
    FLAG_FORMAT(TEX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, true);
    FLAG_FORMAT(TEX_FORMAT_BGRA8_TYPELESS,             true);
    FLAG_FORMAT(TEX_FORMAT_BGRA8_UNORM_SRGB,           true);
    FLAG_FORMAT(TEX_FORMAT_BGRX8_TYPELESS,             true);
    FLAG_FORMAT(TEX_FORMAT_BGRX8_UNORM_SRGB,           true);
    FLAG_FORMAT(TEX_FORMAT_BC6H_TYPELESS,              true);
    FLAG_FORMAT(TEX_FORMAT_BC6H_UF16,                  true);
    FLAG_FORMAT(TEX_FORMAT_BC6H_SF16,                  true);
    FLAG_FORMAT(TEX_FORMAT_BC7_TYPELESS,               true);
    FLAG_FORMAT(TEX_FORMAT_BC7_UNORM,                  true);
    FLAG_FORMAT(TEX_FORMAT_BC7_UNORM_SRGB,             true);
}

bool CreateTestTexture1D(ID3D11Device *pDevice, const D3D11_TEXTURE1D_DESC &TexDesc)
{
    // Set the texture pointer address to nullptr to validate input parameters
    // without creating the texture
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476520(v=vs.85).aspx
    HRESULT hr = pDevice->CreateTexture1D( &TexDesc, nullptr, nullptr );
    return hr == S_FALSE; // S_FALSE means that input parameters passed validation
}

bool CreateTestTexture2D(ID3D11Device *pDevice, const D3D11_TEXTURE2D_DESC &TexDesc)
{
    // Set the texture pointer address to nullptr to validate input parameters
    // without creating the texture
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476521(v=vs.85).aspx
    HRESULT hr = pDevice->CreateTexture2D( &TexDesc, nullptr, nullptr );
    return hr == S_FALSE; // S_FALSE means that input parameters passed validation
}

bool CreateTestTexture3D(ID3D11Device *pDevice, const D3D11_TEXTURE3D_DESC &TexDesc)
{
    // Set the texture pointer address to nullptr to validate input parameters
    // without creating the texture
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476522(v=vs.85).aspx
    HRESULT hr = pDevice->CreateTexture3D( &TexDesc, nullptr, nullptr );
    return hr == S_FALSE; // S_FALSE means that input parameters passed validation
}

void RenderDeviceD3D11Impl::TestTextureFormat( TEXTURE_FORMAT TexFormat )
{
    auto &TexFormatInfo = m_TextureFormatsInfo[TexFormat];
    VERIFY( TexFormatInfo.Supported, "Texture format is not supported" );

    auto DXGIFormat = TexFormatToDXGI_Format(TexFormat);
    UINT DefaultBind = 0;
    if( TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH ||
        TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL )
        DefaultBind = D3D11_BIND_DEPTH_STENCIL;
    else
        DefaultBind = D3D11_BIND_SHADER_RESOURCE;

    const int TestTextureDim = 32;
    const int TestTextureDepth = 8;
    
    // Create test texture 1D
    TexFormatInfo.Tex1DFmt = false;
    if( TexFormatInfo.ComponentType != COMPONENT_TYPE_COMPRESSED )
    {
        D3D11_TEXTURE1D_DESC Tex1DDesc =
        {
            TestTextureDim,            // UINT Width;
            1,                         // UINT MipLevels;
            1,                         // UINT ArraySize;
            DXGIFormat,                // DXGI_FORMAT Format;
            D3D11_USAGE_DEFAULT,       // D3D11_USAGE Usage;
            DefaultBind,               // UINT BindFlags;
            0,                         // UINT CPUAccessFlags;
            0                          // UINT MiscFlags;
        };
        TexFormatInfo.Tex1DFmt = CreateTestTexture1D(m_pd3d11Device, Tex1DDesc );
    }

    // Create test texture 2D
    TexFormatInfo.Tex2DFmt = false;
    TexFormatInfo.TexCubeFmt = false;
    TexFormatInfo.ColorRenderable = false;
    TexFormatInfo.DepthRenderable = false;
    TexFormatInfo.SupportsMS = false;
    {
        D3D11_TEXTURE2D_DESC Tex2DDesc =
        {
            TestTextureDim,      // UINT Width;
            TestTextureDim,      // UINT Height;
            1,                   // UINT MipLevels;
            1,                   // UINT ArraySize;
            DXGIFormat,          // DXGI_FORMAT Format;
            { 1, 0 },            // DXGI_SAMPLE_DESC SampleDesc;
            D3D11_USAGE_DEFAULT, // D3D11_USAGE Usage;
            DefaultBind,         // UINT BindFlags;
            0,                   // UINT CPUAccessFlags;
            0,                   // UINT MiscFlags;
        };
        TexFormatInfo.Tex2DFmt = CreateTestTexture2D( m_pd3d11Device, Tex2DDesc );

        if( TexFormatInfo.Tex2DFmt )
        {
            {
                D3D11_TEXTURE2D_DESC CubeTexDesc = Tex2DDesc;
                CubeTexDesc.ArraySize = 6;
                CubeTexDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
                TexFormatInfo.TexCubeFmt = CreateTestTexture2D( m_pd3d11Device, CubeTexDesc );
            }

            if( TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH ||
                TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL )
            {
                Tex2DDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                TexFormatInfo.DepthRenderable = CreateTestTexture2D( m_pd3d11Device, Tex2DDesc );

                if( TexFormatInfo.DepthRenderable )
                {
                    Tex2DDesc.SampleDesc.Count = 4;
                    TexFormatInfo.SupportsMS = CreateTestTexture2D( m_pd3d11Device, Tex2DDesc );
                }
            }
            else if( TexFormatInfo.ComponentType != COMPONENT_TYPE_COMPRESSED )
            {
                Tex2DDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
                TexFormatInfo.ColorRenderable = CreateTestTexture2D( m_pd3d11Device, Tex2DDesc );
                if( TexFormatInfo.ColorRenderable )
                {
                    Tex2DDesc.SampleDesc.Count = 4;
                    TexFormatInfo.SupportsMS = CreateTestTexture2D( m_pd3d11Device, Tex2DDesc );
                }
            }
        }
    }

    // Create test texture 3D
    TexFormatInfo.Tex3DFmt = false;
    // 3D textures do not support depth formats
    if( !(TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH ||
          TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL) )
    {
        D3D11_TEXTURE3D_DESC Tex3DDesc =
        {
            TestTextureDim,     // UINT Width;
            TestTextureDim,     // UINT Height;
            TestTextureDepth,   // UINT Depth;
            1,                  // UINT MipLevels;
            DXGIFormat,         // DXGI_FORMAT Format;
            D3D11_USAGE_DEFAULT,// D3D11_USAGE Usage;
            DefaultBind,        // UINT BindFlags;
            0,                  // UINT CPUAccessFlags;
            0                   // UINT MiscFlags;
        };
        TexFormatInfo.Tex3DFmt = CreateTestTexture3D( m_pd3d11Device, Tex3DDesc );
    }
}

IMPLEMENT_QUERY_INTERFACE( RenderDeviceD3D11Impl, IID_RenderDeviceD3D11, TRenderDeviceBase )

void RenderDeviceD3D11Impl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D11Impl *pBufferD3D11( new BufferD3D11Impl( this, BuffDesc, BuffData ) );
            pBufferD3D11->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pBufferD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl :: CreateVertexDescription(const LayoutDesc& LayoutDesc, IShader *pVertexShader, IVertexDescription **ppVertexDesc)
{
    CreateDeviceObject( "vertex description", LayoutDesc, ppVertexDesc, 
        [&]()
        {
            VertexDescD3D11Impl *pDescD3D11( new VertexDescD3D11Impl( this, LayoutDesc, pVertexShader ) );
            pDescD3D11->QueryInterface( IID_VertexDescription, reinterpret_cast<IObject**>(ppVertexDesc) );

            OnCreateDeviceObject( pDescD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl :: CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader)
{
    CreateDeviceObject( "shader", ShaderCreationAttribs.Desc, ppShader, 
        [&]()
        {
            ShaderD3D11Impl *pShaderD3D11( new ShaderD3D11Impl( this, ShaderCreationAttribs ) );
            pShaderD3D11->QueryInterface( IID_Shader, reinterpret_cast<IObject**>(ppShader) );

            OnCreateDeviceObject( pShaderD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl :: CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureBaseD3D11 *pTextureD3D11 = nullptr;
            switch( TexDesc.Type )
            {
                case TEXTURE_TYPE_1D:
                case TEXTURE_TYPE_1D_ARRAY:
                    pTextureD3D11 = new Texture1D_D3D11( this, TexDesc, Data );
                break;

                case TEXTURE_TYPE_2D:
                case TEXTURE_TYPE_2D_ARRAY:
                    pTextureD3D11 = new Texture2D_D3D11( this, TexDesc, Data );
                break;

                case TEXTURE_TYPE_3D:
                    pTextureD3D11 = new Texture3D_D3D11( this, TexDesc, Data );
                break;

                default: LOG_ERROR_AND_THROW( "Unknown texture type. (Did you forget to initialize the Type member of TextureDesc structure?)" );
            }
            pTextureD3D11->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl :: CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler)
{
    CreateDeviceObject( "sampler", SamplerDesc, ppSampler, 
        [&]()
        {
            m_SamplersRegistry.Find( SamplerDesc, reinterpret_cast<IDeviceObject**>(ppSampler) );
            if( *ppSampler == nullptr )
            {
                SamplerD3D11Impl *pSamplerD3D11( new SamplerD3D11Impl( this, SamplerDesc ) );
                pSamplerD3D11->QueryInterface( IID_Sampler, reinterpret_cast<IObject**>(ppSampler) );
                OnCreateDeviceObject( pSamplerD3D11 );
                m_SamplersRegistry.Add( SamplerDesc, *ppSampler );
            }
        }
    );
}

void RenderDeviceD3D11Impl::CreateDepthStencilState( const DepthStencilStateDesc &DSSDesc, IDepthStencilState **ppDepthStencilState )
{
    CreateDeviceObject( "depth-stencil state", DSSDesc, ppDepthStencilState, 
        [&]()
        {
            m_DSSRegistry.Find( DSSDesc, reinterpret_cast<IDeviceObject**>(ppDepthStencilState) );
            if( *ppDepthStencilState == nullptr )
            {
                DSStateD3D11Impl *pDepthStencilStateD3D11( new DSStateD3D11Impl( this, DSSDesc ) );
                pDepthStencilStateD3D11->QueryInterface( IID_DepthStencilState, reinterpret_cast<IObject**>(ppDepthStencilState) );
                OnCreateDeviceObject( pDepthStencilStateD3D11 );
                m_DSSRegistry.Add( DSSDesc, *ppDepthStencilState );
            }
        } 
    );
}

void RenderDeviceD3D11Impl::CreateRasterizerState( const RasterizerStateDesc &RSDesc, IRasterizerState **ppRasterizerState )
{
    CreateDeviceObject( "rasterizer state", RSDesc, ppRasterizerState, 
        [&]()
        {
            m_RSRegistry.Find( RSDesc, reinterpret_cast<IDeviceObject**>(ppRasterizerState) );
            if( *ppRasterizerState == nullptr )
            {
                RasterizerStateD3D11Impl *pRasterizerStateD3D11( new RasterizerStateD3D11Impl( this, RSDesc ) );
                pRasterizerStateD3D11->QueryInterface( IID_RasterizerState, reinterpret_cast<IObject**>(ppRasterizerState) );
                OnCreateDeviceObject( pRasterizerStateD3D11 );
                m_RSRegistry.Add( RSDesc, *ppRasterizerState );
            }
        } 
    );
}


void RenderDeviceD3D11Impl::CreateBlendState( const BlendStateDesc &BSDesc, IBlendState **ppBlendState )
{
    CreateDeviceObject( "blend state", BSDesc, ppBlendState, 
        [&]()
        {
            m_BSRegistry.Find( BSDesc, reinterpret_cast<IDeviceObject**>(ppBlendState) );
            if( *ppBlendState == nullptr )
            {
                BlendStateD3D11Impl *pBlendStateD3D11( new BlendStateD3D11Impl( this, BSDesc ) );
                pBlendStateD3D11->QueryInterface( IID_BlendState, reinterpret_cast<IObject**>(ppBlendState) );
                OnCreateDeviceObject( pBlendStateD3D11 );
                m_BSRegistry.Add( BSDesc, *ppBlendState );
            }
        } 
    );
}

}
