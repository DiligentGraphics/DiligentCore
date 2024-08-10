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

#include "QueryManagerWebGPU.hpp"
#include "WebGPUTypeConversions.hpp"

namespace Diligent
{

QueryManagerWebGPU::QueryManagerWebGPU(RenderDeviceWebGPUImpl* pRenderDeviceWebGPU, const Uint32 QueryHeapSizes[])
{
    const auto& DevInfo = pRenderDeviceWebGPU->GetDeviceInfo();

    // clang-format off
    static_assert(QUERY_TYPE_OCCLUSION          == 1, "Unexpected value of QUERY_TYPE_OCCLUSION. EngineWebGPUCreateInfo::QueryPoolSizes must be updated");
    static_assert(QUERY_TYPE_BINARY_OCCLUSION   == 2, "Unexpected value of QUERY_TYPE_BINARY_OCCLUSION. EngineWebGPUCreateInfo::QueryPoolSizes must be updated");
    static_assert(QUERY_TYPE_TIMESTAMP          == 3, "Unexpected value of QUERY_TYPE_TIMESTAMP. EngineWebGPUCreateInfo::QueryPoolSizes must be updated");
    static_assert(QUERY_TYPE_PIPELINE_STATISTICS== 4, "Unexpected value of QUERY_TYPE_PIPELINE_STATISTICS. EngineWebGPUCreateInfo::QueryPoolSizes must be updated");
    static_assert(QUERY_TYPE_DURATION           == 5, "Unexpected value of QUERY_TYPE_DURATION. EngineWebGPUCreateInfo::QueryPoolSizes must be updated");
    static_assert(QUERY_TYPE_NUM_TYPES          == 6, "Unexpected value of QUERY_TYPE_NUM_TYPES. EngineWebGPUCreateInfo::QueryPoolSizes must be updated");
    // clang-format on

    for (Uint32 QueryTypeIdx = QUERY_TYPE_UNDEFINED + 1; QueryTypeIdx < QUERY_TYPE_NUM_TYPES; ++QueryTypeIdx)
    {
        m_PendingReadbackIndices[QueryTypeIdx] = QueryManagerWebGPU::InvalidIndex;

        const auto QueryType = static_cast<QUERY_TYPE>(QueryTypeIdx);

        // clang-format off
        if ((QueryType == QUERY_TYPE_OCCLUSION           && !DevInfo.Features.OcclusionQueries)           ||
            (QueryType == QUERY_TYPE_BINARY_OCCLUSION    && !DevInfo.Features.BinaryOcclusionQueries)     ||
            (QueryType == QUERY_TYPE_TIMESTAMP           && !DevInfo.Features.TimestampQueries)           ||
            (QueryType == QUERY_TYPE_PIPELINE_STATISTICS && !DevInfo.Features.PipelineStatisticsQueries)  ||
            (QueryType == QUERY_TYPE_DURATION            && !DevInfo.Features.DurationQueries))
            continue;
        // clang-format on

        auto& QuerySetInfo = m_QuerySets[QueryType];
        QuerySetInfo.Initialize(pRenderDeviceWebGPU, QueryHeapSizes[QueryType], QueryType);
        VERIFY_EXPR(!QuerySetInfo.IsNull() && QuerySetInfo.GetType() == QueryType);
    }
}

QueryManagerWebGPU::~QueryManagerWebGPU()
{
    std::stringstream QueryUsageSS;
    QueryUsageSS << "WebGPU query manager peak usage:";
    for (Uint32 QueryType = QUERY_TYPE_UNDEFINED + 1; QueryType < QUERY_TYPE_NUM_TYPES; ++QueryType)
    {
        auto& QuerySetInfo = m_QuerySets[QueryType];
        if (QuerySetInfo.IsNull())
            continue;

        QueryUsageSS << std::endl
                     << std::setw(30) << std::left << GetQueryTypeString(static_cast<QUERY_TYPE>(QueryType)) << ": "
                     << std::setw(4) << std::right << QuerySetInfo.GetMaxAllocatedQueries()
                     << '/' << std::setw(4) << QuerySetInfo.GetQueryCount();
    }
    LOG_INFO_MESSAGE(QueryUsageSS.str());
}

Uint32 QueryManagerWebGPU::AllocateQuery(QUERY_TYPE Type)
{
    return m_QuerySets[Type].Allocate();
}

void QueryManagerWebGPU::ReleaseQuery(QUERY_TYPE Type, Uint32 Index)
{
    return m_QuerySets[Type].Release(Index);
}

WGPUQuerySet QueryManagerWebGPU::GetQuerySet(QUERY_TYPE Type) const
{
    return m_QuerySets[Type].GetWebGPUQuerySet();
}

Uint32 QueryManagerWebGPU::GetReadbackBufferIdentifier(QUERY_TYPE Type, Uint64 EventValue) const
{
    return m_QuerySets[Type].GetReadbackBufferIdentifier(EventValue);
}

Uint64 QueryManagerWebGPU::GetQueryResult(QUERY_TYPE Type, Uint32 Index, Uint32 BufferIdentifier) const
{
    return m_QuerySets[Type].GetQueryResult(Index, BufferIdentifier);
}

Uint64 QueryManagerWebGPU::GetNextEventValue(QUERY_TYPE Type)
{
    return m_QuerySets[Type].GetNextEventValue();
}

void QueryManagerWebGPU::ResolveQuerySet(RenderDeviceWebGPUImpl* pDevice, WGPUCommandEncoder wgpuCmdEncoder)
{
    for (Uint32 QueryType = QUERY_TYPE_UNDEFINED + 1; QueryType < QUERY_TYPE_NUM_TYPES; ++QueryType)
    {
        auto& QuerySetInfo                  = m_QuerySets[QueryType];
        m_PendingReadbackIndices[QueryType] = QuerySetInfo.ResolveQueries(pDevice, wgpuCmdEncoder);
    }
}

void QueryManagerWebGPU::ReadbackQuerySet(RenderDeviceWebGPUImpl* pDevice)
{
    for (Uint32 QueryType = QUERY_TYPE_UNDEFINED + 1; QueryType < QUERY_TYPE_NUM_TYPES; ++QueryType)
    {
        auto& QuerySetInfo     = m_QuerySets[QueryType];
        auto  RedbackBufferIdx = m_PendingReadbackIndices[QueryType];
        if (RedbackBufferIdx != QueryManagerWebGPU::InvalidIndex)
        {
            QuerySetInfo.ReadbackQueries(pDevice, RedbackBufferIdx);
            QuerySetInfo.IncrementEventValue();
            m_ActiveQuerySets++;
        }
    }
}

void QueryManagerWebGPU::FinishFrame()
{
    m_ActiveQuerySets = 0;
}

void QueryManagerWebGPU::WaitAllQuerySet(RenderDeviceWebGPUImpl* pDevice)
{
    for (Uint32 QueryType = QUERY_TYPE_UNDEFINED + 1; QueryType < QUERY_TYPE_NUM_TYPES; ++QueryType)
    {
        auto& QuerySetInfo     = m_QuerySets[QueryType];
        auto  RedbackBufferIdx = m_PendingReadbackIndices[QueryType];
        if (RedbackBufferIdx != QueryManagerWebGPU::InvalidIndex)
            QuerySetInfo.WaitAllQueries(pDevice, RedbackBufferIdx);
    }
}

QueryManagerWebGPU::QuerySetInfo::~QuerySetInfo()
{
    if (m_AvailableQueries.size() != m_QueryCount)
    {
        const auto OutstandingQueries = m_QueryCount - m_AvailableQueries.size();
        if (OutstandingQueries == 1)
        {
            LOG_ERROR_MESSAGE("One query of type ", GetQueryTypeString(m_Type),
                              " has not been returned to the query manager");
        }
        else
        {
            LOG_ERROR_MESSAGE(OutstandingQueries, " queries of type ", GetQueryTypeString(m_Type),
                              " have not been returned to the query manager");
        }
    }
}

void QueryManagerWebGPU::QuerySetInfo::Initialize(RenderDeviceWebGPUImpl* pDevice, Uint32 HeapSize, QUERY_TYPE QueryType)
{
    String QuerySetName = String{"QueryManagerWebGPU: Query set ["} + GetQueryTypeString(QueryType) + "]";

    WGPUQuerySetDescriptor wgpuQuerySetDesc{};
    wgpuQuerySetDesc.type  = QueryTypeToWGPUQueryType(QueryType);
    wgpuQuerySetDesc.count = HeapSize;
    wgpuQuerySetDesc.label = QuerySetName.c_str();

    if (QueryType == QUERY_TYPE_DURATION)
        wgpuQuerySetDesc.count *= 2;

    m_Type       = QueryType;
    m_QueryCount = wgpuQuerySetDesc.count;
    m_wgpuQuerySet.Reset(wgpuDeviceCreateQuerySet(pDevice->GetWebGPUDevice(), &wgpuQuerySetDesc));
    if (!m_wgpuQuerySet)
        LOG_ERROR_AND_THROW("Failed to create '", wgpuQuerySetDesc.label, "'");

    String QueryResolveBufferName = String{"QueryManagerWebGPU: Query resolve buffer ["} + GetQueryTypeString(QueryType) + "]";

    WGPUBufferDescriptor wgpuResolveBufferDesc{};
    wgpuResolveBufferDesc.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
    wgpuResolveBufferDesc.size  = static_cast<Uint64>(m_QueryCount) * sizeof(Uint64);
    wgpuResolveBufferDesc.label = QueryResolveBufferName.c_str();
    m_wgpuResolveBuffer.Reset(wgpuDeviceCreateBuffer(pDevice->GetWebGPUDevice(), &wgpuResolveBufferDesc));
    if (!m_wgpuResolveBuffer)
        LOG_ERROR_AND_THROW("Failed to create resolve buffer for '", wgpuQuerySetDesc.label, "'");

    m_PendingReadbackBuffers.reserve(MaxPendingBuffers);
    m_AvailableQueries.resize(m_QueryCount);
    for (Uint32 QueryIdx = 0; QueryIdx < m_QueryCount; ++QueryIdx)
        m_AvailableQueries[QueryIdx] = QueryIdx;
}

Uint32 QueryManagerWebGPU::QuerySetInfo::Allocate()
{
    Uint32 Index = InvalidIndex;

    if (!m_AvailableQueries.empty())
    {
        Index = m_AvailableQueries.back();
        m_AvailableQueries.pop_back();
        m_MaxAllocatedQueries = std::max(m_MaxAllocatedQueries, m_QueryCount - static_cast<Uint32>(m_AvailableQueries.size()));
    }
    return Index;
}

void QueryManagerWebGPU::QuerySetInfo::Release(Uint32 Index)
{
    VERIFY(Index < m_QueryCount, "Query index ", Index, " is out of range");
    VERIFY(std::find(m_AvailableQueries.begin(), m_AvailableQueries.end(), Index) == m_AvailableQueries.end(),
           "Index ", Index, " already present in available queries list");
    m_AvailableQueries.push_back(Index);
}

QUERY_TYPE QueryManagerWebGPU::QuerySetInfo::GetType() const
{
    return m_Type;
}

Uint32 QueryManagerWebGPU::QuerySetInfo::GetQueryCount() const
{
    return m_QueryCount;
}

Uint64 QueryManagerWebGPU::QuerySetInfo::GetQueryResult(Uint32 Index, Uint32 BufferIdentifier) const
{
    return m_PendingReadbackBuffers[BufferIdentifier].DataResult[Index];
}

WGPUQuerySet QueryManagerWebGPU::QuerySetInfo::GetWebGPUQuerySet() const
{
    return m_wgpuQuerySet.Get();
}

Uint32 QueryManagerWebGPU::QuerySetInfo::GetMaxAllocatedQueries() const
{
    return m_MaxAllocatedQueries;
}

bool QueryManagerWebGPU::QuerySetInfo::IsNull() const
{
    return m_wgpuQuerySet.Get() == nullptr;
}

Uint32 QueryManagerWebGPU::QuerySetInfo::ResolveQueries(RenderDeviceWebGPUImpl* pDevice, WGPUCommandEncoder wgpuCmdEncoder)
{
    if (m_AvailableQueries.size() != m_QueryCount && !IsNull())
    {
        auto& ReadbackBuffer = FindAvailableReadbackBuffer(pDevice);
        wgpuCommandEncoderResolveQuerySet(wgpuCmdEncoder, m_wgpuQuerySet, 0, m_QueryCount, m_wgpuResolveBuffer, 0);
        wgpuCommandEncoderCopyBufferToBuffer(wgpuCmdEncoder, m_wgpuResolveBuffer, 0, ReadbackBuffer.ReadbackBuffer, 0, m_QueryCount * sizeof(Uint64));
        return ReadbackBuffer.BufferIdentifier;
    }
    return QueryManagerWebGPU::InvalidIndex;
}

void QueryManagerWebGPU::QuerySetInfo::ReadbackQueries(RenderDeviceWebGPUImpl* pDevice, Uint32 PendingRedbackIndex)
{
    auto* pReadbackInfo = &m_PendingReadbackBuffers[PendingRedbackIndex];

    auto MapAsyncCallback = [](WGPUBufferMapAsyncStatus MapStatus, void* pUserData) {
        if (MapStatus != WGPUBufferMapAsyncStatus_Success && MapStatus != WGPUBufferMapAsyncStatus_DestroyedBeforeCallback)
            DEV_ERROR("Failed wgpuBufferMapAsync: ", MapStatus);

        if (MapStatus == WGPUBufferMapAsyncStatus_Success && pUserData != nullptr)
        {
            auto* pReadBackInfo = static_cast<QuerySetInfo::ReadbackBufferInfo*>(pUserData);

            const auto* pData = static_cast<const uint64_t*>(wgpuBufferGetConstMappedRange(pReadBackInfo->ReadbackBuffer, 0, WGPU_WHOLE_MAP_SIZE));
            VERIFY_EXPR(pData != nullptr);
            pReadBackInfo->DataResult.assign(pData, pData + pReadBackInfo->DataResult.size());
            pReadBackInfo->LastEventValue = pReadBackInfo->PendingEventValue;
            wgpuBufferUnmap(pReadBackInfo->ReadbackBuffer);
        }
    };

    pReadbackInfo->PendingEventValue = GetNextEventValue();
    wgpuBufferMapAsync(pReadbackInfo->ReadbackBuffer, WGPUMapMode_Read, 0, WGPU_WHOLE_MAP_SIZE, MapAsyncCallback, pReadbackInfo);
}

void QueryManagerWebGPU::QuerySetInfo::WaitAllQueries(RenderDeviceWebGPUImpl* pDevice, Uint32 PendingRedbackIndex)
{
    const auto& ReabackBufferInfo = m_PendingReadbackBuffers[PendingRedbackIndex];
    while (ReabackBufferInfo.PendingEventValue != ReabackBufferInfo.LastEventValue)
        pDevice->DeviceTick();
}

QueryManagerWebGPU::QuerySetInfo::ReadbackBufferInfo& QueryManagerWebGPU::QuerySetInfo::FindAvailableReadbackBuffer(RenderDeviceWebGPUImpl* pDevice)
{
    for (auto& ReadbackBuffer : m_PendingReadbackBuffers)
    {
        WGPUBufferMapState MapState = wgpuBufferGetMapState(ReadbackBuffer.ReadbackBuffer.Get());
        if (MapState == WGPUBufferMapState_Unmapped && ReadbackBuffer.PendingEventValue == ReadbackBuffer.LastEventValue)
            return ReadbackBuffer;
    }

    String QueryReadbackBufferName = String{"QueryManagerWebGPU: Query readback buffer ["} + GetQueryTypeString(m_Type) + "]";

    ReadbackBufferInfo Result{};

    WGPUBufferDescriptor wgpuReadbackBufferDesc{};
    wgpuReadbackBufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    wgpuReadbackBufferDesc.size  = m_QueryCount * sizeof(Uint64);
    wgpuReadbackBufferDesc.label = QueryReadbackBufferName.c_str();

    Result.ReadbackBuffer.Reset(wgpuDeviceCreateBuffer(pDevice->GetWebGPUDevice(), &wgpuReadbackBufferDesc));
    Result.DataResult.resize(m_QueryCount);
    Result.BufferIdentifier  = static_cast<Uint32>(m_PendingReadbackBuffers.size());
    Result.LastEventValue    = 0;
    Result.PendingEventValue = 0;

    if (!Result.ReadbackBuffer)
        LOG_ERROR_AND_THROW("Failed to create readback buffer '", wgpuReadbackBufferDesc.label, "'");

    m_PendingReadbackBuffers.emplace_back(std::move(Result));
    VERIFY_EXPR(m_PendingReadbackBuffers.capacity() <= MaxPendingBuffers);
    return m_PendingReadbackBuffers.back();
}

Uint32 QueryManagerWebGPU::QuerySetInfo::GetReadbackBufferIdentifier(Uint64 EventValue) const
{
    for (const auto& ReadbackBuffer : m_PendingReadbackBuffers)
        if (ReadbackBuffer.LastEventValue >= EventValue)
            return ReadbackBuffer.BufferIdentifier;
    return QueryManagerWebGPU::InvalidIndex;
}

Uint64 QueryManagerWebGPU::QuerySetInfo::GetNextEventValue()
{
    return m_EventValue + 1;
}

Uint64 QueryManagerWebGPU::QuerySetInfo::IncrementEventValue()
{
    Uint64 LastEventValue = m_EventValue;
    m_EventValue++;
    return LastEventValue;
}

} // namespace Diligent
