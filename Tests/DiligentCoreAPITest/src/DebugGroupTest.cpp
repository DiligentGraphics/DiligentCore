/*
 *  Copyright 2025 Diligent Graphics LLC
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
#include "ScopedDebugGroup.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

namespace HLSL
{
constexpr char VS[] =
    R"(float4 main() : SV_Position
{
    return float4(0.0, 0.0, 0.0, 1.0);
}
)";

constexpr char PS[] =
    R"(float4 main() : SV_Target
{
	return float4(1.0, 0.0, 0.0, 1.0);
}
)";

constexpr char CS[] =
    R"(
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
}
)";
} // namespace HLSL

class DebugGroupTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
        IRenderDevice*         pDevice    = pEnv->GetDevice();
        ISwapChain*            pSwapChain = pEnv->GetSwapChain();
        const SwapChainDesc&   SCDesc     = pSwapChain->GetDesc();

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
        ShaderCI.EntryPoint     = "main";

        ShaderCI.Desc   = {"DebugGroupTest - VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.Source = HLSL::VS;
        pDevice->CreateShader(ShaderCI, &m_Resources.pVS);
        ASSERT_NE(m_Resources.pVS, nullptr);

        ShaderCI.Desc   = {"DebugGroupTest - PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.Source = HLSL::PS;
        pDevice->CreateShader(ShaderCI, &m_Resources.pPS);
        ASSERT_NE(m_Resources.pPS, nullptr);

        ShaderCI.Desc   = {"DebugGroupTest - CS", SHADER_TYPE_COMPUTE, true};
        ShaderCI.Source = HLSL::CS;
        pDevice->CreateShader(ShaderCI, &m_Resources.pCS);
        ASSERT_NE(m_Resources.pCS, nullptr);

        {
            GraphicsPipelineStateCreateInfo PSOCreateInfo{"DebugGroupTest - Graphics PSO"};
            PSOCreateInfo.pVS                               = m_Resources.pVS;
            PSOCreateInfo.pPS                               = m_Resources.pPS;
            PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
            PSOCreateInfo.GraphicsPipeline.RTVFormats[0]    = SCDesc.ColorBufferFormat;
            PSOCreateInfo.GraphicsPipeline.DSVFormat        = SCDesc.DepthBufferFormat;
            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_Resources.pGraphicsPSO);
            ASSERT_NE(m_Resources.pGraphicsPSO, nullptr);
            m_Resources.pGraphicsPSO->CreateShaderResourceBinding(&m_Resources.pGraphicsSRB, true);
            ASSERT_NE(m_Resources.pGraphicsSRB, nullptr);
        }

        {
            ComputePipelineStateCreateInfo PSOCreateInfo{"DebugGroupTest - Compute CS"};
            PSOCreateInfo.pCS = m_Resources.pCS;
            pDevice->CreateComputePipelineState(PSOCreateInfo, &m_Resources.pComputePSO);
            ASSERT_NE(m_Resources.pComputePSO, nullptr);
            m_Resources.pComputePSO->CreateShaderResourceBinding(&m_Resources.pComputeSRB, true);
            ASSERT_NE(m_Resources.pComputeSRB, nullptr);
        }
    }

    static void TearDownTestSuite()
    {
        m_Resources = {};
    }

    static void Draw()
    {
        GPUTestingEnvironment* pEnv       = GPUTestingEnvironment::GetInstance();
        IDeviceContext*        pCtx       = pEnv->GetDeviceContext();
        ISwapChain*            pSwapChain = pEnv->GetSwapChain();

        constexpr float  DebugGroupColor[] = {1, 0, 0, 0};
        ScopedDebugGroup DebugGroup{pCtx, "DebugGroupTest - Draw", DebugGroupColor};

        ITextureView* RTVs[] = {pSwapChain->GetCurrentBackBufferRTV()};
        pCtx->SetRenderTargets(1, RTVs, pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pCtx->SetPipelineState(m_Resources.pGraphicsPSO);
        pCtx->CommitShaderResources(m_Resources.pGraphicsSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pCtx->Draw({3, DRAW_FLAG_VERIFY_ALL});
    }

    static void DispatchCompute()
    {
        GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
        IDeviceContext*        pCtx = pEnv->GetDeviceContext();

        constexpr float  DebugGroupColor[] = {0, 1, 0, 0};
        ScopedDebugGroup DebugGroup{pCtx, "DebugGroupTest - Compute", DebugGroupColor};

        pCtx->SetPipelineState(m_Resources.pComputePSO);
        pCtx->CommitShaderResources(m_Resources.pComputeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pCtx->DispatchCompute({1, 1, 1});
    }

    struct Resources
    {
        RefCntAutoPtr<IShader>                pVS;
        RefCntAutoPtr<IShader>                pPS;
        RefCntAutoPtr<IShader>                pCS;
        RefCntAutoPtr<IPipelineState>         pGraphicsPSO;
        RefCntAutoPtr<IShaderResourceBinding> pGraphicsSRB;
        RefCntAutoPtr<IPipelineState>         pComputePSO;
        RefCntAutoPtr<IShaderResourceBinding> pComputeSRB;
    };
    static Resources m_Resources;
};
DebugGroupTest::Resources DebugGroupTest::m_Resources;

TEST_F(DebugGroupTest, Empty)
{
    GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx = pEnv->GetDeviceContext();
    pCtx->Flush();

    {
        GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;
        constexpr float                    DebugGroupColor[] = {0, 0, 1, 0};
        ScopedDebugGroup                   DebugGroup{pCtx, "DebugGroupTest - Empty", DebugGroupColor};
    }

    pCtx->Flush();
}

TEST_F(DebugGroupTest, Draw)
{
    GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx = pEnv->GetDeviceContext();
    pCtx->Flush();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    Draw();
}

TEST_F(DebugGroupTest, Compute)
{
    GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx = pEnv->GetDeviceContext();
    pCtx->Flush();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    DispatchCompute();
}

TEST_F(DebugGroupTest, DrawAfterCompute)
{
    GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx = pEnv->GetDeviceContext();
    pCtx->Flush();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    DispatchCompute();
    Draw();
    DispatchCompute();
    Draw();
}

TEST_F(DebugGroupTest, ComputeAfterDraw)
{
    GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx = pEnv->GetDeviceContext();
    pCtx->Flush();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    Draw();
    DispatchCompute();
    Draw();
    DispatchCompute();
}

TEST_F(DebugGroupTest, NestedDraw)
{
    GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx = pEnv->GetDeviceContext();
    pCtx->Flush();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    constexpr float  DebugGroupColor[] = {0, 0, 1, 0};
    ScopedDebugGroup DebugGroup{pCtx, "DebugGroupTest - NestedDraw", DebugGroupColor};
    Draw();
    Draw();
    Draw();
}

TEST_F(DebugGroupTest, NestedCompute)
{
    GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx = pEnv->GetDeviceContext();
    pCtx->Flush();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    constexpr float  DebugGroupColor[] = {0, 0, 1, 0};
    ScopedDebugGroup DebugGroup{pCtx, "DebugGroupTest - NestedCompute", DebugGroupColor};
    DispatchCompute();
    DispatchCompute();
    DispatchCompute();
}

TEST_F(DebugGroupTest, NestedDrawAfterCompute)
{
    GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx = pEnv->GetDeviceContext();
    pCtx->Flush();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    constexpr float  DebugGroupColor[] = {0, 0, 1, 0};
    ScopedDebugGroup DebugGroup{pCtx, "DebugGroupTest - NestedDrawAfterCompute", DebugGroupColor};
    DispatchCompute();
    Draw();
    {
        ScopedDebugGroup DebugGroup2{pCtx, "DebugGroupTest - NestedDrawAfterCompute 2", DebugGroupColor};
        DispatchCompute();
        Draw();
    }
}

TEST_F(DebugGroupTest, NestedComputeAfterDraw)
{
    GPUTestingEnvironment* pEnv = GPUTestingEnvironment::GetInstance();
    IDeviceContext*        pCtx = pEnv->GetDeviceContext();
    pCtx->Flush();

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    constexpr float  DebugGroupColor[] = {0, 0, 1, 0};
    ScopedDebugGroup DebugGroup{pCtx, "DebugGroupTest - NestedComputeAfterDraw", DebugGroupColor};
    Draw();
    DispatchCompute();
    {
        ScopedDebugGroup DebugGroup2{pCtx, "DebugGroupTest - NestedComputeAfterDraw 2", DebugGroupColor};
        Draw();
        DispatchCompute();
    }
}

} // namespace
