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

#include "SerializationDeviceImpl.hpp"
#include "SerializableResourceSignatureImpl.hpp"

#include "PipelineStateMtlImpl.hpp"

namespace Diligent
{

void SerializationDeviceImpl::GetMetalPipelineResourceBindings(const PipelineResourceBindingAttribs& Info,
                                                               std::vector<PipelineResourceBinding>& ResourceBindings,
                                                               const Uint32                          MaxBufferArgs)
{
    ResourceBindings.clear();

    std::array<RefCntAutoPtr<PipelineResourceSignatureMtlImpl>, MAX_RESOURCE_SIGNATURES> Signatures = {};

    Uint32 SignaturesCount = 0;
    for (Uint32 i = 0; i < Info.ResourceSignaturesCount; ++i)
    {
        const auto* pSerPRS = ClassPtrCast<SerializableResourceSignatureImpl>(Info.ppResourceSignatures[i]);
        const auto& Desc    = pSerPRS->GetDesc();

        Signatures[Desc.BindingIndex] = pSerPRS->GetSignature<PipelineResourceSignatureMtlImpl>();
        SignaturesCount               = std::max(SignaturesCount, static_cast<Uint32>(Desc.BindingIndex) + 1);
    }

    const auto            ShaderStages        = (Info.ShaderStages == SHADER_TYPE_UNKNOWN ? static_cast<SHADER_TYPE>(~0u) : Info.ShaderStages);
    constexpr SHADER_TYPE SupportedStagesMask = (SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL | SHADER_TYPE_COMPUTE | SHADER_TYPE_TILE);
    constexpr Uint32      RuntimeArray        = 0;
    MtlResourceCounters   BaseBindings{};

    for (Uint32 sign = 0; sign < SignaturesCount; ++sign)
    {
        const auto& pSignature = Signatures[sign];
        if (pSignature == nullptr)
            continue;
        
        for (Uint32 r = 0; r < pSignature->GetTotalResourceCount(); ++r)
        {
            const auto& ResDesc = pSignature->GetResourceDesc(r);
            const auto& ResAttr = pSignature->GetResourceAttribs(r);
            const auto  Range   = PipelineResourceDescToMtlResourceRange(ResDesc);

            for (auto Stages = ShaderStages & SupportedStagesMask; Stages != 0;)
            {
                const auto ShaderStage = ExtractLSB(Stages);
                const auto ShaderInd   = MtlResourceBindIndices::ShaderTypeToIndex(ShaderStage);
                DEV_CHECK_ERR(ShaderInd < MtlResourceBindIndices::NumShaderTypes,
                              "Unsupported shader stage (", GetShaderTypeLiteralName(ShaderStage), ") for Metal backend");

                if ((ResDesc.ShaderStages & ShaderStage) == 0)
                    continue;

                PipelineResourceBinding Dst{};
                Dst.Name         = ResDesc.Name;
                Dst.ResourceType = ResDesc.ResourceType;
                Dst.Register     = BaseBindings[ShaderInd][Range] + ResAttr.BindIndices[ShaderInd];
                Dst.Space        = 0;
                Dst.ArraySize    = (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY) == 0 ? ResDesc.ArraySize : RuntimeArray;
                Dst.ShaderStages = ShaderStage;
                ResourceBindings.push_back(Dst);
            }
        }
        pSignature->ShiftBindings(BaseBindings);
    }

    // Add vertex buffer bindings.
    // Same as DeviceContextMtlImpl::CommitVertexBuffers()
    if ((Info.ShaderStages & SHADER_TYPE_VERTEX) != 0 && Info.NumVertexBuffers > 0)
    {
        DEV_CHECK_ERR(Info.VertexBufferNames != nullptr, "VertexBufferNames must not be null");

        const auto BaseSlot = MaxBufferArgs - Info.NumVertexBuffers;
        for (Uint32 i = 0; i < Info.NumVertexBuffers; ++i)
        {
            PipelineResourceBinding Dst{};
            Dst.Name         = Info.VertexBufferNames[i];
            Dst.ResourceType = SHADER_RESOURCE_TYPE_BUFFER_SRV;
            Dst.Register     = BaseSlot + i;
            Dst.Space        = 0;
            Dst.ArraySize    = 1;
            Dst.ShaderStages = SHADER_TYPE_VERTEX;
            ResourceBindings.push_back(Dst);
        }
    }
}

} // namespace Diligent
