/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "PipelineStateWebGPUImpl.hpp"
#include "RenderDeviceWebGPUImpl.hpp"
#include "ShaderWebGPUImpl.hpp"
#include "RenderPassWebGPUImpl.hpp"
#include "WebGPUTypeConversions.hpp"
#include "WGSLUtils.hpp"

namespace Diligent
{

constexpr INTERFACE_ID PipelineStateWebGPUImpl::IID_InternalImpl;

PipelineStateWebGPUImpl::PipelineStateWebGPUImpl(IReferenceCounters*                    pRefCounters,
                                                 RenderDeviceWebGPUImpl*                pDevice,
                                                 const GraphicsPipelineStateCreateInfo& CreateInfo) :
    TPipelineStateBase{pRefCounters, pDevice, CreateInfo}
{
    Construct<ShaderWebGPUImpl>(CreateInfo);
}

PipelineStateWebGPUImpl::PipelineStateWebGPUImpl(IReferenceCounters*                   pRefCounters,
                                                 RenderDeviceWebGPUImpl*               pDevice,
                                                 const ComputePipelineStateCreateInfo& CreateInfo) :
    TPipelineStateBase{pRefCounters, pDevice, CreateInfo}
{
    Construct<ShaderWebGPUImpl>(CreateInfo);
}

PipelineStateWebGPUImpl::~PipelineStateWebGPUImpl()
{
    GetStatus(/*WaitForCompletion =*/true);

    Destruct();
};

WGPURenderPipeline PipelineStateWebGPUImpl::GetWebGPURenderPipeline() const
{
    return m_wgpuRenderPipeline.Get();
}

WGPUComputePipeline PipelineStateWebGPUImpl::GetWebGPUComputePipeline() const
{
    return m_wgpuComputePipeline.Get();
}

void PipelineStateWebGPUImpl::Destruct()
{
    TPipelineStateBase::Destruct();
}

template <typename PSOCreateInfoType>
std::vector<PipelineStateWebGPUImpl::WebGPUPipelineShaderStageInfo> PipelineStateWebGPUImpl::InitInternalObjects(const PSOCreateInfoType& CreateInfo)
{
    std::vector<WebGPUPipelineShaderStageInfo> ShaderStages;
    ExtractShaders<ShaderWebGPUImpl>(CreateInfo, ShaderStages, /*WaitUntilShadersReady = */ true);
    VERIFY(!ShaderStages.empty(),
           "There must be at least one shader stage in the pipeline. "
           "This error should've been caught by PSO create info validation.");

    // Memory must be released if an exception is thrown.
    FixedLinearAllocator MemPool{GetRawAllocator()};

    ReserveSpaceForPipelineDesc(CreateInfo, MemPool);
    MemPool.Reserve();

    InitializePipelineDesc(CreateInfo, MemPool);
    InitPipelineLayout(CreateInfo, ShaderStages);

    return ShaderStages;
}


void PipelineStateWebGPUImpl::RemapOrVerifyShaderResources(
    TShaderStages&                                           ShaderStages,
    const RefCntAutoPtr<PipelineResourceSignatureWebGPUImpl> pSignatures[],
    const Uint32                                             SignatureCount,
    const TBindIndexToBindGroupIndex&                        BindIndexToBindGroupIndex,
    bool                                                     bVerifyOnly,
    const char*                                              PipelineName,
    TShaderResources*                                        pDvpShaderResources,
    TResourceAttibutions*                                    pDvpResourceAttibutions) noexcept(false)
{
    if (PipelineName == nullptr)
        PipelineName = "<null>";

    // Verify that pipeline layout is compatible with shader resources and
    // remap resource bindings.
    for (auto& ShaderStage : ShaderStages)
    {
        const ShaderWebGPUImpl* pShader     = ShaderStage.pShader;
        std::string&            PatchedWGSL = ShaderStage.WGSL;
        const SHADER_TYPE       ShaderType  = ShaderStage.Type;

        const auto& pShaderResources = pShader->GetShaderResources();
        VERIFY_EXPR(pShaderResources);

        if (pDvpShaderResources)
            pDvpShaderResources->emplace_back(pShaderResources);

        WGSLResourceMapping ResMapping;

        pShaderResources->ProcessResources(
            [&](const WGSLShaderResourceAttribs& WGSLAttribs, Uint32) //
            {
                const ResourceAttribution ResAttribution = GetResourceAttribution(WGSLAttribs.Name, ShaderType, pSignatures, SignatureCount);
                if (!ResAttribution)
                {
                    LOG_ERROR_AND_THROW("Shader '", pShader->GetDesc().Name, "' contains resource '", WGSLAttribs.Name,
                                        "' that is not present in any pipeline resource signature used to create pipeline state '",
                                        PipelineName, "'.");
                }

                const PipelineResourceSignatureDesc& SignDesc = ResAttribution.pSignature->GetDesc();
                const SHADER_RESOURCE_TYPE           ResType  = WGSLShaderResourceAttribs::GetShaderResourceType(WGSLAttribs.Type);
                const PIPELINE_RESOURCE_FLAGS        Flags    = WGSLShaderResourceAttribs::GetPipelineResourceFlags(WGSLAttribs.Type);

                Uint32 ResourceBinding = ~0u;
                Uint32 BindGroup       = ~0u;
                if (ResAttribution.ResourceIndex != ResourceAttribution::InvalidResourceIndex)
                {
                    const auto& ResDesc = ResAttribution.pSignature->GetResourceDesc(ResAttribution.ResourceIndex);
                    ValidatePipelineResourceCompatibility(ResDesc, ResType, Flags, WGSLAttribs.ArraySize,
                                                          pShader->GetDesc().Name, SignDesc.Name);

                    const auto& ResAttribs{ResAttribution.pSignature->GetResourceAttribs(ResAttribution.ResourceIndex)};
                    ResourceBinding = ResAttribs.BindingIndex;
                    BindGroup       = ResAttribs.BindGroup;
                }
                else if (ResAttribution.ImmutableSamplerIndex != ResourceAttribution::InvalidResourceIndex)
                {
                    if (ResType != SHADER_RESOURCE_TYPE_SAMPLER)
                    {
                        LOG_ERROR_AND_THROW("Shader '", pShader->GetDesc().Name, "' contains resource with name '", WGSLAttribs.Name,
                                            "' and type '", GetShaderResourceTypeLiteralName(ResType),
                                            "' that is not compatible with immutable sampler defined in pipeline resource signature '",
                                            SignDesc.Name, "'.");
                    }
                    const ImmutableSamplerAttribsWebGPU& ImmtblSamAttribs{
                        ResAttribution.pSignature->GetImmutableSamplerAttribs(ResAttribution.ImmutableSamplerIndex)};
                    // Handle immutable samplers that do not have corresponding resources in m_Desc.Resources
                    BindGroup       = ImmtblSamAttribs.BindGroup;
                    ResourceBinding = ImmtblSamAttribs.BindingIndex;
                }
                else
                {
                    UNEXPECTED("Either immutable sampler or resource index should be valid");
                }

                VERIFY_EXPR(ResourceBinding != ~0u && BindGroup != ~0u);
                BindGroup += BindIndexToBindGroupIndex[SignDesc.BindingIndex];
                if (bVerifyOnly)
                {
                    if (WGSLAttribs.BindIndex != ResourceBinding)
                    {
                        LOG_ERROR_AND_THROW("Shader '", pShader->GetDesc().Name, "' maps resource '", WGSLAttribs.Name,
                                            "' to binding ", WGSLAttribs.BindIndex, ", but the same resource in pipeline resource signature '",
                                            SignDesc.Name, "' is mapped to binding ", ResourceBinding, '.');
                    }
                    if (WGSLAttribs.BindGroup != BindGroup)
                    {
                        LOG_ERROR_AND_THROW("Shader '", pShader->GetDesc().Name, "' maps resource '", WGSLAttribs.Name,
                                            "' to bind group ", WGSLAttribs.BindGroup, ", but the same resource in pipeline resource signature '",
                                            SignDesc.Name, "' is mapped to set ", BindGroup, '.');
                    }
                }
                else
                {
                    ResMapping[WGSLAttribs.Name] = {BindGroup, ResourceBinding};
                }

                if (pDvpResourceAttibutions)
                    pDvpResourceAttibutions->emplace_back(ResAttribution);
            });

        if (!bVerifyOnly)
        {
            PatchedWGSL = RamapWGSLResourceBindings(pShader->GetWGSL(), ResMapping);
        }
        else
        {
            PatchedWGSL = pShader->GetWGSL();
        }
    }
}

void PipelineStateWebGPUImpl::InitPipelineLayout(const PipelineStateCreateInfo& CreateInfo, TShaderStages& ShaderStages)
{
    const auto InternalFlags = GetInternalCreateFlags(CreateInfo);
    if (m_UsingImplicitSignature && (InternalFlags & PSO_CREATE_INTERNAL_FLAG_IMPLICIT_SIGNATURE0) == 0)
    {
        const auto SignDesc = GetDefaultResourceSignatureDesc(ShaderStages, m_Desc.Name, m_Desc.ResourceLayout, m_Desc.SRBAllocationGranularity);
        InitDefaultSignature(SignDesc, GetActiveShaderStages(), false /*bIsDeviceInternal*/);
        VERIFY_EXPR(m_Signatures[0]);
    }

    m_PipelineLayout.Create(GetDevice(), m_Signatures, m_SignatureCount);

    const auto RemapResources = (CreateInfo.Flags & PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES) == 0;
    const auto VerifyBindings = !RemapResources && ((InternalFlags & PSO_CREATE_INTERNAL_FLAG_NO_SHADER_REFLECTION) == 0);
    if (RemapResources || VerifyBindings)
    {
        VERIFY_EXPR(RemapResources ^ VerifyBindings);
        TBindIndexToBindGroupIndex BindIndexToBindGroupIndex = {};
        for (Uint32 i = 0; i < m_SignatureCount; ++i)
            BindIndexToBindGroupIndex[i] = m_PipelineLayout.GetFirstBindGroupIndex(i);

        // Note that we always need to strip reflection information when it is present
        RemapOrVerifyShaderResources(ShaderStages,
                                     m_Signatures,
                                     m_SignatureCount,
                                     BindIndexToBindGroupIndex,
                                     VerifyBindings, // VerifyOnly
                                     m_Desc.Name,
#ifdef DILIGENT_DEVELOPMENT
                                     &m_ShaderResources, &m_ResourceAttibutions
#else
                                     nullptr, nullptr
#endif
        );
    }
}

void PipelineStateWebGPUImpl::InitializePipeline(const GraphicsPipelineStateCreateInfo& CreateInfo)
{
    const auto ShaderStages = InitInternalObjects(CreateInfo);

    VERIFY(ShaderStages.size() == 2, "Incorrect shader count for graphics pipeline");
    VERIFY(ShaderStages[0].Type == SHADER_TYPE_VERTEX, "Incorrect shader type: vertex shader is expected");
    VERIFY(ShaderStages[1].Type == SHADER_TYPE_PIXEL, "Incorrect shader type: compute shader is expected");

    const auto& GraphicsPipeline = GetGraphicsPipelineDesc();

    WGPUVertexState       wgpuVertexState{};
    WGPUPrimitiveState    wgpuPrimitiveState{};
    WGPUFragmentState     wgpuFragmentState{};
    WGPUDepthStencilState wgpuDepthStencilState{};
    WGPUMultisampleState  wgpuMultisampleState{};

    using WebGPUVertexAttributeArray = std::array<std::vector<WGPUVertexAttribute>, MAX_LAYOUT_ELEMENTS>;
    using WebGPUVertexBufferLayouts  = std::array<WGPUVertexBufferLayout, MAX_LAYOUT_ELEMENTS>;

    WebGPUVertexAttributeArray wgpuVertexAttributes{};
    WebGPUVertexBufferLayouts  wgpuVertexBufferLayouts{};

    std::vector<WGPUColorTargetState>      wgpuColorTargetStates{};
    std::vector<WGPUBlendState>            wgpuBlendStates{};
    std::vector<WebGPUShaderModuleWrapper> wgpuShaderModules{ShaderStages.size()};

    for (size_t ShaderIdx = 0; ShaderIdx < ShaderStages.size(); ++ShaderIdx)
    {
        WGPUShaderModuleDescriptor     wgpuShaderModuleDesc{};
        WGPUShaderModuleWGSLDescriptor wgpuShaderCodeDesc{};
        wgpuShaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
        wgpuShaderCodeDesc.code        = ShaderStages[ShaderIdx].WGSL.c_str();

        wgpuShaderModuleDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuShaderCodeDesc);
        wgpuShaderModuleDesc.label       = ShaderStages[ShaderIdx].pShader->GetEntryPoint();
        wgpuShaderModules[ShaderIdx].Reset(wgpuDeviceCreateShaderModule(m_pDevice->GetWebGPUDevice(), &wgpuShaderModuleDesc));
    }

