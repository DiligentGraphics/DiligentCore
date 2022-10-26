/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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
#include "RenderStateCache.h"
#include "RenderStateCache.hpp"
#include "FastRand.hpp"

#include "InlineShaders/RayTracingTestHLSL.h"

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

void TestDraw(IShader* pVS, IShader* pPS, IPipelineState* pPSO, bool UseRenderPass)
{
    VERIFY_EXPR((pVS != nullptr && pPS != nullptr) ^ (pPSO != nullptr));

    auto* pEnv       = GPUTestingEnvironment::GetInstance();
    auto* pDevice    = pEnv->GetDevice();
    auto* pCtx       = pEnv->GetDeviceContext();
    auto* pSwapChain = pEnv->GetSwapChain();

    RefCntAutoPtr<IPipelineState> _pPSO;
    if (pPSO == nullptr)
    {
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        auto& PSODesc          = PSOCreateInfo.PSODesc;
        auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

        PSODesc.Name = "Render State Cache Test";

        GraphicsPipeline.NumRenderTargets             = 1;
        GraphicsPipeline.RTVFormats[0]                = pSwapChain->GetDesc().ColorBufferFormat;
        GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
        GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;

        pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &_pPSO);
        ASSERT_NE(_pPSO, nullptr);

        pPSO = _pPSO;
    }

    static FastRandFloat rnd{0, 0, 1};
    const float          ClearColor[] = {rnd(), rnd(), rnd(), rnd()};
    RenderDrawCommandReference(pSwapChain, ClearColor);

    RefCntAutoPtr<IFramebuffer> pFramebuffer;
    if (UseRenderPass)
    {
        ITextureView* pRTAttachments[] = {pSwapChain->GetCurrentBackBufferRTV()};

        FramebufferDesc FBDesc;
        FBDesc.Name            = "Render state cache test";
        FBDesc.pRenderPass     = pPSO->GetGraphicsPipelineDesc().pRenderPass;
        FBDesc.AttachmentCount = _countof(pRTAttachments);
        FBDesc.ppAttachments   = pRTAttachments;
        pDevice->CreateFramebuffer(FBDesc, &pFramebuffer);
        ASSERT_TRUE(pFramebuffer);

        BeginRenderPassAttribs RPBeginInfo;
        RPBeginInfo.pRenderPass  = FBDesc.pRenderPass;
        RPBeginInfo.pFramebuffer = pFramebuffer;

        OptimizedClearValue ClearValues[1];
        ClearValues[0].Color[0] = ClearColor[0];
        ClearValues[0].Color[1] = ClearColor[1];
        ClearValues[0].Color[2] = ClearColor[2];
        ClearValues[0].Color[3] = ClearColor[3];

        RPBeginInfo.pClearValues        = ClearValues;
        RPBeginInfo.ClearValueCount     = _countof(ClearValues);
        RPBeginInfo.StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        pCtx->BeginRenderPass(RPBeginInfo);
    }
    else
    {
        ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
        pCtx->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pCtx->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    pCtx->SetPipelineState(pPSO);
    pCtx->Draw({6, DRAW_FLAG_VERIFY_ALL});

    if (UseRenderPass)
    {
        pCtx->EndRenderPass();
    }

    pSwapChain->Present();
}

void VerifyGraphicsShaders(IShader* pVS, IShader* pPS)
{
    TestDraw(pVS, pPS, nullptr, false);
}

void VerifyGraphicsPSO(IPipelineState* pPSO, bool UseRenderPass)
{
    TestDraw(nullptr, nullptr, pPSO, UseRenderPass);
}

