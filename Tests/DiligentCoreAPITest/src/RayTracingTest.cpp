/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include <algorithm>

#include "TestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"
#include "BasicMath.hpp"

#include "gtest/gtest.h"

#include "InlineShaders/RayTracingTestHLSL.h"

namespace Diligent
{

namespace Testing
{

#if D3D12_SUPPORTED
void RayTracingTriangleClosestHitReferenceD3D12(ISwapChain* pSwapChain);
void RayTracingTriangleAnyHitReferenceD3D12(ISwapChain* pSwapChain);
void RayTracingProceduralIntersectionReferenceD3D12(ISwapChain* pSwapChain);
#endif

#if VULKAN_SUPPORTED
void RayTracingTriangleClosestHitReferenceVk(ISwapChain* pSwapChain);
void RayTracingTriangleAnyHitReferenceVk(ISwapChain* pSwapChain);
void RayTracingProceduralIntersectionReferenceVk(ISwapChain* pSwapChain);
#endif

} // namespace Testing
} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

void CreateBLAS(IRenderDevice* pDevice, IDeviceContext* pContext, const BLASBuildTriangleData* pTriangles, Uint32 TriangleCount, RefCntAutoPtr<IBottomLevelAS>& pBLAS)
{
    // create
    std::vector<BLASTriangleDesc> TriangleInfos;
    TriangleInfos.resize(TriangleCount);
    for (Uint32 i = 0; i < TriangleCount; ++i)
    {
        auto& src = pTriangles[i];
        auto& dst = TriangleInfos[i];

        dst.GeometryName         = src.GeometryName;
        dst.MaxVertexCount       = src.VertexCount;
        dst.VertexValueType      = src.VertexValueType;
        dst.VertexComponentCount = src.VertexComponentCount;
        dst.MaxIndexCount        = src.IndexCount;
        dst.IndexType            = src.IndexType;
    }

    BottomLevelASDesc ASDesc;
    ASDesc.Name          = "Triangle BLAS";
    ASDesc.pTriangles    = TriangleInfos.data();
    ASDesc.TriangleCount = TriangleCount;

    pDevice->CreateBLAS(ASDesc, &pBLAS);
    VERIFY_EXPR(pBLAS != nullptr);

    // create scratch buffer
    RefCntAutoPtr<IBuffer> ScratchBuffer;

    BufferDesc BuffDesc;
    BuffDesc.Name          = "BLAS Scratch Buffer";
    BuffDesc.Usage         = USAGE_DEFAULT;
    BuffDesc.BindFlags     = BIND_RAY_TRACING;
    BuffDesc.uiSizeInBytes = pBLAS->GetScratchBufferSizes().Build;

    pDevice->CreateBuffer(BuffDesc, nullptr, &ScratchBuffer);
    VERIFY_EXPR(ScratchBuffer != nullptr);

    // build
    BLASBuildAttribs Attribs;
    Attribs.pBLAS                       = pBLAS;
    Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.pTriangleData               = pTriangles;
    Attribs.TriangleDataCount           = TriangleCount;
    Attribs.pScratchBuffer              = ScratchBuffer;
    Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->BuildBLAS(Attribs);
}

