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
#include <random>
#include <unordered_map>

#include "TestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"
#include "BasicMath.hpp"

#include "gtest/gtest.h"

#include "InlineShaders/RayTracingTestHLSL.h"
#include "RayTracingTestConstants.hpp"

namespace Diligent
{

namespace Testing
{

#if D3D12_SUPPORTED
void RayTracingTriangleClosestHitReferenceD3D12(ISwapChain* pSwapChain);
void RayTracingTriangleAnyHitReferenceD3D12(ISwapChain* pSwapChain);
void RayTracingProceduralIntersectionReferenceD3D12(ISwapChain* pSwapChain);
void RayTracingMultiGeometryReferenceD3D12(ISwapChain* pSwapChain);
#endif

#if VULKAN_SUPPORTED
void RayTracingTriangleClosestHitReferenceVk(ISwapChain* pSwapChain);
void RayTracingTriangleAnyHitReferenceVk(ISwapChain* pSwapChain);
void RayTracingProceduralIntersectionReferenceVk(ISwapChain* pSwapChain);
void RayTracingMultiGeometryReferenceVk(ISwapChain* pSwapChain);
#endif

} // namespace Testing

} // namespace Diligent

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

template <typename It>
void Shuffle(It first, It last)
{
    std::random_device rd;
    std::mt19937       g{rd()};

    std::shuffle(first, last, g);
}

void CreateBLAS(IRenderDevice* pDevice, IDeviceContext* pContext, BLASBuildTriangleData* pTriangles, Uint32 TriangleCount, bool Update, RefCntAutoPtr<IBottomLevelAS>& pBLAS)
{
    // Create BLAS for triangles
    std::vector<BLASTriangleDesc> TriangleInfos;
    TriangleInfos.resize(TriangleCount + 1);
    for (Uint32 i = 0; i < TriangleCount; ++i)
    {
        auto& src = pTriangles[i];
        auto& dst = TriangleInfos[i];

        if (src.PrimitiveCount == 0)
            src.PrimitiveCount = src.VertexCount / 3;

        dst.GeometryName         = src.GeometryName;
        dst.MaxVertexCount       = src.VertexCount;
        dst.VertexValueType      = src.VertexValueType;
        dst.VertexComponentCount = src.VertexComponentCount;
        dst.MaxPrimitiveCount    = src.PrimitiveCount;
        dst.IndexType            = src.IndexType;
    }

    // add unused geometry for tests
    {
        auto& tri                = TriangleInfos[TriangleCount];
        tri.GeometryName         = "Unused geometry";
        tri.MaxVertexCount       = 40;
        tri.VertexValueType      = VT_FLOAT32;
        tri.VertexComponentCount = 3;
        tri.MaxPrimitiveCount    = 80;
        tri.IndexType            = VT_UINT32;
    }

    Shuffle(TriangleInfos.begin(), TriangleInfos.end());

    BottomLevelASDesc ASDesc;
    ASDesc.Name          = "Triangle BLAS";
    ASDesc.Flags         = RAYTRACING_BUILD_AS_ALLOW_COMPACTION | (Update ? RAYTRACING_BUILD_AS_ALLOW_UPDATE : RAYTRACING_BUILD_AS_NONE);
    ASDesc.pTriangles    = TriangleInfos.data();
    ASDesc.TriangleCount = static_cast<Uint32>(TriangleInfos.size());

    pDevice->CreateBLAS(ASDesc, &pBLAS);
    VERIFY_EXPR(pBLAS != nullptr);

    // Create scratch buffer
    RefCntAutoPtr<IBuffer> ScratchBuffer;

    BufferDesc BuffDesc;
    BuffDesc.Name          = "BLAS Scratch Buffer";
    BuffDesc.Usage         = USAGE_DEFAULT;
    BuffDesc.BindFlags     = BIND_RAY_TRACING;
    BuffDesc.uiSizeInBytes = std::max(pBLAS->GetScratchBufferSizes().Build, pBLAS->GetScratchBufferSizes().Update);

    pDevice->CreateBuffer(BuffDesc, nullptr, &ScratchBuffer);
    VERIFY_EXPR(ScratchBuffer != nullptr);

    // Build
    BuildBLASAttribs Attribs;
    Attribs.pBLAS                       = pBLAS;
    Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.pTriangleData               = pTriangles;
    Attribs.TriangleDataCount           = TriangleCount;
    Attribs.pScratchBuffer              = ScratchBuffer;
    Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->BuildBLAS(Attribs);

    if (Update)
    {
        Shuffle(pTriangles, pTriangles + TriangleCount);

        Attribs.Update = true;
        pContext->BuildBLAS(Attribs);
    }
}

