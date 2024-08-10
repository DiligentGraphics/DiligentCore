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

#pragma once

/// \file
/// Declaration of Diligent::QueryManagerWebGPU class

#include "EngineWebGPUImplTraits.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "DeviceContextWebGPUImpl.hpp"

namespace Diligent
{

class QueryManagerWebGPU
{
public:
    QueryManagerWebGPU(RenderDeviceWebGPUImpl* pRenderDeviceWebGPU,
                       const Uint32            QueryHeapSizes[]);

    ~QueryManagerWebGPU();

    // clang-format off
    QueryManagerWebGPU             (const QueryManagerWebGPU&)  = delete;
    QueryManagerWebGPU             (      QueryManagerWebGPU&&) = delete;
    QueryManagerWebGPU& operator = (const QueryManagerWebGPU&)  = delete;
    QueryManagerWebGPU& operator = (      QueryManagerWebGPU&&) = delete;
    // clang-format on

    static constexpr Uint32 InvalidIndex = UINT32_MAX;

    Uint32 AllocateQuery(QUERY_TYPE Type);

    void ReleaseQuery(QUERY_TYPE Type, Uint32 Index);

    WGPUQuerySet GetQuerySet(QUERY_TYPE Type) const;

    Uint32 GetReadbackBufferIdentifier(QUERY_TYPE Type, Uint64 EventValue) const;

    Uint64 GetQueryResult(QUERY_TYPE Type, Uint32 Index, Uint32 BufferIdentifier) const;

    Uint64 GetNextEventValue(QUERY_TYPE Type);

    void ResolveQuerySet(RenderDeviceWebGPUImpl* pDevice, WGPUCommandEncoder wgpuCmdEncoder);

    void ReadbackQuerySet(RenderDeviceWebGPUImpl* pDevice);

    void FinishFrame();

    void WaitAllQuerySet(RenderDeviceWebGPUImpl* pDevice);

private:
    class QuerySetInfo
    {
    public:
        struct ReadbackBufferInfo
        {
            WebGPUBufferWrapper ReadbackBuffer;
            std::vector<Uint64> DataResult;
            Uint32              BufferIdentifier;
            Uint64              LastEventValue;
            Uint64              PendingEventValue;
        };
        using ReadbackBufferList = std::vector<ReadbackBufferInfo>;

    public:
        QuerySetInfo() = default;

        ~QuerySetInfo();

        // clang-format off
        QuerySetInfo             (const QuerySetInfo&)  = delete;
        QuerySetInfo             (      QuerySetInfo&&) = delete;
        QuerySetInfo& operator = (const QuerySetInfo&)  = delete;
        QuerySetInfo& operator = (      QuerySetInfo&&) = delete;
        // clang-format on

        void Initialize(RenderDeviceWebGPUImpl* pDevice, Uint32 HeapSize, QUERY_TYPE Type);

        Uint32 Allocate();

        void Release(Uint32 Index);

        QUERY_TYPE GetType() const;

        Uint32 GetQueryCount() const;

        Uint64 GetQueryResult(Uint32 Index, Uint32 BufferIdentifier) const;

        WGPUQuerySet GetWebGPUQuerySet() const;

        Uint32 GetMaxAllocatedQueries() const;

        bool IsNull() const;

        Uint32 ResolveQueries(RenderDeviceWebGPUImpl* pDevice, WGPUCommandEncoder wgpuCmdEncoder);

        void ReadbackQueries(RenderDeviceWebGPUImpl* pDevice, Uint32 PendingRedbackIndex);

        void WaitAllQueries(RenderDeviceWebGPUImpl* pDevice, Uint32 PendingRedbackIndex);

        ReadbackBufferInfo& FindAvailableReadbackBuffer(RenderDeviceWebGPUImpl* pDevice);

        Uint32 GetReadbackBufferIdentifier(Uint64 EventValue) const;

        Uint64 GetNextEventValue();

        Uint64 IncrementEventValue();

    private:
        WebGPUQuerySetWrapper m_wgpuQuerySet;
        WebGPUBufferWrapper   m_wgpuResolveBuffer;
        std::vector<Uint32>   m_AvailableQueries;
        ReadbackBufferList    m_PendingReadbackBuffers;

        QUERY_TYPE m_Type                = QUERY_TYPE_UNDEFINED;
        Uint32     m_QueryCount          = 0;
        Uint32     m_MaxAllocatedQueries = 0;
        Uint64     m_EventValue          = 0;

        static constexpr Uint32 MaxPendingBuffers = 16;
    };

    std::array<QuerySetInfo, QUERY_TYPE_NUM_TYPES> m_QuerySets;
    std::array<Uint32, QUERY_TYPE_NUM_TYPES>       m_PendingReadbackIndices;
    Uint32                                         m_ActiveQuerySets = 0;
};

} // namespace Diligent