void VerifyComputePSO(IPipelineState* pPSO, bool UseSignature = false)
{
    auto* pEnv       = GPUTestingEnvironment::GetInstance();
    auto* pCtx       = pEnv->GetDeviceContext();
    auto* pSwapChain = pEnv->GetSwapChain();

    pCtx->Flush();
    pCtx->InvalidateState();
    ComputeShaderReference(pSwapChain);

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    if (UseSignature)
    {
        auto* pSign = pPSO->GetResourceSignature(0);
        ASSERT_NE(pSign, nullptr);
        pSign->CreateShaderResourceBinding(&pSRB, true);
    }
    else
    {
        pPSO->CreateShaderResourceBinding(&pSRB, true);
    }
    ASSERT_TRUE(pSRB);

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_ITestingSwapChain};
    ASSERT_NE(pTestingSwapChain, nullptr);
    pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_tex2DUAV")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

    pCtx->SetPipelineState(pPSO);
    pCtx->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const auto& SCDesc = pSwapChain->GetDesc();

    DispatchComputeAttribs DispatchAttribs;
    DispatchAttribs.ThreadGroupCountX = (SCDesc.Width + 15) / 16;
    DispatchAttribs.ThreadGroupCountY = (SCDesc.Height + 15) / 16;
    pCtx->DispatchCompute(DispatchAttribs);

    pSwapChain->Present();
}

RefCntAutoPtr<IRenderStateCache> CreateCache(IRenderDevice* pDevice, IDataBlob* pCacheData = nullptr)
{
    RenderStateCacheCreateInfo CacheCI{pDevice, true};

    RefCntAutoPtr<IRenderStateCache> pCache;
    CreateRenderStateCache(CacheCI, &pCache);

    if (pCacheData != nullptr)
        pCache->Load(pCacheData);

    return pCache;
}

void CreateShader(IRenderStateCache*               pCache,
                  IShaderSourceInputStreamFactory* pShaderSourceFactory,
                  SHADER_TYPE                      Type,
                  const char*                      Name,
                  const char*                      Path,
                  bool                             PresentInCache,
                  RefCntAutoPtr<IShader>&          pShader)
{
    auto* const pEnv    = GPUTestingEnvironment::GetInstance();
    auto* const pDevice = pEnv->GetDevice();

    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler             = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

    constexpr ShaderMacro Macros[] = {{"EXTERNAL_MACROS", "2"}, {}};
    ShaderCI.Macros                = Macros;

    {
        ShaderCI.Desc     = {Name, Type, true};
        ShaderCI.FilePath = Path;
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
}

void CreateGraphicsShaders(IRenderStateCache*               pCache,
                           IShaderSourceInputStreamFactory* pShaderSourceFactory,
                           RefCntAutoPtr<IShader>&          pVS,
                           RefCntAutoPtr<IShader>&          pPS,
                           bool                             PresentInCache,
                           const char*                      VSPath = nullptr,
                           const char*                      PSPath = nullptr)
{
    CreateShader(pCache, pShaderSourceFactory, SHADER_TYPE_VERTEX,
                 "RenderStateCache - VS", VSPath != nullptr ? VSPath : "VertexShader.vsh",
                 PresentInCache, pVS);
    ASSERT_TRUE(pVS);

    CreateShader(pCache, pShaderSourceFactory, SHADER_TYPE_PIXEL,
                 "RenderStateCache - PS", VSPath != nullptr ? PSPath : "PixelShader.psh",
                 PresentInCache, pPS);
    ASSERT_TRUE(pPS);
}


void CreateComputeShader(IRenderStateCache*               pCache,
                         IShaderSourceInputStreamFactory* pShaderSourceFactory,
                         RefCntAutoPtr<IShader>&          pCS,
                         bool                             PresentInCache)
{
    CreateShader(pCache, pShaderSourceFactory, SHADER_TYPE_COMPUTE,
                 "RenderStateCache - CS", "ComputeShader.csh",
                 PresentInCache, pCS);
}

TEST(RenderStateCacheTest, CreateShader)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/RenderStateCache", &pShaderSourceFactory);
    ASSERT_TRUE(pShaderSourceFactory);

    RefCntAutoPtr<IDataBlob> pData;
    for (Uint32 pass = 0; pass < 3; ++pass)
    {
        // 0: empty cache
        // 1: loaded cache
        // 2: reloaded cache (loaded -> stored -> loaded)

        auto pCache = CreateCache(pDevice, pData);
        ASSERT_TRUE(pCache);

        {
            RefCntAutoPtr<IShader> pVS, pPS;
            CreateGraphicsShaders(pCache, pShaderSourceFactory, pVS, pPS, pData != nullptr);
            ASSERT_NE(pVS, nullptr);
            ASSERT_NE(pPS, nullptr);

            VerifyGraphicsShaders(pVS, pPS);

            RefCntAutoPtr<IShader> pVS2, pPS2;
            CreateGraphicsShaders(pCache, pShaderSourceFactory, pVS2, pPS2, true);
            EXPECT_EQ(pVS, pVS2);
            EXPECT_EQ(pPS, pPS);
        }

        {
            RefCntAutoPtr<IShader> pVS, pPS;
            CreateGraphicsShaders(pCache, pShaderSourceFactory, pVS, pPS, true);
            EXPECT_NE(pVS, nullptr);
            EXPECT_NE(pPS, nullptr);
        }

        {
            RefCntAutoPtr<IShader> pCS;
            CreateComputeShader(pCache, pShaderSourceFactory, pCS, pData != nullptr);
            EXPECT_NE(pCS, nullptr);
        }

        pData.Release();
        pCache->WriteToBlob(&pData);
    }
}

