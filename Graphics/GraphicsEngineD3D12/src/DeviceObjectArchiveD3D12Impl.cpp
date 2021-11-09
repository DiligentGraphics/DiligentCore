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
#include "PipelineResourceSignatureD3D12Impl.hpp"

namespace Diligent
{

DeviceObjectArchiveD3D12Impl::DeviceObjectArchiveD3D12Impl(IReferenceCounters* pRefCounters, IArchive* pSource) :
    DeviceObjectArchiveBase{pRefCounters, pSource, DeviceType::Direct3D12}
{
}

DeviceObjectArchiveD3D12Impl::~DeviceObjectArchiveD3D12Impl()
{
}

void DeviceObjectArchiveD3D12Impl::UnpackResourceSignature(const ResourceSignatureUnpackInfo& DeArchiveInfo, IPipelineResourceSignature*& pSignature)
{
    DeviceObjectArchiveBase::UnpackResourceSignatureImpl(
        DeArchiveInfo, pSignature,
        [&DeArchiveInfo](PRSData& PRS, Serializer<SerializerMode::Read>& Ser, IPipelineResourceSignature*& pSignature) //
        {
            PipelineResourceSignatureSerializedDataD3D12 SerializedData;
            SerializedData.Base = PRS.Serialized;
            SerializerD3D12Impl<SerializerMode::Read>::SerializePRS(Ser, SerializedData, &PRS.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            auto* pRenderDeviceD3D12 = ClassPtrCast<RenderDeviceD3D12Impl>(DeArchiveInfo.pDevice);
            pRenderDeviceD3D12->CreatePipelineResourceSignature(PRS.Desc, SerializedData, &pSignature);
        });
}

template <SerializerMode Mode>
void DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<Mode>::SerializePRS(
    Serializer<Mode>&                                    Ser,
    TQual<PipelineResourceSignatureSerializedDataD3D12>& Serialized,
    DynamicLinearAllocator*                              Allocator)
{
    Ser(Serialized.NumResources);

    auto* pAttribs = ArraySerializerHelper<Mode>::Create(Serialized.pResourceAttribs, Serialized.NumResources, Allocator);
    for (Uint32 i = 0; i < Serialized.NumResources; ++i)
    {
        Ser(pAttribs[i]);
    }

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(Serialized) == 32, "Did you add a new member to PipelineResourceSignatureSerializedDataD3D12? Please add serialization here.");
#endif
}

template struct DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<SerializerMode::Read>;
template struct DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<SerializerMode::Write>;
template struct DeviceObjectArchiveD3D12Impl::SerializerD3D12Impl<SerializerMode::Measure>;

} // namespace Diligent
