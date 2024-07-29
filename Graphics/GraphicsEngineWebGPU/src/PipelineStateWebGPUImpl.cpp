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
    // Wait for asynchronous tasks to complete
    TPipelineStateBase::GetStatus(/*WaitForCompletion =*/true);

    // Note: we do not need to wait for the async callback to complete as
    // it keeps a reference to the async pipeline builder object and
    // can be called even if the pipeline is destroyed.

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
std::vector<PipelineStateWebGPUImpl::ShaderStageInfo> PipelineStateWebGPUImpl::InitInternalObjects(const PSOCreateInfoType& CreateInfo)
{
    std::vector<ShaderStageInfo> ShaderStages;
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
        std::string&            PatchedWGSL = ShaderStage.PatchedWGSL;
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
                Uint32 ArraySize       = 1;
                if (ResAttribution.ResourceIndex != ResourceAttribution::InvalidResourceIndex)
                {
                    PipelineResourceDesc ResDesc = ResAttribution.pSignature->GetResourceDesc(ResAttribution.ResourceIndex);
                    if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT)
                        ResDesc.ResourceType = SHADER_RESOURCE_TYPE_TEXTURE_SRV;

                    ValidatePipelineResourceCompatibility(ResDesc, ResType, Flags, WGSLAttribs.ArraySize,
                                                          pShader->GetDesc().Name, SignDesc.Name);

                    const auto& ResAttribs{ResAttribution.pSignature->GetResourceAttribs(ResAttribution.ResourceIndex)};
                    ResourceBinding = ResAttribs.BindingIndex;
                    BindGroup       = ResAttribs.BindGroup;
                    ArraySize       = ResAttribs.ArraySize;
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
                    ArraySize       = ImmtblSamAttribs.ArraySize;
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
                    ResMapping[WGSLAttribs.Name] = {BindGroup, ResourceBinding, ArraySize};
                }

                if (pDvpResourceAttibutions)
                    pDvpResourceAttibutions->emplace_back(ResAttribution);
            });

        if (!bVerifyOnly)
        {
            PatchedWGSL = RamapWGSLResourceBindings(pShader->GetWGSL(), ResMapping, pShader->GetEmulatedArrayIndexSuffix());
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

struct PipelineStateWebGPUImpl::AsyncPipelineBuilder : public ObjectBase<IObject>
{
    TShaderStages ShaderStages;

    // Shaders must be kept alive until the pipeline is created
    std::vector<RefCntAutoPtr<IShader>> ShaderRefs;

    WebGPURenderPipelineWrapper  wgpuRenderPipeline;
    WebGPUComputePipelineWrapper wgpuComputePipeline;

    enum CallbackStatus : int
    {
        NotStarted,
        InProgress,
        Completed
    };
    std::atomic<CallbackStatus> Status{CallbackStatus::NotStarted};

    AsyncPipelineBuilder(IReferenceCounters* pRefCounters,
                         TShaderStages&&     _ShaderStages) :
        ObjectBase<IObject>{pRefCounters},
        ShaderStages{std::move(_ShaderStages)}
    {
        ShaderRefs.reserve(ShaderStages.size());
        for (const auto& ShaderStage : ShaderStages)
        {
            ShaderRefs.emplace_back(ShaderStage.pShader);
        }
    }

    void InitializePipelines(WGPUCreatePipelineAsyncStatus PipelineStatus,
                             WGPURenderPipeline            RenderPipeline,
                             WGPUComputePipeline           ComputePipeline,
                             const char*                   Message)
    {
        VERIFY_EXPR(Status.load() == CallbackStatus::InProgress);
        if (PipelineStatus == WGPUCreatePipelineAsyncStatus_Success)
        {
            wgpuRenderPipeline.Reset(RenderPipeline);
            wgpuComputePipeline.Reset(ComputePipeline);
        }
        else
        {
            LOG_ERROR_MESSAGE("Failed to create WebGPU render pipeline: ", Message);
        }
        Status.store(CallbackStatus::Completed);
        Release();
    }

    static void CreateRenderPipelineCallback(WGPUCreatePipelineAsyncStatus Status, WGPURenderPipeline Pipeline, const char* Message, void* pUserData)
    {
        static_cast<AsyncPipelineBuilder*>(pUserData)->InitializePipelines(Status, Pipeline, nullptr, Message);
    }

    static void CreateRenderPipelineCallback2(WGPUCreatePipelineAsyncStatus Status, WGPURenderPipeline Pipeline, const char* Message, void* pUserData1, void* pUserData2)
    {
        CreateRenderPipelineCallback(Status, Pipeline, Message, pUserData1);
    }

    static void CreateComputePipelineCallback(WGPUCreatePipelineAsyncStatus Status, WGPUComputePipeline Pipeline, const char* Message, void* pUserData)
    {
        static_cast<AsyncPipelineBuilder*>(pUserData)->InitializePipelines(Status, nullptr, Pipeline, Message);
    }

    static void CreateComputePipelineCallback2(WGPUCreatePipelineAsyncStatus Status, WGPUComputePipeline Pipeline, const char* Message, void* pUserData1, void* pUserData2)
    {
        CreateComputePipelineCallback(Status, Pipeline, Message, pUserData1);
    }
};

void PipelineStateWebGPUImpl::InitializePipeline(const GraphicsPipelineStateCreateInfo& CreateInfo)
{
    TShaderStages ShaderStages = InitInternalObjects(CreateInfo);
    if (!m_InitializeTaskRunning.load())
    {
        InitializeWebGPURenderPipeline(ShaderStages);
    }
    else
    {
        m_AsyncBuilder = MakeNewRCObj<AsyncPipelineBuilder>()(std::move(ShaderStages));
    }
}


void PipelineStateWebGPUImpl::InitializePipeline(const ComputePipelineStateCreateInfo& CreateInfo)
{
    TShaderStages ShaderStages = InitInternalObjects(CreateInfo);
    if (!m_InitializeTaskRunning.load())
    {
        InitializeWebGPUComputePipeline(ShaderStages);
    }
    else
    {
        m_AsyncBuilder = MakeNewRCObj<AsyncPipelineBuilder>()(std::move(ShaderStages));
    }
}

void PipelineStateWebGPUImpl::InitializeWebGPURenderPipeline(const TShaderStages&  ShaderStages,
                                                             AsyncPipelineBuilder* AsyncBuilder)
{
    VERIFY(!ShaderStages.empty() && ShaderStages.size() <= 2, "Incorrect shader count for graphics pipeline");
    VERIFY(ShaderStages[0].Type == SHADER_TYPE_VERTEX, "Incorrect shader type: vertex shader is expected");
    VERIFY(ShaderStages.size() < 2 || ShaderStages[1].Type == SHADER_TYPE_PIXEL, "Incorrect shader type: compute shader is expected");

    const auto& GraphicsPipeline = GetGraphicsPipelineDesc();

    WGPUVertexState       wgpuVertexState{};
    WGPUPrimitiveState    wgpuPrimitiveState{};
    WGPUFragmentState     wgpuFragmentState{};
    WGPUDepthStencilState wgpuDepthStencilState{};
    WGPUMultisampleState  wgpuMultisampleState{};

    using WebGPUVertexAttributeArray = std::array<std::vector<WGPUVertexAttribute>, MAX_LAYOUT_ELEMENTS>;
    using WebGPUVertexBufferLayouts  = std::array<WGPUVertexBufferLayout, MAX_LAYOUT_ELEMENTS>;

    WebGPUVertexAttributeArray    wgpuVertexAttributes{};
    WebGPUVertexBufferLayouts     wgpuVertexBufferLayouts{};
    WGPUPrimitiveDepthClipControl wgpuDepthClipControl{};

    std::vector<WGPUColorTargetState>      wgpuColorTargetStates{};
    std::vector<WGPUBlendState>            wgpuBlendStates{};
    std::vector<WebGPUShaderModuleWrapper> wgpuShaderModules{ShaderStages.size()};

    for (size_t ShaderIdx = 0; ShaderIdx < ShaderStages.size(); ++ShaderIdx)
    {
        WGPUShaderModuleDescriptor     wgpuShaderModuleDesc{};
        WGPUShaderModuleWGSLDescriptor wgpuShaderCodeDesc{};
        wgpuShaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
        wgpuShaderCodeDesc.code        = ShaderStages[ShaderIdx].GetWGSL().c_str();

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

        if (ShaderStages.size() > 1)
        {
            VERIFY(ShaderStages[1].Type == SHADER_TYPE_PIXEL, "Incorrect shader type: pixel shader is expected");
            wgpuFragmentState.targetCount = static_cast<uint32_t>(wgpuColorTargetStates.size());
            wgpuFragmentState.targets     = wgpuColorTargetStates.data();
            wgpuFragmentState.module      = wgpuShaderModules[1].Get();
            wgpuFragmentState.entryPoint  = ShaderStages[1].pShader->GetEntryPoint();
        }
    }

    {
        const auto& DepthStencilDesc = GraphicsPipeline.DepthStencilDesc;

        wgpuDepthStencilState.format            = TextureFormatToWGPUFormat(GraphicsPipeline.DSVFormat);
        wgpuDepthStencilState.depthCompare      = DepthStencilDesc.DepthEnable ? ComparisonFuncToWGPUCompareFunction(DepthStencilDesc.DepthFunc) : WGPUCompareFunction_Always;
        wgpuDepthStencilState.depthWriteEnabled = DepthStencilDesc.DepthEnable ? DepthStencilDesc.DepthWriteEnable : false;

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

        wgpuDepthStencilState.depthBias           = static_cast<int32_t>(GraphicsPipeline.RasterizerDesc.DepthBias);
        wgpuDepthStencilState.depthBiasSlopeScale = GraphicsPipeline.RasterizerDesc.SlopeScaledDepthBias;
        wgpuDepthStencilState.depthBiasClamp      = GraphicsPipeline.RasterizerDesc.DepthBiasClamp;
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

        if (!GraphicsPipeline.RasterizerDesc.DepthClipEnable)
        {
            wgpuDepthClipControl.chain.sType    = WGPUSType_PrimitiveDepthClipControl;
            wgpuDepthClipControl.unclippedDepth = !GraphicsPipeline.RasterizerDesc.DepthClipEnable;
            if (m_pDevice->GetDeviceInfo().Features.DepthClamp)
                wgpuPrimitiveState.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuDepthClipControl);
            else
                LOG_WARNING_MESSAGE("Depth clamping is not supported by the device. The depth clip control will be ignored.");
        }
    }

    {
        wgpuMultisampleState.alphaToCoverageEnabled = GraphicsPipeline.BlendDesc.AlphaToCoverageEnable;
        wgpuMultisampleState.mask                   = GraphicsPipeline.SampleMask;
        wgpuMultisampleState.count                  = GraphicsPipeline.SmplDesc.Count;
    }

    WGPURenderPipelineDescriptor wgpuRenderPipelineDesc{};
    wgpuRenderPipelineDesc.label        = GetDesc().Name;
    wgpuRenderPipelineDesc.vertex       = wgpuVertexState;
    wgpuRenderPipelineDesc.fragment     = wgpuFragmentState.module != nullptr ? &wgpuFragmentState : nullptr;
    wgpuRenderPipelineDesc.depthStencil = GraphicsPipeline.DSVFormat != TEX_FORMAT_UNKNOWN ? &wgpuDepthStencilState : nullptr;
    wgpuRenderPipelineDesc.primitive    = wgpuPrimitiveState;
    wgpuRenderPipelineDesc.multisample  = wgpuMultisampleState;
    wgpuRenderPipelineDesc.layout       = m_PipelineLayout.GetWebGPUPipelineLayout();

    if (AsyncBuilder)
    {
        // The reference will be released from the callback.
        AsyncBuilder->AddRef();
#if PLATFORM_EMSCRIPTEN
        wgpuDeviceCreateRenderPipelineAsync(m_pDevice->GetWebGPUDevice(), &wgpuRenderPipelineDesc, AsyncPipelineBuilder::CreateRenderPipelineCallback, AsyncBuilder);
#else
        wgpuDeviceCreateRenderPipelineAsync2(m_pDevice->GetWebGPUDevice(), &wgpuRenderPipelineDesc,
                                             {
                                                 nullptr,
                                                 WGPUCallbackMode_AllowSpontaneous,
                                                 AsyncPipelineBuilder::CreateRenderPipelineCallback2,
                                                 AsyncBuilder,
                                                 nullptr,
                                             });
#endif
    }
    else
    {
        m_wgpuRenderPipeline.Reset(wgpuDeviceCreateRenderPipeline(m_pDevice->GetWebGPUDevice(), &wgpuRenderPipelineDesc));
        if (!m_wgpuRenderPipeline)
            LOG_ERROR_AND_THROW("Failed to create pipeline state");
    }
}

