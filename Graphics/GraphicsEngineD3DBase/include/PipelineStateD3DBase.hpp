/*
 *  Copyright 2026 Diligent Graphics LLC
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

#pragma once

/// \file
/// Declaration of Diligent::PipelineStateD3DBase class

#include "PipelineStateBase.hpp"
#include "ShaderResources.hpp"
#include "D3DShaderResourceValidation.hpp"

#include <unordered_map>
#include <algorithm>
#include <type_traits>

namespace Diligent
{

struct D3DShaderResourceAttribs;

/// Pipeline state object implementation base class for Direct3D backends.
template <typename EngineImplTraits>
class PipelineStateD3DBase : public PipelineStateBase<EngineImplTraits>
{
public:
    using TBase = PipelineStateBase<EngineImplTraits>;

    using RenderDeviceImplType   = typename EngineImplTraits::RenderDeviceImplType;
    using LocalRootSignatureType = typename EngineImplTraits::LocalRootSignatureType;

    template <typename PSOCreateInfoType>
    PipelineStateD3DBase(IReferenceCounters* pRefCounters, RenderDeviceImplType* pDevice, const PSOCreateInfoType& CreateInfo) :
        TBase{pRefCounters, pDevice, CreateInfo}
    {
    }

protected:
    struct DefaultSignatureDescBuilder
    {
        const char* const                   PSOName;
        const PipelineResourceLayoutDesc&   ResourceLayout;
        const LocalRootSignatureType* const pLocalRootSig;

        PipelineResourceSignatureDescWrapper& SignDesc;

        struct UniqueResourceInfo
        {
            const D3DShaderResourceAttribs& Attribs;
            const Uint32                    ResIdx; // Index in SignDesc
        };
        std::unordered_map<ShaderResourceHashKey, UniqueResourceInfo, ShaderResourceHashKey::Hasher> UniqueResources;

        void ProcessShaderResources(const ShaderResources& Resources) noexcept(false)
        {
            const SHADER_TYPE ShaderType = Resources.GetShaderType();
            const char*       ShaderName = Resources.GetShaderName();
            Resources.ProcessResources(
                [&](const D3DShaderResourceAttribs& Attribs, Uint32) //
                {
                    if constexpr (!std::is_void_v<LocalRootSignatureType>)
                    {
                        if (pLocalRootSig != nullptr && pLocalRootSig->IsShaderRecord(Attribs))
                            return;
                    }

                    if (Attribs.BindCount == 0)
                    {
                        LOG_ERROR_AND_THROW("Resource '", Attribs.Name, "' in shader '", ShaderName, "' is a runtime-sized array. ",
                                            "Use explicit resource signature to specify the array size.");
                    }

                    const char* const SamplerSuffix =
                        (Resources.IsUsingCombinedTextureSamplers() && Attribs.GetInputType() == D3D_SIT_SAMPLER) ?
                        Resources.GetCombinedSamplerSuffix() :
                        nullptr;

                    const ShaderResourceVariableDesc VarDesc  = FindPipelineResourceLayoutVariable(ResourceLayout, Attribs.Name, ShaderType, SamplerSuffix);
                    const SHADER_RESOURCE_TYPE       ResType  = Attribs.GetShaderResourceType();
                    const PIPELINE_RESOURCE_FLAGS    ResFlags = Attribs.GetPipelineResourceFlags() | ShaderVariableFlagsToPipelineResourceFlags(VarDesc.Flags);

                    // For inline constant buffers, the array size is the number of constants
                    const Uint32 ArraySize = (ResFlags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS) ?
                        Attribs.GetInlineConstantCountOrThrow(ShaderName) :
                        Attribs.BindCount;
                    VERIFY((ResFlags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS) == 0 || (ResFlags == PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS),
                           "INLINE_CONSTANTS flag cannot be combined with other flags.");

                    // Note that Attribs.Name != VarDesc.Name for combined samplers
                    auto it_assigned = UniqueResources.emplace(ShaderResourceHashKey{VarDesc.ShaderStages, Attribs.Name},
                                                               UniqueResourceInfo{Attribs, SignDesc.GetNumResources()});
                    if (it_assigned.second)
                    {
                        SignDesc.AddResource(VarDesc.ShaderStages, Attribs.Name, ArraySize, ResType, VarDesc.Type, ResFlags);
                    }
                    else
                    {
                        if (ResFlags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS)
                        {
                            PipelineResourceDesc& InlineCB = SignDesc.GetResource(it_assigned.first->second.ResIdx);
                            VERIFY_EXPR(InlineCB.Flags & PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS);
                            VERIFY_EXPR(InlineCB.ShaderStages & ShaderType);
                            // Use the maximum number of constants across all shaders
                            InlineCB.ArraySize = (std::max)(InlineCB.ArraySize, ArraySize);
                        }
                        VerifyD3DResourceMerge(PSOName, it_assigned.first->second.Attribs, Attribs);
                    }
                } //
            );

            // Merge combined sampler suffixes
            if (Resources.IsUsingCombinedTextureSamplers() && Resources.GetNumSamplers() > 0)
            {
                SignDesc.SetCombinedSamplerSuffix(Resources.GetCombinedSamplerSuffix());
            }
        }
    };
};

} // namespace Diligent
