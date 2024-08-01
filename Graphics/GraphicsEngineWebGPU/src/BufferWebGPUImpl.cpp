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

#include "BufferWebGPUImpl.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "DeviceContextWebGPUImpl.hpp"
#include "WebGPUTypeConversions.hpp"

namespace Diligent
{

namespace
{

Uint64 ComputeBufferAlignment(const RenderDeviceWebGPUImpl* pDevice, const BufferDesc& Desc)
{
    Uint64 Alignment = 16; // Which alignment to use for buffers that don't have any specific requirements?
    if (Desc.BindFlags & BIND_UNIFORM_BUFFER)
        Alignment = pDevice->GetAdapterInfo().Buffer.ConstantBufferOffsetAlignment;

    if (Desc.BindFlags & (BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE))
        Alignment = pDevice->GetAdapterInfo().Buffer.StructuredBufferOffsetAlignment;

    return Alignment;
}

} // namespace

BufferWebGPUImpl::BufferWebGPUImpl(IReferenceCounters*        pRefCounters,
                                   FixedBlockMemoryAllocator& BuffViewObjMemAllocator,
                                   RenderDeviceWebGPUImpl*    pDevice,
                                   const BufferDesc&          Desc,
                                   const BufferData*          pInitData,
                                   bool                       bIsDeviceInternal) :
    // clang-format off
    TBufferBase
    {
        pRefCounters,
        BuffViewObjMemAllocator,
        pDevice,
        Desc,
        bIsDeviceInternal
    },
    m_DynamicAllocations(STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<DynamicAllocation>"))
// clang-format on
{
    ValidateBufferInitData(m_Desc, pInitData);

    if (m_Desc.Usage == USAGE_UNIFIED || m_Desc.Usage == USAGE_SPARSE)
        LOG_ERROR_AND_THROW("Unified and sparse resources are not supported in WebGPU");

    m_Alignment = ComputeBufferAlignment(pDevice, m_Desc);

    WGPUBufferDescriptor wgpuBufferDesc{};
    wgpuBufferDesc.label = m_Desc.Name;
    wgpuBufferDesc.size  = AlignUp(m_Desc.Size, m_Alignment);

    const bool RequiresBackingBuffer = (m_Desc.BindFlags & BIND_UNORDERED_ACCESS) != 0 || ((m_Desc.BindFlags & BIND_SHADER_RESOURCE) != 0 && m_Desc.Mode == BUFFER_MODE_FORMATTED);

    if (m_Desc.Usage == USAGE_DYNAMIC && !RequiresBackingBuffer)
    {
        auto CtxCount = pDevice->GetNumImmediateContexts() + pDevice->GetNumDeferredContexts();
        m_DynamicAllocations.resize(CtxCount);
    }
    else
    {
        if (m_Desc.Usage == USAGE_STAGING)
        {
            if (m_Desc.CPUAccessFlags & CPU_ACCESS_READ)
            {
                wgpuBufferDesc.usage |= WGPUBufferUsage_MapRead;
                wgpuBufferDesc.usage |= WGPUBufferUsage_CopyDst;
            }

            if (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE)
            {
                wgpuBufferDesc.usage |= WGPUBufferUsage_MapWrite;
                wgpuBufferDesc.usage |= WGPUBufferUsage_CopySrc;
            }
        }
        else
        {
            wgpuBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;

            for (auto BindFlags = m_Desc.BindFlags; BindFlags != 0;)
            {
                const auto BindFlag = ExtractLSB(BindFlags);
                switch (BindFlag)
                {
                    case BIND_UNIFORM_BUFFER:
                        wgpuBufferDesc.usage |= WGPUBufferUsage_Uniform;
                        break;
                    case BIND_SHADER_RESOURCE:
                    case BIND_UNORDERED_ACCESS:
                        wgpuBufferDesc.usage |= WGPUBufferUsage_Storage;
                        break;
                    case BIND_VERTEX_BUFFER:
                        wgpuBufferDesc.usage |= WGPUBufferUsage_Vertex;
                        break;
                    case BIND_INDEX_BUFFER:
                        wgpuBufferDesc.usage |= WGPUBufferUsage_Index;
                        break;
                    case BIND_INDIRECT_DRAW_ARGS:
                        wgpuBufferDesc.usage |= WGPUBufferUsage_Indirect;
                        break;
                    default:
                        UNEXPECTED("unsupported buffer usage type");
                        break;
                }
            }
        }

        wgpuBufferDesc.mappedAtCreation = (pInitData != nullptr && pInitData->pData != nullptr);

        m_wgpuBuffer.Reset(wgpuDeviceCreateBuffer(pDevice->GetWebGPUDevice(), &wgpuBufferDesc));
        if (!m_wgpuBuffer)
            LOG_ERROR_AND_THROW("Failed to create WebGPU buffer ", " '", m_Desc.Name ? m_Desc.Name : "", '\'');

        if (wgpuBufferDesc.mappedAtCreation)
        {
            void* pData = wgpuBufferGetMappedRange(m_wgpuBuffer, 0, WGPU_WHOLE_MAP_SIZE);
            memcpy(pData, pInitData->pData, StaticCast<size_t>(pInitData->DataSize));
            wgpuBufferUnmap(m_wgpuBuffer);
        }
    }

    SetState(RESOURCE_STATE_UNDEFINED);
    m_MemoryProperties = MEMORY_PROPERTY_HOST_COHERENT;
}

BufferWebGPUImpl::BufferWebGPUImpl(IReferenceCounters*        pRefCounters,
                                   FixedBlockMemoryAllocator& BuffViewObjMemAllocator,
                                   RenderDeviceWebGPUImpl*    pDevice,
                                   const BufferDesc&          Desc,
                                   RESOURCE_STATE             InitialState,
                                   WGPUBuffer                 wgpuBuffer,
                                   bool                       bIsDeviceInternal) :
    // clang-format off
    TBufferBase
    {
        pRefCounters,
        BuffViewObjMemAllocator,
        pDevice,
        Desc,
        bIsDeviceInternal
    },
    m_wgpuBuffer{wgpuBuffer, {true}},
    m_DynamicAllocations(STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<DynamicAllocation>"))
// clang-format on
{
    m_Alignment = ComputeBufferAlignment(pDevice, Desc);
    VERIFY(m_Desc.Size % m_Alignment == 0, "Size of buffer must be aligned");
    SetState(InitialState);
    m_MemoryProperties = MEMORY_PROPERTY_HOST_COHERENT;
}

Uint64 BufferWebGPUImpl::GetNativeHandle()
{
    return BitCast<Uint64>(GetWebGPUBuffer());
}

SparseBufferProperties BufferWebGPUImpl::GetSparseProperties() const
{
    DEV_ERROR("IBuffer::GetSparseProperties() is not supported in WebGPU");
    return {};
}

WGPUBuffer BufferWebGPUImpl::GetWebGPUBuffer() const
{
    if (m_wgpuBuffer)
        return m_wgpuBuffer.Get();

    VERIFY(m_Desc.Usage == USAGE_DYNAMIC, "Dynamic buffer expected");
    return m_pDevice->GetDynamicMemoryManager().GetWGPUBuffer();
}

void BufferWebGPUImpl::Map(MAP_TYPE MapType, MAP_FLAGS MapFlags, PVoid& pMappedData)
{
    VERIFY(m_Desc.Usage == USAGE_STAGING, "Map working only for staging buffers");
    VERIFY(m_MapState.State == BufferMapState::None, "Buffer is already mapped");

    // We use lazy initialization because in web applications we cannot use blocking Map operations and to reduce memory consumption.
    if (m_MappedData.empty())
        m_MappedData.resize(StaticCast<size_t>(m_Desc.Size));

    if (MapType == MAP_READ)
    {
        auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
            if (MapStatus == WGPUBufferMapAsyncStatus_Success)
            {
                auto* pBuffer = static_cast<BufferWebGPUImpl*>(pUserData);
                VERIFY_EXPR(pBuffer->m_MapState.State == BufferMapState::Read);
                const auto* pData = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(pBuffer->m_wgpuBuffer.Get(), 0, StaticCast<size_t>(pBuffer->m_Desc.Size)));
                VERIFY_EXPR(pData != nullptr);
                memcpy(pBuffer->m_MappedData.data(), pData, StaticCast<size_t>(pBuffer->m_Desc.Size));
                wgpuBufferUnmap(pBuffer->m_wgpuBuffer.Get());
            }
            else
            {
                DEV_ERROR("Failed wgpuBufferMapAsync: ", MapStatus);
            }
        };

        m_MapState.State = BufferMapState::Read;
        wgpuBufferMapAsync(m_wgpuBuffer.Get(), WGPUMapMode_Read, 0, StaticCast<size_t>(m_Desc.Size), MapAsyncCallback, this);
        while (wgpuBufferGetMapState(m_wgpuBuffer.Get()) != WGPUBufferMapState_Unmapped)
            m_pDevice->PollEvents();

        pMappedData = m_MappedData.data();
    }
    else if (MapType == MAP_WRITE)
    {
        m_MapState.State = BufferMapState::Write;
        pMappedData      = m_MappedData.data();
    }
    else if (MapType == MAP_READ_WRITE)
    {
        LOG_ERROR("MAP_READ_WRITE is not supported in WebGPU backend");
    }
    else
    {
        UNEXPECTED("Unknown map type");
    }
}

void BufferWebGPUImpl::MapAsync(MAP_TYPE MapType, MapBufferAsyncCallback pCallback, void* pUserData)
{
    VERIFY(m_Desc.Usage == USAGE_STAGING, "MapAsync only works for staging buffers");
    VERIFY(pCallback != nullptr, "Callback must not be null");

    auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
        if (MapStatus == WGPUBufferMapAsyncStatus_Success)
        {
            auto* pBuffer = static_cast<BufferWebGPUImpl*>(pUserData);
            void* pData   = nullptr;
            if (pBuffer->m_MapState.State == BufferMapState::ReadAsync)
                pData = wgpuBufferGetMappedRange(pBuffer->m_wgpuBuffer.Get(), 0, static_cast<size_t>(pBuffer->m_Desc.Size));
            else if (pBuffer->m_MapState.State == BufferMapState::WriteAsync)
                pData = const_cast<void*>(wgpuBufferGetConstMappedRange(pBuffer->m_wgpuBuffer.Get(), 0, static_cast<size_t>(pBuffer->m_Desc.Size)));
            else
                UNEXPECTED("Unknown map type");
            VERIFY_EXPR(pData != nullptr);

            if (pBuffer->m_MapState.pCallback != nullptr)
                pBuffer->m_MapState.pCallback(pData, pBuffer->m_MapState.pUserData);
        }
        else
        {
            DEV_ERROR("wgpuBufferMapAsync failed: ", MapStatus);
        }
    };

