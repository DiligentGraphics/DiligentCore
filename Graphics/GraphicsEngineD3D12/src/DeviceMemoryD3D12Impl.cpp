/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "DeviceMemoryD3D12Impl.hpp"

#include "RenderDeviceD3D12Impl.hpp"
#include "DeviceContextD3D12Impl.hpp"

#include "D3D12TypeConversions.hpp"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

DeviceMemoryD3D12Impl::DeviceMemoryD3D12Impl(IReferenceCounters*           pRefCounters,
                                             RenderDeviceD3D12Impl*        pDeviceD3D11,
                                             const DeviceMemoryCreateInfo& MemCI) :
    TDeviceMemoryBase{pRefCounters, pDeviceD3D11, MemCI}
{
    if (!Resize(MemCI.InitialSize))
        LOG_ERROR_AND_THROW("Failed to allocate device memory");
}

DeviceMemoryD3D12Impl::~DeviceMemoryD3D12Impl()
{
    // AZ TODO: use release queue
}

IMPLEMENT_QUERY_INTERFACE(DeviceMemoryD3D12Impl, IID_DeviceMemoryD3D12, TDeviceMemoryBase)

Bool DeviceMemoryD3D12Impl::Resize(Uint64 NewSize)
{
    auto*      pd3d12Device = m_pDevice->GetD3D12Device();
    const auto NewPageCount = StaticCast<size_t>(NewSize / m_Desc.PageSize);
    const auto OldPageCount = m_Pages.size();
    const bool UseNVApi     = m_pDevice->GetDummyNVApiHeap() != nullptr;

    D3D12_HEAP_DESC d3d12HeapDesc{};
    d3d12HeapDesc.SizeInBytes                     = m_Desc.PageSize;
    d3d12HeapDesc.Properties.Type                 = D3D12_HEAP_TYPE_CUSTOM;
    d3d12HeapDesc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
    d3d12HeapDesc.Properties.MemoryPoolPreference = m_pDevice->GetAdapterInfo().Type == ADAPTER_TYPE_DISCRETE ? D3D12_MEMORY_POOL_L1 : D3D12_MEMORY_POOL_L0;
    d3d12HeapDesc.Properties.CreationNodeMask     = 0;                                          // equivalent to 1
    d3d12HeapDesc.Properties.VisibleNodeMask      = 0;                                          // equivalent to 1
    d3d12HeapDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; // AZ TODO: D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
    d3d12HeapDesc.Flags                           = D3D12_HEAP_FLAG_NONE;                       // AZ TODO: D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED

    m_Pages.reserve(NewPageCount);

    for (size_t i = OldPageCount; i < NewPageCount; ++i)
    {
        CComPtr<ID3D12Heap> pd3d12Heap;
#ifdef DILIGENT_ENABLE_D3D_NVAPI
        if (UseNVApi)
        {
            if (NvAPI_D3D12_CreateHeap(pd3d12Device, &d3d12HeapDesc, IID_PPV_ARGS(&pd3d12Heap)) != NVAPI_OK)
            {
                LOG_ERROR_MESSAGE("Failed to create D3D12 heap using NVApi");
                return false;
            }
        }
        else
#endif
        {
            if (FAILED(pd3d12Device->CreateHeap(&d3d12HeapDesc, IID_PPV_ARGS(&pd3d12Heap))))
            {
                LOG_ERROR_MESSAGE("Failed to create D3D12 heap");
                return false;
            }
        }
        m_Pages.emplace_back(std::move(pd3d12Heap));
    }

    if (NewPageCount < OldPageCount)
    {
        // AZ TODO: use release queue
        m_Pages.resize(NewPageCount);
    }

    return true;
}

Uint64 DeviceMemoryD3D12Impl::GetCapacity()
{
    return m_Desc.PageSize * m_Pages.size();
}

Bool DeviceMemoryD3D12Impl::IsCompatible(IDeviceObject* pResource) const
{
    return true;
}

DeviceMemoryRangeD3D12 DeviceMemoryD3D12Impl::GetRange(Uint64 Offset, Uint64 Size)
{
    const auto             PageIdx = static_cast<size_t>(Offset / m_Desc.PageSize);
    DeviceMemoryRangeD3D12 Result{};

    if (PageIdx >= m_Pages.size())
    {
        LOG_ERROR_MESSAGE("DeviceMemoryD3D12Impl::GetRange(): Offset is greater than allocated space");
        return Result;
    }

    const auto OffsetInPage = Offset % m_Desc.PageSize;
    if (OffsetInPage + Size > m_Desc.PageSize)
    {
        LOG_ERROR_MESSAGE("DeviceMemoryD3D12Impl::GetRange(): Offset and Size must be inside single page");
        return Result;
    }

    Result.Offset  = OffsetInPage;
    Result.pHandle = m_Pages[PageIdx];
    Result.Size    = std::min(m_Desc.PageSize - OffsetInPage, Size);

    return Result;
}

} // namespace Diligent
