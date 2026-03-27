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

#include <cstring>
#include <string>

#include "GPUTestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"
#include "GraphicsAccessories.hpp"
#include "FastRand.hpp"
#include "RenderStateCache.h"
#include "RenderStateCache.hpp"

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
static constexpr char ComputeCS_GLSL[] = R"(
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

static constexpr char ComputeCS_WGSL[] = R"(
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
static constexpr char GraphicsVS_GLSL[] = R"(
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

static constexpr char GraphicsVS_WGSL[] = R"(
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
static constexpr char GraphicsPS_GLSL[] = R"(
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

static constexpr char GraphicsPS_WGSL[] = R"(
    override sc_Col0_R: f32 = -1.0;
    override sc_Brightness: f32 = -1.0;
    override sc_AlphaScale: f32 = -1.0;

    @fragment
    fn main(@location(0) in_Color: vec3<f32>) -> @location(0) vec4<f32> {
        return vec4<f32>(vec3<f32>(in_Color.r * sc_Col0_R, in_Color.g, in_Color.b) * sc_Brightness,
                         sc_AlphaScale);
    }
)";

static constexpr Uint32 ContentVersion = 987;

struct SpecConstRefAttribs
{
    const Char* Name;
    SHADER_TYPE Stage;
    float       RefValue;
};

// clang-format off
static constexpr SpecConstRefAttribs g_SpecConstRefDescs[] = {
    {"sc_Col0_R",     SHADER_TYPE_VS_PS,   1.0f},
    {"sc_Col0_G",     SHADER_TYPE_VERTEX,  0.0f},
    {"sc_Col0_B",     SHADER_TYPE_VERTEX,  0.0f},
    {"sc_Col1_R",     SHADER_TYPE_VERTEX,  0.0f},
    {"sc_Col1_G",     SHADER_TYPE_VERTEX,  1.0f},
    {"sc_Col1_B",     SHADER_TYPE_VERTEX,  0.0f},
    {"sc_Col2_R",     SHADER_TYPE_VERTEX,  0.0f},
    {"sc_Col2_G",     SHADER_TYPE_VERTEX,  0.0f},
    {"sc_Col2_B",     SHADER_TYPE_VERTEX,  1.0f},
    {"sc_Brightness", SHADER_TYPE_PIXEL,   1.0f},
    {"sc_AlphaScale", SHADER_TYPE_PIXEL,   1.0f},
};
// clang-format on

static constexpr Uint32 g_SpecConstRefDescCount = _countof(g_SpecConstRefDescs);

void InitializeSpecConsts(const SpecConstRefAttribs* pRefDescs,
                          Uint32                     RefDescCount,
                          SpecializationConstant*    pSpecConsts)
{
    for (Uint32 i = 0; i < RefDescCount; ++i)
    {
        pSpecConsts[i].Name         = pRefDescs[i].Name;
        pSpecConsts[i].ShaderStages = pRefDescs[i].Stage;
        pSpecConsts[i].pData        = &pRefDescs[i].RefValue;
        pSpecConsts[i].Size         = sizeof(pRefDescs[i].RefValue);
    }
}

RefCntAutoPtr<IRenderStateCache> CreateCache(IRenderDevice* pDevice, bool HotReload, IDataBlob* pCacheData = nullptr)
{
    RenderStateCacheCreateInfo CacheCI{
        pDevice,
        GPUTestingEnvironment::GetInstance()->GetArchiverFactory(),
        RENDER_STATE_CACHE_LOG_LEVEL_VERBOSE,
        RENDER_STATE_CACHE_FILE_HASH_MODE_BY_CONTENT,
        HotReload,
        /*OptimizeGLShaders=*/true,
    };

    RefCntAutoPtr<IRenderStateCache> pCache;
    CreateRenderStateCache(CacheCI, &pCache);

    if (pCacheData != nullptr)
        pCache->Load(pCacheData, ContentVersion);

    return pCache;
}

