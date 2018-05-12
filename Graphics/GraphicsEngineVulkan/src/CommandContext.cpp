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
#include "TextureVkImpl.h"
#include "BufferVkImpl.h"

#if 0
namespace Diligent
{

CommandContext::CommandContext( IMemoryAllocator &MemAllocator,
                                CommandListManager &CmdListManager, 
                                GPUDescriptorHeap GPUDescriptorHeaps[],
                                const Uint32 DynamicDescriptorAllocationChunkSize[])/* :
	m_pCurGraphicsRootSignature( nullptr),
	m_pCurPipelineState( nullptr),
	m_pCurComputeRootSignature( nullptr),
    m_DynamicGPUDescriptorAllocator
    {
        {MemAllocator, GPUDescriptorHeaps[0], DynamicDescriptorAllocationChunkSize[0]},
        {MemAllocator, GPUDescriptorHeaps[1], DynamicDescriptorAllocationChunkSize[1]}
    },
    m_PendingResourceBarriers( STD_ALLOCATOR_RAW_MEM(Vk_RESOURCE_BARRIER, GetRawAllocator(), "Allocator for vector<Vk_RESOURCE_BARRIER>") ),
    m_PendingBarrierObjects( STD_ALLOCATOR_RAW_MEM(RefCntAutoPtr<IDeviceObject>, GetRawAllocator(), "Allocator for vector<RefCntAutoPtr<IDeviceObject>>") )
    */
{
#if 0
    m_PendingResourceBarriers.reserve(MaxPendingBarriers);
    m_PendingBarrierObjects.reserve(MaxPendingBarriers);

    CmdListManager.CreateNewCommandList(&m_pCommandList, &m_pCurrentAllocator);
#endif
}

CommandContext::~CommandContext( void )
{
}


#if 0
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


IVkGraphicsCommandList* CommandContext::Close(IVkCommandAllocator **ppAllocator)
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



void GraphicsContext::SetRenderTargets( UINT NumRTVs, ITextureViewVk** ppRTVs, ITextureViewVk* pDSV )
{
    Vk_CPU_DESCRIPTOR_HANDLE RTVHandles[8]; // Do not waste time initializing array to zero

	for (UINT i = 0; i < NumRTVs; ++i)
	{
        auto *pRTV = ppRTVs[i];
        if( pRTV )
        {
            auto *pTexture = ValidatedCast<TextureVkImpl>( pRTV->GetTexture() );
	        TransitionResource(pTexture, Vk_RESOURCE_STATE_RENDER_TARGET);
		    RTVHandles[i] = pRTV->GetCPUDescriptorHandle();
            VERIFY_EXPR(RTVHandles[i].ptr != 0);
        }
	}

	if (pDSV)
	{
        auto *pTexture = ValidatedCast<TextureVkImpl>( pDSV->GetTexture() );
		//if (bReadOnlyDepth)
		//{
		//	TransitionResource(*pTexture, Vk_RESOURCE_STATE_DEPTH_READ);
		//	m_pCommandList->OMSetRenderTargets( NumRTVs, RTVHandles, FALSE, &DSV->GetDSV_DepthReadOnly() );
		//}
		//else
		{
			TransitionResource(pTexture, Vk_RESOURCE_STATE_DEPTH_WRITE);
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

void CommandContext::ClearUAVFloat( ITextureViewVk *pTexView, const float* Color )
{
    auto *pTexture = ValidatedCast<TextureVkImpl>( pTexView->GetTexture() );
	TransitionResource(pTexture, Vk_RESOURCE_STATE_UNORDERED_ACCESS, true);

	// After binding a UAV, we can get a GPU handle that is required to clear it as a UAV (because it essentially runs
	// a shader to set all of the values).
    UNSUPPORTED("Not yet implemented");
    Vk_GPU_DESCRIPTOR_HANDLE GpuVisibleHandle = {};//m_DynamicDescriptorHeap.UploadDirect(Target.GetUAV());
	m_pCommandList->ClearUnorderedAccessViewFloat(GpuVisibleHandle, pTexView->GetCPUDescriptorHandle(), pTexture->GetVkResource(), Color, 0, nullptr);
}

void CommandContext::ClearUAVUint( ITextureViewVk *pTexView, const UINT *Color  )
{
    auto *pTexture = ValidatedCast<TextureVkImpl>( pTexView->GetTexture() );
	TransitionResource(pTexture, Vk_RESOURCE_STATE_UNORDERED_ACCESS, true);

	// After binding a UAV, we can get a GPU handle that is required to clear it as a UAV (because it essentially runs
	// a shader to set all of the values).
    UNSUPPORTED("Not yet implemented");
    Vk_GPU_DESCRIPTOR_HANDLE GpuVisibleHandle = {};//m_DynamicDescriptorHeap.UploadDirect(Target.GetUAV());
	//CD3DX12_RECT ClearRect(0, 0, (LONG)Target.GetWidth(), (LONG)Target.GetHeight());

	//TODO: My Nvidia card is not clearing UAVs with either Float or Uint variants.
	m_pCommandList->ClearUnorderedAccessViewUint(GpuVisibleHandle, pTexView->GetCPUDescriptorHandle(), pTexture->GetVkResource(), Color, 0, nullptr/*1, &ClearRect*/);
}


void GraphicsContext::ClearRenderTarget( ITextureViewVk *pRTV, const float *Color )
{
    auto *pTexture = ValidatedCast<TextureVkImpl>( pRTV->GetTexture() );
	TransitionResource(pTexture, Vk_RESOURCE_STATE_RENDER_TARGET, true);
	m_pCommandList->ClearRenderTargetView(pRTV->GetCPUDescriptorHandle(), Color, 0, nullptr);
}

void GraphicsContext::ClearDepthStencil( ITextureViewVk *pDSV, Vk_CLEAR_FLAGS ClearFlags, float Depth, UINT8 Stencil )
{
    auto *pTexture = ValidatedCast<TextureVkImpl>( pDSV->GetTexture() );
	TransitionResource( pTexture, Vk_RESOURCE_STATE_DEPTH_WRITE, true);
	m_pCommandList->ClearDepthStencilView(pDSV->GetCPUDescriptorHandle(), ClearFlags, Depth, Stencil, 0, nullptr);
}


void CommandContext::TransitionResource(ITextureVk *pTexture, Vk_RESOURCE_STATES NewState, bool FlushImmediate)
{
    VERIFY_EXPR( pTexture != nullptr );
    auto *pTexVk = ValidatedCast<TextureVkImpl>(pTexture);
    TransitionResource(*pTexVk, *pTexVk, NewState, FlushImmediate);
}

void CommandContext::TransitionResource(IBufferVk *pBuffer, Vk_RESOURCE_STATES NewState, bool FlushImmediate)
{
    VERIFY_EXPR( pBuffer != nullptr );
    auto *pBuffVk = ValidatedCast<BufferVkImpl>(pBuffer);

#ifdef _DEBUG
    // Dynamic buffers wtih no SRV/UAV bind flags are suballocated in 
    // the upload heap when Map() is called and must always be in 
    // Vk_RESOURCE_STATE_GENERIC_READ state
    if(pBuffVk->GetDesc().Usage == USAGE_DYNAMIC && (pBuffVk->GetDesc().BindFlags & (BIND_SHADER_RESOURCE|BIND_UNORDERED_ACCESS)) == 0)
    {
        VERIFY(pBuffVk->GetState() == Vk_RESOURCE_STATE_GENERIC_READ, "Dynamic buffers that cannot be bound as SRV or UAV are expected to always be in Vk_RESOURCE_STATE_GENERIC_READ state");
        VERIFY( (NewState & Vk_RESOURCE_STATE_GENERIC_READ) == NewState, "Dynamic buffers can only transition to one of Vk_RESOURCE_STATE_GENERIC_READ states");
    }
#endif

    TransitionResource(*pBuffVk, *pBuffVk, NewState, FlushImmediate);

#ifdef _DEBUG
    if(pBuffVk->GetDesc().Usage == USAGE_DYNAMIC && (pBuffVk->GetDesc().BindFlags & (BIND_SHADER_RESOURCE|BIND_UNORDERED_ACCESS)) == 0)
        VERIFY(pBuffVk->GetState() == Vk_RESOURCE_STATE_GENERIC_READ, "Dynamic buffers without SRV/UAV bind flag are expected to never transition from Vk_RESOURCE_STATE_GENERIC_READ state");
#endif
}

void CommandContext::TransitionResource(VkResourceBase& Resource, IDeviceObject &Object, Vk_RESOURCE_STATES NewState, bool FlushImmediate)
{
	Vk_RESOURCE_STATES OldState = Resource.GetState();

    // Check if required state is already set
	if ( (OldState & NewState) != NewState || NewState == 0 && OldState != 0 )
	{
        // If both old state and new state are read-only states, combine the two
        if( (OldState & Vk_RESOURCE_STATE_GENERIC_READ) == OldState &&
            (NewState & Vk_RESOURCE_STATE_GENERIC_READ) == NewState )
            NewState |= OldState;
        
        m_PendingResourceBarriers.emplace_back();
        m_PendingBarrierObjects.emplace_back(&Object);
		Vk_RESOURCE_BARRIER& BarrierDesc = m_PendingResourceBarriers.back();

		BarrierDesc.Type = Vk_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Transition.pResource = Resource.GetVkResource();
		BarrierDesc.Transition.Subresource = Vk_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = OldState;
		BarrierDesc.Transition.StateAfter = NewState;

		// Check to see if we already started the transition
#if 0
		if (NewState == Resource.m_TransitioningState)
		{
			BarrierDesc.Flags = Vk_RESOURCE_BARRIER_FLAG_END_ONLY;
			Resource.m_TransitioningState = (Vk_RESOURCE_STATES)-1;
		}
		else
#endif
			BarrierDesc.Flags = Vk_RESOURCE_BARRIER_FLAG_NONE;

		Resource.SetState( NewState );
	}
	else if (NewState == Vk_RESOURCE_STATE_UNORDERED_ACCESS)
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


void CommandContext::InsertUAVBarrier(VkResourceBase& Resource, IDeviceObject &Object, bool FlushImmediate)
{
    m_PendingResourceBarriers.emplace_back();
    m_PendingBarrierObjects.emplace_back(&Object);
	Vk_RESOURCE_BARRIER& BarrierDesc = m_PendingResourceBarriers.back();

	BarrierDesc.Type = Vk_RESOURCE_BARRIER_TYPE_UAV;
	BarrierDesc.Flags = Vk_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.UAV.pResource = Resource.GetVkResource();

	if (FlushImmediate)
        FlushResourceBarriers();
}

void CommandContext::InsertAliasBarrier(VkResourceBase& Before, VkResourceBase& After, IDeviceObject &BeforeObj, IDeviceObject &AfterObj, bool FlushImmediate)
{
    m_PendingResourceBarriers.emplace_back();
    m_PendingBarrierObjects.emplace_back(&BeforeObj);
    m_PendingBarrierObjects.emplace_back(&AfterObj);
	Vk_RESOURCE_BARRIER& BarrierDesc = m_PendingResourceBarriers.back();

	BarrierDesc.Type = Vk_RESOURCE_BARRIER_TYPE_ALIASING;
	BarrierDesc.Flags = Vk_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Aliasing.pResourceBefore = Before.GetVkResource();
	BarrierDesc.Aliasing.pResourceAfter = After.GetVkResource();

	if (FlushImmediate)
        FlushResourceBarriers();
}

void CommandContext::DiscardDynamicDescriptors(Uint64 FenceValue)
{
    for(size_t HeapType = 0; HeapType < _countof(m_DynamicGPUDescriptorAllocator); ++HeapType)
        m_DynamicGPUDescriptorAllocator[HeapType].DiscardAllocations(FenceValue);
}

DescriptorHeapAllocation CommandContext::AllocateDynamicGPUVisibleDescriptor( Vk_DESCRIPTOR_HEAP_TYPE Type, UINT Count )
{
    VERIFY(Type >= Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && Type <= Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, "Invalid heap type");
    return m_DynamicGPUDescriptorAllocator[Type].Allocate(Count);
}

#endif

}
#endif