void CreateBLAS(IRenderDevice* pDevice, IDeviceContext* pContext, const BLASBuildBoundingBoxData* pBoxes, Uint32 BoxCount, RefCntAutoPtr<IBottomLevelAS>& pBLAS)
{
    // create
    std::vector<BLASBoundingBoxDesc> BoxInfos;
    BoxInfos.resize(BoxCount);
    for (Uint32 i = 0; i < BoxCount; ++i)
    {
        auto& src = pBoxes[i];
        auto& dst = BoxInfos[i];

        dst.GeometryName = src.GeometryName;
        dst.MaxBoxCount  = src.BoxCount;
    }

    BottomLevelASDesc ASDesc;
    ASDesc.Name     = "Boxes BLAS";
    ASDesc.pBoxes   = BoxInfos.data();
    ASDesc.BoxCount = BoxCount;

    pDevice->CreateBLAS(ASDesc, &pBLAS);
    VERIFY_EXPR(pBLAS != nullptr);

    // create scratch buffer
    RefCntAutoPtr<IBuffer> ScratchBuffer;

    BufferDesc BuffDesc;
    BuffDesc.Name          = "BLAS Scratch Buffer";
    BuffDesc.Usage         = USAGE_DEFAULT;
    BuffDesc.BindFlags     = BIND_RAY_TRACING;
    BuffDesc.uiSizeInBytes = pBLAS->GetScratchBufferSizes().Build;

    pDevice->CreateBuffer(BuffDesc, nullptr, &ScratchBuffer);
    VERIFY_EXPR(ScratchBuffer != nullptr);

    // build
    BLASBuildAttribs Attribs;
    Attribs.pBLAS                       = pBLAS;
    Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.pBoxData                    = pBoxes;
    Attribs.BoxDataCount                = BoxCount;
    Attribs.pScratchBuffer              = ScratchBuffer;
    Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->BuildBLAS(Attribs);
}

void CreateTLAS(IRenderDevice* pDevice, IDeviceContext* pContext, const TLASBuildInstanceData* Instances, Uint32 InstanceCount, RefCntAutoPtr<ITopLevelAS>& pTLAS)
{
    // create
    TopLevelASDesc TLASDesc;
    TLASDesc.Name             = "TLAS";
    TLASDesc.MaxInstanceCount = InstanceCount;
    TLASDesc.Flags            = RAYTRACING_BUILD_AS_NONE;
    TLASDesc.BindingMode      = SHADER_BINDING_MODE_PER_GEOMETRY;

    pDevice->CreateTLAS(TLASDesc, &pTLAS);
    VERIFY_EXPR(pTLAS != nullptr);

    // create scratch buffer
    RefCntAutoPtr<IBuffer> ScratchBuffer;

    BufferDesc BuffDesc;
    BuffDesc.Name          = "TLAS Scratch Buffer";
    BuffDesc.Usage         = USAGE_DEFAULT;
    BuffDesc.BindFlags     = BIND_RAY_TRACING;
    BuffDesc.uiSizeInBytes = pTLAS->GetScratchBufferSizes().Build;

    pDevice->CreateBuffer(BuffDesc, nullptr, &ScratchBuffer);
    VERIFY_EXPR(ScratchBuffer != nullptr);

    // create instance buffer
    RefCntAutoPtr<IBuffer> InstanceBuffer;

    BuffDesc.Name          = "TLAS Instance Buffer";
    BuffDesc.Usage         = USAGE_DEFAULT;
    BuffDesc.BindFlags     = BIND_RAY_TRACING;
    BuffDesc.uiSizeInBytes = TLAS_INSTANCE_DATA_SIZE * InstanceCount;

    pDevice->CreateBuffer(BuffDesc, nullptr, &InstanceBuffer);
    VERIFY_EXPR(InstanceBuffer != nullptr);

    // build
    TLASBuildAttribs Attribs;
    Attribs.pTLAS                        = pTLAS;
    Attribs.pInstances                   = Instances;
    Attribs.InstanceCount                = InstanceCount;
    Attribs.HitShadersPerInstance        = 1;
    Attribs.TLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.BLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.pInstanceBuffer              = InstanceBuffer;
    Attribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.pScratchBuffer               = ScratchBuffer;
    Attribs.ScratchBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->BuildTLAS(Attribs);
}


