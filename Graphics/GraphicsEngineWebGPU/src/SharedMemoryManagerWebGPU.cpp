/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#include "Cast.hpp"
#include "Align.hpp"
#include "SharedMemoryManagerWebGPU.hpp"
#include "DebugUtilities.hpp"

namespace Diligent
{

bool SharedMemoryManagerWebGPU::Allocation::IsEmpty() const
{
    return wgpuBuffer == nullptr;
}

SharedMemoryManagerWebGPU::Page::Page(SharedMemoryManagerWebGPU* _pMgr, Uint64 _Size) :
    pMgr{_pMgr},
    PageSize{_Size}
{
    WGPUBufferDescriptor wgpuBufferDesc{};
    wgpuBufferDesc.label = "Shared memory page";
    wgpuBufferDesc.size  = _Size;
    wgpuBufferDesc.usage =
        WGPUBufferUsage_CopyDst |
        WGPUBufferUsage_CopySrc |
        WGPUBufferUsage_Uniform |
        WGPUBufferUsage_Storage |
        WGPUBufferUsage_Vertex |
        WGPUBufferUsage_Index |
        WGPUBufferUsage_Indirect;
    wgpuBuffer.Reset(wgpuDeviceCreateBuffer(pMgr->m_wgpuDevice, &wgpuBufferDesc));
    MappedData.resize(StaticCast<size_t>(_Size));
    pData = MappedData.data();
    LOG_INFO_MESSAGE("Created a new shared memory page, size: ", PageSize >> 10, " KB");
}

SharedMemoryManagerWebGPU::Page::Page(Page&& RHS) noexcept :
    //clang-format off
    pMgr{RHS.pMgr},
    wgpuBuffer{std::move(RHS.wgpuBuffer)},
    MappedData{std::move(RHS.MappedData)},
    PageSize{RHS.PageSize},
    CurrOffset{RHS.CurrOffset},
    pData{RHS.pData}
// clang-format on
{
    RHS = Page{};
}

SharedMemoryManagerWebGPU::Page& SharedMemoryManagerWebGPU::Page::operator=(Page&& RHS) noexcept
{
    if (&RHS == this)
        return *this;

    pMgr       = RHS.pMgr;
    wgpuBuffer = std::move(RHS.wgpuBuffer);
    MappedData = std::move(RHS.MappedData);
    PageSize   = RHS.PageSize;
    CurrOffset = RHS.CurrOffset;
    pData      = RHS.pData;

    RHS.pMgr       = nullptr;
    RHS.PageSize   = 0;
    RHS.CurrOffset = 0;
    RHS.pData      = nullptr;

    return *this;
}

SharedMemoryManagerWebGPU::Page::~Page()
{
    VERIFY(CurrOffset == 0, "Destroying a page that has not been recycled");
}

SharedMemoryManagerWebGPU::Allocation SharedMemoryManagerWebGPU::Page::Allocate(Uint64 Size, Uint64 Alignment)
{
    VERIFY(IsPowerOfTwo(Alignment), "Alignment size must be a power of two");
    Allocation Alloc;
    Alloc.Offset = AlignUp(CurrOffset, Alignment);
    Alloc.Size   = AlignUp(Size, Alignment);
    if (Alloc.Offset + Alloc.Size <= PageSize)
    {
        Alloc.wgpuBuffer = wgpuBuffer.Get();
        Alloc.pData      = pData + Alloc.Offset;
        CurrOffset       = Alloc.Offset + Alloc.Size;
        return Alloc;
    }
    return Allocation{};
}

void SharedMemoryManagerWebGPU::Page::Recycle()
{
    if (pMgr == nullptr)
    {
        UNEXPECTED("The page is empty.");
        return;
    }

    pMgr->RecyclePage(std::move(*this));
}

bool SharedMemoryManagerWebGPU::Page::IsEmpty() const
{
    return wgpuBuffer.Get() == nullptr;
}

SharedMemoryManagerWebGPU::SharedMemoryManagerWebGPU(WGPUDevice wgpuDevice, Uint64 PageSize) :
    m_PageSize{PageSize},
    m_wgpuDevice{wgpuDevice}
{
    VERIFY(IsPowerOfTwo(m_PageSize), "Page size must be power of two");
}

SharedMemoryManagerWebGPU::~SharedMemoryManagerWebGPU()
{
    VERIFY(m_DbgPageCounter == m_AvailablePages.size(),
           "Not all pages have been recycled. This may result in a crash if the page is recycled later.");
    Uint64 TotalSize = 0;
    for (const auto& page : m_AvailablePages)
        TotalSize += page.PageSize;
    LOG_INFO_MESSAGE("SharedMemoryManagerMtl: total allocated memory: ", TotalSize >> 10, " KB");
}

SharedMemoryManagerWebGPU::Page SharedMemoryManagerWebGPU::GetPage(Uint64 Size)
{
    auto PageSize = m_PageSize;
    while (PageSize < Size)
        PageSize *= 2;

    auto Iter = m_AvailablePages.begin();
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

#if DILIGENT_DEBUG
    m_DbgPageCounter++;
#endif

    return Page{this, PageSize};
}

void SharedMemoryManagerWebGPU::RecyclePage(Page&& page)
{
    page.CurrOffset = 0;
    m_AvailablePages.emplace_back(std::move(page));
}

} // namespace Diligent
