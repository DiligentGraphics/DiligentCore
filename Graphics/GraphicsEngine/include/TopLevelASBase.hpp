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
/// Implementation of the Diligent::TopLevelASBase template class

#include <unordered_map>
#include <atomic>

#include "TopLevelAS.h"
#include "BottomLevelASBase.hpp"
#include "DeviceObjectBase.hpp"
#include "RenderDeviceBase.hpp"
#include "StringPool.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

/// Validates top-level AS description and throws an exception in case of an error.
void ValidateTopLevelASDesc(const TopLevelASDesc& Desc) noexcept(false);

/// Template class implementing base functionality for a top-level acceleration structure object.

/// \tparam BaseInterface        - base interface that this class will inheret
///                                (Diligent::ITopLevelASD3D12 or Diligent::ITopLevelASVk).
/// \tparam RenderDeviceImplType - type of the render device implementation
///                                (Diligent::RenderDeviceD3D12Impl or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class BottomLevelASType, class RenderDeviceImplType>
class TopLevelASBase : public DeviceObjectBase<BaseInterface, RenderDeviceImplType, TopLevelASDesc>
{
private:
    struct InstanceDesc
    {
        Uint32                           ContributionToHitGroupIndex = 0;
        Uint32                           InstanceIndex               = 0;
        RefCntAutoPtr<BottomLevelASType> pBLAS;
#ifdef DILIGENT_DEVELOPMENT
        Uint32 Version = 0;
#endif
    };

public:
    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, TopLevelASDesc>;

    /// \param pRefCounters      - reference counters object that controls the lifetime of this BLAS.
    /// \param pDevice           - pointer to the device.
    /// \param Desc              - TLAS description.
    /// \param bIsDeviceInternal - flag indicating if the BLAS is an internal device object and
    ///							   must not keep a strong reference to the device.
    TopLevelASBase(IReferenceCounters*   pRefCounters,
                   RenderDeviceImplType* pDevice,
                   const TopLevelASDesc& Desc,
                   bool                  bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
    {
        ValidateTopLevelASDesc(this->m_Desc);
    }

    ~TopLevelASBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_TopLevelAS, TDeviceObjectBase)

    bool SetInstanceData(const TLASBuildInstanceData* pInstances,
                         const Uint32                 InstanceCount,
                         const Uint32                 BaseContributionToHitGroupIndex,
                         const Uint32                 HitShadersPerInstance,
                         const SHADER_BINDING_MODE    BindingMode) noexcept
    {
        try
        {
            ClearInstanceData();

            size_t StringPoolSize = 0;
            for (Uint32 i = 0; i < InstanceCount; ++i)
            {
                VERIFY_EXPR(pInstances[i].InstanceName != nullptr);
                StringPoolSize += StringPool::GetRequiredReserveSize(pInstances[i].InstanceName);
            }

            this->m_StringPool.Reserve(StringPoolSize, GetRawAllocator());

            Uint32 InstanceOffset = BaseContributionToHitGroupIndex;

            for (Uint32 i = 0; i < InstanceCount; ++i)
            {
                const auto&  Inst     = pInstances[i];
                const char*  NameCopy = this->m_StringPool.CopyString(Inst.InstanceName);
                InstanceDesc Desc     = {};

                Desc.pBLAS                       = ValidatedCast<BottomLevelASType>(Inst.pBLAS);
                Desc.ContributionToHitGroupIndex = Inst.ContributionToHitGroupIndex;
                Desc.InstanceIndex               = i;
                CalculateHitGroupIndex(Desc, InstanceOffset, HitShadersPerInstance, BindingMode);

#ifdef DILIGENT_DEVELOPMENT
                Desc.Version = Desc.pBLAS ? Desc.pBLAS->GetVersion() : ~0u;
#endif
                bool IsUniqueName = this->m_Instances.emplace(NameCopy, Desc).second;
                if (!IsUniqueName)
                    LOG_ERROR_AND_THROW("Instance name must be unique!");
            }

            VERIFY_EXPR(this->m_StringPool.GetRemainingSize() == 0);

            this->m_HitShadersPerInstance            = HitShadersPerInstance;
            this->m_FirstContributionToHitGroupIndex = BaseContributionToHitGroupIndex;
            this->m_LastContributionToHitGroupIndex  = InstanceOffset;
            this->m_BindingMode                      = BindingMode;

#ifdef DILIGENT_DEVELOPMENT
            this->m_DbgVersion.fetch_add(1);
#endif
            return true;
        }
        catch (...)
        {
#ifdef DILIGENT_DEVELOPMENT
            this->m_DbgVersion.fetch_add(1);
#endif
            ClearInstanceData();
            return false;
        }
    }

