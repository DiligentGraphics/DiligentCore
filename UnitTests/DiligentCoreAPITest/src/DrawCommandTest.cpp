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

#include "TestingEnvironment.h"
#include "TestingSwapChainBase.h"
#include "BasicMath.h"

#include "gtest/gtest.h"

namespace Diligent
{

namespace Testing
{

#if D3D11_SUPPORTED
void RenderDrawCommandRefenceD3D11(ISwapChain* pSwapChain);
#endif

#if D3D12_SUPPORTED
void RenderDrawCommandRefenceD3D12(ISwapChain* pSwapChain);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
void RenderDrawCommandRefenceGL(ISwapChain* pSwapChain);
#endif

#if VULKAN_SUPPORTED
void RenderDrawCommandRefenceVk(ISwapChain* pSwapChain);
#endif

#if METAL_SUPPORTED

#endif

} // namespace Testing

} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

const char* ProceduralVSSource = R"(
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

const char* VSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

struct VSInput
{
    float4 Pos   : ATTRIB0;
    float3 Color : ATTRIB1; 
};

void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    PSIn.Pos   = VSIn.Pos;
    PSIn.Color = VSIn.Color;
}
)";

const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = float4(PSIn.Color.rgb, 1.0);
}
)";

struct Vertex
{
    float4 Pos;
    float3 Color;
};

// clang-format off
float4 Pos[] = 
{
    float4(-1.0, -0.5, 0.0, 1.0),
    float4(-0.5, +0.5, 0.0, 1.0),
    float4( 0.0, -0.5, 0.0, 1.0),

    float4(+0.0, -0.5, 0.0, 1.0),
    float4(+0.5, +0.5, 0.0, 1.0),
    float4(+1.0, -0.5, 0.0, 1.0)
};

float3 Color[] =
{
    float3(1.0f, 0.0f, 0.0f),
    float3(0.0f, 1.0f, 0.0f),
    float3(0.0f, 0.0f, 1.0f),
};

Vertex Vert[] = 
{
    {Pos[0], Color[0]},
    {Pos[1], Color[1]},
    {Pos[2], Color[2]},

    {Pos[3], Color[0]},
    {Pos[4], Color[1]},
    {Pos[5], Color[2]}
};
// clang-format on

class DrawCommandTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        auto* pEnv       = TestingEnvironment::GetInstance();
        auto* pDevice    = pEnv->GetDevice();
        auto* pSwapChain = pEnv->GetSwapChain();
        auto* pConext    = pEnv->GetDeviceContext();

        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
        if (pTestingSwapChain)
        {
            pConext->Flush();
            pConext->InvalidateState();

            auto deviceType = pDevice->GetDeviceCaps().DevType;
            switch (deviceType)
            {
#if D3D11_SUPPORTED
                case DeviceType::D3D11:
                    RenderDrawCommandRefenceD3D11(pSwapChain);
                    break;
#endif

#if D3D12_SUPPORTED
                case DeviceType::D3D12:
                    RenderDrawCommandRefenceD3D12(pSwapChain);
                    break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
                case DeviceType::OpenGL:
                case DeviceType::OpenGLES:
                    RenderDrawCommandRefenceGL(pSwapChain);
                    break;

#endif

#if VULKAN_SUPPORTED
                case DeviceType::Vulkan:
                    RenderDrawCommandRefenceVk(pSwapChain);
                    break;

#endif

                default:
                    LOG_ERROR_AND_THROW("Unsupported device type");
            }

            pTestingSwapChain->TakeSnapshot();
        }
        TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

        PipelineStateDesc PSODesc;
        PSODesc.Name = "Procedural triangle PSO";

        PSODesc.IsComputePipeline                             = false;
        PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
        PSODesc.GraphicsPipeline.RTVFormats[0]                = pSwapChain->GetDesc().ColorBufferFormat;
        PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.UseCombinedTextureSamplers = true;

        RefCntAutoPtr<IShader> pProceduralVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Draw command test procedural vertex shader";
            ShaderCI.Source          = ProceduralVSSource;
            pDevice->CreateShader(ShaderCI, &pProceduralVS);
            ASSERT_NE(pProceduralVS, nullptr);
        }

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Draw command test vertex shader";
            ShaderCI.Source          = VSSource;
            pDevice->CreateShader(ShaderCI, &pVS);
            ASSERT_NE(pVS, nullptr);
        }

        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Draw command test pixel shader";
            ShaderCI.Source          = PSSource;
            pDevice->CreateShader(ShaderCI, &pPS);
            ASSERT_NE(pPS, nullptr);
        }

        PSODesc.GraphicsPipeline.pVS = pProceduralVS;
        PSODesc.GraphicsPipeline.pPS = pPS;
        pDevice->CreatePipelineState(PSODesc, &sm_pDrawProceduralPSO);
        ASSERT_NE(sm_pDrawProceduralPSO, nullptr);

        InputLayoutDesc LayoutDesc;
        // clang-format off
        LayoutElement Elems[] =
        {
            LayoutElement{ 0, 0, 4, VT_FLOAT32},
            LayoutElement{ 1, 0, 3, VT_FLOAT32}
        };
        // clang-format on
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = Elems;
        PSODesc.GraphicsPipeline.InputLayout.NumElements    = _countof(Elems);
        PSODesc.GraphicsPipeline.pVS                        = pVS;
        PSODesc.GraphicsPipeline.pPS                        = pPS;
        PSODesc.GraphicsPipeline.PrimitiveTopology          = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pDevice->CreatePipelineState(PSODesc, &sm_pDrawPSO);

        Elems[0].Stride = sizeof(Vertex) * 2;
        pDevice->CreatePipelineState(PSODesc, &sm_pDraw_2xStride_PSO);
    }

    static void TearDownTestSuite()
    {
        sm_pDrawProceduralPSO.Release();

        auto* pEnv = TestingEnvironment::GetInstance();
        pEnv->Reset();
    }

    static void SetRenderTargets(IPipelineState* pPSO)
    {
        auto* pEnv       = TestingEnvironment::GetInstance();
        auto* pContext   = pEnv->GetDeviceContext();
        auto* pSwapChain = pEnv->GetSwapChain();

        ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
        pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const float ClearColor[] = {0.f, 0.f, 0.f, 0.0f};
        pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        pContext->SetPipelineState(pPSO);
        // Commit shader resources. We don't really have any resources, but this call also sets the shaders in OpenGL backend.
        pContext->CommitShaderResources(nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    static void Present()
    {
        auto* pEnv       = TestingEnvironment::GetInstance();
        auto* pSwapChain = pEnv->GetSwapChain();
        auto* pContext   = pEnv->GetDeviceContext();

        pSwapChain->Present();

        pContext->InvalidateState();
    }

    RefCntAutoPtr<IBuffer> CreateVertexBuffer(const void* VertexData, Uint32 DataSize)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Test vertex buffer";
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.uiSizeInBytes = DataSize;

        BufferData InitialData;
        InitialData.pData    = VertexData;
        InitialData.DataSize = DataSize;

        auto*                  pEnv    = TestingEnvironment::GetInstance();
        auto*                  pDevice = pEnv->GetDevice();
        RefCntAutoPtr<IBuffer> pBuffer;
        pDevice->CreateBuffer(BuffDesc, &InitialData, &pBuffer);
        VERIFY_EXPR(pBuffer);
        return pBuffer;
    }

    RefCntAutoPtr<IBuffer> CreateIndexBuffer(const Uint32* Indices, Uint32 NumIndices)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Test index buffer";
        BuffDesc.BindFlags     = BIND_INDEX_BUFFER;
        BuffDesc.uiSizeInBytes = sizeof(Uint32) * NumIndices;

        BufferData InitialData;
        InitialData.pData    = Indices;
        InitialData.DataSize = BuffDesc.uiSizeInBytes;

        auto*                  pEnv    = TestingEnvironment::GetInstance();
        auto*                  pDevice = pEnv->GetDevice();
        RefCntAutoPtr<IBuffer> pBuffer;
        pDevice->CreateBuffer(BuffDesc, &InitialData, &pBuffer);
        VERIFY_EXPR(pBuffer);
        return pBuffer;
    }

    static RefCntAutoPtr<IPipelineState> sm_pDrawProceduralPSO;
    static RefCntAutoPtr<IPipelineState> sm_pDrawPSO;
    static RefCntAutoPtr<IPipelineState> sm_pDraw_2xStride_PSO;
    static RefCntAutoPtr<IPipelineState> sm_pDrawInstancedPSO;
};

