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
#include "TopLevelASBase.hpp"
#include "DeviceObjectBase.hpp"
#include "RenderDeviceBase.hpp"
#include "StringPool.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

/// Validates SBT description and throws an exception in case of an error.
void ValidateShaderBindingTableDesc(const ShaderBindingTableDesc& Desc, Uint32 ShaderGroupHandleSize, Uint32 MaxShaderRecordStride) noexcept(false);

/// Template class implementing base functionality for a shader binding table object.

/// \tparam BaseInterface        - base interface that this class will inheret
///                                (Diligent::IShaderBindingTableD3D12 or Diligent::IShaderBindingTableVk).
/// \tparam RenderDeviceImplType - type of the render device implementation
///                                (Diligent::RenderDeviceD3D12Impl or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class PipelineStateImplType, class TopLevelASImplType, class RenderDeviceImplType>
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
        const auto& DeviceProps = this->m_pDevice->GetProperties();
        ValidateShaderBindingTableDesc(this->m_Desc, DeviceProps.ShaderGroupHandleSize, DeviceProps.MaxShaderRecordStride);

        this->m_pPSO               = ValidatedCast<PipelineStateImplType>(this->m_Desc.pPSO);
        this->m_ShaderRecordSize   = this->m_pPSO->GetRayTracingPipelineDesc().ShaderRecordSize;
        this->m_ShaderRecordStride = this->m_ShaderRecordSize + DeviceProps.ShaderGroupHandleSize;
    }

    ~ShaderBindingTableBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ShaderBindingTable, TDeviceObjectBase)


    void DILIGENT_CALL_TYPE Reset(IPipelineState* pPSO) override final
    {
#ifdef DILIGENT_DEVELOPMENT
        this->m_DbgHitGroupBindings.clear();
#endif
        this->m_RayGenShaderRecord.clear();
        this->m_MissShadersRecord.clear();
        this->m_CallableShadersRecord.clear();
        this->m_HitGroupsRecord.clear();
        this->m_Changed = true;
        this->m_pPSO    = nullptr;

        this->m_Desc.pPSO = pPSO;

        const auto& DeviceProps = this->m_pDevice->GetProperties();
        try
        {
            ValidateShaderBindingTableDesc(this->m_Desc, DeviceProps.ShaderGroupHandleSize, DeviceProps.MaxShaderRecordStride);
        }
        catch (const std::runtime_error&)
        {
            return;
        }

        this->m_pPSO               = ValidatedCast<PipelineStateImplType>(this->m_Desc.pPSO);
        this->m_ShaderRecordSize   = this->m_pPSO->GetRayTracingPipelineDesc().ShaderRecordSize;
        this->m_ShaderRecordStride = this->m_ShaderRecordSize + DeviceProps.ShaderGroupHandleSize;
    }


    void DILIGENT_CALL_TYPE ResetHitGroups() override final
    {
#ifdef DILIGENT_DEVELOPMENT
        this->m_DbgHitGroupBindings.clear();
#endif
        this->m_HitGroupsRecord.clear();
        this->m_Changed = true;
    }


    void DILIGENT_CALL_TYPE BindAll(const BindAllAttribs& Attribs) override final
    {
        // AZ TODO
    }


    void DILIGENT_CALL_TYPE BindRayGenShader(const char* pShaderGroupName, const void* pData, Uint32 DataSize) override final
    {
        VERIFY_EXPR((pData == nullptr) == (DataSize == 0));
        VERIFY_EXPR((pData == nullptr) || (DataSize == this->m_ShaderRecordSize));

        this->m_RayGenShaderRecord.resize(this->m_ShaderRecordStride, Uint8{EmptyElem});
        this->m_pPSO->CopyShaderHandle(pShaderGroupName, this->m_RayGenShaderRecord.data(), this->m_ShaderRecordStride);

        const Uint32 GroupSize = this->m_pDevice->GetProperties().ShaderGroupHandleSize;
        std::memcpy(this->m_RayGenShaderRecord.data() + GroupSize, pData, DataSize);
        this->m_Changed = true;
    }


    void DILIGENT_CALL_TYPE BindMissShader(const char* pShaderGroupName, Uint32 MissIndex, const void* pData, Uint32 DataSize) override final
    {
        VERIFY_EXPR((pData == nullptr) == (DataSize == 0));
        VERIFY_EXPR((pData == nullptr) || (DataSize == this->m_ShaderRecordSize));

        const Uint32 GroupSize = this->m_pDevice->GetProperties().ShaderGroupHandleSize;
        const size_t Stride    = this->m_ShaderRecordStride;
        const size_t Offset    = MissIndex * Stride;
        this->m_MissShadersRecord.resize(std::max(this->m_MissShadersRecord.size(), Offset + Stride), Uint8{EmptyElem});

        this->m_pPSO->CopyShaderHandle(pShaderGroupName, this->m_MissShadersRecord.data() + Offset, Stride);
        std::memcpy(this->m_MissShadersRecord.data() + Offset + GroupSize, pData, DataSize);
        this->m_Changed = true;
    }


    void DILIGENT_CALL_TYPE BindHitGroup(ITopLevelAS* pTLAS,
                                         const char*  pInstanceName,
                                         const char*  pGeometryName,
                                         Uint32       RayOffsetInHitGroupIndex,
                                         const char*  pShaderGroupName,
                                         const void*  pData,
                                         Uint32       DataSize) override final
    {
        VERIFY_EXPR((pData == nullptr) == (DataSize == 0));
        VERIFY_EXPR((pData == nullptr) || (DataSize == this->m_ShaderRecordSize));
        VERIFY_EXPR(pTLAS != nullptr);

        auto*      pTLASImpl = ValidatedCast<TopLevelASImplType>(pTLAS);
        const auto Desc      = pTLASImpl->GetInstanceDesc(pInstanceName);

        VERIFY_EXPR(pTLASImpl->GetBindingMode() == SHADER_BINDING_MODE_PER_GEOMETRY);
        VERIFY_EXPR(RayOffsetInHitGroupIndex < pTLASImpl->GetHitShadersPerInstance());
        VERIFY_EXPR(Desc.ContributionToHitGroupIndex != INVALID_INDEX);

        if (Desc.pBLAS == nullptr)
            return; // this is disabled instance

        const Uint32 InstanceIndex = Desc.ContributionToHitGroupIndex;
        const Uint32 GeometryIndex = Desc.pBLAS->GetGeometryIndex(pGeometryName);
        VERIFY_EXPR(GeometryIndex != INVALID_INDEX);

        const Uint32 Index     = InstanceIndex + GeometryIndex * pTLASImpl->GetHitShadersPerInstance() + RayOffsetInHitGroupIndex;
        const size_t Stride    = this->m_ShaderRecordStride;
        const Uint32 GroupSize = this->m_pDevice->GetProperties().ShaderGroupHandleSize;
        const size_t Offset    = Index * Stride;

        this->m_HitGroupsRecord.resize(std::max(this->m_HitGroupsRecord.size(), Offset + Stride), Uint8{EmptyElem});

        this->m_pPSO->CopyShaderHandle(pShaderGroupName, this->m_HitGroupsRecord.data() + Offset, Stride);
        std::memcpy(this->m_HitGroupsRecord.data() + Offset + GroupSize, pData, DataSize);
        this->m_Changed = true;

#ifdef DILIGENT_DEVELOPMENT
        OnBindHitGroup(pTLASImpl, Index);
#endif
    }


    void DILIGENT_CALL_TYPE BindHitGroups(ITopLevelAS* pTLAS,
                                          const char*  pInstanceName,
                                          Uint32       RayOffsetInHitGroupIndex,
                                          const char*  pShaderGroupName,
                                          const void*  pData,
                                          Uint32       DataSize) override final
    {
        VERIFY_EXPR((pData == nullptr) == (DataSize == 0));
        VERIFY_EXPR(pTLAS != nullptr);

        auto*      pTLASImpl = ValidatedCast<TopLevelASImplType>(pTLAS);
        const auto Desc      = pTLASImpl->GetInstanceDesc(pInstanceName);

        VERIFY_EXPR(pTLASImpl->GetBindingMode() == SHADER_BINDING_MODE_PER_GEOMETRY ||
                    pTLASImpl->GetBindingMode() == SHADER_BINDING_MODE_PER_INSTANCE);
        VERIFY_EXPR(RayOffsetInHitGroupIndex < pTLASImpl->GetHitShadersPerInstance());
        VERIFY_EXPR(Desc.ContributionToHitGroupIndex != INVALID_INDEX);

        const Uint32 InstanceIndex = Desc.ContributionToHitGroupIndex;
        Uint32       GeometryCount = 0;

        switch (pTLASImpl->GetBindingMode())
        {
            // clang-format off
            case SHADER_BINDING_MODE_PER_GEOMETRY: GeometryCount = Desc.pBLAS ? Desc.pBLAS->GetActualGeometryCount() : 0; break;
            case SHADER_BINDING_MODE_PER_INSTANCE: GeometryCount = 1;                                                     break;
            default:                               UNEXPECTED("unknown binding mode");
                // clang-format on
        }

        VERIFY_EXPR((pData == nullptr) || (DataSize == this->m_ShaderRecordSize * GeometryCount));

        const Uint32 BeginIndex = InstanceIndex + RayOffsetInHitGroupIndex;
        const size_t EndIndex   = InstanceIndex + GeometryCount * pTLASImpl->GetHitShadersPerInstance() + RayOffsetInHitGroupIndex;
        const Uint32 GroupSize  = this->m_pDevice->GetProperties().ShaderGroupHandleSize;
        const size_t Stride     = this->m_ShaderRecordStride;
        const auto*  DataPtr    = static_cast<const Uint8*>(pData);

        this->m_HitGroupsRecord.resize(std::max(this->m_HitGroupsRecord.size(), EndIndex * Stride), Uint8{EmptyElem});

        for (Uint32 i = 0; i < GeometryCount; ++i)
        {
            size_t Offset = (BeginIndex + i) * Stride;
            this->m_pPSO->CopyShaderHandle(pShaderGroupName, this->m_HitGroupsRecord.data() + Offset, Stride);

            std::memcpy(this->m_HitGroupsRecord.data() + Offset + GroupSize, DataPtr, this->m_ShaderRecordSize);
            DataPtr += this->m_ShaderRecordSize;

#ifdef DILIGENT_DEVELOPMENT
            OnBindHitGroup(pTLASImpl, BeginIndex + i);
#endif
        }
        this->m_Changed = true;
    }


    void DILIGENT_CALL_TYPE BindHitGroupForAll(ITopLevelAS* pTLAS,
                                               Uint32       RayOffsetInHitGroupIndex,
                                               const char*  pShaderGroupName,
                                               const void*  pData,
                                               Uint32       DataSize) override final
    {
        VERIFY_EXPR((pData == nullptr) == (DataSize == 0));
        VERIFY_EXPR((pData == nullptr) || (DataSize == this->m_ShaderRecordSize));
        VERIFY_EXPR(pTLAS != nullptr);

        auto* pTLASImpl = ValidatedCast<TopLevelASImplType>(pTLAS);
        VERIFY_EXPR(pTLASImpl->GetBindingMode() == SHADER_BINDING_MODE_PER_GEOMETRY ||
                    pTLASImpl->GetBindingMode() == SHADER_BINDING_MODE_PER_INSTANCE ||
                    pTLASImpl->GetBindingMode() == SHADER_BINDING_MODE_PER_ACCEL_STRUCT);
        VERIFY_EXPR(RayOffsetInHitGroupIndex < pTLASImpl->GetHitShadersPerInstance());

        Uint32 FirstContributionToHitGroupIndex, LastContributionToHitGroupIndex;
        pTLASImpl->GetContributionToHitGroupIndex(FirstContributionToHitGroupIndex, LastContributionToHitGroupIndex);

        const Uint32 GroupSize = this->m_pDevice->GetProperties().ShaderGroupHandleSize;
        const size_t Stride    = this->m_ShaderRecordStride;
        this->m_HitGroupsRecord.resize(std::max(this->m_HitGroupsRecord.size(), (LastContributionToHitGroupIndex + 1) * Stride), Uint8{EmptyElem});
        this->m_Changed = true;

        for (Uint32 Index = FirstContributionToHitGroupIndex; Index <= LastContributionToHitGroupIndex; ++Index)
        {
            const size_t Offset = Index * Stride;
            this->m_pPSO->CopyShaderHandle(pShaderGroupName, this->m_HitGroupsRecord.data() + Offset, Stride);
            std::memcpy(this->m_HitGroupsRecord.data() + Offset + GroupSize, pData, DataSize);

#ifdef DILIGENT_DEVELOPMENT
            OnBindHitGroup(pTLASImpl, Index);
#endif
        }
    }


    void DILIGENT_CALL_TYPE BindCallableShader(const char* pShaderGroupName,
                                               Uint32      CallableIndex,
                                               const void* pData,
                                               Uint32      DataSize) override final
    {
        VERIFY_EXPR((pData == nullptr) == (DataSize == 0));
        VERIFY_EXPR((pData == nullptr) || (DataSize == this->m_ShaderRecordSize));

        const Uint32 GroupSize = this->m_pDevice->GetProperties().ShaderGroupHandleSize;
        const size_t Offset    = CallableIndex * this->m_ShaderRecordStride;
        this->m_CallableShadersRecord.resize(std::max(this->m_CallableShadersRecord.size(), Offset + this->m_ShaderRecordStride), Uint8{EmptyElem});

        this->m_pPSO->CopyShaderHandle(pShaderGroupName, this->m_CallableShadersRecord.data() + Offset, this->m_ShaderRecordStride);
        std::memcpy(this->m_CallableShadersRecord.data() + Offset + GroupSize, pData, DataSize);
        this->m_Changed = true;
    }


    Bool DILIGENT_CALL_TYPE Verify(SHADER_BINDING_VALIDATION_FLAGS Flags) const override final
    {
        const auto Stride      = this->m_ShaderRecordStride;
        const auto ShSize      = this->m_pDevice->GetProperties().ShaderGroupHandleSize;
        const auto FindPattern = [&](const std::vector<Uint8>& Data, const char* GroupName) -> bool //
        {
            for (size_t i = 0; i < Data.size(); i += Stride)
            {
                if (Flags & SHADER_BINDING_VALIDATION_SHADER_ONLY)
                {
                    Uint32 Count = 0;
                    for (size_t j = 0; j < ShSize; ++j)
                        Count += (Data[i + j] == EmptyElem);

                    if (Count == ShSize)
                    {
                        LOG_INFO_MESSAGE("Shader binding table '", this->m_Desc.Name, "' is not valid: shader in '", GroupName, "'(", i / Stride, ") is not bound");
                        return false;
                    }
                }

                if ((Flags & SHADER_BINDING_VALIDATION_SHADER_RECORD) && this->m_ShaderRecordSize > 0)
                {
                    Uint32 Count = 0;
                    for (size_t j = ShSize; j < Stride; ++j)
                        Count += (Data[i + j] == EmptyElem);

                    // shader record data may not used in shader
                    if (Count == Stride - ShSize)
                    {
                        LOG_INFO_MESSAGE("Shader binding table '", this->m_Desc.Name, "' is not valid: shader record data in '", GroupName, "'(", i / Stride, ") is not initialized");
                        return false;
                    }
                }
            }
            return true;
        };

        if (m_RayGenShaderRecord.empty())
        {
            LOG_INFO_MESSAGE("Shader binding table '", this->m_Desc.Name, "' is not valid: ray generation shader is not bound");
            return false;
        }

#ifdef DILIGENT_DEVELOPMENT
        if (Flags & SHADER_BINDING_VALIDATION_TLAS)
        {
            for (size_t i = 0; i < m_DbgHitGroupBindings.size(); ++i)
            {
                auto& Binding = m_DbgHitGroupBindings[i];
                auto  pTLAS   = Binding.pTLAS.Lock();
                if (!pTLAS)
                {
                    LOG_INFO_MESSAGE("Shader binding table '", this->m_Desc.Name, "' is not valid: TLAS that was used to bind hit group at index (", i, ") was deleted");
                    return false;
                }
                if (pTLAS->GetVersion() != Binding.Version)
                {
                    LOG_INFO_MESSAGE("Shader binding table '", this->m_Desc.Name, "' is not valid: TLAS that was used to bind hit group at index '(", i,
                                     ") with name '", pTLAS->GetDesc().Name, " was changed and no longer compatible with SBT");
                    return false;
                }
            }
        }
#endif

        bool valid = true;
        valid      = valid && FindPattern(m_RayGenShaderRecord, "ray generation");
        valid      = valid && FindPattern(m_MissShadersRecord, "miss");
        valid      = valid && FindPattern(m_CallableShadersRecord, "callable");
        valid      = valid && FindPattern(m_HitGroupsRecord, "hit groups");
        return valid;
    }


    struct BindingTable
    {
        const void* pData  = nullptr;
        Uint32      Size   = 0;
        Uint32      Offset = 0;
        Uint32      Stride = 0;
    };
    void GetData(IBuffer*&     pSBTBuffer,
                 BindingTable& RaygenShaderBindingTable,
                 BindingTable& MissShaderBindingTable,
                 BindingTable& HitShaderBindingTable,
                 BindingTable& CallableShaderBindingTable)
    {
        const auto ShaderGroupBaseAlignment = this->m_pDevice->GetProperties().ShaderGroupBaseAlignment;

        const auto AlignToLarger = [ShaderGroupBaseAlignment](size_t offset) -> Uint32 {
            return Align(static_cast<Uint32>(offset), ShaderGroupBaseAlignment);
        };

        const Uint32 RayGenOffset          = 0;
        const Uint32 MissShaderOffset      = AlignToLarger(m_RayGenShaderRecord.size());
        const Uint32 HitGroupOffset        = AlignToLarger(MissShaderOffset + m_MissShadersRecord.size());
        const Uint32 CallableShadersOffset = AlignToLarger(HitGroupOffset + m_HitGroupsRecord.size());
        const Uint32 BufSize               = AlignToLarger(CallableShadersOffset + m_CallableShadersRecord.size());

        // recreate buffer
        if (this->m_pBuffer == nullptr || this->m_pBuffer->GetDesc().uiSizeInBytes < BufSize)
        {
            this->m_pBuffer = nullptr;

            String     BuffName = String{this->m_Desc.Name} + " - internal buffer";
            BufferDesc BuffDesc;
            BuffDesc.Name          = BuffName.c_str();
            BuffDesc.Usage         = USAGE_DEFAULT;
            BuffDesc.BindFlags     = BIND_RAY_TRACING;
            BuffDesc.uiSizeInBytes = BufSize;

            this->m_pDevice->CreateBuffer(BuffDesc, nullptr, &this->m_pBuffer);
            VERIFY_EXPR(this->m_pBuffer != nullptr);
        }

        if (this->m_pBuffer == nullptr)
            return; // Something went wrong

        pSBTBuffer = this->m_pBuffer;

        if (m_RayGenShaderRecord.size())
        {
            RaygenShaderBindingTable.pData  = this->m_Changed ? m_RayGenShaderRecord.data() : nullptr;
            RaygenShaderBindingTable.Offset = RayGenOffset;
            RaygenShaderBindingTable.Size   = static_cast<Uint32>(m_RayGenShaderRecord.size());
            RaygenShaderBindingTable.Stride = this->m_ShaderRecordStride;
        }

        if (m_MissShadersRecord.size())
        {
            MissShaderBindingTable.pData  = this->m_Changed ? m_MissShadersRecord.data() : nullptr;
            MissShaderBindingTable.Offset = MissShaderOffset;
            MissShaderBindingTable.Size   = static_cast<Uint32>(m_MissShadersRecord.size());
            MissShaderBindingTable.Stride = this->m_ShaderRecordStride;
        }

        if (m_HitGroupsRecord.size())
        {
            HitShaderBindingTable.pData  = this->m_Changed ? m_HitGroupsRecord.data() : nullptr;
            HitShaderBindingTable.Offset = HitGroupOffset;
            HitShaderBindingTable.Size   = static_cast<Uint32>(m_HitGroupsRecord.size());
            HitShaderBindingTable.Stride = this->m_ShaderRecordStride;
        }

        if (m_CallableShadersRecord.size())
        {
            CallableShaderBindingTable.pData  = this->m_Changed ? m_CallableShadersRecord.data() : nullptr;
            CallableShaderBindingTable.Offset = CallableShadersOffset;
            CallableShaderBindingTable.Size   = static_cast<Uint32>(m_CallableShadersRecord.size());
            CallableShaderBindingTable.Stride = this->m_ShaderRecordStride;
        }

        if (!this->m_Changed)
            return;

        this->m_Changed = false;
    }