void PipelineStateWebGPUImpl::InitializeWebGPUComputePipeline(const TShaderStages&  ShaderStages,
                                                              AsyncPipelineBuilder* AsyncBuilder)
{
    VERIFY(ShaderStages[0].Type == SHADER_TYPE_COMPUTE, "Incorrect shader type: compute shader is expected");
    ShaderWebGPUImpl* pShaderWebGPU = ShaderStages[0].pShader;

    WebGPUShaderModuleWrapper wgpuShaderModule{};

    WGPUShaderModuleWGSLDescriptor wgpuShaderCodeDesc{};
    wgpuShaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgpuShaderCodeDesc.code        = ShaderStages[0].GetWGSL().c_str();

    WGPUShaderModuleDescriptor wgpuShaderModuleDesc{};
    wgpuShaderModuleDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuShaderCodeDesc);
    wgpuShaderModuleDesc.label       = pShaderWebGPU->GetEntryPoint();
    wgpuShaderModule.Reset(wgpuDeviceCreateShaderModule(m_pDevice->GetWebGPUDevice(), &wgpuShaderModuleDesc));

    WGPUComputePipelineDescriptor wgpuComputePipelineDesc{};
    wgpuComputePipelineDesc.label              = GetDesc().Name;
    wgpuComputePipelineDesc.compute.module     = wgpuShaderModule.Get();
    wgpuComputePipelineDesc.compute.entryPoint = pShaderWebGPU->GetEntryPoint();
    wgpuComputePipelineDesc.layout             = m_PipelineLayout.GetWebGPUPipelineLayout();

    if (AsyncBuilder)
    {
        // The reference will be released from the callback.
        AsyncBuilder->AddRef();
#if PLATFORM_EMSCRIPTEN
        wgpuDeviceCreateComputePipelineAsync(m_pDevice->GetWebGPUDevice(), &wgpuComputePipelineDesc, AsyncPipelineBuilder::CreateComputePipelineCallback, AsyncBuilder);
#else
        wgpuDeviceCreateComputePipelineAsync2(m_pDevice->GetWebGPUDevice(), &wgpuComputePipelineDesc,
                                              {
                                                  nullptr,
                                                  WGPUCallbackMode_AllowSpontaneous,
                                                  AsyncPipelineBuilder::CreateComputePipelineCallback2,
                                                  AsyncBuilder,
                                                  nullptr,
                                              });
#endif
    }
    else
    {
        m_wgpuComputePipeline.Reset(wgpuDeviceCreateComputePipeline(m_pDevice->GetWebGPUDevice(), &wgpuComputePipelineDesc));
        if (!m_wgpuComputePipeline)
            LOG_ERROR_AND_THROW("Failed to create pipeline state");
    }
}

