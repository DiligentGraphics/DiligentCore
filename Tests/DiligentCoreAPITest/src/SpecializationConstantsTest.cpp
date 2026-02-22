/*
 *  Copyright 2019-2026 Diligent Graphics LLC
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

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static constexpr char g_TrivialVSSource[] = R"(
void main(out float4 pos : SV_Position)
{
    pos = float4(0.0, 0.0, 0.0, 0.0);
}
)";

static constexpr char g_TrivialPSSource[] = R"(
float4 main() : SV_Target
{
    return float4(0.0, 0.0, 0.0, 0.0);
}
)";

class SpecializationConstantsTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        auto* pEnv    = GPUTestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

        ShaderCI.Source     = g_TrivialVSSource;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc       = {"TrivialVS (SpecializationConstantsTest)", SHADER_TYPE_VERTEX, true};
        pDevice->CreateShader(ShaderCI, &sm_pVS);
        ASSERT_TRUE(sm_pVS);

        ShaderCI.Source     = g_TrivialPSSource;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc       = {"TrivialPS (SpecializationConstantsTest)", SHADER_TYPE_PIXEL, true};
        pDevice->CreateShader(ShaderCI, &sm_pPS);
        ASSERT_TRUE(sm_pPS);
    }

    static void TearDownTestSuite()
    {
        sm_pVS.Release();
        sm_pPS.Release();
        GPUTestingEnvironment::GetInstance()->Reset();
    }

    static GraphicsPipelineStateCreateInfo GetDefaultPSOCI(const char* Name)
    {
        GraphicsPipelineStateCreateInfo PsoCI;
        PsoCI.PSODesc.Name                       = Name;
        PsoCI.pVS                                = sm_pVS;
        PsoCI.pPS                                = sm_pPS;
        PsoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PsoCI.GraphicsPipeline.NumRenderTargets  = 1;
        PsoCI.GraphicsPipeline.RTVFormats[0]     = TEX_FORMAT_RGBA8_UNORM;
        PsoCI.GraphicsPipeline.DSVFormat         = TEX_FORMAT_D32_FLOAT;
        return PsoCI;
    }

    static void TestCreatePSOFailure(GraphicsPipelineStateCreateInfo CI, const char* ExpectedErrorSubstring)
    {
        auto* pEnv    = GPUTestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        RefCntAutoPtr<IPipelineState> pPSO;
        pEnv->SetErrorAllowance(2, "Errors below are expected: testing specialization constants validation\n");
        pEnv->PushExpectedErrorSubstring(ExpectedErrorSubstring);
        pDevice->CreateGraphicsPipelineState(CI, &pPSO);
        ASSERT_FALSE(pPSO);

        CI.PSODesc.Name = nullptr;
        pEnv->SetErrorAllowance(2);
        pEnv->PushExpectedErrorSubstring(ExpectedErrorSubstring);
        pDevice->CreateGraphicsPipelineState(CI, &pPSO);
        ASSERT_FALSE(pPSO);

        pEnv->SetErrorAllowance(0);
    }

    static bool HasSpecializationConstants()
    {
        return GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo().Features.SpecializationConstants == DEVICE_FEATURE_STATE_ENABLED;
    }

    static RefCntAutoPtr<IShader> sm_pVS;
    static RefCntAutoPtr<IShader> sm_pPS;
};

RefCntAutoPtr<IShader> SpecializationConstantsTest::sm_pVS;
RefCntAutoPtr<IShader> SpecializationConstantsTest::sm_pPS;


TEST_F(SpecializationConstantsTest, NullPointerWithNonZeroCount)
{
    auto PsoCI                       = GetDefaultPSOCI("SpecConst - NullPointerWithNonZeroCount");
    PsoCI.NumSpecializationConstants = 1;
    PsoCI.pSpecializationConstants   = nullptr;
    TestCreatePSOFailure(PsoCI, "pSpecializationConstants is null");
}


TEST_F(SpecializationConstantsTest, FeatureDisabled)
{
    if (HasSpecializationConstants())
        GTEST_SKIP() << "Specialization constants are supported by this device";

    const float Data = 1.0f;

    SpecializationConstant SpecConsts[] = {
        {"Constant0", SHADER_TYPE_VERTEX, sizeof(Data), &Data},
    };

    auto PsoCI                       = GetDefaultPSOCI("SpecConst - FeatureDisabled");
    PsoCI.NumSpecializationConstants = _countof(SpecConsts);
    PsoCI.pSpecializationConstants   = SpecConsts;
    TestCreatePSOFailure(PsoCI, "SpecializationConstants device feature is not enabled");
}


TEST_F(SpecializationConstantsTest, NullName)
{
    if (!HasSpecializationConstants())
        GTEST_SKIP() << "Specialization constants are not supported by this device";

    const float Data = 1.0f;

    SpecializationConstant SpecConsts[] = {
        {nullptr, SHADER_TYPE_VERTEX, sizeof(Data), &Data},
    };

    auto PsoCI                       = GetDefaultPSOCI("SpecConst - NullName");
    PsoCI.NumSpecializationConstants = _countof(SpecConsts);
    PsoCI.pSpecializationConstants   = SpecConsts;
    TestCreatePSOFailure(PsoCI, "Name must not be null");
}


TEST_F(SpecializationConstantsTest, EmptyName)
{
    if (!HasSpecializationConstants())
        GTEST_SKIP() << "Specialization constants are not supported by this device";

    const float Data = 1.0f;

    SpecializationConstant SpecConsts[] = {
        {"", SHADER_TYPE_VERTEX, sizeof(Data), &Data},
    };

    auto PsoCI                       = GetDefaultPSOCI("SpecConst - EmptyName");
    PsoCI.NumSpecializationConstants = _countof(SpecConsts);
    PsoCI.pSpecializationConstants   = SpecConsts;
    TestCreatePSOFailure(PsoCI, "Name must not be empty");
}


TEST_F(SpecializationConstantsTest, UnknownShaderStages)
{
    if (!HasSpecializationConstants())
        GTEST_SKIP() << "Specialization constants are not supported by this device";

    const float Data = 1.0f;

    SpecializationConstant SpecConsts[] = {
        {"Constant0", SHADER_TYPE_UNKNOWN, sizeof(Data), &Data},
    };

    auto PsoCI                       = GetDefaultPSOCI("SpecConst - UnknownShaderStages");
    PsoCI.NumSpecializationConstants = _countof(SpecConsts);
    PsoCI.pSpecializationConstants   = SpecConsts;
    TestCreatePSOFailure(PsoCI, "ShaderStages must not be SHADER_TYPE_UNKNOWN");
}


TEST_F(SpecializationConstantsTest, ZeroSize)
{
    if (!HasSpecializationConstants())
        GTEST_SKIP() << "Specialization constants are not supported by this device";

    const float Data = 1.0f;

    SpecializationConstant SpecConsts[] = {
        {"Constant0", SHADER_TYPE_VERTEX, 0, &Data},
    };

    auto PsoCI                       = GetDefaultPSOCI("SpecConst - ZeroSize");
    PsoCI.NumSpecializationConstants = _countof(SpecConsts);
    PsoCI.pSpecializationConstants   = SpecConsts;
    TestCreatePSOFailure(PsoCI, "Size must not be zero");
}


TEST_F(SpecializationConstantsTest, NullData)
{
    if (!HasSpecializationConstants())
        GTEST_SKIP() << "Specialization constants are not supported by this device";

    SpecializationConstant SpecConsts[] = {
        {"Constant0", SHADER_TYPE_VERTEX, sizeof(float), nullptr},
    };

    auto PsoCI                       = GetDefaultPSOCI("SpecConst - NullData");
    PsoCI.NumSpecializationConstants = _countof(SpecConsts);
    PsoCI.pSpecializationConstants   = SpecConsts;
    TestCreatePSOFailure(PsoCI, "pData must not be null");
}


TEST_F(SpecializationConstantsTest, DuplicateNameOverlappingStages)
{
    if (!HasSpecializationConstants())
        GTEST_SKIP() << "Specialization constants are not supported by this device";

    const float Data = 1.0f;

    SpecializationConstant SpecConsts[] = {
        {"Constant0", SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, sizeof(Data), &Data},
        {"Constant0", SHADER_TYPE_VERTEX | SHADER_TYPE_GEOMETRY, sizeof(Data), &Data},
    };

    auto PsoCI                       = GetDefaultPSOCI("SpecConst - DuplicateNameOverlappingStages");
    PsoCI.NumSpecializationConstants = _countof(SpecConsts);
    PsoCI.pSpecializationConstants   = SpecConsts;
    TestCreatePSOFailure(PsoCI, "is defined in overlapping shader stages");
}


TEST_F(SpecializationConstantsTest, ErrorAtSecondElement)
{
    if (!HasSpecializationConstants())
        GTEST_SKIP() << "Specialization constants are not supported by this device";

    const float Data = 1.0f;

    // First element is valid, second has null name
    SpecializationConstant SpecConsts[] = {
        {"Constant0", SHADER_TYPE_VERTEX, sizeof(Data), &Data},
        {nullptr, SHADER_TYPE_PIXEL, sizeof(Data), &Data},
    };

    auto PsoCI                       = GetDefaultPSOCI("SpecConst - ErrorAtSecondElement");
    PsoCI.NumSpecializationConstants = _countof(SpecConsts);
    PsoCI.pSpecializationConstants   = SpecConsts;
    TestCreatePSOFailure(PsoCI, "pSpecializationConstants[1].Name must not be null");
}


} // namespace
