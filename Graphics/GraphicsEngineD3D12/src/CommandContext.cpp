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

#include "CommandContext.hpp"

#include "d3dx12_win.h"

#include "RenderDeviceD3D12Impl.hpp"
#include "TextureD3D12Impl.hpp"
#include "BufferD3D12Impl.hpp"
#include "BottomLevelASD3D12Impl.hpp"
#include "TopLevelASD3D12Impl.hpp"

#include "CommandListManager.hpp"
#include "D3D12TypeConversions.hpp"


namespace Diligent
{

CommandContext::CommandContext(CommandListManager& CmdListManager) :
    m_PendingResourceBarriers(STD_ALLOCATOR_RAW_MEM(D3D12_RESOURCE_BARRIER, GetRawAllocator(), "Allocator for vector<D3D12_RESOURCE_BARRIER>"))
{
    m_PendingResourceBarriers.reserve(32);
    CmdListManager.CreateNewCommandList(&m_pCommandList, &m_pCurrentAllocator, m_MaxInterfaceVer);
}

CommandContext::~CommandContext(void)
{
    DEV_CHECK_ERR(m_pCurrentAllocator == nullptr, "Command allocator must be released prior to destroying the command context");
}

void CommandContext::Reset(CommandListManager& CmdListManager)
{
    // We only call Reset() on previously freed contexts. The command list persists, but we need to
    // request a new allocator
    VERIFY_EXPR(m_pCommandList != nullptr);
    if (!m_pCurrentAllocator)
    {
        CmdListManager.RequestAllocator(&m_pCurrentAllocator);
        // Unlike ID3D12CommandAllocator::Reset, ID3D12GraphicsCommandList::Reset can be called while the
        // command list is still being executed. A typical pattern is to submit a command list and then
        // immediately reset it to reuse the allocated memory for another command list.
        m_pCommandList->Reset(m_pCurrentAllocator, nullptr);
    }

    m_pCurPipelineState         = nullptr;
    m_pCurGraphicsRootSignature = nullptr;
    m_pCurComputeRootSignature  = nullptr;
    m_PendingResourceBarriers.clear();
    m_BoundDescriptorHeaps = ShaderDescriptorHeaps{};

    m_DynamicGPUDescriptorAllocators = nullptr;

    m_PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
#if 0
    BindDescriptorHeaps();
#endif
}

ID3D12GraphicsCommandList* CommandContext::Close(CComPtr<ID3D12CommandAllocator>& pAllocator)
{
    FlushResourceBarriers();

    //if (m_ID.length() > 0)
    //	EngineProfiling::EndBlock(this);

    VERIFY_EXPR(m_pCurrentAllocator != nullptr);
    auto hr = m_pCommandList->Close();
    DEV_CHECK_ERR(SUCCEEDED(hr), "Failed to close the command list");

    pAllocator = std::move(m_pCurrentAllocator);
    return m_pCommandList;
}

void CommandContext::TransitionResource(TextureD3D12Impl& TexD3D12, RESOURCE_STATE NewState)
{
    VERIFY(TexD3D12.IsInKnownState(), "Texture state can't be unknown");
    TransitionResource(TexD3D12, StateTransitionDesc{&TexD3D12, RESOURCE_STATE_UNKNOWN, NewState, true});
}

void CommandContext::TransitionResource(BufferD3D12Impl& BuffD3D12, RESOURCE_STATE NewState)
{
    VERIFY(BuffD3D12.IsInKnownState(), "Buffer state can't be unknown");
    TransitionResource(BuffD3D12, StateTransitionDesc{&BuffD3D12, RESOURCE_STATE_UNKNOWN, NewState, true});
}

void CommandContext::TransitionResource(BottomLevelASD3D12Impl& BlasD3D12, RESOURCE_STATE NewState)
{
    VERIFY(BlasD3D12.IsInKnownState(), "BLAS state can't be unknown");
    TransitionResource(BlasD3D12, StateTransitionDesc{&BlasD3D12, RESOURCE_STATE_UNKNOWN, NewState, true});
}

void CommandContext::TransitionResource(TopLevelASD3D12Impl& TlasD3D12, RESOURCE_STATE NewState)
{
    VERIFY(TlasD3D12.IsInKnownState(), "TLAS state can't be unknown");
    TransitionResource(TlasD3D12, StateTransitionDesc{&TlasD3D12, RESOURCE_STATE_UNKNOWN, NewState, true});
}

namespace
{

D3D12_RESOURCE_BARRIER_FLAGS TransitionTypeToD3D12ResourceBarrierFlag(STATE_TRANSITION_TYPE TransitionType)
{
    switch (TransitionType)
    {
        case STATE_TRANSITION_TYPE_IMMEDIATE:
            return D3D12_RESOURCE_BARRIER_FLAG_NONE;

        case STATE_TRANSITION_TYPE_BEGIN:
            return D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;

        case STATE_TRANSITION_TYPE_END:
            return D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;

        default:
            UNEXPECTED("Unexpected state transition type");
            return D3D12_RESOURCE_BARRIER_FLAG_NONE;
    }
}

class StateTransitionHelper
{
public:
    StateTransitionHelper(const StateTransitionDesc&                                                       Barrier,
                          std::vector<D3D12_RESOURCE_BARRIER, STDAllocatorRawMem<D3D12_RESOURCE_BARRIER>>& d3d12PendingBarriers,
                          D3D12_COMMAND_LIST_TYPE                                                          CmdListType) :
        m_Barrier{Barrier},
        m_d3d12PendingBarriers{d3d12PendingBarriers},
        m_ResStateMask{GetSupportedD3D12ResourceStatesForCommandList(CmdListType)}
    {
        DEV_CHECK_ERR(m_Barrier.NewState != RESOURCE_STATE_UNKNOWN, "New resource state can't be unknown");
    }

