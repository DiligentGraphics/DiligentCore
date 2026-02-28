/*
 *  Copyright 2026 Diligent Graphics LLC
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

// Positive / functional tests for specialization constants.
// Verifies that specialization constant values affect shader output.
//
// PSO creation failure tests (validation of invalid SpecializationConstant entries)
// live in ObjectCreationFailure/PSOCreationFailureTest.cpp.

#include "GPUTestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"
#include "GraphicsAccessories.hpp"
#include "FastRand.hpp"

#include "gtest/gtest.h"

namespace Diligent
{
namespace Testing
{
void RenderDrawCommandReference(ISwapChain* pSwapChain, const float* pClearColor = nullptr);
void ComputeShaderReference(ISwapChain* pSwapChain);
} // namespace Testing
} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

// Spec-const shader: same gradient as FillTextureCS, but channel multipliers
// come from specialization constants.
// Reference output: vec4(vec2(xy % 256) / 256.0, 0.0, 1.0)
// Base color has non-zero B so that sc_MulB is not optimized away.
// To match: sc_MulR=1.0, sc_MulG=1.0, sc_MulB=0.0
static constexpr char g_SpecConstComputeCS_GLSL[] = R"(
    #version 450
    layout(constant_id = 0) const float sc_MulR = -1.0;
    layout(constant_id = 1) const float sc_MulG = -1.0;
    layout(constant_id = 2) const float sc_MulB = -1.0;

    layout(rgba8, binding = 0) writeonly uniform image2D g_tex2DUAV;

    layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
    void main()
    {
        ivec2 dim   = imageSize(g_tex2DUAV);
        ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
        if (coord.x >= dim.x || coord.y >= dim.y)
            return;
        vec2 uv = vec2(gl_GlobalInvocationID.xy % 256u) / 256.0;
        // Base color has non-zero B channel so the compiler cannot
        // eliminate sc_MulB as a dead specialization constant.
        vec4 Color = vec4(uv.x * sc_MulR,
                          uv.y * sc_MulG,
                          uv.x * sc_MulB,
                          1.0);
        imageStore(g_tex2DUAV, coord, Color);
    }
)";

static constexpr char g_SpecConstComputeCS_WGSL[] = R"(
    override sc_MulR: f32 = -1.0;
    override sc_MulG: f32 = -1.0;
    override sc_MulB: f32 = -1.0;

    @group(0) @binding(0) var g_tex2DUAV: texture_storage_2d<rgba8unorm, write>;

    @compute @workgroup_size(16, 16, 1)
    fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
        let dim = textureDimensions(g_tex2DUAV);
        let coord = vec2<i32>(gid.xy);
        if (coord.x >= i32(dim.x) || coord.y >= i32(dim.y)) {
            return;
        }
        let uv = vec2<f32>(gid.xy % 256u) / 256.0;
        let Color = vec4<f32>(uv.x * sc_MulR,
                              uv.y * sc_MulG,
                              uv.x * sc_MulB,
                              1.0);
        textureStore(g_tex2DUAV, coord, Color);
    }
)";

// Vertex shader: hardcoded positions (same as DrawTest_ProceduralTriangleVS),
// per-vertex colors supplied via specialization constants (9 floats).
static constexpr char g_SpecConstGraphicsVS_GLSL[] = R"(
    #version 450

    #ifndef GL_ES
    out gl_PerVertex { vec4 gl_Position; };
    #endif

    // Per-vertex colors as specialization constants (3 colors x RGB).
    layout(constant_id = 0) const float sc_Col0_R = 0.0;
    layout(constant_id = 1) const float sc_Col0_G = 0.0;
    layout(constant_id = 2) const float sc_Col0_B = 0.0;

    layout(constant_id = 3) const float sc_Col1_R = 0.0;
    layout(constant_id = 4) const float sc_Col1_G = 0.0;
    layout(constant_id = 5) const float sc_Col1_B = 0.0;

    layout(constant_id = 6) const float sc_Col2_R = 0.0;
    layout(constant_id = 7) const float sc_Col2_G = 0.0;
    layout(constant_id = 8) const float sc_Col2_B = 0.0;

    layout(location = 0) out vec3 out_Color;

    void main()
    {
        vec4 Pos[6];
        Pos[0] = vec4(-1.0, -0.5, 0.0, 1.0);
        Pos[1] = vec4(-0.5, +0.5, 0.0, 1.0);
        Pos[2] = vec4( 0.0, -0.5, 0.0, 1.0);

        Pos[3] = vec4(+0.0, -0.5, 0.0, 1.0);
        Pos[4] = vec4(+0.5, +0.5, 0.0, 1.0);
        Pos[5] = vec4(+1.0, -0.5, 0.0, 1.0);

        vec3 Col[3];
        Col[0] = vec3(sc_Col0_R, sc_Col0_G, sc_Col0_B);
        Col[1] = vec3(sc_Col1_R, sc_Col1_G, sc_Col1_B);
        Col[2] = vec3(sc_Col2_R, sc_Col2_G, sc_Col2_B);

        gl_Position = Pos[gl_VertexIndex];
        out_Color   = Col[gl_VertexIndex % 3];
    }
)";

static constexpr char g_SpecConstGraphicsVS_WGSL[] = R"(
    override sc_Col0_R: f32 = 0.0;
    override sc_Col0_G: f32 = 0.0;
    override sc_Col0_B: f32 = 0.0;

    override sc_Col1_R: f32 = 0.0;
    override sc_Col1_G: f32 = 0.0;
    override sc_Col1_B: f32 = 0.0;

    override sc_Col2_R: f32 = 0.0;
    override sc_Col2_G: f32 = 0.0;
    override sc_Col2_B: f32 = 0.0;

    struct VSOutput {
        @builtin(position) Position: vec4<f32>,
        @location(0) Color: vec3<f32>,
    };

    @vertex
    fn main(@builtin(vertex_index) VertexIndex: u32) -> VSOutput {
        var Pos: array<vec4<f32>, 6>;
        Pos[0] = vec4<f32>(-1.0, -0.5, 0.0, 1.0);
        Pos[1] = vec4<f32>(-0.5,  0.5, 0.0, 1.0);
        Pos[2] = vec4<f32>( 0.0, -0.5, 0.0, 1.0);

        Pos[3] = vec4<f32>( 0.0, -0.5, 0.0, 1.0);
        Pos[4] = vec4<f32>( 0.5,  0.5, 0.0, 1.0);
        Pos[5] = vec4<f32>( 1.0, -0.5, 0.0, 1.0);

        var Col: array<vec3<f32>, 3>;
        Col[0] = vec3<f32>(sc_Col0_R, sc_Col0_G, sc_Col0_B);
        Col[1] = vec3<f32>(sc_Col1_R, sc_Col1_G, sc_Col1_B);
        Col[2] = vec3<f32>(sc_Col2_R, sc_Col2_G, sc_Col2_B);

        var output: VSOutput;
        output.Position = Pos[VertexIndex];
        output.Color = Col[VertexIndex % 3];
        return output;
    }
)";

// Fragment shader: interpolated color modulated by specialization constants.
// sc_Col0_R is shared with the vertex shader (tests cross-stage matching).
// sc_Brightness and sc_AlphaScale are PS-only.
// To match reference: sc_Col0_R = 1.0, sc_Brightness = 1.0, sc_AlphaScale = 1.0
static constexpr char g_SpecConstGraphicsPS_GLSL[] = R"(
    #version 450

    // Shared with VS (same name, different constant_id in this module).
    layout(constant_id = 0) const float sc_Col0_R     = -1.0;
    // PS-only constants.
    layout(constant_id = 1) const float sc_Brightness = -1.0;
    layout(constant_id = 2) const float sc_AlphaScale = -1.0;

    layout(location = 0) in  vec3 in_Color;
    layout(location = 0) out vec4 out_Color;

    void main()
    {
        out_Color = vec4(vec3(in_Color.r * sc_Col0_R, in_Color.gb) * sc_Brightness,
                         sc_AlphaScale);
    }
)";

static constexpr char g_SpecConstGraphicsPS_WGSL[] = R"(
    override sc_Col0_R: f32 = -1.0;
    override sc_Brightness: f32 = -1.0;
    override sc_AlphaScale: f32 = -1.0;

    @fragment
    fn main(@location(0) in_Color: vec3<f32>) -> @location(0) vec4<f32> {
        return vec4<f32>(vec3<f32>(in_Color.r * sc_Col0_R, in_Color.g, in_Color.b) * sc_Brightness,
                         sc_AlphaScale);
    }
)";


class SpecializationConstants : public ::testing::Test
{
protected:
    static void TearDownTestSuite()
    {
        GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
        pEnv->Reset();
    }

    static void Present()
    {
        GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
        ISwapChain*            pSwapChain = pEnv->GetSwapChain();

        pSwapChain->Present();
    }

    static FastRandFloat sm_Rnd;
};

FastRandFloat SpecializationConstants::sm_Rnd{0, 0.f, 1.f};


// ---------------------------------------------------------------------------
// Compute path: fill the swap chain back buffer via specialization constants.
// Uses ComputeShaderReference for the reference snapshot, just like
// InlineConstantsTest::ComputeResourceLayout.
// ---------------------------------------------------------------------------

TEST_F(SpecializationConstants, ComputePath)
{
    GPUTestingEnvironment*  pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*          pDevice    = pEnv->GetDevice();
    IDeviceContext*         pContext   = pEnv->GetDeviceContext();
    ISwapChain*             pSwapChain = pEnv->GetSwapChain();
    const RenderDeviceInfo& DeviceInfo = pDevice->GetDeviceInfo();

    if (DeviceInfo.Features.SpecializationConstants != DEVICE_FEATURE_STATE_ENABLED)
        GTEST_SKIP() << "Specialization constants are not supported by this device";
    if (!DeviceInfo.Features.ComputeShaders)
        GTEST_SKIP() << "Compute shaders are not supported by this device";

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "SpecializationConstants compute test requires testing swap chain";
    }

    const SwapChainDesc& SCDesc = pSwapChain->GetDesc();

    // --- Reference pass: native-API compute dispatch + TakeSnapshot ---
    ComputeShaderReference(pSwapChain);

    // --- Spec-const pass: same gradient, channel multipliers via specialization constants ---
    {
        ShaderCreateInfo ShaderCI;
        ShaderCI.Desc       = {"SpecConst Compute CS", SHADER_TYPE_COMPUTE, true};
        ShaderCI.EntryPoint = "main";

        if (DeviceInfo.IsWebGPUDevice())
        {
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_WGSL;
            ShaderCI.Source         = g_SpecConstComputeCS_WGSL;
        }
        else
        {
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
            ShaderCI.Source         = g_SpecConstComputeCS_GLSL;
        }

        RefCntAutoPtr<IShader> pCS;
        pDevice->CreateShader(ShaderCI, &pCS);
        ASSERT_NE(pCS, nullptr);

        // Multipliers that reproduce the reference output:
        //   R channel = x-gradient * 1.0
        //   G channel = y-gradient * 1.0
        //   B channel = 0.0 * 0.0 = 0.0
        const float            MulR         = 1.0f;
        const float            MulG         = 1.0f;
        const float            MulB         = 0.0f;
        SpecializationConstant SpecConsts[] = {
            {"sc_MulR", SHADER_TYPE_COMPUTE, sizeof(float), &MulR},
            {"sc_MulG", SHADER_TYPE_COMPUTE, sizeof(float), &MulG},
            {"sc_MulB", SHADER_TYPE_COMPUTE, sizeof(float), &MulB},
        };

        ComputePipelineStateCreateInfo PsoCI;
        PsoCI.PSODesc.Name               = "SpecConst Compute Test";
        PsoCI.PSODesc.PipelineType       = PIPELINE_TYPE_COMPUTE;
        PsoCI.pCS                        = pCS;
        PsoCI.NumSpecializationConstants = _countof(SpecConsts);
        PsoCI.pSpecializationConstants   = SpecConsts;

        RefCntAutoPtr<IPipelineState> pPSO;
        pDevice->CreateComputePipelineState(PsoCI, &pPSO);
        ASSERT_NE(pPSO, nullptr);

        pPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "g_tex2DUAV")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

        RefCntAutoPtr<IShaderResourceBinding> pSRB;
        pPSO->CreateShaderResourceBinding(&pSRB, true);
        ASSERT_NE(pSRB, nullptr);

        pContext->SetPipelineState(pPSO);
        pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DispatchComputeAttribs DispatchAttribs;
        DispatchAttribs.ThreadGroupCountX = (SCDesc.Width + 15) / 16;
        DispatchAttribs.ThreadGroupCountY = (SCDesc.Height + 15) / 16;
        pContext->DispatchCompute(DispatchAttribs);
    }

    Present();
}


// ---------------------------------------------------------------------------
// Graphics path: draw two triangles with per-vertex colors from spec constants.
// Uses RenderDrawCommandReference for the reference snapshot, just like
// DrawCommandTest and InlineConstantsTest.
// ---------------------------------------------------------------------------

TEST_F(SpecializationConstants, GraphicsPath)
{
    GPUTestingEnvironment*  pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*          pDevice    = pEnv->GetDevice();
    IDeviceContext*         pContext   = pEnv->GetDeviceContext();
    ISwapChain*             pSwapChain = pEnv->GetSwapChain();
    const RenderDeviceInfo& DeviceInfo = pDevice->GetDeviceInfo();

    if (DeviceInfo.Features.SpecializationConstants != DEVICE_FEATURE_STATE_ENABLED)
        GTEST_SKIP() << "Specialization constants are not supported by this device";

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "SpecializationConstants graphics test requires testing swap chain";
    }

    const SwapChainDesc& SCDesc = pSwapChain->GetDesc();

    // --- Reference pass: native-API two-triangle draw + TakeSnapshot ---
    const float ClearColor[] = {sm_Rnd(), sm_Rnd(), sm_Rnd(), sm_Rnd()};
    RenderDrawCommandReference(pSwapChain, ClearColor);

    // --- Spec-const pass: same two triangles, colors via specialization constants ---
    {
        ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
        pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        ShaderCreateInfo ShaderCI;

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc       = {"SpecConst Graphics VS", SHADER_TYPE_VERTEX, true};
            ShaderCI.EntryPoint = "main";
            if (DeviceInfo.IsWebGPUDevice())
            {
                ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_WGSL;
                ShaderCI.Source         = g_SpecConstGraphicsVS_WGSL;
            }
            else
            {
                ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
                ShaderCI.Source         = g_SpecConstGraphicsVS_GLSL;
            }
            pDevice->CreateShader(ShaderCI, &pVS);
            ASSERT_NE(pVS, nullptr);
        }

        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc       = {"SpecConst Graphics PS", SHADER_TYPE_PIXEL, true};
            ShaderCI.EntryPoint = "main";
            if (DeviceInfo.IsWebGPUDevice())
            {
                ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_WGSL;
                ShaderCI.Source         = g_SpecConstGraphicsPS_WGSL;
            }
            else
            {
                ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
                ShaderCI.Source         = g_SpecConstGraphicsPS_GLSL;
            }
            pDevice->CreateShader(ShaderCI, &pPS);
            ASSERT_NE(pPS, nullptr);
        }

        // Same per-vertex colors as DrawTest_ProceduralTriangleVS:
        //   Col[0] = (1, 0, 0)  Col[1] = (0, 1, 0)  Col[2] = (0, 0, 1)
        const float3 Col0{1.0f, 0.0f, 0.0f};
        const float3 Col1{0.0f, 1.0f, 0.0f};
        const float3 Col2{0.0f, 0.0f, 1.0f};

        // PS-only constants
        const float Brightness = 1.0f;
        const float AlphaScale = 1.0f;

        // clang-format off
        SpecializationConstant SpecConsts[] = {
            // sc_Col0_R is declared in both VS and PS: test cross-stage matching.
            {"sc_Col0_R", SHADER_TYPE_VS_PS, sizeof(float), &Col0.r}, // Used in both VS and PS
            {"sc_Col0_G", SHADER_TYPE_VS_PS, sizeof(float), &Col0.g}, // Used in VS only
            {"sc_Col0_B", SHADER_TYPE_VS_PS, sizeof(float), &Col0.b}, // Used in VS only
            {"sc_Col1_R", SHADER_TYPE_VERTEX, sizeof(float), &Col1.r},
            {"sc_Col1_G", SHADER_TYPE_VERTEX, sizeof(float), &Col1.g},
            {"sc_Col1_B", SHADER_TYPE_VERTEX, sizeof(float), &Col1.b},
            {"sc_Col2_R", SHADER_TYPE_VERTEX, sizeof(float), &Col2.r},
            {"sc_Col2_G", SHADER_TYPE_VERTEX, sizeof(float), &Col2.g},
            {"sc_Col2_B", SHADER_TYPE_VERTEX, sizeof(float), &Col2.b},
            // PS-only constants
            {"sc_Brightness", SHADER_TYPE_PIXEL, sizeof(float), &Brightness},
            {"sc_AlphaScale", SHADER_TYPE_PIXEL, sizeof(float), &AlphaScale},
        };
        // clang-format on

        GraphicsPipelineStateCreateInfo PsoCI;
        PsoCI.PSODesc.Name                                  = "SpecConst Graphics Test";
        PsoCI.pVS                                           = pVS;
        PsoCI.pPS                                           = pPS;
        PsoCI.GraphicsPipeline.NumRenderTargets             = 1;
        PsoCI.GraphicsPipeline.RTVFormats[0]                = SCDesc.ColorBufferFormat;
        PsoCI.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PsoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
        PsoCI.NumSpecializationConstants                    = _countof(SpecConsts);
        PsoCI.pSpecializationConstants                      = SpecConsts;

        RefCntAutoPtr<IPipelineState> pPSO;
        pDevice->CreateGraphicsPipelineState(PsoCI, &pPSO);
        ASSERT_NE(pPSO, nullptr);

        RefCntAutoPtr<IShaderResourceBinding> pSRB;
        pPSO->CreateShaderResourceBinding(&pSRB, true);
        ASSERT_NE(pSRB, nullptr);

        pContext->SetPipelineState(pPSO);
        pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawAttribs drawAttribs{6, DRAW_FLAG_VERIFY_ALL};
        pContext->Draw(drawAttribs);
    }

    Present();
}


} // namespace