void CreateShader(IRenderStateCache*      pCache,
                  const ShaderCreateInfo& ShaderCI,
                  bool                    PresentInCache,
                  RefCntAutoPtr<IShader>& pShader)
{
    GPUTestingEnvironment* const pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice* const         pDevice = pEnv->GetDevice();

    if (pCache != nullptr)
    {
        EXPECT_EQ(pCache->CreateShader(ShaderCI, &pShader), PresentInCache);
    }
    else
    {
        pDevice->CreateShader(ShaderCI, &pShader);
        EXPECT_EQ(PresentInCache, false);
    }
    ASSERT_TRUE(pShader);
}

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
            ShaderCI.Source         = ComputeCS_WGSL;
        }
        else
        {
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
            ShaderCI.Source         = ComputeCS_GLSL;
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
                ShaderCI.Source         = GraphicsVS_WGSL;
            }
            else
            {
                ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
                ShaderCI.Source         = GraphicsVS_GLSL;
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
                ShaderCI.Source         = GraphicsPS_WGSL;
            }
            else
            {
                ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
                ShaderCI.Source         = GraphicsPS_GLSL;
            }
            pDevice->CreateShader(ShaderCI, &pPS);
            ASSERT_NE(pPS, nullptr);
        }

        SpecializationConstant SpecConsts[g_SpecConstRefDescCount];
        InitializeSpecConsts(g_SpecConstRefDescs, g_SpecConstRefDescCount, SpecConsts);

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

