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
#include "FenceD3D11Impl.h"
#include "EngineMemory.h"

namespace Diligent
{

RenderDeviceD3D11Impl :: RenderDeviceD3D11Impl(IReferenceCounters*          pRefCounters,
                                               IMemoryAllocator&            RawMemAllocator,
                                               IEngineFactory*              pEngineFactory,
                                               const EngineD3D11CreateInfo& EngineAttribs,
                                               ID3D11Device*                pd3d11Device,
                                               Uint32                       NumDeferredContexts) : 
    TRenderDeviceBase
    {
        pRefCounters,
        RawMemAllocator,
        pEngineFactory,
        NumDeferredContexts,
        DeviceObjectSizes
        {
            sizeof(TextureBaseD3D11),
            sizeof(TextureViewD3D11Impl),
            sizeof(BufferD3D11Impl),
            sizeof(BufferViewD3D11Impl),
            sizeof(ShaderD3D11Impl),
            sizeof(SamplerD3D11Impl),
            sizeof(PipelineStateD3D11Impl),
            sizeof(ShaderResourceBindingD3D11Impl),
            sizeof(FenceD3D11Impl)
        }
    },
    m_EngineAttribs{EngineAttribs},
    m_pd3d11Device {pd3d11Device }
{
    m_DeviceCaps.DevType = DeviceType::D3D11;
    auto FeatureLevel = m_pd3d11Device->GetFeatureLevel();
    switch (FeatureLevel)
    {
        case D3D_FEATURE_LEVEL_11_0:
        case D3D_FEATURE_LEVEL_11_1:
            m_DeviceCaps.MajorVersion = 11;
            m_DeviceCaps.MinorVersion = FeatureLevel == D3D_FEATURE_LEVEL_11_1 ? 1 : 0;
        break;
        
        case D3D_FEATURE_LEVEL_10_0:
        case D3D_FEATURE_LEVEL_10_1:
            m_DeviceCaps.MajorVersion = 10;
            m_DeviceCaps.MinorVersion = FeatureLevel == D3D_FEATURE_LEVEL_10_1 ? 1 : 0;
        break;

        default:
            UNEXPECTED("Unexpected D3D feature level");
    }
    m_DeviceCaps.bSeparableProgramSupported              = True;
    m_DeviceCaps.bMultithreadedResourceCreationSupported = True;

    // Direct3D11 only supports shader model 5.0 even if the device feature level is
    // above 11.0 (for example, 11.1 or 12.0), so bindless resources are never available.
    // https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro#overview-for-each-feature-level
    m_DeviceCaps.bBindlessSupported = False;
}

void RenderDeviceD3D11Impl::TestTextureFormat( TEXTURE_FORMAT TexFormat )
{
    auto &TexFormatInfo = m_TextureFormatsInfo[TexFormat];
    VERIFY( TexFormatInfo.Supported, "Texture format is not supported" );

    auto DXGIFormat = TexFormatToDXGI_Format(TexFormat);
   
    UINT FormatSupport = 0;
    auto hr = m_pd3d11Device->CheckFormatSupport(DXGIFormat, &FormatSupport);
    if (FAILED(hr))
    {
        LOG_ERROR_MESSAGE("CheckFormatSupport() failed for format ", DXGIFormat);
        return;
    }

    TexFormatInfo.Filterable      = ((FormatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0) || 
                                    ((FormatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON) != 0);
    TexFormatInfo.ColorRenderable = (FormatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0;
    TexFormatInfo.DepthRenderable = (FormatSupport & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) != 0;
    TexFormatInfo.Tex1DFmt        = (FormatSupport & D3D11_FORMAT_SUPPORT_TEXTURE1D) != 0;
    TexFormatInfo.Tex2DFmt        = (FormatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
    TexFormatInfo.Tex3DFmt        = (FormatSupport & D3D11_FORMAT_SUPPORT_TEXTURE3D) != 0;
    TexFormatInfo.TexCubeFmt      = (FormatSupport & D3D11_FORMAT_SUPPORT_TEXTURECUBE) != 0;

    TexFormatInfo.SampleCounts = 0x0;
    for(Uint32 SampleCount = 1; SampleCount <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; SampleCount *= 2)
    {
        UINT QualityLevels = 0;
        hr = m_pd3d11Device->CheckMultisampleQualityLevels(DXGIFormat, SampleCount, &QualityLevels);
        if(SUCCEEDED(hr) && QualityLevels > 0)
            TexFormatInfo.SampleCounts |= SampleCount;
    }
}

IMPLEMENT_QUERY_INTERFACE( RenderDeviceD3D11Impl, IID_RenderDeviceD3D11, TRenderDeviceBase )

void RenderDeviceD3D11Impl :: CreateBufferFromD3DResource(ID3D11Buffer* pd3d11Buffer, const BufferDesc& BuffDesc, RESOURCE_STATE InitialState, IBuffer** ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D11Impl* pBufferD3D11( NEW_RC_OBJ(m_BufObjAllocator, "BufferD3D11Impl instance", BufferD3D11Impl)
                                                     (m_BuffViewObjAllocator, this, BuffDesc, InitialState, pd3d11Buffer ) );
            pBufferD3D11->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pBufferD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData* pBuffData, IBuffer** ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferD3D11Impl* pBufferD3D11( NEW_RC_OBJ(m_BufObjAllocator, "BufferD3D11Impl instance", BufferD3D11Impl)
                                                     (m_BuffViewObjAllocator, this, BuffDesc, pBuffData ) );
            pBufferD3D11->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pBufferD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl :: CreateShader(const ShaderCreateInfo& ShaderCI, IShader** ppShader)
{
    CreateDeviceObject( "shader", ShaderCI.Desc, ppShader, 
        [&]()
        {
            ShaderD3D11Impl* pShaderD3D11( NEW_RC_OBJ(m_ShaderObjAllocator, "ShaderD3D11Impl instance", ShaderD3D11Impl)
                                                     (this, ShaderCI) );
            pShaderD3D11->QueryInterface( IID_Shader, reinterpret_cast<IObject**>(ppShader) );

            OnCreateDeviceObject( pShaderD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl::CreateTextureFromD3DResource(ID3D11Texture1D* pd3d11Texture, RESOURCE_STATE InitialState, ITexture** ppTexture)
{
    if (pd3d11Texture == nullptr)
        return;

    TextureDesc TexDesc;
    TexDesc.Name = "Texture1D from native d3d11 texture";
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureBaseD3D11* pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture1D_D3D11 instance", Texture1D_D3D11)
                                                        (m_TexViewObjAllocator, this, InitialState, pd3d11Texture);
            pTextureD3D11->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl::CreateTextureFromD3DResource(ID3D11Texture2D* pd3d11Texture, RESOURCE_STATE InitialState, ITexture** ppTexture)
{
    if (pd3d11Texture == nullptr)
        return;

    TextureDesc TexDesc;
    TexDesc.Name = "Texture2D from native d3d11 texture";
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureBaseD3D11* pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture2D_D3D11 instance", Texture2D_D3D11)
                                                        (m_TexViewObjAllocator, this, InitialState, pd3d11Texture);
            pTextureD3D11->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl::CreateTextureFromD3DResource(ID3D11Texture3D* pd3d11Texture, RESOURCE_STATE InitialState, ITexture** ppTexture)
{
    if (pd3d11Texture == nullptr)
        return;

    TextureDesc TexDesc;
    TexDesc.Name = "Texture3D from native d3d11 texture";
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureBaseD3D11* pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture3D_D3D11 instance", Texture3D_D3D11)
                                                        (m_TexViewObjAllocator, this, InitialState, pd3d11Texture);
            pTextureD3D11->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D11 );
        } 
    );
}


void RenderDeviceD3D11Impl :: CreateTexture(const TextureDesc& TexDesc, const TextureData* pData, ITexture** ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureBaseD3D11* pTextureD3D11 = nullptr;
            switch( TexDesc.Type )
            {
                case RESOURCE_DIM_TEX_1D:
                case RESOURCE_DIM_TEX_1D_ARRAY:
                    pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture1D_D3D11 instance", Texture1D_D3D11)
                                              (m_TexViewObjAllocator, this, TexDesc, pData );
                break;

                case RESOURCE_DIM_TEX_2D:
                case RESOURCE_DIM_TEX_2D_ARRAY:
                case RESOURCE_DIM_TEX_CUBE:
                case RESOURCE_DIM_TEX_CUBE_ARRAY:
                    pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture2D_D3D11 instance", Texture2D_D3D11)
                                              (m_TexViewObjAllocator, this, TexDesc, pData );
                break;

                case RESOURCE_DIM_TEX_3D:
                    pTextureD3D11 = NEW_RC_OBJ(m_TexObjAllocator, "Texture3D_D3D11 instance", Texture3D_D3D11)
                                              (m_TexViewObjAllocator, this, TexDesc, pData );
                break;

                default: LOG_ERROR_AND_THROW( "Unknown texture type. (Did you forget to initialize the Type member of TextureDesc structure?)" );
            }
            pTextureD3D11->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureD3D11->CreateDefaultViews();
            OnCreateDeviceObject( pTextureD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl :: CreateSampler(const SamplerDesc& SamplerDesc, ISampler** ppSampler)
{
    CreateDeviceObject( "sampler", SamplerDesc, ppSampler, 
        [&]()
        {
            m_SamplersRegistry.Find( SamplerDesc, reinterpret_cast<IDeviceObject**>(ppSampler) );
            if(* ppSampler == nullptr )
            {
                SamplerD3D11Impl* pSamplerD3D11( NEW_RC_OBJ(m_SamplerObjAllocator, "SamplerD3D11Impl instance",  SamplerD3D11Impl)
                                                           (this, SamplerDesc ) );
                pSamplerD3D11->QueryInterface( IID_Sampler, reinterpret_cast<IObject**>(ppSampler) );
                OnCreateDeviceObject( pSamplerD3D11 );
                m_SamplersRegistry.Add( SamplerDesc,* ppSampler );
            }
        }
    );
}

void RenderDeviceD3D11Impl::CreatePipelineState(const PipelineStateDesc& PipelineDesc, IPipelineState** ppPipelineState)
{
    CreateDeviceObject( "Pipeline state", PipelineDesc, ppPipelineState, 
        [&]()
        {
            PipelineStateD3D11Impl* pPipelineStateD3D11( NEW_RC_OBJ(m_PSOAllocator, "PipelineStateD3D11Impl instance", PipelineStateD3D11Impl)
                                                                   (this, PipelineDesc ) );
            pPipelineStateD3D11->QueryInterface( IID_PipelineState, reinterpret_cast<IObject**>(ppPipelineState) );
            OnCreateDeviceObject( pPipelineStateD3D11 );
        } 
    );
}

void RenderDeviceD3D11Impl::CreateFence(const FenceDesc& Desc, IFence** ppFence)
{
    CreateDeviceObject( "Fence", Desc, ppFence, 
        [&]()
        {
            FenceD3D11Impl* pFenceD3D11( NEW_RC_OBJ(m_FenceAllocator, "FenceD3D11Impl instance", FenceD3D11Impl)
                                                   (this, Desc) );
            pFenceD3D11->QueryInterface( IID_Fence, reinterpret_cast<IObject**>(ppFence) );
            OnCreateDeviceObject( pFenceD3D11 );
        }
    );
}

void RenderDeviceD3D11Impl::IdleGPU()
{
    if (auto pImmediateCtx = m_wpImmediateContext.Lock())
    {
        pImmediateCtx->WaitForIdle();
    }
}

}
