/*
 *  Copyright 2024 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "DynamicMemoryManagerWebGPU.hpp"
#include "Align.hpp"
#include "Cast.hpp"

namespace Diligent
{

DynamicMemoryManagerWebGPU::Page::Page(DynamicMemoryManagerWebGPU* _pMgr, Uint64 Size, Uint64 Offset) :
    pMgr{_pMgr},
    PageSize{Size},
    BufferOffset{Offset}
{
    VERIFY(IsPowerOfTwo(PageSize), "Page size must be power of two");
}

DynamicMemoryManagerWebGPU::Page::Page(Page&& RHS) noexcept :
    //clang-format off
    pMgr{RHS.pMgr},
    PageSize{RHS.PageSize},
    CurrOffset{RHS.CurrOffset},
    BufferOffset{RHS.BufferOffset}
// clang-format on
{
    RHS = Page{};
}

DynamicMemoryManagerWebGPU::Page& DynamicMemoryManagerWebGPU::Page::operator=(Page&& RHS) noexcept
{
    if (&RHS == this)
        return *this;

    pMgr         = RHS.pMgr;
    PageSize     = RHS.PageSize;
    CurrOffset   = RHS.CurrOffset;
    BufferOffset = RHS.BufferOffset;

    RHS.pMgr         = nullptr;
    RHS.PageSize     = 0;
    RHS.CurrOffset   = 0;
    RHS.BufferOffset = 0;

    return *this;
}

DynamicMemoryManagerWebGPU::Page::~Page()
{
    VERIFY(CurrOffset == 0, "Destroying a page that has not been recycled");
}

DynamicMemoryManagerWebGPU::Allocation DynamicMemoryManagerWebGPU::Page::Allocate(Uint64 Size, Uint64 Alignment)
{
    VERIFY(IsPowerOfTwo(Alignment), "Alignment size must be a power of two");

    Uint64 Offset    = AlignUp(CurrOffset, Alignment);
    Uint64 AllocSize = AlignUp(Size, Alignment);
    if (Offset + AllocSize <= PageSize)
    {
        Uint64     MemoryOffset = BufferOffset + Offset;
        Allocation Alloc;
        Alloc.wgpuBuffer = pMgr->m_wgpuBuffer.Get();
        Alloc.pData      = pMgr->m_MappedData.data() + MemoryOffset;
        Alloc.Offset     = MemoryOffset;
        Alloc.Size       = AllocSize;

        CurrOffset = Offset + AllocSize;
        return Alloc;
    }
    return Allocation{};
}

void DynamicMemoryManagerWebGPU::Page::FlushWrites(WGPUQueue wgpuQueue)
{
    if (CurrOffset > 0)
    {
        VERIFY_EXPR(pMgr != nullptr);
        wgpuQueueWriteBuffer(wgpuQueue, pMgr->m_wgpuBuffer.Get(), BufferOffset, GetMappedData(), StaticCast<size_t>(CurrOffset));
    }
}

void DynamicMemoryManagerWebGPU::Page::Recycle()
{
    if (pMgr == nullptr)
    {
        UNEXPECTED("The page is empty.");
        return;
    }
    pMgr->RecyclePage(std::move(*this));
}

const Uint8* DynamicMemoryManagerWebGPU::Page::GetMappedData() const
{
    if (pMgr == nullptr)
    {
        UNEXPECTED("The page is empty.");
        return nullptr;
    }

    return &pMgr->m_MappedData[static_cast<size_t>(BufferOffset)];
}


DynamicMemoryManagerWebGPU::DynamicMemoryManagerWebGPU(WGPUDevice wgpuDevice, Uint64 PageSize, Uint64 BufferSize) :
    m_PageSize{PageSize},
    m_BufferSize{BufferSize},
    m_CurrentOffset{0}
{
    WGPUBufferDescriptor wgpuBufferDesc{};
    wgpuBufferDesc.label = "Dynamic buffer";
    wgpuBufferDesc.size  = BufferSize;
    wgpuBufferDesc.usage =
        WGPUBufferUsage_CopyDst |
        WGPUBufferUsage_CopySrc |
        WGPUBufferUsage_Uniform |
        WGPUBufferUsage_Storage |
        WGPUBufferUsage_Vertex |
        WGPUBufferUsage_Index |
        WGPUBufferUsage_Indirect;
    m_wgpuBuffer.Reset(wgpuDeviceCreateBuffer(wgpuDevice, &wgpuBufferDesc));
    m_MappedData.resize(StaticCast<size_t>(BufferSize));

    LOG_INFO_MESSAGE("Created dynamic buffer: ", BufferSize >> 10, " KB");
}

DynamicMemoryManagerWebGPU::~DynamicMemoryManagerWebGPU()
{
    LOG_INFO_MESSAGE("Dynamic memory manager usage stats:\n"
                     "                       Total size: ",
                     FormatMemorySize(m_BufferSize, 2),
                     ". Peak allocated size: ", FormatMemorySize(m_CurrentOffset, 2, m_BufferSize),
                     ". Peak utilization: ",
                     std::fixed, std::setprecision(1), static_cast<double>(m_CurrentOffset) / static_cast<double>(std::max(m_BufferSize, 1ull)) * 100.0, '%');
}

DynamicMemoryManagerWebGPU::Page DynamicMemoryManagerWebGPU::GetPage(Uint64 Size)
{
    Uint64 PageSize = m_PageSize;
    while (PageSize < Size)
        PageSize *= 2;

    std::lock_guard Lock{m_AvailablePagesMtx};
    auto            Iter = m_AvailablePages.begin();
    while (Iter != m_AvailablePages.end())
    {
        if (PageSize <= Iter->PageSize)
        {
            auto Result = std::move(*Iter);
            m_AvailablePages.erase(Iter);
            return Result;
        }
        ++Iter;
    }

    Uint64 LastOffset = m_CurrentOffset;
    m_CurrentOffset += PageSize;

    if (m_CurrentOffset >= m_BufferSize)
    {
        LOG_ERROR("Requested dynamic allocation size ", m_CurrentOffset, " exceeds maximum dynamic memory size ", m_BufferSize, ". The app should increase dynamic heap size.");
        return Page{};
    }
    return Page{this, PageSize, LastOffset};
}

void DynamicMemoryManagerWebGPU::RecyclePage(Page&& Item)
{
    std::lock_guard Lock{m_AvailablePagesMtx};
    Item.CurrOffset = 0;
    m_AvailablePages.emplace_back(std::move(Item));
}

} // namespace Diligent