TEST(RenderStateCacheTest, BrokenShader)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    GPUTestingEnvironment::ScopedReset AutoReset;

    constexpr char NotASource[] = "Not a shader source";

    auto pCache = CreateCache(pDevice);
    ASSERT_TRUE(pCache);

    ShaderCreateInfo ShaderCI;
    ShaderCI.Source       = NotASource;
    ShaderCI.SourceLength = sizeof(NotASource);

    constexpr ShaderMacro Macros[] = {{"EXTERNAL_MACROS", "2"}, {}};
    ShaderCI.Macros                = Macros;
    ShaderCI.Desc                  = {"Broken shader", SHADER_TYPE_VERTEX, true};
    RefCntAutoPtr<IShader> pShader;
    pEnv->SetErrorAllowance(6, "\n\nNo worries, testing broken shader...\n\n");
    EXPECT_FALSE(pCache->CreateShader(ShaderCI, &pShader));
    EXPECT_EQ(pShader, nullptr);
}

void CreateGraphicsPSO(IRenderStateCache* pCache, bool PresentInCache, IShader* pVS, IShader* pPS, bool UseRenderPass, IPipelineState** ppPSO)
{
    auto* pEnv       = GPUTestingEnvironment::GetInstance();
    auto* pDevice    = pEnv->GetDevice();
    auto* pSwapChain = pEnv->GetSwapChain();

    GraphicsPipelineStateCreateInfo PsoCI;
    PsoCI.PSODesc.Name = "Render State Cache Test";

    PsoCI.pVS = pVS;
    PsoCI.pPS = pPS;

    const auto ColorBufferFormat = pSwapChain->GetDesc().ColorBufferFormat;

    RefCntAutoPtr<IRenderPass> pRenderPass;
    if (UseRenderPass)
    {
        RenderPassAttachmentDesc Attachments[1];
        Attachments[0].Format       = ColorBufferFormat;
        Attachments[0].InitialState = RESOURCE_STATE_RENDER_TARGET;
        Attachments[0].FinalState   = RESOURCE_STATE_RENDER_TARGET;
        Attachments[0].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
        Attachments[0].StoreOp      = ATTACHMENT_STORE_OP_STORE;

        constexpr AttachmentReference RTAttachmentRefs0[] =
            {
                {0, RESOURCE_STATE_RENDER_TARGET},
            };
        SubpassDesc Subpasses[1];
        Subpasses[0].RenderTargetAttachmentCount = _countof(RTAttachmentRefs0);
        Subpasses[0].pRenderTargetAttachments    = RTAttachmentRefs0;

        RenderPassDesc RPDesc;
        RPDesc.Name            = "Render State Cache Test";
        RPDesc.AttachmentCount = _countof(Attachments);
        RPDesc.pAttachments    = Attachments;
        RPDesc.SubpassCount    = _countof(Subpasses);
        RPDesc.pSubpasses      = Subpasses;

        pDevice->CreateRenderPass(RPDesc, &pRenderPass);
        ASSERT_NE(pRenderPass, nullptr);
        PsoCI.GraphicsPipeline.pRenderPass = pRenderPass;
    }
    else
    {
        PsoCI.GraphicsPipeline.NumRenderTargets = 1;
        PsoCI.GraphicsPipeline.RTVFormats[0]    = ColorBufferFormat;
    }

    EXPECT_EQ(pCache->CreateGraphicsPipelineState(PsoCI, ppPSO), PresentInCache);
    if (*ppPSO != nullptr)
    {
        const auto& Desc = (*ppPSO)->GetDesc();
        EXPECT_EQ(PsoCI.PSODesc, Desc);

        if (UseRenderPass)
        {
            auto* _pRenderPass = (*ppPSO)->GetGraphicsPipelineDesc().pRenderPass;
            ASSERT_NE(_pRenderPass, nullptr);
            EXPECT_EQ(_pRenderPass->GetDesc(), pRenderPass->GetDesc());
        }
    }
}

