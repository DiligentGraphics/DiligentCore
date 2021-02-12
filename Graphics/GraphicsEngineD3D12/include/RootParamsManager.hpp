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

#pragma once

/// \file
/// Declaration of Diligent::RootParamsManager class and related data structures

#include <memory>

#include "BasicTypes.h"

namespace Diligent
{

enum ROOT_PARAMETER_GROUP : Uint8
{
    ROOT_PARAMETER_GROUP_STATIC_MUTABLE = 0,
    ROOT_PARAMETER_GROUP_DYNAMIC        = 1,
    ROOT_PARAMETER_GROUP_COUNT
};

class RootParameter
{
public:
    RootParameter(ROOT_PARAMETER_GROUP        Group,
                  Uint32                      RootIndex,
                  const D3D12_ROOT_PARAMETER& d3d12RootParam,
                  Uint32                      DescriptorTableSize = 0) noexcept;

    // clang-format off
    RootParameter           (const RootParameter&)  = delete;
    RootParameter& operator=(const RootParameter&)  = delete;
    RootParameter           (RootParameter&&)       = delete;
    RootParameter& operator=(RootParameter&&)       = delete;
    // clang-format on

    // Initializes descriptor range at the specified index.
    // The parameter type must be D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE.
    void InitDescriptorRange(UINT                          RangeIndex,
                             const D3D12_DESCRIPTOR_RANGE& Range);

    ROOT_PARAMETER_GROUP GetGroup() const { return m_Group; }

    static constexpr D3D12_DESCRIPTOR_RANGE_TYPE InvalidDescriptorRangeType = static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(0xFFFFFFFF);

    Uint32 GetDescriptorTableSize() const
    {
        VERIFY(m_d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Incorrect parameter type: descriptor table is expected");
        return m_DescriptorTableSize;
    }

    D3D12_SHADER_VISIBILITY   GetShaderVisibility() const { return m_d3d12RootParam.ShaderVisibility; }
    D3D12_ROOT_PARAMETER_TYPE GetParameterType() const { return m_d3d12RootParam.ParameterType; }

    Uint32 GetLocalRootIndex() const { return m_RootIndex; }

    operator const D3D12_ROOT_PARAMETER&() const { return m_d3d12RootParam; }

    bool operator==(const RootParameter& rhs) const;
    bool operator!=(const RootParameter& rhs) const { return !(*this == rhs); }

    size_t GetHash() const;

#ifdef DILIGENT_DEBUG
    void DbgValidateAsTable() const;
    void DbgValidateAsView() const;
#endif

private:
    const D3D12_ROOT_PARAMETER m_d3d12RootParam;
    const Uint16               m_RootIndex;
    const ROOT_PARAMETER_GROUP m_Group;
    Uint32                     m_DescriptorTableSize = 0;
};


class RootParamsManager
{
public:
    RootParamsManager(IMemoryAllocator& MemAllocator);

    // clang-format off
    RootParamsManager           (const RootParamsManager&) = delete;
    RootParamsManager& operator=(const RootParamsManager&) = delete;
    RootParamsManager           (RootParamsManager&&)      = delete;
    RootParamsManager& operator=(RootParamsManager&&)      = delete;
    // clang-format on

    Uint32 GetNumRootTables() const { return m_NumRootTables; }
    Uint32 GetNumRootViews() const { return m_NumRootViews; }

    const RootParameter& GetRootTable(Uint32 TableInd) const
    {
        VERIFY_EXPR(TableInd < m_NumRootTables);
        return m_pRootTables[TableInd];
    }

    const RootParameter& GetRootView(Uint32 ViewInd) const
    {
        VERIFY_EXPR(ViewInd < m_NumRootViews);
        return m_pRootViews[ViewInd];
    }

    // Adds a new root view parameter and returns the pointer to it.
    RootParameter* AddRootView(D3D12_ROOT_PARAMETER_TYPE ParameterType,
                               Uint32                    RootIndex,
                               UINT                      Register,
                               UINT                      RegisterSpace,
                               D3D12_SHADER_VISIBILITY   Visibility,
                               ROOT_PARAMETER_GROUP      RootType);

    // Adds a new root table parameter and returns the pointer to it.
    RootParameter* AddRootTable(Uint32                  RootIndex,
                                D3D12_SHADER_VISIBILITY Visibility,
                                ROOT_PARAMETER_GROUP    RootType,
                                Uint32                  NumRangesInNewTable = 1);

    // Adds NumExtraRanges descriptor ranges to the root table at index RootTableInd
    // and returns the pointer to the table.
    RootParameter* ExtendRootTable(Uint32 RootTableInd, Uint32 NumExtraRanges = 1);

    template <class TOperation>
    void ProcessRootTables(TOperation) const;

    bool operator==(const RootParamsManager& RootParams) const;

private:
    // Extends current parameters set by adding NumExtraRootTables root tables,
    // NumExtraRootViews root views, and adding NumExtraDescriptorRanges
    // descriptor ranges to the root table at index RootTableToAddRanges.
    void Extend(Uint32               NumExtraRootTables,
                const RootParameter* ExtraRootTables,
                Uint32               NumExtraRootViews,
                const RootParameter* ExtraRootViews,
                Uint32               NumExtraDescriptorRanges = 0,
                Uint32               RootTableToAddRanges     = ~0u);

    IMemoryAllocator&                                         m_MemAllocator;
    std::unique_ptr<void, STDDeleter<void, IMemoryAllocator>> m_pMemory;

    Uint32         m_NumRootTables = 0;
    Uint32         m_NumRootViews  = 0;
    RootParameter* m_pRootTables   = nullptr;
    RootParameter* m_pRootViews    = nullptr;
};

template <class TOperation>
__forceinline void RootParamsManager::ProcessRootTables(TOperation Operation) const
{
    for (Uint32 rt = 0; rt < m_NumRootTables; ++rt)
    {
        auto&                       RootTable  = GetRootTable(rt);
        auto                        RootInd    = RootTable.GetLocalRootIndex();
        const D3D12_ROOT_PARAMETER& D3D12Param = RootTable;

        VERIFY_EXPR(D3D12Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);

        auto& d3d12Table = D3D12Param.DescriptorTable;
        VERIFY(d3d12Table.NumDescriptorRanges > 0 && RootTable.GetDescriptorTableSize() > 0, "Unexepected empty descriptor table");
        bool                       IsResourceTable = d3d12Table.pDescriptorRanges[0].RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        D3D12_DESCRIPTOR_HEAP_TYPE dbgHeapType     = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
#ifdef DILIGENT_DEBUG
        dbgHeapType = IsResourceTable ? D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV : D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
#endif
        Operation(RootInd, RootTable, D3D12Param, IsResourceTable, dbgHeapType);
    }
}

} // namespace Diligent
