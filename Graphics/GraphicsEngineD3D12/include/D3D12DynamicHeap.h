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

#include <mutex>
#include <map>
#include <deque>

namespace Diligent
{

struct D3D12DynamicAllocation
{
    D3D12DynamicAllocation()noexcept{}
    D3D12DynamicAllocation(ID3D12Resource*           pBuff, 
                           Uint64                    _Offset, 
                           Uint64                    _Size,
                           void*                     _CPUAddress,
                           D3D12_GPU_VIRTUAL_ADDRESS _GPUAddress
#ifdef DEVELOPMENT
                         , Uint64                    _DvpCtxFrameNumber
#endif
    )noexcept :
        pBuffer          (pBuff), 
        Offset           (_Offset),
        Size             (_Size),
        CPUAddress       (_CPUAddress),
        GPUAddress       (_GPUAddress)
#ifdef DEVELOPMENT
      , DvpCtxFrameNumber(_DvpCtxFrameNumber)
#endif
    {}

    ID3D12Resource*           pBuffer    = nullptr;	// The D3D buffer associated with this memory.
    Uint64                    Offset     = 0;			// Offset from start of buffer resource
    Uint64                    Size       = 0;			// Reserved size of this allocation
    void*                     CPUAddress = nullptr;   // The CPU-writeable address
    D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;	// The GPU-visible address
#ifdef DEVELOPMENT
    Uint64 DvpCtxFrameNumber = static_cast<Uint64>(-1);
#endif
};

    
class D3D12DynamicPage
{
public:
    D3D12DynamicPage(ID3D12Device* pd3d12Device, Uint64 Size);
    
    D3D12DynamicPage            (const D3D12DynamicPage&)  = delete;
    D3D12DynamicPage            (      D3D12DynamicPage&&) = default;
    D3D12DynamicPage& operator= (const D3D12DynamicPage&)  = delete;
    D3D12DynamicPage& operator= (      D3D12DynamicPage&&) = delete;

    void* GetCPUAddress(Uint64 Offset)
    {
        VERIFY_EXPR(m_pd3d12Buffer);
        VERIFY(Offset < GetSize(), "Offset (", Offset, ") exceeds buffer size (", GetSize(), ")");
        return reinterpret_cast<Uint8*>(m_CPUVirtualAddress) + Offset;
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress(Uint64 Offset)
    {
        VERIFY_EXPR(m_pd3d12Buffer);
        VERIFY(Offset < GetSize(), "Offset (", Offset, ") exceeds buffer size (", GetSize(), ")");
        return m_GPUVirtualAddress + Offset;
    }

    ID3D12Resource* GetD3D12Buffer()
    {
        return m_pd3d12Buffer;
    }

    Uint64 GetSize()const
    {
        VERIFY_EXPR(m_pd3d12Buffer);
        return m_pd3d12Buffer->GetDesc().Width;
    }

    bool IsValid()const { return m_pd3d12Buffer != nullptr; }

private:
    CComPtr<ID3D12Resource>   m_pd3d12Buffer;
    void*                     m_CPUVirtualAddress = nullptr; // The CPU-writeable address
    D3D12_GPU_VIRTUAL_ADDRESS m_GPUVirtualAddress = 0;	     // The GPU-visible address
};


class D3D12DynamicMemoryManager
{
public:
    D3D12DynamicMemoryManager(IMemoryAllocator& Allocator, 
                              ID3D12Device*     pd3d12Device,
                              Uint32            NumPagesToReserve,
                              Uint64            PageSize);
    ~D3D12DynamicMemoryManager();

    D3D12DynamicMemoryManager            (const D3D12DynamicMemoryManager&)  = delete;
    D3D12DynamicMemoryManager            (      D3D12DynamicMemoryManager&&) = delete;
    D3D12DynamicMemoryManager& operator= (const D3D12DynamicMemoryManager&)  = delete;
    D3D12DynamicMemoryManager& operator= (      D3D12DynamicMemoryManager&&) = delete;

    void DiscardPages(std::vector<D3D12DynamicPage>& Pages, Uint64 FenceValue)
    {
        std::lock_guard<std::mutex> Lock(m_StalePagesMtx);
        for(auto& Page : Pages)
            m_StalePages.emplace_back(FenceValue, std::move(Page));
    }

    void ReleaseStalePages(Uint64 LastCompletedFenceValue)
    {
        std::lock_guard<std::mutex> AvailablePagesLock(m_AvailablePagesMtx);
        std::lock_guard<std::mutex> StalePagesLock(m_StalePagesMtx);
        while (!m_StalePages.empty())
        {
            auto& FirstPage = m_StalePages.front();
            if (FirstPage.FenceValue <= LastCompletedFenceValue)
            {
                auto PageSize = FirstPage.Page.GetSize();
                m_AvailablePages.emplace(PageSize, std::move(FirstPage.Page));
                m_StalePages.pop_front();
            }
            else
                break;
        }
    }

    void Destroy(Uint64 LastCompletedFenceValue);

    D3D12DynamicPage AllocatePage(Uint64 SizeInBytes);

private:
    CComPtr<ID3D12Device> m_pd3d12Device;
    
    std::mutex m_AvailablePagesMtx;
    using AvailablePagesMapElemType = std::pair<Uint64, D3D12DynamicPage>;
    std::multimap<Uint64, D3D12DynamicPage, std::less<Uint64>, STDAllocatorRawMem<AvailablePagesMapElemType> > m_AvailablePages;

    std::mutex m_StalePagesMtx;
    struct StalePageInfo
    {
        StalePageInfo(Uint64 _FenceValue, D3D12DynamicPage&& _Page) : 
            FenceValue(_FenceValue),
            Page      (std::move(_Page))
        {}

        Uint64           FenceValue;
        D3D12DynamicPage Page;
    };
    std::deque<StalePageInfo, STDAllocatorRawMem<StalePageInfo> > m_StalePages;
};


class D3D12DynamicHeap
{
public:
    D3D12DynamicHeap(D3D12DynamicMemoryManager& DynamicMemMgr, std::string HeapName, Uint64 PageSize) :
        m_DynamicMemMgr (DynamicMemMgr),
        m_HeapName      (std::move(HeapName)),
        m_PageSize      (PageSize)
    {}

    D3D12DynamicHeap            (const D3D12DynamicHeap&) = delete;
    D3D12DynamicHeap            (D3D12DynamicHeap&&)      = delete;
    D3D12DynamicHeap& operator= (const D3D12DynamicHeap&) = delete;
    D3D12DynamicHeap& operator= (D3D12DynamicHeap&&)      = delete;
    
    ~D3D12DynamicHeap();

    D3D12DynamicAllocation Allocate(Uint64 SizeInBytes, Uint64 Alignment, Uint64 DvpCtxFrameNumber);
    void FinishFrame(Uint64 FenceValue);

    static constexpr Uint64 InvalidOffset = static_cast<Uint64>(-1);

private:
    D3D12DynamicMemoryManager& m_DynamicMemMgr;
    const std::string m_HeapName;

    std::vector<D3D12DynamicPage> m_AllocatedPages;
    
    const Uint64 m_PageSize;

    Uint64 m_CurrOffset        = InvalidOffset;
    Uint64 m_AvailableSize     = 0;

    Uint64 m_CurrAllocatedSize = 0;
    Uint64 m_CurrUsedSize      = 0;
    Uint64 m_PeakAllocatedSize = 0;
    Uint64 m_PeakUsedSize      = 0;
};

}
