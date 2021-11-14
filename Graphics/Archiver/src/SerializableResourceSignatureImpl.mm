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
#include "RenderDeviceMtlImpl.hpp"
#include "PipelineResourceSignatureMtlImpl.hpp"
#include "DeviceObjectArchiveMtlImpl.hpp"

namespace Diligent
{
    
struct SerializableResourceSignatureImpl::PRSMtlImpl final : IPRSMtl
{
    PipelineResourceSignatureMtlImpl PRS;
    SerializedMemory                 Mem;

    PRSMtlImpl(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& SignatureDesc) :
        PRS{pRefCounters, nullptr, SignatureDesc, nullptr, SHADER_TYPE_UNKNOWN, true}
    {}

    PipelineResourceSignatureMtlImpl* GetPRS() override { return &PRS; }
    SerializedMemory const&           GetMem() override { return Mem; }
    
};

void SerializableResourceSignatureImpl::CompilePRSMtl(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc)
{
    auto* pPRSMtl = new PRSMtlImpl{pRefCounters, Desc};
    m_pPRSMtl.reset(pPRSMtl);

    PipelineResourceSignatureSerializedDataMtl SerializedData;
    pPRSMtl->PRS.Serialize(SerializedData);
    AddPRSDesc(pPRSMtl->PRS.GetDesc(), SerializedData.Base);
    
    Serializer<SerializerMode::Measure> MeasureSer;
    PSOSerializerMtl<SerializerMode::Measure>::SerializePRS(MeasureSer, SerializedData, nullptr);

    const size_t SerSize = MeasureSer.GetSize(nullptr);
    void*        SerPtr  = ALLOCATE_RAW(GetRawAllocator(), "", SerSize);

    Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
    PSOSerializerMtl<SerializerMode::Write>::SerializePRS(Ser, SerializedData, nullptr);
    VERIFY_EXPR(Ser.IsEnd());

    pPRSMtl->Mem = SerializedMemory{SerPtr, SerSize};
}

} // namespace Diligent