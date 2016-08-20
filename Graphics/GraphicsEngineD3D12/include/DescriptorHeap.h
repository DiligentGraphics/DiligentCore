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

#include <mutex>
#include <vector>
#include <queue>
#include <string>
#include <set>
#include "ObjectBase.h"
#include "FreeBlockListManagerGPU.h"

namespace Diligent
{

class DescriptorHeapAllocation;
class DescriptorHeapAllocationManager;
class RenderDeviceD3D12Impl;

class IDescriptorAllocator
{
public:
    virtual DescriptorHeapAllocation Allocate( uint32_t Count ) = 0;
    virtual void Free(DescriptorHeapAllocation&& Allocation) = 0;
    virtual Uint32 GetDescriptorSize()const = 0;
};

class DescriptorHeapAllocation
{
public:
	DescriptorHeapAllocation() : 
        m_NumHandles(1), // One null descriptor handle
        m_pDescriptorHeap(nullptr),
        m_DescriptorSize(0)
	{
		m_FirstCpuHandle.ptr = 0;
		m_FirstGpuHandle.ptr = 0;
	}

	DescriptorHeapAllocation( IDescriptorAllocator *pAllocator, ID3D12DescriptorHeap *pHeap, D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, Uint32 NHandles ) : 
        m_FirstCpuHandle(CpuHandle),
        m_pAllocator(pAllocator),
        m_pDescriptorHeap(pHeap),
        m_NumHandles(NHandles)
	{
		m_FirstGpuHandle.ptr = 0;
        VERIFY_EXPR(m_pAllocator != nullptr && m_pDescriptorHeap != nullptr);
        auto DescriptorSize = m_pAllocator->GetDescriptorSize();
        VERIFY(DescriptorSize < std::numeric_limits<Uint16>::max(), "DescriptorSize exceeds allowed limit")
        m_DescriptorSize = static_cast<Uint16>( DescriptorSize );
	}

	DescriptorHeapAllocation( IDescriptorAllocator *pAllocator, ID3D12DescriptorHeap *pHeap, D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle, Uint32 NHandles, Uint16 AllocationManagerId = static_cast<Uint16>(-1) ) : 
        m_FirstCpuHandle(CpuHandle), 
        m_FirstGpuHandle(GpuHandle),
        m_pAllocator(pAllocator),
        m_NumHandles(NHandles),
        m_pDescriptorHeap(pHeap),
        m_AllocationManagerId(AllocationManagerId)
	{
        VERIFY_EXPR(m_pAllocator != nullptr && m_pDescriptorHeap != nullptr);
        auto DescriptorSize = m_pAllocator->GetDescriptorSize();
        VERIFY(DescriptorSize < std::numeric_limits<Uint16>::max(), "DescriptorSize exceeds allowed limit")
        m_DescriptorSize = static_cast<Uint16>( DescriptorSize );
	}

    DescriptorHeapAllocation(DescriptorHeapAllocation &&Allocation) : 
	    m_FirstCpuHandle(Allocation.m_FirstCpuHandle),
	    m_FirstGpuHandle(Allocation.m_FirstGpuHandle),
        m_NumHandles(Allocation.m_NumHandles),
        m_pAllocator(std::move(Allocation.m_pAllocator)),
        m_AllocationManagerId(std::move(Allocation.m_AllocationManagerId)),
        m_pDescriptorHeap(std::move(Allocation.m_pDescriptorHeap) ),
        m_DescriptorSize(std::move(Allocation.m_DescriptorSize) )
    {
        Allocation.m_pAllocator = nullptr;
        Allocation.m_FirstCpuHandle.ptr = 0;
        Allocation.m_FirstGpuHandle.ptr = 0;
        Allocation.m_NumHandles = 0;
        Allocation.m_pDescriptorHeap = nullptr;
        Allocation.m_DescriptorSize = 0;
        Allocation.m_AllocationManagerId = static_cast<Uint16>(-1);
    }