void CreateBLAS(IRenderDevice* pDevice, IDeviceContext* pContext, BLASBuildBoundingBoxData* pBoxes, Uint32 BoxCount, bool Update, RefCntAutoPtr<IBottomLevelAS>& pBLAS)
{
    // Create BLAS for boxes
    std::vector<BLASBoundingBoxDesc> BoxInfos;
    BoxInfos.resize(BoxCount);
    for (Uint32 i = 0; i < BoxCount; ++i)
    {
        auto& src = pBoxes[i];
        auto& dst = BoxInfos[i];

        dst.GeometryName = src.GeometryName;
        dst.MaxBoxCount  = src.BoxCount;
    }

    Shuffle(BoxInfos.begin(), BoxInfos.end());

    BottomLevelASDesc ASDesc;
    ASDesc.Name     = "Boxes BLAS";
    ASDesc.Flags    = RAYTRACING_BUILD_AS_ALLOW_COMPACTION | (Update ? RAYTRACING_BUILD_AS_ALLOW_UPDATE : RAYTRACING_BUILD_AS_NONE);
    ASDesc.pBoxes   = BoxInfos.data();
    ASDesc.BoxCount = static_cast<Uint32>(BoxInfos.size());

    pDevice->CreateBLAS(ASDesc, &pBLAS);
    VERIFY_EXPR(pBLAS != nullptr);

    // Create scratch buffer
    RefCntAutoPtr<IBuffer> ScratchBuffer;

    BufferDesc BuffDesc;
    BuffDesc.Name          = "BLAS Scratch Buffer";
    BuffDesc.Usage         = USAGE_DEFAULT;
    BuffDesc.BindFlags     = BIND_RAY_TRACING;
    BuffDesc.uiSizeInBytes = std::max(pBLAS->GetScratchBufferSizes().Build, pBLAS->GetScratchBufferSizes().Update);

    pDevice->CreateBuffer(BuffDesc, nullptr, &ScratchBuffer);
    VERIFY_EXPR(ScratchBuffer != nullptr);

    // Build
    BuildBLASAttribs Attribs;
    Attribs.pBLAS                       = pBLAS;
    Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.pBoxData                    = pBoxes;
    Attribs.BoxDataCount                = BoxCount;
    Attribs.pScratchBuffer              = ScratchBuffer;
    Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->BuildBLAS(Attribs);

    if (Update)
    {
        Shuffle(pBoxes, pBoxes + BoxCount);

        Attribs.Update = true;
        pContext->BuildBLAS(Attribs);
    }
}

void CreateTLAS(IRenderDevice* pDevice, IDeviceContext* pContext, TLASBuildInstanceData* pInstances, Uint32 InstanceCount, Uint32 HitShadersPerInstance, bool Update, RefCntAutoPtr<ITopLevelAS>& pTLAS)
{
    // Create TLAS
    TopLevelASDesc TLASDesc;
    TLASDesc.Name             = "TLAS";
    TLASDesc.MaxInstanceCount = InstanceCount;
    TLASDesc.Flags            = RAYTRACING_BUILD_AS_ALLOW_COMPACTION | (Update ? RAYTRACING_BUILD_AS_ALLOW_UPDATE : RAYTRACING_BUILD_AS_NONE);

    pDevice->CreateTLAS(TLASDesc, &pTLAS);
    VERIFY_EXPR(pTLAS != nullptr);

    // Create scratch buffer
    RefCntAutoPtr<IBuffer> ScratchBuffer;

    BufferDesc BuffDesc;
    BuffDesc.Name          = "TLAS Scratch Buffer";
    BuffDesc.Usage         = USAGE_DEFAULT;
    BuffDesc.BindFlags     = BIND_RAY_TRACING;
    BuffDesc.uiSizeInBytes = std::max(pTLAS->GetScratchBufferSizes().Build, pTLAS->GetScratchBufferSizes().Update);

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

    Shuffle(pInstances, pInstances + InstanceCount);

    // Build
    BuildTLASAttribs Attribs;
    Attribs.pTLAS                        = pTLAS;
    Attribs.pInstances                   = pInstances;
    Attribs.InstanceCount                = InstanceCount;
    Attribs.HitShadersPerInstance        = HitShadersPerInstance;
    Attribs.BindingMode                  = SHADER_BINDING_MODE_PER_GEOMETRY;
    Attribs.TLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.BLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.pInstanceBuffer              = InstanceBuffer;
    Attribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.pScratchBuffer               = ScratchBuffer;
    Attribs.ScratchBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->BuildTLAS(Attribs);

    if (Update)
    {
        Shuffle(pInstances, pInstances + InstanceCount);

        Attribs.Update = true;
        pContext->BuildTLAS(Attribs);
    }
}

void CompareGeometryDesc(const ITopLevelAS*, const ITopLevelAS*)
{}

void CompareGeometryDesc(const IBottomLevelAS* pLhsAS, const IBottomLevelAS* pRhsAS)
{
    const auto& lDesc = pLhsAS->GetDesc();
    const auto& rDesc = pRhsAS->GetDesc();

    ASSERT_EQ(lDesc.TriangleCount, rDesc.TriangleCount);
    ASSERT_EQ(lDesc.BoxCount, rDesc.BoxCount);

    std::unordered_map<std::string, const BLASTriangleDesc*>    TriangleMap;
    std::unordered_map<std::string, const BLASBoundingBoxDesc*> BoxMap;

    for (Uint32 i = 0; i < lDesc.TriangleCount; ++i)
        ASSERT_TRUE(TriangleMap.emplace(lDesc.pTriangles[i].GeometryName, &lDesc.pTriangles[i]).second);

    for (Uint32 i = 0; i < lDesc.BoxCount; ++i)
        ASSERT_TRUE(BoxMap.emplace(lDesc.pBoxes[i].GeometryName, &lDesc.pBoxes[i]).second);

    for (Uint32 i = 0; i < rDesc.TriangleCount; ++i)
    {
        const auto& rTri = rDesc.pTriangles[i];
        auto        iter = TriangleMap.find(rTri.GeometryName);
        ASSERT_TRUE(iter != TriangleMap.end());
        const auto& lTri = *iter->second;

        ASSERT_STREQ(lTri.GeometryName, rTri.GeometryName);
        ASSERT_EQ(lTri.MaxVertexCount, rTri.MaxVertexCount);
        ASSERT_EQ(lTri.VertexValueType, rTri.VertexValueType);
        ASSERT_EQ(lTri.VertexComponentCount, rTri.VertexComponentCount);
        ASSERT_EQ(lTri.MaxPrimitiveCount, rTri.MaxPrimitiveCount);
        ASSERT_EQ(lTri.IndexType, rTri.IndexType);
        ASSERT_EQ(lTri.AllowsTransforms, rTri.AllowsTransforms);
    }

    for (Uint32 i = 0; i < rDesc.BoxCount; ++i)
    {
        const auto& rBox = rDesc.pBoxes[i];
        auto        iter = BoxMap.find(rBox.GeometryName);
        ASSERT_TRUE(iter != BoxMap.end());
        const auto& lBox = *iter->second;

        ASSERT_STREQ(lBox.GeometryName, rBox.GeometryName);
        ASSERT_EQ(lBox.MaxBoxCount, rBox.MaxBoxCount);
    }
}

