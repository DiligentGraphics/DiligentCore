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

namespace
{

static Uint32 GetQueryDataSize(QUERY_TYPE QueryType)
{
    static_assert(QUERY_TYPE_NUM_TYPES == 6, "Not all QUERY_TYPE enum values are tested");

    // clang-format off
    switch (QueryType)
    {
        case QUERY_TYPE_OCCLUSION:
        case QUERY_TYPE_BINARY_OCCLUSION:
        case QUERY_TYPE_TIMESTAMP:
        case QUERY_TYPE_DURATION:
            return sizeof(Uint64);
        case QUERY_TYPE_PIPELINE_STATISTICS:
            UNEXPECTED("Pipeline statistics queries aren't supported in WebGPU");
            return 0;
        default:
            UNEXPECTED("Unexpected query type");
            return 0;
    }
    // clang-format on
}

} // namespace

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
        const auto QueryType = static_cast<QUERY_TYPE>(QueryTypeIdx);

        // clang-format off
        if ((QueryType == QUERY_TYPE_OCCLUSION           && !DevInfo.Features.OcclusionQueries)           ||
            (QueryType == QUERY_TYPE_BINARY_OCCLUSION    && !DevInfo.Features.BinaryOcclusionQueries)     ||
            (QueryType == QUERY_TYPE_TIMESTAMP           && !DevInfo.Features.TimestampQueries)           ||
            (QueryType == QUERY_TYPE_PIPELINE_STATISTICS && !DevInfo.Features.PipelineStatisticsQueries)  ||
            (QueryType == QUERY_TYPE_DURATION            && !DevInfo.Features.DurationQueries))
            continue;
        // clang-format on

        String QuerySetName = String{"QueryManagerWebGPU: Query set ["} + GetQueryTypeString(QueryType) + "]";

        WGPUQuerySetDescriptor wgpuQuerySetDesc{};
        wgpuQuerySetDesc.type  = QueryTypeToWGPUQueryType(QueryType);
        wgpuQuerySetDesc.count = QueryHeapSizes[QueryType];
        wgpuQuerySetDesc.label = QuerySetName.c_str();

        if (QueryType == QUERY_TYPE_DURATION)
            wgpuQuerySetDesc.count *= 2;

        auto& QuerySetInfo = m_QuerySets[QueryType];
        QuerySetInfo.Initialize(pRenderDeviceWebGPU->GetWebGPUDevice(), wgpuQuerySetDesc, QueryType);
        VERIFY_EXPR(!QuerySetInfo.IsNull() && QuerySetInfo.GetQueryCount() == wgpuQuerySetDesc.count && QuerySetInfo.GetType() == QueryType);
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

void QueryManagerWebGPU::QuerySetInfo::Initialize(WGPUDevice wgpuDevice, const WGPUQuerySetDescriptor& wgpuQuerySetDesc, QUERY_TYPE Type)
{
    m_Type       = Type;
    m_QueryCount = wgpuQuerySetDesc.count;
    m_wgpuQuerySet.Reset(wgpuDeviceCreateQuerySet(wgpuDevice, &wgpuQuerySetDesc));
    if (!m_wgpuQuerySet)
        LOG_ERROR_AND_THROW("Failed to create '", wgpuQuerySetDesc.label, "'");

    WGPUBufferDescriptor wgpuResolveBufferDesc{};
    wgpuResolveBufferDesc.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
    wgpuResolveBufferDesc.size  = GetQueryDataSize(Type);
    m_wgpuResolveBuffer.Reset(wgpuDeviceCreateBuffer(wgpuDevice, &wgpuResolveBufferDesc));
    if (!m_wgpuResolveBuffer)
        LOG_ERROR_AND_THROW("Failed to create resolve buffer for '", wgpuQuerySetDesc.label, "'");

    WGPUBufferDescriptor wgpuStagingBufferDesc{};
    wgpuStagingBufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    wgpuStagingBufferDesc.size  = GetQueryDataSize(Type);
    m_wgpuStagingBuffer.Reset(wgpuDeviceCreateBuffer(wgpuDevice, &wgpuStagingBufferDesc));
    if (!m_wgpuResolveBuffer)
        LOG_ERROR_AND_THROW("Failed to create staging buffer for '", wgpuQuerySetDesc.label, "'");
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

} // namespace Diligent
