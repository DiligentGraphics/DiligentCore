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

#include "ArchiverImpl.hpp"

namespace Diligent
{
namespace
{

#define LOG_PSO_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of PSO is invalid: ", ##__VA_ARGS__)
#define VERIFY_PSO(Expr, ...)                     \
    do                                            \
    {                                             \
        if (!(Expr))                              \
        {                                         \
            LOG_PSO_ERROR_AND_THROW(__VA_ARGS__); \
        }                                         \
    } while (false)

template <typename PRSMapType>
void ValidatePipelineStateArchiveInfo(const PipelineStateCreateInfo&  PSOCreateInfo,
                                      const PipelineStateArchiveInfo& ArchiveInfo,
                                      const PRSMapType&               PRSMap,
                                      const Uint32                    ValidDeviceBits) noexcept(false)
{
    VERIFY_PSO(ArchiveInfo.DeviceBits != 0, "At least one bit must be set in DeviceBits");
    VERIFY_PSO((ArchiveInfo.DeviceBits & ValidDeviceBits) == ArchiveInfo.DeviceBits, "DeviceBits contains unsupported device type");

    VERIFY_PSO(PSOCreateInfo.PSODesc.Name != nullptr, "Pipeline name in PSOCreateInfo.PSODesc.Name must not be null");
    VERIFY_PSO((PSOCreateInfo.ResourceSignaturesCount != 0) == (PSOCreateInfo.ppResourceSignatures != nullptr),
               "ppResourceSignatures must not be null if ResourceSignaturesCount is not zero");

    // AZ TODO: serialize default PRS
    VERIFY_PSO((PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers == 0 &&
                PSOCreateInfo.PSODesc.ResourceLayout.NumVariables == 0 &&
                PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC),
               "Default resource signature is not supported");

    std::bitset<MAX_RESOURCE_SIGNATURES> PRSExists{0};
    for (Uint32 i = 0; i < PSOCreateInfo.ResourceSignaturesCount; ++i)
    {
        VERIFY_PSO(PSOCreateInfo.ppResourceSignatures[i] != nullptr, "ppResourceSignatures[", i, "] must not be null");

        const auto& Desc = PSOCreateInfo.ppResourceSignatures[i]->GetDesc();
        VERIFY_EXPR(Desc.BindingIndex < PRSExists.size());

        VERIFY_PSO(!PRSExists[Desc.BindingIndex], "PRS binding index must be unique");
        PRSExists[Desc.BindingIndex] = true;
    }
}

} // namespace


const SerializedMemory& ArchiverImpl::RPData::GetSharedData() const
{
    return pRP->GetSharedSerializedMemory();
}

void ArchiverImpl::SerializeShaderBytecode(TShaderIndices& ShaderIndices, DeviceType DevType, const ShaderCreateInfo& CI, const void* Bytecode, size_t BytecodeSize)
{
    auto&                        ShaderMap       = m_Shaders[static_cast<Uint32>(DevType)].Map;
    auto&                        RawMemAllocator = GetRawAllocator();
    const SHADER_SOURCE_LANGUAGE SourceLanguage  = SHADER_SOURCE_LANGUAGE_DEFAULT;
    const SHADER_COMPILER        ShaderCompiler  = SHADER_COMPILER_DEFAULT;

    Serializer<SerializerMode::Measure> MeasureSer;
    MeasureSer(CI.Desc.ShaderType, CI.EntryPoint, SourceLanguage, ShaderCompiler);

    const auto   Size   = MeasureSer.GetSize(nullptr) + BytecodeSize;
    void*        Ptr    = ALLOCATE_RAW(RawMemAllocator, "", Size);
    const Uint8* pBytes = static_cast<const Uint8*>(Bytecode);

    Serializer<SerializerMode::Write> Ser{Ptr, Size};
    Ser(CI.Desc.ShaderType, CI.EntryPoint, SourceLanguage, ShaderCompiler);

    for (size_t s = 0; s < BytecodeSize; ++s)
        Ser(pBytes[s]);

    VERIFY_EXPR(Ser.IsEnd());

    ShaderKey Key;
    Key.Data = SerializedMemory{Ptr, Size};

    auto Iter = ShaderMap.emplace(std::move(Key), ShaderMap.size()).first;
    ShaderIndices.push_back(StaticCast<Uint32>(Iter->second));
}

void ArchiverImpl::SerializeShaderSource(TShaderIndices& ShaderIndices, DeviceType DevType, const ShaderCreateInfo& CI)
{
    auto& ShaderMap       = m_Shaders[static_cast<Uint32>(DevType)].Map;
    auto& RawMemAllocator = GetRawAllocator();

    VERIFY_EXPR(CI.SourceLength > 0);
    VERIFY_EXPR(CI.Macros == nullptr); // AZ TODO: not supported
    VERIFY_EXPR(CI.UseCombinedTextureSamplers == true);
    VERIFY_EXPR(String{"_sampler"} == CI.CombinedSamplerSuffix);

    Serializer<SerializerMode::Measure> MeasureSer;
    MeasureSer(CI.Desc.ShaderType, CI.EntryPoint, CI.SourceLanguage, CI.ShaderCompiler);

    const auto   BytecodeSize = CI.SourceLength * sizeof(*CI.Source);
    const auto   Size         = MeasureSer.GetSize(nullptr) + BytecodeSize;
    void*        Ptr          = ALLOCATE_RAW(RawMemAllocator, "", Size);
    const Uint8* pBytes       = reinterpret_cast<const Uint8*>(CI.Source);

    Serializer<SerializerMode::Write> Ser{Ptr, Size};
    Ser(CI.Desc.ShaderType, CI.EntryPoint, CI.SourceLanguage, CI.ShaderCompiler);

    for (size_t s = 0; s < BytecodeSize; ++s)
        Ser(pBytes[s]);

    VERIFY_EXPR(Ser.IsEnd());

    ShaderKey Key;
    Key.Data = SerializedMemory{Ptr, Size};

    auto Iter = ShaderMap.emplace(std::move(Key), ShaderMap.size()).first;
    ShaderIndices.push_back(StaticCast<Uint32>(Iter->second));
}

#if VULKAN_SUPPORTED
template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersVk(const CreateInfoType& CreateInfo, TShaderIndices& ShaderIndices)
{
    struct ShaderStageInfoVk : PipelineStateVkImpl::ShaderStageInfo
    {
        ShaderStageInfoVk() :
            ShaderStageInfo{} {}

        ShaderStageInfoVk(const SerializableShaderImpl* pShader) :
            ShaderStageInfo{pShader->GetShaderVk()},
            Serializable{pShader}
        {}

        void Append(const SerializableShaderImpl* pShader)
        {
            ShaderStageInfo::Append(pShader->GetShaderVk());
            Serializable.push_back(pShader);
        }

        std::vector<const SerializableShaderImpl*> Serializable;
    };

    std::vector<ShaderStageInfoVk> ShaderStages;
    SHADER_TYPE                    ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateVkImpl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    PipelineStateVkImpl::TShaderStages ShaderStagesVk{ShaderStages.size()};
    for (size_t i = 0; i < ShaderStagesVk.size(); ++i)
    {
        auto& Src   = ShaderStages[i];
        auto& Dst   = ShaderStagesVk[i];
        Dst.Type    = Src.Type;
        Dst.Shaders = std::move(Src.Shaders);
        Dst.SPIRVs  = std::move(Src.SPIRVs);
    }

    try
    {
        std::array<const PipelineResourceSignatureVkImpl*, MAX_RESOURCE_SIGNATURES> Signatures              = {};
        PipelineStateVkImpl::TBindIndexToDescSetIndex                               BindIndexToDescSetIndex = {};

        Uint32 SignaturesCount = 0;
        for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
        {
            const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(CreateInfo.ppResourceSignatures[i]);
            const auto& Desc    = pSerPRS->GetDesc();

            Signatures[Desc.BindingIndex] = pSerPRS->GetSignatureVk();
            SignaturesCount               = std::max(SignaturesCount, static_cast<Uint32>(Desc.BindingIndex) + 1);
        }

        // Same as PipelineLayoutVk::Create()
        Uint32 DescSetLayoutCount = 0;
        for (Uint32 i = 0; i < SignaturesCount; ++i)
        {
            const auto& pSignature = Signatures[i];
            if (pSignature == nullptr)
                continue;

            VERIFY_EXPR(pSignature->GetDesc().BindingIndex == i);
            BindIndexToDescSetIndex[i] = StaticCast<PipelineStateVkImpl::TBindIndexToDescSetIndex::value_type>(DescSetLayoutCount);

            for (auto SetId : {PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_STATIC_MUTABLE, PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_DYNAMIC})
            {
                if (pSignature->GetDescriptorSetSize(SetId) != ~0u)
                    ++DescSetLayoutCount;
            }
        }
        VERIFY_EXPR(DescSetLayoutCount <= MAX_RESOURCE_SIGNATURES * 2);
        VERIFY_EXPR(DescSetLayoutCount >= CreateInfo.ResourceSignaturesCount);

        PipelineStateVkImpl::RemapShaderResources(ShaderStagesVk,
                                                  Signatures.data(),
                                                  SignaturesCount,
                                                  BindIndexToDescSetIndex,
                                                  true); // bStripReflection
    }
    catch (...)
    {
        return false;
    }

    for (size_t j = 0; j < ShaderStagesVk.size(); ++j)
    {
        const auto& Stage = ShaderStagesVk[j];
        for (size_t i = 0; i < Stage.Count(); ++i)
        {
            const auto& CI    = ShaderStages[j].Serializable[i]->GetCreateInfo();
            const auto& SPIRV = Stage.SPIRVs[i];

            SerializeShaderBytecode(ShaderIndices, DeviceType::Vulkan, CI, SPIRV.data(), SPIRV.size() * sizeof(SPIRV[0]));
        }
    }

    // AZ TODO: map ray tracing shaders to shader indices

    return true;
}
#endif // VULKAN_SUPPORTED


#if D3D11_SUPPORTED
struct ShaderStageInfoD3D11
{
    ShaderStageInfoD3D11() {}