template <typename WriteASCompactedSizeAttribs,
          typename ASDescType,
          typename CopyASAttribsType,
          typename ASType,
          typename WriteASCompactedSizeFnType,
          typename CreateASFnType,
          typename CopyASFnType,
          typename ASFieldType,
          typename WriteASTransitionFieldType>
void ASCompaction(IRenderDevice*             pDevice,
                  IDeviceContext*            pContext,
                  ASType*                    pSrcAS,
                  RefCntAutoPtr<ASType>&     pDstAS,
                  WriteASCompactedSizeFnType WriteASCompactedSizeFn,
                  CreateASFnType             CreateASFn,
                  CopyASFnType               CopyASFn,
                  ASFieldType                ASField,
                  WriteASTransitionFieldType WriteASTransitionField)
{
    RefCntAutoPtr<IBuffer> pCompactedSizeBuffer;
    RefCntAutoPtr<IBuffer> pReadbackBuffer;

    BufferDesc BuffDesc;
    BuffDesc.Name          = "AS compacted size Buffer";
    BuffDesc.Usage         = USAGE_DEFAULT;
    BuffDesc.BindFlags     = BIND_UNORDERED_ACCESS;
    BuffDesc.Mode          = BUFFER_MODE_RAW;
    BuffDesc.uiSizeInBytes = sizeof(Uint64);

    pDevice->CreateBuffer(BuffDesc, nullptr, &pCompactedSizeBuffer);
    VERIFY_EXPR(pCompactedSizeBuffer != nullptr);

    BuffDesc.Name           = "Compacted size readback Buffer";
    BuffDesc.Usage          = USAGE_STAGING;
    BuffDesc.BindFlags      = BIND_NONE;
    BuffDesc.Mode           = BUFFER_MODE_UNDEFINED;
    BuffDesc.CPUAccessFlags = CPU_ACCESS_READ;

    pDevice->CreateBuffer(BuffDesc, nullptr, &pReadbackBuffer);
    VERIFY_EXPR(pReadbackBuffer != nullptr);

    WriteASCompactedSizeAttribs Attribs;
    Attribs.*ASField                = pSrcAS;
    Attribs.pDestBuffer             = pCompactedSizeBuffer;
    Attribs.*WriteASTransitionField = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.BufferTransitionMode    = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    (pContext->*WriteASCompactedSizeFn)(Attribs);

    pContext->CopyBuffer(pCompactedSizeBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                         pReadbackBuffer, 0, sizeof(Uint64), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->Flush();
    pContext->WaitForIdle();

    void* pMapped = nullptr;
    pContext->MapBuffer(pReadbackBuffer, MAP_READ, MAP_FLAG_DO_NOT_WAIT, pMapped);

    ASDescType ASDesc;
    ASDesc.Name          = "AS compacted copy";
    ASDesc.CompactedSize = static_cast<Uint32>(*static_cast<Uint64*>(pMapped));

    pContext->UnmapBuffer(pReadbackBuffer, MAP_READ);

    if (ASDesc.CompactedSize == 0)
    {
        GTEST_FAIL() << "Failed to get compacted AS size";
        return;
    }

    (pDevice->*CreateASFn)(ASDesc, &pDstAS);
    VERIFY_EXPR(pDstAS != nullptr);

    CopyASAttribsType CopyAttribs;
    CopyAttribs.pSrc              = pSrcAS;
    CopyAttribs.pDst              = pDstAS;
    CopyAttribs.Mode              = COPY_AS_MODE_COMPACT;
    CopyAttribs.SrcTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    CopyAttribs.DstTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    (pContext->*CopyASFn)(CopyAttribs);

    ASSERT_EQ(pDstAS->GetDesc().CompactedSize, ASDesc.CompactedSize);
    ASSERT_EQ(pDstAS->GetDesc().Flags, ASDesc.Flags);
    ASSERT_STREQ(pDstAS->GetDesc().Name, ASDesc.Name);
    CompareGeometryDesc(pSrcAS, pDstAS);
}

void BLASCompaction(Uint32 TestId, IRenderDevice* pDevice, IDeviceContext* pContext, IBottomLevelAS* pSrcBLAS, RefCntAutoPtr<IBottomLevelAS>& pDstBLAS)
{
    switch (TestId)
    {
        case 0:
        case 2:
        case 5:
        case 7:
        case 8:
            pDstBLAS = pSrcBLAS;
            break;

        case 1:
        case 3:
        {
            std::vector<BLASTriangleDesc>    TriangleInfos;
            std::vector<BLASBoundingBoxDesc> BoxInfos;
            BottomLevelASDesc                ASDesc = pSrcBLAS->GetDesc();

            ASDesc.Name = "BLAS copy";
            if (ASDesc.pTriangles)
            {
                TriangleInfos.assign(ASDesc.pTriangles, ASDesc.pTriangles + ASDesc.TriangleCount);
                Shuffle(TriangleInfos.begin(), TriangleInfos.end());
                ASDesc.pTriangles = TriangleInfos.data();
            }
            if (ASDesc.pBoxes)
            {
                BoxInfos.assign(ASDesc.pBoxes, ASDesc.pBoxes + ASDesc.BoxCount);
                Shuffle(BoxInfos.begin(), BoxInfos.end());
                ASDesc.pBoxes = BoxInfos.data();
            }
            pDevice->CreateBLAS(ASDesc, &pDstBLAS);
            VERIFY_EXPR(pDstBLAS != nullptr);

            CopyBLASAttribs CopyAttribs;
            CopyAttribs.pSrc              = pSrcBLAS;
            CopyAttribs.pDst              = pDstBLAS;
            CopyAttribs.Mode              = COPY_AS_MODE_CLONE;
            CopyAttribs.SrcTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            CopyAttribs.DstTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            pContext->CopyBLAS(CopyAttribs);

            ASSERT_EQ(pDstBLAS->GetDesc().Flags, ASDesc.Flags);
            CompareGeometryDesc(pSrcBLAS, pDstBLAS);
            break;
        }
        case 4:
        case 6:
            ASCompaction<WriteBLASCompactedSizeAttribs, BottomLevelASDesc, CopyBLASAttribs>(
                pDevice, pContext, pSrcBLAS, pDstBLAS,
                &IDeviceContext::WriteBLASCompactedSize,
                &IRenderDevice::CreateBLAS,
                &IDeviceContext::CopyBLAS,
                &WriteBLASCompactedSizeAttribs::pBLAS,
                &WriteBLASCompactedSizeAttribs::BLASTransitionMode);
            break;

        default:
            UNEXPECTED("unsupported TestId");
    }
}

void TLASCompaction(Uint32 TestId, IRenderDevice* pDevice, IDeviceContext* pContext, ITopLevelAS* pSrcTLAS, RefCntAutoPtr<ITopLevelAS>& pDstTLAS)
{
    switch (TestId)
    {
        case 0:
        case 1:
        case 4:
        case 7:
        case 8:
            pDstTLAS = pSrcTLAS;
            break;

        case 2:
        case 3:
        {
            TopLevelASDesc ASDesc = pSrcTLAS->GetDesc();
            ASDesc.Name           = "TLAS copy";
            pDevice->CreateTLAS(ASDesc, &pDstTLAS);
            VERIFY_EXPR(pDstTLAS != nullptr);

            CopyTLASAttribs CopyAttribs;
            CopyAttribs.pSrc              = pSrcTLAS;
            CopyAttribs.pDst              = pDstTLAS;
            CopyAttribs.Mode              = COPY_AS_MODE_CLONE;
            CopyAttribs.SrcTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            CopyAttribs.DstTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            pContext->CopyTLAS(CopyAttribs);

            ASSERT_EQ(pDstTLAS->GetDesc().Flags, ASDesc.Flags);
            break;
        }
        case 5:
        case 6:
            ASCompaction<WriteTLASCompactedSizeAttribs, TopLevelASDesc, CopyTLASAttribs>(
                pDevice, pContext, pSrcTLAS, pDstTLAS,
                &IDeviceContext::WriteTLASCompactedSize,
                &IRenderDevice::CreateTLAS,
                &IDeviceContext::CopyTLAS,
                &WriteTLASCompactedSizeAttribs::pTLAS,
                &WriteTLASCompactedSizeAttribs::TLASTransitionMode);
            break;

        default:
            UNEXPECTED("unsupported TestId");
    }
}

std::string TestIdToString(const testing::TestParamInfo<int>& info)
{
    std::string name;
    switch (info.param)
    {
        case 0: name = "default"; break;
        case 1: name = "copiedBLAS"; break;
        case 2: name = "copiedTLAS"; break;
        case 3: name = "copiedBLAS_copiedTLAS"; break;
        case 4: name = "compactedBLAS"; break;
        case 5: name = "compactedTLAS"; break;
        case 6: name = "compactedBLAS_compactedTLAS"; break;
        case 7: name = "updateBLAS"; break;
        case 8: name = "updateTLAS"; break;
        default: name = std::to_string(info.param); UNEXPECTED("unsupported TestId");
    }
    return name;
}

bool TestBLASUpdate(Uint32 TestId)
{
    return TestId == 7;
}

bool TestTLASUpdate(Uint32 TestId)
{
    return TestId == 8;
}

const auto TestParamRange = testing::Range(0, 9);


class RT1 : public testing::TestWithParam<int>
{};

TEST_P(RT1, TriangleClosestHitShader)
{
    Uint32 TestId  = GetParam();
    auto*  pEnv    = TestingEnvironment::GetInstance();
    auto*  pDevice = pEnv->GetDevice();
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
    ShaderCI.EntryPoint     = "main";

    // Create ray generation shader.
    RefCntAutoPtr<IShader> pRG;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
        ShaderCI.Desc.Name       = "Ray tracing RG";
        ShaderCI.Source          = HLSL::RayTracingTest1_RG.c_str();
        pDevice->CreateShader(ShaderCI, &pRG);
        VERIFY_EXPR(pRG != nullptr);
    }

    // Create ray miss shader.
    RefCntAutoPtr<IShader> pRMiss;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
        ShaderCI.Desc.Name       = "Miss shader";
        ShaderCI.Source          = HLSL::RayTracingTest1_RM.c_str();
        pDevice->CreateShader(ShaderCI, &pRMiss);
        VERIFY_EXPR(pRMiss != nullptr);
    }

    // Create ray closest hit shader.
    RefCntAutoPtr<IShader> pClosestHit;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
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

    const auto& Vertices = TestingConstants::TriangleClosestHit::Vertices;

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
    Triangle.Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    RefCntAutoPtr<IBottomLevelAS> pTempBLAS;
    CreateBLAS(pDevice, pContext, &Triangle, 1, TestBLASUpdate(TestId), pTempBLAS);

    RefCntAutoPtr<IBottomLevelAS> pBLAS;
    BLASCompaction(TestId, pDevice, pContext, pTempBLAS, pBLAS);

    TLASBuildInstanceData Instance;
    Instance.InstanceName = "Instance";
    Instance.pBLAS        = pBLAS;
    Instance.Flags        = RAYTRACING_INSTANCE_NONE;

    RefCntAutoPtr<ITopLevelAS> pTempTLAS;
    const Uint32               HitShadersPerInstance = 1;
    CreateTLAS(pDevice, pContext, &Instance, 1, HitShadersPerInstance, TestTLASUpdate(TestId), pTempTLAS);

    RefCntAutoPtr<ITopLevelAS> pTLAS;
    TLASCompaction(TestId, pDevice, pContext, pTempTLAS, pTLAS);

    ShaderBindingTableDesc SBTDesc;
    SBTDesc.Name = "SBT";
    SBTDesc.pPSO = pRayTracingPSO;

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
    Attribs.DimensionX        = SCDesc.Width;
    Attribs.DimensionY        = SCDesc.Height;
    Attribs.pSBT              = pSBT;
    Attribs.SBTTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->TraceRays(Attribs);

    pSwapChain->Present();
}
INSTANTIATE_TEST_SUITE_P(RayTracingTest, RT1, TestParamRange, TestIdToString);


