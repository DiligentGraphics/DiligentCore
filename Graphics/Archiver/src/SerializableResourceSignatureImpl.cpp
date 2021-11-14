/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "SerializableResourceSignatureImpl.hpp"
#include "PipelineResourceSignatureBase.hpp"
#include "FixedLinearAllocator.hpp"
#include "EngineMemory.h"

#if D3D11_SUPPORTED
#    include "../../GraphicsEngineD3D11/include/pch.h"
#    include "RenderDeviceD3D11Impl.hpp"
#    include "PipelineResourceSignatureD3D11Impl.hpp"
#    include "DeviceObjectArchiveD3D11Impl.hpp"
#endif
#if D3D12_SUPPORTED
#    include "../../GraphicsEngineD3D12/include/pch.h"
#    include "RenderDeviceD3D12Impl.hpp"
#    include "PipelineResourceSignatureD3D12Impl.hpp"
#    include "DeviceObjectArchiveD3D12Impl.hpp"
#endif
#if GL_SUPPORTED || GLES_SUPPORTED
#    include "../../GraphicsEngineOpenGL/include/pch.h"
#    include "RenderDeviceGLImpl.hpp"
#    include "PipelineResourceSignatureGLImpl.hpp"
#    include "DeviceObjectArchiveGLImpl.hpp"
#endif
#if VULKAN_SUPPORTED
#    include "VulkanUtilities/VulkanHeaders.h"
#    include "RenderDeviceVkImpl.hpp"
#    include "PipelineResourceSignatureVkImpl.hpp"
#    include "DeviceObjectArchiveVkImpl.hpp"
#endif

namespace Diligent
{
namespace
{

bool operator==(const PipelineResourceSignatureSerializedData& Lhs, const PipelineResourceSignatureSerializedData& Rhs)
{
    // clang-format off
    return Lhs.ShaderStages          == Rhs.ShaderStages          &&
           Lhs.StaticResShaderStages == Rhs.StaticResShaderStages &&
           Lhs.PipelineType          == Rhs.PipelineType          &&
           Lhs.StaticResStageIndex   == Rhs.StaticResStageIndex;
    // clang-format on
}

void CopyPRSDesc(const PipelineResourceSignatureDesc&            SrcDesc,
                 const PipelineResourceSignatureSerializedData&  SrcSerialized,
                 PipelineResourceSignatureDesc const*&           pDstDesc,
                 PipelineResourceSignatureSerializedData const*& pDstSerialized,
                 SerializedMemory&                               DescPtr,
                 SerializedMemory&                               SharedPtr)
{
    auto& RawAllocator = GetRawAllocator();

    // Copy description & serialization data
    {
        FixedLinearAllocator Allocator{RawAllocator};

        Allocator.AddSpace<PipelineResourceSignatureDesc>();
        Allocator.AddSpaceForString(SrcDesc.Name);
        ReserveSpaceForPipelineResourceSignatureDesc(Allocator, SrcDesc);

        Allocator.AddSpace<PipelineResourceSignatureSerializedData>();

        Allocator.Reserve();

        auto& DstDesc = *Allocator.Copy(SrcDesc);
        pDstDesc      = &DstDesc;

        DstDesc.Name = Allocator.CopyString(SrcDesc.Name);
        if (DstDesc.Name == nullptr)
            DstDesc.Name = "";

        std::array<Uint16, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES + 1> ResourceOffsets = {};
        CopyPipelineResourceSignatureDesc(Allocator, SrcDesc, DstDesc, ResourceOffsets);

        pDstSerialized = Allocator.Copy(SrcSerialized);
        DescPtr        = SerializedMemory{Allocator.ReleaseOwnership(), Allocator.GetCurrentSize()};
    }

    // Serialize description & serialization data
    {
        Serializer<SerializerMode::Measure> MeasureSer;
        PSOSerializer<SerializerMode::Measure>::SerializePRS(MeasureSer, SrcDesc, SrcSerialized, nullptr);

        const size_t SerSize = MeasureSer.GetSize(nullptr);
        void*        SerPtr  = ALLOCATE_RAW(RawAllocator, "", SerSize);

        Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
        PSOSerializer<SerializerMode::Write>::SerializePRS(Ser, SrcDesc, SrcSerialized, nullptr);
        VERIFY_EXPR(Ser.IsEnd());

        SharedPtr = SerializedMemory{SerPtr, SerSize};
    }
}

template <template <SerializerMode> class TSerializerImpl,
          typename TSerializedData>
void CopyPRSSerializedData(const TSerializedData& SrcSerialized,
                           SerializedMemory&      SerializedPtr)
{
    Serializer<SerializerMode::Measure> MeasureSer;
    TSerializerImpl<SerializerMode::Measure>::SerializePRS(MeasureSer, SrcSerialized, nullptr);

    const size_t SerSize = MeasureSer.GetSize(nullptr);
    void*        SerPtr  = ALLOCATE_RAW(GetRawAllocator(), "", SerSize);

    Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
    TSerializerImpl<SerializerMode::Write>::SerializePRS(Ser, SrcSerialized, nullptr);
    VERIFY_EXPR(Ser.IsEnd());

    SerializedPtr = SerializedMemory{SerPtr, SerSize};
}

} // namespace


template <typename ImplType>
struct SerializableResourceSignatureImpl::TPRS
{
    ImplType         PRS;
    SerializedMemory Mem;

