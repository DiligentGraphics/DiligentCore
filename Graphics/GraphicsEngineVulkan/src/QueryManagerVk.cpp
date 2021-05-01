/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "QueryManagerVk.hpp"

#include <algorithm>

#include "RenderDeviceVkImpl.hpp"
#include "GraphicsAccessories.hpp"
#include "VulkanUtilities/VulkanCommandBuffer.hpp"

namespace Diligent
{

QueryManagerVk::QueryManagerVk(RenderDeviceVkImpl*     pRenderDeviceVk,
                               const Uint32            QueryHeapSizes[],
                               const CommandQueueIndex CmdQueueInd)
{
    const auto& LogicalDevice  = pRenderDeviceVk->GetLogicalDevice();
    const auto& PhysicalDevice = pRenderDeviceVk->GetPhysicalDevice();

    auto timestampPeriod = PhysicalDevice.GetProperties().limits.timestampPeriod;
    m_CounterFrequency   = static_cast<Uint64>(1000000000.0 / timestampPeriod);

    const auto  QueueFamilyIndex = HardwareQueueId{pRenderDeviceVk->GetCommandQueue(CmdQueueInd).GetQueueFamilyIndex()};
    const auto& EnabledFeatures  = LogicalDevice.GetEnabledFeatures();
    const auto  StageMask        = LogicalDevice.GetSupportedStagesMask(QueueFamilyIndex);
    const auto  QueueFlags       = PhysicalDevice.GetQueueProperties()[QueueFamilyIndex].queueFlags;

    // Queries supported only in graphics or compute queues.
    if ((QueueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0)
        return;

    VulkanUtilities::CommandPoolWrapper CmdPool;
    VkCommandBuffer                     vkCmdBuff;
    pRenderDeviceVk->AllocateTransientCmdPool(CmdQueueInd, CmdPool, vkCmdBuff, "Transient command pool to reset queries before first use");

    for (Uint32 QueryType = QUERY_TYPE_UNDEFINED + 1; QueryType < QUERY_TYPE_NUM_TYPES; ++QueryType)
    {
        if ((QueryType == QUERY_TYPE_OCCLUSION && !EnabledFeatures.occlusionQueryPrecise) ||
            (QueryType == QUERY_TYPE_PIPELINE_STATISTICS && !EnabledFeatures.pipelineStatisticsQuery))
            continue;

        // Compute queue supports only time queries
        if ((QueryType != QUERY_TYPE_TIMESTAMP && QueryType != QUERY_TYPE_DURATION) && (QueueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            continue;

        // clang-format off
        static_assert(QUERY_TYPE_OCCLUSION          == 1, "Unexpected value of QUERY_TYPE_OCCLUSION. EngineVkCreateInfo::QueryPoolSizes must be updated");
        static_assert(QUERY_TYPE_BINARY_OCCLUSION   == 2, "Unexpected value of QUERY_TYPE_BINARY_OCCLUSION. EngineVkCreateInfo::QueryPoolSizes must be updated");
        static_assert(QUERY_TYPE_TIMESTAMP          == 3, "Unexpected value of QUERY_TYPE_TIMESTAMP. EngineVkCreateInfo::QueryPoolSizes must be updated");
        static_assert(QUERY_TYPE_PIPELINE_STATISTICS== 4, "Unexpected value of QUERY_TYPE_PIPELINE_STATISTICS. EngineVkCreateInfo::QueryPoolSizes must be updated");
        static_assert(QUERY_TYPE_DURATION           == 5, "Unexpected value of QUERY_TYPE_DURATION. EngineVkCreateInfo::QueryPoolSizes must be updated");
        static_assert(QUERY_TYPE_NUM_TYPES          == 6, "Unexpected value of QUERY_TYPE_NUM_TYPES. EngineVkCreateInfo::QueryPoolSizes must be updated");
        // clang-format on

        auto& HeapInfo    = m_Heaps[QueryType];
        HeapInfo.PoolSize = QueryHeapSizes[QueryType];

        VkQueryPoolCreateInfo QueryPoolCI = {};

        QueryPoolCI.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        QueryPoolCI.pNext = nullptr;
        QueryPoolCI.flags = 0;
        switch (QueryType)
        {
            case QUERY_TYPE_OCCLUSION:
            case QUERY_TYPE_BINARY_OCCLUSION:
                QueryPoolCI.queryType = VK_QUERY_TYPE_OCCLUSION;
                break;

            case QUERY_TYPE_TIMESTAMP:
            case QUERY_TYPE_DURATION:
                QueryPoolCI.queryType = VK_QUERY_TYPE_TIMESTAMP;
                break;

            case QUERY_TYPE_PIPELINE_STATISTICS:
            {
                QueryPoolCI.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
                QueryPoolCI.pipelineStatistics =
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

                if (StageMask & VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT)
                {
                    QueryPoolCI.pipelineStatistics |=
                        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
                        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT;
                }
                if (StageMask & VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT)
                    QueryPoolCI.pipelineStatistics |= VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT;
                if (StageMask & VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT)
                    QueryPoolCI.pipelineStatistics |= VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT;
            }
            break;

            default:
                UNEXPECTED("Unexpected query type");
        }

        QueryPoolCI.queryCount = HeapInfo.PoolSize;
        if (QueryType == QUERY_TYPE_DURATION)
            QueryPoolCI.queryCount *= 2;

        HeapInfo.vkQueryPool = LogicalDevice.CreateQueryPool(QueryPoolCI, "QueryManagerVk: query pool");

        // After query pool creation, each query must be reset before it is used.
        // Queries must also be reset between uses (17.2).
        vkCmdResetQueryPool(vkCmdBuff, HeapInfo.vkQueryPool, 0, QueryPoolCI.queryCount);

        HeapInfo.AvailableQueries.resize(HeapInfo.PoolSize);
        for (Uint32 i = 0; i < HeapInfo.PoolSize; ++i)
        {
            HeapInfo.AvailableQueries[i] = i;
        }
    }

    pRenderDeviceVk->ExecuteAndDisposeTransientCmdBuff(CmdQueueInd, vkCmdBuff, std::move(CmdPool));
}

QueryManagerVk::~QueryManagerVk()
{
    std::stringstream QueryUsageSS;
    QueryUsageSS << "Vulkan query manager peak usage:";
    for (Uint32 QueryType = QUERY_TYPE_UNDEFINED + 1; QueryType < QUERY_TYPE_NUM_TYPES; ++QueryType)
    {
        auto& HeapInfo = m_Heaps[QueryType];

        auto OutstandingQueries = HeapInfo.PoolSize - (HeapInfo.AvailableQueries.size() + HeapInfo.StaleQueries.size());
        if (OutstandingQueries != 0)
        {
            if (OutstandingQueries == 1)
            {
                LOG_ERROR_MESSAGE("One query of type ", GetQueryTypeString(static_cast<QUERY_TYPE>(QueryType)),
                                  " has not been returned to the query manager");
            }
            else
            {
                LOG_ERROR_MESSAGE(OutstandingQueries, " queries of type ",
                                  GetQueryTypeString(static_cast<QUERY_TYPE>(QueryType)),
                                  " have not been returned to the query manager");
            }
        }
        QueryUsageSS << std::endl
                     << std::setw(30) << std::left << GetQueryTypeString(static_cast<QUERY_TYPE>(QueryType)) << ": "
                     << std::setw(4) << std::right << HeapInfo.MaxAllocatedQueries
                     << '/' << std::setw(4) << HeapInfo.PoolSize;
    }
    LOG_INFO_MESSAGE(QueryUsageSS.str());
}

Uint32 QueryManagerVk::AllocateQuery(QUERY_TYPE Type)
{
    std::lock_guard<std::mutex> Lock(m_HeapMutex);

    Uint32 Index            = InvalidIndex;
    auto&  HeapInfo         = m_Heaps[Type];
    auto&  AvailableQueries = HeapInfo.AvailableQueries;
    if (!AvailableQueries.empty())
    {
        Index = HeapInfo.AvailableQueries.back();
        AvailableQueries.pop_back();
        HeapInfo.MaxAllocatedQueries = std::max(HeapInfo.MaxAllocatedQueries, HeapInfo.PoolSize - static_cast<Uint32>(AvailableQueries.size()));
    }

    return Index;
}

void QueryManagerVk::DiscardQuery(QUERY_TYPE Type, Uint32 Index)
{
    std::lock_guard<std::mutex> Lock(m_HeapMutex);

    auto& HeapInfo = m_Heaps[Type];
    VERIFY(Index < HeapInfo.PoolSize, "Query index ", Index, " is out of range");
    VERIFY(HeapInfo.vkQueryPool != VK_NULL_HANDLE, "Query pool is not initialized");
#ifdef DILIGENT_DEBUG
    for (const auto& ind : HeapInfo.AvailableQueries)
    {
        VERIFY(ind != Index, "Index ", Index, " already present in available queries list");
    }
    for (const auto& ind : HeapInfo.StaleQueries)
    {
        VERIFY(ind != Index, "Index ", Index, " already present in stale queries list");
    }
#endif
    HeapInfo.StaleQueries.push_back(Index);
}

Uint32 QueryManagerVk::ResetStaleQueries(VulkanUtilities::VulkanCommandBuffer& CmdBuff)
{
    std::lock_guard<std::mutex> Lock(m_HeapMutex);

    Uint32 NumQueriesReset = 0;
    for (auto& HeapInfo : m_Heaps)
    {
        VERIFY(HeapInfo.StaleQueries.empty() || HeapInfo.vkQueryPool != VK_NULL_HANDLE, "Query pool is not initialized");

        for (auto& StaleQuery : HeapInfo.StaleQueries)
        {
            CmdBuff.ResetQueryPool(HeapInfo.vkQueryPool, StaleQuery, 1);
            HeapInfo.AvailableQueries.push_front(StaleQuery);
            ++NumQueriesReset;
        }
        HeapInfo.StaleQueries.clear();
    }

    return NumQueriesReset;
}

} // namespace Diligent