    ShaderStageInfoD3D11(const SerializableShaderImpl* _pShader) :
        Type{_pShader->GetDesc().ShaderType},
        pShader{_pShader->GetShaderD3D11()},
        pSerializable{_pShader}
    {}

    // Needed only for ray tracing
    void Append(const SerializableShaderImpl*) {}

    Uint32 Count() const { return 1; }

    SHADER_TYPE                   Type          = SHADER_TYPE_UNKNOWN;
    ShaderD3D11Impl*              pShader       = nullptr;
    const SerializableShaderImpl* pSerializable = nullptr;

    friend SHADER_TYPE GetShaderStageType(const ShaderStageInfoD3D11& Stage) { return Stage.Type; }
};

template <typename CreateInfoType>
void InitD3D11ShaderResourceCounters(const CreateInfoType& CreateInfo, D3D11ShaderResourceCounters& ResCounters)
{}

void InitD3D11ShaderResourceCounters(const GraphicsPipelineStateCreateInfo& CreateInfo, D3D11ShaderResourceCounters& ResCounters)
{
    VERIFY_EXPR(CreateInfo.PSODesc.IsAnyGraphicsPipeline());

    // In Direct3D11, UAVs use the same register space as render targets
    ResCounters[D3D11_RESOURCE_RANGE_UAV][PSInd] = CreateInfo.GraphicsPipeline.NumRenderTargets;
}

template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersD3D11(const CreateInfoType& CreateInfo, TShaderIndices& ShaderIndices)
{
    std::vector<ShaderStageInfoD3D11> ShaderStages;
    SHADER_TYPE                       ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateD3D11Impl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    std::vector<ShaderD3D11Impl*>  ShadersD3D11{ShaderStages.size()};
    std::vector<CComPtr<ID3DBlob>> ShaderBytecode{ShaderStages.size()};
    for (size_t i = 0; i < ShadersD3D11.size(); ++i)
    {
        auto& Src = ShaderStages[i];
        auto& Dst = ShadersD3D11[i];
        Dst       = Src.pShader;
    }

    try
    {
        std::array<RefCntAutoPtr<PipelineResourceSignatureD3D11Impl>, MAX_RESOURCE_SIGNATURES> Signatures   = {};
        std::array<D3D11ShaderResourceCounters, MAX_RESOURCE_SIGNATURES>                       BaseBindings = {};

        D3D11ShaderResourceCounters ResCounters = {};
        InitD3D11ShaderResourceCounters(CreateInfo, ResCounters);

        Uint32 SignaturesCount = 0;
        for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
        {
            const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(CreateInfo.ppResourceSignatures[i]);
            const auto& Desc    = pSerPRS->GetDesc();

            Signatures[Desc.BindingIndex] = pSerPRS->GetSignatureD3D11();
            SignaturesCount               = std::max(SignaturesCount, static_cast<Uint32>(Desc.BindingIndex) + 1);
        }

        for (Uint32 i = 0; i < SignaturesCount; ++i)
        {
            const PipelineResourceSignatureD3D11Impl* const pSignature = Signatures[i];
            if (pSignature == nullptr)
                continue;

            BaseBindings[i] = ResCounters;
            pSignature->ShiftBindings(ResCounters);
        }

        PipelineStateD3D11Impl::RemapShaderResources(
            ShadersD3D11,
            Signatures.data(),
            SignaturesCount,
            BaseBindings.data(),
            [&ShaderBytecode](size_t ShaderIdx, ShaderD3D11Impl* pShader, ID3DBlob* pPatchedBytecode) //
            {
                ShaderBytecode[ShaderIdx] = pPatchedBytecode;
            });
    }
    catch (...)
    {
        return false;
    }

    for (size_t i = 0; i < ShadersD3D11.size(); ++i)
    {
        const auto& CI        = ShaderStages[i].pSerializable->GetCreateInfo();
        const auto& pBytecode = ShaderBytecode[i];

        SerializeShaderBytecode(ShaderIndices, DeviceType::Direct3D11, CI, pBytecode->GetBufferPointer(), pBytecode->GetBufferSize());
    }
    return true;
}
#endif // D3D11_SUPPORTED


#if D3D12_SUPPORTED
template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersD3D12(const CreateInfoType& CreateInfo, TShaderIndices& ShaderIndices)
{
    struct ShaderStageInfoD3D12 : PipelineStateD3D12Impl::ShaderStageInfo
    {
        ShaderStageInfoD3D12() :
            ShaderStageInfo{} {}

        ShaderStageInfoD3D12(const SerializableShaderImpl* pShader) :
            ShaderStageInfo{pShader->GetShaderD3D12()},
            Serializable{pShader}
        {}

        void Append(const SerializableShaderImpl* pShader)
        {
            ShaderStageInfo::Append(pShader->GetShaderD3D12());
            Serializable.push_back(pShader);
        }

        std::vector<const SerializableShaderImpl*> Serializable;
    };

    std::vector<ShaderStageInfoD3D12> ShaderStages;
    SHADER_TYPE                       ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateD3D12Impl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    PipelineStateD3D12Impl::TShaderStages ShaderStagesD3D12{ShaderStages.size()};
    for (size_t i = 0; i < ShaderStagesD3D12.size(); ++i)
    {
        auto& Src     = ShaderStages[i];
        auto& Dst     = ShaderStagesD3D12[i];
        Dst.Type      = Src.Type;
        Dst.Shaders   = std::move(Src.Shaders);
        Dst.ByteCodes = std::move(Src.ByteCodes);
    }

    try
    {
        std::array<RefCntAutoPtr<PipelineResourceSignatureD3D12Impl>, MAX_RESOURCE_SIGNATURES> Signatures = {};

        Uint32 SignaturesCount = 0;
        for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
        {
            const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(CreateInfo.ppResourceSignatures[i]);
            const auto& Desc    = pSerPRS->GetDesc();

            Signatures[Desc.BindingIndex] = pSerPRS->GetSignatureD3D12();
            SignaturesCount               = std::max(SignaturesCount, static_cast<Uint32>(Desc.BindingIndex) + 1);
        }

        RootSignatureD3D12 RootSig{nullptr, nullptr, Signatures.data(), SignaturesCount, 0};
        PipelineStateD3D12Impl::RemapShaderResources(ShaderStagesD3D12,
                                                     Signatures.data(),
                                                     SignaturesCount,
                                                     RootSig,
                                                     m_pSerializationDevice->GetDxCompilerForDirect3D12());
    }
    catch (...)
    {
        return false;
    }

    for (size_t j = 0; j < ShaderStagesD3D12.size(); ++j)
    {
        const auto& Stage = ShaderStagesD3D12[j];
        for (size_t i = 0; i < Stage.Count(); ++i)
        {
            const auto& CI        = ShaderStages[j].Serializable[i]->GetCreateInfo();
            const auto& pBytecode = Stage.ByteCodes[i];

            SerializeShaderBytecode(ShaderIndices, DeviceType::Direct3D12, CI, pBytecode->GetBufferPointer(), pBytecode->GetBufferSize());
        }
    }

    // AZ TODO: map ray tracing shaders to shader indices

    return true;
}
#endif // D3D12_SUPPORTED


#if GL_SUPPORTED || GLES_SUPPORTED
struct ShaderStageInfoGL
{
    ShaderStageInfoGL() {}

