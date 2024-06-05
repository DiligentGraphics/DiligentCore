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

#include "QueueSignalPoolWebGPU.hpp"
#include "RenderDeviceWebGPUImpl.hpp"

namespace Diligent
{

QueueSignalPoolWebGPU::QueueSignalPoolWebGPU(RenderDeviceWebGPUImpl* pDevice, Uint32 QueryCount) :
    m_QueryCount(QueryCount)
{
    m_QueryStatus.resize(QueryCount);

    WGPUBufferDescriptor wgpuQueryBufferDesc{};
    wgpuQueryBufferDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_QueryResolve;
    wgpuQueryBufferDesc.size  = sizeof(Uint64) * m_QueryCount;
    m_wgpuQueryBuffer.Reset(wgpuDeviceCreateBuffer(pDevice->GetWebGPUDevice(), &wgpuQueryBufferDesc));
    if (!m_wgpuQueryBuffer)
        LOG_ERROR_AND_THROW("Failed to create query buffer");

    WGPUBufferDescriptor wgpuStagingBufferDesc{};
    wgpuStagingBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    wgpuStagingBufferDesc.size  = sizeof(Uint64) * m_QueryCount;
    m_wgpuStagingBuffer.Reset(wgpuDeviceCreateBuffer(pDevice->GetWebGPUDevice(), &wgpuStagingBufferDesc));
    if (!m_wgpuStagingBuffer)
        LOG_ERROR_AND_THROW("Failed to create staging buffer");

    WGPUQuerySetDescriptor wgpuQuerySetDesc{};
    wgpuQuerySetDesc.type  = WGPUQueryType_Timestamp;
    wgpuQuerySetDesc.count = m_QueryCount;
    m_wgpuQuerySet.Reset(wgpuDeviceCreateQuerySet(pDevice->GetWebGPUDevice(), &wgpuQuerySetDesc));
    if (!m_wgpuQuerySet)
        LOG_ERROR_AND_THROW("Failed to create query set");

    const std::vector<Uint64> BufferFillZero(QueryCount, 0);
    wgpuQueueWriteBuffer(wgpuDeviceGetQueue(pDevice->GetWebGPUDevice()), m_wgpuStagingBuffer.Get(), 0, BufferFillZero.data(), BufferFillZero.size());
}

Uint32 QueueSignalPoolWebGPU::AllocateQuery()
{
    for (Uint32 QueryIdx = 0; QueryIdx < m_QueryStatus.size(); ++QueryIdx)
        if (!m_QueryStatus[QueryIdx])
            return QueryIdx;

    LOG_ERROR_MESSAGE("Failed to find available query. Increase QueryCount");
    return UINT32_MAX;
}

void QueueSignalPoolWebGPU::ReleaseQuery(Uint32 QueryIdx)
{
    DEV_CHECK_ERR(QueryIdx < m_QueryCount, "Query index should be less than the size of the query set");
    m_QueryStatus[QueryIdx] = false;
}

void QueueSignalPoolWebGPU::WriteTimestamp(WGPUCommandEncoder wgpuCmdEncoder, Uint32 QueryIdx)
{
    DEV_CHECK_ERR(QueryIdx < m_QueryCount, "Query index should be less than the size of the query set");
    wgpuCommandEncoderWriteTimestamp(wgpuCmdEncoder, m_wgpuQuerySet.Get(), QueryIdx);
}

void QueueSignalPoolWebGPU::ResolveQuery(WGPUCommandEncoder wgpuCmdEncoder, Uint32 QueryIdx)
{
    const Uint64 CopyOffset = QueryIdx * sizeof(Uint64);
    wgpuCommandEncoderResolveQuerySet(wgpuCmdEncoder, m_wgpuQuerySet.Get(), QueryIdx, 1, m_wgpuQueryBuffer.Get(), CopyOffset);
    wgpuCommandEncoderCopyBufferToBuffer(wgpuCmdEncoder, m_wgpuQueryBuffer.Get(), CopyOffset, m_wgpuStagingBuffer.Get(), CopyOffset, sizeof(Uint64));
}

Uint64 QueueSignalPoolWebGPU::GetQueryTimestamp(WGPUDevice wgpuDevice, Uint32 QueryIdx)
{
    DEV_CHECK_ERR(QueryIdx < m_QueryCount, "Query index should be less than the size of the query set");

    struct CallbackCaptureData
    {
        QueueSignalPoolWebGPU* pQueueSignalPool;
        Uint64                 QueryTimestamp;
        Uint32                 QueryIdx;
    } CallbackCapture{this, 0, QueryIdx};

    auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
        if (MapStatus == WGPUBufferMapAsyncStatus_Success)
        {
            auto* pCaptureData    = static_cast<CallbackCaptureData*>(pUserData);
            auto* pQueueSignalSet = pCaptureData->pQueueSignalPool;

            const auto BufferOffset = pCaptureData->QueryIdx * sizeof(Uint64);

            const auto* pQueryData = static_cast<const uint64_t*>(wgpuBufferGetConstMappedRange(pQueueSignalSet->m_wgpuStagingBuffer.Get(), BufferOffset, sizeof(Uint64)));
            VERIFY_EXPR(pUserData != nullptr);
            pCaptureData->QueryTimestamp = *pQueryData;
            wgpuBufferUnmap(pQueueSignalSet->m_wgpuStagingBuffer.Get());
        }
        else
        {
            DEV_ERROR("Failed wgpuBufferMapAsync: ", MapStatus);
        }
    };

    wgpuBufferMapAsync(m_wgpuStagingBuffer.Get(), WGPUMapMode_Read, QueryIdx * sizeof(Uint64), sizeof(Uint64), MapAsyncCallback, &CallbackCapture);
#if !PLATFORM_EMSCRIPTEN
    wgpuQueueSubmit(wgpuDeviceGetQueue(wgpuDevice), 0, nullptr);
#endif
    return CallbackCapture.QueryTimestamp;
}

} // namespace Diligent