    DescriptorHeapAllocation& operator = (DescriptorHeapAllocation &&Allocation)
    { 
	    m_FirstCpuHandle = Allocation.m_FirstCpuHandle;
	    m_FirstGpuHandle = Allocation.m_FirstGpuHandle;
        m_NumHandles = Allocation.m_NumHandles;
        m_pAllocator = std::move(Allocation.m_pAllocator);
        m_AllocationManagerId = std::move(Allocation.m_AllocationManagerId);
        m_pDescriptorHeap = std::move(Allocation.m_pDescriptorHeap);
        m_DescriptorSize = std::move(Allocation.m_DescriptorSize);

        Allocation.m_FirstCpuHandle.ptr = 0;
        Allocation.m_FirstGpuHandle.ptr = 0;
        Allocation.m_NumHandles = 0;
        Allocation.m_pAllocator = nullptr;
        Allocation.m_pDescriptorHeap = nullptr;
        Allocation.m_DescriptorSize = 0;
        Allocation.m_AllocationManagerId = static_cast<Uint16>(-1);

        return *this;
    }

    ~DescriptorHeapAllocation()
    {
        if(!IsNull() && m_pAllocator)
            m_pAllocator->Free(std::move(*this));
    }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(Uint32 Offset = 0) const 
    { 
        VERIFY_EXPR(Offset >= 0 && Offset < m_NumHandles); 

        D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = m_FirstCpuHandle; 
        if (Offset != 0)
        {
            CPUHandle.ptr += m_DescriptorSize * Offset;
        }
        return CPUHandle;
    }

	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(Uint32 Offset = 0) const
    { 
        VERIFY_EXPR(Offset >= 0 && Offset < m_NumHandles); 
        D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = m_FirstGpuHandle;
        if (Offset != 0)
        {
            GPUHandle.ptr += m_DescriptorSize * Offset;
        }
        return GPUHandle;
    }

    ID3D12DescriptorHeap *GetDescriptorHeap(){return m_pDescriptorHeap;}

    size_t GetNumHandles(){return m_NumHandles;}

	bool IsNull() const { return m_FirstCpuHandle.ptr == 0; }
	bool IsShaderVisible() const { return m_FirstGpuHandle.ptr != 0; }
    size_t GetAllocationManagerId(){return m_AllocationManagerId;}
    UINT GetDescriptorSize()const{return m_DescriptorSize;}

private:
    DescriptorHeapAllocation(const DescriptorHeapAllocation&) = delete;
    DescriptorHeapAllocation& operator= (const DescriptorHeapAllocation&) = delete;

	D3D12_CPU_DESCRIPTOR_HANDLE m_FirstCpuHandle = {0};
	D3D12_GPU_DESCRIPTOR_HANDLE m_FirstGpuHandle = {0};
    // Keep strong reference to the parent heap to make sure it is alive while allocation is alive
    //RefCntAutoPtr<IDescriptorAllocator> m_pAllocator;
    IDescriptorAllocator* m_pAllocator = nullptr;
    ID3D12DescriptorHeap* m_pDescriptorHeap = nullptr;
    Uint32 m_NumHandles = 0;
    Uint16 m_AllocationManagerId = static_cast<Uint16>(-1);
    Uint16 m_DescriptorSize = 0;
};


class DescriptorHeapAllocationManager
{
public:
    DescriptorHeapAllocationManager(IMemoryAllocator &Allocator, 
                                    RenderDeviceD3D12Impl *pDeviceD3D12Impl,
                                    IDescriptorAllocator *pParentAllocator,
                                    size_t ThisManagerId,
                                    const D3D12_DESCRIPTOR_HEAP_DESC &HeapDesc);
    DescriptorHeapAllocationManager(IMemoryAllocator &Allocator, 
                                    RenderDeviceD3D12Impl *pDeviceD3D12Impl,
                                    IDescriptorAllocator *pParentAllocator,
                                    size_t ThisManagerId,
                                    ID3D12DescriptorHeap *pd3d12DescriptorHeap,
                                    Uint32 FirstDescriptor,
                                    Uint32 NumDescriptors);


