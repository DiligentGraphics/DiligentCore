/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#include "AttachmentCleanerWebGPU.hpp"

#include "BasicMath.hpp"
#include "DebugUtilities.hpp"
#include "WebGPUTypeConversions.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

namespace
{

constexpr char ShaderSource[] = R"(
struct ClearConstants
{
    Color: vec4f,
    Depth: f32,
    Padding0: f32,
    Padding1: f32,
    Padding2: f32,
}

@group(0) @binding(0) var<uniform> UniformBuffer: ClearConstants;

struct VertexOutput
{
    @builtin(position) Position: vec4f,
    @location(0)       Color: vec4f,
}

@vertex
fn VSMain(@builtin(vertex_index) VertexId : u32) -> VertexOutput
{
    let Texcoord: vec2f = vec2f(f32((VertexId << 1u) & 2u), f32(VertexId & 2u));
    let Position: vec4f = vec4f(Texcoord * vec2f(2.0f, -2.0f) + vec2f(-1.0f, 1.0f), UniformBuffer.Depth, 1.0f);

    var Output: VertexOutput;
    Output.Position = Position;
    Output.Color    = UniformBuffer.Color;
    return Output;
}

@fragment
fn PSMain(Input: VertexOutput) -> @location(MACRO_RTV_INDEX) vec4f 
{
    return Input.Color;
}
)";

bool operator==(const WGPUStencilFaceState& LHS, const WGPUStencilFaceState& RHS)
{
    // clang-format off
    return LHS.compare     == RHS.compare     &&
           LHS.depthFailOp == RHS.depthFailOp &&
           LHS.failOp      == RHS.failOp      &&
           LHS.passOp      == RHS.passOp;
    // clang-format on
}

bool operator==(const WGPUDepthStencilState& LHS, const WGPUDepthStencilState& RHS)
{
    // clang-format off
    return LHS.format              == RHS.format            &&
           LHS.depthWriteEnabled   == RHS.depthWriteEnabled &&
           LHS.depthCompare        == RHS.depthCompare      &&
           LHS.stencilFront        == RHS.stencilFront      &&
           LHS.stencilBack         == RHS.stencilFront      &&
           LHS.stencilReadMask     == RHS.stencilReadMask   &&
           LHS.stencilWriteMask    == RHS.stencilWriteMask  &&
           LHS.depthBias           == RHS.depthBias         &&
           LHS.depthBiasSlopeScale == RHS.depthBiasSlopeScale &&
           LHS.depthBiasClamp      == RHS.depthBiasClamp;
    // clang-format on
}

std::string ReplaceRTVIndex(const std::string& SourceString, const std::string& NewValue)
{
    std::string Result = SourceString;
    std::string Macro  = "MACRO_RTV_INDEX";
    size_t      Iter   = Result.find(Macro);

    while (Iter != std::string::npos)
    {
        Result.replace(Iter, Macro.length(), NewValue);
        Iter = Result.find(Macro, Iter + NewValue.length());
    }

    return Result;
}

} // namespace

bool AttachmentCleanerWebGPU::RenderPassInfo::operator==(const RenderPassInfo& rhs) const
{
    // clang-format off
    if (NumRenderTargets != rhs.NumRenderTargets ||
        SampleCount      != rhs.SampleCount ||
        DSVFormat        != rhs.DSVFormat)
        return false;
    // clang-format on

    for (Uint32 RTIndex = 0; RTIndex < NumRenderTargets; ++RTIndex)
        if (RTVFormats[RTIndex] != rhs.RTVFormats[RTIndex])
            return false;

    return true;
}

size_t AttachmentCleanerWebGPU::RenderPassInfo::GetHash() const
{
    size_t Hash = ComputeHash(NumRenderTargets, Uint32{DSVFormat}, Uint32{SampleCount});
    for (Uint32 RTIndex = 0; RTIndex < NumRenderTargets; ++RTIndex)
        HashCombine(Hash, Uint32{RTVFormats[RTIndex]});
    return Hash;
}

bool AttachmentCleanerWebGPU::ClearPSOHashKey::operator==(const ClearPSOHashKey& rhs) const
{
    if (PSOHash != rhs.PSOHash)
        return false;
    // clang-format off
    return RPInfo     == rhs.RPInfo    &&
           ColorMask  == rhs.ColorMask &&
           RTIndex    == rhs.RTIndex   &&
           DepthState == rhs.DepthState;
    // clang-format on
}