void TestGraphicsPSO(bool UseRenderPass)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/RenderStateCache", &pShaderSourceFactory);
    ASSERT_TRUE(pShaderSourceFactory);

    RefCntAutoPtr<IShader> pUncachedVS, pUncachedPS;
    CreateGraphicsShaders(nullptr, pShaderSourceFactory, pUncachedVS, pUncachedPS, false, "VertexShader2.vsh", "PixelShader2.psh");
    ASSERT_NE(pUncachedVS, nullptr);
    ASSERT_NE(pUncachedPS, nullptr);

    RefCntAutoPtr<IDataBlob> pData;
    for (Uint32 pass = 0; pass < 3; ++pass)
    {
        // 0: empty cache
        // 1: loaded cache
        // 2: reloaded cache (loaded -> stored -> loaded)

        auto pCache = CreateCache(pDevice, pData);
        ASSERT_TRUE(pCache);

        RefCntAutoPtr<IShader> pVS1, pPS1;
        CreateGraphicsShaders(pCache, pShaderSourceFactory, pVS1, pPS1, pData != nullptr);
        ASSERT_NE(pVS1, nullptr);
        ASSERT_NE(pPS1, nullptr);

        RefCntAutoPtr<IPipelineState> pPSO;
        CreateGraphicsPSO(pCache, pData != nullptr, pVS1, pPS1, UseRenderPass, &pPSO);
        ASSERT_NE(pPSO, nullptr);

        VerifyGraphicsPSO(pPSO, UseRenderPass);

        {
            RefCntAutoPtr<IPipelineState> pPSO2;
            CreateGraphicsPSO(pCache, true, pVS1, pPS1, UseRenderPass, &pPSO2);
            EXPECT_EQ(pPSO, pPSO2);
        }

        {
            RefCntAutoPtr<IPipelineState> pPSO2;
            CreateGraphicsPSO(pCache, pData != nullptr, pUncachedVS, pUncachedPS, UseRenderPass, &pPSO2);
            VerifyGraphicsPSO(pPSO2, UseRenderPass);
        }

        pData.Release();
        pCache->WriteToBlob(&pData);
    }
}

TEST(RenderStateCacheTest, CreateGraphicsPSO)
{
    TestGraphicsPSO(/*UseRenderPass = */ false);
}

TEST(RenderStateCacheTest, CreateGraphicsPSO_RenderPass)
{
    TestGraphicsPSO(/*UseRenderPass = */ true);
}

