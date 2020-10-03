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
/// Implementation of the Diligent::BottomLevelASBase, Diligent::TopLevelASBase, Diligent::ShaderBindingTableBase template classes

#include "RayTracing.h"
#include "DeviceObjectBase.hpp"
#include "RenderDeviceBase.hpp"
#include "StringPool.hpp"
#include <map>

namespace Diligent
{

void ValidateBottomLevelASDesc(const BottomLevelASDesc& Desc);
void ValidateTopLevelASDesc(const TopLevelASDesc& Desc);
void ValidateShaderBindingTableDesc(const ShaderBindingTableDesc& Desc);

// AZ TODO: move somewhere
struct StringView
{
    const char* Str;
    size_t      Len;

    explicit StringView(const char* _Str) :
        Str{_Str}, Len{strlen(_Str)} {}

    StringView(const char* _Str, size_t _Len) :
        Str{_Str}, Len{_Len} {}

    bool operator==(const StringView& RHS) const
    {
        if (Len != RHS.Len)
            return false;

        for (size_t i = 0; i < Len; ++i)
        {
            if (Str[i] != RHS.Str[i])
                return false;
        }
        return true;
    }

    bool operator<(const StringView& RHS) const
    {
        if (Len != RHS.Len)
            return Len < RHS.Len;

        for (size_t i = 0; i < Len; ++i)
        {
            if (Str[i] != RHS.Str[i])
                return Str[i] < RHS.Str[i];
        }
        return false;
    }
};


/// Template class implementing base functionality for a bottom-level acceleration structure object.

/// \tparam BaseInterface - base interface that this class will inheret
///                          (Diligent::IBottomLevelASD3D12 or Diligent::IBottomLevelASVk).
/// \tparam RenderDeviceImplType - type of the render device implementation
///                                (Diligent::RenderDeviceD3D12Impl or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class RenderDeviceImplType>
class BottomLevelASBase : public DeviceObjectBase<BaseInterface, RenderDeviceImplType, BottomLevelASDesc>
{
public:
    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, BottomLevelASDesc>;

    /// \param pRefCounters - reference counters object that controls the lifetime of this BLAS.
    /// \param pDevice - pointer to the device.
    /// \param Desc - BLAS description.
    /// \param bIsDeviceInternal - flag indicating if the BLAS is an internal device object and
    ///							   must not keep a strong reference to the device.
    BottomLevelASBase(IReferenceCounters* pRefCounters, RenderDeviceImplType* pDevice, const BottomLevelASDesc& Desc, bool bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
    {
        ValidateBottomLevelASDesc(Desc);

        // Memory must be released even if exception was thrown.
        struct MemOwner
        {
            void* ptr = nullptr;

            ~MemOwner()
            {
                if (ptr != nullptr)
                    GetRawAllocator().Free(ptr);
            }
        } memOwner;

        if (Desc.pTriangles != nullptr)
        {
            size_t StringPoolSize = 0;
            for (Uint32 i = 0; i < Desc.TriangleCount; ++i)
            {
                if (Desc.pTriangles[i].GeometryName == nullptr)
                    LOG_ERROR_AND_THROW("Geometry name can not be null!");

                StringPoolSize += strlen(Desc.pTriangles[i].GeometryName) + 1;
            }

            m_StringPool.Reserve(StringPoolSize, GetRawAllocator());

            auto* pTriangles = ALLOCATE(GetRawAllocator(), "Memory for BLASTriangleDesc array", BLASTriangleDesc, Desc.TriangleCount);
            memOwner.ptr     = pTriangles;

            std::memcpy(pTriangles, Desc.pTriangles, sizeof(*Desc.pTriangles) * Desc.TriangleCount);
            this->m_Desc.pTriangles = pTriangles;
            this->m_Desc.pBoxes     = nullptr;

            // copy strings
            for (Uint32 i = 0; i < Desc.TriangleCount; ++i)
            {
                pTriangles[i].GeometryName = m_StringPool.CopyString(pTriangles[i].GeometryName);
                bool IsUniqueName          = m_NameToIndex.insert({StringView{pTriangles[i].GeometryName}, i}).second;
                if (!IsUniqueName)
                    LOG_ERROR_AND_THROW("Geometry name must be unique!");
            }
        }
        else if (Desc.pBoxes != nullptr)
        {
            size_t StringPoolSize = 0;
            for (Uint32 i = 0; i < Desc.TriangleCount; ++i)
            {
                if (Desc.pBoxes[i].GeometryName == nullptr)
                    LOG_ERROR_AND_THROW("Geometry name can not be null!");

                StringPoolSize += strlen(Desc.pBoxes[i].GeometryName) + 1;
            }

            m_StringPool.Reserve(StringPoolSize, GetRawAllocator());

            auto* pBoxes = ALLOCATE(GetRawAllocator(), "Memory for BLASBoundingBoxDesc array", BLASBoundingBoxDesc, Desc.BoxCount);
            memOwner.ptr = pBoxes;

            std::memcpy(pBoxes, Desc.pBoxes, sizeof(*Desc.pBoxes) * Desc.BoxCount);
            this->m_Desc.pBoxes     = pBoxes;
            this->m_Desc.pTriangles = nullptr;

            // copy strings
            for (Uint32 i = 0; i < Desc.TriangleCount; ++i)
            {
                pBoxes[i].GeometryName = m_StringPool.CopyString(pBoxes[i].GeometryName);
                bool IsUniqueName      = m_NameToIndex.insert({StringView{pBoxes[i].GeometryName}, i}).second;
                if (!IsUniqueName)
                    LOG_ERROR_AND_THROW("Geometry name must be unique!");
            }
        }

        // Constructor completed successfully and memory will be released in destructor.
        memOwner.ptr = nullptr;
    }

    ~BottomLevelASBase()
    {
        if (this->m_Desc.pTriangles != nullptr)
        {
            GetRawAllocator().Free(const_cast<BLASTriangleDesc*>(this->m_Desc.pTriangles));
        }
        if (this->m_Desc.pBoxes != nullptr)
        {
            GetRawAllocator().Free(const_cast<BLASBoundingBoxDesc*>(this->m_Desc.pBoxes));
        }
    }

    virtual Uint32 DILIGENT_CALL_TYPE GetGeometryIndex(const char* Name) const override
    {
        VERIFY_EXPR(Name != nullptr && Name[0] != '\0');

        auto iter = m_NameToIndex.find(StringView{Name});
        if (iter != m_NameToIndex.end())
            return iter->second;

        UNEXPECTED("Can't find geometry with specified name");
        return ~0u; // AZ TODO
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_BottomLevelAS, TDeviceObjectBase)

protected:
    std::map<StringView, Uint32> m_NameToIndex;
    StringPool                   m_StringPool;
};


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