size_t AttachmentCleanerWebGPU::ClearPSOHashKey::Hasher::operator()(const ClearPSOHashKey& Key) const
{
    if (Key.PSOHash == 0)
        Key.PSOHash = ComputeHash(Key.RPInfo.GetHash(), static_cast<Int32>(Key.ColorMask), Key.RTIndex, ComputeHashRaw(&Key.DepthState, sizeof(WGPUDepthStencilState)));

    return Key.PSOHash;
}

AttachmentCleanerWebGPU::AttachmentCleanerWebGPU(WGPUDevice wgpuDevice, Uint32 CleanBufferMaxElementCount) :
    m_wgpuDevice{wgpuDevice},
    m_BufferMaxElementCount{CleanBufferMaxElementCount}
{
    InitializePipelineStates();
    InitializeDynamicUniformBuffer();
    InitializePipelineResourceLayout();
}

void AttachmentCleanerWebGPU::ResetDynamicUniformBuffer()
{
    m_CurrBufferOffset = 0;
}

void AttachmentCleanerWebGPU::ClearColor(WGPURenderPassEncoder wgpuCmdEncoder,
                                         const RenderPassInfo& RPInfo,
                                         COLOR_MASK            ColorMask,
                                         Uint32                RTIndex,
                                         const float           Color[])
{
    ClearPSOHashKey Key;
    Key.RPInfo     = RPInfo;
    Key.ColorMask  = ColorMask;
    Key.RTIndex    = static_cast<Int32>(RTIndex);
    Key.DepthState = m_wgpuDisableDepth;

    std::array<float, 8> ClearData = {Color[0], Color[1], Color[2], Color[3]};
    ClearAttachment(wgpuCmdEncoder, Key, ClearData);
}

void AttachmentCleanerWebGPU::ClearDepthStencil(WGPURenderPassEncoder     wgpuCmdEncoder,
                                                const RenderPassInfo&     RPInfo,
                                                CLEAR_DEPTH_STENCIL_FLAGS Flags,
                                                float                     Depth,
                                                Uint8                     Stencil)
{
    ClearPSOHashKey Key{};
    Key.RPInfo  = RPInfo;
    Key.RTIndex = -1;

    if ((Flags & CLEAR_STENCIL_FLAG) != 0)
    {
        wgpuRenderPassEncoderSetStencilReference(wgpuCmdEncoder, Stencil);
        Key.DepthState = (Flags & CLEAR_DEPTH_FLAG) != 0 ? m_wgpuWriteDepthStencil : m_wgpuWriteStencil;
    }
    else
    {
        VERIFY((Flags & CLEAR_DEPTH_FLAG) != 0, "At least one of CLEAR_DEPTH_FLAG or CLEAR_STENCIL_FLAG flags should be set");
        Key.DepthState = m_wgpuWriteDepth;
    }

    std::array<float, 8> ClearData = {0, 0, 0, 0, Depth};
    ClearAttachment(wgpuCmdEncoder, Key, ClearData);
}

WebGPURenderPipelineWrapper AttachmentCleanerWebGPU::CreatePSO(const ClearPSOHashKey& Key) const
{
    WebGPURenderPipelineWrapper wgpuPipeline;

    try
    {
        std::string ModifiedShaderSource = ReplaceRTVIndex(ShaderSource, std::to_string(Key.RTIndex < 0 ? 0 : Key.RTIndex));

        WGPUShaderModuleWGSLDescriptor wgpuShaderCodeDesc{};
        wgpuShaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
        wgpuShaderCodeDesc.code        = ModifiedShaderSource.c_str();

        WGPUShaderModuleDescriptor wgpuShaderModuleDesc{};
        wgpuShaderModuleDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgpuShaderCodeDesc);
        WebGPUShaderModuleWrapper wgpuShaderModule{wgpuDeviceCreateShaderModule(m_wgpuDevice, &wgpuShaderModuleDesc)};
        if (!wgpuShaderModule)
            LOG_ERROR_AND_THROW("Failed to create shader module");

        const auto& RPInfo = Key.RPInfo;

        WGPUColorTargetState wgpuColorTargetState[MAX_RENDER_TARGETS]{};
        for (Uint32 RTIndex = 0; RTIndex < RPInfo.NumRenderTargets; ++RTIndex)
        {
            wgpuColorTargetState[RTIndex].format    = TextureFormatToWGPUFormat(RPInfo.RTVFormats[RTIndex]);
            wgpuColorTargetState[RTIndex].writeMask = ColorMaskToWGPUColorWriteMask(Key.ColorMask);
        }

        WGPUDepthStencilState wgpuDepthStencilState = Key.DepthState;
        wgpuDepthStencilState.format                = TextureFormatToWGPUFormat(RPInfo.DSVFormat);

        WGPUFragmentState wgpuFragmentState{};
        wgpuFragmentState.module      = wgpuShaderModule.Get();
        wgpuFragmentState.entryPoint  = "PSMain";
        wgpuFragmentState.targetCount = RPInfo.NumRenderTargets;
        wgpuFragmentState.targets     = wgpuColorTargetState;

        WGPURenderPipelineDescriptor wgpuRenderPipelineDesc{};
        wgpuRenderPipelineDesc.label              = "AttachmentCleanerPSO";
        wgpuRenderPipelineDesc.layout             = m_PipelineResourceLayout.wgpuPipelineLayout.Get();
        wgpuRenderPipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        wgpuRenderPipelineDesc.vertex.module      = wgpuShaderModule.Get();
        wgpuRenderPipelineDesc.vertex.entryPoint  = "VSMain";
        wgpuRenderPipelineDesc.fragment           = Key.RTIndex < 0 ? nullptr : &wgpuFragmentState; // Do we need empty fragment shader for depth-stencil clears?
        wgpuRenderPipelineDesc.depthStencil       = wgpuDepthStencilState.depthWriteEnabled ? &wgpuDepthStencilState : nullptr;
        wgpuRenderPipelineDesc.multisample.count  = RPInfo.SampleCount;
        wgpuRenderPipelineDesc.multisample.mask   = 0xFFFFFFFF;

        wgpuPipeline.Reset(wgpuDeviceCreateRenderPipeline(m_wgpuDevice, &wgpuRenderPipelineDesc));

        if (!wgpuPipeline)
            LOG_ERROR_AND_THROW("Failed to create clear attachment render pipeline");
    }
    catch (...)
    {
    }

    return wgpuPipeline;
}

