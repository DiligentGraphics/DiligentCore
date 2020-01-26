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

#include "RenderDeviceMtlImpl.h"
#include "DeviceContextMtlImpl.h"
#include "BufferMtlImpl.h"
#include "ShaderMtlImpl.h"
#include "TextureMtlImpl.h"
#include "SamplerMtlImpl.h"
#include "MtlTypeConversions.h"
#include "TextureViewMtlImpl.h"
#include "PipelineStateMtlImpl.h"
#include "ShaderResourceBindingMtlImpl.h"
#include "FenceMtlImpl.h"
#include "EngineMemory.h"

namespace Diligent
{

RenderDeviceMtlImpl :: RenderDeviceMtlImpl(IReferenceCounters*        pRefCounters,
                                           IMemoryAllocator&          RawMemAllocator,
                                           IEngineFactory*            pEngineFactory,
                                           const EngineMtlCreateInfo& EngineAttribs,
                                           void*                      pMtlDevice) : 
    TRenderDeviceBase
    {
        pRefCounters,
        RawMemAllocator,
        pEngineFactory,
        EngineAttribs.NumDeferredContexts,
        DeviceObjectSizes
        {
            sizeof(TextureMtlImpl),
            sizeof(TextureViewMtlImpl),
            sizeof(BufferMtlImpl),
            sizeof(BufferViewMtlImpl),
            sizeof(ShaderMtlImpl),
            sizeof(SamplerMtlImpl),
            sizeof(PipelineStateMtlImpl),
            sizeof(ShaderResourceBindingMtlImpl),
            sizeof(FenceMtlImpl)
        }
    },
    m_EngineAttribs(EngineAttribs)
{
    m_DeviceCaps.DevType      = RENDER_DEVICE_TYPE_METAL;
    m_DeviceCaps.MajorVersion = 1;
    m_DeviceCaps.MinorVersion = 0;
    m_DeviceCaps.Features.SeparablePrograms             = True;
    m_DeviceCaps.Features.MultithreadedResourceCreation = True;
    m_DeviceCaps.Features.GeometryShaders               = False;
    m_DeviceCaps.Features.Tessellation                  = False;
}

void RenderDeviceMtlImpl::TestTextureFormat( TEXTURE_FORMAT TexFormat )
{
    auto &TexFormatInfo = m_TextureFormatsInfo[TexFormat];
    VERIFY( TexFormatInfo.Supported, "Texture format is not supported" );

    LOG_ERROR_MESSAGE("RenderDeviceMtlImpl::TestTextureFormat() is not implemented");
}

IMPLEMENT_QUERY_INTERFACE( RenderDeviceMtlImpl, IID_RenderDeviceMtl, TRenderDeviceBase )

void RenderDeviceMtlImpl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData* pBuffData, IBuffer** ppBuffer)
{
    CreateDeviceObject("buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferMtlImpl* pBufferMtl( NEW_RC_OBJ(m_BufObjAllocator, "BufferMtlImpl instance", BufferMtlImpl)
                                                 (m_BuffViewObjAllocator, this, BuffDesc, pBuffData ) );
            pBufferMtl->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferMtl->CreateDefaultViews();
            OnCreateDeviceObject( pBufferMtl );
        } 
    );
}

void RenderDeviceMtlImpl :: CreateShader(const ShaderCreateInfo& ShaderCI, IShader** ppShader)
{
    CreateDeviceObject( "shader", ShaderCI.Desc, ppShader, 
        [&]()
        {
            ShaderMtlImpl* pShaderMtl( NEW_RC_OBJ(m_ShaderObjAllocator, "ShaderMtlImpl instance", ShaderMtlImpl)
                                                 (this, ShaderCI ) );
            pShaderMtl->QueryInterface( IID_Shader, reinterpret_cast<IObject**>(ppShader) );

            OnCreateDeviceObject( pShaderMtl );
        } 
    );
}



void RenderDeviceMtlImpl :: CreateTexture(const TextureDesc& TexDesc, const TextureData* pData, ITexture** ppTexture)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            TextureMtlImpl *pTextureMtl = NEW_RC_OBJ(m_TexObjAllocator, "TextureMtlImpl instance", TextureMtlImpl)(m_TexViewObjAllocator, this, TexDesc, pData );

            pTextureMtl->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureMtl->CreateDefaultViews();
            OnCreateDeviceObject( pTextureMtl );
        } 
    );
}

void RenderDeviceMtlImpl :: CreateSampler(const SamplerDesc& SamplerDesc, ISampler** ppSampler)
{
    CreateDeviceObject( "sampler", SamplerDesc, ppSampler, 
        [&]()
        {
            m_SamplersRegistry.Find( SamplerDesc, reinterpret_cast<IDeviceObject**>(ppSampler) );
            if(* ppSampler == nullptr )
            {
                SamplerMtlImpl* pSamplerMtl( NEW_RC_OBJ(m_SamplerObjAllocator, "SamplerMtlImpl instance",  SamplerMtlImpl)
                                                           (this, SamplerDesc ) );
                pSamplerMtl->QueryInterface( IID_Sampler, reinterpret_cast<IObject**>(ppSampler) );
                OnCreateDeviceObject( pSamplerMtl );
                m_SamplersRegistry.Add( SamplerDesc,* ppSampler );
            }
        }
    );
}


void RenderDeviceMtlImpl::CreatePipelineState(const PipelineStateDesc& PipelineDesc, IPipelineState** ppPipelineState)
{
    CreateDeviceObject( "Pipeline state", PipelineDesc, ppPipelineState, 
        [&]()
        {
            PipelineStateMtlImpl* pPipelineStateMtl( NEW_RC_OBJ(m_PSOAllocator, "PipelineStateMtlImpl instance", PipelineStateMtlImpl)
                                                                   (this, PipelineDesc ) );
            pPipelineStateMtl->QueryInterface( IID_PipelineState, reinterpret_cast<IObject**>(ppPipelineState) );
            OnCreateDeviceObject( pPipelineStateMtl );
        } 
    );
}

void RenderDeviceMtlImpl::CreateFence(const FenceDesc& Desc, IFence** ppFence)
{
    CreateDeviceObject( "Fence", Desc, ppFence, 
        [&]()
        {
            FenceMtlImpl* pFenceMtl( NEW_RC_OBJ(m_FenceAllocator, "FenceMtlImpl instance", FenceMtlImpl)
                                               (this, Desc) );
            pFenceMtl->QueryInterface( IID_Fence, reinterpret_cast<IObject**>(ppFence) );
            OnCreateDeviceObject( pFenceMtl );
        }
    );
}

void RenderDeviceMtlImpl::CreateQuery(const QueryDesc& Desc, IQuery** ppQuery)
{
    CreateDeviceObject( "Query", Desc, ppQuery, 
        [&]()
        {
            QueryMtlImpl* pQueryMtl( NEW_RC_OBJ(m_QueryAllocator, "QueryMtlImpl instance", QueryMtlImpl)
                                               (this, Desc) );
            pQueryMtl->QueryInterface( IID_Query, reinterpret_cast<IObject**>(ppQuery) );
            OnCreateDeviceObject( pQueryMtl );
        }
    );
}

void RenderDeviceMtlImpl::IdleGPU()
{
    LOG_ERROR_MESSAGE("RenderDeviceMtlImpl::IdleGPU() is not implemented");
}

}
