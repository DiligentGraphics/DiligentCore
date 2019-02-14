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

#pragma once

/// \file
/// Declaration of Diligent::RenderDeviceMtlImpl class

#include "RenderDeviceMtl.h"
#include "RenderDeviceBase.h"
#include "DeviceContextMtl.h"
#include "EngineMtlAttribs.h"

namespace Diligent
{

/// Implementation of the Diligent::IRenderDeviceMtl interface
class RenderDeviceMtlImpl final : public RenderDeviceBase<IRenderDeviceMtl>
{
public:
    using TRenderDeviceBase = RenderDeviceBase<IRenderDeviceMtl>;

    RenderDeviceMtlImpl( IReferenceCounters*     pRefCounters,
                         IMemoryAllocator&       RawMemAllocator,
                         const EngineMtlAttribs& EngineAttribs,
                         void*                   pMtlDevice,
                         Uint32                  NumDeferredContexts );
    virtual void QueryInterface( const INTERFACE_ID& IID, IObject **ppInterface )override final;

    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData* pBuffData, IBuffer** ppBuffer)override final;

    virtual void CreateShader(const ShaderCreationAttribs& ShaderCreationAttribs, IShader** ppShader)override final;

    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData* pData, ITexture** ppTexture)override final;
    
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler** ppSampler)override final;

    virtual void CreatePipelineState(const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState)override final;

    virtual void CreateFence(const FenceDesc& Desc, IFence** ppFence)override final;

    virtual void ReleaseStaleResources(bool ForceRelease = false)override final {}

    size_t GetCommandQueueCount()const { return 1; }
    Uint64 GetCommandQueueMask()const { return Uint64{1};}

private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat )override final;

    EngineMtlAttribs m_EngineAttribs;

};

}