    TPRS(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& SignatureDesc) :
        PRS{pRefCounters, nullptr, SignatureDesc, SHADER_TYPE_UNKNOWN, true}
    {}
};


void SerializableResourceSignatureImpl::AddPRSDesc(const PipelineResourceSignatureDesc& Desc, const PipelineResourceSignatureSerializedData& Serialized)
{
    if (m_DescMem)
    {
        VERIFY_EXPR(m_pDesc != nullptr);
        VERIFY_EXPR(m_pSerialized != nullptr);

        if (!(*m_pDesc == Desc) || !(*m_pSerialized == Serialized))
            LOG_ERROR_AND_THROW("Pipeline resource signature description is not the same for different backends");
    }
    else
        CopyPRSDesc(Desc, Serialized, m_pDesc, m_pSerialized, m_DescMem, m_SharedData);
}

SerializableResourceSignatureImpl::SerializableResourceSignatureImpl(IReferenceCounters*                  pRefCounters,
                                                                     SerializationDeviceImpl*             pDevice,
                                                                     const PipelineResourceSignatureDesc& Desc,
                                                                     Uint32                               DeviceBits) :
    TBase{pRefCounters}
{
    ValidatePipelineResourceSignatureDesc(Desc, pDevice->GetDevice());

    if ((DeviceBits & pDevice->GetValidDeviceBits()) != DeviceBits)
    {
        LOG_ERROR_AND_THROW("DeviceBits contains unsupported device type");
    }

    for (Uint32 Bits = DeviceBits; Bits != 0;)
    {
        const auto Type = static_cast<RENDER_DEVICE_TYPE>(PlatformMisc::GetLSB(ExtractLSB(Bits)));

        static_assert(RENDER_DEVICE_TYPE_COUNT == 7, "Please update the switch below to handle the new render device type");
        switch (Type)
        {
#if D3D11_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D11:
            {
                m_pPRSD3D11.reset(new TPRS<PipelineResourceSignatureD3D11Impl>{pRefCounters, Desc});
                auto* pPRSD3D11 = &m_pPRSD3D11->PRS;

                PipelineResourceSignatureSerializedDataD3D11 SerializedData;
                pPRSD3D11->Serialize(SerializedData);
                AddPRSDesc(pPRSD3D11->GetDesc(), SerializedData.Base);
                CopyPRSSerializedData<PSOSerializerD3D11>(SerializedData, m_pPRSD3D11->Mem);
                break;
            }
#endif

#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
            {
                m_pPRSD3D12.reset(new TPRS<PipelineResourceSignatureD3D12Impl>{pRefCounters, Desc});
                auto* pPRSD3D12 = &m_pPRSD3D12->PRS;

                PipelineResourceSignatureSerializedDataD3D12 SerializedData;
                pPRSD3D12->Serialize(SerializedData);
                AddPRSDesc(pPRSD3D12->GetDesc(), SerializedData.Base);
                CopyPRSSerializedData<PSOSerializerD3D12>(SerializedData, m_pPRSD3D12->Mem);
                break;
            }
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
            {
                m_pPRSGL.reset(new TPRS<PipelineResourceSignatureGLImpl>{pRefCounters, Desc});
                auto* pPRSGL = &m_pPRSGL->PRS;

                PipelineResourceSignatureSerializedDataGL SerializedData;
                pPRSGL->Serialize(SerializedData);
                AddPRSDesc(pPRSGL->GetDesc(), SerializedData.Base);
                CopyPRSSerializedData<PSOSerializerGL>(SerializedData, m_pPRSGL->Mem);
                break;
            }
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
            {
                m_pPRSVk.reset(new TPRS<PipelineResourceSignatureVkImpl>{pRefCounters, Desc});
                auto* pPRSVk = &m_pPRSVk->PRS;

                PipelineResourceSignatureSerializedDataVk SerializedData;
                pPRSVk->Serialize(SerializedData);
                AddPRSDesc(pPRSVk->GetDesc(), SerializedData.Base);
                CopyPRSSerializedData<PSOSerializerVk>(SerializedData, m_pPRSVk->Mem);
                break;
            }
#endif

#if METAL_SUPPORTED
            case RENDER_DEVICE_TYPE_METAL:
                CompilePRSMtl(pRefCounters, Desc);
                break;
#endif

            case RENDER_DEVICE_TYPE_UNDEFINED:
            case RENDER_DEVICE_TYPE_COUNT:
            default:
                LOG_ERROR_MESSAGE("Unexpected render device type");
                break;
        }
    }
}

SerializableResourceSignatureImpl::~SerializableResourceSignatureImpl()
{
}

#if D3D11_SUPPORTED
PipelineResourceSignatureD3D11Impl* SerializableResourceSignatureImpl::GetSignatureD3D11() const
{
    return &m_pPRSD3D11->PRS;
}

const SerializedMemory& SerializableResourceSignatureImpl::GetSerializedMemoryD3D11() const
{
    return m_pPRSD3D11->Mem;
}
#endif

#if D3D12_SUPPORTED
PipelineResourceSignatureD3D12Impl* SerializableResourceSignatureImpl::GetSignatureD3D12() const
{
    return &m_pPRSD3D12->PRS;
}

const SerializedMemory& SerializableResourceSignatureImpl::GetSerializedMemoryD3D12() const
{
    return m_pPRSD3D12->Mem;
}
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
PipelineResourceSignatureGLImpl* SerializableResourceSignatureImpl::GetSignatureGL() const
{
    return &m_pPRSGL->PRS;
}

const SerializedMemory& SerializableResourceSignatureImpl::GetSerializedMemoryGL() const
{
    return m_pPRSGL->Mem;
}
#endif

#if VULKAN_SUPPORTED
PipelineResourceSignatureVkImpl* SerializableResourceSignatureImpl::GetSignatureVk() const
{
    return &m_pPRSVk->PRS;
}

const SerializedMemory& SerializableResourceSignatureImpl::GetSerializedMemoryVk() const
{
    return m_pPRSVk->Mem;
}
#endif

} // namespace Diligent