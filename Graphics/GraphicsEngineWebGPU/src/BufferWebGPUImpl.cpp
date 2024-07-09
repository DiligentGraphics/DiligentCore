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
    Uint64 Alignment = 16;
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
                                   const BufferData*          pInitData) :
    // clang-format off
    TBufferBase
    {
        pRefCounters,
        BuffViewObjMemAllocator,
        pDevice,
        Desc,
        false
    },
    m_DynamicAllocations(STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<DynamicAllocation>"))
// clang-format on
{
    ValidateBufferInitData(m_Desc, pInitData);

    if (m_Desc.Usage == USAGE_UNIFIED || m_Desc.Usage == USAGE_SPARSE)
        LOG_ERROR_AND_THROW("Unified and sparse resources are not supported in WebGPU");

    m_Alignment = ComputeBufferAlignment(pDevice, m_Desc);
    m_Desc.Size = AlignUp(m_Desc.Size, m_Alignment);

    WGPUBufferDescriptor wgpuBufferDesc{};
    wgpuBufferDesc.label = m_Desc.Name;
    wgpuBufferDesc.size  = m_Desc.Size;

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
                wgpuBufferDesc.usage |= WGPUMapMode_Read;
                wgpuBufferDesc.usage |= WGPUBufferUsage_CopyDst;
            }

            if (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE)
            {
                wgpuBufferDesc.usage |= WGPUMapMode_Write;
                wgpuBufferDesc.usage |= WGPUBufferUsage_CopySrc;
            }

            m_MappedData.resize(StaticCast<size_t>(m_Desc.Size));
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

        const auto InitializeBuffer = (pInitData != nullptr && pInitData->pData != nullptr);
        m_wgpuBuffer.Reset(wgpuDeviceCreateBuffer(pDevice->GetWebGPUDevice(), &wgpuBufferDesc));
        if (!m_wgpuBuffer)
            LOG_ERROR_AND_THROW("Failed to create WebGPU buffer ", " '", m_Desc.Name ? m_Desc.Name : "", '\'');

        if (InitializeBuffer)
        {
            VERIFY_EXPR(pDevice->GetNumImmediateContexts() == 1);
            auto pContext = pDevice->GetImmediateContext(0);
            wgpuQueueWriteBuffer(pContext->GetWebGPUQueue(), m_wgpuBuffer, 0, pInitData->pData, StaticCast<size_t>(pInitData->DataSize));
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
                                   WGPUBuffer                 wgpuBuffer) :
    // clang-format off
    TBufferBase
    {
        pRefCounters,
        BuffViewObjMemAllocator,
        pDevice,
        Desc,
        false
    },
    m_wgpuBuffer{wgpuBuffer, {true}},
    m_DynamicAllocations(STD_ALLOCATOR_RAW_MEM(DynamicAllocation, GetRawAllocator(), "Allocator for vector<DynamicAllocation>"))
// clang-format on
{
    if (m_Desc.Usage == USAGE_STAGING)
        m_MappedData.resize(StaticCast<size_t>(m_Desc.Size));

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
    else
    {
        VERIFY(m_Desc.Usage == USAGE_DYNAMIC, "Dynamic buffer expected");
        return m_pDevice->GetDynamicMemoryManager().GetWGPUBuffer();
    }
}

void BufferWebGPUImpl::Map(MAP_TYPE MapType, Uint32 MapFlags, PVoid& pMappedData)
{
    VERIFY(m_Desc.Usage == USAGE_STAGING, "Map working only for staging buffers");

    if (MapType == MAP_READ)
    {
        struct CallbackCaptureData
        {
            BufferWebGPUImpl* pBuffer;
            bool              IsMapped;
        } CallbackCapture{this, false};

        auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
            if (MapStatus == WGPUBufferMapAsyncStatus_Success)
            {
                auto*       pCaptureData = static_cast<CallbackCaptureData*>(pUserData);
                auto*       pBuffer      = pCaptureData->pBuffer;
                const auto* pData        = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(pBuffer->m_wgpuBuffer.Get(), 0, StaticCast<size_t>(pBuffer->m_Desc.Size)));
                VERIFY_EXPR(pUserData != nullptr);
                memcpy(pBuffer->m_MappedData.data(), pData, StaticCast<size_t>(pBuffer->m_Desc.Size));
                wgpuBufferUnmap(pBuffer->m_wgpuBuffer.Get());
                pCaptureData->IsMapped = true;
            }
            else
            {
                DEV_ERROR("Failed wgpuBufferMapAsync: ", MapStatus);
            }
        };

        wgpuBufferMapAsync(m_wgpuBuffer.Get(), WGPUMapMode_Read, 0, StaticCast<size_t>(m_Desc.Size), MapAsyncCallback, &CallbackCapture);
        while (!CallbackCapture.IsMapped)
            m_pDevice->PollEvents(true);

        pMappedData = m_MappedData.data();
    }
    else if (MapType == MAP_WRITE)
    {
        pMappedData = m_MappedData.data();
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

    if (MapType == MAP_READ)
    {
        // Nothing to do
    }
    else if (MapType == MAP_WRITE)
    {
        struct CallbackCaptureData
        {
            BufferWebGPUImpl* pBuffer;
            bool              IsMapped;
        } CallbackCapture{this, false};

        auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
            if (MapStatus == WGPUBufferMapAsyncStatus_Success)
            {
                auto* pCaptureData = static_cast<CallbackCaptureData*>(pUserData);
                auto* pBuffer      = pCaptureData->pBuffer;
                auto* pData        = static_cast<uint8_t*>(wgpuBufferGetMappedRange(pBuffer->m_wgpuBuffer.Get(), 0, StaticCast<size_t>(pBuffer->m_Desc.Size)));
                VERIFY_EXPR(pUserData != nullptr);
                memcpy(pData, pBuffer->m_MappedData.data(), StaticCast<size_t>(pBuffer->m_Desc.Size));
                wgpuBufferUnmap(pBuffer->m_wgpuBuffer.Get());
                pCaptureData->IsMapped = true;
            }
            else
            {
                DEV_ERROR("Failed wgpuBufferMapAsync: ", MapStatus);
            }
        };

        wgpuBufferMapAsync(m_wgpuBuffer.Get(), WGPUMapMode_Write, 0, StaticCast<size_t>(m_Desc.Size), MapAsyncCallback, &CallbackCapture);
        while (!CallbackCapture.IsMapped)
            m_pDevice->PollEvents(true);
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
            *ppView = NEW_RC_OBJ(BuffViewAllocator, "BufferViewWebGPUImpl instance", BufferViewWebGPUImpl, IsDefaultView ? this : nullptr)(pDeviceWebGPU, ViewDesc, this, IsDefaultView);

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
