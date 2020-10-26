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
#include "ShaderBindingTableD3D12Impl.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "DeviceContextD3D12Impl.hpp"
#include "D3D12TypeConversions.hpp"
#include "GraphicsAccessories.hpp"
#include "DXGITypeConversions.hpp"
#include "EngineMemory.h"
#include "StringTools.hpp"

namespace Diligent
{

ShaderBindingTableD3D12Impl::ShaderBindingTableD3D12Impl(IReferenceCounters*           pRefCounters,
                                                         class RenderDeviceD3D12Impl*  pDeviceD3D12,
                                                         const ShaderBindingTableDesc& Desc,
                                                         bool                          bIsDeviceInternal) :
    TShaderBindingTableBase{pRefCounters, pDeviceD3D12, Desc, bIsDeviceInternal}
{
    ValidateDesc(Desc);

    m_ShaderRecordStride = m_Desc.ShaderRecordSize + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
}

ShaderBindingTableD3D12Impl::~ShaderBindingTableD3D12Impl()
{
}

IMPLEMENT_QUERY_INTERFACE(ShaderBindingTableD3D12Impl, IID_ShaderBindingTableD3D12, TShaderBindingTableBase)

void ShaderBindingTableD3D12Impl::ValidateDesc(const ShaderBindingTableDesc& Desc) const
{
    if (Desc.ShaderRecordSize + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES > D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE)
    {
        LOG_ERROR_AND_THROW("Description of Shader binding table '", (Desc.Name ? Desc.Name : ""),
                            "' is invalid: ShaderRecordSize is too big, max size is: ", D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
}

void ShaderBindingTableD3D12Impl::Verify() const
{
    // AZ TODO
}

void ShaderBindingTableD3D12Impl::Reset(const ShaderBindingTableDesc& Desc)
{
    m_RayGenShaderRecord.clear();
    m_MissShadersRecord.clear();
    m_CallableShadersRecord.clear();
    m_HitGroupsRecord.clear();
    m_Changed = true;

    try
    {
        ValidateShaderBindingTableDesc(Desc);
        ValidateDesc(Desc);
    }
    catch (const std::runtime_error&)
    {
        // AZ TODO
        return;
    }

    m_Desc               = Desc;
    m_ShaderRecordStride = m_Desc.ShaderRecordSize + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
}

void ShaderBindingTableD3D12Impl::ResetHitGroups(Uint32 HitShadersPerInstance)
{
    // AZ TODO
}

void ShaderBindingTableD3D12Impl::BindAll(const BindAllAttribs& Attribs)
{
    // AZ TODO
}

void ShaderBindingTableD3D12Impl::GetD3D12AddressRangeAndStride(IDeviceContextD3D12*                        pContext,
                                                                RESOURCE_STATE_TRANSITION_MODE              TransitionMode,
                                                                D3D12_GPU_VIRTUAL_ADDRESS_RANGE&            RaygenShaderBindingTable,
                                                                D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE& MissShaderBindingTable,
                                                                D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE& HitShaderBindingTable,
                                                                D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE& CallableShaderBindingTable)
{
    const auto AlignToLarger = [](size_t offset) -> Uint32 {
        return Align(static_cast<Uint32>(offset), static_cast<Uint32>(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
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

    const D3D12_GPU_VIRTUAL_ADDRESS BuffHandle = m_pBuffer.RawPtr<BufferD3D12Impl>()->GetGPUAddress(0, ValidatedCast<DeviceContextD3D12Impl>(pContext));

    if (m_RayGenShaderRecord.size())
    {
        RaygenShaderBindingTable.StartAddress = BuffHandle + RayGenOffset;
        RaygenShaderBindingTable.SizeInBytes  = m_RayGenShaderRecord.size();
    }

    if (m_MissShadersRecord.size())
    {
        MissShaderBindingTable.StartAddress  = BuffHandle + MissShaderOffset;
        MissShaderBindingTable.SizeInBytes   = m_MissShadersRecord.size();
        MissShaderBindingTable.StrideInBytes = m_ShaderRecordStride;
    }

    if (m_HitGroupsRecord.size())
    {
        HitShaderBindingTable.StartAddress  = BuffHandle + HitGroupOffset;
        HitShaderBindingTable.SizeInBytes   = m_HitGroupsRecord.size();
        HitShaderBindingTable.StrideInBytes = m_ShaderRecordStride;
    }

    if (m_CallableShadersRecord.size())
    {
        CallableShaderBindingTable.StartAddress  = BuffHandle + CallableShadersOffset;
        CallableShaderBindingTable.SizeInBytes   = m_CallableShadersRecord.size();
        CallableShaderBindingTable.StrideInBytes = m_ShaderRecordStride;
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
