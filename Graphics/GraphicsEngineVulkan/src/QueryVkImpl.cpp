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
 *  In no event and under no legal theory, whether in tort (including neVkigence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly neVkigent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "pch.h"

#include "QueryVkImpl.hpp"
#include "EngineMemory.h"
#include "RenderDeviceVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"

namespace Diligent
{

QueryVkImpl::QueryVkImpl(IReferenceCounters* pRefCounters,
                         RenderDeviceVkImpl* pRendeDeviceVkImpl,
                         const QueryDesc&    Desc,
                         bool                IsDeviceInternal) :
    // clang-format off
    TQueryBase
    {
        pRefCounters,
        pRendeDeviceVkImpl,
        Desc,
        IsDeviceInternal
    }
// clang-format on
{
}

QueryVkImpl::~QueryVkImpl()
{
    if (m_QueryPoolIndex != QueryManagerVk::InvalidIndex)
    {
        VERIFY(m_pContext != nullptr, "Device context is not initialized");
        auto* pQueryMgr = m_pContext.RawPtr<DeviceContextVkImpl>()->GetQueryManager();
        VERIFY_EXPR(pQueryMgr != nullptr);
        pQueryMgr->DiscardQuery(m_Desc.Type, m_QueryPoolIndex);
    }
}

void QueryVkImpl::DiscardQuery()
{
    if (m_QueryPoolIndex != QueryManagerVk::InvalidIndex)
    {
        VERIFY_EXPR(m_pContext);
        auto* pQueryMgr = m_pContext.RawPtr<DeviceContextVkImpl>()->GetQueryManager();
        VERIFY_EXPR(pQueryMgr != nullptr);
        pQueryMgr->DiscardQuery(m_Desc.Type, m_QueryPoolIndex);
        m_QueryPoolIndex = QueryManagerVk::InvalidIndex;
    }
}

void QueryVkImpl::Invalidate()
{
    DiscardQuery();
    TQueryBase::Invalidate();
}

bool QueryVkImpl::AllocateQuery()
{
    DiscardQuery();
    VERIFY_EXPR(m_pContext);
    auto* pQueryMgr = m_pContext.RawPtr<DeviceContextVkImpl>()->GetQueryManager();
    VERIFY_EXPR(pQueryMgr != nullptr);
    VERIFY_EXPR(m_QueryPoolIndex == QueryManagerVk::InvalidIndex);

    m_QueryPoolIndex = pQueryMgr->AllocateQuery(m_Desc.Type);
    if (m_QueryPoolIndex == QueryManagerVk::InvalidIndex)
    {
        LOG_ERROR_MESSAGE("Failed to allocate Vulkan query for type ", GetQueryTypeString(m_Desc.Type),
                          ". Increase the query pool size in EngineVkCreateInfo.");
        return false;
    }

    return true;
}

bool QueryVkImpl::OnBeginQuery(IDeviceContext* pContext)
{
    if (!TQueryBase::OnBeginQuery(pContext))
        return false;

    return AllocateQuery();
}

bool QueryVkImpl::OnEndQuery(IDeviceContext* pContext)
{
    if (!TQueryBase::OnEndQuery(pContext))
        return false;

    if (m_Desc.Type == QUERY_TYPE_TIMESTAMP)
    {
        if (!AllocateQuery())
            return false;
    }

    if (m_QueryPoolIndex == QueryManagerVk::InvalidIndex)
    {
        LOG_ERROR_MESSAGE("Query '", m_Desc.Name, "' is invalid: Vulkan query allocation failed");
        return false;
    }

    auto CmdQueueId      = m_pContext.RawPtr<DeviceContextVkImpl>()->GetCommandQueueId();
    m_QueryEndFenceValue = m_pDevice->GetNextFenceValue(CmdQueueId);

    return true;
}

bool QueryVkImpl::GetData(void* pData, Uint32 DataSize, bool AutoInvalidate)
{
    auto CmdQueueId          = m_pContext.RawPtr<DeviceContextVkImpl>()->GetCommandQueueId();
    auto CompletedFenceValue = m_pDevice->GetCompletedFenceValue(CmdQueueId);
    bool DataAvailable       = false;
    if (CompletedFenceValue >= m_QueryEndFenceValue)
    {
        auto* pQueryMgr = m_pContext.RawPtr<DeviceContextVkImpl>()->GetQueryManager();
        VERIFY_EXPR(pQueryMgr != nullptr);
        const auto& LogicalDevice = m_pDevice->GetLogicalDevice();
        auto        vkQueryPool   = pQueryMgr->GetQueryPool(m_Desc.Type);

        switch (m_Desc.Type)
        {
            case QUERY_TYPE_OCCLUSION:
            {
                uint64_t Results[2];
                // If VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set, the final integer value written for each query
                // is non-zero if the query's status was available or zero if the status was unavailable.

                // Applications must take care to ensure that use of the VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
                // bit has the desired effect.
                // For example, if a query has been used previously and a command buffer records the commands
                // vkCmdResetQueryPool, vkCmdBeginQuery, and vkCmdEndQuery for that query, then the query will
                // remain in the available state until vkResetQueryPoolEXT is called or the vkCmdResetQueryPool
                // command executes on a queue. Applications can use fences or events to ensure that a query has
                // already been reset before checking for its results or availability status. Otherwise, a stale
                // value could be returned from a previous use of the query.
                auto res = LogicalDevice.GetQueryPoolResults(vkQueryPool, m_QueryPoolIndex, 1, sizeof(Results), Results, 0, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

                DataAvailable = (res == VK_SUCCESS && Results[1] != 0);
                if (DataAvailable && pData != nullptr)
                {
                    auto& QueryData      = *reinterpret_cast<QueryDataOcclusion*>(pData);
                    QueryData.NumSamples = Results[0];
                }
            }
            break;

            case QUERY_TYPE_BINARY_OCCLUSION:
            {
                uint64_t Results[2];
                auto     res = LogicalDevice.GetQueryPoolResults(vkQueryPool, m_QueryPoolIndex, 1, sizeof(Results), Results, 0, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

                DataAvailable = (res == VK_SUCCESS && Results[1] != 0);
                if (DataAvailable && pData != nullptr)
                {
                    auto& QueryData           = *reinterpret_cast<QueryDataBinaryOcclusion*>(pData);
                    QueryData.AnySamplePassed = Results[0] != 0;
                }
            }
            break;

            case QUERY_TYPE_TIMESTAMP:
            {
                uint64_t Results[2];
                auto     res = LogicalDevice.GetQueryPoolResults(vkQueryPool, m_QueryPoolIndex, 1, sizeof(Results), Results, 0, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

                DataAvailable = (res == VK_SUCCESS && Results[1] != 0);
                if (DataAvailable && pData != nullptr)
                {
                    auto& QueryData     = *reinterpret_cast<QueryDataTimestamp*>(pData);
                    QueryData.Counter   = Results[0];
                    QueryData.Frequency = pQueryMgr->GetCounterFrequency();
                }
            }
            break;

            case QUERY_TYPE_PIPELINE_STATISTICS:
            {
                // Pipeline statistics queries write one integer value for each bit that is enabled in the
                // pipelineStatistics when the pool is created, and the statistics values are written in bit
                // order starting from the least significant bit. (17.2)

                Uint64 Results[12];
                auto   res = LogicalDevice.GetQueryPoolResults(vkQueryPool, m_QueryPoolIndex, 1, sizeof(Results), Results, 0, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

                DataAvailable = (res == VK_SUCCESS);
                if (DataAvailable && pData != nullptr)
                {
                    auto& QueryData = *reinterpret_cast<QueryDataPipelineStatistics*>(pData);

                    const auto EnabledShaderStages = LogicalDevice.GetEnabledGraphicsShaderStages();

                    auto Idx = 0;

                    QueryData.InputVertices   = Results[Idx++]; // INPUT_ASSEMBLY_VERTICES_BIT   = 0x00000001
                    QueryData.InputPrimitives = Results[Idx++]; // INPUT_ASSEMBLY_PRIMITIVES_BIT = 0x00000002
                    QueryData.VSInvocations   = Results[Idx++]; // VERTEX_SHADER_INVOCATIONS_BIT = 0x00000004
                    if (EnabledShaderStages & VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT)
                    {
                        QueryData.GSInvocations = Results[Idx++]; // GEOMETRY_SHADER_INVOCATIONS_BIT = 0x00000008
                        QueryData.GSPrimitives  = Results[Idx++]; // GEOMETRY_SHADER_PRIMITIVES_BIT  = 0x00000010
                    }
                    QueryData.ClippingInvocations = Results[Idx++]; // CLIPPING_INVOCATIONS_BIT         = 0x00000020
                    QueryData.ClippingPrimitives  = Results[Idx++]; // CLIPPING_PRIMITIVES_BIT          = 0x00000040
                    QueryData.PSInvocations       = Results[Idx++]; // FRAGMENT_SHADER_INVOCATIONS_BIT  = 0x00000080

                    if (EnabledShaderStages & VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT)
                        QueryData.HSInvocations = Results[Idx++]; // TESSELLATION_CONTROL_SHADER_PATCHES_BIT        = 0x00000100

                    if (EnabledShaderStages & VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT)
                        QueryData.DSInvocations = Results[Idx++]; // TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT = 0x00000200

                    QueryData.CSInvocations = Results[Idx++]; // COMPUTE_SHADER_INVOCATIONS_BIT = 0x00000400

                    DataAvailable = Results[Idx] != 0;
                }
            }
            break;

            default:
                UNEXPECTED("Unexpected query type");
        }
    }

    if (DataAvailable && pData != nullptr && AutoInvalidate)
    {
        Invalidate();
    }

    return DataAvailable;
}

} // namespace Diligent