    {
        const auto& InputLayout = GraphicsPipeline.InputLayout;

        Uint32 MaxBufferSlot = 0;
        for (Uint32 Idx = 0; Idx < InputLayout.NumElements; ++Idx)
        {
            const auto& Item = InputLayout.LayoutElements[Idx];

            auto BindingDescIndex = Item.BufferSlot;

            wgpuVertexBufferLayouts[BindingDescIndex].arrayStride = Item.Stride;
            wgpuVertexBufferLayouts[BindingDescIndex].stepMode    = InputElementFrequencyToWGPUVertexStepMode(Item.Frequency);

            WGPUVertexAttribute wgpuVertexAttribute{};
            wgpuVertexAttribute.format         = VertexFormatAttribsToWGPUVertexFormat(Item.ValueType, Item.NumComponents, Item.IsNormalized);
            wgpuVertexAttribute.offset         = Item.RelativeOffset;
            wgpuVertexAttribute.shaderLocation = Item.InputIndex;
            wgpuVertexAttributes[BindingDescIndex].push_back(wgpuVertexAttribute);

            MaxBufferSlot = std::max(MaxBufferSlot, BindingDescIndex);
        }

        for (size_t Idx = 0; Idx < MaxBufferSlot + 1; ++Idx)
        {
            wgpuVertexBufferLayouts[Idx].stepMode       = !wgpuVertexAttributes[Idx].empty() ? wgpuVertexBufferLayouts[Idx].stepMode : WGPUVertexStepMode_VertexBufferNotUsed;
            wgpuVertexBufferLayouts[Idx].attributeCount = static_cast<uint32_t>(wgpuVertexAttributes[Idx].size());
            wgpuVertexBufferLayouts[Idx].attributes     = wgpuVertexAttributes[Idx].data();
        }

        wgpuVertexState.module      = wgpuShaderModules[0].Get();
        wgpuVertexState.entryPoint  = ShaderStages[0].pShader->GetEntryPoint();
        wgpuVertexState.bufferCount = InputLayout.NumElements > 0 ? MaxBufferSlot + 1 : 0;
        wgpuVertexState.buffers     = wgpuVertexBufferLayouts.data();
    }

