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

#include "RootParamsManager.hpp"
#include "D3D12Utils.h"

namespace Diligent
{

RootParameter::RootParameter(ROOT_PARAMETER_GROUP        Group,
                             Uint32                      RootIndex,
                             const D3D12_ROOT_PARAMETER& d3d12RootParam) noexcept :
    m_d3d12RootParam{d3d12RootParam},
    m_RootIndex{static_cast<decltype(m_RootIndex)>(RootIndex)},
    m_Group{Group}
{
    VERIFY(m_RootIndex == RootIndex, "Root index (", RootIndex, ") exceeds representable range");
}

void RootParameter::InitDescriptorRange(UINT                          RangeIndex,
                                        const D3D12_DESCRIPTOR_RANGE& Range)
{
    VERIFY(m_d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
           "Incorrect parameter type: descriptor table is expected");

    const auto& d3d12Tbl = m_d3d12RootParam.DescriptorTable;
    VERIFY(RangeIndex < d3d12Tbl.NumDescriptorRanges, "Invalid descriptor range index");

    auto& DstRange = const_cast<D3D12_DESCRIPTOR_RANGE&>(d3d12Tbl.pDescriptorRanges[RangeIndex]);
    VERIFY(DstRange.RangeType == static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(0xFFFFFFFF), "Descriptor range has already been initialized.");
    DstRange = Range;

    m_DescriptorTableSize = std::max(m_DescriptorTableSize, Range.OffsetInDescriptorsFromTableStart + Range.NumDescriptors);
}

bool RootParameter::operator==(const RootParameter& rhs) const
{
    if (m_Group != rhs.m_Group ||
        m_DescriptorTableSize != rhs.m_DescriptorTableSize ||
        m_RootIndex != rhs.m_RootIndex)
        return false;

    return m_d3d12RootParam == rhs.m_d3d12RootParam;
}


size_t RootParameter::GetHash() const
{
    size_t hash = ComputeHash(m_Group, m_DescriptorTableSize, m_RootIndex);
    HashCombine(hash, int{m_d3d12RootParam.ParameterType}, int{m_d3d12RootParam.ShaderVisibility});

    switch (m_d3d12RootParam.ParameterType)
    {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
        {
            const auto& tbl = m_d3d12RootParam.DescriptorTable;
            HashCombine(hash, tbl.NumDescriptorRanges);
            for (UINT r = 0; r < tbl.NumDescriptorRanges; ++r)
            {
                const auto& rng = tbl.pDescriptorRanges[r];
                HashCombine(hash, int{rng.RangeType}, rng.NumDescriptors, rng.BaseShaderRegister, rng.RegisterSpace, rng.OffsetInDescriptorsFromTableStart);
            }
        }
        break;

        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        {
            const auto& cnst = m_d3d12RootParam.Constants;
            HashCombine(hash, cnst.ShaderRegister, cnst.RegisterSpace, cnst.Num32BitValues);
        }
        break;

        case D3D12_ROOT_PARAMETER_TYPE_CBV:
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
        case D3D12_ROOT_PARAMETER_TYPE_UAV:
        {
            const auto& dscr = m_d3d12RootParam.Descriptor;
            HashCombine(hash, dscr.ShaderRegister, dscr.RegisterSpace);
        }
        break;

        default: UNEXPECTED("Unexpected root parameter type");
    }

    return hash;
}

#ifdef DILIGENT_DEBUG
void RootParameter::DbgValidateAsTable() const
{
    VERIFY(GetParameterType() == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Unexpected parameter type: descriptor table is expected");
    const auto& d3d12SrcTbl = m_d3d12RootParam.DescriptorTable;

    Uint32 dbgTableSize = 0;
    if (d3d12SrcTbl.pDescriptorRanges != nullptr)
    {
        for (Uint32 r = 0; r < d3d12SrcTbl.NumDescriptorRanges; ++r)
        {
            const auto& Range = d3d12SrcTbl.pDescriptorRanges[r];
            dbgTableSize      = std::max(dbgTableSize, Range.OffsetInDescriptorsFromTableStart + Range.NumDescriptors);
        }
    }
    VERIFY(dbgTableSize == GetDescriptorTableSize(), "Incorrect descriptor table size");
}

void RootParameter::DbgValidateAsView() const
{
    const auto ParameterType = GetParameterType();
    VERIFY(ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV || ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV || ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV,
           "Unexpected parameter type: SBV, SRV or UAV is expected");
}
#endif

RootParamsManager::RootParamsManager(IMemoryAllocator& MemAllocator) :
    m_MemAllocator{MemAllocator},
    m_pMemory{nullptr, STDDeleter<void, IMemoryAllocator>(MemAllocator)}
{}


void RootParamsManager::Extend(Uint32               NumExtraRootTables,
                               const RootParameter* ExtraRootTables,
                               Uint32               NumExtraRootViews,
                               const RootParameter* ExtraRootViews,
                               Uint32               NumExtraDescriptorRanges,
                               Uint32               RootTableToAddRanges)
{
    VERIFY(NumExtraRootTables > 0 || NumExtraRootViews > 0 || NumExtraDescriptorRanges > 0,
           "At least one root table, root view or descriptor range must be added");

    const auto NewParamsCount = m_NumRootTables + NumExtraRootTables + m_NumRootViews + NumExtraRootViews;

    auto NewRangesCount = NumExtraDescriptorRanges;
    for (Uint32 rt = 0; rt < m_NumRootTables + NumExtraRootTables; ++rt)
    {
        const auto& Tbl = rt < m_NumRootTables ? GetRootTable(rt) : ExtraRootTables[rt - m_NumRootTables];
        NewRangesCount += static_cast<const D3D12_ROOT_PARAMETER&>(Tbl).DescriptorTable.NumDescriptorRanges;
    }

    const auto MemorySize =
        NewParamsCount * sizeof(RootParameter) +
        NewRangesCount * sizeof(D3D12_DESCRIPTOR_RANGE);

    VERIFY_EXPR(MemorySize > 0);
    decltype(m_pMemory) pNewMemory{
        ALLOCATE_RAW(m_MemAllocator, "Memory buffer for root tables, root views & descriptor ranges", MemorySize),
        m_pMemory.get_deleter() //
    };

#ifdef DILIGENT_DEBUG
    memset(pNewMemory.get(), 0xFF, MemorySize);
#endif

    // Note: this order is more efficient than views->tables->ranges
    auto* pNewRootTables      = reinterpret_cast<RootParameter*>(pNewMemory.get());
    auto* pNewRootViews       = pNewRootTables + (m_NumRootTables + NumExtraRootTables);
    auto* pDescriptorRangePtr = reinterpret_cast<D3D12_DESCRIPTOR_RANGE*>(pNewRootViews + m_NumRootViews + NumExtraRootViews);

    // Copy root tables to new memory
    for (Uint32 rt = 0; rt < m_NumRootTables + NumExtraRootTables; ++rt)
    {
        const auto& SrcTbl = rt < m_NumRootTables ? GetRootTable(rt) : ExtraRootTables[rt - m_NumRootTables];
#ifdef DILIGENT_DEBUG
        SrcTbl.DbgValidateAsTable();
#endif
        auto& d3d12SrcTbl = static_cast<const D3D12_ROOT_PARAMETER&>(SrcTbl).DescriptorTable;
        auto  NumRanges   = d3d12SrcTbl.NumDescriptorRanges;
        if (rt == RootTableToAddRanges)
        {
            VERIFY(d3d12SrcTbl.pDescriptorRanges == nullptr, "Adding extra descriptors to a new table. This is likely not intended.");
            NumRanges += NumExtraDescriptorRanges;
        }

        // Copy existing ranges, if any (pDescriptorRanges == null for extra descriptor tables)
        if (d3d12SrcTbl.pDescriptorRanges != nullptr)
        {
            memcpy(pDescriptorRangePtr, d3d12SrcTbl.pDescriptorRanges, d3d12SrcTbl.NumDescriptorRanges * sizeof(D3D12_DESCRIPTOR_RANGE));
        }

        new (pNewRootTables + rt) RootParameter{
            SrcTbl.GetGroup(), SrcTbl.GetLocalRootIndex(),
            D3D12_ROOT_PARAMETER //
            {
                D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                D3D12_ROOT_DESCRIPTOR_TABLE{NumRanges, pDescriptorRangePtr},
                SrcTbl.GetShaderVisibility() //
            }                                //
        };

        pDescriptorRangePtr += NumRanges;
    }

    // Copy root views to new memory
    for (Uint32 rv = 0; rv < m_NumRootViews + NumExtraRootViews; ++rv)
    {
        const auto& SrcView = rv < m_NumRootViews ? GetRootView(rv) : ExtraRootViews[rv - m_NumRootViews];
#ifdef DILIGENT_DEBUG
        SrcView.DbgValidateAsView();
#endif
        new (pNewRootViews + rv) RootParameter{SrcView.GetGroup(), SrcView.GetLocalRootIndex(), static_cast<const D3D12_ROOT_PARAMETER&>(SrcView)};
    }

    m_pMemory.swap(pNewMemory);
    m_NumRootTables += NumExtraRootTables;
    m_NumRootViews += NumExtraRootViews;
    m_pRootTables = m_NumRootTables != 0 ? pNewRootTables : nullptr;
    m_pRootViews  = m_NumRootViews != 0 ? pNewRootViews : nullptr;
}


RootParameter* RootParamsManager::AddRootView(D3D12_ROOT_PARAMETER_TYPE ParameterType,
                                              Uint32                    RootIndex,
                                              UINT                      Register,
                                              UINT                      RegisterSpace,
                                              D3D12_SHADER_VISIBILITY   Visibility,
                                              ROOT_PARAMETER_GROUP      Group)
{
#ifdef DILIGENT_DEBUG
    VERIFY(ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV || ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV || ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV,
           "Unexpected parameter type SBV, SRV or UAV is expected");

    for (Uint32 rt = 0; rt < GetNumRootTables(); ++rt)
        VERIFY(GetRootTable(rt).GetLocalRootIndex() != RootIndex, "Index ", RootIndex, " is already used by another root table");
    for (Uint32 rv = 0; rv < GetNumRootViews(); ++rv)
        VERIFY(GetRootView(rv).GetLocalRootIndex() != RootIndex, "Index ", RootIndex, " is already used by another root view");
#endif

    D3D12_ROOT_PARAMETER d3d12RootParam{ParameterType, {}, Visibility};
    d3d12RootParam.Descriptor.ShaderRegister = Register;
    d3d12RootParam.Descriptor.RegisterSpace  = RegisterSpace;
    RootParameter NewRootView{Group, RootIndex, d3d12RootParam};
    Extend(0, nullptr, 1, &NewRootView);

    return &m_pRootViews[m_NumRootViews - 1];
}

RootParameter* RootParamsManager::AddRootTable(Uint32                  RootIndex,
                                               D3D12_SHADER_VISIBILITY Visibility,
                                               ROOT_PARAMETER_GROUP    Group,
                                               Uint32                  NumRangesInNewTable)
{
#ifdef DILIGENT_DEBUG
    for (Uint32 rt = 0; rt < GetNumRootTables(); ++rt)
        VERIFY(GetRootTable(rt).GetLocalRootIndex() != RootIndex, "Index ", RootIndex, " is already used by another root table");
    for (Uint32 rv = 0; rv < GetNumRootViews(); ++rv)
        VERIFY(GetRootView(rv).GetLocalRootIndex() != RootIndex, "Index ", RootIndex, " is already used by another root view");
#endif

    RootParameter NewRootTable{
        Group, RootIndex,
        D3D12_ROOT_PARAMETER //
        {
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            D3D12_ROOT_DESCRIPTOR_TABLE{NumRangesInNewTable, nullptr},
            Visibility //
        }              //
    };
    Extend(1, &NewRootTable, 0, nullptr);

    return &m_pRootTables[m_NumRootTables - 1];
}

RootParameter* RootParamsManager::ExtendRootTable(Uint32 RootTableInd, Uint32 NumExtraRanges)
{
    VERIFY_EXPR(RootTableInd < m_NumRootTables);
    Extend(0, nullptr, 0, nullptr, NumExtraRanges, RootTableInd);
    return &m_pRootTables[RootTableInd - 1];
}

bool RootParamsManager::operator==(const RootParamsManager& RootParams) const
{
    if (m_NumRootTables != RootParams.m_NumRootTables ||
        m_NumRootViews != RootParams.m_NumRootViews)
        return false;

    for (Uint32 rv = 0; rv < m_NumRootViews; ++rv)
    {
        const auto& RV0 = GetRootView(rv);
        const auto& RV1 = RootParams.GetRootView(rv);
        if (RV0 != RV1)
            return false;
    }

    for (Uint32 rv = 0; rv < m_NumRootTables; ++rv)
    {
        const auto& RT0 = GetRootTable(rv);
        const auto& RT1 = RootParams.GetRootTable(rv);
        if (RT0 != RT1)
            return false;
    }

    return true;
}

} // namespace Diligent
