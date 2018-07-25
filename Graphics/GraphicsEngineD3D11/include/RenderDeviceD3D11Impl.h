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
/// Declaration of Diligent::RenderDeviceD3D11Impl class

#include "RenderDeviceD3D11.h"
#include "RenderDeviceD3DBase.h"
#include "DeviceContextD3D11.h"
#include "EngineD3D11Attribs.h"

namespace Diligent
{

/// Implementation of the Diligent::IRenderDeviceD3D11 interface
class RenderDeviceD3D11Impl : public RenderDeviceD3DBase<IRenderDeviceD3D11>
{
public:
    using TRenderDeviceBase = RenderDeviceD3DBase<IRenderDeviceD3D11>;

    RenderDeviceD3D11Impl( IReferenceCounters*       pRefCounters,
                           IMemoryAllocator&         RawMemAllocator,
                           const EngineD3D11Attribs& EngineAttribs,
                           ID3D11Device*             pd3d11Device,
                           Uint32                    NumDeferredContexts );
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData& BuffData, IBuffer** ppBuffer)override final;

    virtual void CreateShader(const ShaderCreationAttribs& ShaderCreationAttribs, IShader** ppShader)override final;

    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData& Data, ITexture** ppTexture)override final;
    
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler** ppSampler)override final;

    virtual void CreatePipelineState(const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState)override final;

    virtual void CreateFence(const FenceDesc& Desc, IFence** ppFence)override final;

    ID3D11Device* GetD3D11Device()override final{return m_pd3d11Device;}

    virtual void CreateBufferFromD3DResource(ID3D11Buffer* pd3d11Buffer, const BufferDesc& BuffDesc, IBuffer** ppBuffer)override final;
    virtual void CreateTextureFromD3DResource(ID3D11Texture1D* pd3d11Texture, ITexture** ppTexture)override final;
    virtual void CreateTextureFromD3DResource(ID3D11Texture2D* pd3d11Texture, ITexture** ppTexture)override final;
    virtual void CreateTextureFromD3DResource(ID3D11Texture3D* pd3d11Texture, ITexture** ppTexture)override final;

private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat )override final;

    EngineD3D11Attribs m_EngineAttribs;

    /// D3D11 device
    CComPtr<ID3D11Device> m_pd3d11Device;
};

}