void AttachmentCleanerWebGPU::ClearAttachment(WGPURenderPassEncoder wgpuCmdEncoder, const ClearPSOHashKey& Key, std::array<float, 8>& ClearData)
{
    auto Iter = m_PSOCache.find(Key);
    if (Iter == m_PSOCache.end())
        Iter = m_PSOCache.emplace(Key, CreatePSO(Key)).first;

    WGPURenderPipeline wgpuPipelineState = Iter->second.Get();
    if (wgpuPipelineState == nullptr)
    {
        UNEXPECTED("Clear attachment PSO is null");
        return;
    }

    Uint32 DynamicOffsets[] = {m_CurrBufferOffset};
    m_CurrBufferOffset += m_BufferElementSize;
    VERIFY(m_CurrBufferOffset < m_BufferMaxElementCount * m_BufferElementSize, "Buffer offset more then buffer size");

    WGPUQueue wgpuQueue = wgpuDeviceGetQueue(m_wgpuDevice);
    wgpuQueueWriteBuffer(wgpuQueue, m_wgpuBuffer.Get(), DynamicOffsets[0], ClearData.data(), sizeof(float) * ClearData.size());

    wgpuRenderPassEncoderSetPipeline(wgpuCmdEncoder, wgpuPipelineState);
    wgpuRenderPassEncoderSetBindGroup(wgpuCmdEncoder, 0, m_PipelineResourceLayout.wgpuBindGroup.Get(), _countof(DynamicOffsets), DynamicOffsets);
    wgpuRenderPassEncoderDraw(wgpuCmdEncoder, 3, 1, 0, 0);
}

void AttachmentCleanerWebGPU::InitializePipelineStates()
{
    m_wgpuDisableDepth.depthCompare      = WGPUCompareFunction_Always;
    m_wgpuDisableDepth.depthWriteEnabled = false;

    m_wgpuWriteDepth.depthCompare      = WGPUCompareFunction_Always;
    m_wgpuWriteDepth.depthWriteEnabled = true;

    m_wgpuWriteStencil.depthCompare             = WGPUCompareFunction_Never;
    m_wgpuWriteStencil.depthWriteEnabled        = true;
    m_wgpuWriteStencil.stencilFront.compare     = WGPUCompareFunction_Always;
    m_wgpuWriteStencil.stencilFront.depthFailOp = WGPUStencilOperation_Replace;
    m_wgpuWriteStencil.stencilFront.failOp      = WGPUStencilOperation_Replace;
    m_wgpuWriteStencil.stencilFront.passOp      = WGPUStencilOperation_Replace;
    m_wgpuWriteStencil.stencilBack.compare      = WGPUCompareFunction_Always;
    m_wgpuWriteStencil.stencilBack.depthFailOp  = WGPUStencilOperation_Replace;
    m_wgpuWriteStencil.stencilBack.failOp       = WGPUStencilOperation_Replace;
    m_wgpuWriteStencil.stencilBack.passOp       = WGPUStencilOperation_Replace;

    m_wgpuWriteDepthStencil.depthCompare      = WGPUCompareFunction_Always;
    m_wgpuWriteDepthStencil.depthWriteEnabled = true;
    m_wgpuWriteDepthStencil.stencilFront      = m_wgpuWriteStencil.stencilFront;
    m_wgpuWriteDepthStencil.stencilBack       = m_wgpuWriteStencil.stencilBack;
}

