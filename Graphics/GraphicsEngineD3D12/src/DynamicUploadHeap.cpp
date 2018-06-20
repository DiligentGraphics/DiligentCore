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
#include "DynamicUploadHeap.h"
#include "RenderDeviceD3D12Impl.h"

namespace Diligent
{
    GPURingBuffer::GPURingBuffer(size_t             MaxSize, 
                                 IMemoryAllocator&  Allocator,
                                 ID3D12Device*      pd3d12Device,
                                 bool               AllowCPUAccess) :
        RingBuffer(MaxSize, Allocator),
	    m_CpuVirtualAddress(nullptr),
	    m_GpuVirtualAddress(0)
    {
	    D3D12_HEAP_PROPERTIES HeapProps;
	    HeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	    HeapProps.CreationNodeMask     = 1;
	    HeapProps.VisibleNodeMask      = 1;

	    D3D12_RESOURCE_DESC ResourceDesc;
	    ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	    ResourceDesc.Alignment = 0;
	    ResourceDesc.Height = 1;
	    ResourceDesc.DepthOrArraySize = 1;
	    ResourceDesc.MipLevels = 1;
	    ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	    ResourceDesc.SampleDesc.Count = 1;
	    ResourceDesc.SampleDesc.Quality = 0;
	    ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	    D3D12_RESOURCE_STATES DefaultUsage;
	    if (AllowCPUAccess)
	    {
		    HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		    ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		    DefaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
	    }
	    else
	    {
		    HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		    ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		    DefaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	    }
        ResourceDesc.Width = MaxSize;

	    auto hr = pd3d12Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &ResourceDesc,
		    DefaultUsage, nullptr, __uuidof(m_pBuffer), reinterpret_cast<void**>(static_cast<ID3D12Resource**>(&m_pBuffer)) );
        if(FAILED(hr))
            LOG_ERROR("Failed to create new upload ring buffer");

	    m_pBuffer->SetName(L"Upload Ring Buffer");
        
        m_GpuVirtualAddress = m_pBuffer->GetGPUVirtualAddress();
        
        if (AllowCPUAccess)
        {
	        m_pBuffer->Map(0, nullptr, &m_CpuVirtualAddress);
        }

        LOG_INFO_MESSAGE("GPU ring buffer created. Size: ", MaxSize, "; GPU virtual address 0x", std::hex, m_GpuVirtualAddress);
    }

    void GPURingBuffer::Destroy()
    {
        if(m_pBuffer)
        {
            LOG_INFO_MESSAGE("Destroying GPU ring buffer. Size: ", m_pBuffer->GetDesc().Width, "; GPU virtual address 0x", std::hex, m_GpuVirtualAddress);
        }

	    if (m_CpuVirtualAddress)
        {
	        m_pBuffer->Unmap(0, nullptr);
        }
        m_CpuVirtualAddress = 0;
        m_GpuVirtualAddress = 0;
        m_pBuffer.Release();
    }

    GPURingBuffer::~GPURingBuffer()
    {
        Destroy();
    }

    DynamicUploadHeap::DynamicUploadHeap(IMemoryAllocator&      Allocator,
                                         bool                   bIsCPUAccessible,
                                         RenderDeviceD3D12Impl* pDevice,
                                         size_t                 InitialSize) :
        m_Allocator(Allocator),
        m_pDeviceD3D12(pDevice),
        m_bIsCPUAccessible(bIsCPUAccessible),
        m_RingBuffers(STD_ALLOCATOR_RAW_MEM(GPURingBuffer, GetRawAllocator(), "Allocator for vector<GPURingBuffer>"))
    {
        m_RingBuffers.emplace_back(InitialSize, Allocator, pDevice->GetD3D12Device(), m_bIsCPUAccessible);
    }

    DynamicAllocation DynamicUploadHeap::Allocate(size_t SizeInBytes, size_t Alignment /*= DEFAULT_ALIGN*/)
    {
        // Every device context has its own upload heap, so there is no need to lock

        //std::lock_guard<std::mutex> Lock(m_Mutex);

        //
        //      Deferred contexts must not update resources or map dynamic buffers
        //      across several frames!
        //

	    const size_t AlignmentMask = Alignment - 1;
	    // Assert that it's a power of two.
	    VERIFY_EXPR((AlignmentMask & Alignment) == 0);
	    // Align the allocation
	    const size_t AlignedSize = (SizeInBytes + AlignmentMask) & ~AlignmentMask;
        auto DynAlloc = m_RingBuffers.back().Allocate(AlignedSize);
        if (!DynAlloc.pBuffer)
        {
            auto NewMaxSize = m_RingBuffers.back().GetMaxSize() * 2;
            while(NewMaxSize < AlignedSize)NewMaxSize*=2;
            m_RingBuffers.emplace_back(NewMaxSize, m_Allocator, m_pDeviceD3D12->GetD3D12Device(), m_bIsCPUAccessible);
            DynAlloc = m_RingBuffers.back().Allocate(AlignedSize);
        }
#ifdef _DEBUG
		DynAlloc.FrameNum = m_pDeviceD3D12->GetCurrentFrameNumber();
#endif
        return DynAlloc;
    }

    void DynamicUploadHeap::FinishFrame(Uint64 FenceValue, Uint64 LastCompletedFenceValue)
    {
        // Every device context has its own upload heap, so there is no need to lock
        //std::lock_guard<std::mutex> Lock(m_Mutex);

        //
        //      Deferred contexts must not update resources or map dynamic buffers
        //      across several frames!
        //

        size_t NumBuffsToDelete = 0;
        for(size_t Ind = 0; Ind < m_RingBuffers.size(); ++Ind)
        {
            auto &RingBuff = m_RingBuffers[Ind];
            RingBuff.FinishCurrentFrame(FenceValue);
            RingBuff.ReleaseCompletedFrames(LastCompletedFenceValue);
            if ( NumBuffsToDelete == Ind && Ind < m_RingBuffers.size()-1 && RingBuff.IsEmpty())
            {
                ++NumBuffsToDelete;
            }
        }
        
        if(NumBuffsToDelete)
            m_RingBuffers.erase(m_RingBuffers.begin(), m_RingBuffers.begin()+NumBuffsToDelete);
    }
}
