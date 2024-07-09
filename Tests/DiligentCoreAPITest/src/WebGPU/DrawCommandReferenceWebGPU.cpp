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

#include "WebGPU/TestingEnvironmentWebGPU.hpp"
#include "WebGPU/TestingSwapChainWebGPU.hpp"

#include "InlineShaders/DrawCommandTestWGSL.h"

namespace Diligent
{

namespace Testing
{

namespace
{

WGPUTextureFormat ConvertTexFormatToWGPUTextureFormat(TEXTURE_FORMAT Format)
{
    switch (Format)
    {
        case TEX_FORMAT_RGBA8_UNORM:
            return WGPUTextureFormat_RGBA8Unorm;
        default:
            UNSUPPORTED("Unsupported swap chain format");
    }
    return WGPUTextureFormat_Undefined;
}

WGPUColor ConvertArrayToWGPUColor(const float* pClearColor)
{
    return pClearColor != nullptr ? WGPUColor{pClearColor[0], pClearColor[1], pClearColor[2], pClearColor[3]} : WGPUColor{0.0, 0.0, 0.0, 0.0};
}

class ReferenceTriangleRenderer
{
public:
    ReferenceTriangleRenderer(ISwapChain*     pSwapChain,
                              Uint32          SampleCount         = 1,
                              WGPUTextureView InputAttachmentView = nullptr)
    {
        auto* pEnvWebGPU = TestingEnvironmentWebGPU::GetInstance();

        m_wgpuVSModule = pEnvWebGPU->CreateShaderModule(WGSL::DrawTest_ProceduralTriangleVS);
        VERIFY_EXPR(m_wgpuVSModule != nullptr);

        m_wgpuPSModule = pEnvWebGPU->CreateShaderModule(InputAttachmentView != nullptr ? WGSL::InputAttachmentTest_PS : WGSL::DrawTest_PS);
        VERIFY_EXPR(m_wgpuPSModule != nullptr);

        WGPUPipelineLayoutDescriptor wgpuPipelineLayoutDesc{};
        if (InputAttachmentView != nullptr)
        {
            WGPUBindGroupLayoutEntry wgpuBindGroupLayoutEntries[1]{};
            wgpuBindGroupLayoutEntries[0].binding               = 0;
            wgpuBindGroupLayoutEntries[0].visibility            = WGPUShaderStage_Fragment;
            wgpuBindGroupLayoutEntries[0].texture.sampleType    = WGPUTextureSampleType_UnfilterableFloat;
            wgpuBindGroupLayoutEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;

            WGPUBindGroupLayoutDescriptor wgpuBindGroupLayoutDesc{};
            wgpuBindGroupLayoutDesc.entryCount = _countof(wgpuBindGroupLayoutEntries);
            wgpuBindGroupLayoutDesc.entries    = wgpuBindGroupLayoutEntries;
            m_wgpuBindGroupLayout              = wgpuDeviceCreateBindGroupLayout(pEnvWebGPU->GetWebGPUDevice(), &wgpuBindGroupLayoutDesc);
            VERIFY_EXPR(m_wgpuBindGroupLayout != nullptr);

            wgpuPipelineLayoutDesc.bindGroupLayoutCount = 1;
            wgpuPipelineLayoutDesc.bindGroupLayouts     = &m_wgpuBindGroupLayout;
        }

        m_wgpuPipelineLayout = wgpuDeviceCreatePipelineLayout(pEnvWebGPU->GetWebGPUDevice(), &wgpuPipelineLayoutDesc);
        VERIFY_EXPR(m_wgpuPipelineLayout != nullptr);

        if (InputAttachmentView != nullptr)
        {
            WGPUBindGroupEntry wgpuBindGroupEntry{};
            wgpuBindGroupEntry.textureView = InputAttachmentView;

            WGPUBindGroupDescriptor wgpuBindGroupDesc{};
            wgpuBindGroupDesc.layout     = m_wgpuBindGroupLayout;
            wgpuBindGroupDesc.entries    = &wgpuBindGroupEntry;
            wgpuBindGroupDesc.entryCount = 1;
            m_wgpuBindGroup              = wgpuDeviceCreateBindGroup(pEnvWebGPU->GetWebGPUDevice(), &wgpuBindGroupDesc);
            VERIFY_EXPR(m_wgpuBindGroup != nullptr);
        }

        WGPUColorTargetState wgpuColorTargetStates[1]{};
        wgpuColorTargetStates[0].format    = ConvertTexFormatToWGPUTextureFormat(pSwapChain->GetDesc().ColorBufferFormat);
        wgpuColorTargetStates[0].writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState wgpuFragmentState{};
        wgpuFragmentState.module      = m_wgpuPSModule;
        wgpuFragmentState.entryPoint  = "main";
        wgpuFragmentState.targetCount = _countof(wgpuColorTargetStates);
        wgpuFragmentState.targets     = wgpuColorTargetStates;

        WGPURenderPipelineDescriptor wgpuRenderPipelineDesc{};
        wgpuRenderPipelineDesc.layout             = m_wgpuPipelineLayout;
        wgpuRenderPipelineDesc.multisample.count  = SampleCount;
        wgpuRenderPipelineDesc.multisample.mask   = UINT32_MAX;
        wgpuRenderPipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        wgpuRenderPipelineDesc.vertex.module      = m_wgpuVSModule;
        wgpuRenderPipelineDesc.vertex.entryPoint  = "main";
        wgpuRenderPipelineDesc.fragment           = &wgpuFragmentState;
        m_wgpuRenderPipeline                      = wgpuDeviceCreateRenderPipeline(pEnvWebGPU->GetWebGPUDevice(), &wgpuRenderPipelineDesc);
        VERIFY_EXPR(m_wgpuRenderPipeline != nullptr);
    }

