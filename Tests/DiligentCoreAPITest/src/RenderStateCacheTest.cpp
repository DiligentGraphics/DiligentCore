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
#include "RenderStateCache.h"
#include "FastRand.hpp"

#include "gtest/gtest.h"

namespace Diligent
{
namespace Testing
{
void RenderDrawCommandReference(ISwapChain* pSwapChain, const float* pClearColor = nullptr);
}
} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

void TestDraw(IShader* pVS, IShader* pPS, IPipelineState* pPSO)
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

    ITextureView* pRTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
    pCtx->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pCtx->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pCtx->SetPipelineState(pPSO);
    pCtx->Draw({6, DRAW_FLAG_VERIFY_ALL});

    pSwapChain->Present();
}

void VerifyGraphicsShaders(IShader* pVS, IShader* pPS)
{
    TestDraw(pVS, pPS, nullptr);
}

void VerifyGraphicsPSO(IPipelineState* pPSO)
{
    TestDraw(nullptr, nullptr, pPSO);
}

RefCntAutoPtr<IRenderStateCache> CreateCache(IRenderDevice* pDevice, IDataBlob* pCacheData = nullptr)
{
    RenderStateCacheCreateInfo CacheCI;
    CacheCI.pDevice = pDevice;

    RefCntAutoPtr<IRenderStateCache> pCache;
    CreateRenderStateCache(CacheCI, &pCache);

    if (pCacheData != nullptr)
        pCache->Load(pCacheData);

    return pCache;
}

void CreateGraphicsShaders(IRenderStateCache*               pCache,
                           IShaderSourceInputStreamFactory* pShaderSourceFactory,
                           RefCntAutoPtr<IShader>&          pVS,
                           RefCntAutoPtr<IShader>&          pPS,
                           bool                             PresentInCache)
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
        ShaderCI.Desc     = {"RenderStateCache - VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.FilePath = "VertexShader.vsh";
        if (pCache != nullptr)
        {
            EXPECT_EQ(pCache->CreateShader(ShaderCI, &pVS), PresentInCache);
        }
        else
        {
            pDevice->CreateShader(ShaderCI, &pVS);
            EXPECT_EQ(PresentInCache, false);
        }
        ASSERT_TRUE(pVS);
    }

    {
        ShaderCI.Desc     = {"RenderStateCache - PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.FilePath = "PixelShader.psh";
        if (pCache != nullptr)
        {
            EXPECT_EQ(pCache->CreateShader(ShaderCI, &pPS), PresentInCache);
        }
        else
        {
            pDevice->CreateShader(ShaderCI, &pPS);
            EXPECT_EQ(PresentInCache, false);
        }
        ASSERT_TRUE(pPS);
    }
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
    for (Uint32 pass = 0; pass < 2; ++pass)
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

        pData.Release();
        pCache->WriteToBlob(&pData);
    }
}

TEST(RenderStateCacheTest, CreateGraphicsPSO)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    GPUTestingEnvironment::ScopedReset AutoReset;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/RenderStateCache", &pShaderSourceFactory);
    ASSERT_TRUE(pShaderSourceFactory);

    RefCntAutoPtr<IDataBlob> pData;
    for (Uint32 pass = 0; pass < 2; ++pass)
    {
        // 0: empty cache
        // 1: loaded cache
        // 2: reloaded cache (loaded -> stored -> loaded)

        auto pCache = CreateCache(pDevice, pData);
        ASSERT_TRUE(pCache);

        auto CreatePSO = [&](bool PresentInCache, IShader* pVS, IShader* pPS, IPipelineState** ppPSO) {
            GraphicsPipelineStateCreateInfo PsoCI;
            PsoCI.PSODesc.Name = "Render State Cache Test";

            PsoCI.pVS = pVS;
            PsoCI.pPS = pPS;

            PsoCI.GraphicsPipeline.NumRenderTargets             = 1;
            PsoCI.GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_RGBA8_UNORM;
            PsoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

            EXPECT_EQ(pCache->CreateGraphicsPipelineState(PsoCI, ppPSO), PresentInCache);
        };

        RefCntAutoPtr<IShader> pVS1, pPS1;
        CreateGraphicsShaders(pCache, pShaderSourceFactory, pVS1, pPS1, pData != nullptr);
        ASSERT_NE(pVS1, pPS1);

        RefCntAutoPtr<IPipelineState> pPSO;
        CreatePSO(pData != nullptr, pVS1, pPS1, &pPSO);

        VerifyGraphicsPSO(pPSO);

        {
            RefCntAutoPtr<IPipelineState> pPSO2;
            CreatePSO(true, pVS1, pPS1, &pPSO2);
            EXPECT_EQ(pPSO, pPSO2);
        }

        pData.Release();
        pCache->WriteToBlob(&pData);
    }
}

TEST(RenderStateCacheTest, CreateComputePSO)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    GPUTestingEnvironment::ScopedReset AutoReset;

    auto pCache = CreateCache(pDevice);
    ASSERT_TRUE(pCache);

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/RenderStateCache", &pShaderSourceFactory);
    ASSERT_TRUE(pShaderSourceFactory);

    auto CreatePSO = [&](bool PresentInCache) {
        ShaderCreateInfo ShaderCI;
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler             = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

        RefCntAutoPtr<IShader> pCS;
        {
            ShaderCI.Desc     = {"RenderStateCache - CS", SHADER_TYPE_COMPUTE, true};
            ShaderCI.FilePath = "ComputeShader.csh";
            EXPECT_EQ(pCache->CreateShader(ShaderCI, &pCS), PresentInCache);
            ASSERT_TRUE(pCS);
        }

        ComputePipelineStateCreateInfo PsoCI;
        PsoCI.pCS = pCS;

        RefCntAutoPtr<IPipelineState> pPSO;
        EXPECT_EQ(pCache->CreateComputePipelineState(PsoCI, &pPSO), PresentInCache);
    };
    CreatePSO(false);
    CreatePSO(true);

    RefCntAutoPtr<IDataBlob> pData;
    pCache->WriteToBlob(&pData);

    pCache.Release();
    pCache = CreateCache(pDevice, pData);

    CreatePSO(true);
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

    auto pCache = CreateCache(pDevice);
    ASSERT_TRUE(pCache);
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

} // namespace