void CreateComputePSO(IRenderStateCache* pCache, bool PresentInCache, IShader* pCS, bool UseSignature, IPipelineState** ppPSO)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    ComputePipelineStateCreateInfo PsoCI;
    PsoCI.PSODesc.Name = "Render State Cache Test";
    PsoCI.pCS          = pCS;

    constexpr ShaderResourceVariableDesc Variables[] //
        {
            ShaderResourceVariableDesc{SHADER_TYPE_COMPUTE, "g_tex2DUAV", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE} //
        };

    constexpr PipelineResourceDesc Resources[] //
        {
            PipelineResourceDesc{SHADER_TYPE_COMPUTE, "g_tex2DUAV", 1, SHADER_RESOURCE_TYPE_TEXTURE_UAV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE} //
        };

    RefCntAutoPtr<IPipelineResourceSignature> pSign;
    IPipelineResourceSignature*               ppSignatures[1] = {};

    if (UseSignature)
    {
        PipelineResourceSignatureDesc SignDesc;

        SignDesc.Name         = "Render State Cache Test";
        SignDesc.Resources    = Resources;
        SignDesc.NumResources = _countof(Resources);
        pDevice->CreatePipelineResourceSignature(SignDesc, &pSign);
        ASSERT_NE(pSign, nullptr);
        ppSignatures[0]               = pSign;
        PsoCI.ppResourceSignatures    = ppSignatures;
        PsoCI.ResourceSignaturesCount = _countof(ppSignatures);
    }
    else
    {
        PsoCI.PSODesc.ResourceLayout.Variables    = Variables;
        PsoCI.PSODesc.ResourceLayout.NumVariables = _countof(Variables);
    }

    EXPECT_EQ(pCache->CreateComputePipelineState(PsoCI, ppPSO), PresentInCache);
    if (*ppPSO != nullptr)
    {
        const auto& Desc = (*ppPSO)->GetDesc();
        EXPECT_EQ(PsoCI.PSODesc, Desc);
        if (UseSignature)
        {
            EXPECT_EQ((*ppPSO)->GetResourceSignatureCount(), 1u);
            auto* _pSign = (*ppPSO)->GetResourceSignature(0);
            ASSERT_NE(_pSign, nullptr);
            EXPECT_EQ(_pSign->GetDesc(), pSign->GetDesc());
        }
    }
}

void TestComputePSO(bool UseSignature)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.ComputeShaders)
    {
        GTEST_SKIP() << "Compute shaders are not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/RenderStateCache", &pShaderSourceFactory);
    ASSERT_TRUE(pShaderSourceFactory);

    RefCntAutoPtr<IDataBlob> pData;
    for (Uint32 pass = 0; pass < 3; ++pass)
    {
        // 0: empty cache
        // 1: loaded cache
        // 2: reloaded cache (loaded -> stored -> loaded)

        auto pCache = CreateCache(pDevice, pData);
        ASSERT_TRUE(pCache);

        RefCntAutoPtr<IShader> pCS;
        CreateComputeShader(pCache, pShaderSourceFactory, pCS, pData != nullptr);
        ASSERT_NE(pCS, nullptr);

        RefCntAutoPtr<IPipelineState> pPSO;
        CreateComputePSO(pCache, pData != nullptr, pCS, UseSignature, &pPSO);
        ASSERT_NE(pPSO, nullptr);

        VerifyComputePSO(pPSO, /* UseSignature = */ true);

        {
            RefCntAutoPtr<IPipelineState> pPSO2;
            CreateComputePSO(pCache, true, pCS, UseSignature, &pPSO2);
            EXPECT_EQ(pPSO, pPSO2);
        }

        pData.Release();
        pCache->WriteToBlob(&pData);
    }
}

TEST(RenderStateCacheTest, CreateComputePSO)
{
    TestComputePSO(/*UseSignature = */ false);
}

TEST(RenderStateCacheTest, CreateComputePSO_Sign)
{
    TestComputePSO(/*UseSignature = */ true);
}

void CreateRayTracingShaders(IRenderStateCache*               pCache,
                             IShaderSourceInputStreamFactory* pShaderSourceFactory,
                             RefCntAutoPtr<IShader>&          pRayGen,
                             RefCntAutoPtr<IShader>&          pRayMiss,
                             RefCntAutoPtr<IShader>&          pClosestHit,
                             RefCntAutoPtr<IShader>&          pIntersection,
                             bool                             PresentInCache)
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;
    ShaderCI.HLSLVersion    = {6, 3};
    ShaderCI.EntryPoint     = "main";

    // Create ray generation shader.
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
        ShaderCI.Desc.Name       = "Render State Cache - RayGen";
        ShaderCI.Source          = HLSL::RayTracingTest1_RG.c_str();
        pCache->CreateShader(ShaderCI, &pRayGen);
        ASSERT_NE(pRayGen, nullptr);
    }

    // Create ray miss shader.
    RefCntAutoPtr<IShader> pRMiss;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
        ShaderCI.Desc.Name       = "Render State Cache - Miss Shader";
        ShaderCI.Source          = HLSL::RayTracingTest1_RM.c_str();
        pCache->CreateShader(ShaderCI, &pRayMiss);
        ASSERT_NE(pRayMiss, nullptr);
    }

    // Create ray closest hit shader.
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
        ShaderCI.Desc.Name       = "Render State Cache - Closest Hit";
        ShaderCI.Source          = HLSL::RayTracingTest1_RCH.c_str();
        pCache->CreateShader(ShaderCI, &pClosestHit);
        ASSERT_NE(pClosestHit, nullptr);
    }

    // Create ray intersection shader.
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_INTERSECTION;
        ShaderCI.Desc.Name       = "Ray intersection shader";
        ShaderCI.Source          = HLSL::RayTracingTest3_RI.c_str();
        pCache->CreateShader(ShaderCI, &pIntersection);
        ASSERT_NE(pIntersection, nullptr);
    }
}


