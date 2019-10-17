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

#pragma once

/// \file
/// Declaration of Diligent::RenderDeviceD3D12Impl class
#include "RenderDeviceD3D12.h"
#include "RenderDeviceD3DBase.h"
#include "RenderDeviceNextGenBase.h"
#include "DescriptorHeap.h"
#include "CommandListManager.h"
#include "CommandContext.h"
#include "D3D12DynamicHeap.h"
#include "Atomics.h"
#include "CommandQueueD3D12.h"
#include "GenerateMips.h"

namespace Diligent
{

/// Implementation of the Diligent::IRenderDeviceD3D12 interface
class RenderDeviceD3D12Impl final : public RenderDeviceNextGenBase< RenderDeviceD3DBase<IRenderDeviceD3D12>, ICommandQueueD3D12 >
{
public:
    using TRenderDeviceBase = RenderDeviceNextGenBase< RenderDeviceD3DBase<IRenderDeviceD3D12>, ICommandQueueD3D12 >;

    RenderDeviceD3D12Impl(IReferenceCounters*          pRefCounters, 
                          IMemoryAllocator&            RawMemAllocator,
                          IEngineFactory*              pEngineFactory,
                          const EngineD3D12CreateInfo& EngineCI, 
                          ID3D12Device*                pD3D12Device, 
                          size_t                       CommandQueueCount,
                          ICommandQueueD3D12**         ppCmdQueues);
    ~RenderDeviceD3D12Impl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override final;

    virtual void CreatePipelineState(const PipelineStateDesc& PipelineDesc, IPipelineState** ppPipelineState)override final;

    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData* pBuffData, IBuffer** ppBuffer)override final;

    virtual void CreateShader(const ShaderCreateInfo& ShaderCreateInfo, IShader** ppShader)override final;

    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData* pData, ITexture** ppTexture)override final;
    
    void CreateTexture(const TextureDesc& TexDesc, ID3D12Resource* pd3d12Texture, RESOURCE_STATE InitialState, class TextureD3D12Impl** ppTexture);
    
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler** ppSampler)override final;

    virtual void CreateFence(const FenceDesc& Desc, IFence** ppFence)override final;

    virtual ID3D12Device* GetD3D12Device()override final{return m_pd3d12Device;}
    
    virtual void CreateTextureFromD3DResource(ID3D12Resource* pd3d12Texture, RESOURCE_STATE InitialState, ITexture** ppTexture)override final;

    virtual void CreateBufferFromD3DResource(ID3D12Resource* pd3d12Buffer, const BufferDesc& BuffDesc, RESOURCE_STATE InitialState, IBuffer** ppBuffer)override final;

    DescriptorHeapAllocation AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 );
    DescriptorHeapAllocation AllocateGPUDescriptors( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 );

	virtual void IdleGPU()override final;

    using PooledCommandContext = std::unique_ptr<CommandContext, STDDeleterRawMem<CommandContext> >;
    PooledCommandContext AllocateCommandContext(const Char* ID = "");
    void CloseAndExecuteTransientCommandContext(Uint32 CommandQueueIndex, PooledCommandContext&& Ctx);
    Uint64 CloseAndExecuteCommandContext(Uint32 QueueIndex, PooledCommandContext&& Ctx, bool DiscardStaleObjects, std::vector<std::pair<Uint64, RefCntAutoPtr<IFence> > >* pSignalFences);

    void SignalFences(Uint32 QueueIndex, std::vector<std::pair<Uint64, RefCntAutoPtr<IFence> > >& SignalFences);
    
    // Disposes an unused command context
    void DisposeCommandContext(PooledCommandContext&& Ctx);

    void FlushStaleResources(Uint32 CmdQueueIndex);
    virtual void ReleaseStaleResources(bool ForceRelease = false)override final;

    D3D12DynamicMemoryManager& GetDynamicMemoryManager() {return m_DynamicMemoryManager;}

    GPUDescriptorHeap& GetGPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
    {
        VERIFY_EXPR(Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        return m_GPUDescriptorHeaps[Type];
    }

    const GenerateMipsHelper& GetMipsGenerator()const {return m_MipsGenerator;}

    D3D_FEATURE_LEVEL GetD3DFeatureLevel()const;

private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat )override final;
    void FreeCommandContext(PooledCommandContext&& Ctx);

    CComPtr<ID3D12Device> m_pd3d12Device;

    EngineD3D12CreateInfo m_EngineAttribs;

    CPUDescriptorHeap m_CPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    GPUDescriptorHeap m_GPUDescriptorHeaps[2]; // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == 0
                                               // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER	 == 1
	
    CommandListManager m_CmdListManager;

    std::mutex m_ContextPoolMutex;
	std::vector< PooledCommandContext, STDAllocatorRawMem<PooledCommandContext> > m_ContextPool;
#ifdef DEVELOPMENT
    Atomics::AtomicLong m_AllocatedCtxCounter = 0;
#endif

    D3D12DynamicMemoryManager m_DynamicMemoryManager;

    // Note: mips generator must be released after the device has been idled
    GenerateMipsHelper m_MipsGenerator;
};

}