    ~ReferenceTriangleRenderer()
    {
        if (m_wgpuBindGroup)
            wgpuBindGroupRelease(m_wgpuBindGroup);
        if (m_wgpuPipelineLayout)
            wgpuPipelineLayoutRelease(m_wgpuPipelineLayout);
        if (m_wgpuRenderPipeline)
            wgpuRenderPipelineRelease(m_wgpuRenderPipeline);
        if (m_wgpuBindGroupLayout)
            wgpuBindGroupLayoutRelease(m_wgpuBindGroupLayout);
        if (m_wgpuPSModule)
            wgpuShaderModuleRelease(m_wgpuPSModule);
        if (m_wgpuVSModule)
            wgpuShaderModuleRelease(m_wgpuVSModule);
    }

    void Draw(WGPURenderPassEncoder wgpuRenderPassEncoder)
    {
        if (m_wgpuBindGroup != nullptr)
            wgpuRenderPassEncoderSetBindGroup(wgpuRenderPassEncoder, 0, m_wgpuBindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(wgpuRenderPassEncoder, m_wgpuRenderPipeline);
        wgpuRenderPassEncoderDraw(wgpuRenderPassEncoder, 6, 1, 0, 0);
    }

private:
    WGPUShaderModule    m_wgpuVSModule        = nullptr;
    WGPUShaderModule    m_wgpuPSModule        = nullptr;
    WGPUBindGroupLayout m_wgpuBindGroupLayout = nullptr;
    WGPUPipelineLayout  m_wgpuPipelineLayout  = nullptr;
    WGPURenderPipeline  m_wgpuRenderPipeline  = nullptr;
    WGPUBindGroup       m_wgpuBindGroup       = nullptr;
};

} // namespace

void RenderDrawCommandReferenceWebGPU(ISwapChain* pSwapChain, const float* pClearColor)
{
    auto* pEnvWebGPU              = TestingEnvironmentWebGPU::GetInstance();
    auto* pTestingSwapChainWebGPU = ClassPtrCast<TestingSwapChainWebGPU>(pSwapChain);

    ReferenceTriangleRenderer TriRender{pSwapChain};

    WGPUCommandEncoder wgpuCmdEncoder = pEnvWebGPU->CreateCommandEncoder();

    WGPURenderPassColorAttachment wgpuRenderPassColorAttachments[1]{};
    wgpuRenderPassColorAttachments[0].clearValue = ConvertArrayToWGPUColor(pClearColor);
    wgpuRenderPassColorAttachments[0].loadOp     = WGPULoadOp_Clear;
    wgpuRenderPassColorAttachments[0].storeOp    = WGPUStoreOp_Store;
    wgpuRenderPassColorAttachments[0].view       = pTestingSwapChainWebGPU->GetWebGPUColorTextureView();
    wgpuRenderPassColorAttachments[0].depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    WGPURenderPassDescriptor wgpuRenderPassDesc{};
    wgpuRenderPassDesc.colorAttachments     = wgpuRenderPassColorAttachments;
    wgpuRenderPassDesc.colorAttachmentCount = _countof(wgpuRenderPassColorAttachments);

    const Uint32 TextureWidth  = pSwapChain->GetDesc().Width;
    const Uint32 TextureHeight = pSwapChain->GetDesc().Height;

    WGPURenderPassEncoder wgpuRenderPassEncoder = wgpuCommandEncoderBeginRenderPass(wgpuCmdEncoder, &wgpuRenderPassDesc);
    wgpuRenderPassEncoderSetViewport(wgpuRenderPassEncoder, 0, 0, static_cast<float>(TextureWidth), static_cast<float>(TextureHeight), 0, 1.0);
    wgpuRenderPassEncoderSetScissorRect(wgpuRenderPassEncoder, 0, 0, TextureWidth, TextureHeight);
    TriRender.Draw(wgpuRenderPassEncoder);
    wgpuRenderPassEncoderEnd(wgpuRenderPassEncoder);
    pEnvWebGPU->SubmitCommandEncoder(wgpuCmdEncoder);

    wgpuRenderPassEncoderRelease(wgpuRenderPassEncoder);
    wgpuCommandEncoderRelease(wgpuCmdEncoder);
}

void RenderPassMSResolveReferenceWebGPU(ISwapChain* pSwapChain, const float* pClearColor)
{
    auto* pEnvWebGPU              = TestingEnvironmentWebGPU::GetInstance();
    auto* pTestingSwapChainWebGPU = ClassPtrCast<TestingSwapChainWebGPU>(pSwapChain);

    constexpr Uint32 SampleCount = 4;

    ReferenceTriangleRenderer TriRender{pSwapChain, SampleCount};

    const auto& SCDesc = pTestingSwapChainWebGPU->GetDesc();

    WGPUTextureDescriptor wgpuMSTextureDesc{};
    wgpuMSTextureDesc.dimension               = WGPUTextureDimension_2D;
    wgpuMSTextureDesc.usage                   = WGPUTextureUsage_RenderAttachment;
    wgpuMSTextureDesc.size.width              = SCDesc.Width;
    wgpuMSTextureDesc.size.height             = SCDesc.Height;
    wgpuMSTextureDesc.size.depthOrArrayLayers = 1;
    wgpuMSTextureDesc.mipLevelCount           = 1;
    wgpuMSTextureDesc.sampleCount             = SampleCount;
    wgpuMSTextureDesc.format                  = ConvertTexFormatToWGPUTextureFormat(SCDesc.ColorBufferFormat);

    WGPUTexture wgpuMSTexture = wgpuDeviceCreateTexture(pEnvWebGPU->GetWebGPUDevice(), &wgpuMSTextureDesc);
    VERIFY_EXPR(wgpuMSTexture != nullptr);

    WGPUTextureViewDescriptor wgpuMSTextureViewDesc{};
    wgpuMSTextureViewDesc.format          = ConvertTexFormatToWGPUTextureFormat(SCDesc.ColorBufferFormat);
    wgpuMSTextureViewDesc.aspect          = WGPUTextureAspect_All;
    wgpuMSTextureViewDesc.baseArrayLayer  = 0;
    wgpuMSTextureViewDesc.arrayLayerCount = 1;
    wgpuMSTextureViewDesc.baseMipLevel    = 0;
    wgpuMSTextureViewDesc.mipLevelCount   = 1;
    wgpuMSTextureViewDesc.dimension       = WGPUTextureViewDimension_2D;

    WGPUTextureView wgpuMSTextureView = wgpuTextureCreateView(wgpuMSTexture, &wgpuMSTextureViewDesc);
    VERIFY_EXPR(wgpuMSTextureView != nullptr);

    WGPUCommandEncoder wgpuCmdEncoder = pEnvWebGPU->CreateCommandEncoder();

    WGPURenderPassColorAttachment wgpuRenderPassColorAttachments[1]{};
    wgpuRenderPassColorAttachments[0].clearValue    = ConvertArrayToWGPUColor(pClearColor);
    wgpuRenderPassColorAttachments[0].loadOp        = WGPULoadOp_Clear;
    wgpuRenderPassColorAttachments[0].storeOp       = WGPUStoreOp_Store;
    wgpuRenderPassColorAttachments[0].view          = wgpuMSTextureView;
    wgpuRenderPassColorAttachments[0].resolveTarget = pTestingSwapChainWebGPU->GetWebGPUColorTextureView();
    wgpuRenderPassColorAttachments[0].depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED;

    WGPURenderPassDescriptor wgpuRenderPassDesc{};
    wgpuRenderPassDesc.colorAttachments     = wgpuRenderPassColorAttachments;
    wgpuRenderPassDesc.colorAttachmentCount = _countof(wgpuRenderPassColorAttachments);

    const Uint32 TextureWidth  = pSwapChain->GetDesc().Width;
    const Uint32 TextureHeight = pSwapChain->GetDesc().Height;

    WGPURenderPassEncoder wgpuRenderPassEncoder = wgpuCommandEncoderBeginRenderPass(wgpuCmdEncoder, &wgpuRenderPassDesc);
    wgpuRenderPassEncoderSetViewport(wgpuRenderPassEncoder, 0, 0, static_cast<float>(TextureWidth), static_cast<float>(TextureHeight), 0, 1.0);
    wgpuRenderPassEncoderSetScissorRect(wgpuRenderPassEncoder, 0, 0, TextureWidth, TextureHeight);
    TriRender.Draw(wgpuRenderPassEncoder);
    wgpuRenderPassEncoderEnd(wgpuRenderPassEncoder);
    pEnvWebGPU->SubmitCommandEncoder(wgpuCmdEncoder);

    wgpuRenderPassEncoderRelease(wgpuRenderPassEncoder);
    wgpuCommandEncoderRelease(wgpuCmdEncoder);
    wgpuTextureViewRelease(wgpuMSTextureView);
    wgpuTextureRelease(wgpuMSTexture);
}

void RenderPassInputAttachmentReferenceWebGPU(ISwapChain* pSwapChain, const float* pClearColor)
{
    auto* pEnvWebGPU              = TestingEnvironmentWebGPU::GetInstance();
    auto* pTestingSwapChainWebGPU = ClassPtrCast<TestingSwapChainWebGPU>(pSwapChain);

    const auto& SCDesc = pTestingSwapChainWebGPU->GetDesc();

    WGPUTextureDescriptor wgpuInputTextureDesc{};
    wgpuInputTextureDesc.dimension               = WGPUTextureDimension_2D;
    wgpuInputTextureDesc.usage                   = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    wgpuInputTextureDesc.size.width              = SCDesc.Width;
    wgpuInputTextureDesc.size.height             = SCDesc.Height;
    wgpuInputTextureDesc.size.depthOrArrayLayers = 1;
    wgpuInputTextureDesc.mipLevelCount           = 1;
    wgpuInputTextureDesc.sampleCount             = 1;
    wgpuInputTextureDesc.format                  = ConvertTexFormatToWGPUTextureFormat(SCDesc.ColorBufferFormat);

    WGPUTexture wgpuMSTexture = wgpuDeviceCreateTexture(pEnvWebGPU->GetWebGPUDevice(), &wgpuInputTextureDesc);
    VERIFY_EXPR(wgpuMSTexture != nullptr);

    WGPUTextureViewDescriptor wgpuInputTextureViewDesc{};
    wgpuInputTextureViewDesc.format          = ConvertTexFormatToWGPUTextureFormat(SCDesc.ColorBufferFormat);
    wgpuInputTextureViewDesc.aspect          = WGPUTextureAspect_All;
    wgpuInputTextureViewDesc.baseArrayLayer  = 0;
    wgpuInputTextureViewDesc.arrayLayerCount = 1;
    wgpuInputTextureViewDesc.baseMipLevel    = 0;
    wgpuInputTextureViewDesc.mipLevelCount   = 1;
    wgpuInputTextureViewDesc.dimension       = WGPUTextureViewDimension_2D;

    WGPUTextureView wgpuInputTextureView = wgpuTextureCreateView(wgpuMSTexture, &wgpuInputTextureViewDesc);
    VERIFY_EXPR(wgpuInputTextureView != nullptr);

    const Uint32 TextureWidth  = pSwapChain->GetDesc().Width;
    const Uint32 TextureHeight = pSwapChain->GetDesc().Height;

    WGPUCommandEncoder wgpuCmdEncoder = pEnvWebGPU->CreateCommandEncoder();

    ReferenceTriangleRenderer TriRenderInputWrite{pSwapChain, 1};
    {
        WGPURenderPassColorAttachment wgpuRenderPassColorAttachments[1]{};
        wgpuRenderPassColorAttachments[0].clearValue = ConvertArrayToWGPUColor(pClearColor);
        wgpuRenderPassColorAttachments[0].loadOp     = WGPULoadOp_Clear;
        wgpuRenderPassColorAttachments[0].storeOp    = WGPUStoreOp_Store;
        wgpuRenderPassColorAttachments[0].view       = wgpuInputTextureView;
        wgpuRenderPassColorAttachments[0].depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

        WGPURenderPassDescriptor wgpuRenderPassDesc{};
        wgpuRenderPassDesc.colorAttachments     = wgpuRenderPassColorAttachments;
        wgpuRenderPassDesc.colorAttachmentCount = _countof(wgpuRenderPassColorAttachments);

        WGPURenderPassEncoder wgpuRenderPassEncoder = wgpuCommandEncoderBeginRenderPass(wgpuCmdEncoder, &wgpuRenderPassDesc);
        wgpuRenderPassEncoderSetViewport(wgpuRenderPassEncoder, 0, 0, static_cast<float>(TextureWidth), static_cast<float>(TextureHeight), 0, 1.0);
        wgpuRenderPassEncoderSetScissorRect(wgpuRenderPassEncoder, 0, 0, TextureWidth, TextureHeight);
        TriRenderInputWrite.Draw(wgpuRenderPassEncoder);
        wgpuRenderPassEncoderEnd(wgpuRenderPassEncoder);
        wgpuRenderPassEncoderRelease(wgpuRenderPassEncoder);
    }

    ReferenceTriangleRenderer TriRenderInputRead{pSwapChain, 1, wgpuInputTextureView};
    {
        WGPURenderPassColorAttachment wgpuRenderPassColorAttachments[1]{};
        wgpuRenderPassColorAttachments[0].clearValue = ConvertArrayToWGPUColor(pClearColor);
        wgpuRenderPassColorAttachments[0].loadOp     = WGPULoadOp_Clear;
        wgpuRenderPassColorAttachments[0].storeOp    = WGPUStoreOp_Store;
        wgpuRenderPassColorAttachments[0].view       = pTestingSwapChainWebGPU->GetWebGPUColorTextureView();
        wgpuRenderPassColorAttachments[0].depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

        WGPURenderPassDescriptor wgpuRenderPassDesc{};
        wgpuRenderPassDesc.colorAttachments     = wgpuRenderPassColorAttachments;
        wgpuRenderPassDesc.colorAttachmentCount = _countof(wgpuRenderPassColorAttachments);

        WGPURenderPassEncoder wgpuRenderPassEncoder = wgpuCommandEncoderBeginRenderPass(wgpuCmdEncoder, &wgpuRenderPassDesc);
        wgpuRenderPassEncoderSetViewport(wgpuRenderPassEncoder, 0, 0, static_cast<float>(TextureWidth), static_cast<float>(TextureHeight), 0, 1.0);
        wgpuRenderPassEncoderSetScissorRect(wgpuRenderPassEncoder, 0, 0, TextureWidth, TextureHeight);
        TriRenderInputRead.Draw(wgpuRenderPassEncoder);
        wgpuRenderPassEncoderEnd(wgpuRenderPassEncoder);
        wgpuRenderPassEncoderRelease(wgpuRenderPassEncoder);
    }

    pEnvWebGPU->SubmitCommandEncoder(wgpuCmdEncoder);

    wgpuCommandEncoderRelease(wgpuCmdEncoder);
    wgpuTextureViewRelease(wgpuInputTextureView);
    wgpuTextureRelease(wgpuMSTexture);
}

} // namespace Testing

} // namespace Diligent
