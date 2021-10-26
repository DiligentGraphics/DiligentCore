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

#include "pch.h"
#include "RenderDeviceD3D12Impl.hpp"
#include "DeviceObjectArchiveD3D12Impl.hpp"

namespace Diligent
{

static constexpr auto DevType = DeviceObjectArchiveBase::DeviceType::Direct3D12;

DeviceObjectArchiveD3D12Impl::DeviceObjectArchiveD3D12Impl(IReferenceCounters* pRefCounters, IArchiveSource* pSource) :
    DeviceObjectArchiveBase{pRefCounters, pSource}
{
}

DeviceObjectArchiveD3D12Impl::~DeviceObjectArchiveD3D12Impl()
{
}

void DeviceObjectArchiveD3D12Impl::UnpackGraphicsPSO(const PipelineStateUnpackInfo& DeArchiveInfo, RenderDeviceD3D12Impl* pRenderDeviceD3D12, IPipelineState** ppPSO)
{
    VERIFY_EXPR(pRenderDeviceD3D12 != nullptr);
    VERIFY_EXPR(ppPSO != nullptr);

    PSOData<GraphicsPipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadGraphicsPSOData(DeArchiveInfo.Name, PSO))
        return;

    LoadDeviceSpecificData(
        DevType,
        *PSO.pHeader,
        PSO.Allocator,
        "Graphics pipeline",
        [&](Serializer<SerializerMode::Read>& Ser) //
        {
            // AZ TODO

            pRenderDeviceD3D12->CreateGraphicsPipelineState(PSO.CreateInfo, ppPSO);
        });
}

void DeviceObjectArchiveD3D12Impl::UnpackComputePSO(const PipelineStateUnpackInfo& DeArchiveInfo, RenderDeviceD3D12Impl* pRenderDeviceD3D12, IPipelineState** ppPSO)
{
    VERIFY_EXPR(pRenderDeviceD3D12 != nullptr);
    VERIFY_EXPR(ppPSO != nullptr);

    PSOData<ComputePipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadComputePSOData(DeArchiveInfo.Name, PSO))
        return;

    LoadDeviceSpecificData(
        DevType,
        *PSO.pHeader,
        PSO.Allocator,
        "Compute pipeline",
        [&](Serializer<SerializerMode::Read>& Ser) //
        {
            // AZ TODO

            pRenderDeviceD3D12->CreateComputePipelineState(PSO.CreateInfo, ppPSO);
        });
}

void DeviceObjectArchiveD3D12Impl::UnpackRayTracingPSO(const PipelineStateUnpackInfo& DeArchiveInfo, RenderDeviceD3D12Impl* pRenderDeviceD3D12, IPipelineState** ppPSO)
{
    VERIFY_EXPR(pRenderDeviceD3D12 != nullptr);
    VERIFY_EXPR(ppPSO != nullptr);

    PSOData<RayTracingPipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadRayTracingPSOData(DeArchiveInfo.Name, PSO))
        return;

    LoadDeviceSpecificData(
        DevType,
        *PSO.pHeader,
        PSO.Allocator,
        "Ray tracing pipeline",
        [&](Serializer<SerializerMode::Read>& Ser) //
        {
            // AZ TODO

            pRenderDeviceD3D12->CreateRayTracingPipelineState(PSO.CreateInfo, ppPSO);
        });
}

void DeviceObjectArchiveD3D12Impl::UnpackResourceSignature(const ResourceSignatureUnpackInfo& DeArchiveInfo, RenderDeviceD3D12Impl* pRenderDeviceD3D12, IPipelineResourceSignature** ppSignature)
{
    VERIFY_EXPR(pRenderDeviceD3D12 != nullptr);
    VERIFY_EXPR(ppSignature != nullptr);

    PRSData PRS{GetRawAllocator()};
    if (!ReadPRSData(DeArchiveInfo.Name, PRS))
        return;

    LoadDeviceSpecificData(
        DevType,
        *PRS.pHeader,
        PRS.Allocator,
        "Resource signature",
        [&](Serializer<SerializerMode::Read>& Ser) //
        {
            PipelineResourceSignatureD3D12Impl::SerializedData SerializedData;
            SerializedData.Base = PRS.Serialized;
            SerializerD3D12Impl<SerializerMode::Read>::SerializePRS(Ser, SerializedData, &PRS.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            pRenderDeviceD3D12->CreatePipelineResourceSignature(PRS.Desc, SerializedData, ppSignature);
        });
}

template <SerializerMode Mode>
void DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<Mode>::SerializePRS(
    Serializer<Mode>&                                          Ser,
    TQual<PipelineResourceSignatureD3D12Impl::SerializedData>& Serialized,
    DynamicLinearAllocator*                                    Allocator)
{
    Ser(Serialized.NumResources);

    auto* pAttribs = ArraySerializerHelper<Mode>::Create(Serialized.pResourceAttribs, Serialized.NumResources, Allocator);
    for (Uint32 i = 0; i < Serialized.NumResources; ++i)
    {
        Ser(pAttribs[i]);
    }

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(Serialized) == 32, "Did you add a new member to PipelineResourceSignatureD3D12Impl::SerializedData? Please add serialization here.");
#endif
}

template struct DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<SerializerMode::Read>;
template struct DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<SerializerMode::Write>;
template struct DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<SerializerMode::Measure>;

} // namespace Diligent