void CreateRayTracingPSO(IRenderStateCache* pCache,
                         bool               PresentInCache,
                         IShader*           pRayGen,
                         IShader*           pRayMiss,
                         IShader*           pClosestHit,
                         IShader*           pIntersection,
                         IPipelineState**   ppPSO)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    RayTracingPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Render State Cache - Ray Tracing PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_RAY_TRACING;

    const RayTracingGeneralShaderGroup       GeneralShaders[]       = {{"Main", pRayGen}, {"Miss", pRayMiss}};
    const RayTracingTriangleHitShaderGroup   TriangleHitShaders[]   = {{"TriHitGroup", pClosestHit}};
    const RayTracingProceduralHitShaderGroup ProceduralHitShaders[] = {{"ProcHitGroup", pIntersection, pClosestHit}};

    PSOCreateInfo.pGeneralShaders          = GeneralShaders;
    PSOCreateInfo.GeneralShaderCount       = _countof(GeneralShaders);
    PSOCreateInfo.pTriangleHitShaders      = TriangleHitShaders;
    PSOCreateInfo.TriangleHitShaderCount   = _countof(TriangleHitShaders);
    PSOCreateInfo.pProceduralHitShaders    = ProceduralHitShaders;
    PSOCreateInfo.ProceduralHitShaderCount = _countof(ProceduralHitShaders);

    PSOCreateInfo.RayTracingPipeline.MaxRecursionDepth       = 1;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    pDevice->CreateRayTracingPipelineState(PSOCreateInfo, ppPSO);
    ASSERT_NE(*ppPSO, nullptr);
}

TEST(RenderStateCacheTest, CreateRayTracingPSO)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    if (!pEnv->SupportsRayTracing())
    {
        GTEST_SKIP() << "Ray tracing is not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/RenderStateCache", &pShaderSourceFactory);
    ASSERT_TRUE(pShaderSourceFactory);

    RefCntAutoPtr<IDataBlob> pData;
    for (Uint32 pass = 0; pass < 3; ++pass)
    {
        // 0: empty cache
        // 1: loaded cache
        // 2: reloaded cache (loaded -> stored -> loaded)

        auto pCache = CreateCache(pDevice, pData);
        ASSERT_TRUE(pCache);

        RefCntAutoPtr<IShader> pRayGen;
        RefCntAutoPtr<IShader> pRayMiss;
        RefCntAutoPtr<IShader> pClosestHit;
        RefCntAutoPtr<IShader> pIntersection;
        CreateRayTracingShaders(pCache, pShaderSourceFactory, pRayGen, pRayMiss, pClosestHit, pIntersection, pData != nullptr);

        RefCntAutoPtr<IPipelineState> pPSO;
        CreateRayTracingPSO(pCache, pData != nullptr, pRayGen, pRayMiss, pClosestHit, pIntersection, &pPSO);
        ASSERT_NE(pPSO, nullptr);

        {
            RefCntAutoPtr<IPipelineState> pPSO2;
            CreateRayTracingPSO(pCache, true, pRayGen, pRayMiss, pClosestHit, pIntersection, &pPSO2);
            ASSERT_NE(pPSO2, nullptr);
        }

        pData.Release();
        pCache->WriteToBlob(&pData);
    }
}

TEST(RenderStateCacheTest, CreateTilePSO)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    if (!pDevice->GetDeviceInfo().Features.TileShaders)
    {
        GTEST_SKIP() << "Tile shader is not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset AutoReset;

    auto pCache = CreateCache(pDevice);
    ASSERT_TRUE(pCache);
}


