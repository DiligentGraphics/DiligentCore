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

#include "PipelineStateBase.hpp"

#include <unordered_set>
#include <unordered_map>
#include <array>

#include "HashUtils.hpp"
#include "StringTools.hpp"

namespace Diligent
{

#define LOG_PSO_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of ", GetPipelineTypeString(PSODesc.PipelineType), " PSO '", (PSODesc.Name != nullptr ? PSODesc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

namespace
{

void ValidateRasterizerStateDesc(const PipelineStateDesc& PSODesc, const GraphicsPipelineDesc& GraphicsPipeline) noexcept(false)
{
    const auto& RSDesc = GraphicsPipeline.RasterizerDesc;
    if (RSDesc.FillMode == FILL_MODE_UNDEFINED)
        LOG_PSO_ERROR_AND_THROW("RasterizerDesc.FillMode must not be FILL_MODE_UNDEFINED.");
    if (RSDesc.CullMode == CULL_MODE_UNDEFINED)
        LOG_PSO_ERROR_AND_THROW("RasterizerDesc.CullMode must not be CULL_MODE_UNDEFINED.");
}

void ValidateDepthStencilDesc(const PipelineStateDesc& PSODesc, const GraphicsPipelineDesc& GraphicsPipeline) noexcept(false)
{
    const auto& DSSDesc = GraphicsPipeline.DepthStencilDesc;
    if (DSSDesc.DepthEnable && DSSDesc.DepthFunc == COMPARISON_FUNC_UNKNOWN)
        LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.DepthFunc must not be COMPARISON_FUNC_UNKNOWN when depth is enabled.");

    auto CheckStencilOpDesc = [&](const StencilOpDesc& OpDesc, const char* FaceName) //
    {
        if (DSSDesc.StencilEnable)
        {
            if (OpDesc.StencilFailOp == STENCIL_OP_UNDEFINED)
                LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.", FaceName, ".StencilFailOp must not be STENCIL_OP_UNDEFINED when stencil is enabled.");
            if (OpDesc.StencilDepthFailOp == STENCIL_OP_UNDEFINED)
                LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.", FaceName, ".StencilDepthFailOp must not be STENCIL_OP_UNDEFINED when stencil is enabled.");
            if (OpDesc.StencilPassOp == STENCIL_OP_UNDEFINED)
                LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.", FaceName, ".StencilPassOp must not be STENCIL_OP_UNDEFINED when stencil is enabled.");
            if (OpDesc.StencilFunc == COMPARISON_FUNC_UNKNOWN)
                LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.", FaceName, ".StencilFunc must not be COMPARISON_FUNC_UNKNOWN when stencil is enabled.");
        }
    };
    CheckStencilOpDesc(DSSDesc.FrontFace, "FrontFace");
    CheckStencilOpDesc(DSSDesc.BackFace, "BackFace");
}

void CorrectDepthStencilDesc(GraphicsPipelineDesc& GraphicsPipeline) noexcept
{
    auto& DSSDesc = GraphicsPipeline.DepthStencilDesc;
    if (!DSSDesc.DepthEnable && DSSDesc.DepthFunc == COMPARISON_FUNC_UNKNOWN)
        DSSDesc.DepthFunc = DepthStencilStateDesc{}.DepthFunc;

    auto CorrectStencilOpDesc = [&](StencilOpDesc& OpDesc) //
    {
        if (!DSSDesc.StencilEnable)
        {
            if (OpDesc.StencilFailOp == STENCIL_OP_UNDEFINED)
                OpDesc.StencilFailOp = StencilOpDesc{}.StencilFailOp;
            if (OpDesc.StencilDepthFailOp == STENCIL_OP_UNDEFINED)
                OpDesc.StencilDepthFailOp = StencilOpDesc{}.StencilDepthFailOp;
            if (OpDesc.StencilPassOp == STENCIL_OP_UNDEFINED)
                OpDesc.StencilPassOp = StencilOpDesc{}.StencilPassOp;
            if (OpDesc.StencilFunc == COMPARISON_FUNC_UNKNOWN)
                OpDesc.StencilFunc = StencilOpDesc{}.StencilFunc;
        }
    };
    CorrectStencilOpDesc(DSSDesc.FrontFace);
    CorrectStencilOpDesc(DSSDesc.BackFace);
}

void ValidateBlendStateDesc(const PipelineStateDesc& PSODesc, const GraphicsPipelineDesc& GraphicsPipeline) noexcept(false)
{
    const auto& BlendDesc = GraphicsPipeline.BlendDesc;
    for (Uint32 rt = 0; rt < MAX_RENDER_TARGETS; ++rt)
    {
        auto& RTDesc = BlendDesc.RenderTargets[rt];

        const auto BlendEnable = RTDesc.BlendEnable && (rt == 0 || (BlendDesc.IndependentBlendEnable && rt > 0));
        if (BlendEnable)
        {
            if (RTDesc.SrcBlend == BLEND_FACTOR_UNDEFINED)
                LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].SrcBlend must not be BLEND_FACTOR_UNDEFINED.");
            if (RTDesc.DestBlend == BLEND_FACTOR_UNDEFINED)
                LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].DestBlend must not be BLEND_FACTOR_UNDEFINED.");
            if (RTDesc.BlendOp == BLEND_OPERATION_UNDEFINED)
                LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].BlendOp must not be BLEND_OPERATION_UNDEFINED.");