TEST(RayTracingTest, TriangleClosestHitShader)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceCaps().Features.RayTracing)
    {
        GTEST_SKIP() << "Ray tracing is not supported by this device";
    }

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
    if (pTestingSwapChain)
    {
        pContext->Flush();
        pContext->InvalidateState();

        auto deviceType = pDevice->GetDeviceCaps().DevType;
        switch (deviceType)
        {
#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
                RayTracingTriangleClosestHitReferenceD3D12(pSwapChain);
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                RayTracingTriangleClosestHitReferenceVk(pSwapChain);
                break;
#endif

            default:
                LOG_ERROR_AND_THROW("Unsupported device type");
        }

        pTestingSwapChain->TakeSnapshot();
    }
    TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

    RayTracingPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Ray tracing PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_RAY_TRACING;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;

    // Create ray generation shader.
    RefCntAutoPtr<IShader> pRG;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ray tracing RG";
        ShaderCI.Source          = HLSL::RayTracingTest1_RG.c_str();
        pDevice->CreateShader(ShaderCI, &pRG);
        VERIFY_EXPR(pRG != nullptr);
    }

    // Create ray miss shader.
    RefCntAutoPtr<IShader> pRMiss;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Miss shader";
        ShaderCI.Source          = HLSL::RayTracingTest1_RM.c_str();
        pDevice->CreateShader(ShaderCI, &pRMiss);
        VERIFY_EXPR(pRMiss != nullptr);
    }

    // Create ray closest hit shader.
    RefCntAutoPtr<IShader> pClosestHit;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ray closest hit shader";
        ShaderCI.Source          = HLSL::RayTracingTest1_RCH.c_str();
        pDevice->CreateShader(ShaderCI, &pClosestHit);
        VERIFY_EXPR(pClosestHit != nullptr);
    }

    const RayTracingGeneralShaderGroup     GeneralShaders[]     = {{"Main", pRG}, {"Miss", pRMiss}};
    const RayTracingTriangleHitShaderGroup TriangleHitShaders[] = {{"HitGroup", pClosestHit}};

    PSOCreateInfo.pGeneralShaders        = GeneralShaders;
    PSOCreateInfo.GeneralShaderCount     = _countof(GeneralShaders);
    PSOCreateInfo.pTriangleHitShaders    = TriangleHitShaders;
    PSOCreateInfo.TriangleHitShaderCount = _countof(TriangleHitShaders);

    PSOCreateInfo.RayTracingPipeline.MaxRecursionDepth       = 0;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    RefCntAutoPtr<IPipelineState> pRayTracingPSO;
    pDevice->CreateRayTracingPipelineState(PSOCreateInfo, &pRayTracingPSO);
    VERIFY_EXPR(pRayTracingPSO != nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pRayTracingSRB;
    pRayTracingPSO->CreateShaderResourceBinding(&pRayTracingSRB, true);
    VERIFY_EXPR(pRayTracingSRB != nullptr);

    const float3 Vertices[] = {
        float3{0.25f, 0.25f, 0.0f},
        float3{0.75f, 0.25f, 0.0f},
        float3{0.50f, 0.75f, 0.0f}};

    RefCntAutoPtr<IBuffer> pVertexBuffer;
    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Triangle vertices";
        BuffDesc.Usage         = USAGE_IMMUTABLE;
        BuffDesc.BindFlags     = BIND_RAY_TRACING;
        BuffDesc.uiSizeInBytes = sizeof(Vertices);

        BufferData BufData;
        BufData.pData    = Vertices;
        BufData.DataSize = sizeof(Vertices);

        pDevice->CreateBuffer(BuffDesc, &BufData, &pVertexBuffer);
        VERIFY_EXPR(pVertexBuffer != nullptr);
    }

    BLASBuildTriangleData Triangle;
    Triangle.GeometryName         = "Triangle";
    Triangle.pVertexBuffer        = pVertexBuffer;
    Triangle.VertexStride         = sizeof(Vertices[0]);
    Triangle.VertexCount          = _countof(Vertices);
    Triangle.VertexValueType      = VT_FLOAT32;
    Triangle.VertexComponentCount = 3;
    Triangle.Flags                = RAYTRACING_GEOMETRY_OPAQUE;

    RefCntAutoPtr<IBottomLevelAS> pBLAS;
    CreateBLAS(pDevice, pContext, &Triangle, 1, pBLAS);

    TLASBuildInstanceData Instance;
    Instance.InstanceName                = "Instance";
    Instance.pBLAS                       = pBLAS;
    Instance.CustomId                    = 0;
    Instance.Flags                       = RAYTRACING_INSTANCE_NONE;
    Instance.Mask                        = 0xFF;
    Instance.ContributionToHitGroupIndex = 0;
    Instance.Transform[0][0]             = 1.0f;
    Instance.Transform[1][1]             = 1.0f;
    Instance.Transform[2][2]             = 1.0f;

    RefCntAutoPtr<ITopLevelAS> pTLAS;
    CreateTLAS(pDevice, pContext, &Instance, 1, pTLAS);

    ShaderBindingTableDesc SBTDesc;
    SBTDesc.Name                  = "SBT";
    SBTDesc.pPSO                  = pRayTracingPSO;
    SBTDesc.ShaderRecordSize      = 0;
    SBTDesc.HitShadersPerInstance = 1;

    RefCntAutoPtr<IShaderBindingTable> pSBT;
    pDevice->CreateSBT(SBTDesc, &pSBT);
    VERIFY_EXPR(pSBT != nullptr);

    pSBT->BindRayGenShader("Main");
    pSBT->BindMissShader("Miss", 0);
    pSBT->BindHitGroup(pTLAS, "Instance", "Triangle", 0, "HitGroup");

    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_TLAS")->Set(pTLAS);
    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_ColorBuffer")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

    pContext->SetPipelineState(pRayTracingPSO);
    pContext->CommitShaderResources(pRayTracingSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const auto& SCDesc = pSwapChain->GetDesc();

    TraceRaysAttribs Attribs;
    Attribs.DimensionX     = SCDesc.Width;
    Attribs.DimensionY     = SCDesc.Height;
    Attribs.pSBT           = pSBT;
    Attribs.TransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->TraceRays(Attribs);

    pSwapChain->Present();
}