    wgpuColorTargetStates.reserve(GraphicsPipeline.NumRenderTargets);
    wgpuBlendStates.reserve(GraphicsPipeline.NumRenderTargets);
    {
        const auto& BlendDesc = GraphicsPipeline.BlendDesc;

        for (Uint32 RTIndex = 0; RTIndex < GraphicsPipeline.NumRenderTargets; ++RTIndex)
        {
            const auto& RT            = BlendDesc.RenderTargets[RTIndex];
            const auto  RTBlendEnable = (BlendDesc.RenderTargets[0].BlendEnable && !BlendDesc.IndependentBlendEnable) || (RT.BlendEnable && BlendDesc.IndependentBlendEnable);

            WGPUColorTargetState wgpuColorTargetState{};
            wgpuColorTargetState.format    = TextureFormatToWGPUFormat(GraphicsPipeline.RTVFormats[RTIndex]);
            wgpuColorTargetState.writeMask = ColorMaskToWGPUColorWriteMask(RT.RenderTargetWriteMask);

            if (RTBlendEnable)
            {
                WGPUBlendState wgpuBlendState{};
                wgpuBlendState.color.operation = BlendOpToWGPUBlendOperation(RT.BlendOp);
                wgpuBlendState.color.srcFactor = BlendFactorToWGPUBlendFactor(RT.SrcBlend);
                wgpuBlendState.color.dstFactor = BlendFactorToWGPUBlendFactor(RT.DestBlend);

                wgpuBlendState.alpha.operation = BlendOpToWGPUBlendOperation(RT.BlendOpAlpha);
                wgpuBlendState.alpha.srcFactor = BlendFactorToWGPUBlendFactor(RT.SrcBlendAlpha);
                wgpuBlendState.alpha.dstFactor = BlendFactorToWGPUBlendFactor(RT.DestBlendAlpha);

                wgpuBlendStates.push_back(wgpuBlendState);
                wgpuColorTargetState.blend = &wgpuBlendStates.back();
            }

            wgpuColorTargetStates.push_back(wgpuColorTargetState);
        }

        wgpuFragmentState.targetCount = static_cast<uint32_t>(wgpuColorTargetStates.size());
        wgpuFragmentState.targets     = wgpuColorTargetStates.data();
        wgpuFragmentState.module      = wgpuShaderModules[1].Get();
        wgpuFragmentState.entryPoint  = ShaderStages[1].pShader->GetEntryPoint();
    }