    bool UpdateInstances(const TLASBuildInstanceData* pInstances,
                         const Uint32                 InstanceCount,
                         const Uint32                 BaseContributionToHitGroupIndex,
                         const Uint32                 HitShadersPerInstance,
                         const SHADER_BINDING_MODE    BindingMode) noexcept
    {
#ifdef DILIGENT_DEVELOPMENT
        bool Changed = false;
#endif
        Uint32 InstanceOffset = BaseContributionToHitGroupIndex;

        for (Uint32 i = 0; i < InstanceCount; ++i)
        {
            const auto& Inst = pInstances[i];
            auto        Iter = this->m_Instances.find(Inst.InstanceName);

            if (Iter == this->m_Instances.end())
            {
                UNEXPECTED("Failed to find instance with name '", Inst.InstanceName, "' in instances from previous build");
                return false;
            }

            auto&       Desc      = Iter->second;
            const auto  PrevIndex = Desc.ContributionToHitGroupIndex;
            const auto* pPrevBLAS = Desc.pBLAS.template RawPtr<IBottomLevelAS>();

            Desc.pBLAS                       = ValidatedCast<BottomLevelASType>(Inst.pBLAS);
            Desc.ContributionToHitGroupIndex = Inst.ContributionToHitGroupIndex;
            //Desc.InstanceIndex             = i; // keep Desc.InstanceIndex unmodified
            CalculateHitGroupIndex(Desc, InstanceOffset, HitShadersPerInstance, BindingMode);

#ifdef DILIGENT_DEVELOPMENT
            Changed      = Changed || (pPrevBLAS != Inst.pBLAS);
            Changed      = Changed || (Desc.pBLAS ? Desc.Version != Desc.pBLAS->GetVersion() : false);
            Changed      = Changed || (PrevIndex != Desc.ContributionToHitGroupIndex);
            Desc.Version = Desc.pBLAS ? Desc.pBLAS->GetVersion() : ~0u;
#endif
        }

#ifdef DILIGENT_DEVELOPMENT
        Changed = Changed || (this->m_HitShadersPerInstance != HitShadersPerInstance);
        Changed = Changed || (this->m_FirstContributionToHitGroupIndex != BaseContributionToHitGroupIndex);
        Changed = Changed || (this->m_LastContributionToHitGroupIndex != InstanceOffset);
        Changed = Changed || (this->m_BindingMode != BindingMode);
        if (Changed)
            this->m_DbgVersion.fetch_add(1);
#endif
        this->m_HitShadersPerInstance            = HitShadersPerInstance;
        this->m_FirstContributionToHitGroupIndex = BaseContributionToHitGroupIndex;
        this->m_LastContributionToHitGroupIndex  = InstanceOffset;
        this->m_BindingMode                      = BindingMode;

        return true;
    }

    void CopyInstancceData(const TopLevelASBase& Src) noexcept
    {
        ClearInstanceData();

        this->m_StringPool.Reserve(Src.m_StringPool.GetReservedSize(), GetRawAllocator());
        this->m_HitShadersPerInstance            = Src.m_HitShadersPerInstance;
        this->m_FirstContributionToHitGroupIndex = Src.m_FirstContributionToHitGroupIndex;
        this->m_LastContributionToHitGroupIndex  = Src.m_LastContributionToHitGroupIndex;
        this->m_BindingMode                      = Src.m_BindingMode;

        for (auto& SrcInst : Src.m_Instances)
        {
            const char* NameCopy = this->m_StringPool.CopyString(SrcInst.first.GetStr());
            this->m_Instances.emplace(NameCopy, SrcInst.second);
        }

        VERIFY_EXPR(this->m_StringPool.GetRemainingSize() == 0);

#ifdef DILIGENT_DEVELOPMENT
        this->m_DbgVersion.fetch_add(1);
#endif
    }

