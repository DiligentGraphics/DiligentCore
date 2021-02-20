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

#include "ShaderResourceCacheD3D12.hpp"
#include "BufferD3D12Impl.hpp"
#include "CommandContext.hpp"
#include "BufferD3D12Impl.hpp"
#include "BufferViewD3D12Impl.hpp"
#include "TextureD3D12Impl.hpp"
#include "TextureViewD3D12Impl.hpp"
#include "TopLevelASD3D12Impl.hpp"

namespace Diligent
{

size_t ShaderResourceCacheD3D12::GetRequiredMemorySize(Uint32       NumTables,
                                                       const Uint32 TableSizes[])
{
    size_t MemorySize = NumTables * sizeof(RootTable);
    for (Uint32 t = 0; t < NumTables; ++t)
        MemorySize += TableSizes[t] * sizeof(Resource);
    return MemorySize;
}

// http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-cache#Cache-Structure
void ShaderResourceCacheD3D12::Initialize(IMemoryAllocator& MemAllocator, Uint32 NumTables, const Uint32 TableSizes[])
{
    // Memory layout:
    //                                         __________________________________________________________
    //  m_pMemory                             |             m_pResources, m_NumResources                 |
    //  |                                     |                                                          |
    //  V                                     |                                                          V
    //  |  RootTable[0]  |   ....    |  RootTable[Nrt-1]  |  Res[0]  |  ... |  Res[n-1]  |    ....     | Res[0]  |  ... |  Res[m-1]  |
    //       |                                                A
    //       |                                                |
    //       |________________________________________________|
    //                    m_pResources, m_NumResources
    //

    VERIFY(m_pAllocator == nullptr && m_pMemory == nullptr, "Cache already initialized");
    m_pAllocator = &MemAllocator;
    m_NumTables  = NumTables;

    m_TotalResourceCount = 0;
    for (Uint32 t = 0; t < NumTables; ++t)
        m_TotalResourceCount += TableSizes[t];
    const auto MemorySize = NumTables * sizeof(RootTable) + m_TotalResourceCount * sizeof(Resource);
    VERIFY_EXPR(MemorySize == GetRequiredMemorySize(NumTables, TableSizes));
    if (MemorySize > 0)
    {
        m_pMemory         = ALLOCATE_RAW(*m_pAllocator, "Memory for shader resource cache data", MemorySize);
        auto* pTables     = reinterpret_cast<RootTable*>(m_pMemory);
        auto* pCurrResPtr = reinterpret_cast<Resource*>(pTables + m_NumTables);
        for (Uint32 res = 0; res < m_TotalResourceCount; ++res)
            new (pCurrResPtr + res) Resource{};

        for (Uint32 t = 0; t < NumTables; ++t)
        {
            new (&GetRootTable(t)) RootTable{TableSizes[t], TableSizes[t] > 0 ? pCurrResPtr : nullptr};
            pCurrResPtr += TableSizes[t];
        }
        VERIFY_EXPR((char*)pCurrResPtr == (char*)m_pMemory + MemorySize);
    }
}

ShaderResourceCacheD3D12::~ShaderResourceCacheD3D12()
{
    if (m_pMemory)
    {
        for (Uint32 res = 0; res < m_TotalResourceCount; ++res)
            GetResource(res).~Resource();
        for (Uint32 t = 0; t < m_NumTables; ++t)
            GetRootTable(t).~RootTable();

        m_pAllocator->Free(m_pMemory);
    }
}


void ShaderResourceCacheD3D12::Resource::TransitionResource(CommandContext& Ctx)
{
    static_assert(SHADER_RESOURCE_TYPE_LAST == 8, "Please update this function to handle the new resource type");
    switch (Type)
    {
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
        {
            // No need to use QueryInterface() - types are verified when resources are bound
            auto* pBuffToTransition = pObject.RawPtr<BufferD3D12Impl>();
            if (pBuffToTransition->IsInKnownState() && !pBuffToTransition->CheckState(RESOURCE_STATE_CONSTANT_BUFFER))
                Ctx.TransitionResource(*pBuffToTransition, RESOURCE_STATE_CONSTANT_BUFFER);
        }
        break;

        case SHADER_RESOURCE_TYPE_BUFFER_SRV:
        {
            auto* pBuffViewD3D12    = pObject.RawPtr<BufferViewD3D12Impl>();
            auto* pBuffToTransition = pBuffViewD3D12->GetBuffer<BufferD3D12Impl>();
            if (pBuffToTransition->IsInKnownState() && !pBuffToTransition->CheckState(RESOURCE_STATE_SHADER_RESOURCE))
                Ctx.TransitionResource(*pBuffToTransition, RESOURCE_STATE_SHADER_RESOURCE);
        }
        break;

        case SHADER_RESOURCE_TYPE_BUFFER_UAV:
        {
            auto* pBuffViewD3D12    = pObject.RawPtr<BufferViewD3D12Impl>();
            auto* pBuffToTransition = pBuffViewD3D12->GetBuffer<BufferD3D12Impl>();
            if (pBuffToTransition->IsInKnownState())
            {
                // We must always call TransitionResource() even when the state is already
                // RESOURCE_STATE_UNORDERED_ACCESS as in this case UAV barrier must be executed
                Ctx.TransitionResource(*pBuffToTransition, RESOURCE_STATE_UNORDERED_ACCESS);
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
        {
            auto* pTexViewD3D12    = pObject.RawPtr<TextureViewD3D12Impl>();
            auto* pTexToTransition = pTexViewD3D12->GetTexture<TextureD3D12Impl>();
            if (pTexToTransition->IsInKnownState() && !pTexToTransition->CheckAnyState(RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INPUT_ATTACHMENT))
                Ctx.TransitionResource(*pTexToTransition, RESOURCE_STATE_SHADER_RESOURCE);
        }
        break;

        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
        {
            auto* pTexViewD3D12    = pObject.RawPtr<TextureViewD3D12Impl>();
            auto* pTexToTransition = pTexViewD3D12->GetTexture<TextureD3D12Impl>();
            if (pTexToTransition->IsInKnownState())
            {
                // We must always call TransitionResource() even when the state is already
                // RESOURCE_STATE_UNORDERED_ACCESS as in this case UAV barrier must be executed
                Ctx.TransitionResource(*pTexToTransition, RESOURCE_STATE_UNORDERED_ACCESS);
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_SAMPLER:
            // Nothing to transition
            break;

        case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
        {
            auto* pTlasD3D12 = pObject.RawPtr<TopLevelASD3D12Impl>();
            if (pTlasD3D12->IsInKnownState())
                Ctx.TransitionResource(*pTlasD3D12, RESOURCE_STATE_RAY_TRACING);
        }
        break;

        default:
            // Resource is not bound
            VERIFY(Type == SHADER_RESOURCE_TYPE_UNKNOWN, "Unexpected resource type");
            VERIFY(pObject == nullptr && CPUDescriptorHandle.ptr == 0, "Bound resource is unexpected");
    }
}


#ifdef DILIGENT_DEVELOPMENT
void ShaderResourceCacheD3D12::Resource::DvpVerifyResourceState()
{
    static_assert(SHADER_RESOURCE_TYPE_LAST == 8, "Please update this function to handle the new resource type");
    switch (Type)
    {
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
        {
            // Not using QueryInterface() for the sake of efficiency
            const auto* pBufferD3D12 = pObject.RawPtr<const BufferD3D12Impl>();
            if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_CONSTANT_BUFFER))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBufferD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_CONSTANT_BUFFER state. Actual state: ",
                                  GetResourceStateString(pBufferD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the buffer state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_BUFFER_SRV:
        {
            const auto* pBuffViewD3D12 = pObject.RawPtr<const BufferViewD3D12Impl>();
            const auto* pBufferD3D12   = pBuffViewD3D12->GetBuffer<const BufferD3D12Impl>();
            if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_SHADER_RESOURCE))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBufferD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_SHADER_RESOURCE state.  Actual state: ",
                                  GetResourceStateString(pBufferD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the buffer state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_BUFFER_UAV:
        {
            const auto* pBuffViewD3D12 = pObject.RawPtr<const BufferViewD3D12Impl>();
            const auto* pBufferD3D12   = pBuffViewD3D12->GetBuffer<const BufferD3D12Impl>();
            if (pBufferD3D12->IsInKnownState() && !pBufferD3D12->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                LOG_ERROR_MESSAGE("Buffer '", pBufferD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_UNORDERED_ACCESS state. Actual state: ",
                                  GetResourceStateString(pBufferD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the buffer state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
        {
            const auto* pTexViewD3D12 = pObject.RawPtr<const TextureViewD3D12Impl>();
            const auto* pTexD3D12     = pTexViewD3D12->GetTexture<TextureD3D12Impl>();
            if (pTexD3D12->IsInKnownState() && !pTexD3D12->CheckAnyState(RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INPUT_ATTACHMENT))
            {
                LOG_ERROR_MESSAGE("Texture '", pTexD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_SHADER_RESOURCE state. Actual state: ",
                                  GetResourceStateString(pTexD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the texture state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
        {
            const auto* pTexViewD3D12 = pObject.RawPtr<const TextureViewD3D12Impl>();
            const auto* pTexD3D12     = pTexViewD3D12->GetTexture<const TextureD3D12Impl>();
            if (pTexD3D12->IsInKnownState() && !pTexD3D12->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                LOG_ERROR_MESSAGE("Texture '", pTexD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_UNORDERED_ACCESS state. Actual state: ",
                                  GetResourceStateString(pTexD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the texture state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        case SHADER_RESOURCE_TYPE_SAMPLER:
            // No resource
            break;

        case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
        {
            const auto* pTLASD3D12 = pObject.RawPtr<const TopLevelASD3D12Impl>();
            if (pTLASD3D12->IsInKnownState() && !pTLASD3D12->CheckState(RESOURCE_STATE_RAY_TRACING))
            {
                LOG_ERROR_MESSAGE("TLAS '", pTLASD3D12->GetDesc().Name, "' must be in RESOURCE_STATE_RAY_TRACING state.  Actual state: ",
                                  GetResourceStateString(pTLASD3D12->GetState()),
                                  ". Call IDeviceContext::TransitionShaderResources(), use RESOURCE_STATE_TRANSITION_MODE_TRANSITION "
                                  "when calling IDeviceContext::CommitShaderResources() or explicitly transition the TLAS state "
                                  "with IDeviceContext::TransitionResourceStates().");
            }
        }
        break;

        default:
            // Resource is not bound
            VERIFY(Type == SHADER_RESOURCE_TYPE_UNKNOWN, "Unexpected resource type");
            VERIFY(pObject == nullptr && CPUDescriptorHandle.ptr == 0, "Bound resource is unexpected");
    }
}
#endif // DILIGENT_DEVELOPMENT

void ShaderResourceCacheD3D12::TransitionResources(CommandContext& Ctx,
                                                   bool            PerformTransitions,
                                                   bool            ValidateStates)
{
    for (Uint32 r = 0; r < m_TotalResourceCount; ++r)
    {
        auto& Res = GetResource(r);
        if (PerformTransitions)
        {
            Res.TransitionResource(Ctx);
        }
#ifdef DILIGENT_DEVELOPMENT
        else if (ValidateStates)
        {
            Res.DvpVerifyResourceState();
        }
#endif
    }
}



#ifdef DILIGENT_DEBUG
//void ShaderResourceCacheD3D12::DbgVerifyBoundDynamicCBsCounter() const
//{
//    Uint32 NumDynamicCBsBound = 0;
//    for (Uint32 t = 0; t < m_NumTables; ++t)
//    {
//        const auto& RT = GetRootTable(t);
//        for (Uint32 res = 0; res < RT.GetSize(); ++res)
//        {
//            const auto& Res = RT.GetResource(res);
//            if (Res.Type == CachedResourceType::CBV && Res.pObject && Res.pObject.RawPtr<const BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC)
//                ++NumDynamicCBsBound;
//        }
//    }
//    VERIFY(NumDynamicCBsBound == m_NumDynamicCBsBound, "The number of dynamic CBs bound is invalid");
//}
#endif

} // namespace Diligent