class RT2 : public testing::TestWithParam<int>
{};

TEST_P(RT2, TriangleAnyHitShader)
{
    Uint32 TestId  = GetParam();
    auto*  pEnv    = TestingEnvironment::GetInstance();
    auto*  pDevice = pEnv->GetDevice();
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
    ShaderCI.EntryPoint     = "main";

    // Create ray generation shader.
    RefCntAutoPtr<IShader> pRG;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
        ShaderCI.Desc.Name       = "Ray tracing RG";
        ShaderCI.Source          = HLSL::RayTracingTest2_RG.c_str();
        pDevice->CreateShader(ShaderCI, &pRG);
        VERIFY_EXPR(pRG != nullptr);
    }

    // Create ray miss shader.
    RefCntAutoPtr<IShader> pRMiss;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
        ShaderCI.Desc.Name       = "Miss shader";
        ShaderCI.Source          = HLSL::RayTracingTest2_RM.c_str();
        pDevice->CreateShader(ShaderCI, &pRMiss);
        VERIFY_EXPR(pRMiss != nullptr);
    }

    // Create ray closest hit shader.
    RefCntAutoPtr<IShader> pClosestHit;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
        ShaderCI.Desc.Name       = "Ray closest hit shader";
        ShaderCI.Source          = HLSL::RayTracingTest2_RCH.c_str();
        pDevice->CreateShader(ShaderCI, &pClosestHit);
        VERIFY_EXPR(pClosestHit != nullptr);
    }

    // Create ray any hit shader.
    RefCntAutoPtr<IShader> pAnyHit;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_ANY_HIT;
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

    const auto& Vertices = TestingConstants::TriangleAnyHit::Vertices;

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
    Triangle.Flags                = RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANY_HIT_INVOCATION;

    RefCntAutoPtr<IBottomLevelAS> pTempBLAS;
    CreateBLAS(pDevice, pContext, &Triangle, 1, TestBLASUpdate(TestId), pTempBLAS);

    RefCntAutoPtr<IBottomLevelAS> pBLAS;
    BLASCompaction(TestId, pDevice, pContext, pTempBLAS, pBLAS);

    TLASBuildInstanceData Instance;
    Instance.InstanceName = "Instance";
    Instance.pBLAS        = pBLAS;
    Instance.Flags        = RAYTRACING_INSTANCE_NONE;

    RefCntAutoPtr<ITopLevelAS> pTempTLAS;
    const Uint32               HitShadersPerInstance = 1;
    CreateTLAS(pDevice, pContext, &Instance, 1, HitShadersPerInstance, TestTLASUpdate(TestId), pTempTLAS);

    RefCntAutoPtr<ITopLevelAS> pTLAS;
    TLASCompaction(TestId, pDevice, pContext, pTempTLAS, pTLAS);

    ShaderBindingTableDesc SBTDesc;
    SBTDesc.Name = "SBT";
    SBTDesc.pPSO = pRayTracingPSO;

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
    Attribs.DimensionX        = SCDesc.Width;
    Attribs.DimensionY        = SCDesc.Height;
    Attribs.pSBT              = pSBT;
    Attribs.SBTTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->TraceRays(Attribs);

    pSwapChain->Present();
}
INSTANTIATE_TEST_SUITE_P(RayTracingTest, RT2, TestParamRange, TestIdToString);