    {
        const auto& DepthStencilDesc = GraphicsPipeline.DepthStencilDesc;

        wgpuDepthStencilState.format            = TextureFormatToWGPUFormat(GraphicsPipeline.DSVFormat);
        wgpuDepthStencilState.depthCompare      = ComparisonFuncToWGPUCompareFunction(DepthStencilDesc.DepthFunc);
        wgpuDepthStencilState.depthWriteEnabled = DepthStencilDesc.DepthWriteEnable;

        wgpuDepthStencilState.stencilBack.compare     = ComparisonFuncToWGPUCompareFunction(DepthStencilDesc.BackFace.StencilFunc);
        wgpuDepthStencilState.stencilBack.failOp      = StencilOpToWGPUStencilOperation(DepthStencilDesc.BackFace.StencilFailOp);
        wgpuDepthStencilState.stencilBack.depthFailOp = StencilOpToWGPUStencilOperation(DepthStencilDesc.BackFace.StencilDepthFailOp);
        wgpuDepthStencilState.stencilBack.passOp      = StencilOpToWGPUStencilOperation(DepthStencilDesc.BackFace.StencilPassOp);

        wgpuDepthStencilState.stencilFront.compare     = ComparisonFuncToWGPUCompareFunction(DepthStencilDesc.FrontFace.StencilFunc);
        wgpuDepthStencilState.stencilFront.failOp      = StencilOpToWGPUStencilOperation(DepthStencilDesc.FrontFace.StencilFailOp);
        wgpuDepthStencilState.stencilFront.depthFailOp = StencilOpToWGPUStencilOperation(DepthStencilDesc.FrontFace.StencilDepthFailOp);
        wgpuDepthStencilState.stencilFront.passOp      = StencilOpToWGPUStencilOperation(DepthStencilDesc.FrontFace.StencilPassOp);

        wgpuDepthStencilState.stencilReadMask  = DepthStencilDesc.StencilReadMask;
        wgpuDepthStencilState.stencilWriteMask = DepthStencilDesc.StencilWriteMask;
    }

