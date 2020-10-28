/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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
#include "TopLevelASD3D12Impl.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "DeviceContextD3D12Impl.hpp"
#include "D3D12TypeConversions.hpp"
#include "GraphicsAccessories.hpp"
#include "DXGITypeConversions.hpp"
#include "EngineMemory.h"
#include "StringTools.hpp"

namespace Diligent
{

TopLevelASD3D12Impl::TopLevelASD3D12Impl(IReferenceCounters*          pRefCounters,
                                         class RenderDeviceD3D12Impl* pDeviceD3D12,
                                         const TopLevelASDesc&        Desc,
                                         bool                         bIsDeviceInternal) :
    TTopLevelASBase{pRefCounters, pDeviceD3D12, Desc, bIsDeviceInternal}
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO TopLevelPrebuildInfo = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  TopLevelInputs       = {};

    TopLevelInputs.Type        = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    TopLevelInputs.Flags       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    TopLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    TopLevelInputs.NumDescs    = Desc.MaxInstanceCount;

    VERIFY_EXPR(Desc.MaxInstanceCount <= D3D12_RAYTRACING_MAX_INSTANCES_PER_TOP_LEVEL_ACCELERATION_STRUCTURE);

    auto* pd3d12Device = pDeviceD3D12->GetD3D12Device5();

    pd3d12Device->GetRaytracingAccelerationStructurePrebuildInfo(&TopLevelInputs, &TopLevelPrebuildInfo);
    if (TopLevelPrebuildInfo.ResultDataMaxSizeInBytes == 0)
        LOG_ERROR_AND_THROW("Failed to get ray tracing acceleration structure prebuild info");

    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    HeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask     = 1;
    HeapProps.VisibleNodeMask      = 1;

    D3D12_RESOURCE_DESC ASDesc = {};
    ASDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
    ASDesc.Alignment           = 0;
    ASDesc.Width               = TopLevelPrebuildInfo.ResultDataMaxSizeInBytes;
    ASDesc.Height              = 1;
    ASDesc.DepthOrArraySize    = 1;
    ASDesc.MipLevels           = 1;
    ASDesc.Format              = DXGI_FORMAT_UNKNOWN;
    ASDesc.SampleDesc.Count    = 1;
    ASDesc.SampleDesc.Quality  = 0;
    ASDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ASDesc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    auto hr = pd3d12Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
                                                    &ASDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
                                                    __uuidof(m_pd3d12Resource),
                                                    reinterpret_cast<void**>(static_cast<ID3D12Resource**>(&m_pd3d12Resource)));
    if (FAILED(hr))
        LOG_ERROR_AND_THROW("Failed to create D3D12 Top-level acceleration structure");

    if (*m_Desc.Name != 0)
        m_pd3d12Resource->SetName(WidenString(m_Desc.Name).c_str());

    m_ScratchSize.Build  = static_cast<Uint32>(TopLevelPrebuildInfo.ScratchDataSizeInBytes);
    m_ScratchSize.Update = static_cast<Uint32>(TopLevelPrebuildInfo.UpdateScratchDataSizeInBytes);

    m_DescriptorHandle = pDeviceD3D12->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    SRVDesc.ViewDimension                            = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    SRVDesc.Shader4ComponentMapping                  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SRVDesc.Format                                   = DXGI_FORMAT_UNKNOWN;
    SRVDesc.RaytracingAccelerationStructure.Location = GetGPUAddress();
    pd3d12Device->CreateShaderResourceView(nullptr, &SRVDesc, m_DescriptorHandle.GetCpuHandle());
}

TopLevelASD3D12Impl::~TopLevelASD3D12Impl()
{
    // D3D12 object can only be destroyed when it is no longer used by the GPU
    auto* pDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>(GetDevice());
    pDeviceD3D12Impl->SafeReleaseDeviceObject(std::move(m_pd3d12Resource), m_Desc.CommandQueueMask);
}

IMPLEMENT_QUERY_INTERFACE(TopLevelASD3D12Impl, IID_TopLevelASD3D12, TTopLevelASBase)

} // namespace Diligent