    template <typename ResourceType>
    void GetD3D12ResourceAndState(ResourceType& Resource);

    template <> void GetD3D12ResourceAndState<BufferD3D12Impl>(BufferD3D12Impl& Buffer);

    void AddD3D12ResourceBarriers(TextureD3D12Impl& Tex, D3D12_RESOURCE_BARRIER& d3d12Barrier);
    void AddD3D12ResourceBarriers(BufferD3D12Impl& Buff, D3D12_RESOURCE_BARRIER& d3d12Barrier);
    void AddD3D12ResourceBarriers(TopLevelASD3D12Impl& TLAS, D3D12_RESOURCE_BARRIER& d3d12Barrier);
    void AddD3D12ResourceBarriers(BottomLevelASD3D12Impl& BLAS, D3D12_RESOURCE_BARRIER& d3d12Barrier);

    template <typename ResourceType>
    void operator()(ResourceType& Resource);

private:
    const StateTransitionDesc& m_Barrier;

    std::vector<D3D12_RESOURCE_BARRIER, STDAllocatorRawMem<D3D12_RESOURCE_BARRIER>>& m_d3d12PendingBarriers;

    RESOURCE_STATE  m_OldState       = RESOURCE_STATE_UNKNOWN;
    ID3D12Resource* m_pd3d12Resource = nullptr;

    bool m_RequireUAVBarrier = false;

