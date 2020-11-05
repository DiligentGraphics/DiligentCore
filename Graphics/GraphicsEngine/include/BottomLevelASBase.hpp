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
#include <memory>

#include "BottomLevelAS.h"
#include "DeviceObjectBase.hpp"
#include "RenderDeviceBase.hpp"
#include "LinearAllocator.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

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
        ValidateBottomLevelASDesc(Desc);

        if (Desc.CompactedSize > 0)
        {}
        else
        {
            LinearAllocator MemPool{GetRawAllocator()};
            CopyDescription(Desc, this->m_Desc, MemPool, m_NameToIndex);
            this->m_pRawPtr = MemPool.ReleaseOwnership();
        }
    }

    ~BottomLevelASBase()
    {
        if (this->m_pRawPtr)
        {
            GetRawAllocator().Free(this->m_pRawPtr);
        }
    }

    static constexpr Uint32 InvalidGeometryIndex = ~0u;

    virtual Uint32 DILIGENT_CALL_TYPE GetGeometryIndex(const char* Name) const override final
    {
        VERIFY_EXPR(Name != nullptr && Name[0] != '\0');

        auto iter = m_NameToIndex.find(Name);
        if (iter != m_NameToIndex.end())
            return iter->second;

        UNEXPECTED("Can't find geometry with specified name");
        return InvalidGeometryIndex;
    }

    virtual void DILIGENT_CALL_TYPE SetState(RESOURCE_STATE State) override final
    {
        VERIFY(State == RESOURCE_STATE_BUILD_AS_READ || State == RESOURCE_STATE_BUILD_AS_WRITE,
               "Unsupported state for bottom-level acceleration structure");
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

    void CopyDescription(const BottomLevelASBase& Src)
    {
        const auto& SrcDesc = Src.GetDesc();
        auto&       DstDesc = this->m_Desc;

        try
        {
            if (this->m_pRawPtr)
            {
                GetRawAllocator().Free(this->m_pRawPtr);
                this->m_pRawPtr = nullptr;
            }
            m_NameToIndex.clear();

            DstDesc.TriangleCount = SrcDesc.TriangleCount;
            DstDesc.BoxCount      = SrcDesc.BoxCount;

            LinearAllocator MemPool{GetRawAllocator()};
            CopyDescription(SrcDesc, DstDesc, MemPool, m_NameToIndex);
            this->m_pRawPtr = MemPool.ReleaseOwnership();
        }
        catch (...)
        {
            // memory for arrays is not allocated or have been freed
            DstDesc.pTriangles = nullptr;
            DstDesc.pBoxes     = nullptr;
            m_NameToIndex.clear();
        }
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

protected:
    static void ValidateBottomLevelASDesc(const BottomLevelASDesc& Desc)
    {
#define LOG_BLAS_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of Bottom-level AS '", (Desc.Name ? Desc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

        if (Desc.CompactedSize > 0)
        {
            if (Desc.pTriangles != nullptr || Desc.pBoxes != nullptr)
                LOG_BLAS_ERROR_AND_THROW("If CompactedSize is specified then pTriangles and pBoxes must be null");

            if (Desc.Flags != RAYTRACING_BUILD_AS_NONE)
                LOG_BLAS_ERROR_AND_THROW("If CompactedSize is specified then Flags must be RAYTRACING_BUILD_AS_NONE");
        }
        else
        {
            if (!((Desc.pBoxes != nullptr) ^ (Desc.pTriangles != nullptr)))
                LOG_BLAS_ERROR_AND_THROW("Exactly one of pTriangles and pBoxes must be defined");

            if (Desc.pBoxes == nullptr && Desc.BoxCount > 0)
                LOG_BLAS_ERROR_AND_THROW("pBoxes is null but BoxCount is not 0");

            if (Desc.pTriangles == nullptr && Desc.TriangleCount > 0)
                LOG_BLAS_ERROR_AND_THROW("pTriangles is null but TriangleCount is not 0");

            if ((Desc.Flags & RAYTRACING_BUILD_AS_PREFER_FAST_TRACE) && (Desc.Flags & RAYTRACING_BUILD_AS_PREFER_FAST_BUILD))
                LOG_BLAS_ERROR_AND_THROW("can not set both flags RAYTRACING_BUILD_AS_PREFER_FAST_TRACE and RAYTRACING_BUILD_AS_PREFER_FAST_BUILD");

#ifdef DILIGENT_DEVELOPMENT
            for (Uint32 i = 0; i < Desc.TriangleCount; ++i)
            {
                const auto& tri = Desc.pTriangles[i];

                if (tri.GeometryName == nullptr)
                    LOG_BLAS_ERROR_AND_THROW("pTriangles[", i, "].GeometryName must not be null");

                if (tri.VertexValueType >= VT_NUM_TYPES)
                    LOG_BLAS_ERROR_AND_THROW("pTriangles[", i, "].VertexValueType must be valid type");

                if (tri.VertexComponentCount != 2 && tri.VertexComponentCount != 3)
                    LOG_BLAS_ERROR_AND_THROW("pTriangles[", i, "].VertexComponentCount must be 2 or 3");

                if (tri.MaxVertexCount == 0)
                    LOG_BLAS_ERROR_AND_THROW("pTriangles[", i, "].MaxVertexCount must be greater then 0");

                if (tri.MaxPrimitiveCount == 0)
                    LOG_BLAS_ERROR_AND_THROW("pTriangles[", i, "].MaxPrimitiveCount must be greater then 0");

                if (tri.IndexType == VT_UNDEFINED)
                {
                    if (tri.MaxVertexCount != tri.MaxPrimitiveCount * 3)
                        LOG_BLAS_ERROR_AND_THROW("pTriangles[", i, "].MaxVertexCount must equal to (MaxPrimitiveCount * 3)");
                }
                else
                {
                    if (tri.IndexType != VT_UINT32 && tri.IndexType != VT_UINT16)
                        LOG_BLAS_ERROR_AND_THROW("pTriangles[", i, "].IndexType must be VT_UINT16 or VT_UINT32");
                }
            }

            for (Uint32 i = 0; i < Desc.BoxCount; ++i)
            {
                const auto& box = Desc.pBoxes[i];

                if (box.GeometryName == nullptr)
                    LOG_BLAS_ERROR_AND_THROW("pBoxes[", i, "].GeometryName must not be null");

                if (box.MaxBoxCount == 0)
                    LOG_BLAS_ERROR_AND_THROW("pBoxes[", i, "].MaxBoxCount must be greater then 0");
            }
#endif // DILIGENT_DEVELOPMENT
        }

#undef LOG_BLAS_ERROR_AND_THROW
    }

    static void CopyDescription(const BottomLevelASDesc&                                                SrcDesc,
                                BottomLevelASDesc&                                                      DstDesc,
                                LinearAllocator&                                                        MemPool,
                                std::unordered_map<HashMapStringKey, Uint32, HashMapStringKey::Hasher>& NameToIndex)
    {
        if (SrcDesc.pTriangles != nullptr)
        {
            MemPool.AddSpace<decltype(*SrcDesc.pTriangles)>(SrcDesc.TriangleCount);

            for (Uint32 i = 0; i < SrcDesc.TriangleCount; ++i)
                MemPool.AddSpaceForString(SrcDesc.pTriangles[i].GeometryName);

            MemPool.Reserve();

            auto* pTriangles = MemPool.CopyArray(SrcDesc.pTriangles, SrcDesc.TriangleCount);

            // copy strings
            for (Uint32 i = 0; i < SrcDesc.TriangleCount; ++i)
            {
                pTriangles[i].GeometryName = MemPool.CopyString(SrcDesc.pTriangles[i].GeometryName);
                bool IsUniqueName          = NameToIndex.emplace(SrcDesc.pTriangles[i].GeometryName, i).second;
                if (!IsUniqueName)
                    LOG_ERROR_AND_THROW("Geometry name must be unique!");
            }
            DstDesc.pTriangles = pTriangles;
            DstDesc.pBoxes     = nullptr;
            DstDesc.BoxCount   = 0;
        }
        else if (SrcDesc.pBoxes != nullptr)
        {
            MemPool.AddSpace<decltype(*SrcDesc.pBoxes)>(SrcDesc.BoxCount);

            for (Uint32 i = 0; i < SrcDesc.BoxCount; ++i)
                MemPool.AddSpaceForString(SrcDesc.pBoxes[i].GeometryName);

            MemPool.Reserve();

            auto* pBoxes = MemPool.CopyArray(SrcDesc.pBoxes, SrcDesc.BoxCount);

            // copy strings
            for (Uint32 i = 0; i < SrcDesc.BoxCount; ++i)
            {
                pBoxes[i].GeometryName = MemPool.CopyString(SrcDesc.pBoxes[i].GeometryName);
                bool IsUniqueName      = NameToIndex.emplace(SrcDesc.pBoxes[i].GeometryName, i).second;
                if (!IsUniqueName)
                    LOG_ERROR_AND_THROW("Geometry name must be unique!");
            }
            DstDesc.pBoxes        = pBoxes;
            DstDesc.pTriangles    = nullptr;
            DstDesc.TriangleCount = 0;
        }
        else
        {
            LOG_ERROR_AND_THROW("Either pTriangles or pBoxes must not be null");
        }
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_BottomLevelAS, TDeviceObjectBase)

protected:
    RESOURCE_STATE m_State = RESOURCE_STATE_UNKNOWN;

    std::unordered_map<HashMapStringKey, Uint32, HashMapStringKey::Hasher> m_NameToIndex;

    void* m_pRawPtr = nullptr;

#ifdef DILIGENT_DEVELOPMENT
    std::atomic<Uint32> m_Version{0};
#endif
};

} // namespace Diligent
