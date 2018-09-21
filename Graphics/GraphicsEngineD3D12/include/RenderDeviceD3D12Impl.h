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
#include "ResourceReleaseQueue.h"

/// Namespace for the Direct3D11 implementation of the graphics engine
namespace Diligent
{

/// Implementation of the Diligent::IRenderDeviceD3D12 interface
class RenderDeviceD3D12Impl final : public RenderDeviceNextGenBase< RenderDeviceD3DBase<IRenderDeviceD3D12>, ICommandQueueD3D12 >
{
public:
    using TRenderDeviceBase = RenderDeviceNextGenBase< RenderDeviceD3DBase<IRenderDeviceD3D12>, ICommandQueueD3D12 >;

    RenderDeviceD3D12Impl( IReferenceCounters*       pRefCounters, 
                           IMemoryAllocator&         RawMemAllocator, 
                           const EngineD3D12Attribs& CreationAttribs, 
                           ID3D12Device*             pD3D12Device, 
                           ICommandQueueD3D12*       pCmdQueue, 
                           Uint32                    NumDeferredContexts );
    ~RenderDeviceD3D12Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

    virtual void CreatePipelineState( const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState )override final;

    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer)override final;

    virtual void CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader)override final;

    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)override final;
    
    void CreateTexture(const TextureDesc& TexDesc, ID3D12Resource *pd3d12Texture, class TextureD3D12Impl **ppTexture);
    
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler)override final;

    virtual void CreateFence(const FenceDesc& Desc, IFence** ppFence)override final;

    virtual ID3D12Device* GetD3D12Device()override final{return m_pd3d12Device;}
    
    virtual void CreateTextureFromD3DResource(ID3D12Resource *pd3d12Texture, ITexture **ppTexture)override final;

    virtual void CreateBufferFromD3DResource(ID3D12Resource *pd3d12Buffer, const BufferDesc& BuffDesc, IBuffer **ppBuffer)override final;

    DescriptorHeapAllocation AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 );
    DescriptorHeapAllocation AllocateGPUDescriptors( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 );

    Uint64 GetCompletedFenceValue();
	virtual Uint64 GetNextFenceValue() override final
    {
        return m_pCommandQueue->GetNextFenceValue();
    }

	Uint64 GetCurrentFrameNumber()const {return static_cast<Uint64>(m_FrameNumber);}
    virtual Bool IsFenceSignaled(Uint64 FenceValue) override final;

    ICommandQueueD3D12 *GetCmdQueue(){return m_pCommandQueue;}

	void IdleGPU(bool ReleaseStaleObjects);
    CommandContext* AllocateCommandContext(const Char *ID = "");
    Uint64 CloseAndExecuteCommandContext(CommandContext *pCtx, bool DiscardStaleObjects, std::vector<std::pair<Uint64, RefCntAutoPtr<IFence> > >* pSignalFences);
    void DisposeCommandContext(CommandContext*);

    void SafeReleaseD3D12Object(ID3D12Object* pObj);
    void FinishFrame(bool ReleaseAllResources);
    virtual void FinishFrame()override final { FinishFrame(false); }

    D3D12DynamicMemoryManager& GetDynamicMemoryManager() {return m_DynamicMemoryManager;}

private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat )override final;

    /// D3D12 device
    CComPtr<ID3D12Device> m_pd3d12Device;
    RefCntAutoPtr<ICommandQueueD3D12> m_pCommandQueue;

    EngineD3D12Attribs m_EngineAttribs;

    CPUDescriptorHeap m_CPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    GPUDescriptorHeap m_GPUDescriptorHeaps[2]; // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == 0
                                               // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER	 == 1
	
	const Uint32 m_DynamicDescriptorAllocationChunkSize[2];

	std::mutex m_CmdQueueMutex;

	Atomics::AtomicInt64 m_FrameNumber;
    Atomics::AtomicInt64 m_NextCmdListNumber;

    // The following basic requirement guarantees correctness of resource deallocation:
    //
    //        A resource is never released before the last draw command referencing it is invoked on the immediate context
    //
    // See http://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-resource-lifetimes/

    //
    // CPU
    //                       Last Reference
    //                        of resource X
    //                             |
    //                             |     Submit Cmd       Submit Cmd            Submit Cmd
    //                             |      List N           List N+1              List N+2
    //                             V         |                |                     |
    //    NextFenceValue       |   *  N      |      N+1       |          N+2        |
    //
    //
    //    CompletedFenceValue       |     N-3      |      N-2      |        N-1        |        N       |
    //                              .              .               .                   .                .
    // -----------------------------.--------------.---------------.-------------------.----------------.-------------
    //                              .              .               .                   .                .
    //       
    // GPU                          | Cmd List N-2 | Cmd List N-1  |    Cmd List N     |   Cmd List N+1 |
    //                                                                                 |
    //                                                                                 |
    //                                                                          Resource X can
    //                                                                           be released

    CommandListManager m_CmdListManager;

    typedef std::unique_ptr<CommandContext, STDDeleterRawMem<CommandContext> > ContextPoolElemType;
	std::vector< ContextPoolElemType, STDAllocatorRawMem<ContextPoolElemType> > m_ContextPool;

	std::deque<CommandContext*, STDAllocatorRawMem<CommandContext*> > m_AvailableContexts;
	std::mutex m_ContextAllocationMutex;

    ResourceReleaseQueue<StaticStaleResourceWrapper<CComPtr<ID3D12Object>>> m_ReleaseQueue;

    D3D12DynamicMemoryManager m_DynamicMemoryManager;
};

}