void AttachmentCleanerWebGPU::InitializeDynamicUniformBuffer()
{
    // Rework when push constants will be available https://github.com/gpuweb/gpuweb/pull/4612 in WebGPU

    WGPUSupportedLimits wgpuLimits{};
    wgpuDeviceGetLimits(m_wgpuDevice, &wgpuLimits);

    constexpr Uint32 SizeofUniformBuffer = 2u * sizeof(float4);
    m_BufferElementSize                  = AlignUp(SizeofUniformBuffer, wgpuLimits.limits.minUniformBufferOffsetAlignment);

    WGPUBufferDescriptor wgpuBufferDesc{};
    wgpuBufferDesc.size  = m_BufferMaxElementCount * static_cast<Uint64>(m_BufferElementSize);
    wgpuBufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    m_wgpuBuffer.Reset(wgpuDeviceCreateBuffer(m_wgpuDevice, &wgpuBufferDesc));
}

void AttachmentCleanerWebGPU::InitializePipelineResourceLayout()
{
    WGPUBindGroupLayoutEntry wgpuBindGroupLayoutEntries[1]{};
    wgpuBindGroupLayoutEntries[0].binding                 = 0;
    wgpuBindGroupLayoutEntries[0].visibility              = WGPUShaderStage_Vertex;
    wgpuBindGroupLayoutEntries[0].buffer.type             = WGPUBufferBindingType_Uniform;
    wgpuBindGroupLayoutEntries[0].buffer.hasDynamicOffset = true;
    wgpuBindGroupLayoutEntries[0].buffer.minBindingSize   = 0;

    WGPUBindGroupLayoutDescriptor wgpuBindGroupLayoutDesc{};
    wgpuBindGroupLayoutDesc.entries    = wgpuBindGroupLayoutEntries;
    wgpuBindGroupLayoutDesc.entryCount = _countof(wgpuBindGroupLayoutEntries);

    m_PipelineResourceLayout.wgpuBindGroupLayout.Reset(wgpuDeviceCreateBindGroupLayout(m_wgpuDevice, &wgpuBindGroupLayoutDesc));
    if (!m_PipelineResourceLayout.wgpuBindGroupLayout)
        LOG_ERROR_AND_THROW("Failed to create clear attachment bind group layout");

    WGPUPipelineLayoutDescriptor wgpuPipelineLayoutDesc{};
    wgpuPipelineLayoutDesc.label                = "AttachmentCleanerLayout";
    wgpuPipelineLayoutDesc.bindGroupLayouts     = &m_PipelineResourceLayout.wgpuBindGroupLayout.Get();
    wgpuPipelineLayoutDesc.bindGroupLayoutCount = 1;
    m_PipelineResourceLayout.wgpuPipelineLayout.Reset(wgpuDeviceCreatePipelineLayout(m_wgpuDevice, &wgpuPipelineLayoutDesc));
    if (!m_PipelineResourceLayout.wgpuPipelineLayout)
        LOG_ERROR_AND_THROW("Failed to create clear attachment pipeline layout");

    WGPUBindGroupEntry wgpuBindGroupEntry[1]{};
    wgpuBindGroupEntry[0].binding = 0;
    wgpuBindGroupEntry[0].buffer  = m_wgpuBuffer.Get();
    wgpuBindGroupEntry[0].offset  = 0;
    wgpuBindGroupEntry[0].size    = static_cast<Uint64>(m_BufferElementSize) * m_BufferMaxElementCount;

    WGPUBindGroupDescriptor wgpuBindGroupDesc{};
    wgpuBindGroupDesc.layout     = m_PipelineResourceLayout.wgpuBindGroupLayout.Get();
    wgpuBindGroupDesc.entries    = wgpuBindGroupEntry;
    wgpuBindGroupDesc.entryCount = _countof(wgpuBindGroupEntry);
    m_PipelineResourceLayout.wgpuBindGroup.Reset(wgpuDeviceCreateBindGroup(m_wgpuDevice, &wgpuBindGroupDesc));
    if (!m_PipelineResourceLayout.wgpuBindGroup)
        LOG_ERROR_AND_THROW("Failed to create clear attachment bind group");
}

} // namespace Diligent
