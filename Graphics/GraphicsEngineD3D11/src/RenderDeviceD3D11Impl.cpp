/*     Copyright 2015-2017 Egor Yusov
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
#include "ShaderD3D11Impl.h"
#include "Texture1D_D3D11.h"
#include "Texture2D_D3D11.h"
#include "Texture3D_D3D11.h"
#include "SamplerD3D11Impl.h"
#include "D3D11TypeConversions.h"
#include "TextureViewD3D11Impl.h"
#include "PipelineStateD3D11Impl.h"
#include "ShaderResourceBindingD3D11Impl.h"
#include "EngineMemory.h"

namespace Diligent
{

RenderDeviceD3D11Impl :: RenderDeviceD3D11Impl(IReferenceCounters *pRefCounters, IMemoryAllocator &RawMemAllocator, const EngineD3D11Attribs& EngineAttribs, ID3D11Device *pd3d11Device, Uint32 NumDeferredContexts) : 
    TRenderDeviceBase(pRefCounters, RawMemAllocator, NumDeferredContexts, sizeof(TextureBaseD3D11), sizeof(TextureViewD3D11Impl), sizeof(BufferD3D11Impl), sizeof(BufferViewD3D11Impl), sizeof(ShaderD3D11Impl), sizeof(SamplerD3D11Impl), sizeof(PipelineStateD3D11Impl), sizeof(ShaderResourceBindingD3D11Impl)),
    m_EngineAttribs(EngineAttribs),
    m_pd3d11Device(pd3d11Device)
{
    m_DeviceCaps.DevType = DeviceType::D3D11;
    m_DeviceCaps.MajorVersion = 11;
    m_DeviceCaps.MinorVersion = 0;
    m_DeviceCaps.bSeparableProgramSupported = True;
    m_DeviceCaps.bMultithreadedResourceCreationSupported = True;
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
            else if( TexFormatInfo.ComponentType != COMPONENT_TYPE_COMPRESSED && 
                     TexFormatInfo.Format != DXGI_FORMAT_R9G9B9E5_SHAREDEXP)
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

void RenderDeviceD3D11Impl :: CreateBufferFromD3DResource(ID3D11Buffer *pd3d11Buffer, const BufferDesc& BuffDesc, IBuffer **ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D11Impl *pBufferD3D11( NEW_RC_OBJ(m_BufObjAllocator, "BufferD3D11Impl instance", BufferD3D11Impl)
                                                     (m_BuffViewObjAllocator, this, BuffDesc, pd3d11Buffer ) );
            pBufferD3D11->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pBufferD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D11Impl *pBufferD3D11( NEW_RC_OBJ(m_BufObjAllocator, "BufferD3D11Impl instance", BufferD3D11Impl)
                                                     (m_BuffViewObjAllocator, this, BuffDesc, BuffData ) );
            pBufferD3D11->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pBufferD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl :: CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader)
{
    CreateDeviceObject( "shader", ShaderCreationAttribs.Desc, ppShader, 
        [&]()
        {
            ShaderD3D11Impl *pShaderD3D11( NEW_RC_OBJ(m_ShaderObjAllocator, "ShaderD3D11Impl instance", ShaderD3D11Impl)
                                                     (this, ShaderCreationAttribs ) );
            pShaderD3D11->QueryInterface( IID_Shader, reinterpret_cast<IObject**>(ppShader) );

            OnCreateDeviceObject( pShaderD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl::CreateTextureFromD3DResource(ID3D11Texture1D *pd3d11Texture, ITexture **ppTexture)
{
    if (pd3d11Texture == nullptr)
        return;

    TextureDesc TexDesc;
    TexDesc.Name = "Texture1D from native d3d11 texture";
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureBaseD3D11 *pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture1D_D3D11 instance", Texture1D_D3D11)
                                                        (m_TexViewObjAllocator, this, pd3d11Texture);
            pTextureD3D11->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl::CreateTextureFromD3DResource(ID3D11Texture2D *pd3d11Texture, ITexture **ppTexture)
{
    if (pd3d11Texture == nullptr)
        return;

    TextureDesc TexDesc;
    TexDesc.Name = "Texture2D from native d3d11 texture";
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureBaseD3D11 *pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture2D_D3D11 instance", Texture2D_D3D11)
                                                        (m_TexViewObjAllocator, this, pd3d11Texture);
            pTextureD3D11->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl::CreateTextureFromD3DResource(ID3D11Texture3D *pd3d11Texture, ITexture **ppTexture)
{
    if (pd3d11Texture == nullptr)
        return;

    TextureDesc TexDesc;
    TexDesc.Name = "Texture3D from native d3d11 texture";
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureBaseD3D11 *pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture3D_D3D11 instance", Texture3D_D3D11)
                                                        (m_TexViewObjAllocator, this, pd3d11Texture);
            pTextureD3D11->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D11 );
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
                case RESOURCE_DIM_TEX_1D:
                case RESOURCE_DIM_TEX_1D_ARRAY:
                    pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture1D_D3D11 instance", Texture1D_D3D11)
                                              (m_TexViewObjAllocator, this, TexDesc, Data );
                break;

                case RESOURCE_DIM_TEX_2D:
                case RESOURCE_DIM_TEX_2D_ARRAY:
                case RESOURCE_DIM_TEX_CUBE:
                case RESOURCE_DIM_TEX_CUBE_ARRAY:
                    pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture2D_D3D11 instance", Texture2D_D3D11)
                                              (m_TexViewObjAllocator, this, TexDesc, Data );
                break;

                case RESOURCE_DIM_TEX_3D:
                    pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture3D_D3D11 instance", Texture3D_D3D11)
                                              (m_TexViewObjAllocator, this, TexDesc, Data );
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
                SamplerD3D11Impl *pSamplerD3D11( NEW_RC_OBJ(m_SamplerObjAllocator, "SamplerD3D11Impl instance",  SamplerD3D11Impl)
                                                           (this, SamplerDesc ) );
                pSamplerD3D11->QueryInterface( IID_Sampler, reinterpret_cast<IObject**>(ppSampler) );
                OnCreateDeviceObject( pSamplerD3D11 );
                m_SamplersRegistry.Add( SamplerDesc, *ppSampler );
            }
        }
    );
}

void RenderDeviceD3D11Impl::CreatePipelineState(const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState)
{
    CreateDeviceObject( "Pipeline state", PipelineDesc, ppPipelineState, 
        [&]()
        {
            PipelineStateD3D11Impl *pPipelineStateD3D11( NEW_RC_OBJ(m_PSOAllocator, "PipelineStateD3D11Impl instance", PipelineStateD3D11Impl)
                                                                   (this, PipelineDesc ) );
            pPipelineStateD3D11->QueryInterface( IID_PipelineState, reinterpret_cast<IObject**>(ppPipelineState) );
            OnCreateDeviceObject( pPipelineStateD3D11 );
        } 
    );
}

}