    const D3D12_RESOURCE_STATES m_ResStateMask;
};

template <typename ResourceType>
void StateTransitionHelper::GetD3D12ResourceAndState(ResourceType& Resource)
{
    VERIFY_EXPR(m_Barrier.pResource == &Resource);
    m_OldState       = Resource.GetState();
    m_pd3d12Resource = Resource.GetD3D12Resource();
}

template <>
void StateTransitionHelper::GetD3D12ResourceAndState<BufferD3D12Impl>(BufferD3D12Impl& Buffer)
{
    VERIFY_EXPR(m_Barrier.pResource == &Buffer);
#ifdef DILIGENT_DEVELOPMENT
    // Dynamic buffers that have no backing d3d12 resource are suballocated in the upload heap
    // when Map() is called and must always be in D3D12_RESOURCE_STATE_GENERIC_READ state.
    if (Buffer.GetDesc().Usage == USAGE_DYNAMIC && Buffer.GetD3D12Resource() == nullptr)
    {
        DEV_CHECK_ERR(Buffer.GetState() == RESOURCE_STATE_GENERIC_READ, "Dynamic buffers that have no backing d3d12 resource are expected to always be in D3D12_RESOURCE_STATE_GENERIC_READ state");
        VERIFY((m_Barrier.NewState & RESOURCE_STATE_GENERIC_READ) == m_Barrier.NewState, "Dynamic buffers can only transition to one of RESOURCE_STATE_GENERIC_READ states");
    }
#endif

    m_OldState       = Buffer.GetState();
    m_pd3d12Resource = Buffer.GetD3D12Resource();
}



void StateTransitionHelper::AddD3D12ResourceBarriers(TextureD3D12Impl& Tex, D3D12_RESOURCE_BARRIER& d3d12Barrier)
{
    // Note that RESOURCE_STATE_UNDEFINED != RESOURCE_STATE_PRESENT, but
    // D3D12_RESOURCE_STATE_COMMON == D3D12_RESOURCE_STATE_PRESENT
    if (d3d12Barrier.Transition.StateBefore != d3d12Barrier.Transition.StateAfter)
    {
        const auto& TexDesc = Tex.GetDesc();
        VERIFY(m_Barrier.FirstMipLevel < TexDesc.MipLevels, "First mip level is out of range");
        VERIFY(m_Barrier.MipLevelsCount == REMAINING_MIP_LEVELS || m_Barrier.FirstMipLevel + m_Barrier.MipLevelsCount <= TexDesc.MipLevels,
               "Invalid mip level range");
        VERIFY(m_Barrier.FirstArraySlice < TexDesc.ArraySize, "First array slice is out of range");
        VERIFY(m_Barrier.ArraySliceCount == REMAINING_ARRAY_SLICES || m_Barrier.FirstArraySlice + m_Barrier.ArraySliceCount <= TexDesc.ArraySize,
               "Invalid array slice range");

        if (m_Barrier.FirstMipLevel == 0 && (m_Barrier.MipLevelsCount == REMAINING_MIP_LEVELS || m_Barrier.MipLevelsCount == TexDesc.MipLevels) &&
            m_Barrier.FirstArraySlice == 0 && (m_Barrier.ArraySliceCount == REMAINING_ARRAY_SLICES || m_Barrier.ArraySliceCount == TexDesc.ArraySize))
        {
            d3d12Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            m_d3d12PendingBarriers.emplace_back(d3d12Barrier);
        }
        else
        {
            Uint32 EndMip   = m_Barrier.MipLevelsCount == REMAINING_MIP_LEVELS ? TexDesc.MipLevels : m_Barrier.FirstMipLevel + m_Barrier.MipLevelsCount;
            Uint32 EndSlice = m_Barrier.ArraySliceCount == REMAINING_ARRAY_SLICES ? TexDesc.ArraySize : m_Barrier.FirstArraySlice + m_Barrier.ArraySliceCount;
            for (Uint32 mip = m_Barrier.FirstMipLevel; mip < EndMip; ++mip)
            {
                for (Uint32 slice = m_Barrier.FirstArraySlice; slice < EndSlice; ++slice)
                {
                    d3d12Barrier.Transition.Subresource = D3D12CalcSubresource(mip, slice, 0, TexDesc.MipLevels, TexDesc.ArraySize);
                    m_d3d12PendingBarriers.emplace_back(d3d12Barrier);
                }
            }
        }
    }
}

void StateTransitionHelper::AddD3D12ResourceBarriers(BufferD3D12Impl& Buff, D3D12_RESOURCE_BARRIER& d3d12Barrier)
{
    if (d3d12Barrier.Transition.StateBefore != d3d12Barrier.Transition.StateAfter)
    {
        m_d3d12PendingBarriers.emplace_back(d3d12Barrier);
    }
}

void StateTransitionHelper::AddD3D12ResourceBarriers(TopLevelASD3D12Impl& TLAS, D3D12_RESOURCE_BARRIER& /*d3d12Barrier*/)
{
    // Acceleration structure is always in D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
    // and requires UAV barrier instead of state transition.
    // If either old or new state is a write state, we need to issue a UAV barrier to complete
    // all previous read/write operations.
    if (m_OldState == RESOURCE_STATE_BUILD_AS_WRITE || m_Barrier.NewState == RESOURCE_STATE_BUILD_AS_WRITE)
        m_RequireUAVBarrier = true;

#ifdef DILIGENT_DEVELOPMENT
    if (m_Barrier.NewState & RESOURCE_STATE_RAY_TRACING)
    {
        TLAS.ValidateContent();
    }
#endif
}

void StateTransitionHelper::AddD3D12ResourceBarriers(BottomLevelASD3D12Impl& BLAS, D3D12_RESOURCE_BARRIER& /*d3d12Barrier*/)
{
    // Acceleration structure is always in D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
    // and requires UAV barrier instead of state transition.
    // If either old or new state is a write state, we need to issue a UAV barrier to complete
    // all previous read/write operations.
    if (m_OldState == RESOURCE_STATE_BUILD_AS_WRITE || m_Barrier.NewState == RESOURCE_STATE_BUILD_AS_WRITE)
        m_RequireUAVBarrier = true;
}

template <typename ResourceType>
void StateTransitionHelper::operator()(ResourceType& Resource)
{
    GetD3D12ResourceAndState(Resource);

    if (m_OldState == RESOURCE_STATE_UNKNOWN)
    {
        DEV_CHECK_ERR(m_Barrier.OldState != RESOURCE_STATE_UNKNOWN, "When resource state is unknown (which means it is managed by the app), OldState member must not be RESOURCE_STATE_UNKNOWN");
        m_OldState = m_Barrier.OldState;
    }
    else
    {
        DEV_CHECK_ERR(m_Barrier.OldState == RESOURCE_STATE_UNKNOWN || m_Barrier.OldState == m_OldState,
                      "Resource state is known (", GetResourceStateString(m_OldState), ") and does not match the OldState (",
                      GetResourceStateString(m_Barrier.OldState),
                      ") specified in the resource barrier. Set OldState member to RESOURCE_STATE_UNKNOWN "
                      "to make the engine use current resource state");
    }

    // RESOURCE_STATE_UNORDERED_ACCESS and RESOURCE_STATE_BUILD_AS_WRITE are converted to D3D12_RESOURCE_STATE_UNORDERED_ACCESS.
    // UAV barrier must be inserted between D3D12_RESOURCE_STATE_UNORDERED_ACCESS resource usages.
    m_RequireUAVBarrier =
        (m_OldState == RESOURCE_STATE_UNORDERED_ACCESS || m_OldState == RESOURCE_STATE_BUILD_AS_WRITE) &&
        (m_Barrier.NewState == RESOURCE_STATE_UNORDERED_ACCESS || m_Barrier.NewState == RESOURCE_STATE_BUILD_AS_WRITE);

    // Check if required state is already set
    if ((m_OldState & m_Barrier.NewState) != m_Barrier.NewState)
    {
        auto NewState = m_Barrier.NewState;
        // If both old state and new state are read-only states, combine the two
        if ((m_OldState & RESOURCE_STATE_GENERIC_READ) == m_OldState &&
            (NewState & RESOURCE_STATE_GENERIC_READ) == NewState)
            NewState |= m_OldState;

        D3D12_RESOURCE_BARRIER d3d12Barrier;
        d3d12Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        d3d12Barrier.Flags                  = TransitionTypeToD3D12ResourceBarrierFlag(m_Barrier.TransitionType);
        d3d12Barrier.Transition.pResource   = m_pd3d12Resource;
        d3d12Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        d3d12Barrier.Transition.StateBefore = ResourceStateFlagsToD3D12ResourceStates(m_OldState) & m_ResStateMask;
        d3d12Barrier.Transition.StateAfter  = ResourceStateFlagsToD3D12ResourceStates(NewState) & m_ResStateMask;

        AddD3D12ResourceBarriers(Resource, d3d12Barrier);

        if (m_Barrier.UpdateResourceState)
        {
            VERIFY(m_Barrier.TransitionType == STATE_TRANSITION_TYPE_IMMEDIATE || m_Barrier.TransitionType == STATE_TRANSITION_TYPE_END,
                   "Resource state can't be updated in begin-split barrier");
            Resource.SetState(NewState);
        }
    }

    if (m_RequireUAVBarrier)
    {
        // UAV barrier indicates that all UAV accesses (reads or writes) to a particular resource
        // must complete before any future UAV accesses (reads or writes) can begin.

        DEV_CHECK_ERR(m_Barrier.TransitionType == STATE_TRANSITION_TYPE_IMMEDIATE, "UAV barriers must not be split");
        m_d3d12PendingBarriers.emplace_back(D3D12_RESOURCE_BARRIER{D3D12_RESOURCE_BARRIER_TYPE_UAV, D3D12_RESOURCE_BARRIER_FLAG_NONE});
        m_d3d12PendingBarriers.back().UAV.pResource = m_pd3d12Resource;
    }
}

} // namespace

void CommandContext::TransitionResource(TextureD3D12Impl& Texture, const StateTransitionDesc& Barrier)
{
    StateTransitionHelper Helper{Barrier, m_PendingResourceBarriers, GetCommandListType()};
    Helper(Texture);
}

void CommandContext::TransitionResource(BufferD3D12Impl& Buffer, const StateTransitionDesc& Barrier)
{
    StateTransitionHelper Helper{Barrier, m_PendingResourceBarriers, GetCommandListType()};
    Helper(Buffer);
}

void CommandContext::TransitionResource(BottomLevelASD3D12Impl& BLAS, const StateTransitionDesc& Barrier)
{
    StateTransitionHelper Helper{Barrier, m_PendingResourceBarriers, GetCommandListType()};
    Helper(BLAS);
}

void CommandContext::TransitionResource(TopLevelASD3D12Impl& TLAS, const StateTransitionDesc& Barrier)
{
    StateTransitionHelper Helper{Barrier, m_PendingResourceBarriers, GetCommandListType()};
    Helper(TLAS);
}

void CommandContext::InsertAliasBarrier(D3D12ResourceBase& Before, D3D12ResourceBase& After, bool FlushImmediate)
{
    m_PendingResourceBarriers.emplace_back();
    D3D12_RESOURCE_BARRIER& BarrierDesc = m_PendingResourceBarriers.back();

    BarrierDesc.Type                     = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    BarrierDesc.Flags                    = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    BarrierDesc.Aliasing.pResourceBefore = Before.GetD3D12Resource();
    BarrierDesc.Aliasing.pResourceAfter  = After.GetD3D12Resource();

    if (FlushImmediate)
        FlushResourceBarriers();
}

} // namespace Diligent
