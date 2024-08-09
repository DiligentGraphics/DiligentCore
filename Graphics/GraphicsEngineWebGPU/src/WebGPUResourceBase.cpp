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

#include "WebGPUResourceBase.hpp"

namespace Diligent
{

WebGPUResourceBase::WebGPUResourceBase(IReferenceCounters* pRefCounters, size_t MaxPendingBuffers) :
    m_pRefCounters{pRefCounters}
{
    m_StagingBuffers.reserve(MaxPendingBuffers);
}

WebGPUResourceBase::~WebGPUResourceBase()
{
}

WebGPUResourceBase::StagingBufferInfo* WebGPUResourceBase::FindStagingWriteBuffer(WGPUDevice wgpuDevice, const char* ResourceName)
{
    if (m_StagingBuffers.empty())
    {
        String StagingBufferName = "Staging write buffer for '";
        StagingBufferName += ResourceName;
        StagingBufferName += '\'';

        WGPUBufferDescriptor wgpuBufferDesc{};
        wgpuBufferDesc.label            = StagingBufferName.c_str();
        wgpuBufferDesc.size             = m_MappedData.size();
        wgpuBufferDesc.usage            = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
        wgpuBufferDesc.mappedAtCreation = true;

        WebGPUBufferWrapper wgpuBuffer{wgpuDeviceCreateBuffer(wgpuDevice, &wgpuBufferDesc)};
        if (!wgpuBuffer)
        {
            LOG_ERROR("Failed to create WebGPU buffer '", StagingBufferName, '\'');
            return nullptr;
        }

        m_StagingBuffers.emplace_back(StagingBufferInfo{
            *this,
            std::move(wgpuBuffer),
        });
    }

    return &m_StagingBuffers.back();
}

WebGPUResourceBase::StagingBufferInfo* WebGPUResourceBase::FindStagingReadBuffer(WGPUDevice wgpuDevice, const char* ResourceName)
{
    for (StagingBufferInfo& BufferInfo : m_StagingBuffers)
    {
        if (wgpuBufferGetMapState(BufferInfo.wgpuBuffer) == WGPUBufferMapState_Unmapped)
        {
            BufferInfo.pSyncPoint->Reset();
            return &BufferInfo;
        }
    }

    String StagingBufferName = "Staging read buffer for '";
    StagingBufferName += ResourceName;
    StagingBufferName += '\'';

    WGPUBufferDescriptor wgpuBufferDesc{};
    wgpuBufferDesc.label = StagingBufferName.c_str();
    wgpuBufferDesc.size  = StaticCast<Uint64>(m_MappedData.size());
    wgpuBufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;

    WebGPUBufferWrapper wgpuBuffer{wgpuDeviceCreateBuffer(wgpuDevice, &wgpuBufferDesc)};
    if (!wgpuBuffer)
    {
        LOG_ERROR("Failed to create WebGPU buffer '", StagingBufferName, '\'');
        return nullptr;
    }

    if (m_StagingBuffers.size() == m_StagingBuffers.capacity())
    {
        LOG_ERROR("Too many pending staging buffers.");
        return nullptr;
    }

    m_StagingBuffers.emplace_back(
        StagingBufferInfo{
            *this,
            std::move(wgpuBuffer),
            RefCntAutoPtr<SyncPointWebGPUImpl>{MakeNewRCObj<SyncPointWebGPUImpl>()()},
        });
    return &m_StagingBuffers.back();
}


WebGPUResourceBase::StagingBufferInfo* WebGPUResourceBase::GetStagingBufferInfo(WGPUDevice wgpuDevice, const char* ResourceName, CPU_ACCESS_FLAGS Access)
{
    VERIFY(Access == CPU_ACCESS_READ || Access == CPU_ACCESS_WRITE, "Read or write access is expected");
    return Access == CPU_ACCESS_READ ?
        FindStagingReadBuffer(wgpuDevice, ResourceName) :
        FindStagingWriteBuffer(wgpuDevice, ResourceName);
}

void* WebGPUResourceBase::Map(MAP_TYPE MapType, Uint64 Offset)
{
    VERIFY(m_MapState == MapState::None, "Texture is already mapped");

    if (MapType == MAP_READ)
    {
        m_MapState = MapState::Read;
        return m_MappedData.data() + Offset;
    }
    else if (MapType == MAP_WRITE)
    {
        m_MapState = MapState::Write;
        return m_MappedData.data() + Offset;
    }
    else if (MapType == MAP_READ_WRITE)
    {
        LOG_ERROR("MAP_READ_WRITE is not supported in WebGPU backend");
    }
    else
    {
        UNEXPECTED("Unknown map type");
    }

    return nullptr;
}

void WebGPUResourceBase::Unmap()
{
    VERIFY(m_MapState != MapState::None, "Texture is not mapped");

    if (m_MapState == MapState::Read || m_MapState == MapState::Write)
    {
        // Nothing to do
    }
    else
    {
        UNEXPECTED("No matching call to Map()");
    }

    m_MapState = MapState::None;
}

void WebGPUResourceBase::FlushPendingWrites(StagingBufferInfo& Buffer)
{
    VERIFY_EXPR(!Buffer.pSyncPoint);
    VERIFY_EXPR(m_StagingBuffers.size() == 1);

    void* pData = wgpuBufferGetMappedRange(Buffer.wgpuBuffer, 0, WGPU_WHOLE_MAP_SIZE);
    memcpy(pData, m_MappedData.data(), m_MappedData.size());
    wgpuBufferUnmap(Buffer.wgpuBuffer);

    m_StagingBuffers.clear();
}

void WebGPUResourceBase::ProcessAsyncReadback(StagingBufferInfo& Buffer)
{
    auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
        if (MapStatus != WGPUBufferMapAsyncStatus_Success && MapStatus != WGPUBufferMapAsyncStatus_DestroyedBeforeCallback)
            DEV_ERROR("Failed wgpuBufferMapAsync: ", MapStatus);

        if (MapStatus == WGPUBufferMapAsyncStatus_Success && pUserData != nullptr)
        {
            StagingBufferInfo* pBufferInfo = static_cast<StagingBufferInfo*>(pUserData);

            const auto* pData = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(pBufferInfo->wgpuBuffer, 0, WGPU_WHOLE_MAP_SIZE));
            VERIFY_EXPR(pData != nullptr);
            memcpy(pBufferInfo->Resource.m_MappedData.data(), pData, pBufferInfo->Resource.m_MappedData.size());
            wgpuBufferUnmap(pBufferInfo->wgpuBuffer.Get());
            pBufferInfo->pSyncPoint->Trigger();

            // Release the reference to the resource
            pBufferInfo->Resource.m_pRefCounters->ReleaseStrongRef();
        }
    };

    // Keep the resource alive until the callback is called
    m_pRefCounters->AddStrongRef();
    wgpuBufferMapAsync(Buffer.wgpuBuffer, WGPUMapMode_Read, 0, WGPU_WHOLE_MAP_SIZE, MapAsyncCallback, &Buffer);
}

} // namespace Diligent
