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

#include "InlineShaders/ComputeShaderTestWGSL.h"

namespace Diligent
{

namespace Testing
{

void ComputeShaderReferenceWebGPU(ISwapChain* pSwapChain)
{
    auto*       pEnvWebGPU              = TestingEnvironmentWebGPU::GetInstance();
    auto*       pTestingSwapChainWebGPU = ClassPtrCast<TestingSwapChainWebGPU>(pSwapChain);
    const auto& SCDesc                  = pSwapChain->GetDesc();

    WGPUShaderModule wgpuCSModule = pEnvWebGPU->CreateShaderModule(WGSL::FillTextureCS);
    VERIFY_EXPR(wgpuCSModule != nullptr);

    WGPUBindGroupLayoutEntry wgpuBindGroupLayoutEntry{};
    wgpuBindGroupLayoutEntry.binding                      = 0;
    wgpuBindGroupLayoutEntry.visibility                   = WGPUShaderStage_Compute;
    wgpuBindGroupLayoutEntry.storageTexture.format        = WGPUTextureFormat_RGBA8Unorm;
    wgpuBindGroupLayoutEntry.storageTexture.access        = WGPUStorageTextureAccess_WriteOnly;
    wgpuBindGroupLayoutEntry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutDescriptor wgpuBindGroupLayoutDesc{};
    wgpuBindGroupLayoutDesc.entries         = &wgpuBindGroupLayoutEntry;
    wgpuBindGroupLayoutDesc.entryCount      = 1;
    WGPUBindGroupLayout wgpuBindGroupLayout = wgpuDeviceCreateBindGroupLayout(pEnvWebGPU->GetWebGPUDevice(), &wgpuBindGroupLayoutDesc);
    VERIFY_EXPR(wgpuBindGroupLayout != nullptr);

    WGPUPipelineLayoutDescriptor wgpuPipelineLayoutDesc{};
    wgpuPipelineLayoutDesc.bindGroupLayoutCount = 1;
    wgpuPipelineLayoutDesc.bindGroupLayouts     = &wgpuBindGroupLayout;
    WGPUPipelineLayout wgpuPipelineLayout       = wgpuDeviceCreatePipelineLayout(pEnvWebGPU->GetWebGPUDevice(), &wgpuPipelineLayoutDesc);
    VERIFY_EXPR(wgpuPipelineLayout != nullptr);

    WGPUComputePipelineDescriptor wgpuComputePipelineDesc{};
    wgpuComputePipelineDesc.label              = "Compute shader test (reference)";
    wgpuComputePipelineDesc.layout             = wgpuPipelineLayout;
    wgpuComputePipelineDesc.compute.module     = wgpuCSModule;
    wgpuComputePipelineDesc.compute.entryPoint = "main";
    WGPUComputePipeline wgpuComputePipeline    = wgpuDeviceCreateComputePipeline(pEnvWebGPU->GetWebGPUDevice(), &wgpuComputePipelineDesc);
    VERIFY_EXPR(wgpuComputePipeline != nullptr);

    WGPUCommandEncoder        wgpuCmdEncoder = pEnvWebGPU->CreateCommandEncoder();
    WGPUComputePassDescriptor wgpuComputePassDesc{};
    WGPUComputePassEncoder    wgpuComputePassEncoder = wgpuCommandEncoderBeginComputePass(wgpuCmdEncoder, &wgpuComputePassDesc);
    VERIFY_EXPR(wgpuComputePassEncoder != nullptr);

    WGPUBindGroupEntry wgpuBindGroupEntry{};
    wgpuBindGroupEntry.binding     = 0;
    wgpuBindGroupEntry.textureView = pTestingSwapChainWebGPU->GetWebGPUColorTextureView();

    WGPUBindGroupDescriptor wgpuBindGroupDesc{};
    wgpuBindGroupDesc.layout     = wgpuBindGroupLayout;
    wgpuBindGroupDesc.entryCount = 1;
    wgpuBindGroupDesc.entries    = &wgpuBindGroupEntry;
    WGPUBindGroup wgpuBindGroup  = wgpuDeviceCreateBindGroup(pEnvWebGPU->GetWebGPUDevice(), &wgpuBindGroupDesc);
    VERIFY_EXPR(wgpuBindGroup != nullptr);

    wgpuComputePassEncoderSetPipeline(wgpuComputePassEncoder, wgpuComputePipeline);
    wgpuComputePassEncoderSetBindGroup(wgpuComputePassEncoder, 0, wgpuBindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(wgpuComputePassEncoder, (SCDesc.Width + 15) / 16, (SCDesc.Height + 15) / 16, 1);
    wgpuComputePassEncoderEnd(wgpuComputePassEncoder);
    pEnvWebGPU->SubmitCommandEncoder(wgpuCmdEncoder);

    wgpuComputePassEncoderRelease(wgpuComputePassEncoder);
    wgpuCommandEncoderRelease(wgpuCmdEncoder);
    wgpuBindGroupRelease(wgpuBindGroup);
    wgpuComputePipelineRelease(wgpuComputePipeline);
    wgpuPipelineLayoutRelease(wgpuPipelineLayout);
    wgpuBindGroupLayoutRelease(wgpuBindGroupLayout);
    wgpuShaderModuleRelease(wgpuCSModule);
}

} // namespace Testing

} // namespace Diligent