            if (RTDesc.SrcBlendAlpha == BLEND_FACTOR_UNDEFINED)
                LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].SrcBlendAlpha must not be BLEND_FACTOR_UNDEFINED.");
            if (RTDesc.DestBlendAlpha == BLEND_FACTOR_UNDEFINED)
                LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].DestBlendAlpha must not be BLEND_FACTOR_UNDEFINED.");
            if (RTDesc.BlendOpAlpha == BLEND_OPERATION_UNDEFINED)
                LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].BlendOpAlpha must not be BLEND_OPERATION_UNDEFINED.");
        }
    }
}

void CorrectBlendStateDesc(GraphicsPipelineDesc& GraphicsPipeline) noexcept
{
    auto& BlendDesc = GraphicsPipeline.BlendDesc;
    for (Uint32 rt = 0; rt < MAX_RENDER_TARGETS; ++rt)
    {
        auto& RTDesc = BlendDesc.RenderTargets[rt];
        // clang-format off
        const auto  BlendEnable   = RTDesc.BlendEnable          && (rt == 0 || (BlendDesc.IndependentBlendEnable && rt > 0));
        const auto  LogicOpEnable = RTDesc.LogicOperationEnable && (rt == 0 || (BlendDesc.IndependentBlendEnable && rt > 0));
        // clang-format on
        if (!BlendEnable)
        {
            if (RTDesc.SrcBlend == BLEND_FACTOR_UNDEFINED)
                RTDesc.SrcBlend = RenderTargetBlendDesc{}.SrcBlend;
            if (RTDesc.DestBlend == BLEND_FACTOR_UNDEFINED)
                RTDesc.DestBlend = RenderTargetBlendDesc{}.DestBlend;
            if (RTDesc.BlendOp == BLEND_OPERATION_UNDEFINED)
                RTDesc.BlendOp = RenderTargetBlendDesc{}.BlendOp;

            if (RTDesc.SrcBlendAlpha == BLEND_FACTOR_UNDEFINED)
                RTDesc.SrcBlendAlpha = RenderTargetBlendDesc{}.SrcBlendAlpha;
            if (RTDesc.DestBlendAlpha == BLEND_FACTOR_UNDEFINED)
                RTDesc.DestBlendAlpha = RenderTargetBlendDesc{}.DestBlendAlpha;
            if (RTDesc.BlendOpAlpha == BLEND_OPERATION_UNDEFINED)
                RTDesc.BlendOpAlpha = RenderTargetBlendDesc{}.BlendOpAlpha;
        }

        if (!LogicOpEnable)
            RTDesc.LogicOp = RenderTargetBlendDesc{}.LogicOp;
    }
}


