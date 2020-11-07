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

#include "TopLevelAS.h"
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

    void SetInstanceData(const TLASBuildInstanceData* pInstances, Uint32 InstanceCount, Uint32 HitShadersPerInstance) noexcept
    {
        try
        {
            ClearInstanceData();

            this->m_HitShadersPerInstance = HitShadersPerInstance;

            size_t StringPoolSize = 0;
            for (Uint32 i = 0; i < InstanceCount; ++i)
            {
                VERIFY_EXPR(pInstances[i].InstanceName != nullptr);
                StringPoolSize += StringPool::GetRequiredReserveSize(pInstances[i].InstanceName);
            }

            this->m_StringPool.Reserve(StringPoolSize, GetRawAllocator());

            Uint32 InstanceOffset = 0;

            for (Uint32 i = 0; i < InstanceCount; ++i)
            {
                auto&        inst     = pInstances[i];
                const char*  NameCopy = this->m_StringPool.CopyString(inst.InstanceName);
                InstanceDesc Desc     = {};

                Desc.ContributionToHitGroupIndex = inst.ContributionToHitGroupIndex;
                Desc.pBLAS                       = ValidatedCast<BottomLevelASType>(inst.pBLAS);

#ifdef DILIGENT_DEVELOPMENT
                Desc.Version = Desc.pBLAS->GetVersion();
#endif

                if (Desc.ContributionToHitGroupIndex == TLAS_INSTANCE_OFFSET_AUTO)
                {
                    Desc.ContributionToHitGroupIndex = InstanceOffset;
                    auto& BLASDesc                   = Desc.pBLAS->GetDesc();
                    switch (this->m_Desc.BindingMode)
                    {
                        // clang-format off
                        case SHADER_BINDING_MODE_PER_GEOMETRY: InstanceOffset += (BLASDesc.TriangleCount + BLASDesc.BoxCount) * HitShadersPerInstance;     break;
                        case SHADER_BINDING_MODE_PER_INSTANCE: InstanceOffset += HitShadersPerInstance;                                                    break;
                        case SHADER_BINDING_USER_DEFINED:      UNEXPECTED("TLAS_INSTANCE_OFFSET_AUTO is not compatible with SHADER_BINDING_USER_DEFINED"); break;
                        default:                               UNEXPECTED("Unknown ray tracing shader binding mode");
                            // clang-format on
                    }
                }

                bool IsUniqueName = this->m_Instances.emplace(NameCopy, Desc).second;
                if (!IsUniqueName)
                    LOG_ERROR_AND_THROW("Instance name must be unique!");
            }

            VERIFY_EXPR(this->m_StringPool.GetRemainingSize() == 0);
        }
        catch (...)
        {
            ClearInstanceData();
        }
    }

    void CopyInstancceData(const TopLevelASBase& Src) noexcept
    {
        ClearInstanceData();

        this->m_StringPool.Reserve(Src.m_StringPool.GetReservedSize(), GetRawAllocator());
        this->m_HitShadersPerInstance = Src.m_HitShadersPerInstance;
        this->m_Desc.BindingMode      = Src.m_Desc.BindingMode;

        for (auto& SrcInst : Src.m_Instances)
        {
            const char* NameCopy = this->m_StringPool.CopyString(SrcInst.first.GetStr());
            this->m_Instances.emplace(NameCopy, SrcInst.second);
        }

        VERIFY_EXPR(this->m_StringPool.GetRemainingSize() == 0);
    }

    virtual TLASInstanceDesc DILIGENT_CALL_TYPE GetInstanceDesc(const char* Name) const override final
    {
        VERIFY_EXPR(Name != nullptr && Name[0] != '\0');

        TLASInstanceDesc Result = {};

        auto iter = this->m_Instances.find(Name);
        if (iter != this->m_Instances.end())
        {
            Result.ContributionToHitGroupIndex = iter->second.ContributionToHitGroupIndex;
            Result.pBLAS                       = iter->second.pBLAS.template RawPtr<IBottomLevelAS>();
        }
        else
        {
            UNEXPECTED("Can't find instance with the specified name ('", Name, "')");
        }

        return Result;
    }

    virtual void DILIGENT_CALL_TYPE SetState(RESOURCE_STATE State) override final
    {
        VERIFY(State == RESOURCE_STATE_BUILD_AS_READ || State == RESOURCE_STATE_BUILD_AS_WRITE || State == RESOURCE_STATE_RAY_TRACING,
               "Unsupported state for top-level acceleration structure");
        this->m_State = State;
    }

    virtual RESOURCE_STATE DILIGENT_CALL_TYPE GetState() const override final
    {
        return this->m_State;
    }

    /// Implementation of ITopLevelAS::GetScratchBufferSizes().
    virtual ScratchBufferSizes DILIGENT_CALL_TYPE GetScratchBufferSizes() const override
    {
        return m_ScratchSize;
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

        if (m_Instances.empty())
        {
            LOG_ERROR_MESSAGE("TLAS with name ('", this->m_Desc.Name, "') doesn't have instances, use IDeviceContext::BuildTLAS() or IDeviceContext::CopyTLAS() to initialize TLAS content");
            result = false;
        }

        // Validate instances
        for (const auto& NameAndInst : m_Instances)
        {
            const InstanceDesc&      Inst = NameAndInst.second;
            const BottomLevelASDesc& Desc = Inst.pBLAS->GetDesc();

            if (Inst.Version != Inst.pBLAS->GetVersion())
            {
                LOG_ERROR_MESSAGE("Instance with name ('", NameAndInst.first.GetStr(), "') has BLAS with name ('", Desc.Name, "') that was changed after TLAS build, you must rebuild TLAS");
                result = false;
            }

            if (Inst.pBLAS->GetState() != RESOURCE_STATE_BUILD_AS_READ)
            {
                LOG_ERROR_MESSAGE("Instance with name ('", NameAndInst.first.GetStr(), "') has BLAS with name ('", Desc.Name, "') that must be in BUILD_AS_READ state, but current state is ",
                                  GetResourceStateFlagString(Inst.pBLAS->GetState()));
                result = false;
            }

            if (!Inst.pBLAS->ValidateContent())
            {
                LOG_ERROR_MESSAGE("Instance with name ('", NameAndInst.first.GetStr(), "') has BLAS with name ('", Desc.Name, "') that is not valid");
                result = false;
            }
        }
        return result;
    }
#endif

private:
    void ClearInstanceData()
    {
        this->m_Instances.clear();
        this->m_StringPool.Clear();
    }

protected:
    RESOURCE_STATE m_State                 = RESOURCE_STATE_UNKNOWN;
    Uint32         m_HitShadersPerInstance = 0;

    ScratchBufferSizes m_ScratchSize;

    StringPool m_StringPool;

    struct InstanceDesc
    {
        Uint32                           ContributionToHitGroupIndex = 0;
        RefCntAutoPtr<BottomLevelASType> pBLAS;

#ifdef DILIGENT_DEVELOPMENT
        Uint32 Version = 0;
#endif
    };
    std::unordered_map<HashMapStringKey, InstanceDesc, HashMapStringKey::Hasher> m_Instances;
};

} // namespace Diligent
