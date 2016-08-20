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

//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//
// Adapted to Diligent Engine: Egor Yusov
//

#pragma once

#include "pch.h"
#include <vector>

#include "D3D12ResourceBase.h"
#include "TextureViewD3D12.h"
#include "TextureD3D12.h"
#include "BufferD3D12.h"
#include "DescriptorHeap.h"

namespace Diligent
{


struct DWParam
{
	DWParam( FLOAT f ) : Float(f) {}
	DWParam( UINT u ) : Uint(u) {}
	DWParam( INT i ) : Int(i) {}

	void operator= ( FLOAT f ) { Float = f; }
	void operator= ( UINT u ) { Uint = u; }
	void operator= ( INT i ) { Int = i; }

	union
	{
		FLOAT Float;
		UINT Uint;
		INT Int;
	};
};


class CommandContext
{
public:

	CommandContext( IMemoryAllocator &MemAllocator,
                    class CommandListManager& CmdListManager, 
                    GPUDescriptorHeap GPUDescriptorHeaps[],
                    Uint32 DynamicDescriptorAllocationChunkSize[]);

    ~CommandContext(void);

	// Submit the command buffer and reset it.  This is encouraged to keep the GPU busy and reduce latency.
	// Taking too long to build command lists and submit them can idle the GPU.
	// Returns a fence value to verify completion.  (Use it with the CommandListManager.)

	ID3D12GraphicsCommandList* Close(ID3D12CommandAllocator **ppAllocator);
	void Reset( CommandListManager& CmdListManager );

	class GraphicsContext& AsGraphicsContext();
    class ComputeContext& AsComputeContext();

	void ClearUAVFloat( ITextureViewD3D12 *pTexView, const float* Color );
	void ClearUAVUint( ITextureViewD3D12 *pTexView, const UINT *Color  );

    void CopyResource(ID3D12Resource *pDstRes, ID3D12Resource *pSrcRes)
    {
        m_pCommandList->CopyResource(pDstRes, pSrcRes);
    }

	//void CopyBuffer( GpuResource& Dest, GpuResource& Src );
	//void CopyBufferRegion( GpuResource& Dest, size_t DestOffset, GpuResource& Src, size_t SrcOffset, size_t NumBytes );
	//void CopySubresource(GpuResource& Dest, UINT DestSubIndex, GpuResource& Src, UINT SrcSubIndex);
	//void CopyCounter(GpuResource& Dest, size_t DestOffset, StructuredBuffer& Src);
	//void ResetCounter(StructuredBuffer& Buf, uint32_t Value = 0);

	//static void InitializeTextureArraySlice(GpuResource& Dest, UINT SliceIndex, GpuResource& Src);