    if (MapType == MAP_READ || MapType == MAP_WRITE)
    {
        m_MapState.State     = MapType == MAP_READ ? BufferMapState::ReadAsync : BufferMapState::WriteAsync;
        m_MapState.pUserData = pUserData;
        m_MapState.pCallback = pCallback;
        wgpuBufferMapAsync(m_wgpuBuffer.Get(), MapType == MAP_READ ? WGPUMapMode_Read : WGPUMapMode_Write, 0, static_cast<size_t>(m_Desc.Size), MapAsyncCallback, this);
    }
    else if (MapType == MAP_READ_WRITE)
    {
        LOG_ERROR("MAP_READ_WRITE is not supported in WebGPU backend");
    }
    else
    {
        UNEXPECTED("Unknown map type");
    }
}

void BufferWebGPUImpl::Unmap(MAP_TYPE MapType)
{
    VERIFY(m_Desc.Usage == USAGE_STAGING, "Unmap working only for staging buffers");
    VERIFY(m_MapState.State != BufferMapState::None, "Buffer is not mapped");

    if (m_MapState.State == BufferMapState::Read)
    {
        // Nothing to do
    }
    else if (m_MapState.State == BufferMapState::Write)
    {
        auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
            if (MapStatus == WGPUBufferMapAsyncStatus_Success)
            {
                auto* pBuffer = static_cast<BufferWebGPUImpl*>(pUserData);
                VERIFY_EXPR(pBuffer->m_MapState.State == BufferMapState::Write);
                auto* pData = static_cast<uint8_t*>(wgpuBufferGetMappedRange(pBuffer->m_wgpuBuffer.Get(), 0, StaticCast<size_t>(pBuffer->m_Desc.Size)));
                VERIFY_EXPR(pData != nullptr);
                memcpy(pData, pBuffer->m_MappedData.data(), StaticCast<size_t>(pBuffer->m_Desc.Size));
                wgpuBufferUnmap(pBuffer->m_wgpuBuffer.Get());
            }
            else
            {
                DEV_ERROR("Failed wgpuBufferMapAsync: ", MapStatus);
            }
        };

        wgpuBufferMapAsync(m_wgpuBuffer.Get(), WGPUMapMode_Write, 0, StaticCast<size_t>(m_Desc.Size), MapAsyncCallback, this);
        while (wgpuBufferGetMapState(m_wgpuBuffer.Get()) != WGPUBufferMapState_Unmapped)
            m_pDevice->PollEvents();
    }
    else if (m_MapState.State == BufferMapState::ReadAsync || m_MapState.State == BufferMapState::WriteAsync)
    {
        wgpuBufferUnmap(m_wgpuBuffer.Get());
    }
    else
    {
        UNEXPECTED("Unknown map type");
    }

    m_MapState = {};
}

