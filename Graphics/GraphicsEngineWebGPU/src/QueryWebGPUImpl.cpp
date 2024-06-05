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

#include "QueryWebGPUImpl.hpp"

namespace Diligent
{

QueryWebGPUImpl::QueryWebGPUImpl(IReferenceCounters*     pRefCounters,
                                 RenderDeviceWebGPUImpl* pDevice,
                                 const QueryDesc&        Desc) :
    // clang-format off
    TQueryBase
    {
        pRefCounters,
        pDevice,
        Desc
    }
// clang-format on
{
}

QueryWebGPUImpl::~QueryWebGPUImpl()
{
    ReleaseQueries();
}

bool QueryWebGPUImpl::AllocateQueries()
{
    ReleaseQueries();
    VERIFY_EXPR(m_pContext != nullptr);
    m_pQueryMgr = &m_pContext->GetQueryManager();
    for (Uint32 i = 0; i < (m_Desc.Type == QUERY_TYPE_DURATION ? Uint32{2} : Uint32{1}); ++i)
    {
        m_QuerySetIndex[i] = m_pQueryMgr->AllocateQuery(m_Desc.Type);
        if (m_QuerySetIndex[i] == QueryManagerWebGPU::InvalidIndex)
        {
            LOG_ERROR_MESSAGE("Failed to allocate WebGPU query for type ", GetQueryTypeString(m_Desc.Type),
                              ". Increase the query pool size in EngineWebGPUCreateInfo.");
            ReleaseQueries();
            return false;
        }
    }
    return true;
}

void QueryWebGPUImpl::ReleaseQueries()
{
    for (const auto& SetIdx : m_QuerySetIndex)
    {
        if (SetIdx != QueryManagerWebGPU::InvalidIndex)
        {
            VERIFY_EXPR(m_pQueryMgr != nullptr);
            m_pQueryMgr->ReleaseQuery(m_Desc.Type, SetIdx);
        }
    }
    m_pQueryMgr = nullptr;
}

bool QueryWebGPUImpl::GetData(void* pData, Uint32 DataSize, bool AutoInvalidate)
{
    return false;
}

void QueryWebGPUImpl::Invalidate()
{
    ReleaseQueries();
    TQueryBase::Invalidate();
}

bool QueryWebGPUImpl::OnBeginQuery(DeviceContextWebGPUImpl* pContext)
{
    TQueryBase::OnBeginQuery(pContext);
    return AllocateQueries();
}

bool QueryWebGPUImpl::OnEndQuery(DeviceContextWebGPUImpl* pContext)
{
    TQueryBase::OnEndQuery(pContext);

    if (m_Desc.Type == QUERY_TYPE_TIMESTAMP)
    {
        if (!AllocateQueries())
            return false;
    }

    if (m_QuerySetIndex[0] == QueryManagerWebGPU::InvalidIndex || (m_Desc.Type == QUERY_TYPE_DURATION && m_QuerySetIndex[1] == QueryManagerWebGPU::InvalidIndex))
    {
        LOG_ERROR_MESSAGE("Query '", m_Desc.Name, "' is invalid: WebGPU query allocation failed");
        return false;
    }

    VERIFY_EXPR(m_pQueryMgr != nullptr);

    return true;
}

} // namespace Diligent
