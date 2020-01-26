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
#include <atlbase.h>

#include "QueryD3D12Impl.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "GraphicsAccessories.hpp"
#include "DeviceContextD3D12Impl.hpp"

namespace Diligent
{

QueryD3D12Impl::QueryD3D12Impl(IReferenceCounters*    pRefCounters,
                               RenderDeviceD3D12Impl* pDevice,
                               const QueryDesc&       Desc) :
    TQueryBase{pRefCounters, pDevice, Desc}
{
    auto& QueryMgr   = pDevice->GetQueryManager();
    m_QueryHeapIndex = QueryMgr.AllocateQuery(m_Desc.Type);
    if (m_QueryHeapIndex == QueryManagerD3D12::InvalidIndex)
    {
        LOG_ERROR_AND_THROW("Failed to allocate D3D12 query for type ", GetQueryTypeString(m_Desc.Type),
                            ". Increase the query pool size in EngineD3D12CreateInfo.");
    }
}

QueryD3D12Impl::~QueryD3D12Impl()
{
    auto& QueryMgr = m_pDevice->GetQueryManager();
    QueryMgr.ReleaseQuery(m_Desc.Type, m_QueryHeapIndex);
}

bool QueryD3D12Impl::OnEndQuery(IDeviceContext* pContext)
{
    if (!TQueryBase::OnEndQuery(pContext))
        return false;

    auto CmdQueueId      = m_pContext.RawPtr<DeviceContextD3D12Impl>()->GetCommandQueueId();
    m_QueryEndFenceValue = m_pDevice->GetNextFenceValue(CmdQueueId);

    return true;
}

bool QueryD3D12Impl::GetData(void* pData, Uint32 DataSize, bool AutoInvalidate)
{
    auto CmdQueueId          = m_pContext.RawPtr<DeviceContextD3D12Impl>()->GetCommandQueueId();
    auto CompletedFenceValue = m_pDevice->GetCompletedFenceValue(CmdQueueId);
    if (CompletedFenceValue >= m_QueryEndFenceValue)
    {
        auto& QueryMgr = m_pDevice->GetQueryManager();

        switch (m_Desc.Type)
        {
            case QUERY_TYPE_OCCLUSION:
            {
                UINT64 NumSamples;
                QueryMgr.ReadQueryData(m_Desc.Type, m_QueryHeapIndex, &NumSamples, sizeof(NumSamples));
                if (pData != nullptr)
                {
                    auto& QueryData      = *reinterpret_cast<QueryDataOcclusion*>(pData);
                    QueryData.NumSamples = NumSamples;
                }
            }
            break;

            case QUERY_TYPE_BINARY_OCCLUSION:
            {
                UINT64 AnySamplePassed;
                QueryMgr.ReadQueryData(m_Desc.Type, m_QueryHeapIndex, &AnySamplePassed, sizeof(AnySamplePassed));
                if (pData != nullptr)
                {
                    auto& QueryData = *reinterpret_cast<QueryDataBinaryOcclusion*>(pData);
                    // Binary occlusion queries write 64-bits per query. The least significant bit is either 0 or 1. The rest of the bits are 0.
                    // https://microsoft.github.io/DirectX-Specs/d3d/CountersAndQueries.html#resolvequerydata
                    QueryData.AnySamplePassed = AnySamplePassed != 0;
                }
            }
            break;

            case QUERY_TYPE_TIMESTAMP:
            {
                UINT64 Counter;
                QueryMgr.ReadQueryData(m_Desc.Type, m_QueryHeapIndex, &Counter, sizeof(Counter));
                if (pData != nullptr)
                {
                    auto& QueryData   = *reinterpret_cast<QueryDataTimestamp*>(pData);
                    QueryData.Counter = Counter;

                    const auto& CmdQueue    = m_pDevice->GetCommandQueue(CmdQueueId);
                    auto*       pd3d12Queue = const_cast<ICommandQueueD3D12&>(CmdQueue).GetD3D12CommandQueue();

                    // https://microsoft.github.io/DirectX-Specs/d3d/CountersAndQueries.html#timestamp-frequency
                    UINT64 TimestampFrequency = 0;
                    pd3d12Queue->GetTimestampFrequency(&TimestampFrequency);
                    QueryData.Frequency = TimestampFrequency;
                }
            }
            break;

            case QUERY_TYPE_PIPELINE_STATISTICS:
            {
                D3D12_QUERY_DATA_PIPELINE_STATISTICS d3d12QueryData;
                QueryMgr.ReadQueryData(m_Desc.Type, m_QueryHeapIndex, &d3d12QueryData, sizeof(d3d12QueryData));
                if (pData != nullptr)
                {
                    auto& QueryData = *reinterpret_cast<QueryDataPipelineStatistics*>(pData);

                    QueryData.InputVertices       = d3d12QueryData.IAVertices;
                    QueryData.InputPrimitives     = d3d12QueryData.IAPrimitives;
                    QueryData.GSPrimitives        = d3d12QueryData.GSPrimitives;
                    QueryData.ClippingInvocations = d3d12QueryData.CInvocations;
                    QueryData.ClippingPrimitives  = d3d12QueryData.CPrimitives;
                    QueryData.VSInvocations       = d3d12QueryData.VSInvocations;
                    QueryData.GSInvocations       = d3d12QueryData.GSInvocations;
                    QueryData.PSInvocations       = d3d12QueryData.PSInvocations;
                    QueryData.HSInvocations       = d3d12QueryData.HSInvocations;
                    QueryData.DSInvocations       = d3d12QueryData.DSInvocations;
                    QueryData.CSInvocations       = d3d12QueryData.CSInvocations;
                }
            }
            break;

            default:
                UNEXPECTED("Unexpected query type");
        }

        if (pData != nullptr && AutoInvalidate)
        {
            Invalidate();
        }

        return true;
    }
    else
    {
        return false;
    }
}

} // namespace Diligent