RefCntAutoPtr<IPipelineState> DrawCommandTest::sm_pDrawProceduralPSO;
RefCntAutoPtr<IPipelineState> DrawCommandTest::sm_pDrawPSO;
RefCntAutoPtr<IPipelineState> DrawCommandTest::sm_pDraw_2xStride_PSO;

TEST_F(DrawCommandTest, DrawProcedural)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDrawProceduralPSO);

    DrawAttribs drawAttrs{6, DRAW_FLAG_VERIFY_ALL};
    pContext->Draw(drawAttrs);

    Present();
}

TEST_F(DrawCommandTest, Draw)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDrawPSO);

    // clang-format off
    const Vertex Triangles[] =
    {
        Vert[0], Vert[1], Vert[2],
        Vert[3], Vert[4], Vert[5]
    };
    // clang-format on

    auto     pVB       = CreateVertexBuffer(Triangles, sizeof(Triangles));
    IBuffer* pVBs[]    = {pVB};
    Uint32   Offsets[] = {0};
    pContext->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs drawAttrs{6, DRAW_FLAG_VERIFY_ALL};
    pContext->Draw(drawAttrs);

    Present();
}

TEST_F(DrawCommandTest, Draw_StartVertex)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDrawPSO);

    // clang-format off
    const Vertex Triangles[] =
    {
        {}, {},
        Vert[0], Vert[1], Vert[2],
        Vert[3], Vert[4], Vert[5]
    };
    // clang-format on

    auto     pVB       = CreateVertexBuffer(Triangles, sizeof(Triangles));
    IBuffer* pVBs[]    = {pVB};
    Uint32   Offsets[] = {0};
    pContext->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs drawAttrs{6, DRAW_FLAG_VERIFY_ALL};
    drawAttrs.StartVertexLocation = 2;
    pContext->Draw(drawAttrs);

    Present();
}

TEST_F(DrawCommandTest, Draw_VBOffset)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDrawPSO);

    // clang-format off
    const Vertex Triangles[] =
    {
        {}, {}, {},
        Vert[0], Vert[1], Vert[2],
        Vert[3], Vert[4], Vert[5]
    };
    // clang-format on

    auto     pVB       = CreateVertexBuffer(Triangles, sizeof(Triangles));
    IBuffer* pVBs[]    = {pVB};
    Uint32   Offsets[] = {3 * sizeof(Vertex)};
    pContext->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs drawAttrs{6, DRAW_FLAG_VERIFY_ALL};
    pContext->Draw(drawAttrs);

    Present();
}

TEST_F(DrawCommandTest, Draw_StartVertex_VBOffset)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDrawPSO);

    // clang-format off
    const Vertex Triangles[] =
    {
        {}, {}, {}, // Offset
        {}, {}, // Start vertex
        Vert[0], Vert[1], Vert[2],
        Vert[3], Vert[4], Vert[5]
    };
    // clang-format on

    auto     pVB       = CreateVertexBuffer(Triangles, sizeof(Triangles));
    IBuffer* pVBs[]    = {pVB};
    Uint32   Offsets[] = {3 * sizeof(Vertex)};
    pContext->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs drawAttrs{6, DRAW_FLAG_VERIFY_ALL};
    drawAttrs.StartVertexLocation = 2;
    pContext->Draw(drawAttrs);

    Present();
}