    Uint32 GetInstanceCount() const
    {
        return static_cast<Uint32>(this->m_Instances.size());
    }

    Uint32 GetHitShadersPerInstance() const
    {
        return this->m_HitShadersPerInstance;
    }

    SHADER_BINDING_MODE GetBindingMode() const
    {
        return this->m_BindingMode;
    }

    virtual TLASInstanceDesc DILIGENT_CALL_TYPE GetInstanceDesc(const char* Name) const override final
    {
        VERIFY_EXPR(Name != nullptr && Name[0] != '\0');

        TLASInstanceDesc Result = {};

        auto Iter = this->m_Instances.find(Name);
        if (Iter != this->m_Instances.end())
        {
            const auto& Inst                   = Iter->second;
            Result.ContributionToHitGroupIndex = Inst.ContributionToHitGroupIndex;
            Result.InstanceIndex               = Inst.InstanceIndex;
            Result.pBLAS                       = Inst.pBLAS.template RawPtr<IBottomLevelAS>();
        }
        else
        {
            Result.ContributionToHitGroupIndex = INVALID_INDEX;
            Result.InstanceIndex               = INVALID_INDEX;
            LOG_ERROR_MESSAGE("Can't find instance with the specified name ('", Name, "')");
        }

        return Result;
    }

    virtual void DILIGENT_CALL_TYPE GetContributionToHitGroupIndex(Uint32& FirstContributionToHitGroupIndex,
                                                                   Uint32& LastContributionToHitGroupIndex) const override final
    {
        FirstContributionToHitGroupIndex = this->m_FirstContributionToHitGroupIndex;
        LastContributionToHitGroupIndex  = this->m_LastContributionToHitGroupIndex;

        VERIFY_EXPR(FirstContributionToHitGroupIndex <= LastContributionToHitGroupIndex);
    }

    virtual void DILIGENT_CALL_TYPE SetState(RESOURCE_STATE State) override final
    {
        VERIFY(State == RESOURCE_STATE_UNKNOWN || State == RESOURCE_STATE_BUILD_AS_READ || State == RESOURCE_STATE_BUILD_AS_WRITE || State == RESOURCE_STATE_RAY_TRACING,
               "Unsupported state for top-level acceleration structure");
        this->m_State = State;
    }

    virtual RESOURCE_STATE DILIGENT_CALL_TYPE GetState() const override final
    {
        return this->m_State;
    }

    /// Implementation of ITopLevelAS::GetScratchBufferSizes().
    virtual ScratchBufferSizes DILIGENT_CALL_TYPE GetScratchBufferSizes() const override final
    {
        return this->m_ScratchSize;
    }

    bool IsInKnownState() const
    {
        return this->m_State != RESOURCE_STATE_UNKNOWN;
    }

    bool CheckState(RESOURCE_STATE State) const
    {
        VERIFY((State & (State - 1)) == 0, "Single state is expected");
        VERIFY(IsInKnownState(), "TLAS state is unknown");
        return (this->m_State & State) == State;
    }

#ifdef DILIGENT_DEVELOPMENT
    bool ValidateContent() const
    {
        bool result = true;

        if (this->m_Instances.empty())
        {
            LOG_ERROR_MESSAGE("TLAS with name ('", this->m_Desc.Name, "') doesn't have instances, use IDeviceContext::BuildTLAS() or IDeviceContext::CopyTLAS() to initialize TLAS content");
            result = false;
        }

        // Validate instances
        for (const auto& NameAndInst : this->m_Instances)
        {
            const InstanceDesc& Inst = NameAndInst.second;

            if (Inst.pBLAS == nullptr)
                continue;

            if (Inst.Version != Inst.pBLAS->GetVersion())
            {
                LOG_ERROR_MESSAGE("Instance with name ('", NameAndInst.first.GetStr(), "') has BLAS with name ('", Inst.pBLAS->GetDesc().Name,
                                  "') that was changed after TLAS build, you must rebuild TLAS");
                result = false;
            }

            if (Inst.pBLAS->IsInKnownState() && Inst.pBLAS->GetState() != RESOURCE_STATE_BUILD_AS_READ)
            {
                LOG_ERROR_MESSAGE("Instance with name ('", NameAndInst.first.GetStr(), "') has BLAS with name ('", Inst.pBLAS->GetDesc().Name,
                                  "') that must be in BUILD_AS_READ state, but current state is ",
                                  GetResourceStateFlagString(Inst.pBLAS->GetState()));
                result = false;
            }
        }
        return result;
    }

