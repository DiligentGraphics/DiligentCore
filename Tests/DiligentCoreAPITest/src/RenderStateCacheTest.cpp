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

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

RefCntAutoPtr<IRenderStateCache> CreateCache(IRenderDevice* pDevice)
{
    RenderStateCacheCreateInfo CacheCI;
    CacheCI.pDevice = pDevice;

    RefCntAutoPtr<IRenderStateCache> pCache;
    CreateRenderStateCache(CacheCI, &pCache);
    return pCache;
}

TEST(RenderStateCacheTest, CreateGraphicsPSO)
{
    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    GPUTestingEnvironment::ScopedReset AutoReset;

    auto pCache = CreateCache(pDevice);
    ASSERT_TRUE(pCache);

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders/RenderStateCache", &pShaderSourceFactory);
    ASSERT_TRUE(pShaderSourceFactory);

    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.Name       = "RenderStateCache - VS";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.FilePath        = "VertexShader.vsh";
        EXPECT_FALSE(pCache->CreateShader(ShaderCI, &pVS));
        ASSERT_TRUE(pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.Name       = "RenderStateCache - PS";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.FilePath        = "PixelShader.psh";
        EXPECT_FALSE(pCache->CreateShader(ShaderCI, &pPS));
        ASSERT_TRUE(pPS);
    }

    GraphicsPipelineStateCreateInfo PsoCI;
    PsoCI.pVS = pVS;
    PsoCI.pPS = pPS;

    constexpr LayoutElement VSInputs[] =
        {
            LayoutElement{0, 0, 4, VT_FLOAT32},
            LayoutElement{1, 0, 3, VT_FLOAT32},
            LayoutElement{2, 0, 2, VT_FLOAT32},
        };
    PsoCI.GraphicsPipeline.InputLayout = {VSInputs, _countof(VSInputs)};

    PsoCI.GraphicsPipeline.NumRenderTargets             = 1;
    PsoCI.GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_RGBA8_UNORM;
    PsoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    RefCntAutoPtr<IPipelineState> pPSO;
    EXPECT_FALSE(pCache->CreateGraphicsPipelineState(PsoCI, &pPSO));
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

    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pCS;
    {
        ShaderCI.Desc.Name       = "RenderStateCache - CS";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.FilePath        = "ComputeShader.csh";
        EXPECT_FALSE(pCache->CreateShader(ShaderCI, &pCS));
        ASSERT_TRUE(pCS);
    }

    ComputePipelineStateCreateInfo PsoCI;
    PsoCI.pCS = pCS;

    RefCntAutoPtr<IPipelineState> pPSO;
    EXPECT_FALSE(pCache->CreateComputePipelineState(PsoCI, &pPSO));
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