void ValidatePipelineResourceSignatures(const PipelineStateCreateInfo& CreateInfo,
                                        const DeviceFeatures&          Features) noexcept(false)
{
    const auto& PSODesc = CreateInfo.PSODesc;

    if (CreateInfo.ResourceSignaturesCount != 0 && CreateInfo.ppResourceSignatures == nullptr)
        LOG_PSO_ERROR_AND_THROW("ppResourceSignatures is null, but ResourceSignaturesCount (", CreateInfo.ResourceSignaturesCount, ") is not zero.");

    if (CreateInfo.ppResourceSignatures != nullptr && CreateInfo.ResourceSignaturesCount == 0)
        LOG_PSO_ERROR_AND_THROW("ppResourceSignatures is not null, but ResourceSignaturesCount is zero.");

    if (CreateInfo.ppResourceSignatures == nullptr)
        return;

    if (CreateInfo.PSODesc.SRBAllocationGranularity != 1)
    {
        LOG_WARNING_MESSAGE("PSODesc.SRBAllocationGranularity is ignored when explicit resource signatures are used. Use default value (1) to silence this warning.");
    }

    if (CreateInfo.PSODesc.ResourceLayout.NumVariables != 0)
    {
        LOG_PSO_ERROR_AND_THROW("The number of variables defined through resource layout (", CreateInfo.PSODesc.ResourceLayout.NumVariables,
                                ") must be zero when resource signatures are used.");
    }

    if (CreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers != 0)
    {
        LOG_PSO_ERROR_AND_THROW("The number of immutable samplers defined through resource layout (", CreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers,
                                ") must be zero when resource signatures are used.");
    }


    std::unordered_multimap<HashMapStringKey, std::pair<SHADER_TYPE, const IPipelineResourceSignature*>, HashMapStringKey::Hasher> AllResources;
    std::unordered_multimap<HashMapStringKey, std::pair<SHADER_TYPE, const IPipelineResourceSignature*>, HashMapStringKey::Hasher> AllImtblSamplers;

    std::array<const IPipelineResourceSignature*, MAX_RESOURCE_SIGNATURES> ppSignatures = {};
    for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
    {
        const auto* const pSignature = CreateInfo.ppResourceSignatures[i];
        if (pSignature == nullptr)
            LOG_PSO_ERROR_AND_THROW("Pipeline resource signature at index ", i, " is null");

        const auto& SignDesc = pSignature->GetDesc();
        VERIFY(SignDesc.BindingIndex < MAX_RESOURCE_SIGNATURES,
               "Resource signature binding index exceeds the limit. This error should've been caught by ValidatePipelineResourceSignatureDesc.");

        if (ppSignatures[SignDesc.BindingIndex] != nullptr)
        {
            LOG_PSO_ERROR_AND_THROW("Pipeline resource signature '", pSignature->GetDesc().Name, "' at binding index ", Uint32{SignDesc.BindingIndex},
                                    " conflicts with another resource signature '", ppSignatures[SignDesc.BindingIndex]->GetDesc().Name,
                                    "' that uses the same index.");
        }
        ppSignatures[SignDesc.BindingIndex] = pSignature;

        for (Uint32 res = 0; res < SignDesc.NumResources; ++res)
        {
            const auto& ResDesc = SignDesc.Resources[res];
            VERIFY(ResDesc.Name != nullptr && ResDesc.Name[0] != '\0', "Resource name can't be null or empty. This should've been caught by ValidatePipelineResourceSignatureDesc()");
            VERIFY(ResDesc.ShaderStages != SHADER_TYPE_UNKNOWN, "Shader stages can't be UNKNOWN. This should've been caught by ValidatePipelineResourceSignatureDesc()");

            auto range = AllResources.equal_range(ResDesc.Name);
            for (auto it = range.first; it != range.second; ++it)
            {
                const auto& StageSig = it->second;
                if ((StageSig.first & ResDesc.ShaderStages) != 0)
                {
                    VERIFY(StageSig.second != pSignature, "Overlapping resources in one signature should've been caught by ValidatePipelineResourceSignatureDesc()");

                    LOG_PSO_ERROR_AND_THROW("Shader resource '", ResDesc.Name, "' is found in more than one resource signature ('", SignDesc.Name,
                                            "' and '", StageSig.second->GetDesc().Name,
                                            "') in the same shader stage. Every shader resource in the PSO must be unambiguously defined by only one resource signature.");
                }

                if (Features.SeparablePrograms == DEVICE_FEATURE_STATE_DISABLED)
                {
                    VERIFY_EXPR(StageSig.first != SHADER_TYPE_UNKNOWN);
                    VERIFY(StageSig.second != pSignature, "Resources with the same name in one signature should've been caught by ValidatePipelineResourceSignatureDesc()");

                    LOG_PSO_ERROR_AND_THROW("This device does not support separable programs, but shader resource '", ResDesc.Name, "' is found in more than one resource signature ('",
                                            SignDesc.Name, "' and '", StageSig.second->GetDesc().Name,
                                            "') in different stages. When separable programs are not supported, every resource is always shared between all stages. "
                                            "Use distinct resource names for each stage or define a single resource for all stages.");
                }
            }
            AllResources.emplace(ResDesc.Name, std::make_pair(ResDesc.ShaderStages, pSignature));
        }

        for (Uint32 res = 0; res < SignDesc.NumImmutableSamplers; ++res)
        {
            const auto& SamDesc = SignDesc.ImmutableSamplers[res];
            VERIFY(SamDesc.SamplerOrTextureName != nullptr && SamDesc.SamplerOrTextureName[0] != '\0', "Sampler name can't be null or empty. This should've been caught by ValidatePipelineResourceSignatureDesc()");
            VERIFY(SamDesc.ShaderStages != SHADER_TYPE_UNKNOWN, "Shader stage can't be UNKNOWN. This should've been caught by ValidatePipelineResourceSignatureDesc()");

            auto range = AllImtblSamplers.equal_range(SamDesc.SamplerOrTextureName);
            for (auto it = range.first; it != range.second; ++it)
            {
                const auto& StageSig = it->second;
                if ((StageSig.first & SamDesc.ShaderStages) != 0)
                {
                    VERIFY(StageSig.second != pSignature, "Overlapping immutable samplers in one signature should've been caught by ValidatePipelineResourceSignatureDesc()");

                    LOG_PSO_ERROR_AND_THROW("Immutable sampler '", SamDesc.SamplerOrTextureName, "' is found in more than one resource signature ('", SignDesc.Name,
                                            "' and '", StageSig.second->GetDesc().Name,
                                            "') in the same stage. Every immutable sampler in the PSO must be unambiguously defined by only one resource signature.");
                }

                if (Features.SeparablePrograms == DEVICE_FEATURE_STATE_DISABLED)
                {
                    VERIFY_EXPR(StageSig.first != SHADER_TYPE_UNKNOWN);
                    VERIFY(StageSig.second != pSignature, "Immutable samplers with the same name in one signature should've been caught by ValidatePipelineResourceSignatureDesc()");

                    LOG_PSO_ERROR_AND_THROW("This device does not support separable programs, but immutable sampler '", SamDesc.SamplerOrTextureName, "' is found in more than one resource signature ('",
                                            SignDesc.Name, "' and '", StageSig.second->GetDesc().Name,
                                            "') in different stages. When separable programs are not supported, every resource is always shared between all stages. "
                                            "Use distinct resource names for each stage or define a single immutable sampler for all stages.");
                }
            }
            AllImtblSamplers.emplace(SamDesc.SamplerOrTextureName, std::make_pair(SamDesc.ShaderStages, pSignature));
        }
    }
}