    // = default causes compiler error when instantiating std::vector::emplace_back() in Visual Studio 2015 (Version 14.0.23107.0 D14REL)
    DescriptorHeapAllocationManager(DescriptorHeapAllocationManager&& rhs) : 
        m_FreeBlockManager(std::move(rhs.m_FreeBlockManager)),
        m_HeapDesc(rhs.m_HeapDesc),
	    m_pd3d12DescriptorHeap(std::move(rhs.m_pd3d12DescriptorHeap)),
	    m_FirstCPUHandle(rhs.m_FirstCPUHandle),
        m_FirstGPUHandle(rhs.m_FirstGPUHandle),
        m_DescriptorSize(rhs.m_DescriptorSize),
        m_NumDescriptorsInAllocation(rhs.m_NumDescriptorsInAllocation),
        // Mutex is not movable
        //m_AllocationMutex(std::move(rhs.m_AllocationMutex))
         m_pDeviceD3D12Impl(rhs.m_pDeviceD3D12Impl),
         m_pParentAllocator(rhs.m_pParentAllocator),
         m_ThisManagerId(rhs.m_ThisManagerId)
    {
	    rhs.m_FirstCPUHandle.ptr = 0;
        rhs.m_FirstGPUHandle.ptr = 0;
        rhs.m_DescriptorSize = 0;
        rhs.m_NumDescriptorsInAllocation = 0;
        rhs.m_HeapDesc.NumDescriptors = 0;
        rhs.m_pDeviceD3D12Impl = nullptr;
        rhs.m_pParentAllocator = nullptr;
        rhs.m_ThisManagerId = static_cast<size_t>(-1);
    }

    DescriptorHeapAllocationManager& operator = (DescriptorHeapAllocationManager&& rhs) = delete;
    DescriptorHeapAllocationManager(const DescriptorHeapAllocationManager&) = delete;
    DescriptorHeapAllocationManager& operator = (const DescriptorHeapAllocationManager&) = delete;

    ~DescriptorHeapAllocationManager();

    DescriptorHeapAllocation Allocate( uint32_t Count );
    void Free(DescriptorHeapAllocation&& Allocation);
    void ReleaseStaleAllocations(Uint64 NumCompletedFrames);
    size_t GetNumAvailableDescriptors()const{return m_FreeBlockManager.GetFreeSize();}

private:
    FreeBlockListManagerGPU m_FreeBlockManager;
    D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
	CComPtr<ID3D12DescriptorHeap> m_pd3d12DescriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_FirstCPUHandle = {0};
    D3D12_GPU_DESCRIPTOR_HANDLE m_FirstGPUHandle = {0};
    UINT m_DescriptorSize = 0;
    Uint32 m_NumDescriptorsInAllocation = 0;
    std::mutex m_AllocationMutex;
    RenderDeviceD3D12Impl *m_pDeviceD3D12Impl = nullptr;
    IDescriptorAllocator *m_pParentAllocator = nullptr;
    size_t m_ThisManagerId = static_cast<size_t>(-1);
};

// This is an unbounded resource descriptor heap.  It is intended to provide space for CPU-visible resource descriptors
// as resources are created as well as for static and mutable shader descriptor tables.
class CPUDescriptorHeap : public IDescriptorAllocator
{
public:
	CPUDescriptorHeap(IMemoryAllocator &Allocator, 
                      RenderDeviceD3D12Impl *pDeviceD3D12Impl, 
                      Uint32 NumDescriptorsInHeap, 
                      D3D12_DESCRIPTOR_HEAP_TYPE Type, 
                      D3D12_DESCRIPTOR_HEAP_FLAGS Flags);

    CPUDescriptorHeap(const CPUDescriptorHeap&) = delete;
    CPUDescriptorHeap(CPUDescriptorHeap&&) = delete;
    CPUDescriptorHeap& operator = (const CPUDescriptorHeap&) = delete;
    CPUDescriptorHeap& operator = (CPUDescriptorHeap&&) = delete;

    ~CPUDescriptorHeap();

	virtual DescriptorHeapAllocation Allocate( uint32_t Count )override;
    virtual void Free(DescriptorHeapAllocation&& Allocation)override;
    virtual Uint32 GetDescriptorSize()const override{return m_DescriptorSize;}