    void TransitionResource(ITextureD3D12 *pTexture, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
    void TransitionResource(IBufferD3D12 *pBuffer, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
	//void BeginResourceTransition(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
	void FlushResourceBarriers();

	//void InsertTimeStamp( ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx );
	//void ResolveTimeStamps( ID3D12Resource* pReadbackHeap, ID3D12QueryHeap* pQueryHeap, uint32_t NumQueries );
	//void PIXBeginEvent(const wchar_t* label);
	//void PIXEndEvent(void);
	//void PIXSetMarker(const wchar_t* label);

    struct ShaderDescriptorHeaps
    {
        ID3D12DescriptorHeap* pSrvCbvUavHeap;
        ID3D12DescriptorHeap* pSamplerHeap;
        ShaderDescriptorHeaps(ID3D12DescriptorHeap* _pSrvCbvUavHeap = nullptr, ID3D12DescriptorHeap* _pSamplerHeap = nullptr) :
            pSrvCbvUavHeap(_pSrvCbvUavHeap),
            pSamplerHeap(_pSamplerHeap)
        {}
        bool operator == (const ShaderDescriptorHeaps& rhs)const
        {
            return pSrvCbvUavHeap == rhs.pSrvCbvUavHeap && pSamplerHeap == rhs.pSamplerHeap;
        }
        operator bool()const
        {
            return pSrvCbvUavHeap != nullptr || pSamplerHeap != nullptr;
        }
    };
	void SetDescriptorHeaps( ShaderDescriptorHeaps& Heaps );

	void ExecuteIndirect(ID3D12CommandSignature *pCmdSignature, ID3D12Resource *pBuff, Uint64 ArgsOffset)
    {
	    FlushResourceBarriers();
	    m_pCommandList->ExecuteIndirect(pCmdSignature, 1, pBuff, ArgsOffset, nullptr, 0);
    }

    void SetID(const Char* ID) { m_ID = ID; }
    ID3D12GraphicsCommandList *GetCommandList(){return m_pCommandList;}
    
    void DiscardUsedDescriptorHeaps(Uint64 FrameNumber);
    DescriptorHeapAllocation AllocateDynamicGPUVisibleDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 );

    void InsertUAVBarrier(D3D12ResourceBase& Resource, IDeviceObject &Object, bool FlushImmediate = false);

protected:
    void TransitionResource(D3D12ResourceBase& Resource, IDeviceObject &Object, D3D12_RESOURCE_STATES NewState, bool FlushImmediate);
    void InsertAliasBarrier(D3D12ResourceBase& Before, D3D12ResourceBase& After, IDeviceObject &BeforeObj, IDeviceObject &AfterObj, bool FlushImmediate = false);

	//void FinishTimeStampQueryBatch();
	//void BindDescriptorHeaps( void );

	CComPtr<ID3D12GraphicsCommandList> m_pCommandList;
	CComPtr<ID3D12CommandAllocator> m_pCurrentAllocator;

	ID3D12RootSignature* m_pCurGraphicsRootSignature;
	ID3D12PipelineState* m_pCurGraphicsPipelineState;
	ID3D12RootSignature* m_pCurComputeRootSignature;
	ID3D12PipelineState* m_pCurComputePipelineState;

    static const int MaxPendingBarriers = 16;
	std::vector<D3D12_RESOURCE_BARRIER, STDAllocatorRawMem<D3D12_RESOURCE_BARRIER> > m_PendingResourceBarriers;
    // We must make sure that all referenced objects are alive until barriers are executed
    // Keeping reference to ID3D12Resource is not sufficient!
    // TextureD3D12Impl::~TextureD3D12Impl() and BufferD3D12Impl::~BufferD3D12Impl()
    // are responsible for putting the D3D12 resource in the release queue
    std::vector< RefCntAutoPtr<IDeviceObject>, STDAllocatorRawMem<RefCntAutoPtr<IDeviceObject>> >  m_PendingBarrierObjects;

    ShaderDescriptorHeaps m_BoundDescriptorHeaps;
    
    // Every context must use its own allocator that maintains individual list of retired descriptor heaps to 
    // avoid interference with other command contexts
    // The heaps can only be discarded after the command list is submitted for execution
    DynamicSuballocationsManager m_DynamicGPUDescriptorAllocator[2];

	String m_ID;

    D3D12_PRIMITIVE_TOPOLOGY m_PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
};


class GraphicsContext : public CommandContext
{
public:
	void ClearRenderTarget( ITextureViewD3D12 *pRTV, const float *Color );
	void ClearDepthStencil( ITextureViewD3D12 *pDSV, D3D12_CLEAR_FLAGS ClearFlags, float Depth, UINT8 Stencil );

	void SetRootSignature( ID3D12RootSignature *pRootSig )
    {
	    if (pRootSig != m_pCurGraphicsRootSignature)
        {
    	    m_pCommandList->SetGraphicsRootSignature(m_pCurGraphicsRootSignature = pRootSig);
        }
    }

	void SetRenderTargets( UINT NumRTVs, ITextureViewD3D12** ppRTVs, ITextureViewD3D12* pDSV );

	void SetViewports( UINT NumVPs, const D3D12_VIEWPORT* pVPs )
    {
        m_pCommandList->RSSetViewports(NumVPs, pVPs);
    }

	void SetScissorRects( UINT NumRects, const D3D12_RECT* pRects )
    {
        m_pCommandList->RSSetScissorRects(NumRects, pRects);
    }

	void SetStencilRef( UINT StencilRef )
    {
	    m_pCommandList->OMSetStencilRef( StencilRef );
    }

	void SetBlendFactor( const float* BlendFactor )
    {
	    m_pCommandList->OMSetBlendFactor( BlendFactor );
    }

	void SetPrimitiveTopology( D3D12_PRIMITIVE_TOPOLOGY Topology )
    {
        if(m_PrimitiveTopology != Topology)
        {
            m_PrimitiveTopology = Topology;
	        m_pCommandList->IASetPrimitiveTopology(Topology);
        }
    }

	void SetPipelineState( ID3D12PipelineState* pPSO )
    {
        if (pPSO != m_pCurGraphicsPipelineState)
        {
	        m_pCommandList->SetPipelineState(m_pCurGraphicsPipelineState = pPSO);
        }
    }