    ShaderStageInfoGL(const SerializableShaderImpl* _pShader) :
        Type{_pShader->GetDesc().ShaderType},
        pShader{_pShader}
    {}

    // Needed only for ray tracing
    void Append(const SerializableShaderImpl*) {}

    Uint32 Count() const { return 1; }

    SHADER_TYPE                   Type    = SHADER_TYPE_UNKNOWN;
    const SerializableShaderImpl* pShader = nullptr;

    friend SHADER_TYPE GetShaderStageType(const ShaderStageInfoGL& Stage) { return Stage.Type; }
};

template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersGL(const CreateInfoType& CreateInfo, TShaderIndices& ShaderIndices)
{
    std::vector<ShaderStageInfoGL> ShaderStages;
    SHADER_TYPE                    ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateGLImpl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    for (size_t i = 0; i < ShaderStages.size(); ++i)
    {
        SerializeShaderSource(ShaderIndices, DeviceType::OpenGL, ShaderStages[i].pShader->GetCreateInfo());
    }
    return true;
}
#endif // GL_SUPPORTED || GLES_SUPPORTED


void ArchiverImpl::SerializeShadersForPSO(const TShaderIndices& ShaderIndices, SerializedMemory& DeviceData) const
{
    auto& RawMemAllocator = GetRawAllocator();

    ShaderIndexArray Indices{ShaderIndices.data(), static_cast<Uint32>(ShaderIndices.size())};

    Serializer<SerializerMode::Measure> MeasureSer;
    SerializerImpl<SerializerMode::Measure>::SerializeShaders(MeasureSer, Indices, nullptr);

    const size_t SerSize = MeasureSer.GetSize(nullptr);
    void*        SerPtr  = ALLOCATE_RAW(RawMemAllocator, "", SerSize);

    Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
    SerializerImpl<SerializerMode::Write>::SerializeShaders(Ser, Indices, nullptr);
    VERIFY_EXPR(Ser.IsEnd());

    DeviceData = SerializedMemory{SerPtr, SerSize};
}

bool ArchiverImpl::AddRenderPass(IRenderPass* pRP)
{
    DEV_CHECK_ERR(pRP != nullptr, "pRP must not be null");
    if (pRP == nullptr)
        return false;

    auto* pRPImpl         = ClassPtrCast<SerializableRenderPassImpl>(pRP);
    auto  IterAndInserted = m_RPMap.emplace(String{pRPImpl->GetDesc().Name}, RPData{});
    if (!IterAndInserted.second)
    {
        if (IterAndInserted.first->second.pRP != pRPImpl)
        {
            LOG_ERROR_MESSAGE("Render pass must have unique name");
            return false;
        }
        else
            return true;
    }

    IterAndInserted.first->second.pRP = pRPImpl;
    return true;
}

namespace
{

template <SerializerMode Mode>
void PSOSerializer(Serializer<Mode>&                                 Ser,
                   const GraphicsPipelineStateCreateInfo&            PSOCreateInfo,
                   std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    const char* RPName = PSOCreateInfo.GraphicsPipeline.pRenderPass != nullptr ? PSOCreateInfo.GraphicsPipeline.pRenderPass->GetDesc().Name : "";
    SerializerImpl<Mode>::SerializeGraphicsPSO(Ser, PSOCreateInfo, PRSNames, RPName, nullptr);
}

template <SerializerMode Mode>
void PSOSerializer(Serializer<Mode>&                                 Ser,
                   const ComputePipelineStateCreateInfo&             PSOCreateInfo,
                   std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    SerializerImpl<Mode>::SerializeComputePSO(Ser, PSOCreateInfo, PRSNames, nullptr);
}

template <SerializerMode Mode>
void PSOSerializer(Serializer<Mode>&                                 Ser,
                   const TilePipelineStateCreateInfo&                PSOCreateInfo,
                   std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    SerializerImpl<Mode>::SerializeTilePSO(Ser, PSOCreateInfo, PRSNames, nullptr);
}

template <SerializerMode Mode>
void PSOSerializer(Serializer<Mode>&                                 Ser,
                   const RayTracingPipelineStateCreateInfo&          PSOCreateInfo,
                   std::array<const char*, MAX_RESOURCE_SIGNATURES>& PRSNames)
{
    SerializerImpl<Mode>::SerializeRayTracingPSO(Ser, PSOCreateInfo, PRSNames, nullptr);
}

} // namespace

template <typename CreateInfoType>
bool ArchiverImpl::SerializePSO(std::unordered_map<String, TPSOData<CreateInfoType>>& PSOMap,
                                const CreateInfoType&                                 PSOCreateInfo,
                                const PipelineStateArchiveInfo&                       ArchiveInfo) noexcept
{
    try
    {
        ValidatePipelineStateArchiveInfo(PSOCreateInfo, ArchiveInfo, m_PRSMap, m_pSerializationDevice->GetValidDeviceBits());
        ValidatePSOCreateInfo(m_pSerializationDevice->GetDevice(), PSOCreateInfo);
    }
    catch (...)
    {
        return false;
    }

    auto IterAndInserted = PSOMap.emplace(String{PSOCreateInfo.PSODesc.Name}, TPSOData<CreateInfoType>{});
    if (!IterAndInserted.second)
    {
        LOG_ERROR_MESSAGE("Pipeline must have unique name");
        return false;
    }

    auto& Data            = IterAndInserted.first->second;
    auto& RawMemAllocator = GetRawAllocator();

    if (!Data.SharedData)
    {
        TPRSNames PRSNames = {};
        for (Uint32 i = 0; i < PSOCreateInfo.ResourceSignaturesCount; ++i)
        {
            if (!AddPipelineResourceSignature(PSOCreateInfo.ppResourceSignatures[i]))
                return false;
            PRSNames[i] = PSOCreateInfo.ppResourceSignatures[i]->GetDesc().Name;
        }

        Serializer<SerializerMode::Measure> MeasureSer;
        PSOSerializer(MeasureSer, PSOCreateInfo, PRSNames);

        const size_t SerSize = MeasureSer.GetSize(nullptr);
        void*        SerPtr  = ALLOCATE_RAW(RawMemAllocator, "", SerSize);

        Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
        PSOSerializer(Ser, PSOCreateInfo, PRSNames);
        VERIFY_EXPR(Ser.IsEnd());

        Data.SharedData = SerializedMemory{SerPtr, SerSize};
    }

    for (Uint32 Bits = ArchiveInfo.DeviceBits; Bits != 0;)
    {
        const auto Type = static_cast<RENDER_DEVICE_TYPE>(PlatformMisc::GetLSB(ExtractLSB(Bits)));

        static_assert(RENDER_DEVICE_TYPE_COUNT == 7, "Please update the switch below to handle the new render device type");
        switch (Type)
        {
#if D3D11_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D11:
            {
                TShaderIndices ShaderIndices;
                if (!PatchShadersD3D11(PSOCreateInfo, ShaderIndices))
                    return false;

                SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[static_cast<Uint32>(DeviceType::Direct3D11)]);
                break;
            }
#endif

#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
            {
                TShaderIndices ShaderIndices;
                if (!PatchShadersD3D12(PSOCreateInfo, ShaderIndices))
                    return false;

                SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[static_cast<Uint32>(DeviceType::Direct3D12)]);
                break;
            }
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
            {
                TShaderIndices ShaderIndices;
                if (!PatchShadersGL(PSOCreateInfo, ShaderIndices))
                    return false;

                SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[static_cast<Uint32>(DeviceType::OpenGL)]);
                break;
            }
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
            {
                TShaderIndices ShaderIndices;
                if (!PatchShadersVk(PSOCreateInfo, ShaderIndices))
                    return false;

                SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[static_cast<Uint32>(DeviceType::Vulkan)]);
                break;
            }
