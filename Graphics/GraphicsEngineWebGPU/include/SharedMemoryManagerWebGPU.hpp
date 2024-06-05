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

#pragma once

/// \file
/// Declaration of Diligent::SharedMemoryManagerWebGPU class

#include "WebGPUObjectWrappers.hpp"
#include "BasicTypes.h"

namespace Diligent
{

class SharedMemoryManagerWebGPU
{
public:
    struct Allocation
    {
        bool IsEmpty() const;

        WGPUBuffer wgpuBuffer = nullptr;
        Uint64     Offset     = 0;
        Uint64     Size       = 0;
        Uint8*     pData      = nullptr;
    };

    struct Page
    {
        Page() noexcept = default;
        Page(SharedMemoryManagerWebGPU* pMgr, Uint64 Size);

        Page(const Page&) = delete;
        Page& operator=(const Page&) = delete;

        Page(Page&& RHS) noexcept;
        Page& operator=(Page&& RHS) noexcept;

        ~Page();

        Allocation Allocate(Uint64 Size, Uint64 Alignment = 16);

        void Recycle();

        bool IsEmpty() const;

        SharedMemoryManagerWebGPU* pMgr = nullptr;
        WebGPUBufferWrapper        wgpuBuffer;
        std::vector<Uint8>         MappedData;

        Uint64 PageSize   = 0;
        Uint64 CurrOffset = 0;
        Uint8* pData      = nullptr;
    };

    SharedMemoryManagerWebGPU(WGPUDevice wgpuDevice, Uint64 PageSize);

    ~SharedMemoryManagerWebGPU();

    Page GetPage(Uint64 Size);

private:
    void RecyclePage(Page&& page);

private:
    const Uint64      m_PageSize;
    WGPUDevice        m_wgpuDevice;
    std::vector<Page> m_AvailablePages;
#if DILIGENT_DEBUG
    Uint32 m_DbgPageCounter{0};
#endif
};

} // namespace Diligent