	void SetConstants( UINT RootIndex, UINT NumConstants, const void* pConstants )
    {
	    m_pCommandList->SetGraphicsRoot32BitConstants( RootIndex, NumConstants, pConstants, 0 );
    }

	void SetConstants( UINT RootIndex, DWParam X )
    {
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, X.Uint, 0 );
    }

	void SetConstants( UINT RootIndex, DWParam X, DWParam Y )
    {
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, X.Uint, 0 );
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, Y.Uint, 1 );
    }

	void SetConstants( UINT RootIndex, DWParam X, DWParam Y, DWParam Z )
    {
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, X.Uint, 0 );
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, Y.Uint, 1 );
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, Z.Uint, 2 );
    }

	void SetConstants( UINT RootIndex, DWParam X, DWParam Y, DWParam Z, DWParam W )
    {
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, X.Uint, 0 );
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, Y.Uint, 1 );
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, Z.Uint, 2 );
	    m_pCommandList->SetGraphicsRoot32BitConstant( RootIndex, W.Uint, 3 );
    }

	void SetConstantBuffer( UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV )
    {
	    m_pCommandList->SetGraphicsRootConstantBufferView(RootIndex, CBV);
    }

	//void SetDynamicConstantBufferView( UINT RootIndex, size_t BufferSize, const void* BufferData );
	//void SetBufferSRV( UINT RootIndex, const GpuBuffer& SRV );
	//void SetBufferUAV( UINT RootIndex, const GpuBuffer& UAV );
	void SetDescriptorTable( UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle )
    {
	    m_pCommandList->SetGraphicsRootDescriptorTable( RootIndex, FirstHandle );
    }


	//void SetDynamicDescriptor( UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle );
	//void SetDynamicDescriptors( UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[] );

	void SetIndexBuffer( const D3D12_INDEX_BUFFER_VIEW& IBView )
    {
	    m_pCommandList->IASetIndexBuffer(&IBView);
    }

	void SetVertexBuffers( UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[] )
    {
	    m_pCommandList->IASetVertexBuffers(StartSlot, Count, VBViews);
    }

	//void SetDynamicVB( UINT Slot, size_t NumVertices, size_t VertexStride, const void* VBData );
	//void SetDynamicIB( size_t IndexCount, const uint16_t* IBData );
	//void SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData);

	void Draw(UINT VertexCountPerInstance, UINT InstanceCount,
	          UINT StartVertexLocation, UINT StartInstanceLocation)
    {
	    FlushResourceBarriers();
	    m_pCommandList->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
    }

	void DrawIndexed(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
		             INT BaseVertexLocation, UINT StartInstanceLocation)
    {
	    FlushResourceBarriers();
	    m_pCommandList->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
    }
};

class ComputeContext : public CommandContext
{
public:

	void SetRootSignature( ID3D12RootSignature *pRootSig )
    {
	    if (pRootSig != m_pCurComputeRootSignature)
        {
	        m_pCommandList->SetComputeRootSignature(m_pCurComputeRootSignature = pRootSig);
        }
    }


	void SetPipelineState( ID3D12PipelineState* pPSO )
    {
	    if (pPSO != m_pCurComputePipelineState)
        {   	    
            m_pCommandList->SetPipelineState(m_pCurComputePipelineState = pPSO);
        }
    }

	void SetConstants( UINT RootIndex, UINT NumConstants, const void* pConstants )
    {
	    m_pCommandList->SetComputeRoot32BitConstants( RootIndex, NumConstants, pConstants, 0 );
    }

	void SetConstants( UINT RootIndex, DWParam X )
    {
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, X.Uint, 0 );
    }

	void SetConstants( UINT RootIndex, DWParam X, DWParam Y )
    {
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, X.Uint, 0 );
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, Y.Uint, 1 );
    }

	void SetConstants( UINT RootIndex, DWParam X, DWParam Y, DWParam Z )
    {
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, X.Uint, 0 );
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, Y.Uint, 1 );
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, Z.Uint, 2 );
    }

	void SetConstants( UINT RootIndex, DWParam X, DWParam Y, DWParam Z, DWParam W )
    {
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, X.Uint, 0 );
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, Y.Uint, 1 );
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, Z.Uint, 2 );
	    m_pCommandList->SetComputeRoot32BitConstant( RootIndex, W.Uint, 3 );
    }
    

	void SetConstantBuffer( UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV )
    {
	    m_pCommandList->SetComputeRootConstantBufferView(RootIndex, CBV);
    }

	//void SetDynamicConstantBufferView( UINT RootIndex, size_t BufferSize, const void* BufferData );
	//void SetDynamicSRV( UINT RootIndex, size_t BufferSize, const void* BufferData ); 
	//void SetBufferSRV( UINT RootIndex, const GpuBuffer& SRV );
	//void SetBufferUAV( UINT RootIndex, const GpuBuffer& UAV );
	void SetDescriptorTable( UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle )
    {
	    m_pCommandList->SetComputeRootDescriptorTable( RootIndex, FirstHandle );
    }


	//void SetDynamicDescriptor( UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle );
	//void SetDynamicDescriptors( UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[] );

	void Dispatch( size_t GroupCountX = 1, size_t GroupCountY = 1, size_t GroupCountZ = 1 )
    {
	    FlushResourceBarriers();
	    m_pCommandList->Dispatch((UINT)GroupCountX, (UINT)GroupCountY, (UINT)GroupCountZ);
    }
};