protected:
    std::vector<Uint8> m_RayGenShaderRecord;
    std::vector<Uint8> m_MissShadersRecord;
    std::vector<Uint8> m_CallableShadersRecord;
    std::vector<Uint8> m_HitGroupsRecord;

    RefCntAutoPtr<PipelineStateImplType> m_pPSO;
    RefCntAutoPtr<IBuffer>               m_pBuffer;

    Uint32 m_ShaderRecordSize   = 0;
    Uint32 m_ShaderRecordStride = 0;
    bool   m_Changed            = true;

    static constexpr Uint8 EmptyElem = 0xA7;

private:
#ifdef DILIGENT_DEVELOPMENT
    struct HitGroupBinding
    {
        RefCntWeakPtr<TopLevelASImplType> pTLAS;
        Uint32                            Version = ~0u;
    };
    mutable std::vector<HitGroupBinding> m_DbgHitGroupBindings;

    void OnBindHitGroup(TopLevelASImplType* pTLAS, Uint32 Index)
    {
        this->m_DbgHitGroupBindings.resize(Index + 1);

        auto& Binding   = this->m_DbgHitGroupBindings[Index];
        Binding.pTLAS   = pTLAS;
        Binding.Version = pTLAS->GetVersion();
    }
#endif
};

} // namespace Diligent
