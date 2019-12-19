/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "D3D12/TestingEnvironmentD3D12.h"
#include "D3D12/TestingSwapChainD3D12.h"
#include "../include/DXGITypeConversions.h"
#include "../include/d3dx12_win.h"

#include "RenderDeviceD3D12.h"
#include "DeviceContextD3D12.h"

namespace Diligent
{

namespace Testing
{

static const char* VSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

void main(in  uint    VertId : SV_VertexID,
          out PSInput PSIn) 
{
    float4 Pos[6];
    Pos[0] = float4(-1.0, -0.5, 0.0, 1.0);
    Pos[1] = float4(-0.5, +0.5, 0.0, 1.0);
    Pos[2] = float4( 0.0, -0.5, 0.0, 1.0);

    Pos[3] = float4(+0.0, -0.5, 0.0, 1.0);
    Pos[4] = float4(+0.5, +0.5, 0.0, 1.0);
    Pos[5] = float4(+1.0, -0.5, 0.0, 1.0);

    float3 Col[6];
    Col[0] = float3(1.0, 0.0, 0.0);
    Col[1] = float3(0.0, 1.0, 0.0);
    Col[2] = float3(0.0, 0.0, 1.0);

    Col[3] = float3(1.0, 0.0, 0.0);
    Col[4] = float3(0.0, 1.0, 0.0);
    Col[5] = float3(0.0, 0.0, 1.0);

    PSIn.Pos   = Pos[VertId];
    PSIn.Color = Col[VertId];
}
)";

static const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

float4 main(in PSInput PSIn) : SV_Target
{
    return float4(PSIn.Color.rgb, 1.0);
}
)";

void RenderDrawCommandRefenceD3D12(ISwapChain* pSwapChain)
{
    auto* pEnv                   = TestingEnvironmentD3D12::GetInstance();
    auto* pContext               = pEnv->GetDeviceContext();
    auto* pd3d12Device           = pEnv->GetD3D12Device();
    auto* pTestingSwapChainD3D12 = ValidatedCast<TestingSwapChainD3D12>(pSwapChain);

    const auto& SCDesc = pSwapChain->GetDesc();

    CComPtr<ID3DBlob> pVSByteCode, pPSByteCode;

    auto hr = CompileD3DShader(VSSource, "main", nullptr, "vs_5_0", &pVSByteCode);
    ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile vertex shader";

    hr = CompileD3DShader(PSSource, "main", nullptr, "ps_5_0", &pPSByteCode);
    ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to compile pixel shader";


    D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};

    RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    CComPtr<ID3DBlob>            signature;
    CComPtr<ID3D12RootSignature> pd3d12RootSignature;
    D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
    pd3d12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(pd3d12RootSignature), reinterpret_cast<void**>(static_cast<ID3D12RootSignature**>(&pd3d12RootSignature)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};

    PSODesc.pRootSignature     = pd3d12RootSignature;
    PSODesc.VS.pShaderBytecode = pVSByteCode->GetBufferPointer();
    PSODesc.VS.BytecodeLength  = pVSByteCode->GetBufferSize();
    PSODesc.PS.pShaderBytecode = pPSByteCode->GetBufferPointer();
    PSODesc.PS.BytecodeLength  = pPSByteCode->GetBufferSize();

    PSODesc.BlendState        = CD3DX12_BLEND_DESC{D3D12_DEFAULT};
    PSODesc.RasterizerState   = CD3DX12_RASTERIZER_DESC{D3D12_DEFAULT};
    PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC{D3D12_DEFAULT};

    PSODesc.RasterizerState.CullMode         = D3D12_CULL_MODE_NONE;
    PSODesc.DepthStencilState.DepthEnable    = FALSE;
    PSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    PSODesc.SampleMask = 0xFFFFFFFF;

    PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PSODesc.NumRenderTargets      = 1;
    PSODesc.RTVFormats[0]         = TexFormatToDXGI_Format(SCDesc.ColorBufferFormat);
    PSODesc.SampleDesc.Count      = 1;
    PSODesc.SampleDesc.Quality    = 0;
    PSODesc.NodeMask              = 0;
    PSODesc.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE;

    CComPtr<ID3D12PipelineState> pd3d12PSO;
    hr = pd3d12Device->CreateGraphicsPipelineState(&PSODesc, __uuidof(pd3d12PSO), reinterpret_cast<void**>(static_cast<ID3D12PipelineState**>(&pd3d12PSO)));
    ASSERT_HRESULT_SUCCEEDED(hr) << "Failed to create pipeline state";

    auto pCmdList = pEnv->CreateGraphicsCommandList();
    pTestingSwapChainD3D12->TransitionRenderTarget(pCmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_VIEWPORT d3d12VP = {};
    d3d12VP.Width          = static_cast<float>(SCDesc.Width);
    d3d12VP.Height         = static_cast<float>(SCDesc.Height);
    d3d12VP.MaxDepth       = 1;
    pCmdList->RSSetViewports(1, &d3d12VP);
    D3D12_RECT Rect = {0, 0, static_cast<LONG>(SCDesc.Width), static_cast<LONG>(SCDesc.Height)};
    pCmdList->RSSetScissorRects(1, &Rect);

    auto RTVDesriptorHandle = pTestingSwapChainD3D12->GetRTVDescriptorHandle();

    pCmdList->OMSetRenderTargets(1, &RTVDesriptorHandle, FALSE, nullptr);

    float ClearColor[] = {0, 0, 0, 0};
    pCmdList->ClearRenderTargetView(RTVDesriptorHandle, ClearColor, 0, nullptr);

    pCmdList->SetPipelineState(pd3d12PSO);
    pCmdList->SetGraphicsRootSignature(pd3d12RootSignature);
    pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCmdList->DrawInstanced(6, 1, 0, 0);

    pCmdList->Close();
    ID3D12CommandList* pCmdLits[] = {pCmdList};

    RefCntAutoPtr<IDeviceContextD3D12> pContextD3D12{pContext, IID_DeviceContextD3D12};

    auto* pQeueD3D12  = pContextD3D12->LockCommandQueue();
    auto* pd3d12Queue = pQeueD3D12->GetD3D12CommandQueue();

    pd3d12Queue->ExecuteCommandLists(_countof(pCmdLits), pCmdLits);
    pEnv->IdleCommandQueue(pd3d12Queue);

    pContextD3D12->UnlockCommandQueue();
}

} // namespace Testing

} // namespace Diligent
