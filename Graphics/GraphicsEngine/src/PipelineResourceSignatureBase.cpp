/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include "PipelineResourceSignatureBase.hpp"

#include <unordered_map>

#include "HashUtils.hpp"
#include "StringTools.hpp"

namespace Diligent
{

#define LOG_PRS_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of a pipeline resource signature '", (Desc.Name ? Desc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

void ValidatePipelineResourceSignatureDesc(const PipelineResourceSignatureDesc& Desc) noexcept(false)
{
    if (Desc.BindingIndex >= MAX_RESOURCE_SIGNATURES)
        LOG_PRS_ERROR_AND_THROW("Desc.BindingIndex (", Uint32{Desc.BindingIndex}, ") exceeds the maximum allowed value (", MAX_RESOURCE_SIGNATURES - 1, ").");

    if (Desc.NumResources > MAX_RESOURCES_IN_SIGNATURE)
        LOG_PRS_ERROR_AND_THROW("Desc.NumResources (", Uint32{Desc.NumResources}, ") exceeds the maximum allowed value (", MAX_RESOURCES_IN_SIGNATURE, ").");

    if (Desc.NumResources != 0 && Desc.Resources == nullptr)
        LOG_PRS_ERROR_AND_THROW("Desc.NumResources (", Uint32{Desc.NumResources}, ") is not zero, but Desc.Resources is null.");

    if (Desc.NumImmutableSamplers != 0 && Desc.ImmutableSamplers == nullptr)
        LOG_PRS_ERROR_AND_THROW("Desc.NumImmutableSamplers (", Uint32{Desc.NumImmutableSamplers}, ") is not zero, but Desc.ImmutableSamplers is null.");

    if (Desc.UseCombinedTextureSamplers && (Desc.CombinedSamplerSuffix == nullptr || Desc.CombinedSamplerSuffix[0] == '\0'))
        LOG_PRS_ERROR_AND_THROW("Desc.UseCombinedTextureSamplers is true, but Desc.CombinedSamplerSuffix is null or empty");

    std::unordered_map<HashMapStringKey, SHADER_TYPE, HashMapStringKey::Hasher> ResourceShaderStages;

    for (Uint32 i = 0; i < Desc.NumResources; ++i)
    {
        const auto& Res = Desc.Resources[i];

        if (Res.Name == nullptr)
            LOG_PRS_ERROR_AND_THROW("Desc.Resources[", i, "].Name must not be null");

        if (Res.ShaderStages == SHADER_TYPE_UNKNOWN)
            LOG_PRS_ERROR_AND_THROW("Desc.Resources[", i, "].ShaderStages must not be SHADER_TYPE_UNKNOWN");

        if (Res.ArraySize == 0)
            LOG_PRS_ERROR_AND_THROW("Desc.Resources[", i, "].ArraySize must not be 0");

        auto& UsedStages = ResourceShaderStages[Res.Name];
        if ((UsedStages & Res.ShaderStages) != 0)
        {
            LOG_PRS_ERROR_AND_THROW("Multiple resources with name '", Res.Name,
                                    "' specify overlapping shader stages. There may be multiple resources with the same name in different shader stages, "
                                    "but the stages must not overlap.");
        }
        UsedStages |= Res.ShaderStages;

        static_assert(SHADER_RESOURCE_TYPE_LAST == 8, "Please add the new resource type to the switch below");
        switch (Res.ResourceType)
        {
            case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:
                if ((Res.Flags & ~PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS) != 0)
                {
                    LOG_PRS_ERROR_AND_THROW("Incorrect Desc.Resources[", i, "].Flags (", GetPipelineResourceFlagsString(Res.Flags),
                                            "): NO_DYNAMIC_BUFFERS is the only valid flag for a constant buffer.");
                }
                break;

            case SHADER_RESOURCE_TYPE_TEXTURE_SRV:
                if ((Res.Flags & ~PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER) != 0)
                {
                    LOG_PRS_ERROR_AND_THROW("Incorrect Desc.Resources[", i, "].Flags (", GetPipelineResourceFlagsString(Res.Flags),
                                            "): COMBINED_SAMPLER is the only valid flag for a texture SRV.");
                }
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_SRV:
                if ((Res.Flags & ~(PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS | PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER)) != 0)
                {
                    LOG_PRS_ERROR_AND_THROW("Incorrect Desc.Resources[", i, "].Flags (", GetPipelineResourceFlagsString(Res.Flags),
                                            "): NO_DYNAMIC_BUFFERS and FORMATTED_BUFFER are the only valid flags for a buffer SRV.");
                }
                break;

            case SHADER_RESOURCE_TYPE_TEXTURE_UAV:
                if (Res.Flags != PIPELINE_RESOURCE_FLAG_UNKNOWN)
                {
                    LOG_PRS_ERROR_AND_THROW("Incorrect Desc.Resources[", i, "].Flags (", GetPipelineResourceFlagsString(Res.Flags),
                                            "): UNKNOWN is the only valid flag for a texture UAV.");
                }
                break;

            case SHADER_RESOURCE_TYPE_BUFFER_UAV:
                if ((Res.Flags & ~(PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS | PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER)) != 0)
                {
                    LOG_PRS_ERROR_AND_THROW("Incorrect Desc.Resources[", i, "].Flags (", GetPipelineResourceFlagsString(Res.Flags),
                                            "): NO_DYNAMIC_BUFFERS and FORMATTED_BUFFER are the only valid flags for a buffer UAV.");
                }
                break;

            case SHADER_RESOURCE_TYPE_SAMPLER:
                if (Res.Flags != PIPELINE_RESOURCE_FLAG_UNKNOWN)
                {
                    LOG_PRS_ERROR_AND_THROW("Incorrect Desc.Resources[", i, "].Flags (", GetPipelineResourceFlagsString(Res.Flags),
                                            "): UNKNOWN is the only valid flag for a sampler.");
                }
                break;

            case SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT:
                if (Res.Flags != PIPELINE_RESOURCE_FLAG_UNKNOWN)
                {
                    LOG_PRS_ERROR_AND_THROW("Incorrect Desc.Resources[", i, "].Flags (", GetPipelineResourceFlagsString(Res.Flags),
                                            "): UNKNOWN is the only valid flag for an input attachment.");
                }
                break;

            case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:
                if (Res.Flags != PIPELINE_RESOURCE_FLAG_UNKNOWN)
                {
                    LOG_PRS_ERROR_AND_THROW("Incorrect Desc.Resources[", i, "].Flags (", GetPipelineResourceFlagsString(Res.Flags),
                                            "): UNKNOWN is the only valid flag for an acceleration structure.");
                }
                break;

            default:
                UNEXPECTED("Unexpected resource type");
        }

        // NB: when creating immutable sampler array, we have to define the sampler as both resource and
        //     immutable sampler. The sampler will not be exposed as a shader variable though.
        //if (Res.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER)
        //{
        //    if (FindImmutableSampler(Desc.ImmutableSamplers, Desc.NumImmutableSamplers, Res.ShaderStages, Res.Name,
        //                             Desc.UseCombinedTextureSamplers ? Desc.CombinedSamplerSuffix : nullptr) >= 0)
        //    {
        //        LOG_PRS_ERROR_AND_THROW("Sampler '", Res.Name, "' is defined as both shader resource and immutable sampler.");
        //    }
        //}
    }

    if (Desc.UseCombinedTextureSamplers)
    {
        VERIFY_EXPR(Desc.CombinedSamplerSuffix != nullptr);
        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            const auto& Res = Desc.Resources[i];
            if (Res.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV)
            {
                auto AssignedSamplerName = String{Res.Name} + Desc.CombinedSamplerSuffix;
                for (Uint32 s = 0; s < Desc.NumResources; ++s)
                {
                    const auto& Sam = Desc.Resources[s];
                    if (Sam.ResourceType != SHADER_RESOURCE_TYPE_SAMPLER)
                        continue;

                    if (AssignedSamplerName == Sam.Name && (Sam.ShaderStages & Res.ShaderStages) != 0)
                    {
                        if (Sam.ShaderStages != Res.ShaderStages)
                        {
                            LOG_PRS_ERROR_AND_THROW("Texture '", Res.Name, "' and sampler '", Sam.Name, "' assigned to it use different shader stages.");
                        }

                        if (Sam.VarType != Res.VarType)
                        {
                            LOG_PRS_ERROR_AND_THROW("The type (", GetShaderVariableTypeLiteralName(Res.VarType), ") of texture resource '", Res.Name,
                                                    "' does not match the type (", GetShaderVariableTypeLiteralName(Sam.VarType),
                                                    ") of sampler '", Sam.Name, "' that is assigned to it.");
                        }
                    }
                }
            }
        }
    }

    std::unordered_map<HashMapStringKey, SHADER_TYPE, HashMapStringKey::Hasher> ImtblSamShaderStages;
    for (Uint32 i = 0; i < Desc.NumImmutableSamplers; ++i)
    {
        const auto& SamDesc = Desc.ImmutableSamplers[i];
        if (SamDesc.SamplerOrTextureName == nullptr)
            LOG_PRS_ERROR_AND_THROW("Desc.ImmutableSamplers[", i, "].SamplerOrTextureName must not be null");

        auto& UsedStages = ImtblSamShaderStages[SamDesc.SamplerOrTextureName];
        if ((UsedStages & SamDesc.ShaderStages) != 0)
        {
            LOG_PRS_ERROR_AND_THROW("Multiple immutable samplers with name '", SamDesc.SamplerOrTextureName,
                                    "' specify overlapping shader stages. There may be multiple immutable samplers with the same name in different shader stages, "
                                    "but the stages must not overlap.");
        }
        UsedStages |= SamDesc.ShaderStages;
    }
}

#undef LOG_PRS_ERROR_AND_THROW


} // namespace Diligent