PIPELINE_STATE_STATUS PipelineStateWebGPUImpl::GetStatus(bool WaitForCompletion)
{
    // Check the status of asynchronous tasks
    PIPELINE_STATE_STATUS Status = TPipelineStateBase::GetStatus(WaitForCompletion);
    if (Status != PIPELINE_STATE_STATUS_READY)
    {
        if (Status == PIPELINE_STATE_STATUS_FAILED && m_AsyncBuilder)
            m_AsyncBuilder.Release();

        return Status;
    }

    if (m_AsyncBuilder)
    {
        AsyncPipelineBuilder::CallbackStatus CallbackStatus = m_AsyncBuilder->Status.load();
        switch (CallbackStatus)
        {
            case AsyncPipelineBuilder::CallbackStatus::NotStarted:
                m_AsyncBuilder->Status.store(AsyncPipelineBuilder::CallbackStatus::InProgress);
                if (m_Desc.IsAnyGraphicsPipeline())
                    InitializeWebGPURenderPipeline(m_AsyncBuilder->ShaderStages, m_AsyncBuilder);
                else if (m_Desc.IsComputePipeline())
                    InitializeWebGPUComputePipeline(m_AsyncBuilder->ShaderStages, m_AsyncBuilder);
                else
                    UNEXPECTED("Unexpected pipeline type");
                // Do not change the m_Status
                return PIPELINE_STATE_STATUS_COMPILING;

            case AsyncPipelineBuilder::CallbackStatus::InProgress:
                // Keep waiting
                return PIPELINE_STATE_STATUS_COMPILING;

            case AsyncPipelineBuilder::CallbackStatus::Completed:
                m_wgpuRenderPipeline  = std::move(m_AsyncBuilder->wgpuRenderPipeline);
                m_wgpuComputePipeline = std::move(m_AsyncBuilder->wgpuComputePipeline);
                m_AsyncBuilder.Release();
                m_Status.store(m_wgpuRenderPipeline || m_wgpuComputePipeline ? PIPELINE_STATE_STATUS_READY : PIPELINE_STATE_STATUS_FAILED);
                break;

            default:
                UNEXPECTED("Unexpected status");
        }
    }

    return m_Status.load();
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
                    (ShaderResources.IsUsingCombinedSamplers() && (Attribs.Type == WGSLShaderResourceAttribs::ResourceType::Sampler || Attribs.Type == WGSLShaderResourceAttribs::ResourceType::ComparisonSampler)) ?
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
                    const WebGPUResourceAttribs   WebGPUAttribs = Attribs.GetWebGPUAttribs(VarDesc.Flags);
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
