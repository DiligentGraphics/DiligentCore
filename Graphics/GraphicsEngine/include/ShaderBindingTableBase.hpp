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

#pragma once

/// \file
/// Implementation of the Diligent::ShaderBindingTableBase template class

#include <unordered_map>

#include "ShaderBindingTable.h"
#include "DeviceObjectBase.hpp"
#include "RenderDeviceBase.hpp"
#include "StringPool.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

/// Template class implementing base functionality for a shader binding table object.

/// \tparam BaseInterface - base interface that this class will inheret
///                          (Diligent::IShaderBindingTableD3D12 or Diligent::IShaderBindingTableVk).
/// \tparam RenderDeviceImplType - type of the render device implementation
///                                (Diligent::RenderDeviceD3D12Impl or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class PipelineStateImplType, class RenderDeviceImplType>
class ShaderBindingTableBase : public DeviceObjectBase<BaseInterface, RenderDeviceImplType, ShaderBindingTableDesc>
{
public:
    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, ShaderBindingTableDesc>;

    /// \param pRefCounters      - reference counters object that controls the lifetime of this SBT.
    /// \param pDevice           - pointer to the device.
    /// \param Desc              - SBT description.
    /// \param bIsDeviceInternal - flag indicating if the BLAS is an internal device object and
    ///							   must not keep a strong reference to the device.
    ShaderBindingTableBase(IReferenceCounters*           pRefCounters,
                           RenderDeviceImplType*         pDevice,
                           const ShaderBindingTableDesc& Desc,
                           bool                          bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
    {
        ValidateShaderBindingTableDesc(Desc);
    }

    ~ShaderBindingTableBase()
    {
    }

    void BindRayGenShader(const char* ShaderGroupName, const void* Data, Uint32 DataSize) override final
    {
        VERIFY(Data == nullptr && DataSize == 0, "not supported yet");

        m_RayGenShaderRecord.resize(m_ShaderRecordStride);
        ValidatedCast<PipelineStateImplType>(m_Desc.pPSO)->CopyShaderHandle(ShaderGroupName, m_RayGenShaderRecord.data(), m_ShaderRecordStride);
        m_Changed = true;
    }

    void BindMissShader(const char* ShaderGroupName, Uint32 MissIndex, const void* Data, Uint32 DataSize) override final
    {
        VERIFY(Data == nullptr && DataSize == 0, "not supported yet");

        const Uint32 Offset = MissIndex * m_ShaderRecordStride;
        m_MissShadersRecord.resize(std::max<size_t>(m_MissShadersRecord.size(), Offset + m_ShaderRecordStride));

        ValidatedCast<PipelineStateImplType>(m_Desc.pPSO)->CopyShaderHandle(ShaderGroupName, m_MissShadersRecord.data() + Offset, m_ShaderRecordStride);
        m_Changed = true;
    }

    void BindHitGroup(ITopLevelAS* pTLAS,
                      const char*  InstanceName,
                      const char*  GeometryName,
                      Uint32       RayOffsetInHitGroupIndex,
                      const char*  ShaderGroupName,
                      const void*  Data,
                      Uint32       DataSize) override final
    {
        VERIFY(Data == nullptr && DataSize == 0, "not supported yet");
        VERIFY_EXPR(pTLAS != nullptr);
        VERIFY_EXPR(RayOffsetInHitGroupIndex < m_Desc.HitShadersPerInstance);
        VERIFY_EXPR(pTLAS->GetDesc().BindingMode == SHADER_BINDING_MODE_PER_GEOMETRY);

        const auto Desc = pTLAS->GetInstanceDesc(InstanceName);
        VERIFY_EXPR(Desc.pBLAS != nullptr);

        const Uint32 InstanceIndex = Desc.ContributionToHitGroupIndex;
        const Uint32 GeometryIndex = Desc.pBLAS->GetGeometryIndex(GeometryName);
        const Uint32 Index         = InstanceIndex + GeometryIndex * m_Desc.HitShadersPerInstance + RayOffsetInHitGroupIndex;
        const Uint32 Offset        = Index * m_ShaderRecordStride;

        m_HitGroupsRecord.resize(std::max<size_t>(m_HitGroupsRecord.size(), Offset + m_ShaderRecordStride));

        ValidatedCast<PipelineStateImplType>(m_Desc.pPSO)->CopyShaderHandle(ShaderGroupName, m_HitGroupsRecord.data() + Offset, m_ShaderRecordStride);
        m_Changed = true;
    }

    void BindHitGroups(ITopLevelAS* pTLAS,
                       const char*  InstanceName,
                       Uint32       RayOffsetInHitGroupIndex,
                       const char*  ShaderGroupName,
                       const void*  Data,
                       Uint32       DataSize) override final
    {
        VERIFY(Data == nullptr && DataSize == 0, "not supported yet");
        VERIFY_EXPR(pTLAS != nullptr);
        VERIFY_EXPR(RayOffsetInHitGroupIndex < m_Desc.HitShadersPerInstance);
        VERIFY_EXPR(pTLAS->GetDesc().BindingMode == SHADER_BINDING_MODE_PER_GEOMETRY ||
                    pTLAS->GetDesc().BindingMode == SHADER_BINDING_MODE_PER_INSTANCE);

        const auto Desc = pTLAS->GetInstanceDesc(InstanceName);
        VERIFY_EXPR(Desc.pBLAS != nullptr);

        const Uint32           InstanceIndex = Desc.ContributionToHitGroupIndex;
        const auto&            GeometryDesc  = Desc.pBLAS->GetDesc();
        const Uint32           GeometryCount = GeometryDesc.BoxCount + GeometryDesc.TriangleCount;
        const Uint32           BeginIndex    = InstanceIndex + 0 * m_Desc.HitShadersPerInstance + RayOffsetInHitGroupIndex;
        const Uint32           EndIndex      = InstanceIndex + GeometryCount * m_Desc.HitShadersPerInstance + RayOffsetInHitGroupIndex;
        PipelineStateImplType* pPSO          = ValidatedCast<PipelineStateImplType>(m_Desc.pPSO);

        m_HitGroupsRecord.resize(std::max<size_t>(m_HitGroupsRecord.size(), EndIndex * m_ShaderRecordStride));

        for (Uint32 i = 0; i < GeometryCount; ++i)
        {
            Uint32 Offset = (BeginIndex + i) * m_ShaderRecordStride;
            pPSO->CopyShaderHandle(ShaderGroupName, m_HitGroupsRecord.data() + Offset, m_ShaderRecordStride);
        }
        m_Changed = true;
    }

    void BindCallableShader(const char* ShaderGroupName,
                            Uint32      CallableIndex,
                            const void* Data,
                            Uint32      DataSize) override final
    {
        VERIFY(Data == nullptr && DataSize == 0, "not supported yet");

        const Uint32 Offset = CallableIndex * m_ShaderRecordStride;
        m_CallableShadersRecord.resize(std::max<size_t>(m_CallableShadersRecord.size(), Offset + m_ShaderRecordStride));

        ValidatedCast<PipelineStateImplType>(m_Desc.pPSO)->CopyShaderHandle(ShaderGroupName, m_CallableShadersRecord.data() + Offset, m_ShaderRecordStride);
        m_Changed = true;
    }

protected:
    static void ValidateShaderBindingTableDesc(const ShaderBindingTableDesc& Desc)
    {
#define LOG_SBT_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of Shader binding table '", (Desc.Name ? Desc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

        if (Desc.pPSO == nullptr)
        {
            LOG_SBT_ERROR_AND_THROW("pPSO must not be null");
        }

        if (Desc.pPSO->GetDesc().PipelineType != PIPELINE_TYPE_RAY_TRACING)
        {
            LOG_SBT_ERROR_AND_THROW("pPSO must be ray tracing pipeline");
        }

#undef LOG_SBT_ERROR_AND_THROW
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ShaderBindingTable, TDeviceObjectBase)

protected:
    RefCntAutoPtr<IBuffer> m_pBuffer;
    std::vector<Uint8>     m_RayGenShaderRecord;
    std::vector<Uint8>     m_MissShadersRecord;
    std::vector<Uint8>     m_CallableShadersRecord;
    std::vector<Uint8>     m_HitGroupsRecord;

    Uint32 m_ShaderRecordStride = 0;
    bool   m_Changed            = true;
};

} // namespace Diligent