TEST(RayTracingTest, TriangleAnyHitShader)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceCaps().Features.RayTracing)
    {
        GTEST_SKIP() << "Ray tracing is not supported by this device";
    }

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
    if (pTestingSwapChain)
    {
        pContext->Flush();
        pContext->InvalidateState();

        auto deviceType = pDevice->GetDeviceCaps().DevType;
        switch (deviceType)
        {
#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
                RayTracingTriangleAnyHitReferenceD3D12(pSwapChain);
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                RayTracingTriangleAnyHitReferenceVk(pSwapChain);
                break;
#endif

            default:
                LOG_ERROR_AND_THROW("Unsupported device type");
        }

        pTestingSwapChain->TakeSnapshot();
    }
    TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

    RayTracingPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Ray tracing PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_RAY_TRACING;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;

    // Create ray generation shader.
    RefCntAutoPtr<IShader> pRG;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ray tracing RG";
        ShaderCI.Source          = HLSL::RayTracingTest2_RG.c_str();
        pDevice->CreateShader(ShaderCI, &pRG);
        VERIFY_EXPR(pRG != nullptr);
    }

    // Create ray miss shader.
    RefCntAutoPtr<IShader> pRMiss;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Miss shader";
        ShaderCI.Source          = HLSL::RayTracingTest2_RM.c_str();
        pDevice->CreateShader(ShaderCI, &pRMiss);
        VERIFY_EXPR(pRMiss != nullptr);
    }

    // Create ray closest hit shader.
    RefCntAutoPtr<IShader> pClosestHit;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ray closest hit shader";
        ShaderCI.Source          = HLSL::RayTracingTest2_RCH.c_str();
        pDevice->CreateShader(ShaderCI, &pClosestHit);
        VERIFY_EXPR(pClosestHit != nullptr);
    }

    // Create ray any hit shader.
    RefCntAutoPtr<IShader> pAnyHit;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_ANY_HIT;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ray any hit shader";
        ShaderCI.Source          = HLSL::RayTracingTest2_RAH.c_str();
        pDevice->CreateShader(ShaderCI, &pAnyHit);
        VERIFY_EXPR(pAnyHit != nullptr);
    }

    const RayTracingGeneralShaderGroup     GeneralShaders[]     = {{"Main", pRG}, {"Miss", pRMiss}};
    const RayTracingTriangleHitShaderGroup TriangleHitShaders[] = {{"HitGroup", pClosestHit, pAnyHit}};

    PSOCreateInfo.pGeneralShaders        = GeneralShaders;
    PSOCreateInfo.GeneralShaderCount     = _countof(GeneralShaders);
    PSOCreateInfo.pTriangleHitShaders    = TriangleHitShaders;
    PSOCreateInfo.TriangleHitShaderCount = _countof(TriangleHitShaders);

    PSOCreateInfo.RayTracingPipeline.MaxRecursionDepth       = 0;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    RefCntAutoPtr<IPipelineState> pRayTracingPSO;
    pDevice->CreateRayTracingPipelineState(PSOCreateInfo, &pRayTracingPSO);
    VERIFY_EXPR(pRayTracingPSO != nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pRayTracingSRB;
    pRayTracingPSO->CreateShaderResourceBinding(&pRayTracingSRB, true);
    VERIFY_EXPR(pRayTracingSRB != nullptr);

    const float3 Vertices[] = {
        float3{0.25f, 0.25f, 0.0f}, float3{0.75f, 0.25f, 0.0f}, float3{0.50f, 0.75f, 0.0f},
        float3{0.50f, 0.10f, 0.1f}, float3{0.90f, 0.90f, 0.1f}, float3{0.10f, 0.90f, 0.1f},
        float3{0.40f, 1.00f, 0.2f}, float3{0.20f, 0.40f, 0.2f}, float3{1.00f, 0.70f, 0.2f}};

    RefCntAutoPtr<IBuffer> pVertexBuffer;
    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Triangle vertices";
        BuffDesc.Usage         = USAGE_IMMUTABLE;
        BuffDesc.BindFlags     = BIND_RAY_TRACING;
        BuffDesc.uiSizeInBytes = sizeof(Vertices);

        BufferData BufData;
        BufData.pData    = Vertices;
        BufData.DataSize = sizeof(Vertices);

        pDevice->CreateBuffer(BuffDesc, &BufData, &pVertexBuffer);
        VERIFY_EXPR(pVertexBuffer != nullptr);
    }

    BLASBuildTriangleData Triangle;
    Triangle.GeometryName         = "Triangle";
    Triangle.pVertexBuffer        = pVertexBuffer;
    Triangle.VertexStride         = sizeof(Vertices[0]);
    Triangle.VertexCount          = _countof(Vertices);
    Triangle.VertexValueType      = VT_FLOAT32;
    Triangle.VertexComponentCount = 3;
    Triangle.Flags                = RAYTRACING_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION;

    RefCntAutoPtr<IBottomLevelAS> pBLAS;
    CreateBLAS(pDevice, pContext, &Triangle, 1, pBLAS);

    TLASBuildInstanceData Instance;
    Instance.InstanceName                = "Instance";
    Instance.pBLAS                       = pBLAS;
    Instance.CustomId                    = 0;
    Instance.Flags                       = RAYTRACING_INSTANCE_NONE;
    Instance.Mask                        = 0xFF;
    Instance.ContributionToHitGroupIndex = 0;
    Instance.Transform[0][0]             = 1.0f;
    Instance.Transform[1][1]             = 1.0f;
    Instance.Transform[2][2]             = 1.0f;

    RefCntAutoPtr<ITopLevelAS> pTLAS;
    CreateTLAS(pDevice, pContext, &Instance, 1, pTLAS);

    ShaderBindingTableDesc SBTDesc;
    SBTDesc.Name                  = "SBT";
    SBTDesc.pPSO                  = pRayTracingPSO;
    SBTDesc.ShaderRecordSize      = 0;
    SBTDesc.HitShadersPerInstance = 1;

    RefCntAutoPtr<IShaderBindingTable> pSBT;
    pDevice->CreateSBT(SBTDesc, &pSBT);
    VERIFY_EXPR(pSBT != nullptr);

    pSBT->BindRayGenShader("Main");
    pSBT->BindMissShader("Miss", 0);
    pSBT->BindHitGroup(pTLAS, "Instance", "Triangle", 0, "HitGroup");

    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_TLAS")->Set(pTLAS);
    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_ColorBuffer")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

    pContext->SetPipelineState(pRayTracingPSO);
    pContext->CommitShaderResources(pRayTracingSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const auto& SCDesc = pSwapChain->GetDesc();

    TraceRaysAttribs Attribs;
    Attribs.DimensionX     = SCDesc.Width;
    Attribs.DimensionY     = SCDesc.Height;
    Attribs.pSBT           = pSBT;
    Attribs.TransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->TraceRays(Attribs);

    pSwapChain->Present();
}


