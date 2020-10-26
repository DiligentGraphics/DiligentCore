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

/// Template class implementing base functionality for a top-level acceleration structure object.

/// \tparam BaseInterface - base interface that this class will inheret
///                          (Diligent::ITopLevelASD3D12 or Diligent::ITopLevelASVk).
/// \tparam RenderDeviceImplType - type of the render device implementation
///                                (Diligent::RenderDeviceD3D12Impl or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class RenderDeviceImplType>
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
        ValidateTopLevelASDesc(Desc);
    }

    ~TopLevelASBase()
    {
    }

    void SetInstanceData(const TLASBuildInstanceData* pInstances, Uint32 InstanceCount, Uint32 HitShadersPerInstance)
    {
        m_Instances.clear();
        m_StringPool.Release();

        size_t StringPoolSize = 0;
        for (Uint32 i = 0; i < InstanceCount; ++i)
        {
            StringPoolSize += strlen(pInstances[i].InstanceName) + 1;
        }

        m_StringPool.Reserve(StringPoolSize, GetRawAllocator());

        Uint32 InstanceOffset = 0;

        for (Uint32 i = 0; i < InstanceCount; ++i)
        {
            auto&        inst     = pInstances[i];
            const char*  NameCopy = m_StringPool.CopyString(inst.InstanceName);
            InstanceDesc Desc     = {};

            Desc.ContributionToHitGroupIndex = inst.ContributionToHitGroupIndex;
            Desc.pBLAS                       = inst.pBLAS;

            if (Desc.ContributionToHitGroupIndex == TLAS_INSTANCE_OFFSET_AUTO)
            {
                Desc.ContributionToHitGroupIndex = InstanceOffset;
                auto& BLASDesc                   = Desc.pBLAS->GetDesc();
                InstanceOffset += (BLASDesc.TriangleCount + BLASDesc.BoxCount) * HitShadersPerInstance;
            }

            bool IsUniqueName = m_Instances.emplace(NameCopy, Desc).second;
            if (!IsUniqueName)
                LOG_ERROR_AND_THROW("Instance name must be unique!");
        }
    }

    virtual TLASInstanceDesc DILIGENT_CALL_TYPE GetInstanceDesc(const char* Name) const override final
    {
        VERIFY_EXPR(Name != nullptr && Name[0] != '\0');

        TLASInstanceDesc Result = {};

        auto iter = m_Instances.find(Name);
        if (iter != m_Instances.end())
        {
            Result.ContributionToHitGroupIndex = iter->second.ContributionToHitGroupIndex;
            Result.pBLAS                       = iter->second.pBLAS;
        }
        else
        {
            UNEXPECTED("Can't find instance with the specified name ('", Name, "')");
        }

        return Result;
    }

    virtual void DILIGENT_CALL_TYPE SetState(RESOURCE_STATE State) override final
    {
        this->m_State = State;
    }

    virtual RESOURCE_STATE DILIGENT_CALL_TYPE GetState() const override final
    {
        return this->m_State;
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

protected:
    static void ValidateTopLevelASDesc(const TopLevelASDesc& Desc)
    {
#define LOG_TLAS_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of Top-level AS '", (Desc.Name ? Desc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

        if (Desc.MaxInstanceCount == 0)
        {
            LOG_TLAS_ERROR_AND_THROW("MaxInstanceCount must not be zero");
        }

        if ((Desc.Flags & RAYTRACING_BUILD_AS_PREFER_FAST_TRACE) != 0 ||
            (Desc.Flags & RAYTRACING_BUILD_AS_PREFER_FAST_BUILD) != 0)
        {
            LOG_TLAS_ERROR_AND_THROW("RAYTRACING_BUILD_AS_PREFER_FAST_TRACE and RAYTRACING_BUILD_AS_PREFER_FAST_BUILD are invalid");
        }

#undef LOG_TLAS_ERROR_AND_THROW
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_TopLevelAS, TDeviceObjectBase)

protected:
    RESOURCE_STATE m_State = RESOURCE_STATE_UNKNOWN;

    StringPool m_StringPool;

    struct InstanceDesc
    {
        Uint32                                ContributionToHitGroupIndex = 0;
        mutable RefCntAutoPtr<IBottomLevelAS> pBLAS;
    };
    std::unordered_map<HashMapStringKey, InstanceDesc, HashMapStringKey::Hasher> m_Instances;
};

} // namespace Diligent