void CreateShaders(IRenderStateCache*      pCache,
                   bool                    PresentInCache,
                   bool                    CompileAsync,
                   RefCntAutoPtr<IShader>& pVS,
                   RefCntAutoPtr<IShader>& pPS)
{
    GPUTestingEnvironment* const pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice* const         pDevice    = pEnv->GetDevice();
    const RenderDeviceInfo&      DeviceInfo = pDevice->GetDeviceInfo();

    ShaderCreateInfo ShaderCI;
    ShaderCI.CompileFlags = CompileAsync ? SHADER_COMPILE_FLAG_ASYNCHRONOUS : SHADER_COMPILE_FLAG_NONE;

    {
        ShaderCI.Desc       = {"SpecConsts Cache VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "main";
        if (DeviceInfo.IsWebGPUDevice())
        {
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_WGSL;
            ShaderCI.Source         = GraphicsVS_WGSL;
        }
        else
        {
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
            ShaderCI.Source         = GraphicsVS_GLSL;
        }
        CreateShader(pCache, ShaderCI, PresentInCache, pVS);
        ASSERT_TRUE(pVS);
    }

    {
        ShaderCI.Desc       = {"SpecConsts Cache PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        if (DeviceInfo.IsWebGPUDevice())
        {
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_WGSL;
            ShaderCI.Source         = GraphicsPS_WGSL;
        }
        else
        {
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
            ShaderCI.Source         = GraphicsPS_GLSL;
        }
        CreateShader(pCache, ShaderCI, PresentInCache, pPS);
        ASSERT_TRUE(pPS);
    }
}

void CreateGraphicsPSO(IRenderStateCache*             pCache,
                       bool                           PresentInCache,
                       bool                           CompileAsync,
                       const Char*                    Name,
                       IShader*                       pVS,
                       IShader*                       pPS,
                       const SpecializationConstant*  pSpecConsts,
                       Uint32                         NumSpecConsts,
                       RefCntAutoPtr<IPipelineState>& pPSO,
                       bool*                          pFoundInCache = nullptr)
{
    GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice    = pEnv->GetDevice();
    ISwapChain*            pSwapChain = pEnv->GetSwapChain();

    GraphicsPipelineStateCreateInfo PsoCI;
    PsoCI.PSODesc.Name = Name;
    PsoCI.Flags        = CompileAsync ? PSO_CREATE_FLAG_ASYNCHRONOUS : PSO_CREATE_FLAG_NONE;

    PsoCI.pVS = pVS;
    PsoCI.pPS = pPS;

    PsoCI.GraphicsPipeline.NumRenderTargets             = 1;
    PsoCI.GraphicsPipeline.RTVFormats[0]                = pSwapChain->GetDesc().ColorBufferFormat;
    PsoCI.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PsoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    PsoCI.pSpecializationConstants   = pSpecConsts;
    PsoCI.NumSpecializationConstants = NumSpecConsts;

    if (pCache != nullptr)
    {
        bool PSOFound = pCache->CreateGraphicsPipelineState(PsoCI, &pPSO);
        if (!CompileAsync)
            EXPECT_EQ(PSOFound, PresentInCache);
        if (pFoundInCache != nullptr)
            *pFoundInCache = PSOFound;
    }
    else
    {
        EXPECT_FALSE(PresentInCache);
        pDevice->CreateGraphicsPipelineState(PsoCI, &pPSO);
    }
    ASSERT_NE(pPSO, nullptr);
}

void VerifyPSO(IPipelineState* pPSO)
{
    GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx       = pEnv->GetDeviceContext();
    ISwapChain*            pSwapChain = pEnv->GetSwapChain();

    static FastRandFloat rnd{2, 0, 1};
    const float          ClearColor[] = {rnd(), rnd(), rnd(), rnd()};
    RenderDrawCommandReference(pSwapChain, ClearColor);

    ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pCtx->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pCtx->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pCtx->SetPipelineState(pPSO);
    pCtx->Draw({6, DRAW_FLAG_VERIFY_ALL});

    pSwapChain->Present();
}

void TestCaches(bool CompileAsync)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();

    if (pDevice->GetDeviceInfo().Features.SpecializationConstants != DEVICE_FEATURE_STATE_ENABLED)
    {
        GTEST_SKIP() << "Specialization constants are not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset AutoReset;

    SpecializationConstant SpecConsts[g_SpecConstRefDescCount];
    InitializeSpecConsts(g_SpecConstRefDescs, g_SpecConstRefDescCount, SpecConsts);

    RefCntAutoPtr<IShader> pUncachedVS, pUncachedPS;
    CreateShaders(nullptr, false, false, pUncachedVS, pUncachedPS);
    ASSERT_NE(pUncachedVS, nullptr);
    ASSERT_NE(pUncachedPS, nullptr);

    RefCntAutoPtr<IPipelineState> pRefPSO;
    CreateGraphicsPSO(nullptr, false, false, "SpecConsts Cache Test", pUncachedVS, pUncachedPS, SpecConsts, _countof(SpecConsts), pRefPSO);
    ASSERT_NE(pRefPSO, nullptr);
    ASSERT_EQ(pRefPSO->GetStatus(), PIPELINE_STATE_STATUS_READY);

    RefCntAutoPtr<IDataBlob> pData;
    for (Uint32 pass = 0; pass < 3; ++pass)
    {
        // 0: empty cache
        // 1: loaded cache
        // 2: reloaded cache (loaded -> stored -> loaded)

        RefCntAutoPtr<IRenderStateCache> pCache = CreateCache(pDevice, /*HotReload=*/false, pData);
        ASSERT_TRUE(pCache);

        RefCntAutoPtr<IShader> pVS, pPS;
        CreateShaders(pCache, pData != nullptr, CompileAsync, pVS, pPS);
        ASSERT_NE(pVS, nullptr);
        ASSERT_NE(pPS, nullptr);

        if (CompileAsync && pass == 0)
        {
            std::vector<std::string>            MutableNames(g_SpecConstRefDescCount);
            std::vector<float>                  MutableValues(g_SpecConstRefDescCount);
            std::vector<SpecializationConstant> MutableSpecConsts(g_SpecConstRefDescCount);

            for (Uint32 i = 0; i < g_SpecConstRefDescCount; ++i)
            {
                MutableNames[i]                   = g_SpecConstRefDescs[i].Name;
                MutableValues[i]                  = g_SpecConstRefDescs[i].RefValue;
                MutableSpecConsts[i].Name         = MutableNames[i].c_str();
                MutableSpecConsts[i].ShaderStages = g_SpecConstRefDescs[i].Stage;
                MutableSpecConsts[i].pData        = &MutableValues[i];
                MutableSpecConsts[i].Size         = sizeof(MutableValues[i]);
            }

            RefCntAutoPtr<IPipelineState> pMutablePSO;
            CreateGraphicsPSO(pCache, false, CompileAsync, "SpecConsts Cache Mutable Test", pVS, pPS, MutableSpecConsts.data(), g_SpecConstRefDescCount, pMutablePSO);
            ASSERT_NE(pMutablePSO, nullptr);

            // Make sure the strings and data are properly copied in CreateGraphicsPSO.
            MutableNames.clear();
            MutableValues.clear();
            MutableSpecConsts.clear();

            ASSERT_EQ(pMutablePSO->GetStatus(true), PIPELINE_STATE_STATUS_READY);
            VerifyPSO(pMutablePSO);
        }

        RefCntAutoPtr<IPipelineState> pPSO;
        CreateGraphicsPSO(pCache, pData != nullptr, CompileAsync, "SpecConsts Cache Test", pVS, pPS, SpecConsts, _countof(SpecConsts), pPSO);
        ASSERT_NE(pPSO, nullptr);
        ASSERT_EQ(pPSO->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);
        EXPECT_TRUE(pRefPSO->IsCompatibleWith(pPSO));
        EXPECT_TRUE(pPSO->IsCompatibleWith(pRefPSO));

        VerifyPSO(pPSO);

        {
            RefCntAutoPtr<IPipelineState> pPSO2;
            bool                          PSOFound2 = false;
            CreateGraphicsPSO(pCache, true, CompileAsync, "SpecConsts Cache Test", pVS, pPS, SpecConsts, _countof(SpecConsts), pPSO2, &PSOFound2);
            EXPECT_TRUE(PSOFound2);
            ASSERT_NE(pPSO2, nullptr);
            ASSERT_EQ(pPSO2->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);

            RefCntAutoPtr<IPipelineState> pPSO3;
            bool                          PSOFound3 = false;
            CreateGraphicsPSO(pCache, true, CompileAsync, "SpecConsts Cache Test", pVS, pPS, SpecConsts, _countof(SpecConsts), pPSO3, &PSOFound3);
            EXPECT_TRUE(PSOFound3);
            ASSERT_NE(pPSO3, nullptr);
            ASSERT_EQ(pPSO3->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);

            if (!CompileAsync)
            {
                EXPECT_EQ(pPSO, pPSO2);
            }
            else
            {
                EXPECT_EQ(pPSO2, pPSO3);
            }
        }

        pData.Release();
        pCache->WriteToBlob(pass == 0 ? ContentVersion : ~0u, &pData);
    }
}

void TestDistinctEntries(bool CompileAsync)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();

    if (pDevice->GetDeviceInfo().Features.SpecializationConstants != DEVICE_FEATURE_STATE_ENABLED)
    {
        GTEST_SKIP() << "Specialization constants are not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IDataBlob> pData;
    for (Uint32 pass = 0; pass < 3; ++pass)
    {
        // 0: empty cache
        // 1: loaded cache
        // 2: reloaded cache (loaded -> stored -> loaded)

        RefCntAutoPtr<IRenderStateCache> pCache = CreateCache(pDevice, /*HotReload=*/false, pData);
        ASSERT_TRUE(pCache);

        RefCntAutoPtr<IShader> pVS, pPS;
        CreateShaders(pCache, pData != nullptr, CompileAsync, pVS, pPS);
        ASSERT_NE(pVS, nullptr);
        ASSERT_NE(pPS, nullptr);

        float RefValuesB[g_SpecConstRefDescCount];
        for (Uint32 i = 0; i < g_SpecConstRefDescCount; ++i)
        {
            RefValuesB[i] = g_SpecConstRefDescs[i].RefValue;
            if (strcmp(g_SpecConstRefDescs[i].Name, "sc_Brightness") == 0)
                RefValuesB[i] = 2.0f;
        }

        SpecializationConstant SpecConstsA[g_SpecConstRefDescCount];
        InitializeSpecConsts(g_SpecConstRefDescs, g_SpecConstRefDescCount, SpecConstsA);

        SpecializationConstant SpecConstsB[g_SpecConstRefDescCount];
        for (Uint32 i = 0; i < g_SpecConstRefDescCount; ++i)
        {
            SpecConstsB[i].Name         = g_SpecConstRefDescs[i].Name;
            SpecConstsB[i].ShaderStages = g_SpecConstRefDescs[i].Stage;
            SpecConstsB[i].pData        = &RefValuesB[i];
            SpecConstsB[i].Size         = sizeof(RefValuesB[i]);
        }

        RefCntAutoPtr<IPipelineState> pPSO_A;
        CreateGraphicsPSO(pCache, pData != nullptr, CompileAsync, "SpecConsts Distinct Test",
                          pVS, pPS, SpecConstsA, _countof(SpecConstsA), pPSO_A);
        ASSERT_NE(pPSO_A, nullptr);
        ASSERT_EQ(pPSO_A->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);
        VerifyPSO(pPSO_A);

        RefCntAutoPtr<IPipelineState> pPSO_B;
        CreateGraphicsPSO(pCache, pData != nullptr, CompileAsync, "SpecConsts Distinct Test",
                          pVS, pPS, SpecConstsB, _countof(SpecConstsB), pPSO_B);
        ASSERT_NE(pPSO_B, nullptr);
        ASSERT_EQ(pPSO_B->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);

        EXPECT_NE(pPSO_A, pPSO_B);

        RefCntAutoPtr<IPipelineState> pPSO_A2;
        bool                          PSOFoundA2 = false;
        CreateGraphicsPSO(pCache, true, CompileAsync, "SpecConsts Distinct Test",
                          pVS, pPS, SpecConstsA, _countof(SpecConstsA), pPSO_A2, &PSOFoundA2);
        EXPECT_TRUE(PSOFoundA2);
        ASSERT_NE(pPSO_A2, nullptr);
        ASSERT_EQ(pPSO_A2->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);
        if (!CompileAsync)
        {
            EXPECT_EQ(pPSO_A, pPSO_A2);
        }
        else
        {
            RefCntAutoPtr<IPipelineState> pPSO_A3;
            bool                          PSOFoundA3 = false;
            CreateGraphicsPSO(pCache, true, CompileAsync, "SpecConsts Distinct Test",
                              pVS, pPS, SpecConstsA, _countof(SpecConstsA), pPSO_A3, &PSOFoundA3);
            EXPECT_TRUE(PSOFoundA3);
            ASSERT_NE(pPSO_A3, nullptr);
            ASSERT_EQ(pPSO_A3->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);
            EXPECT_EQ(pPSO_A2, pPSO_A3);
        }

        RefCntAutoPtr<IPipelineState> pPSO_B2;
        bool                          PSOFoundB2 = false;
        CreateGraphicsPSO(pCache, true, CompileAsync, "SpecConsts Distinct Test",
                          pVS, pPS, SpecConstsB, _countof(SpecConstsB), pPSO_B2, &PSOFoundB2);
        EXPECT_TRUE(PSOFoundB2);
        ASSERT_NE(pPSO_B2, nullptr);
        ASSERT_EQ(pPSO_B2->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);
        if (!CompileAsync)
        {
            EXPECT_EQ(pPSO_B, pPSO_B2);
        }
        else
        {
            RefCntAutoPtr<IPipelineState> pPSO_B3;
            bool                          PSOFoundB3 = false;
            CreateGraphicsPSO(pCache, true, CompileAsync, "SpecConsts Distinct Test",
                              pVS, pPS, SpecConstsB, _countof(SpecConstsB), pPSO_B3, &PSOFoundB3);
            EXPECT_TRUE(PSOFoundB3);
            ASSERT_NE(pPSO_B3, nullptr);
            ASSERT_EQ(pPSO_B3->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);
            EXPECT_EQ(pPSO_B2, pPSO_B3);
        }

        RefCntAutoPtr<IPipelineState> pPSO_None;
        CreateGraphicsPSO(pCache, pData != nullptr, CompileAsync, "SpecConsts Distinct Test",
                          pVS, pPS, nullptr, 0, pPSO_None);
        ASSERT_NE(pPSO_None, nullptr);
        ASSERT_EQ(pPSO_None->GetStatus(CompileAsync), PIPELINE_STATE_STATUS_READY);
        EXPECT_NE(pPSO_None, pPSO_A);
        EXPECT_NE(pPSO_None, pPSO_B);

        pData.Release();
        pCache->WriteToBlob(pass == 0 ? ContentVersion : ~0u, &pData);
    }
}

TEST_F(SpecializationConstants, RenderStateCacheTest)
{
    TestCaches(/*CompileAsync = */ false);
}

TEST_F(SpecializationConstants, RenderStateCacheTest_Async)
{
    TestCaches(/*CompileAsync = */ true);
}

TEST_F(SpecializationConstants, RenderStateCacheTest_DistinctEntries)
{
    TestDistinctEntries(/*CompileAsync = */ false);
}

TEST_F(SpecializationConstants, RenderStateCacheTest_DistinctEntries_Async)
{
    TestDistinctEntries(/*CompileAsync = */ true);
}

} // namespace