    /// \param pRefCounters - reference counters object that controls the lifetime of this BLAS.
    /// \param pDevice - pointer to the device.
    /// \param Desc - TLAS description.
    /// \param bIsDeviceInternal - flag indicating if the BLAS is an internal device object and
    ///							   must not keep a strong reference to the device.
    TopLevelASBase(IReferenceCounters* pRefCounters, RenderDeviceImplType* pDevice, const TopLevelASDesc& Desc, bool bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
    {
        ValidateTopLevelASDesc(Desc);
    }

    ~TopLevelASBase()
    {
    }

    virtual TLASInstanceDesc DILIGENT_CALL_TYPE GetInstanceDesc(const char* Name) const override
    {
        VERIFY_EXPR(Name != nullptr && Name[0] != '\0');

        auto iter = m_Instances.find(StringView{Name});
        if (iter != m_Instances.end())
            return iter->second;

        UNEXPECTED("Can't find instance with specified name");
        return {};
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_TopLevelAS, TDeviceObjectBase)

protected:
    std::map<StringView, Uint32>           m_NameToIndex;
    StringPool                             m_StringPool;
    std::map<StringView, TLASInstanceDesc> m_Instances;
};


/// Template class implementing base functionality for a shader binding table object.

/// \tparam BaseInterface - base interface that this class will inheret
///                          (Diligent::IShaderBindingTableD3D12 or Diligent::IShaderBindingTableVk).
/// \tparam RenderDeviceImplType - type of the render device implementation
///                                (Diligent::RenderDeviceD3D12Impl or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class RenderDeviceImplType>
class ShaderBindingTableBase : public DeviceObjectBase<BaseInterface, RenderDeviceImplType, ShaderBindingTableDesc>
{
public:
    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, ShaderBindingTableDesc>;

    /// \param pRefCounters - reference counters object that controls the lifetime of this SBT.
    /// \param pDevice - pointer to the device.
    /// \param Desc - SBT description.
    /// \param bIsDeviceInternal - flag indicating if the BLAS is an internal device object and
    ///							   must not keep a strong reference to the device.
    ShaderBindingTableBase(IReferenceCounters* pRefCounters, RenderDeviceImplType* pDevice, const ShaderBindingTableDesc& Desc, bool bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
    {
        ValidateShaderBindingTableDesc(Desc);
    }

    ~ShaderBindingTableBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ShaderBindingTable, TDeviceObjectBase)

protected:
    std::map<StringView, Uint32>           m_NameToIndex;
    StringPool                             m_StringPool;
    std::map<StringView, TLASInstanceDesc> m_Instances;
};

} // namespace Diligent