    {
        const auto& RasterizerDesc = GraphicsPipeline.RasterizerDesc;

        wgpuPrimitiveState.frontFace = RasterizerDesc.FrontCounterClockwise ? WGPUFrontFace_CCW : WGPUFrontFace_CW;
        wgpuPrimitiveState.cullMode  = CullModeToWGPUCullMode(RasterizerDesc.CullMode);
        wgpuPrimitiveState.topology  = PrimitiveTopologyWGPUPrimitiveType(GraphicsPipeline.PrimitiveTopology);
        // For pipelines with strip topologies ("line-strip" or "triangle-strip"), the index buffer format and primitive restart value
        // ("uint16"/0xFFFF or "uint32"/0xFFFFFFFF). Not allowed on pipelines with non-strip topologies.
        wgpuPrimitiveState.stripIndexFormat =
            (GraphicsPipeline.PrimitiveTopology == PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
             GraphicsPipeline.PrimitiveTopology == PRIMITIVE_TOPOLOGY_LINE_STRIP) ?
            WGPUIndexFormat_Uint32 :
            WGPUIndexFormat_Undefined;
    }

    {
        wgpuMultisampleState.alphaToCoverageEnabled = GraphicsPipeline.BlendDesc.AlphaToCoverageEnable;
        wgpuMultisampleState.mask                   = GraphicsPipeline.SampleMask;
        wgpuMultisampleState.count                  = GraphicsPipeline.SmplDesc.Count;
    }