void ValidatePipelineResourceLayoutDesc(const PipelineStateDesc& PSODesc, const DeviceFeatures& Features) noexcept(false)
{
    const auto& Layout = PSODesc.ResourceLayout;
    {
        std::unordered_multimap<HashMapStringKey, SHADER_TYPE, HashMapStringKey::Hasher> UniqueVariables;
        for (Uint32 i = 0; i < Layout.NumVariables; ++i)
        {
            const auto& Var = Layout.Variables[i];

            if (Var.Name == nullptr)
                LOG_PSO_ERROR_AND_THROW("ResourceLayout.Variables[", i, "].Name must not be null.");

            if (Var.Name[0] == '\0')
                LOG_PSO_ERROR_AND_THROW("ResourceLayout.Variables[", i, "].Name must not be empty.");

            if (Var.ShaderStages == SHADER_TYPE_UNKNOWN)
                LOG_PSO_ERROR_AND_THROW("ResourceLayout.Variables[", i, "].ShaderStages must not be SHADER_TYPE_UNKNOWN.");

            auto range = UniqueVariables.equal_range(Var.Name);
            for (auto it = range.first; it != range.second; ++it)
            {
                if ((it->second & Var.ShaderStages) != 0)
                {
                    LOG_PSO_ERROR_AND_THROW("Shader variable '", Var.Name, "' is defined in overlapping shader stages (", GetShaderStagesString(Var.ShaderStages),
                                            " and ", GetShaderStagesString(it->second),
                                            "). Multiple variables with the same name are allowed, but shader stages they use must not overlap.");
                }
                if (Features.SeparablePrograms == DEVICE_FEATURE_STATE_DISABLED)
                {
                    VERIFY_EXPR(it->second != SHADER_TYPE_UNKNOWN);
                    LOG_PSO_ERROR_AND_THROW("This device does not support separable programs, but there are separate resources with the name '",
                                            Var.Name, "' in shader stages ",
                                            GetShaderStagesString(Var.ShaderStages), " and ",
                                            GetShaderStagesString(it->second),
                                            ". When separable programs are not supported, every resource is always shared between all stages. "
                                            "Use distinct resource names for each stage or define a single resource for all stages.");
                }
            }
            UniqueVariables.emplace(Var.Name, Var.ShaderStages);
        }
    }
    {
        std::unordered_multimap<HashMapStringKey, SHADER_TYPE, HashMapStringKey::Hasher> UniqueSamplers;
        for (Uint32 i = 0; i < Layout.NumImmutableSamplers; ++i)
        {
            const auto& Sam = Layout.ImmutableSamplers[i];

            if (Sam.SamplerOrTextureName == nullptr)
                LOG_PSO_ERROR_AND_THROW("ResourceLayout.ImmutableSamplers[", i, "].SamplerOrTextureName must not be null.");

            if (Sam.SamplerOrTextureName[0] == '\0')
                LOG_PSO_ERROR_AND_THROW("ResourceLayout.ImmutableSamplers[", i, "].SamplerOrTextureName must not be empty.");

            if (Sam.ShaderStages == SHADER_TYPE_UNKNOWN)
                LOG_PSO_ERROR_AND_THROW("ResourceLayout.ImmutableSamplers[", i, "].ShaderStages must not be SHADER_TYPE_UNKNOWN.");

            auto range = UniqueSamplers.equal_range(Sam.SamplerOrTextureName);
            for (auto it = range.first; it != range.second; ++it)
            {
                if ((it->second & Sam.ShaderStages) != 0)
                {
                    LOG_PSO_ERROR_AND_THROW("Immutable sampler '", Sam.SamplerOrTextureName, "' is defined in overlapping shader stages (", GetShaderStagesString(Sam.ShaderStages),
                                            " and ", GetShaderStagesString(it->second),
                                            "). Multiple immutable samplers with the same name are allowed, but shader stages they use must not overlap.");
                }
                if (Features.SeparablePrograms == DEVICE_FEATURE_STATE_DISABLED)
                {
                    VERIFY_EXPR(it->second != SHADER_TYPE_UNKNOWN);
                    LOG_PSO_ERROR_AND_THROW("This device does not support separable programs, but there are separate immutable samplers with the name '",
                                            Sam.SamplerOrTextureName, "' in shader stages ",
                                            GetShaderStagesString(Sam.ShaderStages), " and ",
                                            GetShaderStagesString(it->second),
                                            ". When separable programs are not supported, every resource is always shared between all stages. "
                                            "Use distinct immutable sampler names for each stage or define a single sampler for all stages.");
                }
            }
            UniqueSamplers.emplace(Sam.SamplerOrTextureName, Sam.ShaderStages);
        }
    }
}