TEST(RenderStateCacheTest, BrokenPSO)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    GPUTestingEnvironment::ScopedReset AutoReset;

    auto pCache = CreateCache(pDevice);
    ASSERT_TRUE(pCache);

    GraphicsPipelineStateCreateInfo PipelineCI;
    PipelineCI.PSODesc.Name = "Invalid PSO";
    PipelineCI.pVS          = nullptr; // Must not be null
    pEnv->SetErrorAllowance(2, "\n\nNo worries, testing broken PSO...\n\n");
    RefCntAutoPtr<IPipelineState> pPSO;
    EXPECT_FALSE(pCache->CreateGraphicsPipelineState(PipelineCI, &pPSO));
    EXPECT_EQ(pPSO, nullptr);
}


TEST(RenderStateCacheTest, AppendData)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceInfo().Features.ComputeShaders)
    {
        GTEST_SKIP() << "Compute shaders are not supported by this device";
    }

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/RenderStateCache", &pShaderSourceFactory);
    ASSERT_TRUE(pShaderSourceFactory);

    constexpr bool UseSignature  = false;
    constexpr bool UseRenderPass = false;

    RefCntAutoPtr<IDataBlob> pData;
    {
        auto pCache = CreateCache(pDevice);

        RefCntAutoPtr<IShader> pCS;
        CreateComputeShader(pCache, pShaderSourceFactory, pCS, false);
        ASSERT_NE(pCS, nullptr);

        RefCntAutoPtr<IPipelineState> pPSO;
        CreateComputePSO(pCache, /*PresentInCache = */ false, pCS, UseSignature, &pPSO);
        ASSERT_NE(pPSO, nullptr);

        pCache->WriteToBlob(&pData);
        ASSERT_NE(pData, nullptr);
    }

    for (Uint32 pass = 0; pass < 3; ++pass)
    {
        auto pCache = CreateCache(pDevice, pData);

        RefCntAutoPtr<IShader> pVS1, pPS1;
        CreateGraphicsShaders(pCache, pShaderSourceFactory, pVS1, pPS1, pass > 0);
        ASSERT_NE(pVS1, nullptr);
        ASSERT_NE(pPS1, nullptr);

        RefCntAutoPtr<IPipelineState> pPSO;
        CreateGraphicsPSO(pCache, pass > 0, pVS1, pPS1, UseRenderPass, &pPSO);
        ASSERT_NE(pPSO, nullptr);

        VerifyGraphicsPSO(pPSO, UseRenderPass);

        pData.Release();
        pCache->WriteToBlob(&pData);
        ASSERT_NE(pData, nullptr);
    }
}

TEST(RenderStateCacheTest, RenderDeviceWithCache)
{
    constexpr bool Execute = false;
    if (Execute)
    {
        RenderDeviceWithCache<> Device{nullptr, nullptr};
        {
            auto pShader = Device.CreateShader(ShaderCreateInfo{});
            pShader.Release();
        }
        {
            auto pPSO = Device.CreateGraphicsPipelineState(GraphicsPipelineStateCreateInfo{});
            pPSO.Release();
            pPSO = Device.CreatePipelineState(GraphicsPipelineStateCreateInfo{});
            pPSO.Release();
        }
        {
            auto pPSO = Device.CreateComputePipelineState(ComputePipelineStateCreateInfo{});
            pPSO.Release();
            pPSO = Device.CreatePipelineState(ComputePipelineStateCreateInfo{});
            pPSO.Release();
        }
        {
            auto pPSO = Device.CreateRayTracingPipelineState(RayTracingPipelineStateCreateInfo{});
            pPSO.Release();
            pPSO = Device.CreatePipelineState(RayTracingPipelineStateCreateInfo{});
            pPSO.Release();
        }
        {
            auto pPSO = Device.CreateTilePipelineState(TilePipelineStateCreateInfo{});
            pPSO.Release();
            pPSO = Device.CreatePipelineState(TilePipelineStateCreateInfo{});
            pPSO.Release();
        }
    }
}

} // namespace
