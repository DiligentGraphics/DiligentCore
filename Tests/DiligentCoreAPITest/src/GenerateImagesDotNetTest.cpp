/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

#include "GPUTestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"
#include "MapHelper.hpp"
#include "BasicMath.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

struct Vertex
{
    float3 Position;
    float4 Color;
};

TEST(GenerateImagesDotNetTest, GenerateCubeTexture)
{
    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* pEnv       = GPUTestingEnvironment::GetInstance();
    auto* pDevice    = pEnv->GetDevice();
    auto* pContext   = pEnv->GetDeviceContext();
    auto* pSwapChain = pEnv->GetSwapChain();

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);

    Vertex Vertices[] = {
        {{-1, -1, -1}, {1, 0, 0, 1}},
        {{-1, +1, -1}, {0, 1, 0, 1}},
        {{+1, +1, -1}, {0, 0, 1, 1}},
        {{+1, -1, -1}, {1, 1, 1, 1}},

        {{-1, -1, +1}, {1, 1, 0, 1}},
        {{-1, +1, +1}, {0, 1, 1, 1}},
        {{+1, +1, +1}, {1, 0, 1, 1}},
        {{+1, -1, +1}, {0.2f, 0.2f, 0.2f, 1}}};

    Uint32 Indices[] = {
        2, 0, 1, 2, 3, 0,
        4, 6, 5, 4, 7, 6,
        0, 7, 4, 0, 3, 7,
        1, 0, 4, 1, 4, 5,
        1, 5, 2, 5, 6, 2,
        3, 6, 7, 3, 2, 6};

    RefCntAutoPtr<IBuffer> pVertexBuffer;
    {
        BufferDesc Desc{};
        Desc.Name      = "Cube vertex buffer";
        Desc.Usage     = USAGE_IMMUTABLE;
        Desc.BindFlags = BIND_VERTEX_BUFFER;
        Desc.Size      = sizeof(Vertices);

        BufferData Data{Vertices, sizeof(Vertices)};
        pDevice->CreateBuffer(Desc, &Data, &pVertexBuffer);
        ASSERT_NE(pVertexBuffer, nullptr);
    }

    RefCntAutoPtr<IBuffer> pIndexBuffer;
    {
        BufferDesc Desc{};
        Desc.Name      = "Cube index buffer";
        Desc.Usage     = USAGE_IMMUTABLE;
        Desc.BindFlags = BIND_INDEX_BUFFER;
        Desc.Size      = sizeof(Indices);

        BufferData Data{Indices, sizeof(Indices)};
        pDevice->CreateBuffer(Desc, &Data, &pIndexBuffer);
        ASSERT_NE(pIndexBuffer, nullptr);
    }

    RefCntAutoPtr<IBuffer> pUniformBuffer;
    {
        BufferDesc Desc{};
        Desc.Name           = "Uniform buffer";
        Desc.Usage          = USAGE_DYNAMIC;
        Desc.BindFlags      = BIND_UNIFORM_BUFFER;
        Desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        Desc.Size           = sizeof(Indices);

        pDevice->CreateBuffer(Desc, nullptr, &pUniformBuffer);
        ASSERT_NE(pUniformBuffer, nullptr);
    }

    RefCntAutoPtr<IPipelineState> pGraphicsPSO;
    {
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCreateInfo ShaderCI{};
            ShaderCI.FilePath                        = "DotNetCube.vsh";
            ShaderCI.pShaderSourceStreamFactory      = pShaderSourceFactory;
            ShaderCI.Desc.Name                       = "Cube vertex shader";
            ShaderCI.Desc.ShaderType                 = SHADER_TYPE_VERTEX;
            ShaderCI.Desc.UseCombinedTextureSamplers = true;
            ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
            pDevice->CreateShader(ShaderCI, &pVS);
            ASSERT_NE(pVS, nullptr);
        }
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCreateInfo ShaderCI{};
            ShaderCI.FilePath                        = "DotNetCube.psh";
            ShaderCI.pShaderSourceStreamFactory      = pShaderSourceFactory;
            ShaderCI.Desc.Name                       = "Cube pixel shader";
            ShaderCI.Desc.ShaderType                 = SHADER_TYPE_PIXEL;
            ShaderCI.Desc.UseCombinedTextureSamplers = true;
            ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
            pDevice->CreateShader(ShaderCI, &pPS);
            ASSERT_NE(pPS, nullptr);
        }

        LayoutElement LayoutElements[] = {
            LayoutElement{0, 0, 3, VT_FLOAT32, false},
            LayoutElement{1, 0, 4, VT_FLOAT32, false}};

        InputLayoutDesc InputLayout{};
        InputLayout.LayoutElements = LayoutElements;
        InputLayout.NumElements    = _countof(LayoutElements);

        GraphicsPipelineStateCreateInfo PipelineCI{};
        PipelineCI.PSODesc.Name                                          = "Cube Graphics PSO";
        PipelineCI.pVS                                                   = pVS;
        PipelineCI.pPS                                                   = pPS;
        PipelineCI.GraphicsPipeline.InputLayout                          = InputLayout;
        PipelineCI.GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PipelineCI.GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
        PipelineCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = false;
        PipelineCI.GraphicsPipeline.DepthStencilDesc.DepthEnable         = true;
        PipelineCI.GraphicsPipeline.NumRenderTargets                     = 1;
        PipelineCI.GraphicsPipeline.RTVFormats[0]                        = pSwapChain->GetDesc().ColorBufferFormat;
        PipelineCI.GraphicsPipeline.DSVFormat                            = pSwapChain->GetDesc().DepthBufferFormat;
        pDevice->CreateGraphicsPipelineState(PipelineCI, &pGraphicsPSO);
        ASSERT_NE(pGraphicsPSO, nullptr);

        auto* pVariable = pGraphicsPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
        ASSERT_NE(pVariable, nullptr);
        pVariable->Set(pUniformBuffer, SET_SHADER_RESOURCE_FLAG_NONE);
    }

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pGraphicsPSO->CreateShaderResourceBinding(&pSRB, true);
    ASSERT_NE(pSRB, nullptr);

    {
        auto AspectRatio = pSwapChain->GetDesc().Width / static_cast<float>(pSwapChain->GetDesc().Height);

        auto W   = float4x4::RotationY(PI_F / 4.0f) * float4x4::RotationX(-PI_F * 0.1f);
        auto V   = float4x4::Translation(0.0f, 0.0f, 5.0f);
        auto P   = float4x4::Projection(PI_F / 4.0f, AspectRatio, 0.01f, 100.0f, pDevice->GetDeviceInfo().IsGLDevice());
        auto WVP = (W * V * P).Transpose();

        MapHelper<float4x4> pData{pContext, pUniformBuffer, MAP_WRITE, MAP_FLAG_DISCARD};
        *pData = WVP;
    }

    auto*  pRTV = pSwapChain->GetCurrentBackBufferRTV();
    auto*  pDSV = pSwapChain->GetDepthBufferDSV();
    float4 ClearColor{0.35f, 0.35f, 0.35f, 0.35f};
    pContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->SetPipelineState(pGraphicsPSO);
    pContext->SetVertexBuffers(0, 1, &pVertexBuffer, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->SetIndexBuffer(pIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->DrawIndexed(DrawIndexedAttribs{_countof(Indices), VT_UINT32, DRAW_FLAG_VERIFY_ALL});
    pTestingSwapChain->DumpBackBuffer("DotNetCubeTexture");
}

} // namespace
