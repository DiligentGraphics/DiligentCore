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

#pragma once

/// \file
/// Declaration of Diligent::RenderDeviceD3D12Impl class

#include "RenderDeviceD3D12.h"
#include "RenderDeviceD3DBase.h"
#include "DescriptorHeap.h"
#include "CommandListManager.h"
#include "CommandContext.h"
#include "DynamicUploadHeap.h"

/// Namespace for the Direct3D11 implementation of the graphics engine
namespace Diligent
{

/// Implementation of the Diligent::IRenderDeviceD3D12 interface
class RenderDeviceD3D12Impl : public RenderDeviceD3DBase<IRenderDeviceD3D12>
{
public:
    typedef RenderDeviceD3DBase<IRenderDeviceD3D12> TRenderDeviceBase;

    RenderDeviceD3D12Impl( IMemoryAllocator &RawMemAllocator, const EngineD3D12Attribs &CreationAttribs, ID3D12Device *pD3D12Device, ID3D12CommandQueue *pd3d12CmdQueue, Uint32 NumDeferredContexts );
    ~RenderDeviceD3D12Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void CreatePipelineState( const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState )override;

    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBuffer);

    virtual void CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader);

    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture);
    
    void CreateTexture(TextureDesc& TexDesc, ID3D12Resource *pd3d12Texture, class TextureD3D12Impl **ppTexture);
    
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler);

    virtual ID3D12Device* GetD3D12Device()override{return m_pd3d12Device;}
    
    DescriptorHeapAllocation AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 );
    DescriptorHeapAllocation AllocateGPUDescriptors( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 );

    bool IsFenceComplete(Uint64 FenceValue);
    bool IsFrameComplete(Uint64 Frame);

    ID3D12CommandQueue *GetCmdQueue(){return m_pd3d12CmdQueue;}

	Uint64 IncrementFence();
	void WaitForFence(Uint64 FenceValue);
	void IdleGPU(bool ReleasePendingObjects = false);
    CommandContext* AllocateCommandContext(const Char *ID = "");
    Uint64 CloseAndExecuteCommandContext(CommandContext *pCtx);
    void DisposeCommandContext(CommandContext*);

    void SafeReleaseD3D12Object(ID3D12Object* pObj);
    Uint64 FinishFrame();
    
    DynamicUploadHeap* RequestUploadHeap();
    void ReleaseUploadHeap(DynamicUploadHeap* pUploadHeap);
    Uint64 GetCurrentFrame()const {return m_CurrentFrame;}

private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat );
    void ProcessReleaseQueue(bool ForceRelease = false);

    /// D3D12 device
    CComPtr<ID3D12Device> m_pd3d12Device;
    CComPtr<ID3D12CommandQueue> m_pd3d12CmdQueue;

    EngineD3D12Attribs m_EngineAttribs;

    CPUDescriptorHeap m_CPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    GPUDescriptorHeap m_GPUDescriptorHeaps[2]; // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == 0
                                               // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER	 == 1

	std::mutex m_FenceMutex;
	std::mutex m_EventMutex;

	CComPtr<ID3D12Fence> m_pFence;
	Uint64 m_NextFenceValue;
	Uint64 m_LastCompletedFenceValue;
	HANDLE m_FenceEventHandle;

    CComPtr<ID3D12Fence> m_pNumCompletedFramesFence;
    Uint64 m_CurrentFrame = 0;
    Uint64 m_NumCompletedFrames = 0;


    CommandListManager m_CmdListManager;

    typedef std::unique_ptr<CommandContext, STDDeleterRawMem<CommandContext> > ContextPoolElemType;
	std::vector< ContextPoolElemType, STDAllocatorRawMem<ContextPoolElemType> > m_ContextPool;

	std::deque<CommandContext*, STDAllocatorRawMem<CommandContext*> > m_AvailableContexts;
	std::mutex m_ContextAllocationMutex;

    // Object that must be kept alive
    std::mutex m_ReleasedObjectsMutex;
    // Release queue
    typedef std::pair<Uint64, CComPtr<ID3D12Object> > ReleaseQueueElemType;
    std::deque< ReleaseQueueElemType, STDAllocatorRawMem<ReleaseQueueElemType> > m_D3D12ObjReleaseQueue;

    std::mutex m_UploadHeapMutex;
    typedef std::unique_ptr<DynamicUploadHeap, STDDeleterRawMem<DynamicUploadHeap> > UploadHeapPoolElemType;
    std::vector< UploadHeapPoolElemType, STDAllocatorRawMem<UploadHeapPoolElemType> > m_UploadHeaps;
};

}