#define VALIDATE_SHADER_TYPE(Shader, ExpectedType, ShaderName)                                                                           \
    if (Shader != nullptr && Shader->GetDesc().ShaderType != ExpectedType)                                                               \
    {                                                                                                                                    \
        LOG_ERROR_AND_THROW(GetShaderTypeLiteralName(Shader->GetDesc().ShaderType), " is not a valid type for ", ShaderName, " shader"); \
    }

void ValidateGraphicsPipelineCreateInfo(const GraphicsPipelineStateCreateInfo& CreateInfo,
                                        const DeviceFeatures&                  Features) noexcept(false)
{
    const auto& PSODesc = CreateInfo.PSODesc;
    if (PSODesc.PipelineType != PIPELINE_TYPE_GRAPHICS && PSODesc.PipelineType != PIPELINE_TYPE_MESH)
        LOG_PSO_ERROR_AND_THROW("Pipeline type must be GRAPHICS or MESH.");

    ValidatePipelineResourceSignatures(CreateInfo, Features);

    const auto& GraphicsPipeline = CreateInfo.GraphicsPipeline;

    ValidateBlendStateDesc(PSODesc, GraphicsPipeline);
    ValidateRasterizerStateDesc(PSODesc, GraphicsPipeline);
    ValidateDepthStencilDesc(PSODesc, GraphicsPipeline);
    ValidatePipelineResourceLayoutDesc(PSODesc, Features);


    if (PSODesc.PipelineType == PIPELINE_TYPE_GRAPHICS)
    {
        if (CreateInfo.pVS == nullptr)
            LOG_PSO_ERROR_AND_THROW("Vertex shader must not be null.");

        DEV_CHECK_ERR(CreateInfo.pAS == nullptr && CreateInfo.pMS == nullptr, "Mesh shaders are not supported in graphics pipeline.");
    }
    else if (PSODesc.PipelineType == PIPELINE_TYPE_MESH)
    {
        if (CreateInfo.pMS == nullptr)
            LOG_PSO_ERROR_AND_THROW("Mesh shader must not be null.");

        DEV_CHECK_ERR(CreateInfo.pVS == nullptr && CreateInfo.pGS == nullptr && CreateInfo.pDS == nullptr && CreateInfo.pHS == nullptr,
                      "Vertex, geometry and tessellation shaders are not supported in a mesh pipeline.");
        DEV_CHECK_ERR(GraphicsPipeline.InputLayout.NumElements == 0, "Input layout is ignored in a mesh pipeline.");
        DEV_CHECK_ERR(GraphicsPipeline.PrimitiveTopology == PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
                          GraphicsPipeline.PrimitiveTopology == PRIMITIVE_TOPOLOGY_UNDEFINED,
                      "Primitive topology is ignored in a mesh pipeline, set it to undefined or keep default value (triangle list).");
    }


    VALIDATE_SHADER_TYPE(CreateInfo.pVS, SHADER_TYPE_VERTEX, "vertex")
    VALIDATE_SHADER_TYPE(CreateInfo.pPS, SHADER_TYPE_PIXEL, "pixel")
    VALIDATE_SHADER_TYPE(CreateInfo.pGS, SHADER_TYPE_GEOMETRY, "geometry")
    VALIDATE_SHADER_TYPE(CreateInfo.pHS, SHADER_TYPE_HULL, "hull")
    VALIDATE_SHADER_TYPE(CreateInfo.pDS, SHADER_TYPE_DOMAIN, "domain")
    VALIDATE_SHADER_TYPE(CreateInfo.pAS, SHADER_TYPE_AMPLIFICATION, "amplification")
    VALIDATE_SHADER_TYPE(CreateInfo.pMS, SHADER_TYPE_MESH, "mesh")


    if (GraphicsPipeline.pRenderPass != nullptr)
    {
        if (GraphicsPipeline.NumRenderTargets != 0)
            LOG_PSO_ERROR_AND_THROW("NumRenderTargets must be 0 when explicit render pass is used.");
        if (GraphicsPipeline.DSVFormat != TEX_FORMAT_UNKNOWN)
            LOG_PSO_ERROR_AND_THROW("DSVFormat must be TEX_FORMAT_UNKNOWN when explicit render pass is used.");

        for (Uint32 rt = 0; rt < MAX_RENDER_TARGETS; ++rt)
        {
            if (GraphicsPipeline.RTVFormats[rt] != TEX_FORMAT_UNKNOWN)
                LOG_PSO_ERROR_AND_THROW("RTVFormats[", rt, "] must be TEX_FORMAT_UNKNOWN when explicit render pass is used.");
        }

        const auto& RPDesc = GraphicsPipeline.pRenderPass->GetDesc();
        if (GraphicsPipeline.SubpassIndex >= RPDesc.SubpassCount)
            LOG_PSO_ERROR_AND_THROW("Subpass index (", Uint32{GraphicsPipeline.SubpassIndex}, ") exceeds the number of subpasses (", Uint32{RPDesc.SubpassCount}, ") in render pass '", RPDesc.Name, "'.");
    }
    else
    {
        for (Uint32 rt = GraphicsPipeline.NumRenderTargets; rt < _countof(GraphicsPipeline.RTVFormats); ++rt)
        {
            auto RTVFmt = GraphicsPipeline.RTVFormats[rt];
            if (RTVFmt != TEX_FORMAT_UNKNOWN)
            {
                LOG_ERROR_MESSAGE("Render target format (", GetTextureFormatAttribs(RTVFmt).Name, ") of unused slot ", rt,
                                  " must be set to TEX_FORMAT_UNKNOWN.");
            }
        }

        if (GraphicsPipeline.SubpassIndex != 0)
            LOG_PSO_ERROR_AND_THROW("Subpass index (", Uint32{GraphicsPipeline.SubpassIndex}, ") must be 0 when explicit render pass is not used.");
    }
}

