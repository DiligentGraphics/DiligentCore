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

namespace Diligent
{
namespace
{

template <typename SignatureType>
using SignatureArray = std::array<RefCntAutoPtr<SignatureType>, MAX_RESOURCE_SIGNATURES>;

template <typename CreateInfoType, typename SignatureType>
static void SortResourceSignatures(const CreateInfoType& CreateInfo, SignatureArray<SignatureType>& Signatures, Uint32& SignaturesCount)
{
    for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
    {
        const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(CreateInfo.ppResourceSignatures[i]);
        VERIFY_EXPR(pSerPRS != nullptr);

        const auto& Desc = pSerPRS->GetDesc();

        Signatures[Desc.BindingIndex] = pSerPRS->template GetSignature<SignatureType>();
        SignaturesCount               = std::max(SignaturesCount, static_cast<Uint32>(Desc.BindingIndex) + 1);
    }
}

} // namespace


template <typename FnType>
bool ArchiverImpl::CreateDefaultResourceSignature(DefaultPRSInfo& DefPRS, const FnType& CreatePRS)
{
    try
    {
        RefCntAutoPtr<IPipelineResourceSignature> pDefaultPRS = CreatePRS(); // may throw exception
        if (pDefaultPRS == nullptr)
            return false;

        if (DefPRS.pPRS)
        {
            if (!(DefPRS.pPRS.RawPtr<SerializableResourceSignatureImpl>()->IsCompatible(*pDefaultPRS.RawPtr<SerializableResourceSignatureImpl>(), DefPRS.DeviceFlags)))
            {
                LOG_ERROR_MESSAGE("Default signatures does not match between different backends");
                return false;
            }
            pDefaultPRS = DefPRS.pPRS;
        }
        else
        {
            if (!CachePipelineResourceSignature(pDefaultPRS))
                return false;
            DefPRS.pPRS = pDefaultPRS;
        }
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to create default resource signature");
        return false;
    }
    return true;
}


template <typename ShaderType, typename... ArgTypes>
void SerializableShaderImpl::CreateShader(std::unique_ptr<ICompiledShader>& pShader,
                                          String&                           CompilationLog,
                                          const char*                       DeviceTypeName,
                                          IReferenceCounters*               pRefCounters,
                                          ShaderCreateInfo&                 ShaderCI,
                                          const ArgTypes&... Args)
{
    // Mem leak when used RefCntAutoPtr
    IDataBlob* pLog           = nullptr;
    ShaderCI.ppCompilerOutput = &pLog;
    try
    {
        pShader = std::make_unique<ShaderType>(pRefCounters, ShaderCI, Args...);
    }
    catch (...)
    {
        if (pLog && pLog->GetConstDataPtr())
        {
            CompilationLog += "Failed to compile ";
            CompilationLog += DeviceTypeName;
            CompilationLog += " shader:\n";
            CompilationLog += static_cast<const char*>(pLog->GetConstDataPtr());
        }
    }

    if (pLog)
        pLog->Release();
}


template <typename ImplType>
struct SerializableResourceSignatureImpl::TPRS final : IPRSWapper
{
    ImplType         PRS;
    SerializedMemory Mem;

    TPRS(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& SignatureDesc, SHADER_TYPE ShaderStages) :
        PRS{pRefCounters, nullptr, SignatureDesc, ShaderStages, true}
    {}

    IPipelineResourceSignature* GetPRS() override { return &PRS; }
    SerializedMemory const*     GetMem() override { return &Mem; }
};


namespace
{
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

} // namespace Diligent
