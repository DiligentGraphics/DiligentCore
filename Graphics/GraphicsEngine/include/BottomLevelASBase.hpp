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
/// Implementation of the Diligent::BottomLevelASBase template class

#include <unordered_map>
#include <atomic>

#include "BottomLevelAS.h"
#include "DeviceObjectBase.hpp"
#include "RenderDeviceBase.hpp"
#include "LinearAllocator.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

/// Validates bottom-level AS description and throws and exception in case of an error.
void ValidateBottomLevelASDesc(const BottomLevelASDesc& Desc) noexcept(false);

/// Copies bottom-level AS description (except for the Name) using MemPool to allocate required dynamic space.
void CopyBottomLevelASDesc(const BottomLevelASDesc&                                                SrcDesc,
                           BottomLevelASDesc&                                                      DstDesc,
                           LinearAllocator&                                                        MemPool,
                           std::unordered_map<HashMapStringKey, Uint32, HashMapStringKey::Hasher>& NameToIndex) noexcept(false);


/// Template class implementing base functionality for a bottom-level acceleration structure object.

/// \tparam BaseInterface        - base interface that this class will inheret
///                                (Diligent::IBottomLevelASD3D12 or Diligent::IBottomLevelASVk).
/// \tparam RenderDeviceImplType - type of the render device implementation
///                                (Diligent::RenderDeviceD3D12Impl or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class RenderDeviceImplType>
class BottomLevelASBase : public DeviceObjectBase<BaseInterface, RenderDeviceImplType, BottomLevelASDesc>
{
public:
    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, BottomLevelASDesc>;

    /// \param pRefCounters      - reference counters object that controls the lifetime of this BLAS.
    /// \param pDevice           - pointer to the device.
    /// \param Desc              - BLAS description.
    /// \param bIsDeviceInternal - flag indicating if the BLAS is an internal device object and
    ///							   must not keep a strong reference to the device.
    BottomLevelASBase(IReferenceCounters*      pRefCounters,
                      RenderDeviceImplType*    pDevice,
                      const BottomLevelASDesc& Desc,
                      bool                     bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
    {
        ValidateBottomLevelASDesc(this->m_Desc);

        if (Desc.CompactedSize > 0)
        {
        }
        else
        {
            CopyDescriptionUnsafe(Desc);
        }
    }

    ~BottomLevelASBase()
    {
        Clear();
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_BottomLevelAS, TDeviceObjectBase)

    static constexpr Uint32 InvalidGeometryIndex = ~0u;

    virtual Uint32 DILIGENT_CALL_TYPE GetGeometryIndex(const char* Name) const override final
    {
        VERIFY_EXPR(Name != nullptr && Name[0] != '\0');

        auto iter = m_NameToIndex.find(Name);
        if (iter != m_NameToIndex.end())
            return iter->second;

        LOG_ERROR_MESSAGE("Can't find geometry with name '", Name, '\'');
        return InvalidGeometryIndex;
    }

    virtual void DILIGENT_CALL_TYPE SetState(RESOURCE_STATE State) override final
    {
        VERIFY(State == RESOURCE_STATE_BUILD_AS_READ || State == RESOURCE_STATE_BUILD_AS_WRITE,
               "Unsupported state for a bottom-level acceleration structure");
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
        VERIFY(IsInKnownState(), "BLAS state is unknown");
        return (this->m_State & State) == State;
    }

#ifdef DILIGENT_DEVELOPMENT
    void UpdateVersion()
    {
        m_Version.fetch_add(1);
    }

    Uint32 GetVersion() const
    {
        return m_Version.load();
    }

    bool ValidateContent() const
    {
        // AZ TODO
        return true;
    }
#endif // DILIGENT_DEVELOPMENT

    void CopyDescription(const BottomLevelASBase& SrcBLAS) noexcept
    {
        Clear();

        try
        {
            CopyDescriptionUnsafe(SrcBLAS.GetDesc());
        }
        catch (...)
        {
            Clear();
        }
    }

private:
    void CopyDescriptionUnsafe(const BottomLevelASDesc& SrcDesc) noexcept(false)
    {
        LinearAllocator MemPool{GetRawAllocator()};
        CopyBottomLevelASDesc(SrcDesc, this->m_Desc, MemPool, m_NameToIndex);
        this->m_pRawPtr = MemPool.Release();
    }

    void Clear() noexcept
    {
        if (this->m_pRawPtr != nullptr)
        {
            GetRawAllocator().Free(this->m_pRawPtr);
            this->m_pRawPtr = nullptr;
        }

        // Preserve original name - it was allocated by DeviceObjectBase
        auto* Name        = this->m_Desc.Name;
        this->m_Desc      = BottomLevelASDesc{};
        this->m_Desc.Name = Name;

        m_NameToIndex.clear();
    }

protected:
    RESOURCE_STATE m_State = RESOURCE_STATE_UNKNOWN;

    std::unordered_map<HashMapStringKey, Uint32, HashMapStringKey::Hasher> m_NameToIndex;

    void* m_pRawPtr = nullptr;

#ifdef DILIGENT_DEVELOPMENT
    std::atomic<Uint32> m_Version{0};
#endif
};

} // namespace Diligent
