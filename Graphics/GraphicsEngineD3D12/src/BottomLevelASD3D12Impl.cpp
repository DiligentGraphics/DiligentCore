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
#include "BottomLevelASD3D12Impl.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "DeviceContextD3D12Impl.hpp"
#include "D3D12TypeConversions.hpp"
#include "GraphicsAccessories.hpp"
#include "DXGITypeConversions.hpp"
#include "EngineMemory.h"
#include "StringTools.hpp"

namespace Diligent
{

BottomLevelASD3D12Impl::BottomLevelASD3D12Impl(IReferenceCounters*          pRefCounters,
                                               class RenderDeviceD3D12Impl* pDeviceD3D12,
                                               const BottomLevelASDesc&     Desc,
                                               bool                         bIsDeviceInternal) :
    TBottomLevelASBase{pRefCounters, pDeviceD3D12, Desc, bIsDeviceInternal}
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO BottomLevelPrebuildInfo = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS  BottomLevelInputs       = {};
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>           Geometries;

    if (m_Desc.pTriangles != nullptr)
    {
        Geometries.resize(m_Desc.TriangleCount);
        Uint32 MaxPrimitiveCount = 0;
        for (uint32_t i = 0; i < m_Desc.TriangleCount; ++i)
        {
            auto& src = m_Desc.pTriangles[i];
            auto& dst = Geometries[i];

            dst.Type                                 = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            dst.Flags                                = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
            dst.Triangles.VertexBuffer.StartAddress  = 0;
            dst.Triangles.VertexBuffer.StrideInBytes = 0;
            dst.Triangles.VertexFormat               = TypeToDXGI_Format(src.VertexValueType, src.VertexComponentCount, src.VertexValueType < VT_FLOAT16);
            dst.Triangles.VertexCount                = src.MaxVertexCount;
            dst.Triangles.IndexCount                 = src.MaxIndexCount;
            dst.Triangles.IndexFormat                = ValueTypeToIndexType(src.IndexType);
            dst.Triangles.IndexBuffer                = 0;
            dst.Triangles.Transform3x4               = 0;

            MaxPrimitiveCount += src.MaxIndexCount ? src.MaxIndexCount / 3 : src.MaxVertexCount / 3;
        }
        VERIFY_EXPR(MaxPrimitiveCount <= D3D12_RAYTRACING_MAX_PRIMITIVES_PER_BOTTOM_LEVEL_ACCELERATION_STRUCTURE);
    }
    else if (m_Desc.pBoxes != nullptr)
    {
        Geometries.resize(m_Desc.BoxCount);
        Uint32 MaxBoxCount = 0;
        for (uint32_t i = 0; i < m_Desc.BoxCount; ++i)
        {
            auto& src = m_Desc.pBoxes[i];
            auto& dst = Geometries[i];

            dst.Type                      = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
            dst.Flags                     = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
            dst.AABBs.AABBCount           = src.MaxBoxCount;
            dst.AABBs.AABBs.StartAddress  = 0;
            dst.AABBs.AABBs.StrideInBytes = 0;

            MaxBoxCount += src.MaxBoxCount;
        }
        VERIFY_EXPR(MaxBoxCount <= D3D12_RAYTRACING_MAX_PRIMITIVES_PER_BOTTOM_LEVEL_ACCELERATION_STRUCTURE);
    }

    VERIFY_EXPR(Geometries.size() <= D3D12_RAYTRACING_MAX_GEOMETRIES_PER_BOTTOM_LEVEL_ACCELERATION_STRUCTURE);

    BottomLevelInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    BottomLevelInputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    BottomLevelInputs.DescsLayout    = D3D12_ELEMENTS_LAYOUT_ARRAY;
    BottomLevelInputs.pGeometryDescs = Geometries.data();
    BottomLevelInputs.NumDescs       = static_cast<UINT>(Geometries.size());

    auto* pd3d12Device = pDeviceD3D12->GetD3D12Device5();

    pd3d12Device->GetRaytracingAccelerationStructurePrebuildInfo(&BottomLevelInputs, &BottomLevelPrebuildInfo);
    if (BottomLevelPrebuildInfo.ResultDataMaxSizeInBytes == 0)
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
    ASDesc.Width               = BottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;
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
        LOG_ERROR_AND_THROW("Failed to create D3D12 Bottom-level acceleration structure");

    if (*m_Desc.Name != 0)
        m_pd3d12Resource->SetName(WidenString(m_Desc.Name).c_str());

    m_ScratchSize.Build  = static_cast<Uint32>(BottomLevelPrebuildInfo.ScratchDataSizeInBytes);
    m_ScratchSize.Update = static_cast<Uint32>(BottomLevelPrebuildInfo.UpdateScratchDataSizeInBytes);
}

BottomLevelASD3D12Impl::~BottomLevelASD3D12Impl()
{
    // D3D12 object can only be destroyed when it is no longer used by the GPU
    auto* pDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>(GetDevice());
    pDeviceD3D12Impl->SafeReleaseDeviceObject(std::move(m_pd3d12Resource), m_Desc.CommandQueueMask);
}

IMPLEMENT_QUERY_INTERFACE(BottomLevelASD3D12Impl, IID_BottomLevelASD3D12, TBottomLevelASBase)

} // namespace Diligent