void ValidateComputePipelineCreateInfo(const ComputePipelineStateCreateInfo& CreateInfo,
                                       const DeviceFeatures&                 Features) noexcept(false)
{
    const auto& PSODesc = CreateInfo.PSODesc;
    if (PSODesc.PipelineType != PIPELINE_TYPE_COMPUTE)
        LOG_PSO_ERROR_AND_THROW("Pipeline type must be COMPUTE.");

    ValidatePipelineResourceSignatures(CreateInfo, Features);
    ValidatePipelineResourceLayoutDesc(PSODesc, Features);

    if (CreateInfo.pCS == nullptr)
        LOG_PSO_ERROR_AND_THROW("Compute shader must not be null.");

    VALIDATE_SHADER_TYPE(CreateInfo.pCS, SHADER_TYPE_COMPUTE, "compute")
}

void ValidateRayTracingPipelineCreateInfo(IRenderDevice*                           pDevice,
                                          const RayTracingPipelineStateCreateInfo& CreateInfo) noexcept(false)
{
    const auto& DeviceInfo = pDevice->GetDeviceInfo();
    const auto& RTProps    = pDevice->GetAdapterInfo().RayTracing;
    const auto& PSODesc    = CreateInfo.PSODesc;
    if (PSODesc.PipelineType != PIPELINE_TYPE_RAY_TRACING)
        LOG_PSO_ERROR_AND_THROW("Pipeline type must be RAY_TRACING.");

    if (!DeviceInfo.Features.RayTracing || (RTProps.CapFlags & RAY_TRACING_CAP_FLAG_STANDALONE_SHADERS) == 0)
        LOG_PSO_ERROR_AND_THROW("Standalone ray tracing shaders are not supported");

    ValidatePipelineResourceSignatures(CreateInfo, DeviceInfo.Features);
    ValidatePipelineResourceLayoutDesc(PSODesc, DeviceInfo.Features);

    if (DeviceInfo.Type == RENDER_DEVICE_TYPE_D3D12)
    {
        if ((CreateInfo.pShaderRecordName != nullptr) != (CreateInfo.RayTracingPipeline.ShaderRecordSize > 0))
            LOG_PSO_ERROR_AND_THROW("pShaderRecordName must not be null if RayTracingPipeline.ShaderRecordSize is not zero.");
    }

    if (CreateInfo.RayTracingPipeline.MaxRecursionDepth > RTProps.MaxRecursionDepth)
    {
        LOG_PSO_ERROR_AND_THROW("MaxRecursionDepth (", Uint32{CreateInfo.RayTracingPipeline.MaxRecursionDepth},
                                ") exceeds device limit (", RTProps.MaxRecursionDepth, ").");
    }

    std::unordered_set<HashMapStringKey, HashMapStringKey::Hasher> GroupNames;

    auto VerifyShaderGroupName = [&](const char* MemberName, // "pGeneralShaders", "pTriangleHitShaders", or "pProceduralHitShaders"
                                     Uint32      GroupInd,
                                     const char* GroupName) {
        if (GroupName == nullptr)
            LOG_PSO_ERROR_AND_THROW(MemberName, "[", GroupInd, "].Name must not be null.");

        if (GroupName[0] == '\0')
            LOG_PSO_ERROR_AND_THROW(MemberName, "[", GroupInd, "].Name must not be empty.");

        const bool IsNewName = GroupNames.emplace(HashMapStringKey{GroupName}).second;
        if (!IsNewName)
            LOG_PSO_ERROR_AND_THROW(MemberName, "[", GroupInd, "].Name ('", GroupName, "') has already been assigned to another group. All group names must be unique.");
    };

    for (Uint32 i = 0; i < CreateInfo.GeneralShaderCount; ++i)
    {
        const auto& Group = CreateInfo.pGeneralShaders[i];

        VerifyShaderGroupName("pGeneralShaders", i, Group.Name);

        if (Group.pShader == nullptr)
            LOG_PSO_ERROR_AND_THROW("pGeneralShaders[", i, "].pShader must not be null.");

        switch (Group.pShader->GetDesc().ShaderType)
        {
            case SHADER_TYPE_RAY_GEN:
            case SHADER_TYPE_RAY_MISS:
            case SHADER_TYPE_CALLABLE: break;
            default:
                LOG_PSO_ERROR_AND_THROW(GetShaderTypeLiteralName(Group.pShader->GetDesc().ShaderType), " is not a valid type for ray tracing general shader.");
        }
    }

    for (Uint32 i = 0; i < CreateInfo.TriangleHitShaderCount; ++i)
    {
        const auto& Group = CreateInfo.pTriangleHitShaders[i];

        VerifyShaderGroupName("pTriangleHitShaders", i, Group.Name);

        if (Group.pClosestHitShader == nullptr)
            LOG_PSO_ERROR_AND_THROW("pTriangleHitShaders[", i, "].pClosestHitShader must not be null.");

        VALIDATE_SHADER_TYPE(Group.pClosestHitShader, SHADER_TYPE_RAY_CLOSEST_HIT, "ray tracing triangle closest hit")
        VALIDATE_SHADER_TYPE(Group.pAnyHitShader, SHADER_TYPE_RAY_ANY_HIT, "ray tracing triangle any hit")
    }

    for (Uint32 i = 0; i < CreateInfo.ProceduralHitShaderCount; ++i)
    {
        const auto& Group = CreateInfo.pProceduralHitShaders[i];

        VerifyShaderGroupName("pProceduralHitShaders", i, Group.Name);

        if (Group.pIntersectionShader == nullptr)
            LOG_PSO_ERROR_AND_THROW("pProceduralHitShaders[", i, "].pIntersectionShader must not be null.");

        VALIDATE_SHADER_TYPE(Group.pIntersectionShader, SHADER_TYPE_RAY_INTERSECTION, "ray tracing procedural intersection")
        VALIDATE_SHADER_TYPE(Group.pClosestHitShader, SHADER_TYPE_RAY_CLOSEST_HIT, "ray tracing procedural closest hit")
        VALIDATE_SHADER_TYPE(Group.pAnyHitShader, SHADER_TYPE_RAY_ANY_HIT, "ray tracing procedural any hit")
    }
}

