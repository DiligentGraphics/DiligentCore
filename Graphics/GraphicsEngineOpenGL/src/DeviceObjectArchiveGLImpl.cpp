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
#include "RenderDeviceGLImpl.hpp"
#include "DeviceObjectArchiveGLImpl.hpp"
#include "PipelineResourceSignatureGLImpl.hpp"

namespace Diligent
{

DeviceObjectArchiveGLImpl::DeviceObjectArchiveGLImpl(IReferenceCounters* pRefCounters, IArchive* pSource) :
    DeviceObjectArchiveBase{pRefCounters, pSource, DeviceType::OpenGL}
{
}

DeviceObjectArchiveGLImpl::~DeviceObjectArchiveGLImpl()
{
}

void DeviceObjectArchiveGLImpl::UnpackResourceSignature(const ResourceSignatureUnpackInfo& DeArchiveInfo, IPipelineResourceSignature*& pSignature)
{
    DeviceObjectArchiveBase::UnpackResourceSignatureImpl(
        DeArchiveInfo, pSignature,
        [&DeArchiveInfo](PRSData& PRS, Serializer<SerializerMode::Read>& Ser, IPipelineResourceSignature*& pSignature) //
        {
            PipelineResourceSignatureSerializedDataGL SerializedData;
            SerializedData.Base = PRS.Serialized;
            PSOSerializerGL<SerializerMode::Read>::SerializePRS(Ser, SerializedData, &PRS.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            auto* pRenderDeviceGL = ClassPtrCast<RenderDeviceGLImpl>(DeArchiveInfo.pDevice);
            pRenderDeviceGL->CreatePipelineResourceSignature(PRS.Desc, SerializedData, &pSignature);
        });
}

template <SerializerMode Mode>
void PSOSerializerGL<Mode>::SerializePRS(
    Serializer<Mode>&                                 Ser,
    TQual<PipelineResourceSignatureSerializedDataGL>& Serialized,
    DynamicLinearAllocator*                           Allocator)
{
    Ser(Serialized.NumResources);

    auto* pAttribs = PSOSerializer_ArrayHelper<Mode>::Create(Serialized.pResourceAttribs, Serialized.NumResources, Allocator);
    for (Uint32 i = 0; i < Serialized.NumResources; ++i)
        Ser(pAttribs[i]);

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(Serialized) == 48, "Did you add a new member to PipelineResourceSignatureSerializedDataGL? Please add serialization here.");
#endif
}

template struct PSOSerializerGL<SerializerMode::Read>;
template struct PSOSerializerGL<SerializerMode::Write>;
template struct PSOSerializerGL<SerializerMode::Measure>;

} // namespace Diligent
