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

#include "RingBuffer.h"

namespace Diligent
{

// Constant blocks must be multiples of 16 constants @ 16 bytes each
#define DEFAULT_ALIGN 256

struct DynamicAllocation
{
	DynamicAllocation(ID3D12Resource *pBuff = nullptr, size_t ThisOffset = 0, size_t ThisSize = 0)
		: pBuffer(pBuff), Offset(ThisOffset), Size(ThisSize) {}

	//CComPtr<ID3D12Resource> pBuffer;	    // The D3D buffer associated with this memory.
    ID3D12Resource *pBuffer = nullptr;	    // The D3D buffer associated with this memory.
	size_t Offset = 0;			                // Offset from start of buffer resource
	size_t Size = 0;			                // Reserved size of this allocation
	void* CPUAddress = 0;			            // The CPU-writeable address
	D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;	// The GPU-visible address
#ifdef _DEBUG
    Uint64 FrameNum = static_cast<Uint64>(-1);
#endif
};

class GPURingBuffer : public RingBuffer
{
public:
    GPURingBuffer(size_t MaxSize, IMemoryAllocator &Allocator, ID3D12Device *pd3d12Device, bool AllowCPUAccess);
    
    GPURingBuffer(GPURingBuffer&& rhs) : 
        RingBuffer(std::move(rhs)),
        m_CpuVirtualAddress(rhs.m_CpuVirtualAddress),
        m_GpuVirtualAddress(rhs.m_GpuVirtualAddress),
        m_pBuffer(std::move(rhs.m_pBuffer))
    {
        rhs.m_CpuVirtualAddress = nullptr;
        rhs.m_GpuVirtualAddress = 0;
        rhs.m_pBuffer.Release();
    }

    GPURingBuffer& operator =(GPURingBuffer&& rhs)
    {
        Destroy();

        static_cast<RingBuffer&>(*this) = std::move(rhs);
        m_CpuVirtualAddress = rhs.m_CpuVirtualAddress;
        m_GpuVirtualAddress = rhs.m_GpuVirtualAddress;
        m_pBuffer = std::move(rhs.m_pBuffer);
        rhs.m_CpuVirtualAddress = 0;
        rhs.m_GpuVirtualAddress = 0;
        
        return *this;
    }

    ~GPURingBuffer();

    DynamicAllocation Allocate(size_t SizeInBytes)
    {
        auto Offset = RingBuffer::Allocate(SizeInBytes);
        if (Offset != RingBuffer::InvalidOffset)
        {
            DynamicAllocation DynAlloc(m_pBuffer, Offset, SizeInBytes);
            DynAlloc.GPUAddress = m_GpuVirtualAddress + Offset;
            DynAlloc.CPUAddress = m_CpuVirtualAddress;
            if(DynAlloc.CPUAddress)
                DynAlloc.CPUAddress = reinterpret_cast<char*>(DynAlloc.CPUAddress) + Offset;
            return DynAlloc;
        }
        else
        {
            return DynamicAllocation(nullptr, 0, 0);
        }
    }

    GPURingBuffer(const GPURingBuffer&) = delete;
    GPURingBuffer& operator =(GPURingBuffer&) = delete;

private:
    void Destroy();

	void* m_CpuVirtualAddress;
	D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
    CComPtr<ID3D12Resource> m_pBuffer;
};

class DynamicUploadHeap
{
public:

	DynamicUploadHeap(IMemoryAllocator &Allocator, bool bIsCPUAccessible, class RenderDeviceD3D12Impl* pDevice, size_t InitialSize);
    
    DynamicUploadHeap(const DynamicUploadHeap&)=delete;
    DynamicUploadHeap(DynamicUploadHeap&&)=delete;
    DynamicUploadHeap& operator=(const DynamicUploadHeap&)=delete;
    DynamicUploadHeap& operator=(DynamicUploadHeap&&)=delete;

	DynamicAllocation Allocate( size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN );

    void FinishFrame(Uint64 FenceValue, Uint64 LastCompletedFenceValue);

private:
    const bool m_bIsCPUAccessible;
    // When a chunk of dynamic memory is requested, the heap first tries to allocate the memory in the largest GPU buffer. 
    // If allocation fails, a new ring buffer is created that provides enough space and requests memory from that buffer.
    // Only the largest buffer is used for allocation and all other buffers are released when GPU is done with corresponding frames
    std::vector<GPURingBuffer, STDAllocatorRawMem<GPURingBuffer> > m_RingBuffers;
    IMemoryAllocator &m_Allocator;
    RenderDeviceD3D12Impl* m_pDeviceD3D12 = nullptr;
    //std::mutex m_Mutex;
};

}