inline GraphicsContext& CommandContext::AsGraphicsContext() 
{
    return static_cast<GraphicsContext&>(*this);
}

inline ComputeContext& CommandContext::AsComputeContext() 
{
    return static_cast<ComputeContext&>(*this);
}

#if 0

inline void GraphicsContext::SetDynamicConstantBufferView( UINT RootIndex, size_t BufferSize, const void* BufferData )
{
	ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
	DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
	//SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
	memcpy(cb.DataPtr, BufferData, BufferSize);
	m_pCommandList->SetGraphicsRootConstantBufferView(RootIndex, cb.GpuAddress);
}

inline void ComputeContext::SetDynamicConstantBufferView( UINT RootIndex, size_t BufferSize, const void* BufferData )
{
	ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
	DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
	//SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
	memcpy(cb.DataPtr, BufferData, BufferSize);
	m_pCommandList->SetComputeRootConstantBufferView(RootIndex, cb.GpuAddress);
}

inline void GraphicsContext::SetDynamicVB( UINT Slot, size_t NumVertices, size_t VertexStride, const void* VertexData )
{
	ASSERT(VertexData != nullptr && Math::IsAligned(VertexData, 16));

	size_t BufferSize = Math::AlignUp(NumVertices * VertexStride, 16);
	DynAlloc vb = m_CpuLinearAllocator.Allocate(BufferSize);

	SIMDMemCopy(vb.DataPtr, VertexData, BufferSize >> 4);

	D3D12_VERTEX_BUFFER_VIEW VBView;
	VBView.BufferLocation = vb.GpuAddress;
	VBView.SizeInBytes = (UINT)BufferSize;
	VBView.StrideInBytes = (UINT)VertexStride;

	m_pCommandList->IASetVertexBuffers(Slot, 1, &VBView);
}

inline void GraphicsContext::SetDynamicIB( size_t IndexCount, const uint16_t* IndexData )
{
	ASSERT(IndexData != nullptr && Math::IsAligned(IndexData, 16));

	size_t BufferSize = Math::AlignUp(IndexCount * sizeof(uint16_t), 16);
	DynAlloc ib = m_CpuLinearAllocator.Allocate(BufferSize);

	SIMDMemCopy(ib.DataPtr, IndexData, BufferSize >> 4);

	D3D12_INDEX_BUFFER_VIEW IBView;
	IBView.BufferLocation = ib.GpuAddress;
	IBView.SizeInBytes = (UINT)(IndexCount * sizeof(uint16_t));
	IBView.Format = DXGI_FORMAT_R16_UINT;

	m_pCommandList->IASetIndexBuffer(&IBView);
}

inline void GraphicsContext::SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
	ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
	DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
	SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
	m_pCommandList->SetGraphicsRootShaderResourceView(RootIndex, cb.GpuAddress);
}

inline void ComputeContext::SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
	ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
	DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
	SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
	m_pCommandList->SetComputeRootShaderResourceView(RootIndex, cb.GpuAddress);
}

inline void GraphicsContext::SetBufferSRV( UINT RootIndex, const GpuBuffer& SRV )
{
	ASSERT((SRV.m_UsageState & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) != 0);
	m_pCommandList->SetGraphicsRootShaderResourceView(RootIndex, SRV.GetGpuVirtualAddress());
}

inline void ComputeContext::SetBufferSRV( UINT RootIndex, const GpuBuffer& SRV )
{
	ASSERT((SRV.m_UsageState & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) != 0);
	m_pCommandList->SetComputeRootShaderResourceView(RootIndex, SRV.GetGpuVirtualAddress());
}

