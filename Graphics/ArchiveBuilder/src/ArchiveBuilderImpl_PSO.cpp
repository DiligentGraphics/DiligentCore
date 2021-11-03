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

#include "ArchiveBuilderImpl.hpp"

namespace Diligent
{
namespace
{

template <typename PRSMapType>
void ValidatePipelineStateArchiveInfo(const PipelineStateCreateInfo&  PSOCreateInfo,
                                      const PipelineStateArchiveInfo& ArchiveInfo,
                                      const PRSMapType&               PRSMap,
                                      const Uint32                    ValidDeviceBits)
{
    DEV_CHECK_ERR(ArchiveInfo.DeviceBits != 0, "At least one bit must be set in DeviceBits");
    DEV_CHECK_ERR((ArchiveInfo.DeviceBits & ValidDeviceBits) == ArchiveInfo.DeviceBits, "DeviceBits contains unsupported device type");

    DEV_CHECK_ERR(PSOCreateInfo.PSODesc.Name != nullptr, "Pipeline name in PSOCreateInfo.PSODesc.Name must not be null");
    DEV_CHECK_ERR((PSOCreateInfo.ResourceSignaturesCount != 0) == (PSOCreateInfo.ppResourceSignatures != nullptr),
                  "ppResourceSignatures must not be null if ResourceSignaturesCount is not zero");

    // AZ TODO: serialize default PRS
    DEV_CHECK_ERR((PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers == 0 &&
                   PSOCreateInfo.PSODesc.ResourceLayout.NumVariables == 0 &&
                   PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC),
                  "Default resource signature is not supported");

    std::bitset<MAX_RESOURCE_SIGNATURES> PRSExists{0};
    for (Uint32 i = 0; i < PSOCreateInfo.ResourceSignaturesCount; ++i)
    {
        DEV_CHECK_ERR(PSOCreateInfo.ppResourceSignatures[i] != nullptr, "ppResourceSignatures[", i, "] must not be null");

        const auto& Desc = PSOCreateInfo.ppResourceSignatures[i]->GetDesc();
        VERIFY_EXPR(Desc.BindingIndex < PRSExists.size());

        DEV_CHECK_ERR(!PRSExists[Desc.BindingIndex], "PRS binding index must be unique");
        PRSExists[Desc.BindingIndex] = true;
    }
}

} // namespace


const SerializedMemory& ArchiveBuilderImpl::RPData::GetSharedData() const
{
    return pRP->GetSharedSerializedMemory();
}

#if VULKAN_SUPPORTED
template <typename CreateInfoType>
bool ArchiveBuilderImpl::PatchShadersVk(const CreateInfoType&           CreateInfo,
                                        const PipelineStateArchiveInfo& ArchiveInfo,
                                        TShaderIndices&                 ShaderIndices)
{
    struct ShaderStageInfoVk : PipelineStateVkImpl::ShaderStageInfo
    {
        ShaderStageInfoVk() :
            ShaderStageInfo{} {}

        ShaderStageInfoVk(const SerializableShaderImpl* pShader) :
            ShaderStageInfo{pShader->GetShaderVk()}
        {}

        void Append(const SerializableShaderImpl* pShader)
        {
            ShaderStageInfo::Append(pShader->GetShaderVk());
        }
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

    std::array<const PipelineResourceSignatureVkImpl*, MAX_RESOURCE_SIGNATURES> Signatures              = {};
    PipelineStateVkImpl::TBindIndexToDescSetIndex                               BindIndexToDescSetIndex = {};

    try
    {
        for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
        {
            const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(CreateInfo.ppResourceSignatures[i]);
            const auto& Desc    = pSerPRS->GetDesc();

            Signatures[Desc.BindingIndex] = pSerPRS->GetSignatureVk();
        }

        Uint32 DescSetLayoutCount = 0;
        for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
        {
            const auto& pSignature = Signatures[i];
            if (pSignature == nullptr)
                continue;

            VERIFY_EXPR(pSignature->GetDesc().BindingIndex == i);
            BindIndexToDescSetIndex[i] = StaticCast<PipelineStateVkImpl::TBindIndexToDescSetIndex::value_type>(DescSetLayoutCount);

            for (auto SetId : {PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_STATIC_MUTABLE, PipelineResourceSignatureVkImpl::DESCRIPTOR_SET_ID_DYNAMIC})
            {
                if (pSignature->HasDescriptorSet(SetId))
                    ++DescSetLayoutCount;
            }
        }
        VERIFY_EXPR(DescSetLayoutCount <= MAX_RESOURCE_SIGNATURES * 2);

        PipelineStateVkImpl::RemapShaderResources(ShaderStagesVk,
                                                  Signatures.data(),
                                                  CreateInfo.ResourceSignaturesCount,
                                                  BindIndexToDescSetIndex,
                                                  true); // bStripReflection
    }
    catch (...)
    {
        return false;
    }

    auto& ShaderMap       = m_Shaders[Uint32{DeviceType::Vulkan}].Map;
    auto& RawMemAllocator = GetRawAllocator();

    for (size_t j = 0; j < ShaderStagesVk.size(); ++j)
    {
        const auto& Stage = ShaderStagesVk[j];
        for (size_t i = 0; i < Stage.Count(); ++i)
        {
            const char* Entry = Stage.Shaders[i]->GetEntryPoint();
            const auto& SPIRV = Stage.SPIRVs[i];

            Serializer<SerializerMode::Measure> MeasureSer;
            MeasureSer(Stage.Type, Entry);

            const auto Size = MeasureSer.GetSize(nullptr) + SPIRV.size() * sizeof(SPIRV[0]);
            void*      Ptr  = ALLOCATE_RAW(RawMemAllocator, "", Size);

            Serializer<SerializerMode::Write> Ser{Ptr, Size};
            Ser(Stage.Type, Entry);

            for (size_t s = 0; s < SPIRV.size(); ++s)
                Ser(SPIRV[s]);

            ShaderKey Key;
            Key.Data = SerializedMemory{Ptr, Ser.GetSize(Ptr)};

            auto Iter = ShaderMap.emplace(std::move(Key), ShaderMap.size()).first;
            ShaderIndices.push_back(StaticCast<Uint32>(Iter->second));
        }
    }

    // AZ TODO: map ray tracing shaders to shader indices

    return true;
}
#endif // VULKAN_SUPPORTED


void ArchiveBuilderImpl::SerializeShadersForPSO(const TShaderIndices& ShaderIndices, SerializedMemory& DeviceData) const
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

bool ArchiveBuilderImpl::AddRenderPass(IRenderPass* pRP)
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

Bool ArchiveBuilderImpl::ArchiveGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
                                                      const PipelineStateArchiveInfo&        ArchiveInfo)
{

    ValidatePipelineStateArchiveInfo(PSOCreateInfo, ArchiveInfo, m_PRSMap, m_pRenderDevice->GetValidDeviceBits());
    ValidatePSOCreateInfo(m_pRenderDevice, PSOCreateInfo);

    // AZ TODO: PSO may contain different shaders for different device types
    auto IterAndInserted = m_GraphicsPSOMap.emplace(String{PSOCreateInfo.PSODesc.Name}, GraphicsPSOData{});
    if (!IterAndInserted.second)
    {
        LOG_ERROR_MESSAGE("Graphics pipeline must have unique name");
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

        const char* RPName = "";
        if (PSOCreateInfo.GraphicsPipeline.pRenderPass != nullptr)
        {
            RPName = PSOCreateInfo.GraphicsPipeline.pRenderPass->GetDesc().Name;
            if (!AddRenderPass(PSOCreateInfo.GraphicsPipeline.pRenderPass))
                return false;
        }

        Serializer<SerializerMode::Measure> MeasureSer;
        SerializerImpl<SerializerMode::Measure>::SerializeGraphicsPSO(MeasureSer, PSOCreateInfo, PRSNames, RPName, nullptr);

        const size_t SerSize = MeasureSer.GetSize(nullptr);
        void*        SerPtr  = ALLOCATE_RAW(RawMemAllocator, "", SerSize);

        Serializer<SerializerMode::Write> Ser{SerPtr, SerSize};
        SerializerImpl<SerializerMode::Write>::SerializeGraphicsPSO(Ser, PSOCreateInfo, PRSNames, RPName, nullptr);
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
                // AZ TODO
                break;
#endif

#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
            {
                // AZ TODO
                break;
            }
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
                // AZ TODO
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
            {
                TShaderIndices ShaderIndices;
                if (!PatchShadersVk(PSOCreateInfo, ArchiveInfo, ShaderIndices))
                    return false;

                SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[Uint32{DeviceType::Vulkan}]);
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

Bool ArchiveBuilderImpl::ArchiveComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo,
                                                     const PipelineStateArchiveInfo&       ArchiveInfo)
{
    ValidatePipelineStateArchiveInfo(PSOCreateInfo, ArchiveInfo, m_PRSMap, m_pRenderDevice->GetValidDeviceBits());
    ValidatePSOCreateInfo(m_pRenderDevice, PSOCreateInfo);

    // AZ TODO
    return false;
}

Bool ArchiveBuilderImpl::ArchiveRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
                                                        const PipelineStateArchiveInfo&          ArchiveInfo)
{
    ValidatePipelineStateArchiveInfo(PSOCreateInfo, ArchiveInfo, m_PRSMap, m_pRenderDevice->GetValidDeviceBits());
    ValidatePSOCreateInfo(m_pRenderDevice, PSOCreateInfo);

    // AZ TODO
    return false;
}

Bool ArchiveBuilderImpl::ArchiveTilePipelineState(const TilePipelineStateCreateInfo& PSOCreateInfo,
                                                  const PipelineStateArchiveInfo&    ArchiveInfo)
{
    ValidatePipelineStateArchiveInfo(PSOCreateInfo, ArchiveInfo, m_PRSMap, m_pRenderDevice->GetValidDeviceBits());
    ValidatePSOCreateInfo(m_pRenderDevice, PSOCreateInfo);

    // AZ TODO
    return false;
}

} // namespace Diligent
