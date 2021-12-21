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
#include "RenderDeviceD3D11Impl.hpp"
#include "DeviceObjectArchiveD3D11Impl.hpp"
#include "PipelineResourceSignatureD3D11Impl.hpp"
#include "PSOSerializer.hpp"

namespace Diligent
{

DeviceObjectArchiveD3D11Impl::DeviceObjectArchiveD3D11Impl(IReferenceCounters* pRefCounters, IArchive* pSource) :
    DeviceObjectArchiveBase{pRefCounters, pSource, DeviceType::Direct3D11}
{
}

DeviceObjectArchiveD3D11Impl::~DeviceObjectArchiveD3D11Impl()
{
}

RefCntAutoPtr<IPipelineResourceSignature> DeviceObjectArchiveD3D11Impl::UnpackResourceSignature(const ResourceSignatureUnpackInfo& DeArchiveInfo, bool IsImplicit)
{
    return DeviceObjectArchiveBase::UnpackResourceSignatureImpl(
        DeArchiveInfo, IsImplicit,
        [&DeArchiveInfo](PRSData& PRS, Serializer<SerializerMode::Read>& Ser) //
        {
            PipelineResourceSignatureSerializedDataD3D11 SerializedData{PRS.Serialized};
            PSOSerializerD3D11<SerializerMode::Read>::SerializePRSDesc(Ser, SerializedData, &PRS.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            auto* pRenderDeviceD3D11 = ClassPtrCast<RenderDeviceD3D11Impl>(DeArchiveInfo.pDevice);

            RefCntAutoPtr<IPipelineResourceSignature> pSignature;
            pRenderDeviceD3D11->CreatePipelineResourceSignature(PRS.Desc, SerializedData, &pSignature);
            return pSignature;
        });
}

template <SerializerMode Mode>
void PSOSerializerD3D11<Mode>::SerializePRSDesc(
    Serializer<Mode>&                                    Ser,
    TQual<PipelineResourceSignatureSerializedDataD3D11>& Serialized,
    DynamicLinearAllocator*                              Allocator)
{
    PSOSerializer<Mode>::SerializeArrayRaw(Ser, Allocator, Serialized.pResourceAttribs, Serialized.NumResources);
    PSOSerializer<Mode>::SerializeArrayRaw(Ser, Allocator, Serialized.pImmutableSamplers, Serialized.NumImmutableSamplers);

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(Serialized) == 56, "Did you add a new member to PipelineResourceSignatureSerializedDataD3D11? Please add serialization here.");
#endif
}

template struct PSOSerializerD3D11<SerializerMode::Read>;
template struct PSOSerializerD3D11<SerializerMode::Write>;
template struct PSOSerializerD3D11<SerializerMode::Measure>;

} // namespace Diligent