class RT3 : public testing::TestWithParam<int>
{};

TEST_P(RT3, ProceduralIntersection)
{
    Uint32 TestId  = GetParam();
    auto*  pEnv    = TestingEnvironment::GetInstance();
    auto*  pDevice = pEnv->GetDevice();
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
    ShaderCI.EntryPoint     = "main";

    // Create ray generation shader.
    RefCntAutoPtr<IShader> pRG;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
        ShaderCI.Desc.Name       = "Ray tracing RG";
        ShaderCI.Source          = HLSL::RayTracingTest3_RG.c_str();
        pDevice->CreateShader(ShaderCI, &pRG);
        VERIFY_EXPR(pRG != nullptr);
    }

    // Create ray miss shader.
    RefCntAutoPtr<IShader> pRMiss;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
        ShaderCI.Desc.Name       = "Miss shader";
        ShaderCI.Source          = HLSL::RayTracingTest3_RM.c_str();
        pDevice->CreateShader(ShaderCI, &pRMiss);
        VERIFY_EXPR(pRMiss != nullptr);
    }

    // Create ray closest hit shader.
    RefCntAutoPtr<IShader> pClosestHit;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
        ShaderCI.Desc.Name       = "Ray closest hit shader";
        ShaderCI.Source          = HLSL::RayTracingTest3_RCH.c_str();
        pDevice->CreateShader(ShaderCI, &pClosestHit);
        VERIFY_EXPR(pClosestHit != nullptr);
    }

    // Create ray intersection shader.
    RefCntAutoPtr<IShader> pIntersection;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_INTERSECTION;
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

    const auto& Boxes = TestingConstants::ProceduralIntersection::Boxes;

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
    Box.Flags        = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    RefCntAutoPtr<IBottomLevelAS> pTempBLAS;
    CreateBLAS(pDevice, pContext, &Box, 1, TestBLASUpdate(TestId), pTempBLAS);

    RefCntAutoPtr<IBottomLevelAS> pBLAS;
    BLASCompaction(TestId, pDevice, pContext, pTempBLAS, pBLAS);

    TLASBuildInstanceData Instance;
    Instance.InstanceName = "Instance";
    Instance.pBLAS        = pBLAS;
    Instance.Flags        = RAYTRACING_INSTANCE_NONE;

    RefCntAutoPtr<ITopLevelAS> pTempTLAS;
    const Uint32               HitShadersPerInstance = 1;
    CreateTLAS(pDevice, pContext, &Instance, 1, HitShadersPerInstance, TestTLASUpdate(TestId), pTempTLAS);

    RefCntAutoPtr<ITopLevelAS> pTLAS;
    TLASCompaction(TestId, pDevice, pContext, pTempTLAS, pTLAS);

    ShaderBindingTableDesc SBTDesc;
    SBTDesc.Name = "SBT";
    SBTDesc.pPSO = pRayTracingPSO;

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
    Attribs.DimensionX        = SCDesc.Width;
    Attribs.DimensionY        = SCDesc.Height;
    Attribs.pSBT              = pSBT;
    Attribs.SBTTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->TraceRays(Attribs);

    pSwapChain->Present();
}
INSTANTIATE_TEST_SUITE_P(RayTracingTest, RT3, TestParamRange, TestIdToString);