TEST(RayTracingTest, ProceduralIntersection)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();
    if (!pDevice->GetDeviceCaps().Features.RayTracing)
    {
        GTEST_SKIP() << "Ray tracing is not supported by this device";
    }

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);
    if (pTestingSwapChain)
    {
        pContext->Flush();
        pContext->InvalidateState();

        auto deviceType = pDevice->GetDeviceCaps().DevType;
        switch (deviceType)
        {
#if D3D12_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D12:
                RayTracingProceduralIntersectionReferenceD3D12(pSwapChain);
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                RayTracingProceduralIntersectionReferenceVk(pSwapChain);
                break;
#endif

            default:
                LOG_ERROR_AND_THROW("Unsupported device type");
        }

        pTestingSwapChain->TakeSnapshot();
    }
    TestingEnvironment::ScopedReleaseResources EnvironmentAutoReset;

    RayTracingPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Ray tracing PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_RAY_TRACING;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;

    // Create ray generation shader.
    RefCntAutoPtr<IShader> pRG;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ray tracing RG";
        ShaderCI.Source          = HLSL::RayTracingTest3_RG.c_str();
        pDevice->CreateShader(ShaderCI, &pRG);
        VERIFY_EXPR(pRG != nullptr);
    }

    // Create ray miss shader.
    RefCntAutoPtr<IShader> pRMiss;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Miss shader";
        ShaderCI.Source          = HLSL::RayTracingTest3_RM.c_str();
        pDevice->CreateShader(ShaderCI, &pRMiss);
        VERIFY_EXPR(pRMiss != nullptr);
    }

    // Create ray closest hit shader.
    RefCntAutoPtr<IShader> pClosestHit;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ray closest hit shader";
        ShaderCI.Source          = HLSL::RayTracingTest3_RCH.c_str();
        pDevice->CreateShader(ShaderCI, &pClosestHit);
        VERIFY_EXPR(pClosestHit != nullptr);
    }

    // Create ray intersection shader.
    RefCntAutoPtr<IShader> pIntersection;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_INTERSECTION;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ray intersection shader";
        ShaderCI.Source          = HLSL::RayTracingTest3_RI.c_str();
        pDevice->CreateShader(ShaderCI, &pIntersection);
        VERIFY_EXPR(pIntersection != nullptr);
    }

    const RayTracingGeneralShaderGroup       GeneralShaders[]       = {{"Main", pRG}, {"Miss", pRMiss}};
    const RayTracingProceduralHitShaderGroup ProceduralHitShaders[] = {{"HitGroup", pIntersection, pClosestHit}};

    PSOCreateInfo.pGeneralShaders          = GeneralShaders;
    PSOCreateInfo.GeneralShaderCount       = _countof(GeneralShaders);
    PSOCreateInfo.pProceduralHitShaders    = ProceduralHitShaders;
    PSOCreateInfo.ProceduralHitShaderCount = _countof(ProceduralHitShaders);

    PSOCreateInfo.RayTracingPipeline.MaxRecursionDepth       = 0;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    RefCntAutoPtr<IPipelineState> pRayTracingPSO;
    pDevice->CreateRayTracingPipelineState(PSOCreateInfo, &pRayTracingPSO);
    VERIFY_EXPR(pRayTracingPSO != nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pRayTracingSRB;
    pRayTracingPSO->CreateShaderResourceBinding(&pRayTracingSRB, true);
    VERIFY_EXPR(pRayTracingSRB != nullptr);

    const float3 Boxes[] = {
        float3{0.25f, 0.5f, 2.0f} - float3{1.0f, 1.0f, 1.0f},
        float3{0.25f, 0.5f, 2.0f} + float3{1.0f, 1.0f, 1.0f}};

    RefCntAutoPtr<IBuffer> pBoxBuffer;
    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Boxes";
        BuffDesc.Usage         = USAGE_IMMUTABLE;
        BuffDesc.BindFlags     = BIND_RAY_TRACING;
        BuffDesc.uiSizeInBytes = sizeof(Boxes);

        BufferData BufData;
        BufData.pData    = Boxes;
        BufData.DataSize = sizeof(Boxes);

        pDevice->CreateBuffer(BuffDesc, &BufData, &pBoxBuffer);
        VERIFY_EXPR(pBoxBuffer != nullptr);
    }

    BLASBuildBoundingBoxData Box;
    Box.GeometryName = "Sphere";
    Box.pBoxBuffer   = pBoxBuffer;
    Box.BoxCount     = _countof(Boxes) / 2;
    Box.BoxStride    = sizeof(float3) * 2;
    Box.Flags        = RAYTRACING_GEOMETRY_OPAQUE;

    RefCntAutoPtr<IBottomLevelAS> pBLAS;
    CreateBLAS(pDevice, pContext, &Box, 1, pBLAS);

    TLASBuildInstanceData Instance;
    Instance.InstanceName                = "Instance";
    Instance.pBLAS                       = pBLAS;
    Instance.CustomId                    = 0;
    Instance.Flags                       = RAYTRACING_INSTANCE_NONE;
    Instance.Mask                        = 0xFF;
    Instance.ContributionToHitGroupIndex = 0;
    Instance.Transform[0][0]             = 1.0f;
    Instance.Transform[1][1]             = 1.0f;
    Instance.Transform[2][2]             = 1.0f;

    RefCntAutoPtr<ITopLevelAS> pTLAS;
    CreateTLAS(pDevice, pContext, &Instance, 1, pTLAS);

    ShaderBindingTableDesc SBTDesc;
    SBTDesc.Name                  = "SBT";
    SBTDesc.pPSO                  = pRayTracingPSO;
    SBTDesc.ShaderRecordSize      = 0;
    SBTDesc.HitShadersPerInstance = 1;

    RefCntAutoPtr<IShaderBindingTable> pSBT;
    pDevice->CreateSBT(SBTDesc, &pSBT);
    VERIFY_EXPR(pSBT != nullptr);

    pSBT->BindRayGenShader("Main");
    pSBT->BindMissShader("Miss", 0);
    pSBT->BindHitGroup(pTLAS, "Instance", "Sphere", 0, "HitGroup");

    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_TLAS")->Set(pTLAS);
    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_ColorBuffer")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

    pContext->SetPipelineState(pRayTracingPSO);
    pContext->CommitShaderResources(pRayTracingSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const auto& SCDesc = pSwapChain->GetDesc();

    TraceRaysAttribs Attribs;
    Attribs.DimensionX     = SCDesc.Width;
    Attribs.DimensionY     = SCDesc.Height;
    Attribs.pSBT           = pSBT;
    Attribs.TransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->TraceRays(Attribs);

    pSwapChain->Present();
}

} // namespace