    void ReleaseStaleAllocations(Uint64 NumCompletedFrames);

protected:

    std::vector<DescriptorHeapAllocationManager, STDAllocatorRawMem<DescriptorHeapAllocationManager> > m_HeapPool;
    std::set<size_t, std::less<size_t>, STDAllocatorRawMem<size_t> > m_AvailableHeaps;
    IMemoryAllocator &m_MemAllocator;
 
	std::mutex m_AllocationMutex;

    D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
	UINT m_DescriptorSize;
    RenderDeviceD3D12Impl *m_pDeviceD3D12Impl;
    Uint32 m_MaxHeapSize = 0;
    Uint32 m_CurrentSize = 0; // This size does not count stale allocation
};


class GPUDescriptorHeap : public IDescriptorAllocator
{
public:
	GPUDescriptorHeap(IMemoryAllocator &Allocator, 
                      RenderDeviceD3D12Impl *pDevice, 
                      Uint32 NumDescriptorsInHeap, 
                      Uint32 NumDynamicDescriptors,
                      D3D12_DESCRIPTOR_HEAP_TYPE Type, 
                      D3D12_DESCRIPTOR_HEAP_FLAGS Flags);

    GPUDescriptorHeap(const GPUDescriptorHeap&) = delete;
    GPUDescriptorHeap(GPUDescriptorHeap&&) = delete;
    GPUDescriptorHeap& operator = (const GPUDescriptorHeap&) = delete;
    GPUDescriptorHeap& operator = (GPUDescriptorHeap&&) = delete;

    ~GPUDescriptorHeap();

	virtual DescriptorHeapAllocation Allocate( uint32_t Count )override;
    virtual void Free(DescriptorHeapAllocation&& Allocation)override;
    virtual Uint32 GetDescriptorSize()const override{return m_DescriptorSize;}

    DescriptorHeapAllocation AllocateDynamic( uint32_t Count );

    void ReleaseStaleAllocations(Uint64 NumCompletedFrames);

    D3D12_DESCRIPTOR_HEAP_DESC &GetHeapDesc(){return m_HeapDesc;}

protected:

    D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
    CComPtr<ID3D12DescriptorHeap> m_pd3d12DescriptorHeap;


    UINT m_DescriptorSize = 0;

	std::mutex m_Mutex;
    DescriptorHeapAllocationManager m_HeapAllocationManager;
    DescriptorHeapAllocationManager m_DynamicAllocationsManager;
        
    RenderDeviceD3D12Impl *m_pDeviceD3D12;
    Uint32 m_CurrentSize = 0;
    Uint32 m_MaxHeapSize = 0;
    Uint32 m_CurrentDynamicSize = 0;
    Uint32 m_MaxDynamicSize = 0;
};



class DynamicSuballocationsManager : public IDescriptorAllocator
{
public:
    DynamicSuballocationsManager(IMemoryAllocator &Allocator, GPUDescriptorHeap& ParentGPUHeap, Uint32 DynamicChunkSize);

    DynamicSuballocationsManager(const DynamicSuballocationsManager&) = delete;
    DynamicSuballocationsManager(DynamicSuballocationsManager&&) = delete;
    DynamicSuballocationsManager& operator = (const DynamicSuballocationsManager&) = delete;
    DynamicSuballocationsManager& operator = (DynamicSuballocationsManager&&) = delete;

    void DiscardAllocations(Uint64 FrameNumber);

	virtual DescriptorHeapAllocation Allocate( Uint32 Count )override;
    virtual void Free(DescriptorHeapAllocation&& Allocation)override;

    virtual Uint32 GetDescriptorSize()const override{return m_ParentGPUHeap.GetDescriptorSize();}

private:
    std::vector<DescriptorHeapAllocation, STDAllocatorRawMem<DescriptorHeapAllocation> > m_Suballocations;

	Uint32 m_CurrentSuballocationOffset = 0;
    Uint32 m_DynamicChunkSize = 0;

    GPUDescriptorHeap &m_ParentGPUHeap;
};

}