    WGPURenderPipelineDescriptor wgpuRenderPipelineDesc{};
    wgpuRenderPipelineDesc.label        = GetDesc().Name;
    wgpuRenderPipelineDesc.vertex       = wgpuVertexState;
    wgpuRenderPipelineDesc.fragment     = wgpuFragmentState.targetCount > 0 ? &wgpuFragmentState : nullptr;
    wgpuRenderPipelineDesc.depthStencil = GraphicsPipeline.DSVFormat != TEX_FORMAT_UNKNOWN ? &wgpuDepthStencilState : nullptr;
    wgpuRenderPipelineDesc.primitive    = wgpuPrimitiveState;
    wgpuRenderPipelineDesc.multisample  = wgpuMultisampleState;
    wgpuRenderPipelineDesc.layout       = m_PipelineLayout.GetWebGPUPipelineLayout();

    m_wgpuRenderPipeline.Reset(wgpuDeviceCreateRenderPipeline(m_pDevice->GetWebGPUDevice(), &wgpuRenderPipelineDesc));
    if (!m_wgpuRenderPipeline)
        LOG_ERROR_AND_THROW("Failed to create pipeline state");
}

void PipelineStateWebGPUImpl::InitializePipeline(const ComputePipelineStateCreateInfo& CreateInfo)
{
    const auto ShaderStages = InitInternalObjects(CreateInfo);
    VERIFY(ShaderStages[0].Type == SHADER_TYPE_COMPUTE, "Incorrect shader type: compute shader is expected");

    ShaderWebGPUImpl* pShaderWebGPU = ShaderStages[0].pShader;

    WebGPUShaderModuleWrapper wgpuShaderModule{};

    WGPUShaderModuleWGSLDescriptor wgpuShaderCodeDesc{};
    wgpuShaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgpuShaderCodeDesc.code        = ShaderStages[0].WGSL.c_str();

    WGPUShaderModuleDescriptor wgpuShaderModuleDesc{};
    wgpuShaderModuleDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuShaderCodeDesc);
    wgpuShaderModuleDesc.label       = pShaderWebGPU->GetEntryPoint();
    wgpuShaderModule.Reset(wgpuDeviceCreateShaderModule(m_pDevice->GetWebGPUDevice(), &wgpuShaderModuleDesc));

    WGPUComputePipelineDescriptor wgpuComputePipelineDesc{};
    wgpuComputePipelineDesc.label              = GetDesc().Name;
    wgpuComputePipelineDesc.compute.module     = wgpuShaderModule.Get();
    wgpuComputePipelineDesc.compute.entryPoint = pShaderWebGPU->GetEntryPoint();
    wgpuComputePipelineDesc.layout             = m_PipelineLayout.GetWebGPUPipelineLayout();

    m_wgpuComputePipeline.Reset(wgpuDeviceCreateComputePipeline(m_pDevice->GetWebGPUDevice(), &wgpuComputePipelineDesc));
    if (!m_wgpuComputePipeline)
        LOG_ERROR_AND_THROW("Failed to create pipeline state");
}


static void VerifyResourceMerge(const char*                      PSOName,
                                const WGSLShaderResourceAttribs& ExistingRes,
                                const WGSLShaderResourceAttribs& NewResAttribs)
{
#define LOG_RESOURCE_MERGE_ERROR_AND_THROW(PropertyName)                                                  \
    LOG_ERROR_AND_THROW("Shader variable '", NewResAttribs.Name,                                          \
                        "' is shared between multiple shaders in pipeline '", (PSOName ? PSOName : ""),   \
                        "', but its " PropertyName " varies. A variable shared between multiple shaders " \
                        "must be defined identically in all shaders. Either use separate variables for "  \
                        "different shader stages, change resource name or make sure that " PropertyName " is consistent.");

    if (ExistingRes.Type != NewResAttribs.Type)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("type");

    if (ExistingRes.ResourceDim != NewResAttribs.ResourceDim)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("resource dimension");

    if (ExistingRes.ArraySize != NewResAttribs.ArraySize)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("array size");

    if (ExistingRes.SampleType != NewResAttribs.SampleType)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("sample type");

    if (ExistingRes.Format != NewResAttribs.Format)
        LOG_RESOURCE_MERGE_ERROR_AND_THROW("texture format type");

#undef LOG_RESOURCE_MERGE_ERROR_AND_THROW
}

