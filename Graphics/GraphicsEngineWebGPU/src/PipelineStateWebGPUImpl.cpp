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

void PipelineStateWebGPUImpl::InitPipelineLayout(const PipelineStateCreateInfo& CreateInfo, TShaderStages& ShaderStages)
{
    const auto InternalFlags = GetInternalCreateFlags(CreateInfo);
    if (m_UsingImplicitSignature && (InternalFlags & PSO_CREATE_INTERNAL_FLAG_IMPLICIT_SIGNATURE0) == 0)
    {
        const auto SignDesc = GetDefaultResourceSignatureDesc(ShaderStages, m_Desc.Name, m_Desc.ResourceLayout, m_Desc.SRBAllocationGranularity);
        InitDefaultSignature(SignDesc, GetActiveShaderStages(), false /*bIsDeviceInternal*/);
        VERIFY_EXPR(m_Signatures[0]);
    }
}

void PipelineStateWebGPUImpl::InitializePipeline(const GraphicsPipelineStateCreateInfo& CreateInfo)
{
    const auto ShaderStages = InitInternalObjects(CreateInfo);

    VERIFY(ShaderStages.size() == 2, "Incorrect shader count for graphics pipeline");
    VERIFY(ShaderStages[0].Type == SHADER_TYPE_VERTEX, "Incorrect shader type: vertex shader is expected");
    VERIFY(ShaderStages[1].Type == SHADER_TYPE_PIXEL, "Incorrect shader type: compute shader is expected");

    const auto& GraphicsPipeline = GetGraphicsPipelineDesc();

    WGPUVertexState                        wgpuVertexState{};
    WGPUPrimitiveState                     wgpuPrimitiveState{};
    WGPUFragmentState                      wgpuFragmentState{};
    WGPUDepthStencilState                  wgpuDepthStencilState{};
    WGPUMultisampleState                   wgpuMultisampleState{};
    std::vector<WGPUColorTargetState>      wgpuColorTargetStates{};
    std::vector<WGPUBlendState>            wgpuBlendStates{};
    std::vector<WebGPUShaderModuleWrapper> wgpuShaderModules{ShaderStages.size()};

    for (size_t ShaderIdx = 0; ShaderIdx < ShaderStages.size(); ++ShaderIdx)
    {
        WGPUShaderModuleDescriptor     wgpuShaderModuleDesc{};
        WGPUShaderModuleWGSLDescriptor wgpuShaderCodeDesc{};
        wgpuShaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
        wgpuShaderCodeDesc.code        = ShaderStages[ShaderIdx].pShader->GetWGSL().c_str();

        wgpuShaderModuleDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuShaderCodeDesc);
        wgpuShaderModuleDesc.label       = ShaderStages[ShaderIdx].pShader->GetEntryPoint();
        wgpuShaderModules[ShaderIdx].Reset(wgpuDeviceCreateShaderModule(m_pDevice->GetWebGPUDevice(), &wgpuShaderModuleDesc));
    }

    wgpuColorTargetStates.reserve(GraphicsPipeline.NumRenderTargets);
    wgpuBlendStates.reserve(GraphicsPipeline.NumRenderTargets);

    {
        wgpuVertexState.module     = wgpuShaderModules[0].Get();
        wgpuVertexState.entryPoint = ShaderStages[0].pShader->GetEntryPoint();
    }

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
    }

    {
        const auto& RasterizerDesc = GraphicsPipeline.RasterizerDesc;

        wgpuPrimitiveState.frontFace        = RasterizerDesc.FrontCounterClockwise ? WGPUFrontFace_CCW : WGPUFrontFace_CW;
        wgpuPrimitiveState.cullMode         = CullModeToWGPUCullMode(RasterizerDesc.CullMode);
        wgpuPrimitiveState.topology         = PrimitiveTopologyWGPUPrimitiveType(GraphicsPipeline.PrimitiveTopology);
        wgpuPrimitiveState.stripIndexFormat = WGPUIndexFormat_Undefined;
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

    m_wgpuRenderPipeline.Reset(wgpuDeviceCreateRenderPipeline(m_pDevice->GetWebGPUDevice(), &wgpuRenderPipelineDesc));
    if (!m_wgpuRenderPipeline)
        LOG_ERROR_AND_THROW("Failed to create pipeline state");
}

void PipelineStateWebGPUImpl::InitializePipeline(const ComputePipelineStateCreateInfo& CreateInfo)
{
    const auto ShaderStages = InitInternalObjects(CreateInfo);
    VERIFY(ShaderStages[0].Type == SHADER_TYPE_COMPUTE, "Incorrect shader type: compute shader is expected");

    auto* pShaderWebGPU = ShaderStages[0].pShader;

    WebGPUShaderModuleWrapper wgpuShaderModule{};

    WGPUShaderModuleWGSLDescriptor wgpuShaderCodeDesc{};
    wgpuShaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgpuShaderCodeDesc.code        = pShaderWebGPU->GetWGSL().c_str();

    WGPUShaderModuleDescriptor wgpuShaderModuleDesc{};
    wgpuShaderModuleDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuShaderCodeDesc);
    wgpuShaderModuleDesc.label       = pShaderWebGPU->GetEntryPoint();
    wgpuShaderModule.Reset(wgpuDeviceCreateShaderModule(m_pDevice->GetWebGPUDevice(), &wgpuShaderModuleDesc));

    WGPUComputePipelineDescriptor wgpuComputePipelineDesc{};
    wgpuComputePipelineDesc.label              = GetDesc().Name;
    wgpuComputePipelineDesc.compute.module     = wgpuShaderModule.Get();
    wgpuComputePipelineDesc.compute.entryPoint = pShaderWebGPU->GetEntryPoint();

    m_wgpuComputePipeline.Reset(wgpuDeviceCreateComputePipeline(m_pDevice->GetWebGPUDevice(), &wgpuComputePipelineDesc));
    if (!m_wgpuComputePipeline)
        LOG_ERROR_AND_THROW("Failed to create pipeline state");
}

PipelineResourceSignatureDescWrapper PipelineStateWebGPUImpl::GetDefaultResourceSignatureDesc(const TShaderStages&              ShaderStages,
                                                                                              const char*                       PSOName,
                                                                                              const PipelineResourceLayoutDesc& ResourceLayout,
                                                                                              Uint32                            SRBAllocationGranularity)
{
    PipelineResourceSignatureDescWrapper SignDesc{PSOName, ResourceLayout, SRBAllocationGranularity};

    // TODO
    return SignDesc;
}

} // namespace Diligent