void ValidateTilePipelineCreateInfo(const TilePipelineStateCreateInfo& CreateInfo,
                                    const DeviceFeatures&              Features) noexcept(false)
{
    const auto& PSODesc = CreateInfo.PSODesc;
    if (PSODesc.PipelineType != PIPELINE_TYPE_TILE)
        LOG_PSO_ERROR_AND_THROW("Pipeline type must be TILE.");

    ValidatePipelineResourceSignatures(CreateInfo, Features);
    ValidatePipelineResourceLayoutDesc(PSODesc, Features);

    if (CreateInfo.pTS == nullptr)
        LOG_PSO_ERROR_AND_THROW("Tile shader must not be null.");

    VALIDATE_SHADER_TYPE(CreateInfo.pTS, SHADER_TYPE_TILE, "tile")
}

} // namespace

void CopyRTShaderGroupNames(std::unordered_map<HashMapStringKey, Uint32, HashMapStringKey::Hasher>& NameToGroupIndex,
                            const RayTracingPipelineStateCreateInfo&                                CreateInfo,
                            FixedLinearAllocator&                                                   MemPool) noexcept
{
    Uint32 GroupIndex = 0;

    for (Uint32 i = 0; i < CreateInfo.GeneralShaderCount; ++i)
    {
        const auto* Name      = CreateInfo.pGeneralShaders[i].Name;
        const bool  IsNewName = NameToGroupIndex.emplace(HashMapStringKey{MemPool.CopyString(Name)}, GroupIndex++).second;
        VERIFY(IsNewName, "All group names must be unique. ValidateRayTracingPipelineCreateInfo() should've caught this error.");
    }
    for (Uint32 i = 0; i < CreateInfo.TriangleHitShaderCount; ++i)
    {
        const auto* Name      = CreateInfo.pTriangleHitShaders[i].Name;
        const bool  IsNewName = NameToGroupIndex.emplace(HashMapStringKey{MemPool.CopyString(Name)}, GroupIndex++).second;
        VERIFY(IsNewName, "All group names must be unique. ValidateRayTracingPipelineCreateInfo() should've caught this error.");
    }
    for (Uint32 i = 0; i < CreateInfo.ProceduralHitShaderCount; ++i)
    {
        const auto* Name      = CreateInfo.pProceduralHitShaders[i].Name;
        const bool  IsNewName = NameToGroupIndex.emplace(HashMapStringKey{MemPool.CopyString(Name)}, GroupIndex++).second;
        VERIFY(IsNewName, "All group names must be unique. ValidateRayTracingPipelineCreateInfo() should've caught this error.");
    }

    VERIFY_EXPR(CreateInfo.GeneralShaderCount + CreateInfo.TriangleHitShaderCount + CreateInfo.ProceduralHitShaderCount == GroupIndex);
}

#undef VALIDATE_SHADER_TYPE
#undef LOG_PSO_ERROR_AND_THROW

