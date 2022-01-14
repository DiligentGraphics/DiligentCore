/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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
    return DeviceObjectArchiveBase::UnpackResourceSignatureImpl<RenderDeviceD3D11Impl, PSOSerializerD3D11<SerializerMode::Read>>(DeArchiveInfo, IsImplicit);
}

template <SerializerMode Mode>
void PSOSerializerD3D11<Mode>::SerializePRSInternalData(
    Serializer<Mode>&                                      Ser,
    ConstQual<PipelineResourceSignatureInternalDataD3D11>& InternalData,
    DynamicLinearAllocator*                                Allocator)
{
    PSOSerializer<Mode>::SerializePRSInternalData(Ser, InternalData, Allocator);

    Ser.SerializeArrayRaw(Allocator, InternalData.pResourceAttribs, InternalData.NumResources);
    Ser.SerializeArrayRaw(Allocator, InternalData.pImmutableSamplers, InternalData.NumImmutableSamplers);

    ASSERT_SIZEOF64(InternalData, 56, "Did you add a new member to PipelineResourceSignatureInternalDataD3D11? Please add serialization here.");
}

template struct PSOSerializerD3D11<SerializerMode::Read>;
template struct PSOSerializerD3D11<SerializerMode::Write>;
template struct PSOSerializerD3D11<SerializerMode::Measure>;

} // namespace Diligent