PipelineResourceSignatureDescWrapper PipelineStateWebGPUImpl::GetDefaultResourceSignatureDesc(const TShaderStages&              ShaderStages,
                                                                                              const char*                       PSOName,
                                                                                              const PipelineResourceLayoutDesc& ResourceLayout,
                                                                                              Uint32                            SRBAllocationGranularity)
{
    PipelineResourceSignatureDescWrapper SignDesc{PSOName, ResourceLayout, SRBAllocationGranularity};

    std::unordered_map<ShaderResourceHashKey, const WGSLShaderResourceAttribs&, ShaderResourceHashKey::Hasher> UniqueResources;
    for (auto& Stage : ShaderStages)
    {
        const ShaderWebGPUImpl*    pShader         = Stage.pShader;
        const WGSLShaderResources& ShaderResources = *pShader->GetShaderResources();

        ShaderResources.ProcessResources(
            [&](const WGSLShaderResourceAttribs& Attribs, Uint32) //
            {
                const char* const SamplerSuffix =
                    (ShaderResources.IsUsingCombinedSamplers() && Attribs.Type == WGSLShaderResourceAttribs::ResourceType::Sampler) ?
                    ShaderResources.GetCombinedSamplerSuffix() :
                    nullptr;

                const ShaderResourceVariableDesc VarDesc = FindPipelineResourceLayoutVariable(ResourceLayout, Attribs.Name, Stage.Type, SamplerSuffix);
                // Note that Attribs.Name != VarDesc.Name for combined samplers
                const auto it_assigned = UniqueResources.emplace(ShaderResourceHashKey{VarDesc.ShaderStages, Attribs.Name}, Attribs);
                if (it_assigned.second)
                {
                    if (Attribs.ArraySize == 0)
                    {
                        LOG_ERROR_AND_THROW("Resource '", Attribs.Name, "' in shader '", pShader->GetDesc().Name, "' is a runtime-sized array. ",
                                            "You must use explicit resource signature to specify the array size.");
                    }

                    const SHADER_RESOURCE_TYPE    ResType       = WGSLShaderResourceAttribs::GetShaderResourceType(Attribs.Type);
                    const PIPELINE_RESOURCE_FLAGS Flags         = WGSLShaderResourceAttribs::GetPipelineResourceFlags(Attribs.Type) | ShaderVariableFlagsToPipelineResourceFlags(VarDesc.Flags);
                    const WebGPUResourceAttribs   WebGPUAttribs = Attribs.GetWebGPUAttribs();
                    SignDesc.AddResource(VarDesc.ShaderStages, Attribs.Name, Attribs.ArraySize, ResType, VarDesc.Type, Flags, WebGPUAttribs);
                }
                else
                {
                    VerifyResourceMerge(PSOName, it_assigned.first->second, Attribs);
                }
            });

        // Merge combined sampler suffixes
        if (ShaderResources.IsUsingCombinedSamplers() && ShaderResources.GetNumSamplers() > 0)
        {
            SignDesc.SetCombinedSamplerSuffix(ShaderResources.GetCombinedSamplerSuffix());
        }
    }

    return SignDesc;
}

#ifdef DILIGENT_DEVELOPMENT
void PipelineStateWebGPUImpl::DvpVerifySRBResources(const ShaderResourceCacheArrayType& ResourceCaches) const
{
    auto res_info = m_ResourceAttibutions.begin();
    for (const auto& pResources : m_ShaderResources)
    {
        pResources->ProcessResources(
            [&](const WGSLShaderResourceAttribs& ResAttribs, Uint32) //
            {
                VERIFY_EXPR(res_info->pSignature != nullptr);
                VERIFY_EXPR(res_info->pSignature->GetDesc().BindingIndex == res_info->SignatureIndex);
                const ShaderResourceCacheWebGPU* pResourceCache = ResourceCaches[res_info->SignatureIndex];
                DEV_CHECK_ERR(pResourceCache != nullptr, "Resource cache at index ", res_info->SignatureIndex, " is null.");
                if (res_info->IsImmutableSampler())
                {
                    res_info->pSignature->DvpValidateImmutableSampler(ResAttribs, res_info->ImmutableSamplerIndex, *pResourceCache,
                                                                      pResources->GetShaderName(), m_Desc.Name);
                }
                else
                {
                    res_info->pSignature->DvpValidateCommittedResource(ResAttribs, res_info->ResourceIndex, *pResourceCache,
                                                                       pResources->GetShaderName(), m_Desc.Name);
                }
                ++res_info;
            } //
        );
    }
    VERIFY_EXPR(res_info == m_ResourceAttibutions.end());
}
#endif

} // namespace Diligent