TEST_F(DrawCommandTest, Draw_StartVertex_VBOffset_2xStride)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDraw_2xStride_PSO);

    // clang-format off
    const Vertex Triangles[] =
    {
        {}, {}, {},     // Offset
        {}, {}, {}, {}, // Start vertex
        Vert[0], {}, Vert[1], {}, Vert[2], {}, 
        Vert[3], {}, Vert[4], {}, Vert[5], {}
    };
    // clang-format on

    auto     pVB       = CreateVertexBuffer(Triangles, sizeof(Triangles));
    IBuffer* pVBs[]    = {pVB};
    Uint32   Offsets[] = {3 * sizeof(Vertex)};
    pContext->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    DrawAttribs drawAttrs{6, DRAW_FLAG_VERIFY_ALL};
    drawAttrs.StartVertexLocation = 2;
    pContext->Draw(drawAttrs);

    Present();
}

TEST_F(DrawCommandTest, DrawIndexed)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDrawPSO);

    // clang-format off
    const Vertex Triangles[] =
    {
        {}, {},
        Vert[0], {}, Vert[1], {}, {}, Vert[2],
        Vert[3], {}, {}, Vert[5], Vert[4]
    };
    Uint32 Indices[] = {2,4,7, 8,12,11};
    // clang-format on

    auto pVB = CreateVertexBuffer(Triangles, sizeof(Triangles));
    auto pIB = CreateIndexBuffer(Indices, _countof(Indices));

    IBuffer* pVBs[]    = {pVB};
    Uint32   Offsets[] = {0};
    pContext->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(pIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs{6, VT_UINT32, DRAW_FLAG_VERIFY_ALL};
    pContext->DrawIndexed(drawAttrs);

    Present();
}

TEST_F(DrawCommandTest, DrawIndexed_IBOffset)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDrawPSO);

    // clang-format off
    const Vertex Triangles[] =
    {
        {}, {},
        Vert[0], {}, Vert[1], {}, {}, Vert[2],
        Vert[3], {}, {}, Vert[5], Vert[4]
    };
    Uint32 Indices[] = {0,0,0,0, 2,4,7, 8,12,11};
    // clang-format on

    auto pVB = CreateVertexBuffer(Triangles, sizeof(Triangles));
    auto pIB = CreateIndexBuffer(Indices, _countof(Indices));

    IBuffer* pVBs[]    = {pVB};
    Uint32   Offsets[] = {0};
    pContext->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(pIB, sizeof(Uint32) * 4, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs{6, VT_UINT32, DRAW_FLAG_VERIFY_ALL};
    pContext->DrawIndexed(drawAttrs);

    Present();
}

TEST_F(DrawCommandTest, DrawIndexed_IBOffset_BaseVertex)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    SetRenderTargets(sm_pDrawPSO);

    Uint32 bv = 2;
    // clang-format off
    const Vertex Triangles[] =
    {
        {}, {},
        Vert[0], {}, Vert[1], {}, {}, Vert[2],
        Vert[3], {}, {}, Vert[5], Vert[4]
    };
    Uint32 Indices[] = {0,0,0,0, 2-bv,4-bv,7-bv, 8-bv,12-bv,11-bv};
    // clang-format on

    auto pVB = CreateVertexBuffer(Triangles, sizeof(Triangles));
    auto pIB = CreateIndexBuffer(Indices, _countof(Indices));

    IBuffer* pVBs[]    = {pVB};
    Uint32   Offsets[] = {0};
    pContext->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(pIB, sizeof(Uint32) * 4, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs drawAttrs{6, VT_UINT32, DRAW_FLAG_VERIFY_ALL};
    drawAttrs.BaseVertex = bv;
    pContext->DrawIndexed(drawAttrs);

    Present();
}

} // namespace