#endif

#if METAL_SUPPORTED
            case RENDER_DEVICE_TYPE_METAL:
                // AZ TODO
                break;
#endif

            case RENDER_DEVICE_TYPE_UNDEFINED:
            case RENDER_DEVICE_TYPE_COUNT:
            default:
                LOG_ERROR_MESSAGE("Unexpected render device type");
                break;
        }
    }
    return true;
}

Bool ArchiverImpl::ArchiveGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
                                                const PipelineStateArchiveInfo&        ArchiveInfo)
{
    if (PSOCreateInfo.GraphicsPipeline.pRenderPass != nullptr)
    {
        if (!AddRenderPass(PSOCreateInfo.GraphicsPipeline.pRenderPass))
            return false;
    }

    return SerializePSO(m_GraphicsPSOMap, PSOCreateInfo, ArchiveInfo);
}

Bool ArchiverImpl::ArchiveComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo,
                                               const PipelineStateArchiveInfo&       ArchiveInfo)
{
    return SerializePSO(m_ComputePSOMap, PSOCreateInfo, ArchiveInfo);
}

Bool ArchiverImpl::ArchiveRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
                                                  const PipelineStateArchiveInfo&          ArchiveInfo)
{
    return SerializePSO(m_RayTracingPSOMap, PSOCreateInfo, ArchiveInfo);
}

Bool ArchiverImpl::ArchiveTilePipelineState(const TilePipelineStateCreateInfo& PSOCreateInfo,
                                            const PipelineStateArchiveInfo&    ArchiveInfo)
{
    return SerializePSO(m_TilePSOMap, PSOCreateInfo, ArchiveInfo);
}

} // namespace Diligent
