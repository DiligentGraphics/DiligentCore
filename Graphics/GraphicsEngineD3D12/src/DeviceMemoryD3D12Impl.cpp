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
    m_pDevice->SafeReleaseDeviceObject(std::move(m_Pages), m_Desc.ImmediateContextMask);
}

IMPLEMENT_QUERY_INTERFACE(DeviceMemoryD3D12Impl, IID_DeviceMemoryD3D12, TDeviceMemoryBase)

inline CComPtr<ID3D12Heap> CreateD3D12Heap(RenderDeviceD3D12Impl* pDevice, const D3D12_HEAP_DESC& d3d12HeapDesc)
{
    auto* const pd3d12Device = pDevice->GetD3D12Device();

    CComPtr<ID3D12Heap> pd3d12Heap;
#ifdef DILIGENT_ENABLE_D3D_NVAPI
    const auto UseNVApi = pDevice->GetDummyNVApiHeap() != nullptr;
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

    return pd3d12Heap;
}

Bool DeviceMemoryD3D12Impl::Resize(Uint64 NewSize)
{
    D3D12_HEAP_DESC d3d12HeapDesc{};
    d3d12HeapDesc.SizeInBytes                     = m_Desc.PageSize;
    d3d12HeapDesc.Properties.Type                 = D3D12_HEAP_TYPE_CUSTOM;
    d3d12HeapDesc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
    d3d12HeapDesc.Properties.MemoryPoolPreference = m_pDevice->GetAdapterInfo().Type == ADAPTER_TYPE_DISCRETE ? D3D12_MEMORY_POOL_L1 : D3D12_MEMORY_POOL_L0;
    d3d12HeapDesc.Properties.CreationNodeMask     = 0;                                                                  // equivalent to 1
    d3d12HeapDesc.Properties.VisibleNodeMask      = 0;                                                                  // equivalent to 1
    d3d12HeapDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;                         // AZ TODO: D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
    d3d12HeapDesc.Flags                           = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS; // AZ TODO: D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED

    const auto NewPageCount = StaticCast<size_t>(NewSize / m_Desc.PageSize);
    m_Pages.reserve(NewPageCount);

    while (m_Pages.size() < NewPageCount)
    {
        m_Pages.emplace_back(CreateD3D12Heap(m_pDevice, d3d12HeapDesc));
    }

    while (m_Pages.size() > NewPageCount)
    {
        m_pDevice->SafeReleaseDeviceObject(std::move(m_Pages.back()), m_Desc.ImmediateContextMask);
        m_Pages.pop_back();
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
    const auto PageIdx = static_cast<size_t>(Offset / m_Desc.PageSize);

    DeviceMemoryRangeD3D12 Range{};
    if (PageIdx >= m_Pages.size())
    {
        LOG_ERROR_MESSAGE("DeviceMemoryD3D12Impl::GetRange(): Offset is out of bounds of allocated space");
        return Range;
    }

    const auto OffsetInPage = Offset % m_Desc.PageSize;
    if (OffsetInPage + Size > m_Desc.PageSize)
    {
        LOG_ERROR_MESSAGE("DeviceMemoryD3D12Impl::GetRange(): Offset and Size must be inside a single page");
        return Range;
    }

    Range.Offset  = OffsetInPage;
    Range.pHandle = m_Pages[PageIdx];
    Range.Size    = std::min(m_Desc.PageSize - OffsetInPage, Size);

    return Range;
}

} // namespace Diligent
