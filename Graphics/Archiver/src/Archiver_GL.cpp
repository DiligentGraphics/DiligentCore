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
#include "Archiver_Inc.hpp"

#include "../../GraphicsEngineOpenGL/include/pch.h"
#include "RenderDeviceGLImpl.hpp"
#include "PipelineResourceSignatureGLImpl.hpp"
#include "PipelineStateGLImpl.hpp"
#include "ShaderGLImpl.hpp"
#include "DeviceObjectArchiveGLImpl.hpp"

namespace Diligent
{

namespace
{

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
};

inline SHADER_TYPE GetShaderStageType(const ShaderStageInfoGL& Stage) { return Stage.Type; }

} // namespace

template <typename CreateInfoType>
bool ArchiverImpl::PatchShadersGL(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS)
{
    TShaderIndices ShaderIndices;

    std::vector<ShaderStageInfoGL> ShaderStages;
    SHADER_TYPE                    ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateGLImpl::ExtractShaders<SerializableShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    (void)GetShaderStageType(ShaderStageInfoGL{});

    // AZ TODO: default PRS

    for (size_t i = 0; i < ShaderStages.size(); ++i)
    {
        SerializeShaderSource(ShaderIndices, DeviceType::OpenGL, ShaderStages[i].pShader->GetCreateInfo());
    }
    SerializeShadersForPSO(ShaderIndices, Data.PerDeviceData[static_cast<Uint32>(DeviceType::OpenGL)]);
    return true;
}

template bool ArchiverImpl::PatchShadersGL<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersGL<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersGL<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
template bool ArchiverImpl::PatchShadersGL<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);


PipelineResourceSignatureGLImpl* SerializableResourceSignatureImpl::GetSignatureGL() const
{
    return m_pPRSGL ? m_pPRSGL->GetPRS<PipelineResourceSignatureGLImpl>() : nullptr;
}

const SerializedMemory* SerializableResourceSignatureImpl::GetSerializedMemoryGL() const
{
    return m_pPRSGL ? m_pPRSGL->GetMem() : nullptr;
}

void SerializableResourceSignatureImpl::CreatePRSGL(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages)
{
    auto pPRSGL = std::make_unique<TPRS<PipelineResourceSignatureGLImpl>>(pRefCounters, Desc, ShaderStages);

    PipelineResourceSignatureSerializedDataGL SerializedData;
    pPRSGL->PRS.Serialize(SerializedData);
    AddPRSDesc(pPRSGL->PRS.GetDesc(), SerializedData);
    CopyPRSSerializedData<PSOSerializerGL>(SerializedData, pPRSGL->Mem);

    m_pPRSGL.reset(pPRSGL.release());
}


void SerializationDeviceImpl::GetPipelineResourceBindingsGL(const PipelineResourceBindingAttribs& Info,
                                                            std::vector<PipelineResourceBinding>& ResourceBindings)
{
    const auto            ShaderStages        = (Info.ShaderStages == SHADER_TYPE_UNKNOWN ? static_cast<SHADER_TYPE>(~0u) : Info.ShaderStages);
    constexpr SHADER_TYPE SupportedStagesMask = (SHADER_TYPE_ALL_GRAPHICS | SHADER_TYPE_COMPUTE);

    SignatureArray<PipelineResourceSignatureGLImpl> Signatures      = {};
    Uint32                                          SignaturesCount = 0;
    SortResourceSignatures(Info, Signatures, SignaturesCount);

    PipelineResourceSignatureGLImpl::TBindings BaseBindings = {};
    for (Uint32 s = 0; s < SignaturesCount; ++s)
    {
        const auto& pSignature = Signatures[s];
        if (pSignature == nullptr)
            continue;

        for (Uint32 r = 0; r < pSignature->GetTotalResourceCount(); ++r)
        {
            const auto& ResDesc = pSignature->GetResourceDesc(r);
            const auto& ResAttr = pSignature->GetResourceAttribs(r);
            const auto  Range   = PipelineResourceToBindingRange(ResDesc);

            for (auto Stages = ShaderStages & SupportedStagesMask; Stages != 0;)
            {
                const auto ShaderStage = ExtractLSB(Stages);

                if ((ResDesc.ShaderStages & ShaderStage) == 0)
                    continue;

                PipelineResourceBinding Dst{};
                Dst.Name         = ResDesc.Name;
                Dst.ResourceType = ResDesc.ResourceType;
                Dst.Register     = BaseBindings[Range] + ResAttr.CacheOffset;
                Dst.Space        = 0;
                Dst.ArraySize    = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY) == 0 ? ResDesc.ArraySize : RuntimeArray;
                Dst.ShaderStages = ShaderStage;
                ResourceBindings.push_back(Dst);
            }
        }
        pSignature->ShiftBindings(BaseBindings);
    }
}

} // namespace Diligent