inline void GraphicsContext::SetBufferUAV( UINT RootIndex, const GpuBuffer& UAV )
{
	ASSERT((UAV.m_UsageState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0);
	m_pCommandList->SetGraphicsRootUnorderedAccessView(RootIndex, UAV.GetGpuVirtualAddress());
}

inline void ComputeContext::SetBufferUAV( UINT RootIndex, const GpuBuffer& UAV )
{
	ASSERT((UAV.m_UsageState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0);
	m_pCommandList->SetComputeRootUnorderedAccessView(RootIndex, UAV.GetGpuVirtualAddress());
}



inline void CommandContext::SetDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr )
{
	if (m_CurrentDescriptorHeaps[Type] != HeapPtr)
	{
		m_CurrentDescriptorHeaps[Type] = HeapPtr;
		BindDescriptorHeaps();
	}
}
#endif

inline void CommandContext::SetDescriptorHeaps( ShaderDescriptorHeaps& Heaps )
{
#ifdef _DEBUG
    VERIFY(Heaps.pSrvCbvUavHeap != nullptr || Heaps.pSamplerHeap != nullptr, "At least one heap is expected to be set");
    VERIFY(Heaps.pSrvCbvUavHeap == nullptr || Heaps.pSrvCbvUavHeap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "Invalid heap type provided in pSrvCbvUavHeap");
    VERIFY(Heaps.pSamplerHeap == nullptr || Heaps.pSamplerHeap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "Invalid heap type provided in pSamplerHeap");
#endif

    if (!(Heaps == m_BoundDescriptorHeaps))
    {
        m_BoundDescriptorHeaps = Heaps;

        ID3D12DescriptorHeap **ppHeaps = reinterpret_cast<ID3D12DescriptorHeap**>(&Heaps);
        UINT NumHeaps = (ppHeaps[0] != nullptr ? 1 : 0) + (ppHeaps[1] != nullptr ? 1 : 0);
        if(ppHeaps[0] == nullptr)
            ++ppHeaps;

        m_pCommandList->SetDescriptorHeaps(NumHeaps, ppHeaps);
    }
}

#if 0
inline void GraphicsContext::SetDynamicDescriptor( UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle )
{
	SetDynamicDescriptors(RootIndex, Offset, 1, &Handle);
}

inline void ComputeContext::SetDynamicDescriptor( UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle )
{
	SetDynamicDescriptors(RootIndex, Offset, 1, &Handle);
}

inline void GraphicsContext::SetDynamicDescriptors( UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[] )
{
	m_DynamicDescriptorHeap.SetGraphicsDescriptorHandles(RootIndex, Offset, Count, Handles);
}

inline void ComputeContext::SetDynamicDescriptors( UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[] )
{
	m_DynamicDescriptorHeap.SetComputeDescriptorHandles(RootIndex, Offset, Count, Handles);
}


inline void CommandContext::CopyBuffer( GpuResource& Dest, GpuResource& Src )
{
	TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
	FlushResourceBarriers();
	m_pCommandList->CopyResource(Dest.GetResource(), Src.GetResource());
}


inline void CommandContext::CopyCounter(GpuResource& Dest, size_t DestOffset, StructuredBuffer& Src)
{
	TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionResource(Src.GetCounterBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	FlushResourceBarriers();
	m_pCommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, Src.GetCounterBuffer().GetResource(), 0, 4);
}

inline void CommandContext::ResetCounter(StructuredBuffer& Buf, uint32_t Value )
{
	FillBuffer(Buf.GetCounterBuffer(), 0, Value, sizeof(uint32_t));
	TransitionResource(Buf.GetCounterBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

inline void CommandContext::InsertTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx)
{
	m_pCommandList->EndQuery(pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, QueryIdx);
}

inline void CommandContext::ResolveTimeStamps(ID3D12Resource* pReadbackHeap, ID3D12QueryHeap* pQueryHeap, uint32_t NumQueries)
{
	m_pCommandList->ResolveQueryData(pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, NumQueries, pReadbackHeap, 0);
}

inline void CommandContext::PIXBeginEvent(const wchar_t* label)
{
#if defined(RELEASE) || _MSC_VER < 1800
	(label);
#else
	::PIXBeginEvent(m_pCommandList, 0, label);
#endif
}

inline void CommandContext::PIXEndEvent(void)
{
#if !defined(RELEASE) && _MSC_VER >= 1800
	::PIXEndEvent(m_pCommandList);
#endif
}

inline void CommandContext::PIXSetMarker(const wchar_t* label)
{
#if defined(RELEASE) || _MSC_VER < 1800
	(label);
#else
	::PIXSetMarker(m_pCommandList, 0, label);
#endif
}
#endif
}