class RT4 : public testing::TestWithParam<int>
{};

TEST_P(RT4, MultiGeometry)
{
    Uint32 TestId  = GetParam();
    auto*  pEnv    = TestingEnvironment::GetInstance();
    auto*  pDevice = pEnv->GetDevice();
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
                RayTracingMultiGeometryReferenceD3D12(pSwapChain);
                break;
#endif

#if VULKAN_SUPPORTED
            case RENDER_DEVICE_TYPE_VULKAN:
                RayTracingMultiGeometryReferenceVk(pSwapChain);
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
    ShaderCI.EntryPoint     = "main";

    // Create ray generation shader.
    RefCntAutoPtr<IShader> pRG;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
        ShaderCI.Desc.Name       = "Ray tracing RG";
        ShaderCI.Source          = HLSL::RayTracingTest4_RG.c_str();
        pDevice->CreateShader(ShaderCI, &pRG);
        VERIFY_EXPR(pRG != nullptr);
    }

    // Create ray miss shader.
    RefCntAutoPtr<IShader> pRMiss;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
        ShaderCI.Desc.Name       = "Miss shader";
        ShaderCI.Source          = HLSL::RayTracingTest4_RM.c_str();
        pDevice->CreateShader(ShaderCI, &pRMiss);
        VERIFY_EXPR(pRMiss != nullptr);
    }

    // Create ray closest hit shader.
    RefCntAutoPtr<IShader> pClosestHit1;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
        ShaderCI.Desc.Name       = "Ray closest hit shader 1";
        ShaderCI.Source          = HLSL::RayTracingTest4_RCH1.c_str();
        pDevice->CreateShader(ShaderCI, &pClosestHit1);
        VERIFY_EXPR(pClosestHit1 != nullptr);
    }

    RefCntAutoPtr<IShader> pClosestHit2;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
        ShaderCI.Desc.Name       = "Ray closest hit shader 2";
        ShaderCI.Source          = HLSL::RayTracingTest4_RCH2.c_str();
        pDevice->CreateShader(ShaderCI, &pClosestHit2);
        VERIFY_EXPR(pClosestHit2 != nullptr);
    }

    const RayTracingGeneralShaderGroup     GeneralShaders[]     = {{"Main", pRG}, {"Miss", pRMiss}};
    const RayTracingTriangleHitShaderGroup TriangleHitShaders[] = {{"HitGroup1", pClosestHit1}, {"HitGroup2", pClosestHit2}};

    PSOCreateInfo.pGeneralShaders        = GeneralShaders;
    PSOCreateInfo.GeneralShaderCount     = _countof(GeneralShaders);
    PSOCreateInfo.pTriangleHitShaders    = TriangleHitShaders;
    PSOCreateInfo.TriangleHitShaderCount = _countof(TriangleHitShaders);

    PSOCreateInfo.RayTracingPipeline.MaxRecursionDepth = 0;

    PSOCreateInfo.RayTracingPipeline.ShaderRecordSize = TestingConstants::MultiGeometry::ShaderRecordSize;
    PSOCreateInfo.pShaderRecordName                   = "g_LocalRoot";

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    RefCntAutoPtr<IPipelineState> pRayTracingPSO;
    pDevice->CreateRayTracingPipelineState(PSOCreateInfo, &pRayTracingPSO);
    VERIFY_EXPR(pRayTracingPSO != nullptr);

    RefCntAutoPtr<IShaderResourceBinding> pRayTracingSRB;
    pRayTracingPSO->CreateShaderResourceBinding(&pRayTracingSRB, true);
    VERIFY_EXPR(pRayTracingSRB != nullptr);

    const auto& Vertices         = TestingConstants::MultiGeometry::Vertices;
    const auto& Indices          = TestingConstants::MultiGeometry::Indices;
    const auto& Weights          = TestingConstants::MultiGeometry::Weights;
    const auto& PrimitiveOffsets = TestingConstants::MultiGeometry::PrimitiveOffsets;
    const auto& Primitives       = TestingConstants::MultiGeometry::Primitives;

    RefCntAutoPtr<IBuffer> pVertexBuffer;
    RefCntAutoPtr<IBuffer> pIndexBuffer;
    RefCntAutoPtr<IBuffer> pPerInstanceBuffer;
    RefCntAutoPtr<IBuffer> pPrimitiveBuffer;
    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Indices";
        BuffDesc.Usage         = USAGE_IMMUTABLE;
        BuffDesc.BindFlags     = BIND_RAY_TRACING;
        BuffDesc.uiSizeInBytes = sizeof(Indices);
        BufferData BufData     = {Indices, sizeof(Indices)};
        pDevice->CreateBuffer(BuffDesc, &BufData, &pIndexBuffer);
        VERIFY_EXPR(pIndexBuffer != nullptr);

        BuffDesc.Name              = "Vertices";
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.BindFlags         = BIND_RAY_TRACING | BIND_SHADER_RESOURCE;
        BuffDesc.uiSizeInBytes     = sizeof(Vertices);
        BuffDesc.ElementByteStride = sizeof(Vertices[0]);
        BufData                    = {Vertices, sizeof(Vertices)};
        pDevice->CreateBuffer(BuffDesc, &BufData, &pVertexBuffer);
        VERIFY_EXPR(pVertexBuffer != nullptr);

        BuffDesc.Name              = "PerInstanceData";
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.uiSizeInBytes     = sizeof(PrimitiveOffsets);
        BuffDesc.ElementByteStride = sizeof(PrimitiveOffsets[0]);
        BufData                    = {PrimitiveOffsets, sizeof(PrimitiveOffsets)};
        pDevice->CreateBuffer(BuffDesc, &BufData, &pPerInstanceBuffer);
        VERIFY_EXPR(pPerInstanceBuffer != nullptr);

        BuffDesc.Name              = "PrimitiveData";
        BuffDesc.uiSizeInBytes     = sizeof(Primitives);
        BuffDesc.ElementByteStride = sizeof(Primitives[0]);
        BufData                    = {Primitives, sizeof(Primitives)};
        pDevice->CreateBuffer(BuffDesc, &BufData, &pPrimitiveBuffer);
        VERIFY_EXPR(pPrimitiveBuffer != nullptr);
    }

    BLASBuildTriangleData Triangles[3] = {};
    Triangles[0].GeometryName          = "Geom 1";
    Triangles[0].pVertexBuffer         = pVertexBuffer;
    Triangles[0].VertexStride          = sizeof(Vertices[0]);
    Triangles[0].VertexCount           = _countof(Vertices);
    Triangles[0].VertexValueType       = VT_FLOAT32;
    Triangles[0].VertexComponentCount  = 3;
    Triangles[0].pIndexBuffer          = pIndexBuffer;
    Triangles[0].IndexType             = VT_UINT32;
    Triangles[0].PrimitiveCount        = (PrimitiveOffsets[1] - PrimitiveOffsets[0]);
    Triangles[0].IndexOffset           = PrimitiveOffsets[0] * sizeof(uint) * 3;
    Triangles[0].Flags                 = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    Triangles[1].GeometryName         = "Geom 2";
    Triangles[1].pVertexBuffer        = pVertexBuffer;
    Triangles[1].VertexStride         = sizeof(Vertices[0]);
    Triangles[1].VertexCount          = _countof(Vertices);
    Triangles[1].VertexValueType      = VT_FLOAT32;
    Triangles[1].VertexComponentCount = 3;
    Triangles[1].pIndexBuffer         = pIndexBuffer;
    Triangles[1].IndexType            = VT_UINT32;
    Triangles[1].PrimitiveCount       = (PrimitiveOffsets[2] - PrimitiveOffsets[1]);
    Triangles[1].IndexOffset          = PrimitiveOffsets[1] * sizeof(uint) * 3;
    Triangles[1].Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    Triangles[2].GeometryName         = "Geom 3";
    Triangles[2].pVertexBuffer        = pVertexBuffer;
    Triangles[2].VertexStride         = sizeof(Vertices[0]);
    Triangles[2].VertexCount          = _countof(Vertices);
    Triangles[2].VertexValueType      = VT_FLOAT32;
    Triangles[2].VertexComponentCount = 3;
    Triangles[2].pIndexBuffer         = pIndexBuffer;
    Triangles[2].IndexType            = VT_UINT32;
    Triangles[2].PrimitiveCount       = (_countof(Primitives) - PrimitiveOffsets[2]);
    Triangles[2].IndexOffset          = PrimitiveOffsets[2] * sizeof(uint) * 3;
    Triangles[2].Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    RefCntAutoPtr<IBottomLevelAS> pTempBLAS;
    CreateBLAS(pDevice, pContext, Triangles, _countof(Triangles), TestBLASUpdate(TestId), pTempBLAS);

    RefCntAutoPtr<IBottomLevelAS> pBLAS;
    BLASCompaction(TestId, pDevice, pContext, pTempBLAS, pBLAS);

    TLASBuildInstanceData Instances[2] = {};

    Instances[0].InstanceName = "Instance 1";
    Instances[0].pBLAS        = pBLAS;
    Instances[0].Flags        = RAYTRACING_INSTANCE_NONE;

    Instances[1].InstanceName = "Instance 2";
    Instances[1].pBLAS        = pBLAS;
    Instances[1].Flags        = RAYTRACING_INSTANCE_NONE;
    Instances[1].Transform.SetTranslation(0.1f, 0.5f, 0.0f);

    RefCntAutoPtr<ITopLevelAS> pTempTLAS;
    const Uint32               HitShadersPerInstance = 1;
    CreateTLAS(pDevice, pContext, Instances, _countof(Instances), HitShadersPerInstance, TestTLASUpdate(TestId), pTempTLAS);

    RefCntAutoPtr<ITopLevelAS> pTLAS;
    TLASCompaction(TestId, pDevice, pContext, pTempTLAS, pTLAS);

    ShaderBindingTableDesc SBTDesc;
    SBTDesc.Name = "SBT";
    SBTDesc.pPSO = pRayTracingPSO;

    RefCntAutoPtr<IShaderBindingTable> pSBT;
    pDevice->CreateSBT(SBTDesc, &pSBT);
    VERIFY_EXPR(pSBT != nullptr);

    pSBT->BindRayGenShader("Main");
    pSBT->BindMissShader("Miss", 0);
    pSBT->BindHitGroup(pTLAS, "Instance 1", "Geom 1", 0, "HitGroup1", &Weights[0], sizeof(Weights[0]));
    pSBT->BindHitGroup(pTLAS, "Instance 1", "Geom 2", 0, "HitGroup1", &Weights[1], sizeof(Weights[0]));
    pSBT->BindHitGroup(pTLAS, "Instance 1", "Geom 3", 0, "HitGroup1", &Weights[2], sizeof(Weights[0]));
    pSBT->BindHitGroup(pTLAS, "Instance 2", "Geom 1", 0, "HitGroup2", &Weights[3], sizeof(Weights[0]));
    pSBT->BindHitGroup(pTLAS, "Instance 2", "Geom 2", 0, "HitGroup2", &Weights[4], sizeof(Weights[0]));
    pSBT->BindHitGroup(pTLAS, "Instance 2", "Geom 3", 0, "HitGroup2", &Weights[5], sizeof(Weights[0]));

    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_TLAS")->Set(pTLAS);
    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_ColorBuffer")->Set(pTestingSwapChain->GetCurrentBackBufferUAV());

    IDeviceObject* pObject = pPerInstanceBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_PerInstance")->SetArray(&pObject, 0, 1);
    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_PerInstance")->SetArray(&pObject, 1, 1);

    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_Primitives")->Set(pPrimitiveBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_Vertices")->Set(pVertexBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    pContext->SetPipelineState(pRayTracingPSO);
    pContext->CommitShaderResources(pRayTracingSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const auto& SCDesc = pSwapChain->GetDesc();

    TraceRaysAttribs Attribs;
    Attribs.DimensionX        = SCDesc.Width;
    Attribs.DimensionY        = SCDesc.Height;
    Attribs.pSBT              = pSBT;
    Attribs.SBTTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    pContext->TraceRays(Attribs);

    pSwapChain->Present();
}
INSTANTIATE_TEST_SUITE_P(RayTracingTest, RT4, TestParamRange, TestIdToString);

} // namespace
