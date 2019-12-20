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

#include "PSOTestBase.h"
#include "TestingEnvironment.h"

namespace Diligent
{

namespace Testing
{

PSOTestBase::Resources PSOTestBase::sm_Resources;

static const char g_ShaderSource[] = R"(
void VSMain(out float4 pos : SV_POSITION)
{
	pos = float4(0.0, 0.0, 0.0, 0.0);
}

void PSMain(out float4 col : SV_TARGET)
{
	col = float4(0.0, 0.0, 0.0, 0.0);
}
)";

void PSOTestBase::InitResources()
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    ShaderCreateInfo Attrs;
    Attrs.Source                     = g_ShaderSource;
    Attrs.EntryPoint                 = "VSMain";
    Attrs.Desc.ShaderType            = SHADER_TYPE_VERTEX;
    Attrs.Desc.Name                  = "TrivialVS (TestPipelineStateBase)";
    Attrs.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    Attrs.UseCombinedTextureSamplers = true;
    pDevice->CreateShader(Attrs, &sm_Resources.pTrivialVS);

    Attrs.EntryPoint      = "PSMain";
    Attrs.Desc.ShaderType = SHADER_TYPE_PIXEL;
    Attrs.Desc.Name       = "TrivialPS (TestPipelineStateBase)";
    pDevice->CreateShader(Attrs, &sm_Resources.pTrivialPS);

    auto& PSODesc = sm_Resources.PSODesc;

    PSODesc.GraphicsPipeline.pVS               = sm_Resources.pTrivialVS;
    PSODesc.GraphicsPipeline.pPS               = sm_Resources.pTrivialPS;
    PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSODesc.GraphicsPipeline.NumRenderTargets  = 1;
    PSODesc.GraphicsPipeline.RTVFormats[0]     = TEX_FORMAT_RGBA8_UNORM;
    PSODesc.GraphicsPipeline.DSVFormat         = TEX_FORMAT_D32_FLOAT;
}

void PSOTestBase::ReleaseResources()
{
    sm_Resources = Resources{};
    TestingEnvironment::GetInstance()->Reset();
}

RefCntAutoPtr<IPipelineState> PSOTestBase::CreateTestPSO(const PipelineStateDesc& PSODesc, bool BindPSO)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    RefCntAutoPtr<IPipelineState> pPSO;
    pDevice->CreatePipelineState(PSODesc, &pPSO);
    if (BindPSO && pPSO)
    {
        pContext->SetPipelineState(pPSO);
    }
    return pPSO;
}

} // namespace Testing

} // namespace Diligent
