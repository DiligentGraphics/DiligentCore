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
#include "ShaderBindingTableVkImpl.hpp"
#include "BufferVkImpl.hpp"
#include "VulkanTypeConversions.hpp"

namespace Diligent
{

ShaderBindingTableVkImpl::ShaderBindingTableVkImpl(IReferenceCounters*           pRefCounters,
                                                   RenderDeviceVkImpl*           pRenderDeviceVk,
                                                   const ShaderBindingTableDesc& Desc,
                                                   bool                          bIsDeviceInternal) :
    TShaderBindingTableBase{pRefCounters, pRenderDeviceVk, Desc, bIsDeviceInternal}
{
}

ShaderBindingTableVkImpl::~ShaderBindingTableVkImpl()
{
}

void ShaderBindingTableVkImpl::ResetHitGroups(Uint32 HitShadersPerInstance)
{
    // AZ TODO

    m_Changed = true;
}

void ShaderBindingTableVkImpl::BindAll(const BindAllAttribs& Attribs)
{
    // AZ TODO
}

void ShaderBindingTableVkImpl::GetVkStridedBufferRegions(IDeviceContextVk*              pContext,
                                                         RESOURCE_STATE_TRANSITION_MODE TransitionMode,
                                                         VkStridedBufferRegionKHR&      RaygenShaderBindingTable,
                                                         VkStridedBufferRegionKHR&      MissShaderBindingTable,
                                                         VkStridedBufferRegionKHR&      HitShaderBindingTable,
                                                         VkStridedBufferRegionKHR&      CallableShaderBindingTable)
{
    const auto ShaderGroupBaseAlignment = GetDevice()->GetPhysicalDevice().GetExtProperties().RayTracing.shaderGroupBaseAlignment;

    const auto AlignToLarger = [ShaderGroupBaseAlignment](size_t offset) -> Uint32 {
        return Align(static_cast<Uint32>(offset), ShaderGroupBaseAlignment);
    };

    const Uint32 RayGenOffset          = 0;
    const Uint32 MissShaderOffset      = AlignToLarger(m_RayGenShaderRecord.size());
    const Uint32 HitGroupOffset        = AlignToLarger(MissShaderOffset + m_MissShadersRecord.size());
    const Uint32 CallableShadersOffset = AlignToLarger(HitGroupOffset + m_HitGroupsRecord.size());
    const Uint32 BufSize               = AlignToLarger(CallableShadersOffset + m_CallableShadersRecord.size());

    // recreate buffer
    if (m_pBuffer == nullptr || m_pBuffer->GetDesc().uiSizeInBytes < BufSize)
    {
        m_pBuffer = nullptr;

        String     BuffName = String{GetDesc().Name} + " - internal buffer";
        BufferDesc BuffDesc;
        BuffDesc.Name          = BuffName.c_str();
        BuffDesc.Usage         = USAGE_DEFAULT;
        BuffDesc.BindFlags     = BIND_RAY_TRACING;
        BuffDesc.uiSizeInBytes = BufSize;

        GetDevice()->CreateBuffer(BuffDesc, nullptr, &m_pBuffer);
        VERIFY_EXPR(m_pBuffer != nullptr);
    }

    if (m_pBuffer == nullptr)
        return; // something goes wrong

    VkBuffer BuffHandle = m_pBuffer.RawPtr<BufferVkImpl>()->GetVkBuffer();

    if (m_RayGenShaderRecord.size())
    {
        RaygenShaderBindingTable.buffer = BuffHandle;
        RaygenShaderBindingTable.offset = RayGenOffset;
        RaygenShaderBindingTable.size   = m_RayGenShaderRecord.size();
        RaygenShaderBindingTable.stride = m_ShaderRecordStride;
    }

    if (m_MissShadersRecord.size())
    {
        MissShaderBindingTable.buffer = BuffHandle;
        MissShaderBindingTable.offset = MissShaderOffset;
        MissShaderBindingTable.size   = m_MissShadersRecord.size();
        MissShaderBindingTable.stride = m_ShaderRecordStride;
    }

    if (m_HitGroupsRecord.size())
    {
        HitShaderBindingTable.buffer = BuffHandle;
        HitShaderBindingTable.offset = HitGroupOffset;
        HitShaderBindingTable.size   = m_HitGroupsRecord.size();
        HitShaderBindingTable.stride = m_ShaderRecordStride;
    }

    if (m_CallableShadersRecord.size())
    {
        CallableShaderBindingTable.buffer = BuffHandle;
        CallableShaderBindingTable.offset = CallableShadersOffset;
        CallableShaderBindingTable.size   = m_CallableShadersRecord.size();
        CallableShaderBindingTable.stride = m_ShaderRecordStride;
    }

    if (!m_Changed)
        return;

    m_Changed = false;

    // update buffer data
    if (m_RayGenShaderRecord.size())
        pContext->UpdateBuffer(m_pBuffer, RayGenOffset, static_cast<Uint32>(m_RayGenShaderRecord.size()), m_RayGenShaderRecord.data(), TransitionMode);

    if (m_MissShadersRecord.size())
        pContext->UpdateBuffer(m_pBuffer, MissShaderOffset, static_cast<Uint32>(m_MissShadersRecord.size()), m_MissShadersRecord.data(), TransitionMode);

    if (m_HitGroupsRecord.size())
        pContext->UpdateBuffer(m_pBuffer, HitGroupOffset, static_cast<Uint32>(m_HitGroupsRecord.size()), m_HitGroupsRecord.data(), TransitionMode);

    if (m_CallableShadersRecord.size())
        pContext->UpdateBuffer(m_pBuffer, CallableShadersOffset, static_cast<Uint32>(m_CallableShadersRecord.size()), m_CallableShadersRecord.data(), TransitionMode);

    if (TransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
    {
        StateTransitionDesc Barrier;
        Barrier.pResource           = m_pBuffer;
        Barrier.NewState            = RESOURCE_STATE_RAY_TRACING;
        Barrier.UpdateResourceState = true;
        pContext->TransitionResourceStates(1, &Barrier);
    }
    else if (TransitionMode == RESOURCE_STATE_TRANSITION_MODE_VERIFY)
    {
        VERIFY_EXPR(m_pBuffer->GetState() == RESOURCE_STATE_RAY_TRACING);
    }
}

} // namespace Diligent