void ValidatePipelineResourceCompatibility(const PipelineResourceDesc& ResDesc,
                                           SHADER_RESOURCE_TYPE        Type,
                                           PIPELINE_RESOURCE_FLAGS     ResourceFlags,
                                           Uint32                      ArraySize,
                                           const char*                 ShaderName,
                                           const char*                 SignatureName) noexcept(false)
{
    if (Type != ResDesc.ResourceType)
    {
        LOG_ERROR_AND_THROW("Shader '", ShaderName, "' contains resource with name '", ResDesc.Name,
                            "' and type '", GetShaderResourceTypeLiteralName(Type), "' that is not compatible with type '",
                            GetShaderResourceTypeLiteralName(ResDesc.ResourceType), "' specified in pipeline resource signature '", SignatureName, "'.");
    }

    if ((ResourceFlags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER))
    {
        LOG_ERROR_AND_THROW("Shader '", ShaderName, "' contains resource '", ResDesc.Name,
                            "' that is", ((ResourceFlags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) ? "" : " not"),
                            " labeled as formatted buffer, while the same resource specified by the pipeline resource signature '",
                            SignatureName, "' is", ((ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) ? "" : " not"),
                            " labeled as such.");
    }

    VERIFY(ResDesc.ArraySize > 0, "ResDesc.ArraySize can't be zero. This error should've be caught by ValidatePipelineResourceSignatureDesc().");

    if (ArraySize == 0)
    {
        // ArraySize == 0 means that the resource is a runtime-sized array and ResDesc.ArraySize from the
        // resource signature may have any non-zero value.
        if ((ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY) == 0)
        {
            LOG_ERROR_AND_THROW("Shader '", ShaderName, "' contains resource '", ResDesc.Name,
                                "' that is a runtime-sized array, but in the resource signature '", SignatureName,
                                "' the resource is defined without the PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY flag.");
        }
    }
    else
    {
        if (ResDesc.ArraySize < ArraySize)
        {
            LOG_ERROR_AND_THROW("Shader '", ShaderName, "' contains resource '", ResDesc.Name,
                                "' whose array size (", ArraySize, ") is greater than the array size (",
                                ResDesc.ArraySize, ") specified by the pipeline resource signature '", SignatureName, "'.");
        }

        //if (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY)
        //{
        //    LOG_WARNING_MESSAGE("Shader '", ShaderName, "' contains resource with name '", ResDesc.Name,
        //                        "' that is defined in resource signature '", SignatureName,
        //                        "' with flag PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY, but the resource is not a runtime-sized array.");
        //}
    }
}

void CorrectGraphicsPipelineDesc(GraphicsPipelineDesc& GraphicsPipeline) noexcept
{
    CorrectBlendStateDesc(GraphicsPipeline);
    CorrectDepthStencilDesc(GraphicsPipeline);
}

ShaderResourceVariableDesc FindPipelineResourceLayoutVariable(
    const PipelineResourceLayoutDesc& LayoutDesc,
    const char*                       Name,
    SHADER_TYPE                       ShaderStage,
    const char*                       CombinedSamplerSuffix)
{
    for (Uint32 i = 0; i < LayoutDesc.NumVariables; ++i)
    {
        const auto& Var = LayoutDesc.Variables[i];
        if ((Var.ShaderStages & ShaderStage) != 0 && StreqSuff(Name, Var.Name, CombinedSamplerSuffix))
        {
#ifdef DILIGENT_DEBUG
            for (Uint32 j = i + 1; j < LayoutDesc.NumVariables; ++j)
            {
                const auto& Var2 = LayoutDesc.Variables[j];
                VERIFY(!((Var2.ShaderStages & ShaderStage) != 0 && StreqSuff(Name, Var2.Name, CombinedSamplerSuffix)),
                       "There must be no variables with overlapping stages in Desc.ResourceLayout. "
                       "This error should've been caught by ValidatePipelineResourceLayoutDesc().");
            }
#endif
            return Var;
        }
    }

    // Use default properties
    if (ShaderStage & LayoutDesc.DefaultVariableMergeStages)
        ShaderStage = LayoutDesc.DefaultVariableMergeStages;
    return {ShaderStage, Name, LayoutDesc.DefaultVariableType};
}


template <>
void ValidatePSOCreateInfo<GraphicsPipelineStateCreateInfo>(IRenderDevice*                         pDevice,
                                                            const GraphicsPipelineStateCreateInfo& CreateInfo) noexcept(false)
{
    VERIFY_EXPR(pDevice != nullptr);
    ValidateGraphicsPipelineCreateInfo(CreateInfo, pDevice->GetDeviceInfo().Features);
}

template <>
void ValidatePSOCreateInfo<ComputePipelineStateCreateInfo>(IRenderDevice*                        pDevice,
                                                           const ComputePipelineStateCreateInfo& CreateInfo) noexcept(false)
{
    VERIFY_EXPR(pDevice != nullptr);
    ValidateComputePipelineCreateInfo(CreateInfo, pDevice->GetDeviceInfo().Features);
}


template <>
void ValidatePSOCreateInfo<RayTracingPipelineStateCreateInfo>(IRenderDevice*                           pDevice,
                                                              const RayTracingPipelineStateCreateInfo& CreateInfo) noexcept(false)
{
    VERIFY_EXPR(pDevice != nullptr);
    ValidateRayTracingPipelineCreateInfo(pDevice, CreateInfo);
}

template <>
void ValidatePSOCreateInfo<TilePipelineStateCreateInfo>(IRenderDevice*                     pDevice,
                                                        const TilePipelineStateCreateInfo& CreateInfo) noexcept(false)
{
    VERIFY_EXPR(pDevice != nullptr);
    ValidateTilePipelineCreateInfo(CreateInfo, pDevice->GetDeviceInfo().Features);
}

} // namespace Diligent
