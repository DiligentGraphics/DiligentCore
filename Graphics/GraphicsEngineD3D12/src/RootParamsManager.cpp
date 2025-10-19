/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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
#include "D3D12TypeConversions.hpp"
#include "HashUtils.hpp"
#include "EngineMemory.h"

namespace Diligent
{

namespace
{

const D3D12_DESCRIPTOR_RANGE_TYPE InvalidDescriptorRangeType = static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(-1);

#ifdef DILIGENT_DEBUG
void DbgValidateD3D12RootTable(const D3D12_ROOT_DESCRIPTOR_TABLE& d3d12Tbl)
{
    VERIFY(d3d12Tbl.NumDescriptorRanges > 0, "Descriptor table must contain at least one range");
    VERIFY_EXPR(d3d12Tbl.pDescriptorRanges != nullptr);

    const bool IsSampler  = d3d12Tbl.pDescriptorRanges[0].RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    Uint32     CurrOffset = 0;
    for (Uint32 r = 0; r < d3d12Tbl.NumDescriptorRanges; ++r)
    {
        const D3D12_DESCRIPTOR_RANGE& Range = d3d12Tbl.pDescriptorRanges[r];
        VERIFY(Range.RangeType != InvalidDescriptorRangeType, "Range is not initialized");
        VERIFY(Range.OffsetInDescriptorsFromTableStart == CurrOffset, "Invalid offset");
        VERIFY(Range.NumDescriptors != 0, "Range must contain at lest one descriptor");
        if (IsSampler)
        {
            VERIFY(Range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, "All ranges in the sampler table must be D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER");
        }
        else
        {
            VERIFY(Range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, "D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER is not allowed in the resource table");
        }

        CurrOffset += Range.NumDescriptors;
    }
}
#endif

} // namespace

RootParameter::RootParameter(Uint32                      _RootIndex,
                             ROOT_PARAMETER_GROUP        _Group,
                             const D3D12_ROOT_PARAMETER& _d3d12RootParam,
                             Uint32                      _TableOffsetInGroupAllocation) noexcept :
    // clang-format off
    RootIndex                   {_RootIndex},
    Group                       {_Group},
    TableOffsetInGroupAllocation{_TableOffsetInGroupAllocation},
    d3d12RootParam              {_d3d12RootParam}
// clang-format on
{
    VERIFY_EXPR(RootIndex == _RootIndex);
    VERIFY_EXPR(Group == Group);

#ifdef DILIGENT_DEBUG
    if (d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        DbgValidateD3D12RootTable(d3d12RootParam.DescriptorTable);
#endif
}

bool RootParameter::operator==(const RootParameter& rhs) const noexcept
{
    // clang-format off
    return Group          == rhs.Group     &&
           RootIndex      == rhs.RootIndex &&
           d3d12RootParam == rhs.d3d12RootParam;
    // clang-format on
}


size_t RootParameter::GetHash() const
{
    size_t hash = ComputeHash(Group, RootIndex);
    HashCombine(hash, int{d3d12RootParam.ParameterType}, int{d3d12RootParam.ShaderVisibility});

    switch (d3d12RootParam.ParameterType)
    {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
        {
            const D3D12_ROOT_DESCRIPTOR_TABLE& tbl = d3d12RootParam.DescriptorTable;
            HashCombine(hash, tbl.NumDescriptorRanges);
            for (UINT r = 0; r < tbl.NumDescriptorRanges; ++r)
            {
                const D3D12_DESCRIPTOR_RANGE& rng = tbl.pDescriptorRanges[r];
                HashCombine(hash, int{rng.RangeType}, rng.NumDescriptors, rng.BaseShaderRegister, rng.RegisterSpace, rng.OffsetInDescriptorsFromTableStart);
            }
        }
        break;

        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        {
            const D3D12_ROOT_CONSTANTS& cnst = d3d12RootParam.Constants;
            HashCombine(hash, cnst.ShaderRegister, cnst.RegisterSpace, cnst.Num32BitValues);
        }
        break;

        case D3D12_ROOT_PARAMETER_TYPE_CBV:
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
        case D3D12_ROOT_PARAMETER_TYPE_UAV:
        {
            const D3D12_ROOT_DESCRIPTOR& dscr = d3d12RootParam.Descriptor;
            HashCombine(hash, dscr.ShaderRegister, dscr.RegisterSpace);
        }
        break;

        default: UNEXPECTED("Unexpected root parameter type");
    }

    return hash;
}

RootParamsManager::~RootParamsManager()
{
    static_assert(std::is_trivially_destructible<RootParameter>::value, "m_pRootTables, m_pRootViews and m_pRootConstants must be manually destroyed");
}

bool RootParamsManager::operator==(const RootParamsManager& RootParams) const noexcept
{
    if (m_NumRootTables != RootParams.m_NumRootTables ||
        m_NumRootViews != RootParams.m_NumRootViews ||
        m_NumRootConstants != RootParams.m_NumRootConstants)
        return false;

    for (Uint32 rv = 0; rv < m_NumRootViews; ++rv)
    {
        const RootParameter& RV0 = GetRootView(rv);
        const RootParameter& RV1 = RootParams.GetRootView(rv);
        if (RV0 != RV1)
            return false;
    }

    for (Uint32 rv = 0; rv < m_NumRootTables; ++rv)
    {
        const RootParameter& RT0 = GetRootTable(rv);
        const RootParameter& RT1 = RootParams.GetRootTable(rv);
        if (RT0 != RT1)
            return false;
    }

    for (Uint32 rc = 0; rc < m_NumRootConstants; ++rc)
    {
        const RootParameter& RC0 = GetRootConstants(rc);
        const RootParameter& RC1 = RootParams.GetRootConstants(rc);
        if (RC0 != RC1)
            return false;
    }

    return true;
}

#ifdef DILIGENT_DEBUG
void RootParamsManager::Validate() const
{
    std::array<std::array<std::vector<bool>, ROOT_PARAMETER_GROUP_COUNT>, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1> DescriptorSlots;
    for (D3D12_DESCRIPTOR_HEAP_TYPE d3d12HeapType : {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER})
    {
        for (Uint32 group = 0; group < ROOT_PARAMETER_GROUP_COUNT; ++group)
        {
            const ROOT_PARAMETER_GROUP Group = static_cast<ROOT_PARAMETER_GROUP>(group);
            DescriptorSlots[d3d12HeapType][Group].resize(GetParameterGroupSize(d3d12HeapType, Group));
        }
    }

    for (Uint32 i = 0; i < GetNumRootTables(); ++i)
    {
        const RootParameter& RootTbl = GetRootTable(i);
        VERIFY_EXPR(RootTbl.d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
        const D3D12_ROOT_DESCRIPTOR_TABLE& d3d12DescriptorTbl = RootTbl.d3d12RootParam.DescriptorTable;
        DbgValidateD3D12RootTable(d3d12DescriptorTbl);
        const D3D12_DESCRIPTOR_HEAP_TYPE d3d12HeapType = D3D12DescriptorRangeTypeToD3D12HeapType(d3d12DescriptorTbl.pDescriptorRanges[0].RangeType);
        const Uint32                     TableOffset   = RootTbl.TableOffsetInGroupAllocation;
        VERIFY_EXPR(TableOffset != RootParameter::InvalidTableOffsetInGroupAllocation);

        std::vector<bool>& TableSlots = DescriptorSlots[d3d12HeapType][RootTbl.Group];
        for (Uint32 r = 0; r < d3d12DescriptorTbl.NumDescriptorRanges; ++r)
        {
            const D3D12_DESCRIPTOR_RANGE& d3d12Range = d3d12DescriptorTbl.pDescriptorRanges[r];
            VERIFY_EXPR(D3D12DescriptorRangeTypeToD3D12HeapType(d3d12Range.RangeType) == d3d12HeapType);
            VERIFY_EXPR(d3d12Range.NumDescriptors > 0);
            const Uint32 RangeStartOffset = TableOffset + d3d12Range.OffsetInDescriptorsFromTableStart;
            VERIFY(size_t{RangeStartOffset} + size_t{d3d12Range.NumDescriptors} <= TableSlots.size(),
                   "Descriptor range exceeds allocated descriptor table size");
            for (Uint32 slot = RangeStartOffset; slot < RangeStartOffset + d3d12Range.NumDescriptors; ++slot)
            {
                VERIFY(!TableSlots[slot], "Slot ", slot, " has already been used by another descriptor range. Overlapping ranges is a bug.");
                TableSlots[slot] = true;
            }
        }
    }

    for (D3D12_DESCRIPTOR_HEAP_TYPE d3d12HeapType : {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER})
    {
        for (Uint32 group = 0; group < ROOT_PARAMETER_GROUP_COUNT; ++group)
        {
            const std::vector<bool>& TableSlots = DescriptorSlots[d3d12HeapType][group];
            for (size_t i = 0; i < TableSlots.size(); ++i)
            {
                VERIFY(TableSlots[i], "Descriptor ", i, " is not used by any of the descriptor ranges. All ranges must be tightly packed.");
            }
        }
    }

    for (Uint32 i = 0; i < GetNumRootViews(); ++i)
    {
        const RootParameter& RootView = GetRootView(i);
        VERIFY(RootView.TableOffsetInGroupAllocation == RootParameter::InvalidTableOffsetInGroupAllocation,
               "Root views must not be assigned to descriptor table allocations.");
    }

    for (Uint32 i = 0; i < GetNumRootConstants(); ++i)
    {
        const RootParameter& RootConst = GetRootConstants(i);
        VERIFY(RootConst.TableOffsetInGroupAllocation == RootParameter::InvalidTableOffsetInGroupAllocation,
               "Root constants must not be assigned to descriptor table allocations.");
    }
}
#endif



RootParamsBuilder::RootParamsBuilder()
{
    for (auto& Map : m_SrvCbvUavRootTablesMap)
        Map.fill(InvalidRootTableIndex);
    for (auto& Map : m_SamplerRootTablesMap)
        Map.fill(InvalidRootTableIndex);
}

#ifdef DILIGENT_DEBUG
void RootParamsBuilder::DbgCheckRootIndexUniqueness(Uint32 RootIndex) const
{
    for (const RootTableData& RootTbl : m_RootTables)
        VERIFY(RootTbl.RootIndex != RootIndex, "Index ", RootIndex, " is already used by another root table");
    for (const RootParameter& RootView : m_RootViews)
        VERIFY(RootView.RootIndex != RootIndex, "Index ", RootIndex, " is already used by another root view");
    for (const RootParameter& RootConst : m_RootConstants)
        VERIFY(RootConst.RootIndex != RootIndex, "Index ", RootIndex, " is already used by another root constant");
}
#endif

RootParameter& RootParamsBuilder::AddRootView(D3D12_ROOT_PARAMETER_TYPE ParameterType,
                                              Uint32                    RootIndex,
                                              UINT                      Register,
                                              UINT                      RegisterSpace,
                                              D3D12_SHADER_VISIBILITY   Visibility,
                                              ROOT_PARAMETER_GROUP      Group)
{
#ifdef DILIGENT_DEBUG
    VERIFY((ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ||
            ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ||
            ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV),
           "Unexpected parameter type SBV, SRV or UAV is expected");
    DbgCheckRootIndexUniqueness(RootIndex);
#endif

    D3D12_ROOT_PARAMETER d3d12RootParam{ParameterType, {}, Visibility};
    d3d12RootParam.Descriptor.ShaderRegister = Register;
    d3d12RootParam.Descriptor.RegisterSpace  = RegisterSpace;
    m_RootViews.emplace_back(RootIndex, Group, d3d12RootParam);

    return m_RootViews.back();
}

RootParameter& RootParamsBuilder::AddRootConstants(Uint32                  RootIndex,
                                                   UINT                    Register,
                                                   UINT                    RegisterSpace,
                                                   UINT                    Num32BitValues,
                                                   D3D12_SHADER_VISIBILITY Visibility,
                                                   ROOT_PARAMETER_GROUP    RootType)
{
#ifdef DILIGENT_DEBUG
    DbgCheckRootIndexUniqueness(RootIndex);
#endif

    D3D12_ROOT_PARAMETER d3d12RootParam{D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS, {}, Visibility};
    d3d12RootParam.Constants.ShaderRegister = Register;
    d3d12RootParam.Constants.RegisterSpace  = RegisterSpace;
    d3d12RootParam.Constants.Num32BitValues = Num32BitValues;
    m_RootConstants.emplace_back(RootIndex, RootType, d3d12RootParam);

    return m_RootConstants.back();
}

RootParamsBuilder::RootTableData::RootTableData(Uint32                  _RootIndex,
                                                D3D12_SHADER_VISIBILITY _Visibility,
                                                ROOT_PARAMETER_GROUP    _Group,
                                                Uint32                  _NumRanges) :
    RootIndex{_RootIndex},
    Group{_Group},
    d3d12RootParam{
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        {D3D12_ROOT_DESCRIPTOR_TABLE{0, nullptr}},
        _Visibility //
    }
{
    Extend(_NumRanges);
}

void RootParamsBuilder::RootTableData::Extend(Uint32 NumExtraRanges)
{
    D3D12_ROOT_DESCRIPTOR_TABLE& d3d12Tbl = d3d12RootParam.DescriptorTable;
    VERIFY_EXPR(d3d12Tbl.NumDescriptorRanges == Ranges.size());
    d3d12Tbl.NumDescriptorRanges += NumExtraRanges;
    Ranges.resize(d3d12Tbl.NumDescriptorRanges);
    d3d12Tbl.pDescriptorRanges = Ranges.data();

#ifdef DILIGENT_DEBUG
    for (Uint32 i = d3d12Tbl.NumDescriptorRanges - NumExtraRanges; i < d3d12Tbl.NumDescriptorRanges; ++i)
        Ranges[i].RangeType = InvalidDescriptorRangeType;
#endif
}

RootParamsBuilder::RootTableData& RootParamsBuilder::AddRootTable(Uint32                  RootIndex,
                                                                  D3D12_SHADER_VISIBILITY Visibility,
                                                                  ROOT_PARAMETER_GROUP    Group,
                                                                  Uint32                  NumRangesInNewTable)
{
#ifdef DILIGENT_DEBUG
    DbgCheckRootIndexUniqueness(RootIndex);
#endif

    m_RootTables.emplace_back(RootIndex, Visibility, Group, NumRangesInNewTable);

    return m_RootTables.back();
}

void RootParamsBuilder::AllocateResourceSlot(SHADER_TYPE                   ShaderStages,
                                             SHADER_RESOURCE_VARIABLE_TYPE VariableType,
                                             D3D12_ROOT_PARAMETER_TYPE     RootParameterType,
                                             D3D12_DESCRIPTOR_RANGE_TYPE   RangeType,
                                             Uint32                        ArraySize,
                                             Uint32                        Register,
                                             Uint32                        Space,
                                             Uint32&                       RootIndex,           // Output parameter
                                             Uint32&                       OffsetFromTableStart // Output parameter
)
{
    const D3D12_SHADER_VISIBILITY ShaderVisibility = ShaderStagesToD3D12ShaderVisibility(ShaderStages);
    const ROOT_PARAMETER_GROUP    ParameterGroup   = VariableTypeToRootParameterGroup(VariableType);

    // Get the next available root index past all allocated tables, root views and root constants
    RootIndex = static_cast<Uint32>(m_RootTables.size() + m_RootViews.size() + m_RootConstants.size());

    if (RootParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ||
        RootParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ||
        RootParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV)
    {
        VERIFY(ArraySize == 1, "Only single descriptors can be added as root view");

        // Allocate single CBV directly in the root signature
        OffsetFromTableStart = 0;

        // Add new root view to existing root parameters
        AddRootView(RootParameterType, RootIndex, Register, Space, ShaderVisibility, ParameterGroup);
    }
    else if (RootParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
        OffsetFromTableStart = 0;

        // Add new 32-bit constants parameter to existing root parameters
        AddRootConstants(RootIndex, Register, Space, ArraySize, ShaderVisibility, ParameterGroup);
    }
    else if (RootParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
    {
        const bool IsSampler = (RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);
        // Get the table array index (this is not the root index!)
        int& RootTableArrayInd = (IsSampler ? m_SamplerRootTablesMap : m_SrvCbvUavRootTablesMap)[ParameterGroup][ShaderVisibility];

        RootTableData* pRootTable = nullptr;
        if (RootTableArrayInd == InvalidRootTableIndex)
        {
            // Root table has not been assigned to this combination yet
            RootTableArrayInd = static_cast<int>(m_RootTables.size());
            // Add root table with one single-descriptor range
            pRootTable = &AddRootTable(RootIndex, ShaderVisibility, ParameterGroup, 1);
        }
        else
        {
            // Add a new single-descriptor range to the existing table at index RootTableArrayInd
            pRootTable = &m_RootTables[RootTableArrayInd];
            pRootTable->Extend(1);
        }

        // Pointer to either existing or just added table
        RootIndex = pRootTable->RootIndex;

        const D3D12_ROOT_PARAMETER& d3d12RootParam = pRootTable->d3d12RootParam;

        VERIFY(d3d12RootParam.ShaderVisibility == ShaderVisibility, "Shader visibility is not correct");

        // New just added range is the last range in the descriptor table
        Uint32 NewDescriptorRangeIndex = d3d12RootParam.DescriptorTable.NumDescriptorRanges - 1;
        if (NewDescriptorRangeIndex > 0)
        {
            // Descriptors are tightly packed, so the next descriptor offset starts after the previous range
            const D3D12_DESCRIPTOR_RANGE& PrevRange = pRootTable->Ranges[NewDescriptorRangeIndex - 1];
            OffsetFromTableStart                    = PrevRange.OffsetInDescriptorsFromTableStart + PrevRange.NumDescriptors;
        }
        else
        {
            OffsetFromTableStart = 0;
        }

        D3D12_DESCRIPTOR_RANGE& NewRange           = pRootTable->Ranges[NewDescriptorRangeIndex];
        NewRange.RangeType                         = RangeType;            // Range type (CBV, SRV, UAV or SAMPLER)
        NewRange.NumDescriptors                    = ArraySize;            // Number of registers used (1 for non-array resources)
        NewRange.BaseShaderRegister                = Register;             // Shader register
        NewRange.RegisterSpace                     = Space;                // Shader register space
        NewRange.OffsetInDescriptorsFromTableStart = OffsetFromTableStart; // Offset in descriptors from the table start
#ifdef DILIGENT_DEBUG
        DbgValidateD3D12RootTable(d3d12RootParam.DescriptorTable);
#endif
    }
    else
    {
        UNSUPPORTED("Unsupported root parameter type");
    }
}

void RootParamsBuilder::InitializeMgr(IMemoryAllocator& MemAllocator, RootParamsManager& ParamsMgr)
{
    VERIFY(!ParamsMgr.m_pMemory, "Params manager has already been initialized!");

    Uint32& NumRootTables    = ParamsMgr.m_NumRootTables;
    Uint32& NumRootViews     = ParamsMgr.m_NumRootViews;
    Uint32& NumRootConstants = ParamsMgr.m_NumRootConstants;

    NumRootTables    = static_cast<Uint32>(m_RootTables.size());
    NumRootViews     = static_cast<Uint32>(m_RootViews.size());
    NumRootConstants = static_cast<Uint32>(m_RootConstants.size());
    if (NumRootTables == 0 && NumRootViews == 0 && NumRootConstants == 0)
        return;

    const size_t TotalRootParamsCount = m_RootTables.size() + m_RootViews.size() + m_RootConstants.size();

    size_t TotalRangesCount = 0;
    for (RootTableData& Tbl : m_RootTables)
    {
        VERIFY_EXPR(Tbl.d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && Tbl.d3d12RootParam.DescriptorTable.NumDescriptorRanges > 0);
        TotalRangesCount += Tbl.d3d12RootParam.DescriptorTable.NumDescriptorRanges;
    }

    const size_t MemorySize = TotalRootParamsCount * sizeof(RootParameter) + TotalRangesCount * sizeof(D3D12_DESCRIPTOR_RANGE);
    VERIFY_EXPR(MemorySize > 0);
    ParamsMgr.m_pMemory = decltype(ParamsMgr.m_pMemory){
        ALLOCATE_RAW(MemAllocator, "Memory buffer for root tables, root views & descriptor ranges", MemorySize),
        STDDeleter<void, IMemoryAllocator>(MemAllocator) //
    };

#ifdef DILIGENT_DEBUG
    memset(ParamsMgr.m_pMemory.get(), 0xFF, MemorySize);
#endif

    // Note: this order is more efficient than views->tables->ranges
    RootParameter* const          pRootTables       = reinterpret_cast<RootParameter*>(ParamsMgr.m_pMemory.get());
    RootParameter* const          pRootViews        = pRootTables + NumRootTables;
    RootParameter* const          pRootConstants    = pRootViews + NumRootViews;
    D3D12_DESCRIPTOR_RANGE* const pDescriptorRanges = reinterpret_cast<D3D12_DESCRIPTOR_RANGE*>(pRootConstants + NumRootConstants);

    // Copy descriptor tables
    D3D12_DESCRIPTOR_RANGE* pCurrDescrRangePtr = pDescriptorRanges;
    for (Uint32 rt = 0; rt < NumRootTables; ++rt)
    {
        const RootTableData&               SrcTbl        = m_RootTables[rt];
        const D3D12_ROOT_PARAMETER&        d3d12SrcParam = SrcTbl.d3d12RootParam;
        const D3D12_ROOT_DESCRIPTOR_TABLE& d3d12SrcTbl   = d3d12SrcParam.DescriptorTable;
#ifdef DILIGENT_DEBUG
        VERIFY(d3d12SrcParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
               "Unexpected parameter type: descriptor table is expected");
        DbgValidateD3D12RootTable(d3d12SrcTbl);
#endif
        memcpy(pCurrDescrRangePtr, d3d12SrcTbl.pDescriptorRanges, d3d12SrcTbl.NumDescriptorRanges * sizeof(D3D12_DESCRIPTOR_RANGE));

        const D3D12_DESCRIPTOR_HEAP_TYPE d3d12HeapType = D3D12DescriptorRangeTypeToD3D12HeapType(d3d12SrcTbl.pDescriptorRanges[0].RangeType);

        Uint32& TableOffsetInGroupAllocation = ParamsMgr.m_ParameterGroupSizes[d3d12HeapType][SrcTbl.Group];

        RootParameter* pTbl = new (pRootTables + rt) RootParameter{
            SrcTbl.RootIndex, SrcTbl.Group,
            D3D12_ROOT_PARAMETER //
            {
                D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                {D3D12_ROOT_DESCRIPTOR_TABLE{d3d12SrcTbl.NumDescriptorRanges, pCurrDescrRangePtr}},
                d3d12SrcParam.ShaderVisibility //
            },
            TableOffsetInGroupAllocation //
        };

        TableOffsetInGroupAllocation += pTbl->GetDescriptorTableSize();
        pCurrDescrRangePtr += d3d12SrcTbl.NumDescriptorRanges;
    }
    VERIFY_EXPR(pCurrDescrRangePtr == pDescriptorRanges + TotalRangesCount);

    // Copy root views
    for (Uint32 rv = 0; rv < NumRootViews; ++rv)
    {
        const RootParameter&        SrcView        = m_RootViews[rv];
        const D3D12_ROOT_PARAMETER& d3d12RootParam = SrcView.d3d12RootParam;
        VERIFY((d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ||
                d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ||
                d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV),
               "Unexpected parameter type: SBV, SRV or UAV is expected");
        new (pRootViews + rv) RootParameter{SrcView.RootIndex, SrcView.Group, d3d12RootParam};
    }

    // Copy root constants
    for (Uint32 rc = 0; rc < NumRootConstants; ++rc)
    {
        const RootParameter&        SrcConst       = m_RootConstants[rc];
        const D3D12_ROOT_PARAMETER& d3d12RootParam = SrcConst.d3d12RootParam;
        VERIFY(d3d12RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
               "Unexpected parameter type: 32-bit constants is expected");
        new (pRootConstants + rc) RootParameter{SrcConst.RootIndex, SrcConst.Group, d3d12RootParam};
    }

    ParamsMgr.m_pRootTables    = NumRootTables != 0 ? pRootTables : nullptr;
    ParamsMgr.m_pRootViews     = NumRootViews != 0 ? pRootViews : nullptr;
    ParamsMgr.m_pRootConstants = NumRootConstants != 0 ? pRootConstants : nullptr;

#ifdef DILIGENT_DEBUG
    ParamsMgr.Validate();
#endif
}

} // namespace Diligent