    Uint32 GetVersion() const
    {
        return this->m_DbgVersion.load();
    }
#endif // DILIGENT_DEVELOPMENT

private:
    void ClearInstanceData()
    {
        this->m_Instances.clear();
        this->m_StringPool.Clear();

        this->m_BindingMode                      = SHADER_BINDING_MODE_LAST;
        this->m_HitShadersPerInstance            = 0;
        this->m_FirstContributionToHitGroupIndex = INVALID_INDEX;
        this->m_LastContributionToHitGroupIndex  = INVALID_INDEX;
    }

    static void CalculateHitGroupIndex(InstanceDesc& Desc, Uint32& InstanceOffset, const Uint32 HitShadersPerInstance, const SHADER_BINDING_MODE BindingMode)
    {
        static_assert(SHADER_BINDING_MODE_LAST == SHADER_BINDING_USER_DEFINED, "Please update the switch below to handle the new shader binding mode");

        if (Desc.ContributionToHitGroupIndex == TLAS_INSTANCE_OFFSET_AUTO)
        {
            Desc.ContributionToHitGroupIndex = InstanceOffset;
            switch (BindingMode)
            {
                // clang-format off
                case SHADER_BINDING_MODE_PER_GEOMETRY:     InstanceOffset += Desc.pBLAS ? Desc.pBLAS->GetActualGeometryCount() * HitShadersPerInstance : 0; break;
                case SHADER_BINDING_MODE_PER_INSTANCE:     InstanceOffset += HitShadersPerInstance;                                                         break;
                case SHADER_BINDING_MODE_PER_ACCEL_STRUCT: /* InstanceOffset is a constant */                                                               break;
                case SHADER_BINDING_USER_DEFINED:          UNEXPECTED("TLAS_INSTANCE_OFFSET_AUTO is not compatible with SHADER_BINDING_USER_DEFINED");      break;
                default:                                   UNEXPECTED("Unknown ray tracing shader binding mode");
                    // clang-format on
            }
        }
        else
        {
            VERIFY(BindingMode == SHADER_BINDING_USER_DEFINED, "BindingMode must be SHADER_BINDING_USER_DEFINED");
        }

        constexpr Uint32 MaxIndex = (1u << 24);
        VERIFY(Desc.ContributionToHitGroupIndex < MaxIndex, "ContributionToHitGroupIndex must be less than ", MaxIndex);
    }

protected:
    RESOURCE_STATE      m_State                            = RESOURCE_STATE_UNKNOWN;
    SHADER_BINDING_MODE m_BindingMode                      = SHADER_BINDING_MODE_LAST;
    Uint32              m_HitShadersPerInstance            = 0;
    Uint32              m_FirstContributionToHitGroupIndex = INVALID_INDEX;
    Uint32              m_LastContributionToHitGroupIndex  = INVALID_INDEX;
    ScratchBufferSizes  m_ScratchSize;

    std::unordered_map<HashMapStringKey, InstanceDesc, HashMapStringKey::Hasher> m_Instances;
    StringPool                                                                   m_StringPool;

#ifdef DILIGENT_DEVELOPMENT
    std::atomic<Uint32> m_DbgVersion{0};
#endif
};

} // namespace Diligent
