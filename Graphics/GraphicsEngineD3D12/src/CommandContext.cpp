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

#include "pch.h"
#include "CommandContext.h"
#include "TextureD3D12Impl.h"
#include "BufferD3D12Impl.h"
#include "CommandListManager.h"

namespace Diligent
{

CommandContext::CommandContext( IMemoryAllocator &MemAllocator,
                                CommandListManager &CmdListManager, 
                                GPUDescriptorHeap GPUDescriptorHeaps[],
                                const Uint32 DynamicDescriptorAllocationChunkSize[]) :
	m_pCurGraphicsRootSignature( nullptr),
	m_pCurPipelineState( nullptr),
	m_pCurComputeRootSignature( nullptr),
    m_DynamicGPUDescriptorAllocator
    {
        {MemAllocator, GPUDescriptorHeaps[0], DynamicDescriptorAllocationChunkSize[0]},
        {MemAllocator, GPUDescriptorHeaps[1], DynamicDescriptorAllocationChunkSize[1]}
    },
    m_PendingResourceBarriers( STD_ALLOCATOR_RAW_MEM(D3D12_RESOURCE_BARRIER, GetRawAllocator(), "Allocator for vector<D3D12_RESOURCE_BARRIER>") ),
    m_PendingBarrierObjects( STD_ALLOCATOR_RAW_MEM(RefCntAutoPtr<IDeviceObject>, GetRawAllocator(), "Allocator for vector<RefCntAutoPtr<IDeviceObject>>") )
{
    m_PendingResourceBarriers.reserve(MaxPendingBarriers);
    m_PendingBarrierObjects.reserve(MaxPendingBarriers);

    CmdListManager.CreateNewCommandList(&m_pCommandList, &m_pCurrentAllocator);
}

CommandContext::~CommandContext( void )
{
}



void CommandContext::Reset( CommandListManager& CmdListManager )
{
	// We only call Reset() on previously freed contexts. The command list persists, but we need to
	// request a new allocator if there is none
    // The allocator may not be null if the command context was previously disposed without being executed
	VERIFY_EXPR(m_pCommandList != nullptr);
    if( !m_pCurrentAllocator )
    {
        CmdListManager.RequestAllocator(&m_pCurrentAllocator);
        m_pCommandList->Reset(m_pCurrentAllocator, nullptr);
    }

    m_pCurPipelineState = nullptr;
	m_pCurGraphicsRootSignature = nullptr;
	m_pCurComputeRootSignature = nullptr;
	m_PendingResourceBarriers.clear();
    m_PendingBarrierObjects.clear();
    m_BoundDescriptorHeaps = ShaderDescriptorHeaps();

    m_PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
#if 0
	BindDescriptorHeaps();
#endif
}

ID3D12GraphicsCommandList* CommandContext::Close(ID3D12CommandAllocator **ppAllocator)
{
	FlushResourceBarriers();

	//if (m_ID.length() > 0)
	//	EngineProfiling::EndBlock(this);

	VERIFY_EXPR(m_pCurrentAllocator != nullptr);
	auto hr = m_pCommandList->Close();
    VERIFY(SUCCEEDED(hr), "Failed to close the command list");

    if( ppAllocator != nullptr )
        *ppAllocator = m_pCurrentAllocator.Detach();
    return m_pCommandList;
}



void GraphicsContext::SetRenderTargets( UINT NumRTVs, ITextureViewD3D12** ppRTVs, ITextureViewD3D12* pDSV )
{
    D3D12_CPU_DESCRIPTOR_HANDLE RTVHandles[8]; // Do not waste time initializing array to zero

	for (UINT i = 0; i < NumRTVs; ++i)
	{
        auto *pRTV = ppRTVs[i];
        if( pRTV )
        {
            auto *pTexture = ValidatedCast<TextureD3D12Impl>( pRTV->GetTexture() );
	        TransitionResource(pTexture, D3D12_RESOURCE_STATE_RENDER_TARGET);
		    RTVHandles[i] = pRTV->GetCPUDescriptorHandle();
            VERIFY_EXPR(RTVHandles[i].ptr != 0);
        }
	}

	if (pDSV)
	{
        auto *pTexture = ValidatedCast<TextureD3D12Impl>( pDSV->GetTexture() );
		//if (bReadOnlyDepth)
		//{
		//	TransitionResource(*pTexture, D3D12_RESOURCE_STATE_DEPTH_READ);
		//	m_pCommandList->OMSetRenderTargets( NumRTVs, RTVHandles, FALSE, &DSV->GetDSV_DepthReadOnly() );
		//}
		//else
		{
			TransitionResource(pTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            auto DSVHandle = pDSV->GetCPUDescriptorHandle();
            VERIFY_EXPR(DSVHandle.ptr != 0);
			m_pCommandList->OMSetRenderTargets( NumRTVs, RTVHandles, FALSE, &DSVHandle );
		}
	}
	else if(NumRTVs > 0)
	{
		m_pCommandList->OMSetRenderTargets( NumRTVs, RTVHandles, FALSE, nullptr );
	}
}

void CommandContext::ClearUAVFloat( ITextureViewD3D12 *pTexView, const float* Color )
{
    auto *pTexture = ValidatedCast<TextureD3D12Impl>( pTexView->GetTexture() );
	TransitionResource(pTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	// After binding a UAV, we can get a GPU handle that is required to clear it as a UAV (because it essentially runs
	// a shader to set all of the values).
    UNSUPPORTED("Not yet implemented");
    D3D12_GPU_DESCRIPTOR_HANDLE GpuVisibleHandle = {};//m_DynamicDescriptorHeap.UploadDirect(Target.GetUAV());
	m_pCommandList->ClearUnorderedAccessViewFloat(GpuVisibleHandle, pTexView->GetCPUDescriptorHandle(), pTexture->GetD3D12Resource(), Color, 0, nullptr);
}

void CommandContext::ClearUAVUint( ITextureViewD3D12 *pTexView, const UINT *Color  )
{
    auto *pTexture = ValidatedCast<TextureD3D12Impl>( pTexView->GetTexture() );
	TransitionResource(pTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	// After binding a UAV, we can get a GPU handle that is required to clear it as a UAV (because it essentially runs
	// a shader to set all of the values).
    UNSUPPORTED("Not yet implemented");
    D3D12_GPU_DESCRIPTOR_HANDLE GpuVisibleHandle = {};//m_DynamicDescriptorHeap.UploadDirect(Target.GetUAV());
	//CD3DX12_RECT ClearRect(0, 0, (LONG)Target.GetWidth(), (LONG)Target.GetHeight());

	//TODO: My Nvidia card is not clearing UAVs with either Float or Uint variants.
	m_pCommandList->ClearUnorderedAccessViewUint(GpuVisibleHandle, pTexView->GetCPUDescriptorHandle(), pTexture->GetD3D12Resource(), Color, 0, nullptr/*1, &ClearRect*/);
}


void GraphicsContext::ClearRenderTarget( ITextureViewD3D12 *pRTV, const float *Color )
{
    auto *pTexture = ValidatedCast<TextureD3D12Impl>( pRTV->GetTexture() );
	TransitionResource(pTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	m_pCommandList->ClearRenderTargetView(pRTV->GetCPUDescriptorHandle(), Color, 0, nullptr);
}

void GraphicsContext::ClearDepthStencil( ITextureViewD3D12 *pDSV, D3D12_CLEAR_FLAGS ClearFlags, float Depth, UINT8 Stencil )
{
    auto *pTexture = ValidatedCast<TextureD3D12Impl>( pDSV->GetTexture() );
	TransitionResource( pTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
	m_pCommandList->ClearDepthStencilView(pDSV->GetCPUDescriptorHandle(), ClearFlags, Depth, Stencil, 0, nullptr);
}


void CommandContext::TransitionResource(ITextureD3D12 *pTexture, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
    VERIFY_EXPR( pTexture != nullptr );
    auto *pTexD3D12 = ValidatedCast<TextureD3D12Impl>(pTexture);
    TransitionResource(*pTexD3D12, *pTexD3D12, NewState, FlushImmediate);
}

void CommandContext::TransitionResource(IBufferD3D12 *pBuffer, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
    VERIFY_EXPR( pBuffer != nullptr );
    auto *pBuffD3D12 = ValidatedCast<BufferD3D12Impl>(pBuffer);

#ifdef _DEBUG
    // Dynamic buffers wtih no SRV/UAV bind flags are suballocated in 
    // the upload heap when Map() is called and must always be in 
    // D3D12_RESOURCE_STATE_GENERIC_READ state
    if(pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC && (pBuffD3D12->GetDesc().BindFlags & (BIND_SHADER_RESOURCE|BIND_UNORDERED_ACCESS)) == 0)
    {
        VERIFY(pBuffD3D12->GetState() == D3D12_RESOURCE_STATE_GENERIC_READ, "Dynamic buffers that cannot be bound as SRV or UAV are expected to always be in D3D12_RESOURCE_STATE_GENERIC_READ state");
        VERIFY( (NewState & D3D12_RESOURCE_STATE_GENERIC_READ) == NewState, "Dynamic buffers can only transition to one of D3D12_RESOURCE_STATE_GENERIC_READ states");
    }
#endif

    TransitionResource(*pBuffD3D12, *pBuffD3D12, NewState, FlushImmediate);

#ifdef _DEBUG
    if(pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC && (pBuffD3D12->GetDesc().BindFlags & (BIND_SHADER_RESOURCE|BIND_UNORDERED_ACCESS)) == 0)
        VERIFY(pBuffD3D12->GetState() == D3D12_RESOURCE_STATE_GENERIC_READ, "Dynamic buffers without SRV/UAV bind flag are expected to never transition from D3D12_RESOURCE_STATE_GENERIC_READ state");
#endif
}

void CommandContext::TransitionResource(D3D12ResourceBase& Resource, IDeviceObject &Object, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
	D3D12_RESOURCE_STATES OldState = Resource.GetState();

    // Check if required state is already set
	if ( (OldState & NewState) != NewState || NewState == 0 && OldState != 0 )
	{
        // If both old state and new state are read-only states, combine the two
        if( (OldState & D3D12_RESOURCE_STATE_GENERIC_READ) == OldState &&
            (NewState & D3D12_RESOURCE_STATE_GENERIC_READ) == NewState )
            NewState |= OldState;
        
        m_PendingResourceBarriers.emplace_back();
        m_PendingBarrierObjects.emplace_back(&Object);
		D3D12_RESOURCE_BARRIER& BarrierDesc = m_PendingResourceBarriers.back();

		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Transition.pResource = Resource.GetD3D12Resource();
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = OldState;
		BarrierDesc.Transition.StateAfter = NewState;

		// Check to see if we already started the transition
#if 0
		if (NewState == Resource.m_TransitioningState)
		{
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
			Resource.m_TransitioningState = (D3D12_RESOURCE_STATES)-1;
		}
		else
#endif
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		Resource.SetState( NewState );
	}
	else if (NewState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		InsertUAVBarrier(Resource, Object, FlushImmediate);

	if (FlushImmediate || m_PendingResourceBarriers.size() >= MaxPendingBarriers)
        FlushResourceBarriers();
}

void CommandContext::FlushResourceBarriers()
{
	if (m_PendingResourceBarriers.empty())
    {
        VERIFY_EXPR(m_PendingBarrierObjects.empty());
		return;
    }

	m_pCommandList->ResourceBarrier(static_cast<UINT>(m_PendingResourceBarriers.size()), m_PendingResourceBarriers.data());
	m_PendingResourceBarriers.clear();
    m_PendingBarrierObjects.clear();
}


void CommandContext::InsertUAVBarrier(D3D12ResourceBase& Resource, IDeviceObject &Object, bool FlushImmediate)
{
    m_PendingResourceBarriers.emplace_back();
    m_PendingBarrierObjects.emplace_back(&Object);
	D3D12_RESOURCE_BARRIER& BarrierDesc = m_PendingResourceBarriers.back();

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.UAV.pResource = Resource.GetD3D12Resource();

	if (FlushImmediate)
        FlushResourceBarriers();
}

void CommandContext::InsertAliasBarrier(D3D12ResourceBase& Before, D3D12ResourceBase& After, IDeviceObject &BeforeObj, IDeviceObject &AfterObj, bool FlushImmediate)
{
    m_PendingResourceBarriers.emplace_back();
    m_PendingBarrierObjects.emplace_back(&BeforeObj);
    m_PendingBarrierObjects.emplace_back(&AfterObj);
	D3D12_RESOURCE_BARRIER& BarrierDesc = m_PendingResourceBarriers.back();

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Aliasing.pResourceBefore = Before.GetD3D12Resource();
	BarrierDesc.Aliasing.pResourceAfter = After.GetD3D12Resource();

	if (FlushImmediate)
        FlushResourceBarriers();
}

void CommandContext::DiscardDynamicDescriptors(Uint64 FenceValue)
{
    for(size_t HeapType = 0; HeapType < _countof(m_DynamicGPUDescriptorAllocator); ++HeapType)
        m_DynamicGPUDescriptorAllocator[HeapType].DiscardAllocations(FenceValue);
}

DescriptorHeapAllocation CommandContext::AllocateDynamicGPUVisibleDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count )
{
    VERIFY(Type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && Type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "Invalid heap type");
    return m_DynamicGPUDescriptorAllocator[Type].Allocate(Count);
}

}