Uint64 BufferWebGPUImpl::GetAlignment() const
{
    return m_Alignment;
}

const DynamicMemoryManagerWebGPU::Allocation& BufferWebGPUImpl::GetDynamicAllocation(DeviceContextIndex CtxId) const
{
    return m_DynamicAllocations[CtxId];
}

void BufferWebGPUImpl::SetDynamicAllocation(DeviceContextIndex CtxId, DynamicMemoryManagerWebGPU::Allocation&& Allocation)
{
    m_DynamicAllocations[CtxId] = std::move(Allocation);
}

void BufferWebGPUImpl::CreateViewInternal(const BufferViewDesc& OrigViewDesc, IBufferView** ppView, bool IsDefaultView)
{
    VERIFY(ppView != nullptr, "Null pointer provided");
    if (!ppView) return;
    VERIFY(*ppView == nullptr, "Overwriting reference to existing object may cause memory leaks");

    *ppView = nullptr;

    try
    {
        auto* const pDeviceWebGPU = GetDevice();

        auto ViewDesc = OrigViewDesc;
        ValidateAndCorrectBufferViewDesc(m_Desc, ViewDesc, pDeviceWebGPU->GetAdapterInfo().Buffer.StructuredBufferOffsetAlignment);

        auto& BuffViewAllocator = pDeviceWebGPU->GetBuffViewObjAllocator();
        VERIFY(&BuffViewAllocator == &m_dbgBuffViewAllocator, "Buffer view allocator does not match allocator provided at buffer initialization");

        if (ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS || ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE)
            *ppView = NEW_RC_OBJ(BuffViewAllocator, "BufferViewWebGPUImpl instance", BufferViewWebGPUImpl, IsDefaultView ? this : nullptr)(pDeviceWebGPU, ViewDesc, this, IsDefaultView, m_bIsDeviceInternal);

        if (!IsDefaultView && *ppView)
            (*ppView)->AddRef();
    }
    catch (const std::runtime_error&)
    {
        const auto* ViewTypeName = GetBufferViewTypeLiteralName(OrigViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", OrigViewDesc.Name ? OrigViewDesc.Name : "", "\" (", ViewTypeName, ") for buffer \"", m_Desc.Name, "\"");
    }
}

} // namespace Diligent
